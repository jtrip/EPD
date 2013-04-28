// -*- mode: c++ -*-
// Copyright 2013 Pervasive Displays, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at:
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
// express or implied.  See the License for the specific language
// governing permissions and limitations under the License.


// This program is to illustrate the display operation as described in
// the datasheets.  The code is in a simple linear fashion and all the
// delays are set to maximum, but the SPI clock is set lower than its
// limit.  Therfore the display sequence will be much slower than
// normal and all of the individual display stages be clearly visible.

// Modified by WyoLum to display all "*.WIF" files in the "/IMAGES/" directory.

#include <inttypes.h>
#include <ctype.h>

#include <SPI.h>
#include <SD.h>
#include "EPD.h"
#include "S5813A.h"
#include "FLASH.h"

#ifdef EPD_SMALL
  #define EPD_SIZE EPD_1_44
  #define short EPD_WIDTH 128
  #define EPD_HEIGHT 96
#elifdef EPD_MEDIUM
  #define EPD_SIZE EPD_2_0
  #define EPD_WIDTH 200
  #define EPD_HEIGHT 96
#else
  #define EPD_SIZE EPD_2_7
  #define EPD_WIDTH 264
  #define EPD_HEIGHT 176
#endif
const uint32_t EPD_BYTES = (EPD_WIDTH * EPD_HEIGHT / 8);


// configure images for display size
// change these to match display size above

// no futher changed below this point

// current version number
#define DEMO_VERSION "W"


// Add Images library to compiler path
#include <Images.h>  // this is just an empty file

// Arduino IO layout
const int Pin_TEMPERATURE = A4;
const int Pin_PANEL_ON = 2;
const int Pin_BORDER = 3;
const int Pin_DISCHARGE = 4;
const int Pin_PWM = 5;
const int Pin_RESET = 6;
const int Pin_BUSY = 7;
const int Pin_EPD_CS = 8;
const int Pin_FLASH_CS = 9;

//const int Pin_SW2 = 12;
//const int Pin_RED_LED = 13;

// LED anode through resistor to I/O pin
// LED cathode to Ground



// define the E-Ink display
EPD_Class EPD(EPD_SIZE, Pin_PANEL_ON, Pin_BORDER, Pin_DISCHARGE, Pin_PWM, Pin_RESET, Pin_BUSY, Pin_EPD_CS, SPI);

File imgFile;
File root;

// I/O setup
void setup() {

  Serial.begin(115200);
	//pinMode(Pin_RED_LED, OUTPUT);
	//pinMode(Pin_SW2, INPUT);
	pinMode(Pin_TEMPERATURE, INPUT);
	pinMode(Pin_PWM, OUTPUT);
	pinMode(Pin_BUSY, INPUT);
	pinMode(Pin_RESET, OUTPUT);
	pinMode(Pin_PANEL_ON, OUTPUT);
	pinMode(Pin_DISCHARGE, OUTPUT);
	pinMode(Pin_BORDER, OUTPUT);
	pinMode(Pin_EPD_CS, OUTPUT);
	pinMode(Pin_FLASH_CS, OUTPUT);

	//digitalWrite(Pin_RED_LED, LOW);
	digitalWrite(Pin_PWM, LOW);
	digitalWrite(Pin_RESET, LOW);
	digitalWrite(Pin_PANEL_ON, LOW);
	digitalWrite(Pin_DISCHARGE, LOW);
	digitalWrite(Pin_BORDER, LOW);
	digitalWrite(Pin_EPD_CS, LOW);
	digitalWrite(Pin_FLASH_CS, HIGH);

	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
	SPI.setClockDivider(SPI_CLOCK_DIV4);

	// wait for USB CDC serial port to connect.  Arduino Leonardo only
	while (!Serial) {
	}

	Serial.println();
	Serial.println();
	Serial.println("Demo version: " DEMO_VERSION);
	Serial.print("Display: ");
	Serial.println(EPD_SIZE);
	Serial.println();

	FLASH.begin(Pin_FLASH_CS, SPI);
	if (FLASH.available()) {
		Serial.println("FLASH chip detected OK");
	} else {
		Serial.println("unsupported FLASH chip");
	}

	// configure temperature sensor
	S5813A.begin(Pin_TEMPERATURE);
  Serial.print("Initializing SD card...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output
  // or the SD library functions will not work.
   pinMode(10, OUTPUT);

  if (!SD.begin(10)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  root = SD.open("/IMAGES");

  // queue up images
  next_image();
  char buffer1[100];
  char buffer2[100];
  for(int i = 0; i < 26; i++){
    buffer1[i] = 'A' + i;
  }
  EPD.clear();
}


static int state = 0;
char *match = ".WIF";
bool keeper(char* fn){
  byte n = strlen(fn);
  bool out = true;
  for(byte i = n - 4; i < n; i++){
    if(fn[i] != match[i - n + 4]){
      out = false;
      break;
    }
  }
  if(out){
    Serial.print(fn);
  }
  return out;
}

void next_image(){
  // copy image data to old_image data
  char buffer[EPD_WIDTH];

  erase_img(imgFile);
  imgFile.close();

  imgFile =  root.openNextFile();
  if(!imgFile){
    root.close();
    root = SD.open("/IMAGES");
    // root.rewindDirectory();
    imgFile.close();
    imgFile =  root.openNextFile();
  }
  while(!keeper(imgFile.name())){
    imgFile.close();
    imgFile =  root.openNextFile();
    if(!imgFile){
      root.close();
      root = SD.open("/IMAGES");
      //root.rewindDirectory();
      imgFile.close();
      imgFile =  root.openNextFile();
    }
  }
  if(!imgFile){
    Serial.println("new image not found");
    while(1){
      delay(1000);
    }
  }
  Serial.print("New File: ");
  Serial.println(imgFile.name());

  set_spi_for_epd();
  draw_img(imgFile);
}

//***  ensure clock is ok for EPD
void set_spi_for_epd() {
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
	SPI.setClockDivider(SPI_CLOCK_DIV4);
}


void erase_img(File imgFile){
  int temperature = S5813A.read();
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" Celcius");

  //*** maybe need to ensure clock is ok for EPD
  // set_spi_for_epd();

  EPD.begin(); // power up the EPD panel
  EPD.setFactor(temperature); // adjust for current temperature
  EPD.frame_cb_repeat(0, SD_reader, EPD_compensate);
  EPD.frame_cb_repeat(0, SD_reader, EPD_white);
  // EPD.end();   // power down the EPD panel
  
}
void draw_img(File imgFile){
  int temperature = S5813A.read();
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" Celcius");

  //*** maybe need to ensure clock is ok for EPD
  // set_spi_for_epd();

  // EPD.begin(); // power up the EPD panel
  // EPD.setFactor(temperature); // adjust for current temperature
  EPD.frame_cb_repeat(0, SD_reader, EPD_inverse);
  EPD.frame_cb_repeat(0, SD_reader, EPD_normal);
  EPD.end();   // power down the EPD panel
  
}

// main loop
unsigned long int loop_count = 0;
unsigned long int last_loop_time = 0;
void loop() {

  next_image();
  delay(10000);

}

void SD_reader(void *buffer, uint32_t address, uint16_t length){
  byte *my_buffer = (byte *)buffer;
  unsigned short my_width;
  unsigned short my_height;
  uint32_t my_address; 
  
  
  imgFile.seek(0);
  my_height = (unsigned short)imgFile.read();
  my_height += (unsigned short)imgFile.read() << 8;
  my_width = (unsigned short)imgFile.read();
  my_width += (unsigned short)imgFile.read() << 8;

  // compensate for long/short files widths
  my_address = (address * my_width) / EPD_WIDTH;
  // Serial.print(imgFile.name());
  // Serial.print(" my width: ");
  // Serial.println(my_width);
  if((my_address * 8 / my_width) < my_height){
    imgFile.seek(my_address + 4);
    for(int i=0; i < length && i < my_width / 8; i++){
      *(my_buffer + i) = imgFile.read();
    }
    // fill in rest zeros
    for(int i=my_width / 8; i < length; i++){
      *(my_buffer + i) = 0;
    }      
  }
  else{
    for(int i=0; i < length; i++){
      *(my_buffer + i) = 0;
    }
  }
  //*** add SPI reset here as
  //*** file operations above may have changed SPI mode
}

/*
 * SD card reader Comply with EPD_reader template
 *
 * buffer -- write buffer for outgoing bytes
 * address -- byte index, 0 to display height
 * length -- number of bytes to read
 */
