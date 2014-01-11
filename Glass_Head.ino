/* Glass Head art project using an Arduino Spark Fun Pro Micro 5V/16MHz, two MSGEQ7 ICs connected to
   the left and right channels of a stereo input jack, and 72 WS2812 LEDs wired into a glass head.
   
   High-level summary:
    1) The switches are debounced and stored
    2) Any new operating states or variables are calculated
    2) The audio signal is filtered by the MSGEQ7 into 7 channels of DC peak values (for each side)
    3) An expanded set of spectrum data is calculated by first averaging the left and right sides for
       each channel and then a value between channels is calculated.
       (7 real values + 6 interpolated values = 13 total)
    4) The overall brightness for the strip is set based on the center touch switch
    5) Each of the 72 LEDS has calculated a color and intensity based on the appropriate spectrum value
    6) The strip is refreshed
    7) Repeat forever
    
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

#define  SAMPLES  8 // number of analog samples to take into the average
#define  OFFSET  60 // the DC offset seen on the output of the MSGEQ7 (300 to 600mV nominal)
#define  NOISE   16 // the analog value below which we assume is noise (keeps the display from flickering)

#define  DATAPIN  2 // the LED strip data pin

// Strip maximum brightness levels
byte brightnessLevel[3] = {20, 100, 255};

// LED Mapping tables - maps the LED number to it's true position in the glass head
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


// setup - initialize the I/O ********************************************************************************
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

// Main execution loop ***************************************************************************************
void loop() {
// Initialize all variables
  int spectrumL[7] = {0, 0, 0, 0, 0, 0, 0}; // stored values for the left channel spectrum
  int spectrumR[7] = {0, 0, 0, 0, 0, 0, 0}; // values for the right side
  byte spectrumINT[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // interpolated 
  byte spectrumAVG = 0; // average valume across all channels

  
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

  // process the LEFT touch switch
  if (switchesNow.touchleft == 0 ) {  // low means it was touched
    colorMap = (colorMap + 1) % 255;    // increment color Map index counter
  }

  // process the RIGHT touch switch
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
    
    spectrumL[i] = averageDetect(AUDIOL, OFFSET, SAMPLES, NOISE);  // fill the spectrum matrix with left side values
    spectrumR[i] = averageDetect(AUDIOR, OFFSET, SAMPLES, NOISE);  // same for right side

  }
  
  // Calculate the spectrum data to be used to drive the display
  spectrumINT[0] = (spectrumL[0] + spectrumR[0]) / 8;   // Average the Left and Right channels and scale down by /4
  spectrumINT[2] = (spectrumL[1] + spectrumR[1]) / 8;   //   Note: original values are in range of 0 to 1024 and we want
  spectrumINT[4] = (spectrumL[2] + spectrumR[2]) / 8;   //     to get to a range 0 to 256
  spectrumINT[6] = (spectrumL[3] + spectrumR[3]) / 8;
  spectrumINT[8] = (spectrumL[4] + spectrumR[4]) / 8;
  spectrumINT[10] = (spectrumL[5] + spectrumR[5]) / 8;
  spectrumINT[12] = (spectrumL[6] + spectrumR[6]) / 8;
  
  spectrumINT[1] = (spectrumINT[0] + spectrumINT[2]) / 2;  // Average the two adjecent channels to get the in-between
  spectrumINT[3] = (spectrumINT[2] + spectrumINT[4]) / 2;  //  Note: getting an interpolated spectrum value helps
  spectrumINT[5] = (spectrumINT[4] + spectrumINT[6]) / 2;  //    fill in the colors between channels so the display
  spectrumINT[7] = (spectrumINT[6] + spectrumINT[8]) / 2;  //    is smoother
  spectrumINT[9] = (spectrumINT[7] + spectrumINT[10]) / 2;
  spectrumINT[11] = (spectrumINT[10] + spectrumINT[12]) / 2;
  
  // Now calculate a single average value for the mode that flashes all the LEDs at the same intensity
  spectrumAVG = (spectrumINT[0] + spectrumINT[2] + spectrumINT[4] + spectrumINT[6] + spectrumINT[8] + spectrumINT[10] + spectrumINT[12]) / 7;
  
// We now have all the inputs we need to display

// Set the overall brightness of the LEDs based on the center touch switch ***********************************
  strip.setBrightness(brightnessLevel[brightnessMode]);
  
// Output the appropriate colors *****************************************************************************
  for (int i = 0; i < 72; i++){   // cycle through all 72 LEDs
    byte Color = i * colorScale + colorMap;  // this results in a color map position
    byte Channel = i / 6 ;   // there are 13 interpolated channels, 0 through 12.  
 
    byte Red1 = (WheelR(Color) * spectrumINT[Channel]) / 255;   // brightness proportionally to the loudness
    byte Green1 = (WheelG(Color) * spectrumINT[Channel]) / 255; //  of the individual channel
    byte Blue1 = (WheelB(Color) * spectrumINT[Channel]) / 255;
    
    byte Red2 = (WheelR(Color) * spectrumAVG) / 255;    // brightness proportionally to the loudness
    byte Green2 = (WheelG(Color) * spectrumAVG) / 255;  //   of the average level across all channels
    byte Blue2 = (WheelB(Color) * spectrumAVG) / 255;
    
    
    if (switchesNow.kniferight == 0 && switchesNow.knifeleft == 1) {  // knife switch to the right
      strip.setPixelColor(Bottom2Top[i], Red1, Green1, Blue1);  //   color the pixel with the given color
    }
    if (switchesNow.kniferight == 1 && switchesNow.knifeleft == 0) {  // knife switch to the left
      strip.setPixelColor(Right2Left[i], Red2, Green2, Blue2);  //   color the pixel with the given color
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
int averageDetect(int analogIn, int offset, int samples, int noise){
 
  long int sum = 0;         // initial register to hold the sum
  int sensorValue = 0;      // analog input sample (A/D= 0 to 1023, 0 - 5V)

  for (int i = 0; i < samples; i++){                // read analog values and find the average
    sensorValue = analogRead(analogIn) - offset;    // read analog voltage (range 0 to 1024) and subtract offset
    if (sensorValue < noise) {sensorValue = noise;}       // if less than 20, set to 20 to filter out noise
    if (sensorValue > 1023) {sensorValue = 1023;}   // if greater than 1023, set to 1023

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
