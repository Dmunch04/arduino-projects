#include <FastLED.h>

#define NUM_LEDS 360
#define UPDATE_LEDS 1
CRGB leds[NUM_LEDS];

#define DATA_PIN 6

#define DELAY 180

#define HUE_DIFF 25
#define SATURATION_DIFF 8
#define VALUE_DIFF 8

#define SATURATION_MINIMUM 220
#define VALUE_MINIMUM 230

int prevHue;
int prevSat;
int prevVal;

struct color {
  int h;
  int s;
  int v;
};
typedef struct color Color;

void setup() { 
  Serial.begin(9600);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(0, 0, 0);
  }

  prevHue = random(0, 360);
  prevSat = random(0, 255);
  prevVal = random(0, 255);

  FastLED.show();
}

void loop() { 
  for (int i = NUM_LEDS - 1; i >= UPDATE_LEDS; i--) {
    leds[i] = leds[i - UPDATE_LEDS];
  }

  Color color = findColor();

  for (int i = 0; i < UPDATE_LEDS; i++) {
    //leds[i] = CHSV(color.h, 100, 100);
    leds[i] = CHSV(color.h, 255, 255);
    //leds[i] = CHSV(color.h, color.s, color.v);
  }

  FastLED.show();

  //delay(1);
  //delay(80);
  //delay(DELAY);
  //delay(color.h);
  //delay(color.h % DELAY);

  // delay based on hue value. high vue = slow, low vue = fast
  //int del = constrain(color.h / 2, 1, 100);
  int del = map(color.h, 0, 360, 1, 100);
  Serial.println(del);
  delay(del);
}

void setColor(int h, int s, int v) {
  CHSV color = CHSV(h, s, v);
  fill_solid(leds, NUM_LEDS, color);
}

Color findColor() {
  Color color;

  int hue;
  do {
    hue = random(constrain(prevHue - HUE_DIFF, 0, 360), constrain(prevHue + HUE_DIFF, 0, 360));
    //hue = random((prevHue - HUE_DIFF) % 360, (prevHue + HUE_DIFF) % 360);
  } while(hue == prevHue);

  int sat;
  do {
    sat = random(prevSat - SATURATION_DIFF, prevSat + SATURATION_DIFF);
  } while(sat == prevSat);

  int val;
  do {
    val = random(prevVal - VALUE_DIFF, prevVal + VALUE_DIFF);
  } while(val == prevVal);

  sat = constrain(sat, SATURATION_MINIMUM, 255);
  val = constrain(val, VALUE_MINIMUM, 255);

  prevHue = hue;
  prevSat = sat;
  prevVal = val;

  setColor(&color, hue, sat, val);
  printColor(color);

  return color;
}

void setColor(Color *c, int h, int s, int v) {
  c->h = h;
  c->s = s;
  c->v = v;
}

void printColor(Color c) {
  Serial.print("(");
  Serial.print(c.h);
  Serial.print(", ");
  Serial.print(c.s);
  Serial.print(", ");
  Serial.print(c.v);
  Serial.println(")");
}
