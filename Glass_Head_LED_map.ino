/* Glass Head art project using an Arduino Spark Fun Pro Micro 5V/16MHz, two MSGEQ7 ICs connected to a stereo input jack,
    and 72 LEDs w/ WS2812 LEDs wired into a glass mead

    THIS IS THE TEST CODE TO HELP MAP ALL THE LED positions
    
    strip.setPixelColor(n, red, green, blue);
    strip.setPixelColor(n, color);
    strip.show();
    strip.setBrightness(brightness);  sets the brightness of the entire strip! 0 to 255
    uint32_t color = strip.getPixelColor(11);
    uint16_t n = strip.numPixels();
    
*/

#include <Adafruit_NeoPixel.h>  //The library file for the WS2812 chips

// Number of RGB LEDs in strand:
const int nLEDs = 72;
byte colorMode = 0;    // cycles through the different colors
byte ledCounter = 1;  // moves LED position up and down, using the left and right touch sensors 


#define  TRIGGER  3 // digital output debug port to help trigger scope

#define  KNIFEL   6 // digital input connected to the front knife switch left position
#define  KNIFER   7 // digital input connected to the front knife switch right position
#define  TOUCHL   8 // digital input connected to the left touch switch
#define  TOUCHR   9 // digital input connected to the right touch switch
#define  TOUCHC  10 // digital input connected to the center touch switch

#define  DATAPIN  2 // the LED strip data pin

byte colorR[7] = {0xFF, 0xEF, 0xBF, 0x00, 0x00, 0x00, 0x00}; //Rainbow Red to Blue
byte colorG[7] = {0x00, 0x3F, 0xBF, 0xFF, 0xFF, 0x3F, 0x00}; //Rainbow Red to Blue
byte colorB[7] = {0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF}; //Rainbow Red to Blue

byte Right2Left[72] = {30,49,66,51,50,7,6,5,8,29,48,47,52,28,27,65,31,53,10,9,67,63,46,4,3,12,11,62,72,33,34,64,26,32
     ,54,35,69,14,36,13,70,71,2,61,15,37,55,45,44,1,22,25,42,17,16,56,24,57,23,60,43,18,19,68,58,38,20,39,21,59,41,40};

byte Bottom2Top[72] = {22,4,2,9,6,10,11,13,14,16,20,33,1,3,5,7,8,12,15,17,18,19,21,23,24,26,28,31,32,34,25,27,29,30,35
     ,36,37,39,40,42,45,46,50,51,38,41,44,47,48,49,52,53,55,62,63,43,54,56,57,58,59,60,61,64,65,66,67,68,69,70,71,72};

struct SWITCH {  // a structure to hold the input switches
  byte knifeleft;
  byte kniferight;
  byte touchleft;
  byte touchright;
  byte touchcenter;
};
struct SWITCH switchesNow = {0, 0, 0, 0, 0};  // define a structure to hold the current switch values
struct SWITCH switchesLast = {0, 0, 0, 0, 0};  // define a structure to hold the previous switch values

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(nLEDs, DATAPIN, NEO_GRB + NEO_KHZ800);


// setup - initialize the I/O
void setup()
{
  Serial.begin(9600);  // remove as comment to turn on the Serial port
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  pinMode(TRIGGER, OUTPUT); // use to help debug code
  pinMode(KNIFEL, INPUT);    // input from the selection switch
  pinMode(KNIFER, INPUT);   // input from the selection switch
  pinMode(TOUCHL, INPUT);   // digital input connected to the left touch switch
  pinMode(TOUCHR, INPUT);   // digital input connected to the right touch switch
  pinMode(TOUCHC, INPUT);   // digital input connected to the center touch switch
  
  delay(500);  //wait 0.5 second
}


// Main execution loop
void loop() {
// Initialize all variables
  
// Code start ************************************************************************************************
  switchesNow = debounceSwitch();     // read the input switches:

// process the color mode select switch - Touch center
  if (switchesNow.touchcenter == 0 && switchesLast.touchcenter == 1) {  // high to low means it was touched
    colorMode = (colorMode + 1) % 8;  // increment color on touch = 0 to 7 and wrap around
    switchesLast.touchcenter = switchesNow.touchcenter;
  }
  if (switchesNow.touchcenter == 1 && switchesLast.touchcenter == 0) {  // low to high means it was released
    switchesLast.touchcenter = switchesNow.touchcenter;
  }  

  // process the LEFT LED position switch
  if (switchesNow.touchleft == 0 && switchesLast.touchleft == 1) {  // high to low means it was touched
    ledCounter = ledCounter + 1;    // increment gain counter on Low to High
    switchesLast.touchleft = switchesNow.touchleft;
  }
  if (switchesNow.touchleft == 1 && switchesLast.touchleft == 0) {  // low to high means it was released
    switchesLast.touchleft = switchesNow.touchleft;
  }  

  // process the RIGHT LED position switch
  if (switchesNow.touchright == 0 && switchesLast.touchright == 1) {  // high to low means it was touched
    ledCounter = ledCounter - 1;    // decrement the gain counter on Low to High
    switchesLast.touchright = switchesNow.touchright;
  }
  if (switchesNow.touchright == 1 && switchesLast.touchright == 0) {  // low to high means it was released
    switchesLast.touchright = switchesNow.touchright;
  }  

  if (switchesNow.knifeleft == 0) {
    ledCounter = ledCounter + 1;
  }
  
  if (switchesNow.kniferight == 0) {
    ledCounter = ledCounter - 1;
  }


  if (ledCounter > 72){  // look for maximum LED position
    ledCounter = 72;
  }
  if (ledCounter < 1){  // look for minimum LED position
    ledCounter = 1; 
  }


/*  for (int i = 0; i < 72; i++){
    strip.setPixelColor(Right2Left[i] - 1, colorR[colorMode], colorG[colorMode], colorB[colorMode]);
    strip.show();   //show the new strip colors - FYI - takes rough 4.66 milliseconds on the Uno
    delay(10);
  }
*/

  Serial.print("Colormode:");
  Serial.print(colorMode);
  Serial.print("    LED:");
  Serial.println(ledCounter);

  for (int i = 0; i < 72; i++){   // clear the strip
    strip.setPixelColor(i, 0, 0, 0);
  }
  strip.show();
  
  strip.setPixelColor(ledCounter-1, colorR[colorMode], colorG[colorMode], colorB[colorMode]);

  strip.show();   //show the new strip colors - FYI - takes rough 4.66 milliseconds on the Uno
  delay(50);

   
}  // END of Main Loop ***************************************************************************************


/*******************************************************************************************/
/* debounceSwitch: reads all switch inputs 64 times then divides by 50 to return a "1" if
/*   if input is high 50 or greater times.
/*******************************************************************************************/
struct SWITCH debounceSwitch () {
  struct SWITCH sw = {0,0,0,0,0};
  
  for (int i = 0; i < 64; i++){
    sw.knifeleft = sw.knifeleft + digitalRead(KNIFEL);
    sw.kniferight = sw.kniferight + digitalRead(KNIFER);
    sw.touchleft = sw.touchleft + digitalRead(TOUCHL);
    sw.touchright = sw.touchright + digitalRead(TOUCHR);
    sw.touchcenter = sw.touchcenter + digitalRead(TOUCHC);
  }

  sw.knifeleft = sw.knifeleft / 50;
  sw.kniferight = sw.kniferight / 50;
  sw.touchleft = sw.touchleft / 50;
  sw.touchright = sw.touchright / 50;
  sw.touchcenter = sw.touchcenter / 50;
  
  return (sw);
}

