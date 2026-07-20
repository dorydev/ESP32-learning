
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

uint16_t leds_number = 15;
int16_t  GPIO_addr   = 8;
uint16_t type_LED    = NEO_GRB;

#define CMD_SET_COLOR    0x01
#define CMD_SET_EFFECT   0x02
#define CMD_CLEAR        0x03
#define CMD_QUERY_STATE  0x04

#define SERVICE_UUID        "39e3241f-9641-45e4-bdfd-9248675f8799"
#define CHARACTERISTIC_UUID "15fb5008-ab32-44c9-b709-eadaf7929407"

BLECharacteristic *pCharacteristic;
Adafruit_NeoPixel strip(leds_number, GPIO_addr, type_LED);
Preferences       prefs;

struct LedState {
  uint8_t red        = 255;
  uint8_t green      = 0;
  uint8_t blue       = 0;
  uint8_t brightness = 128;
  uint8_t effectId   = 0;
  uint8_t speed      = 50;
} state;

bool deviceConnected    = false;
bool oldDeviceConnected = false;
bool ledsOff            = false;

void saveState() {
  prefs.begin("led", false);
  prefs.putUChar("r",   state.red);
  prefs.putUChar("g",   state.green);
  prefs.putUChar("b",   state.blue);
  prefs.putUChar("bri", state.brightness);
  prefs.putUChar("fx",  state.effectId);
  prefs.putUChar("spd", state.speed);
  prefs.end();
  Serial.println("State saved to NVS");
}

void loadState() {
  prefs.begin("led", true);  // read-only
  state.red        = prefs.getUChar("r",   255);
  state.green      = prefs.getUChar("g",   0);
  state.blue       = prefs.getUChar("b",   0);
  state.brightness = prefs.getUChar("bri", 128);
  state.effectId   = prefs.getUChar("fx",  0);
  state.speed      = prefs.getUChar("spd", 50);
  prefs.end();
  Serial.println("State loaded from NVS");
}

void applyStaticColor() {
  strip.setBrightness(state.brightness);
  for (uint16_t i = 0; i < leds_number; i++) {
    strip.setPixelColor(i, state.red, state.green, state.blue);
  }
  strip.show();
}

unsigned long lastEffectTick = 0;
uint16_t      effectStep     = 0;

unsigned long speedToInterval() {
  return max(5UL, 1000UL / (unsigned long)state.speed);
}

void tickBreathe() {
  float phase = (effectStep % 360) * PI / 180.0;
  uint8_t bri = (uint8_t)(state.brightness * (0.5 + 0.5 * sin(phase)));
  strip.setBrightness(bri);
  for (uint16_t i = 0; i < leds_number; i++) {
    strip.setPixelColor(i, state.red, state.green, state.blue);
  }
  strip.show();
  effectStep = (effectStep + 3) % 360;
}

void tickRainbow() {
  strip.setBrightness(state.brightness);
  for (uint16_t i = 0; i < leds_number; i++) {
    uint16_t hue = (effectStep * 256 + (i * 65536 / leds_number)) & 0xFFFF;
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
  }
  strip.show();
  effectStep = (effectStep + 1) % 256;
}

void tickFade() {
  float t = (effectStep % 200) / 100.0;
  if (t > 1.0) t = 2.0 - t;
  uint8_t r = (uint8_t)(state.red   * (1.0 - t));
  uint8_t g = (uint8_t)(state.green * (1.0 - t));
  uint8_t b = (uint8_t)(state.blue  * (1.0 - t));
  strip.setBrightness(state.brightness);
  for (uint16_t i = 0; i < leds_number; i++) {
    strip.setPixelColor(i, r, g, b);
  }
  strip.show();
  effectStep = (effectStep + 1) % 200;
}

void tickEffect() {
  unsigned long now = millis();
  if (now - lastEffectTick < speedToInterval()) return;
  lastEffectTick = now;

  switch (state.effectId) {
    case 1: tickBreathe(); break;
    case 2: tickRainbow();  break;
    case 3: tickFade();     break;
    default: break;
  }
}

void sendStateNotification() {
  if (!deviceConnected || pCharacteristic == nullptr) return;
  uint8_t response[9] = {
    CMD_QUERY_STATE,
    state.red, state.green, state.blue,
    state.brightness,
    state.effectId,
    state.speed,
    (uint8_t)(leds_number >> 8),
    (uint8_t)(leds_number & 0xFF),
  };
  pCharacteristic->setValue(response, 9);
  pCharacteristic->notify();
  Serial.println("State notification sent");
}

void handleCommand(uint8_t* data, uint8_t length) {
  if (length == 0) return;
  uint8_t cmd = data[0];

  switch (cmd) {

    case CMD_SET_COLOR:
    {
      if (length < 5) return;
      ledsOff = false;
      state.red        = data[1];
      state.green      = data[2];
      state.blue       = data[3];
      state.brightness = data[4];
      state.effectId   = 0;
      effectStep       = 0;
      applyStaticColor();
      saveState();
      Serial.printf("CMD_SET_COLOR: #%02X%02X%02X bri=%d\n",
      state.red, state.green, state.blue, state.brightness);
      break;
    }

    case CMD_SET_EFFECT:
    {
      if (length < 7) return;
      ledsOff = false;
      state.effectId   = data[1];
      state.speed      = data[2];
      state.red        = data[3];
      state.green      = data[4];
      state.blue       = data[5];
      state.brightness = data[6];
      effectStep       = 0;
      lastEffectTick   = 0;
      if (state.effectId == 0) applyStaticColor();
      saveState();
      Serial.printf("CMD_SET_EFFECT: fx=%d spd=%d #%02X%02X%02X bri=%d\n",
      state.effectId, state.speed,
      state.red, state.green, state.blue, state.brightness);
      break;
    }

    case CMD_CLEAR:
      ledsOff = true;
      effectStep = 0;
      strip.clear();
      strip.show();
      Serial.println("CMD_CLEAR (LEDs off, NVS preserved)");
      break;

    case CMD_QUERY_STATE:
      Serial.println("CMD_QUERY_STATE");
      sendStateNotification();
      break;
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE: Client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE: Client disconnected");
  }
};

class WriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value.length() > 0) {
      Serial.printf("BLE ← %d bytes, CMD=0x%02X\n",
      value.length(), (uint8_t)value[0]);
      handleCommand((uint8_t*)value.c_str(), value.length());
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32 LED Controller ===");

  loadState();

  strip.begin();
  applyStaticColor();

  BLEDevice::init("ESP32 C3 Super Mini");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new WriteCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("BLE advertising started");
  Serial.printf("Restored: fx=%d #%02X%02X%02X bri=%d spd=%d\n",
  state.effectId, state.red, state.green, state.blue,
  state.brightness, state.speed);
}


void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    BLEDevice::getAdvertising()->start();
    Serial.println("BLE: Restarted advertising");
    oldDeviceConnected = false;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = true;
  }
  if (state.effectId > 0 && !ledsOff) {
    tickEffect();
  }
}
