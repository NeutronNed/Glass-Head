/* Glass Head art project using an Arduino Spark Fun Pro Micro 5V/16MHz, two MSGEQ7 ICs connected to a stereo input jack,
    two microphone modules and 72 LEDs w/ WS2812 LEDs wired into a glass mead

    Analog signal is filtered by the MSGEQ7 into 7 channels of DC peak values
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
byte brightnessMode = 2;    // display brightness =  0=Low - 1=Medium - 2=Bright - start bright
byte colorMap = 85;          // the offset position for the color map 
byte colorScale = 2;        // the variation in the color map - 0=one color, 3=full spectrum

#define  AUDIOL   0 // Left channel from the MSGEQ7 board= 0.3V + peak signal
#define  AUDIOR   1 // Right channel from the MSGEQ7 board= 0.3V + peak signal

#define  TRIGGER  3 // digital output debug port to help trigger scope
#define  STROBE   4 // digital output connected to the Strobe line on the MSGEQ7 board
#define  RESET    5 // digital output connected to the Reset Line on the MSGEQ7 board

#define  KNIFEL   6 // digital input connected to the front knife switch left position
#define  KNIFER   7 // digital input connected to the front knife switch right position
#define  TOUCHL   8 // digital input connected to the left touch switch
#define  TOUCHR   9 // digital input connected to the right touch switch
#define  TOUCHC  10 // digital input connected to the center touch switch

#define  SAMPLES 8 // number of analog samples to take into the average
#define  OFFSET  60 // the DC offset seen on the output of the MSGEQ7 (300 to 600mV nominal)
#define  DATAPIN  2 // the LED strip data pin

#define PEAKCOUNT 2      //the number of samples to look at for peak detection
#define AVERAGECOUNT 4    //the number of sample to average

byte brightnessLevel[3] = {20, 100, 255};

byte colorR[7] = {0xFF, 0xEF, 0xBF, 0x00, 0x00, 0x00, 0x00}; //Rainbow Red to Blue
byte colorG[7] = {0x00, 0x3F, 0xBF, 0xFF, 0xFF, 0x3F, 0x00}; //Rainbow Red to Blue
byte colorB[7] = {0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF}; //Rainbow Red to Blue

byte Right2Left[72] = {29,48,65,50,49,6,5,4,7,28,47,46,51,27,26,64,30,52,9,8,66,62,45,3,2,11,10,61,71,32,33,63,25,31
     ,53,34,68,13,35,12,69,70,1,60,14,36,54,44,43,0,21,24,41,16,15,55,23,56,22,59,42,17,18,67,57,37,19,38,20,58,40,39};

byte Bottom2Top[72] = {21,3,1,8,5,9,10,12,13,15,19,32,0,2,4,6,7,11,14,16,17,18,20,22,23,25,27,30,31,33,24,26,28,29,34
     ,35,36,38,39,41,44,45,49,50,37,40,43,46,47,48,51,52,54,61,62,42,53,55,56,57,58,59,60,63,64,65,66,67,68,69,70,71};
     
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
//  Serial.begin(9600);  // remove as comment to turn on the Serial port
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  pinMode(TRIGGER, OUTPUT); // use to help debug code
  pinMode(STROBE, OUTPUT);  // strobe pin on the MSGEQ7 chip
  pinMode(RESET, OUTPUT);   // reset pin on the MSGEQ7 chip
  pinMode(KNIFEL, INPUT);    // input from the selection switch
  pinMode(KNIFER, INPUT);   // input from the selection switch
  pinMode(TOUCHL, INPUT);   // digital input connected to the left touch switch
  pinMode(TOUCHR, INPUT);   // digital input connected to the right touch switch
  pinMode(TOUCHC, INPUT);   // digital input connected to the center touch switch
  
  digitalWrite(TRIGGER, LOW);  //start with the trigger pin low
  digitalWrite(RESET, LOW);   // start with both control lines to the MSGEQ7 chip low
  digitalWrite(STROBE, LOW);  // 
  
  delay(1000);  //wait 1 second
  
  digitalWrite(RESET, HIGH);  // set the Reset line high
  delay(1);  //wait 1 millisecond
  digitalWrite(RESET, LOW);  // set the Reset line low 
  delay(1);  //wait 1 millisecond - chip now ready for strobing through the outputs
}

// Main execution loop
void loop() {
// Initialize all variables
  int spectrumL[7] = {0, 0, 0, 0, 0, 0, 0}; // stored values for the left channel spectrum
  int spectrumR[7] = {0, 0, 0, 0, 0, 0, 0}; // values for the right side
  int spectrumAVG[7] = {0, 0, 0, 0, 0, 0, 0}; // the average of the two sides
  int spectrumINT[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // interpolated 
  

  
// Code start ************************************************************************************************
  switchesNow = debounceSwitch();     // read the input switches:

// process the color brightness select switch - Touch center
  if (switchesNow.touchcenter == 0 && switchesLast.touchcenter == 1) {  // high to low means it was touched
    brightnessMode = (brightnessMode + 1) % 3;  // increment brightness on touch = 0, 1, 2 and wrap around
    switchesLast.touchcenter = switchesNow.touchcenter;
  }
  if (switchesNow.touchcenter == 1 && switchesLast.touchcenter == 0) {  // low to high means it was released
    switchesLast.touchcenter = switchesNow.touchcenter;
  }  

  // process the LEFT gain switch
  if (switchesNow.touchleft == 0 ) {  // low means it was touched
    colorMap = (colorMap + 1) % 255;    // increment color Map index counter
  }

  // process the RIGHT gain switch
  if (switchesNow.touchright == 0 && switchesLast.touchright == 1) {  // high to low means it was touched
    colorScale = (colorScale + 1) % 4;  // increment color scale on touch - 0 through 3 and wrap around
    switchesLast.touchright = switchesNow.touchright;
  }
  if (switchesNow.touchright == 1 && switchesLast.touchright == 0) {  // low to high means it was released
    switchesLast.touchright = switchesNow.touchright;
  }  


// Read spectrum analyzer chips ******************************************************************************
  digitalWrite(RESET, HIGH);    // set the Reset line high to initialize the chips' internal multiplexor
  delayMicroseconds(100);       // wait 100 microseconds
  digitalWrite(RESET, LOW);     // set the Reset line low 
  delayMicroseconds(100);       // wait 100 microsecond - chip now ready for strobing through the outputs
  
  for (byte i = 0; i < 7; i++){   // cycle through the seven channels
    digitalWrite(STROBE, HIGH);   // Strobe High to switch to the next analog channel
    delayMicroseconds(100);       // wait 150 microseconds to allow the output to settle - minimum 36 usec
    digitalWrite(STROBE, LOW);    // set the Strobe line low for the next time
    delayMicroseconds(200);       // wait 200 microseconds for the analog signal to stabilize
    
    spectrumL[i] = averageDetect(AUDIOL, OFFSET, SAMPLES);  // fill the spectrum matrix with left side values
    spectrumR[i] = averageDetect(AUDIOR, OFFSET, SAMPLES);  // same for right side
    spectrumAVG[i] = (spectrumL[i] + spectrumL[i]) / 2;  // calculate the average of the Right and Left sides
  }

  spectrumINT[0] = spectrumAVG[0];
  spectrumINT[1] = (spectrumAVG[0] + spectrumAVG[1]) / 2;
  spectrumINT[2] = spectrumAVG[1];
  spectrumINT[3] = (spectrumAVG[1] + spectrumAVG[2]) / 2;
  spectrumINT[4] = spectrumAVG[2];
  spectrumINT[5] = (spectrumAVG[2] + spectrumAVG[3]) / 2;
  spectrumINT[6] = spectrumAVG[3];
  spectrumINT[7] = (spectrumAVG[3] + spectrumAVG[4]) / 2;
  spectrumINT[8] = spectrumAVG[4];
  spectrumINT[9] = (spectrumAVG[4] + spectrumAVG[5]) / 2;
  spectrumINT[10] = spectrumAVG[5];
  spectrumINT[11] = (spectrumAVG[5] + spectrumAVG[6]) / 2;
  spectrumINT[12] = spectrumAVG[6];
  
  
// We now have all the inputs we need to display  - THIS IS WHERE THE SERIAL PRINT COMMANDS WOULD GO


// Set the overall brightness of the LEDs based on the center touch switch ***********************************
  strip.setBrightness(brightnessLevel[brightnessMode]);
  
// Output the appropriate colors *****************************************************************************
  for (int i = 0; i < 72; i++){
    byte Color = i * colorScale + colorMap;  // this results in a color map position
    byte Channel = i / 6 ;   // there are 13 interpolated channels, 0 through 12.  
    
    
    byte Red = (WheelR(Color) * spectrumINT[Channel]) / 1024; // brightness proportionally to the loudness
    byte Green = (WheelG(Color) * spectrumINT[Channel]) / 1024;
    byte Blue = (WheelB(Color) * spectrumINT[Channel]) / 1024;
    
    if (switchesNow.kniferight == 0 && switchesNow.knifeleft == 1) {  // knife switch to the right
      strip.setPixelColor(Bottom2Top[i], Red, Green, Blue);  //   color the pixel with the given color
    }
    if (switchesNow.kniferight == 1 && switchesNow.knifeleft == 0) {  // knife switch to the left
      strip.setPixelColor(Right2Left[i], Red, Green, Blue);  //   color the pixel with the given color
    }
    if (switchesNow.kniferight == 1 && switchesNow.knifeleft == 1) {  // knife switch between sides
      strip.setPixelColor(Bottom2Top[i], WheelR(Color), WheelG(Color), WheelB(Color));  // demo
    }

  }
  
  strip.show();   //show the new strip colors - FYI - takes rough 4.66 milliseconds on the Uno
   
}  // END of Main Loop ***************************************************************************************


/*******************************************************************************************/
/* averageDetect: Get the average analog voltage on the analog input pin
/*******************************************************************************************/
int averageDetect(int analogIn, int offset, int samples){
 
  long int sum = 0;         // initial register to hold the sum
  int sensorValue = 0;      // analog input sample (A/D= 0 to 1023, 0 - 5V)

  for (int i = 0; i < samples; i++){                    // read analog values and find the average
    sensorValue = abs(analogRead(analogIn) - offset);   // read analog voltage (range 0 to 1024) and subtract offset
    if (sensorValue < 0) {sensorValue = 0;}             // if less than zero, set to zero
    if (sensorValue > 1023) {sensorValue = 1023;}       // if greater than 1023, set to 1023

    sum = sum + sensorValue;        // create a sum of the values
  }
  
  return (sum / samples);    //divide by the number of samples
}


/*******************************************************************************************/
/* debounceSwitch: reads all switch inputs 32 times then divides by 25 to return a "1" if
/*   if input is high 25 or greater times.
/*******************************************************************************************/
struct SWITCH debounceSwitch () {
  struct SWITCH sw = {0,0,0,0,0};
  
  for (int i = 0; i < 32; i++){
    sw.knifeleft = sw.knifeleft + digitalRead(KNIFEL);
    sw.kniferight = sw.kniferight + digitalRead(KNIFER);
    sw.touchleft = sw.touchleft + digitalRead(TOUCHL);
    sw.touchright = sw.touchright + digitalRead(TOUCHR);
    sw.touchcenter = sw.touchcenter + digitalRead(TOUCHC);
  }

  sw.knifeleft = sw.knifeleft / 25;
  sw.kniferight = sw.kniferight / 25;
  sw.touchleft = sw.touchleft / 25;
  sw.touchright = sw.touchright / 25;
  sw.touchcenter = sw.touchcenter / 25;
  
  return (sw);
}

// LED Strip color routines *******************************************************************************


/*******************************************************************************************/
/* colorLine: draws a solid line of LEDS from the "startPos" value to the "endPos" value
/*******************************************************************************************/
void colorLine(int startPos, int endPos, byte R, byte G, byte B){

  for (int i = 0; i < nLEDs; i++){      // increment from the start to the end of the strip
   
    if (i >= startPos && i<= endPos){   // if the position is between the given positions,
      strip.setPixelColor(Bottom2Top[i] - 1, R, B, G);  //   color the pixel with the given color
                       // Note:  this routine has the color specified Red, Blue, Green
    }
//    else {                              // else, turn off all the pixels outside the range
//      strip.setPixelColor(i, 0, 0, 0); 
//    }
  }

}


/*******************************************************************************************/
/* colorChase: Chase one dot down the full strip.
/*******************************************************************************************/
void colorChase(uint32_t c, uint8_t wait) {
  int i;

  // Start by turning all pixels off:
  for(i=0; i<strip.numPixels(); i++) strip.setPixelColor(i, 0);

  // Then display one pixel at a time:
  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c); // Set new pixel 'on'
    strip.show();              // Refresh LED states
    strip.setPixelColor(i, 0); // Erase pixel, but don't refresh!
    delay(wait);
  }

  strip.show(); // Refresh to turn off last pixel
}


/*******************************************************************************************/
/* colorWipe: Fill the dots one after the other with a color.
/*******************************************************************************************/
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}


/*******************************************************************************************/
/* rainbow: fills the strip with a rotating rainbow of colors.  NOTE: takes a long time
/*******************************************************************************************/
void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

/*******************************************************************************************/
/* rainbowCycle: Slightly different, this makes the rainbow equally distributed throughout.  
/*    NOTE: takes a long time
/*******************************************************************************************/
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}


/*******************************************************************************************/
/* Wheel: standard color look-up routine - Input a value 0 to 255 to get a returned color value. 
/*    NOTE: The colors are a transition r - g - b - back to r.
/*******************************************************************************************/
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
/*******************************************************************************************/
/* WheelR: same as above but just returned the Red byte (not the entire RGB word)          */
byte WheelR(byte WheelPos) {
  if(WheelPos < 85) {
    return WheelPos * 3; 
  } 
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return 255 - WheelPos * 3;
  } else {
    return 0;
  }
}
/*******************************************************************************************/
/* WheelG: same as above but just returned the Green byte (not the entire RGB word)        */
byte WheelG(byte WheelPos) {
  if(WheelPos < 85) {
   return 255 - WheelPos * 3;
  } else if(WheelPos < 170) {
   return  0;
  } else {
   WheelPos -= 170;
   return  WheelPos * 3;
  }
}
/*******************************************************************************************/
/* WheelB: same as above but just returned the Blue byte (not the entire RGB word)         */
byte WheelB(byte WheelPos) {
  if(WheelPos < 85) {
   return 0;
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return  WheelPos * 3;
  } else {
   WheelPos -= 170;
   return  255 - WheelPos * 3;
  }
}

/*******************************************************************************************/
/* DebugPrints: prints all the operating values  
/*    NOTE: CUT&PASTE back into the main operating loop
/*******************************************************************************************/
/*
  Serial.print("Mic SW:");
  Serial.print(switchesNow.micinput);
  Serial.print("    Jack SW:");
  Serial.print(switchesNow.jackinput);
  Serial.print("    Touch L:");
  Serial.print(switchesNow.touchleft);
  Serial.print("    Touch R:");
  Serial.print(switchesNow.touchright);
  Serial.print("    Touch C:");
  Serial.println(switchesNow.touchcenter);  

  Serial.print("Colormode:");
  Serial.print(colorMode);
  Serial.print("    Gain:");
  Serial.println(gainCounter);

  Serial.print("Mic Left:");
  Serial.print(micaudioL);
  Serial.print("    Mic Right:");
  Serial.print(micaudioR);
  Serial.print("    Mic AVG:");
  Serial.println(micaudioAVG);
  
  Serial.print("SpecAnalyzer Left:");
  for (byte i = 0; i < 7; i++){
    Serial.print(spectrumL[i]);
    Serial.print("    ");
  }
  Serial.println(" ");

  Serial.print("SpecAnalyzer Right:");
  for (byte i = 0; i < 7; i++){
    Serial.print(spectrumR[i]);
    Serial.print("    ");
  }
  Serial.println(" ");

  Serial.print("SpecAnalyzer AVG:");
  for (byte i = 0; i < 7; i++){
    Serial.print(spectrumAVG[i]);
    Serial.print("    ");
  }
    Serial.println(" ");
*/

/*  OLD CODE
  // process the LEFT gain switch
  if (switchesNow.touchleft == 0 && switchesLast.touchleft == 1) {  // high to low means it was touched
    gainCounter = gainCounter + 1;    // increment gain counter on Low to High
    switchesLast.touchleft = switchesNow.touchleft;
  }
  if (switchesNow.touchleft == 1 && switchesLast.touchleft == 0) {  // low to high means it was released
    switchesLast.touchleft = switchesNow.touchleft;
  }  

  // process the RIGHT gain switch
  if (switchesNow.touchright == 0 && switchesLast.touchright == 1) {  // high to low means it was touched
    gainCounter = gainCounter - 1;    // decrement the gain counter on Low to High
    switchesLast.touchright = switchesNow.touchright;
  }
  if (switchesNow.touchright == 1 && switchesLast.touchright == 0) {  // low to high means it was released
    switchesLast.touchright = switchesNow.touchright;
  }  



    byte colorRed = (colorR[0] * (spectrumAVG[0] + spectrumAVG[1])) / 2049;
    byte colorGreen = (colorG[0] * (spectrumAVG[0] + spectrumAVG[1])) / 2049;
    byte colorBlue = (colorB[0] * (spectrumAVG[0] + spectrumAVG[1])) / 2049;
    colorLine(0, 22, colorRed, colorGreen, colorBlue);
    

    colorRed = (colorR[3] * (spectrumAVG[2] + spectrumAVG[3])) / 2049;
    colorGreen = (colorG[3] * (spectrumAVG[2] + spectrumAVG[3])) / 2049;
    colorBlue = (colorB[3] * (spectrumAVG[2] + spectrumAVG[3])) / 2049;
    colorLine(23, 48, colorRed, colorGreen, colorBlue);
    

    colorRed = (colorR[5] * (spectrumAVG[4] + spectrumAVG[5])) / 2049;
    colorGreen = (colorG[5] * (spectrumAVG[4] + spectrumAVG[5])) / 2049;
    colorBlue = (colorB[5] * (spectrumAVG[4] + spectrumAVG[5])) / 2049;
    colorLine(49, 71, colorRed, colorGreen, colorBlue);
*/
