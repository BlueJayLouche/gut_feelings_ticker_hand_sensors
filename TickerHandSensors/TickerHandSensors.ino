/*******************************************************************************

 Bare Conductive MPR121 library
 ------------------------------
 
 SimpleTouch.ino - simple MPR121 touch detection demo with serial output
 
 Based on code by Jim Lindblom and plenty of inspiration from the Freescale 
 Semiconductor datasheets and application notes.
 
 Bare Conductive code written by Stefan Dzisiewski-Smith and Peter Krige.
 
 This work is licensed under a MIT license https://opensource.org/licenses/MIT
 
 Copyright (c) 2016, Bare Conductive
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

Modified by AC - 12/02/19 (adding FastLED integration)

*******************************************************************************/

#include <MPR121.h>
#include <Wire.h>
#include <FastLED.h>
/*
extern const TProgmemRGBPalette16 scanColors_p FL_PROGMEM =
{
    0xE8927C, 0x332113, 0xE8927C, 0xE8927C, 0x332113, 
    0xE8927C, 0xE8927C, 0xE8927C, 0x332113, 0xE8927C, 
    0xE8927C, 0x332113, 0xE8927C, 0xE8927C, 0x332113,
    0x332113
};

extern const TProgmemRGBPalette16 waitColors_p FL_PROGMEM =
{
    0x115E67, 0x115E67, 0x97D4E0, 0x115E67, 0x115E67, 
    0x115E67, 0x97D4E0, 0x115E67, 0x115E67, 0x97D4E0, 
    0x115E67, 0x115E67, 0x115E67, 0x97D4E0, 0x115E67,
    0x115E67
};
*/

extern const TProgmemRGBPalette16 scanColors_p FL_PROGMEM =
{
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 
    0xFFFFFF
};

extern const TProgmemRGBPalette16 waitColors_p FL_PROGMEM =
{
    0x888888, 0x888888, 0x888888, 0x888888, 0x888888, 
    0x888888, 0x888888, 0x888888, 0x888888, 0x888888, 
    0x888888, 0x888888, 0x888888, 0x888888, 0x888888, 
    0x888888
};

FASTLED_USING_NAMESPACE

#define numElectrodes 7

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN    8
//#define CLK_PIN   4
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS_PANEL 18
#define NUM_LEDS    (NUM_LEDS_PANEL*numElectrodes)
CRGB leds[NUM_LEDS];

#define BRIGHTNESS          255
#define FRAMES_PER_SECOND   144

int panelArray[numElectrodes] = {
  0,
  NUM_LEDS_PANEL,
  NUM_LEDS_PANEL*2,  
  NUM_LEDS_PANEL*3,
  NUM_LEDS_PANEL*4,
  NUM_LEDS_PANEL*5,
  NUM_LEDS_PANEL*6
};
uint8_t hue = 0;
uint8_t sat = 255;
uint8_t val = 0;

bool hands[numElectrodes] = {false};
bool fadeDirection = 1;  // [1=fade up, 0=fade down]

void setup()
{
  delay(3000); // 3 second delay for recovery
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(0xDDEEFF);
  //FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  
  Serial.begin(9600);
  while(!Serial);  // only needed if you want serial feedback 
  
  // 0x5C is the MPR121 I2C address on the Bare Touch Board
  // 0x5A for Teensy 3.2
  if(!MPR121.begin(0x5A)){ 
    Serial.println("error setting up MPR121");  
    switch(MPR121.getError()){
      case NO_ERROR:
        Serial.println("no error");
        break;  
      case ADDRESS_UNKNOWN:
        Serial.println("incorrect address");
        break;
      case READBACK_FAIL:
        Serial.println("readback failure");
        break;
      case OVERCURRENT_FLAG:
        Serial.println("overcurrent on REXT pin");
        break;      
      case OUT_OF_RANGE:
        Serial.println("electrode out of range");
        break;
      case NOT_INITED:
        Serial.println("not initialised");
        break;
      default:
        Serial.println("unknown error");
        break;      
    }
    while(1);
  }
  
  // pin 4 is the MPR121 interrupt
  MPR121.setInterruptPin(2);

  // this is the touch threshold - setting it low makes it more like a proximity trigger
  // default value is 40 for touch
  MPR121.setTouchThreshold(20);
  
  // this is the release threshold - must ALWAYS be smaller than the touch threshold
  // default value is 20 for touch
  MPR121.setReleaseThreshold(10);  

  // initial data update
  MPR121.updateTouchData();
}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void loop()
{
  if(MPR121.touchStatusChanged()){
    MPR121.updateTouchData();
    for(int i=0; i<numElectrodes; i++){
      if(MPR121.isNewTouch(i)){
        //Serial.print("electrode ");
        //Serial.print(i, DEC);
        //Serial.println(" was just touched");
        //Serial.println(1);
        hands[i] = true;   
        Serial.print(hands[0]);
        Serial.print(hands[1]);
        Serial.print(hands[2]);
        Serial.print(hands[3]);
        Serial.print(hands[4]);
        Serial.print(hands[5]);
        Serial.println(hands[6]);//hands[1]hands[2]hands[3],hands[4]);
      } 
      else if(MPR121.isNewRelease(i)){
        //Serial.print("electrode ");
        //Serial.print(i, DEC);
        //Serial.println(" was just released");
        //Serial.println(0);
        hands[i] = false;
        Serial.print(hands[0]);
        Serial.print(hands[1]);
        Serial.print(hands[2]);
        Serial.print(hands[3]);
        Serial.print(hands[4]);
        Serial.print(hands[5]);
        Serial.println(hands[6]);
      }
    }
  }
  
  for(int i=0; i<numElectrodes; i++){
    if(hands[i] == true)  {
      bpm(scanColors_p, i, 62);
    }
    else  {
      bpm(waitColors_p, i, 10);
    }
  }
  
  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void bpm(CRGBPalette16 palette, int hand, uint8_t BeatsPerMinute)
{
  uint8_t firstLED = panelArray[hand];
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
//  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS_PANEL; i++) { //9948
    leds[i + firstLED] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

//void  fadeLED(int hand)
//{
//  uint8_t firstLED = panelArray[hand];
//  
//  if (fadeDirection == 1) {  //fade up
//    EVERY_N_MILLISECONDS(3){
//      for( int i = 0; i < NUM_LEDS_PANEL; i++) { //9948
//        leds[i + firstLED] = fill_solid( CHSV(hue,sat,val) );
//      }
//    }
//      val = val + 1;
//      if (val == 255) {
//        fadeDirection = !fadeDirection;  //reverse direction
//      }
//    }
//  }
//
//  if (fadeDirection == 0) {  //fade down
//    EVERY_N_MILLISECONDS(9){
//      for( int i = 0; i < NUM_LEDS_PANEL; i++) { //9948
//        leds[i + firstLED] = fill_solid( CHSV(hue,sat,val) );
//      }
//      val = val - 1;
//      if (val == 0) {
//        fadeDirection = !fadeDirection;  //reverse direction
//      }
//    }
//  }
//}
