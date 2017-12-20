/*
 Main code by Richard Visokey AD7C - www.ad7c.com
 Revision 3.0 - July 2014 by IZ2ZPH
 Revision 4.0 - Agoust 2014 - Oled arrangement by IW2ESL
 */
 
// Include the library code
#include <rotary.h>
#include <EEPROM.h>
#include "U8glib.h"

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);	// I2C / TWI
char OledLine1[17]; // CON QUESTO FONT SONO 16 CARATTERI
char OledLine2[17];
char OledLine3[17];
char OledLine4[17];

//Setup some items
//dds AD9850 I/FACE
#define W_CLK 1   // Pin 8 - connect to AD9850 module word load clock pin (CLK)
#define FQ_UD 11  // Pin 9 - connect to freq update pin (FQ)
#define DATA 12   // Pin 10 - connect to serial data load pin (DATA)
#define RESET 13  // Pin 11 - connect to reset pin (RST) 
#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }

//Rotary Encoder
Rotary r = Rotary(2, 3); // sets the pins the rotary encoder uses.  Must be interrupt pins.

//steps label
char* stepStr[] = {
  "  1  Hz", " 10  Hz", "100  Hz", "  1 KHz", "2.5 KHz", "  5 KHz", " 10 KHz", " 50 KHz", "100 KHz", "500 KHz", "  1 MHz", " 10 MHz"
};

//steps values
int_fast32_t stepVal[] =  {
  1, 10, 100, 1000, 2500, 5000, 10000, 50000, 100000, 500000, 1000000, 10000000
};

//band labels
char* bandStr[] = {
  "160m", " 80m", " 40m", " 30m", " 20m", " 17m", " 15m", " 12m", " 10m"
};
//band freq
int_fast32_t bandVal[] =  {1830000, 3500000, 7000000, 10100000, 14000000, 18068000, 21000000, 24890000, 28000000};


int_fast32_t scanStart = 0;
int_fast32_t scanStop = 0;

int_fast32_t scanCnt = 0;
int_fast32_t rx3 = 0;


int lcd_key     = 0;
int tasto_stop  = 0;
int adc_key_in  = 0;
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5


//default step (10 hz)
byte stepPnt = 10;
//default band (40m)
byte band = 2;
//DDS Limits
int_fast32_t lowLimit = 1000000;
int_fast32_t upLimit = 30000000;


int_fast32_t rx = 5000000; // Poweron frequency of VFO
int_fast32_t rx2 = 1; // variable to hold the updated frequency
int_fast32_t isStep = 10; // starting VFO update increment in HZ.
//keypad stats
int buttonSt = 0;
int stopState = 0;
String hertz = stepStr[stepPnt];

byte ones, tens, hundreds, thousands, tenthousands, hundredthousands, millions ; //Placeholders
String freq; // string to hold the frequency
String xstepPnt; // string to hold the step
int_fast32_t timepassed = millis(); // int to hold the arduino miilis since startup
int memstatus = 1;  // value to notify if memory is current or old. 0=old, 1=current.

int ForceFreq = 1;  // Change this to 0 after you upload and run a working sketch to activate the EEPROM memory.
                    // YOU MUST PUT THIS BACK TO 0 AND UPLOAD THE SKETCH AGAIN AFTER STARTING FREQUENCY IS SET!

/**********************
 * SETUP
 **********************/
void setup() {
  
  // OLED SETUP
  // assign default color value
  if ( u8g.getMode() == U8G_MODE_R3G3B2 ) {
    u8g.setColorIndex(255);     // white
  }
  else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) {
    u8g.setColorIndex(3);         // max intensity
  }
  else if ( u8g.getMode() == U8G_MODE_BW ) {
    u8g.setColorIndex(1);         // pixel on
  }
  else if ( u8g.getMode() == U8G_MODE_HICOLOR ) {
    u8g.setHiColorByRGB(255,255,255);
  }
  
  u8g.setFont(u8g_font_unifont);
  sprintf(OledLine1, "          IW2ESL");
  sprintf(OledLine2, "line2");
  sprintf(OledLine3, "line3");
  sprintf(OledLine4, "line4");
  
  
  pinMode(A0, INPUT); // Connect to a button that goes to GND on push
  digitalWrite(A0, HIGH);

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  
  //interrupt enable
  sei();
  
  //setup DDS
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT);
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode on the AD9850 - Datasheet page 12.
  //defaults
  band = 2;
  stepPnt = 1;

  // Load the stored frequency on startup if saved

  if (ForceFreq == 0) {
    freq = String(EEPROM.read(0)) + String(EEPROM.read(1)) + String(EEPROM.read(2)) + String(EEPROM.read(3)) + String(EEPROM.read(4)) + String(EEPROM.read(5)) + String(EEPROM.read(6));
    rx = freq.toInt();
    if (rx > 0 ) {
      stepPnt =  EEPROM.read(7);
      band =  EEPROM.read(8);
    }
  }

  //init VFO
  sendFrequency(rx);
  showFreq();
  isStep = stepVal[stepPnt];
  //showHertz(stepPnt);
}

/************************************************
 * MAIN LOOP
 ************************************************/
void loop() {
  //store last freq in ram
  rx3 = rx;
  
  // DISPLAY LINES UPDATE
  u8g.firstPage();  
  do {
    draw(OledLine1,OledLine2,OledLine3,OledLine4);
  } while( u8g.nextPage() );
  
  // Write the frequency to memory if not stored and 2 seconds have passed since the last frequency change.
  if (memstatus == 0) {
    if (timepassed + 2000 < millis()) {
      storeMEM();
    }
  }
  //end main loop
}

/**********************
 * DRAW
 **********************/
void draw(char *stringPtr1,char *stringPtr2,char *stringPtr3,char *stringPtr4) {
  u8g.drawStr( 0, 14, stringPtr1);
  u8g.drawStr( 0, 30, stringPtr2);
  u8g.drawStr( 0, 45, stringPtr3);
  u8g.drawStr( 0, 62, stringPtr4);
}

/**********************
 * INTERRUPT SERVICE
 **********************/
ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result) {
    if (result == DIR_CW)
    {
      rx  = rx3;
      tuneUp();
      //tuneup
    }
    else {
      rx  = rx3;
      tuneDown();
      //tunedown
    };
  }
}

/**********************
 * TUNEUP
 **********************/
void tuneUp() {
  //tune up by step
  rx = rx + isStep;
  if (rx > upLimit) {
    rx = upLimit;
  };
  sendFrequency(rx);
  showFreq();
  delay(250);

};

/**********************
 * TUNEDOWN
 **********************/
void tuneDown() {
  // tune down by step
  rx = rx - isStep;
  if (rx <  lowLimit) {
    rx = lowLimit;
  };
  sendFrequency(rx);

  showFreq();
  delay(250);
};

void setisStep() {
  //step selector loop
  stepPnt++;
  if (stepPnt > 11) {
    stepPnt = 0;
  };
  if (stepPnt < 0) {
    stepPnt = 11;
  };

  isStep = stepVal[stepPnt];
  hertz = stepStr[stepPnt];
  showHertz(stepPnt);

  delay(250); // Adjust this delay to speed up/slow down the button menu scroll speed.
};

void setdecrement() {
  //step down
  stepPnt--;
  if (stepPnt > 11) {
    stepPnt = 0;
  };
  if (stepPnt < 0) {
    stepPnt = 11;
  };

  isStep = stepVal[stepPnt];
  hertz = stepStr[stepPnt];
  showHertz(stepPnt);

  delay(150); // Adjust this delay to speed up/slow down the button menu scroll speed.
};



void storeMEM() {
  //Write each frequency section to a EPROM slot.  Yes, it's cheating but it works!
  EEPROM.write(0, millions);
  EEPROM.write(1, hundredthousands);
  EEPROM.write(2, tenthousands);
  EEPROM.write(3, thousands);
  EEPROM.write(4, hundreds);
  EEPROM.write(5, tens);
  EEPROM.write(6, ones);
  EEPROM.write(7, stepPnt);
  EEPROM.write(8, band);
  memstatus = 1;  // Let program know memory has been written
};


//send freq to DDS
// frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
void sendFrequency(double frequency) {

  int32_t freq = frequency * 4294967295 / 125000000; // note 125 MHz clock on 9850.  You can make 'slight' tuning variations here by adjusting the clock frequency.
  for (int b = 0; b < 4; b++, freq >>= 8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x000);   // Final control byte, all 0 for 9850 chip
  pulseHigh(FQ_UD);  // Done!  Should see output
}


// transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
void tfr_byte(byte data)
{
  for (int i = 0; i < 8; i++, data >>= 1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
  }

}

/**********************
 * SHOW FREQ
 **********************/
void showFreq() {
  millions = int(rx / 1000000);
  hundredthousands = ((rx / 100000) % 10);
  tenthousands = ((rx / 10000) % 10);
  thousands = ((rx / 1000) % 10);
  hundreds = ((rx / 100) % 10);
  tens = ((rx / 10) % 10);
  ones = ((rx / 1) % 10);

  sprintf(OledLine2, "%d.%d%d%d,%d%d%d MHz", millions, hundredthousands, tenthousands, thousands, hundreds, tens, ones );

  showHertz(stepPnt);
  timepassed = millis();
  memstatus = 0; // Trigger memory write
};


void showHertz(byte stepPnt) {
  //display step label
  sprintf(OledLine3, "Step %s", stepStr[stepPnt] );
}





