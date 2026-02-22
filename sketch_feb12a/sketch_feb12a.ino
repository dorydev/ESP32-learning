#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(128, 64, &Wire, -1);

int pins[] = {3, 2, 1, 0};

void setup() {
	for (int j = 0; j < 4; j++)
	{
		pinMode(pins[j], OUTPUT);
	}
	display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
}

void showBitsOnOLED(int i) {
  int b0 = (i >> 0) & 1;
  int b1 = (i >> 1) & 1;
  int b2 = (i >> 2) & 1;
  int b3 = (i >> 3) & 1;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Q4 Q3 Q2 Q1");
  display.println(" ");

  // Values line
  display.setTextSize(2);
  display.setCursor(2, 16);

  display.print(b3); display.print("  ");
  display.print(b2); display.print("  ");
  display.print(b1); display.print("  ");
  display.print(b0);

  display.display();
}


void loop() {

	// 4 bits comp
	for (int i = 0; i < 16; i++)
	{
		for (int k = 0; k < 4; k++)
		{
			digitalWrite(pins[k], (i >> k) & 1);
		}
		showBitsOnOLED(i);
		delay(500);
	}
}

