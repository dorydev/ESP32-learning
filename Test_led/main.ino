#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(128, 64, &Wire, -1);

const int BTN1 = 2;
const int BTN2 = 10;

void setup() {
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
}

void loop() {
  bool pressedRight = (digitalRead(BTN1) == LOW);
  bool pressedLeft = (digitalRead(BTN2) == LOW);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(20, 20);

  if (pressedRight) {
    display.println("> right button");
  } 
  else if (pressedLeft)
  {
    display.println("> left button");
  }
  else {
    display.println("Hello");
    display.println("World");
  }

  display.display();
  delay(50);
}