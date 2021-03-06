// TC4_diag.ino
//
// TC4 diagnostic utility program to program the eeprom calibration
// and do some low level test of the TC4 board.
//
// Derived from aCatuai.ino and other utility programs
// *** BSD License ***
// ------------------------------------------------------------------------------------------
// Copyright (c) 2013, Stan Gardner
// All rights reserved.
//
// Contributor:  Stan Garnder
//
// Redistribution and use in source and binary forms, with or without modification, are 
// permitted provided that the following conditions are met:
//
//   Redistributions of source code must retain the above copyright notice, this list of 
//   conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright notice, this list 
//   of conditions and the following disclaimer in the documentation and/or other materials 
//   provided with the distribution.
//
//   Neither the name of the MLG Properties, LLC nor the names of its contributors may be 
//   used to endorse or promote products derived from this software without specific prior 
//   written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS 
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// ------------------------------------------------------------------------------------------
//
// Derived from aCatuai.ino and other utility programs
// *** BSD License ***
// ------------------------------------------------------------------------------------------
// Copyright (c) 2011, MLG Properties, LLC
// All rights reserved.
//
// Contributor:  Jim Gallt
//
// Redistribution and use in source and binary forms, with or without modification, are 
// permitted provided that the following conditions are met:
//
//   Redistributions of source code must retain the above copyright notice, this list of 
//   conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright notice, this list 
//   of conditions and the following disclaimer in the documentation and/or other materials 
//   provided with the distribution.
//
//   Neither the name of the MLG Properties, LLC nor the names of its contributors may be 
//   used to endorse or promote products derived from this software without specific prior 
//   written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS 
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// ------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------
// TC4_diag Rev.s
// V0.01 Sept. 27,2013 Stan Gardner Initial rev
// V0.02 Sept. 28,2013 Stan Garnder Add ADC test
// V0.03 Oct. 7,2013   Stan Gardner added EEprom dump more printout information
// V0.04 Oct. 9,2013   Stan Gardner added temp calibration support 'S'
// V0.05 Oct. 9,2013   Stan Gardner removed debug #define
// V0.06 Oct. 10,2013   Stan Gardner added read microVolt, code cleanup
// V0.07 Oct. 11,2013   Stan Gardner added readRaw ADC test
// V0.08 Oct. 12,2013   Stan Gardner added i2c scanner
// V0.09 Oct. 18,2013   Stan Gardner added toggle and read pin 
// V0.10 Oct. 19,2013   Stan Gardner added copy eeprom to fill 
// V0.11 Oct. 20,2013   Stan Gardner added powerup pin checking 
// V0.12 Oct. 23,2013   Stan Gardner added input overrun protect to PCB,version commands 
#define BANNER_CAT "TC4_diag V0.12" // version

#if defined(ARDUINO) && ARDUINO >= 100
#define _READ read
#define _WRITE write
#else
#define _READ receive
#define _WRITE send
#endif


// The user.h file contains user-definable compiler options
// It must be located in the same folder as TC4_diag.ino
#include "user.h"

// this library included with the arduino distribution
#include <Wire.h>

#include <avr/pgmspace.h>
#include <alloca.h>

// these "contributed" libraries must be installed in your sketchbook's arduino/libraries folder
#include <thermocouple.h>
#include <cADC.h>
#include <mcEEPROM.h>

// ------------------------ other compile directives
#define MIN_DELAY 300   // ms between ADC samples (tested OK at 270)
#define NCHAN 4  // number of TC input channels
#define DP 1  // decimal places for output on serial port
#define D_MULT 0.001 // multiplier to convert temperatures from int to float
#define MAX_COMMAND 80 // max length of a command string
#define LOOPTIME 1000 // cycle time, in ms

//----------------- user interface ----------------
#define SAMPLES_10 11
#define SAMPLES_100 101
#define A_EEPROM ADDR_BITS

uint8_t channels_displayed = 1;
double calibration_temp = 0; //Temp in Celcius for calibration ref
uint8_t calibration_TC = 1;  //which Tc to use for TC calibration
uint8_t verbose_mode = 0;  //toggle to display extra debug info 
uint8_t measure_diff=0;

//Dont mess with serial port or I2C bus
#define MAX_PIN 17
#define MIN_PIN 2

uint8_t pin2read=0;
uint8_t read_pin_mode=0;

uint8_t Toggle_mode=0;
uint8_t toggle_pin=0;
uint8_t last_toggle = 0;
uint8_t default_pinmode[MAX_PIN-MIN_PIN+1]={
  INPUT_PULLUP, /*2*/
  INPUT_PULLUP,
  INPUT_PULLUP,
  INPUT_PULLUP,
  INPUT_PULLUP,
  INPUT_PULLUP,
  INPUT_PULLUP,/*8*/
  OUTPUT,      /*OT1*/
  OUTPUT,      /*0T2*/
  INPUT_PULLUP,
  INPUT_PULLUP,
  OUTPUT,      /*13 Arduino LED Blink PIN*/
  INPUT_PULLUP,/*AIN0*/
  INPUT_PULLUP,/*AIN1*/
  INPUT_PULLUP,/*16*/
  INPUT_PULLUP};
  
char *input_ptr;
//shadow memory of information to fill eeprom calibration
calBlock blank_fill= {
 "TC4_SHIELD",
  "4.00",  // edit this field to comply with the version of your TC4 board
  1.000, // gain
  0, // uV offset
  0.0, // type T offset temp
  0.0 // type K offset temp
};


// --------------------------------------------------------------
// global variables

// eeprom calibration data structure
calBlock caldata;
// class objects
mcEEPROM eeprom;
cADC adc( A_ADC ); // MCP3424
ambSensor amb( A_AMB ); // MCP9800
filterRC fT[NCHAN]; // filter for displayed/logged ET, BT

int32_t temps[NCHAN]; //  stored temperatures are divided by D_MULT
int32_t ftemps[NCHAN]; // heavily filtered temps
int32_t ftimes[NCHAN]; // filtered sample timestamps
int32_t flast[NCHAN]; // for calculating derivative
int32_t lasttimes[NCHAN]; // for calculating derivative

uint8_t chan_map[NCHAN] = { LOGCHAN1, LOGCHAN2, LOGCHAN3, LOGCHAN4 };

// used in main loop
float timestamp = 0;
boolean first;
uint32_t nextLoop;
float reftime; // reference for measuring elapsed time
boolean standAlone = true; // default is standalone mode


char command[MAX_COMMAND+1]; // input buffer for commands from the serial port
uint8_t sample_cnt = 0;



// prototypes
void serialPrintln_P(const prog_char* s);
void serialPrint_P(const prog_char* s);
float calcRise( int32_t T1, int32_t T2, int32_t t1, int32_t t2 );
void logger(void);
void logger_diff(void);
void append( char* str, char c );
void resetTimer(void);

void display_cal(void);
void display_cal_block(calBlock *caldata);
void display_menu(void);
void eeprom_dump(int page);
void show_variables(void);
int check_adc(void);
int check_MCP9800(void);
void input_error(void);
void input_accepted(void);
void processCommand(void);  // a newline character has been received, so process the command
void checkSerial(void);  // buffer the input from the serial port
void checkStatus( uint32_t ms ); // this is an active delay loop
void get_samples(void); // this function talks to the amb sensor and ADC via I2C


void display_menu(){
  serialPrintln_P(PSTR(""));
  serialPrintln_P(PSTR("a = display cal block"));
  serialPrintln_P(PSTR("b = display cal fill info"));
  serialPrintln_P(PSTR("c = write fill block to eeprom "));
  serialPrintln_P(PSTR("C = Copy cal block from eeprom to fill block "));
  serialPrintln_P(PSTR("d = change fill PCB, Should start TC4"));
  serialPrintln_P(PSTR("e = change fill Version"));
  serialPrintln_P(PSTR("f = change fill Cal Gain"));
  serialPrintln_P(PSTR("g = change fill Cal offset"));
  serialPrintln_P(PSTR("h = change fill T offset"));
  serialPrintln_P(PSTR("j = change fill K offset"));
  serialPrintln_P(PSTR("k = Set Number of TC channels to display or selects TC "));
  serialPrintln_P(PSTR("m = read TC(s) up to 1000 times"));
  serialPrintln_P(PSTR("M = read TC microVolt "));
  serialPrintln_P(PSTR("n = test adc"));
  serialPrintln_P(PSTR("N = read Raw adc data"));
  serialPrintln_P(PSTR("q = test MCP9800"));
  serialPrintln_P(PSTR("Q = read Raw MCP9800 data"));
  serialPrintln_P(PSTR("r = eeprom dump"));
  serialPrintln_P(PSTR("s = Calibration reference temp"));
  serialPrintln_P(PSTR("S = toggle calculate Cal diff"));
  serialPrintln_P(PSTR("T = pin number to toggle(arduino numbers 2-17 or T enter to reset)"));
  serialPrintln_P(PSTR("t = toggle pin"));
  serialPrintln_P(PSTR("U = pin number to read(arduino numbers 2-17 or U enter to reset)"));
  serialPrintln_P(PSTR("u = read pin"));
  serialPrintln_P(PSTR("v = toggle verbose debug mode"));
  serialPrintln_P(PSTR("V = show program variables"));
  serialPrintln_P(PSTR("1 = scan I2C bus"));
  serialPrintln_P(PSTR("Enter a Letter to run item"));
  return;
}
// -------------------------------------
void input_accepted(void){serialPrintln_P(PSTR("Input Accepted"));}
void input_error(void){serialPrintln_P(PSTR("Error - line too short"));}
void processCommand() {  // a newline character has been received, so process the command
//  char [MAX_COMMAND+1] cmd_buffer ="";
  double temp_f = 0.0;
  int temp_i=0;
  if(check_I2C()){
    serialPrintln_P(PSTR("TroubleShoot the problem then continue"));
    return;
  }
 switch (command[0]){
   case 'a':
    display_cal();
   break;
   case 'b':
    serialPrintln_P(PSTR("# EEPROM data fill: "));
    display_cal_block(&blank_fill);
    break;
   case 'c':
    eeprom.write( 0, (uint8_t*) &blank_fill, sizeof( blank_fill ) );
    serialPrintln_P(PSTR(""));
    serialPrintln_P(PSTR("# New content "));
    display_cal();
    adc.setCal( caldata.cal_gain, caldata.cal_offset );
    amb.setOffset( caldata.K_offset );
    serialPrintln_P(PSTR(""));
    break;
   case 'C':
     copy_eeprom2fill();
     break; 
   case 'd':
      if(strlen(command) >= 3){
        input_ptr = &blank_fill.PCB[0];
        strncpy(input_ptr,command+2,sizeof(blank_fill.PCB));
        blank_fill.PCB[sizeof(blank_fill.PCB)-1]='\0';
        input_accepted();
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: d SingleSpace PCBString"));
      }        
    break;
   case 'e':
      if(strlen(command) >= 3){
        input_ptr = &blank_fill.version[0];
        strncpy(input_ptr,command+2,sizeof(blank_fill.version));
        blank_fill.version[sizeof(blank_fill.version)-1]='\0';
        input_accepted();
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: e SingleSpace VersionString"));
      }        
    break;
   case 'f':
      if(strlen(command) >= 3){
        temp_f = atof(command+2);
        blank_fill.cal_gain = (float)temp_f;        
        input_accepted();
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: f SingleSpace FloatingPointValue"));
      }        
    break;
   case 'g':
      if(strlen(command) >= 3){
        temp_i = atoi(command+2);
          blank_fill.cal_offset = (int16_t)temp_i;                
        input_accepted();
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: g SingleSpace IntegerValue"));
      }        
    break;
   case 'h':
      if(strlen(command) >= 3){
        temp_f = atof(command+2);
        blank_fill.T_offset = (float)temp_f;        
        input_accepted();
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: h SingleSpace FloatingPointValue"));
      }        
    break;
   case 'j':
      if(strlen(command) >= 3){
        temp_f = atof(command+2);
        blank_fill.K_offset = (float)temp_f;        
        input_accepted();
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: j SingleSpace FloatingPointValue"));
      }        
    break;
   case 'k':
      if(strlen(command) >= 3){
        temp_i = atoi(command+2);
        if((temp_i > NCHAN) || (temp_i <= 0)){
          serialPrintln_P(PSTR("Error, enter 1,2,3,4 only"));
        }
        else{       
          channels_displayed = (uint8_t)temp_i;        
          calibration_TC=channels_displayed;
          input_accepted();
        }
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: k SingleSpace NumberChannelsDisplayedValue"));
      }        
    break;
   case 'm':
      if(strlen(command) >= 3){
        temp_i = atoi(command+2);
        if((temp_i > 1000) || (temp_i <= 0)){
          serialPrintln_P(PSTR("Error, enter 1 TO 1000 only"));
        }
        else{       
          sample_cnt = temp_i+1;
          first = true;
          serialPrint_P(PSTR("# time,ambient"));
          if(measure_diff){
            if( channels_displayed == 1 ) serialPrint_P(PSTR(",T0"));
            if( channels_displayed == 2 ) serialPrint_P(PSTR(",T1"));
            if( channels_displayed == 3 ) serialPrint_P(PSTR(",T2"));
            if( channels_displayed == 4 ) serialPrint_P(PSTR(",T3"));
            serialPrint_P(PSTR(",ref_temp,diff"));
          }
          else{
            if( channels_displayed >= 1 ) serialPrint_P(PSTR(",T0"));
            if( channels_displayed >= 2 ) serialPrint_P(PSTR(",T1"));
            if( channels_displayed >= 3 ) serialPrint_P(PSTR(",T2"));
            if( channels_displayed >= 4 ) serialPrint_P(PSTR(",T3"));
          }           
          serialPrintln_P(PSTR(""));
          resetTimer();
         }
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: m SingleSpace NumberChannelsDisplayedValue"));
      }        
    break;
  case 'M':
      read_microvolt();
      break;
   case 'n':   
   check_adc();
   break;
   case 'N':   
   read_raw_adc();
   break;
   case 'q':   
   check_MCP9800();
   break;
   case 'Q':   
   readraw_MCP9800();
   break;
    case 'r':   
      if(strlen(command) >= 3){         
        temp_i = atoi(command+2);
        if((temp_i > 512) || (temp_i < 0)){
          serialPrintln_P(PSTR("Error, enter 0 TO 511 only"));
        }
        else{       
          if(temp_i == 512){
            temp_i=511;
            while(temp_i){
              eeprom_dump(temp_i);
              temp_i--;
            }
          }
          else{
              eeprom_dump(temp_i);
          }
        }
      }
      else{
        eeprom_dump(0);
      }
   break;
   case 's':
      if(strlen(command) >= 3){
        temp_f = atof(command+2);
        if((temp_f > 1000) || (temp_f < 0)){
          serialPrintln_P(PSTR("Error, enter 0 TO 1000 only"));
        }
        else{       
          calibration_temp =temp_f;
          input_accepted();
         }
      }
      else{
        input_error();
        serialPrintln_P(PSTR("Usage: s SingleSpace floatValue"));
      }        
    break;

  case 'S':
      if(measure_diff){
        measure_diff = 0;
        serialPrintln_P(PSTR("Measure Diff Off"));
      }
      else{
        measure_diff = 1;
        serialPrintln_P(PSTR("Measure Diff On"));
      }
      break;
   case 'T':
      set_toggle_pin();
    break;
  case 't':
      toggle_pins();
      break;
   case 'U':
      set_read_pin();
    break;
  case 'u':
      read_pins();
      break;
  case 'V':
      show_variables();
      break;
  case 'v':
      if(verbose_mode){
        verbose_mode = 0;
        serialPrintln_P(PSTR("Verbose Mode Off"));
      }
      else{
        verbose_mode = 1;
        serialPrintln_P(PSTR("Verbose Mode On"));
      }
      break;

  case '1':
      i2c_scanner();
      break;
            
  case 'i':
  case 'l':
  case 'o':
    serialPrintln_P(PSTR("Not supported"));
  default:
  display_menu();
  break;
 }
  return;
}

void show_variables(void){
  serialPrintln_P(PSTR("\n\nProgram Information\n"));
  serialPrintln_P(PSTR(BANNER_CAT));
  serialPrint_P(PSTR("verbose_mode = "));
  Serial.print(verbose_mode);
  serialPrintln_P(PSTR(", Verbose Debug Mode when 1"));
  serialPrint_P(PSTR("channels_displayed = "));
  Serial.print(channels_displayed);
  serialPrintln_P(PSTR(", Number of channels  for read TC"));
  serialPrint_P(PSTR("calibration_TC = "));
  Serial.print(calibration_TC);
  serialPrintln_P(PSTR(", Which TC is used for Calibration"));
  serialPrint_P(PSTR("measure_diff = "));
  Serial.print(measure_diff);
  serialPrintln_P(PSTR(", Display TC vs Reference for read TC when 1"));
  serialPrint_P(PSTR("calibration_temp = "));
  Serial.print(calibration_temp);
  serialPrintln_P(PSTR(", Calibration Reference Temperature"));
  serialPrint_P(PSTR("Toggle_mode = "));
   Serial.print(Toggle_mode);
  serialPrintln_P(PSTR(", Toggle pin mode ON when 1"));
  serialPrint_P(PSTR("toggle_pin = "));
   Serial.print(toggle_pin);
  serialPrintln_P(PSTR(", Pin that toggle when Toggle_mode = 1"));
  serialPrint_P(PSTR("read_pin_mode = "));
   Serial.print(read_pin_mode);
  serialPrintln_P(PSTR(", Read pin mode ON when 1"));
  serialPrint_P(PSTR("pin2read = "));
   Serial.print(pin2read);
  serialPrintln_P(PSTR(", Pin that is read when read_pin_mode = 1"));

}

void serialPrint_P(const prog_char* s)
{
   char* p = (char*)alloca(strlen_P(s) + 1);
  strcpy_P(p, s);
  Serial.print(p);
}

void serialPrintln_P(const prog_char* s)
{
  serialPrint_P(s);
  serialPrint_P(PSTR("\n"));
}
void debug_Println_P(const prog_char* s)
{
  if(verbose_mode){
    serialPrint_P(s);
    serialPrint_P(PSTR("\n"));
  }
}
void toggle_pins(void){
  if(Toggle_mode){
    serialPrint_P(PSTR("Pin "));
    if(last_toggle){
      digitalWrite(toggle_pin,0);
      Serial.print(toggle_pin);
        serialPrintln_P(PSTR(" Toggled Low"));
    }
    else{
      digitalWrite(toggle_pin,1);
      Serial.print(toggle_pin);
        serialPrintln_P(PSTR(" Toggled High"));
    }
    last_toggle = !last_toggle;
  }
  else{
        serialPrintln_P(PSTR(" Toggle Pin not set"));
  }    
}

void set_toggle_pin(void){
  int temp_i = 0;
  if(strlen(command) >= 3){
    temp_i = atoi(command+2);
     if((temp_i > MAX_PIN) || (temp_i < MIN_PIN)){
          serialPrint_P(PSTR("Error, enter a number between  "));
          Serial.print(MIN_PIN);
          serialPrint_P(PSTR(" and "));
          Serial.println(MAX_PIN);          
     }
     else{       
        if(Toggle_mode && toggle_pin){
          if(default_pinmode[toggle_pin-MIN_PIN] != OUTPUT){
            pinMode(toggle_pin,default_pinmode[toggle_pin-MIN_PIN]);  
          }
          serialPrint_P(PSTR("Pin toggle on pin "));
          Serial.print(toggle_pin);
          serialPrintln_P(PSTR(" turned Off"));
          if(default_pinmode[toggle_pin-MIN_PIN] == OUTPUT){
            serialPrint_P(PSTR("Pin default is output, leaving set to "));
            Serial.println(last_toggle);
          }
        }
        toggle_pin = (uint8_t)temp_i;        
        last_toggle = 0;
        Toggle_mode=1;
        pinMode(toggle_pin,OUTPUT);
        digitalWrite(toggle_pin,0);                 
        input_accepted();
     }
  }
  else{
      if(toggle_pin){
        if(default_pinmode[toggle_pin-MIN_PIN] != OUTPUT){
          pinMode(toggle_pin,default_pinmode[toggle_pin-MIN_PIN]);  
        }
        serialPrint_P(PSTR("Pin toggle on pin "));
        Serial.print(toggle_pin);
        serialPrintln_P(PSTR(" turned Off"));
        if(default_pinmode[toggle_pin-MIN_PIN] == OUTPUT){
          serialPrint_P(PSTR("Pin default is output, leaving set to "));
          Serial.println(last_toggle);
        }
      }
      else{
        serialPrintln_P(PSTR("Toggle still off"));
      }
      last_toggle = 0;
      Toggle_mode=0;
      toggle_pin=0;      
  }        
  return;
}
void read_pins(void){
  if(read_pin_mode){
    serialPrint_P(PSTR("Pin "));
    if(digitalRead(pin2read)){
      Serial.print(pin2read);
      serialPrintln_P(PSTR(" is High"));
    }
    else{
      Serial.print(pin2read);
      serialPrintln_P(PSTR(" is Low"));
    }
  }
  else{
        serialPrintln_P(PSTR(" Read Pin not set"));
  }    
}


void set_read_pin(void){
  int temp_i = 0;
  if(strlen(command) >= 3){
    temp_i = atoi(command+2);
     if((temp_i > MAX_PIN) || (temp_i < MIN_PIN)){
          serialPrint_P(PSTR("Error, enter a number between  "));
          Serial.print(MIN_PIN);
          serialPrint_P(PSTR(" and "));
          Serial.println(MAX_PIN);          
     }
     else{       
        if(read_pin_mode && pin2read){
          pinMode(pin2read,default_pinmode[pin2read-MIN_PIN]);  
          if(default_pinmode[pin2read-MIN_PIN] == OUTPUT)
            digitalWrite(pin2read,0);
        }
        pin2read = (uint8_t)temp_i;        
        read_pin_mode=1;
        pinMode(pin2read,INPUT_PULLUP);
        input_accepted();
     }
  }
  else{
      read_pin_mode=0;
      if(pin2read){
        pinMode(pin2read,default_pinmode[pin2read-MIN_PIN]);  
        if(default_pinmode[pin2read-MIN_PIN] == OUTPUT)
           digitalWrite(pin2read,0);
        serialPrint_P(PSTR("Read  pin "));
        Serial.print(pin2read);
        serialPrintln_P(PSTR(" turned Off"));
      }
      else{
        serialPrintln_P(PSTR("Read pin still off"));
      }
      pin2read=0;      
  }        
  return;
}

int check_I2C(void)
{
  int rval=0;
  if(!digitalRead(19)){
    serialPrintln_P(PSTR("Error I2C clock pin low"));
    rval=1;
  }    
  if(!digitalRead(18)){
    serialPrintln_P(PSTR("Error I2C data pin low")); 
    rval +=2;
  }
  return (rval);
}


void i2c_scanner()
{
  byte error, address;
  int nDevices;
  
  if(check_I2C())
    return;
    
  serialPrintln_P(PSTR("expecting 0x48,0x50,0x68"));
  serialPrintln_P(PSTR("Scanning..."));

  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      serialPrint_P(PSTR("I2C device found at address 0x"));
      if (address<16)
        serialPrint_P(PSTR("0"));
      Serial.print(address,HEX);
      serialPrintln_P(PSTR("  !"));

      nDevices++;
    }
    else if (error==4)
    {
      serialPrint_P(PSTR("Unknow error at address 0x"));
      if (address<16)
        serialPrint_P(PSTR("0"));
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    serialPrintln_P(PSTR("No I2C devices found\n"));
  else
    serialPrintln_P(PSTR("done\n"));

  delay(5000);           // wait 5 seconds for next scan
}
// ------------------------------------------------------------------
void logger_diff()
{
  int i=0,j=0;
  float t1,t2,t_amb;

  // print timestamp from when samples were taken
  Serial.print( timestamp, DP );

  // print ambient
  serialPrint_P(PSTR(","));
#ifdef CELSIUS
  t_amb = amb.getAmbC();
#else
  t_amb = amb.getAmbF();
#endif
  Serial.print( t_amb, DP );
  // print temperature, rate for each channel
  i = 0;
  if( channels_displayed == 1 ) {
    serialPrint_P(PSTR(","));
    Serial.print( t1 = D_MULT*temps[i], DP );
    j=i;
  };
    i++;
  
  if( channels_displayed == 2 ) {
    serialPrint_P(PSTR(","));
    Serial.print( t2 = D_MULT * temps[i], DP );
    j=i;
  };
    i++;
  
  if( channels_displayed == 3 ) {
    serialPrint_P(PSTR(","));
    Serial.print( D_MULT * temps[i], DP );
    j=i;
  };
    i++;
  
  if( channels_displayed == 4 ) {
    serialPrint_P(PSTR(","));
    Serial.print( D_MULT * temps[i], DP );
    j=i;
  };
    serialPrint_P(PSTR(","));
    Serial.print( calibration_temp, DP );
    serialPrint_P(PSTR(","));
    Serial.print( calibration_temp - D_MULT * temps[j], DP );
  Serial.println();
};

void display_cal() {
  if( readCalBlock( eeprom, caldata ) ) {
    serialPrintln_P(PSTR(("# EEPROM data read: ")));
    display_cal_block(&caldata);
  }
  else { // if there was a problem with EEPROM read, then use default values
    serialPrintln_P(PSTR("# Failed to read EEPROM.  Using default calibration data. "));
  }   

  return;
}
void copy_eeprom2fill() {

  if( readCalBlock( eeprom, caldata ) ) {
    blank_fill=caldata;
    serialPrintln_P(PSTR(("# EEPROM copied to fill_block, new values: ")));
    display_cal_block(&blank_fill);
  }
  else { // if there was a problem with EEPROM read, then use default values
    serialPrintln_P(PSTR("# Failed to read EEPROM.  No copy done "));
  }   

  return;
}


void display_cal_block(calBlock *caldata) {
    Serial.print("# PCB = ");
    Serial.print( caldata->PCB); serialPrint_P(PSTR("  Version "));
    Serial.println( caldata->version );
    Serial.print("# cal gain ");
    Serial.print( caldata->cal_gain, 6 ); serialPrint_P(PSTR("  cal offset "));
    Serial.println( caldata->cal_offset );
    Serial.print("# K offset ");
    Serial.print( caldata->K_offset, 2 ); serialPrint_P(PSTR("  T offset "));
    Serial.println( caldata->T_offset, 2 );
  return;
}
void debug_print_int(uint8_t a){
  if(verbose_mode)
    Serial.println(a,HEX);
  return;
}

void eeprom_dump(int page){
uint8_t a=0,page_lo=0,page_hi=0;
int j=0,i=0;
  page_hi = ((0x80*page & 0xFF00)>>8);
  if(page % 2 == 1)
    page_lo = 0x80;
   else
    page_lo = 0x00; 
  Wire.beginTransmission( A_EEPROM );
  for(j=0;j<8;j++){
    Wire.beginTransmission( A_EEPROM );
    Wire._WRITE( page_hi); //address
    Wire._WRITE( 0x10 * j | page_lo ); // 
    Wire.endTransmission();
    Wire.requestFrom( A_EEPROM, 16 );
    serialPrint_P(PSTR("address 0x"));
      if((page_hi == 0) || (page_hi < 0x10))    
        Serial.print(0,HEX);
    Serial.print(page_hi,HEX);
    if((j == 0) && (page_lo == 0))    
        Serial.print(0,HEX);
    Serial.print((page_lo+j*0x10),HEX);
    for(i=0;i<16;i++){
      serialPrint_P(PSTR(" "));
      a = Wire._READ(); // first data byte
      if((a == 0) || (a < 0x10))    
        Serial.print(0,HEX);
      Serial.print(a,HEX);     
    }
    serialPrint_P(PSTR("\n"));
    Wire.endTransmission();
  }
}
//Used to force error condition
//#define A_ADC 0 
int check_adc(){
// check ADC is ready to process conversions
// Request a conversion, check it started
// wait make sure it is completed in normal delay
// check various bits can be set and cleared
  int testnum=0,test_stat=0;
  Wire.beginTransmission( A_ADC );
  Wire._WRITE( 0x2a );
  Wire.endTransmission();
  Wire.requestFrom( A_ADC, 4 );
  uint8_t a = Wire._READ(); // first data byte
  uint8_t b = Wire._READ(); // second data byte
  uint8_t c = Wire._READ(); // 3rd data byte
  uint8_t stat = Wire._READ(); // read the status byte returned from the ADC
  debug_Println_P(PSTR("Check to see if the ADC is ready"));
  debug_print_int(a);  // debug
  debug_print_int(b);  // debug
  debug_print_int(c);  // debug
  debug_print_int(stat);  // debug
  if(stat & 0x80){
    debug_Println_P(PSTR("Error ADC not ready"));
    test_stat |= 1<<testnum;
  }
  testnum++;
  if((stat & 0x7F) != 0x2a){
    serialPrintln_P(PSTR("Error ADC config bits 1 does not match"));
    test_stat |= 1<<testnum;
  }
  testnum++;
  Wire.beginTransmission(A_ADC);
  Wire._WRITE( 0xc5 );
  Wire.endTransmission();
  Wire.requestFrom( A_ADC, 4 );
  a = Wire._READ(); // first data byte
  b = Wire._READ(); // second data byte
  c = Wire._READ(); // 3rd data byte
  stat = Wire._READ(); // read the status byte returned from the ADC
  debug_Println_P(PSTR("Check to see if the ADC is not ready"));
  debug_print_int(a);  // debug
  debug_print_int(b);  // debug
  debug_print_int(c);  // debug
  debug_print_int(stat);  // debug
  if((stat & 0x80) == 0x00){
    debug_Println_P(PSTR("Error ADC ready too fast"));
    test_stat |= 1<<testnum;
  }
  testnum++;
  if((stat & 0x7F) != 0x45){
    debug_Println_P(PSTR("Error ADC config bits 2 does not match"));
    test_stat |= 1<<testnum;
  }
  testnum++;
  checkStatus(MIN_DELAY); // give the chips time to perform the conversions
  Wire.requestFrom(A_ADC, 4);
  a = Wire._READ(); // first data byte
  b = Wire._READ(); // second data byte
  c = Wire._READ(); // 3rd data byte
  stat = Wire._READ(); // read the status byte returned from the ADC
  debug_Println_P(PSTR("Check to see if the ADC is ready after proper delay"));
  debug_print_int(a);  // debug
  debug_print_int(b);  // debug
  debug_print_int(c);  // debug
  debug_print_int(stat);  // debug
  if(stat & 0x80){
    debug_Println_P(PSTR("Error ADC not ready after delay"));
    test_stat |= 1<<testnum;
  }
  testnum++;
  if(test_stat){
    serialPrint_P(PSTR("ADC test fail "));
    Serial.println(test_stat,HEX);
    return(test_stat);
  }
  else{
    serialPrintln_P(PSTR("ADC test passed"));
    return (int)0x00; 
  }
}
int read_raw_adc(){
// Read Raw data from the ADC 
// using 1 conversion 18bit sample Gain 8
  int i;
  uint8_t a=0,b=0,c=0,stat=0,config_byte=0;
  serialPrint_P(PSTR("Read Raw ADC data for TC"));
  Serial.print(channels_displayed-1);
  serialPrintln_P(PSTR(" output MSB,BYTE2,LSB,STAT"));
  config_byte = (0x8f | ((channels_displayed-1)<<5));
  for(i=0;i<10;i++){
    Wire.beginTransmission( A_ADC );
    Wire._WRITE( config_byte );
    Wire.endTransmission();
    checkStatus(MIN_DELAY); // give the chips time to perform the conversions
    Wire.requestFrom(A_ADC, 4);
    a = Wire._READ(); // first data byte
    b = Wire._READ(); // second data byte
    c = Wire._READ(); // 3rd data byte
    stat = Wire._READ(); // read the status byte returned from the ADC
    if(a < 16)
      Serial.print(0x00,HEX);
    Serial.print(a,HEX); serialPrint_P(PSTR(","));
    if(b < 16)
      Serial.print(0x00,HEX);
    Serial.print(b,HEX); serialPrint_P(PSTR(","));
    if(c < 16)
      Serial.print(0x00,HEX);
    Serial.print(c,HEX); serialPrint_P(PSTR(","));
    if(stat < 16)
      Serial.print(0x00,HEX);
    Serial.println(stat,HEX);  
  }
}
int readraw_MCP9800(){
// Read Raw data from the Ambient Temp sensor Temp register  

  int i;
  uint8_t a=0,b=0;
  serialPrintln_P(PSTR("Read Raw temp data for MCP9800"));
  serialPrintln_P(PSTR("MSB,LSB"));

  Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x01 ); //address
  Wire._WRITE( 0xE0 ); // config data
  Wire.endTransmission();
  Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x00 ); //set temp address
  Wire.endTransmission();

  for(i=0;i<10;i++){
    checkStatus(MIN_DELAY); // give the chips time to perform the conversions
    Wire.requestFrom(A_AMB, 2);
    a = Wire._READ(); // first data byte
    b = Wire._READ(); // second data byte
    if(a < 16)
      Serial.print(0x00,HEX);
    Serial.print(a,HEX); serialPrint_P(PSTR(","));
    if(b < 16)
      Serial.print(0x00,HEX);
    Serial.println(b,HEX); 
  }
}
int check_MCP9800(){
// check Temp sensor operation
// write and read programmable registers
uint8_t a=0,b=0,c=0,testnum=0;
  Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x01 ); //address
  Wire._WRITE( 0xaa ); // config data
  debug_Println_P(PSTR("Test register 1 pattern 0xaa"));
  Wire.endTransmission();
  Wire.requestFrom( A_AMB, 1 );
  a = Wire._READ(); // first data byte
  debug_Println_P(PSTR("Register 1 read  "));
  debug_print_int(a);  // debug
  if(a != 0xaa){
    c |= 1<<testnum;
  }
  testnum++;
  Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x01 ); //address
  Wire._WRITE( 0x55 ); // config data
  Wire.endTransmission();
  Wire.requestFrom( A_AMB, 1 );
  debug_Println_P(PSTR("Test register 1 pattern 0x55"));
  a = Wire._READ(); // first data byte
  debug_print_int(a);  // debug
  if(a != 0x55){
    c |= 1<<testnum;
  }
  testnum++;

//hysteresis register
 Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x02 ); //address
  Wire._WRITE( 0x55 ); // config data
  Wire._WRITE( 0x00 ); // config data
  Wire.endTransmission();
  Wire.requestFrom( A_AMB, 2 );
  a = Wire._READ(); // first data byte
  b = Wire._READ(); // first data byte
  debug_Println_P(PSTR("Test register 2 pattern 0x55 0x00"));
  debug_Println_P(PSTR("Test register 2 read "));
  debug_print_int(a);  // debug
  debug_print_int(b);  // debug
  if((a != 0x55) || ((b &0x80) != 0x00)){
    c |= 1<<testnum;
  }
  testnum++;

 Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x02 ); //address
  Wire._WRITE( 0xaa ); // config data
  Wire._WRITE( 0x80 ); // config data
  Wire.endTransmission();
  Wire.requestFrom( A_AMB, 2 );
  debug_Println_P(PSTR("Test register 2 pattern 0xaa 0x80"));
  debug_Println_P(PSTR("Test register 2 read "));
  a = Wire._READ(); // first data byte
  b = Wire._READ(); // first data byte
  debug_print_int(a);  // debug
  debug_print_int(b);  // debug
  if((a != 0xaa) || ((b & 0x80) != 0x80)){
    c |= 1<<testnum;
  }
  testnum++;

  //set back to default
 Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x02 ); //address
  Wire._WRITE( 0x4b ); // config data
  Wire._WRITE( 0x00 ); // config data
  Wire.endTransmission();

//alarm set point register
 Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x03 ); //address
  Wire._WRITE( 0x55 ); // config data
  Wire._WRITE( 0x00 ); // config data
  Wire.endTransmission();
  Wire.requestFrom( A_AMB, 2 );
  debug_Println_P(PSTR("Test register 3 pattern 0x55 0x00"));
  debug_Println_P(PSTR("Test register 3 read "));

  a = Wire._READ(); // first data byte
  b = Wire._READ(); // first data byte
  debug_print_int(a);  // debug
  debug_print_int(b);  // debug
  if((a != 0x55) || ((b &0x80) != 0x00)){
    c |= 1<<testnum;
  }
  testnum++;

 Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x03 ); //address
  Wire._WRITE( 0xaa ); // config data
  Wire._WRITE( 0x80 ); // config data
  Wire.endTransmission();
  Wire.requestFrom( A_AMB, 2 );
  debug_Println_P(PSTR("Test register 3 pattern 0xaa 0x80"));
  debug_Println_P(PSTR("Test register 3 read "));
  a = Wire._READ(); // first data byte
  b = Wire._READ(); // first data byte
  debug_print_int(a);  // debug
  debug_print_int(b);  // debug
  if((a != 0xaa) || ((b &0x80) != 0x80)){
    c |= 1<<testnum;
  }
  testnum++;

  //set back to default
 Wire.beginTransmission( A_AMB );
  Wire._WRITE( 0x03 ); //address
  Wire._WRITE( 0x50 ); // config data
  Wire._WRITE( 0x00 ); // config data
  Wire.endTransmission();

  if(c){
    serialPrint_P(PSTR("MCP9800 test fail "));
    Serial.println(c,HEX);
  }
  else
    serialPrintln_P(PSTR("MCP9800 test pass"));
  return 0x00;
}

#define CALPT 50000.0
#define DEFAULT_CAL 1.000
void read_microvolt(void){
filterRC f;
long i = 0;
int j=50;
float uv_cal;
float stored_cal = DEFAULT_CAL;
long uv;
  uv_cal = stored_cal;
//  adc.setCfg( ADC_BITS_18 );
  adc.setCal (stored_cal, 0 );
  f.init( 50 );
  serialPrintln_P(PSTR("#,uVolt x.001,uVolt,GainValue,CALPT"));
  while(--j){
    adc.nextConversion( channels_displayed - 1 );
    delay( 300 );
    uv = f.doFilter( adc.readuV() );
    Serial.print( i++ ); serialPrint_P(PSTR( "," ));
    Serial.print( (float)uv * 0.001, 3 ); serialPrint_P(PSTR( "," ));
    Serial.print( uv ); serialPrint_P(PSTR( "," ));
    if( uv != 0.0 )
      uv_cal = stored_cal * CALPT / (float) uv;
    Serial.print( uv_cal, 5 ); serialPrint_P(PSTR( "," ));
    Serial.println( round( uv_cal * (float) uv ) );
  }
}  

void logger()
{
  int i;
  float t1,t2,t_amb;

  // print timestamp from when samples were taken
  Serial.print( timestamp, DP );

  // print ambient
  serialPrint_P(PSTR(","));
#ifdef CELSIUS
  t_amb = amb.getAmbC();
#else
  t_amb = amb.getAmbF();
#endif
  Serial.print( t_amb, DP );
  // print temperature, rate for each channel
  i = 0;
  if( channels_displayed >= 1 ) {
    serialPrint_P(PSTR(","));
    Serial.print( t1 = D_MULT*temps[i], DP );
    i++;
  };
  
  if( channels_displayed >= 2 ) {
    serialPrint_P(PSTR(","));
    Serial.print( t2 = D_MULT * temps[i], DP );
    i++;
  };
  
  if( channels_displayed >= 3 ) {
    serialPrint_P(PSTR(","));
    Serial.print( D_MULT * temps[i], DP );
    i++;
  };
  
  if( channels_displayed >= 4 ) {
    serialPrint_P(PSTR(","));
    Serial.print( D_MULT * temps[i], DP );
  };
  Serial.println();
};
// T1, T2 = temperatures x 1000
// t1, t2 = time marks, milliseconds
// ---------------------------------------------------
float calcRise( int32_t T1, int32_t T2, int32_t t1, int32_t t2 ) {
  int32_t dt = t2 - t1;
  if( dt == 0 ) return 0.0;  // fixme -- throw an exception here?
  float dT = (T2 - T1) * D_MULT;
  float dS = dt * 0.001; // convert from milli-seconds to seconds
  return ( dT / dS ) * 60.0; // rise per minute
}

// -------------------------------------
void append( char* str, char c ) { // reinventing the wheel
  int len = strlen( str );
  str[len] = c;
  str[len+1] = '\0';
}

// ----------------------------
void resetTimer() {
  nextLoop = 10 + millis(); // wait 10 ms and force a sample/log cycle
  reftime = 0.001 * nextLoop; // reset the reference point for timestamp
  return;
}

// -------------------------------------
void checkSerial() {  // buffer the input from the serial port
  char c;
  while( Serial.available() > 0 ) {
    c = Serial.read();
    if( ( c == '\n' ) || ( strlen( command ) == MAX_COMMAND ) ) { // check for newline, or buffer overflow
      processCommand();
      strcpy( command, "" ); // empty the buffer
    } // end if
    else if( c != '\r' ) { // ignore CR for compatibility with CR-LF pairs
//      append( command, toupper(c) );
      append( command, c );
    } // end else
  } // end while
}

// ----------------------------------
void checkStatus( uint32_t ms ) { // this is an active delay loop
  uint32_t tod = millis();
  while( millis() < tod + ms ) {
  }
}

// --------------------------------------------------------------------------
void get_samples() // this function talks to the amb sensor and ADC via I2C
{
  int32_t v;
  TC_TYPE tc;
  float tempC;
  
  for( int j = 0; j < channels_displayed; j++ ) { // one-shot conversions on both chips
    adc.nextConversion( chan_map[j] ); // start ADC conversion on channel j
    amb.nextConversion(); // start ambient sensor conversion
    checkStatus( MIN_DELAY ); // give the chips time to perform the conversions
    ftimes[j] = millis(); // record timestamp for RoR calculations
    amb.readSensor(); // retrieve value from ambient temp register
    v = adc.readuV(); // retrieve microvolt sample from MCP3424
    tempC = tc.Temp_C( 0.001 * v, amb.getAmbC() ); // convert to Celsius
#ifdef CELSIUS
    v = round( tempC / D_MULT ); // store results as integers
#else
    v = round( C_TO_F( tempC ) / D_MULT ); // store results as integers
#endif
    temps[j] = fT[j].doFilter( v ); // apply digital filtering for display/logging
  }
}
  
// ------------------------------------------------------------------------
// MAIN
//
void setup()
{
  int i=0;
  delay(500);

  for (i=0;i <= (MAX_PIN-MIN_PIN);i++){
    pinMode(MIN_PIN+i,default_pinmode[i]);
    if(default_pinmode[i]==OUTPUT)
      digitalWrite(i+MIN_PIN,0);
  }    

  Serial.begin(BAUD);
  delay(500);
  amb.init( AMB_FILTER );  // initialize ambient temp filtering
  serialPrintln_P(PSTR(BANNER_CAT));

  for (i=0;i <= (MAX_PIN-MIN_PIN);i++){
    if(!digitalRead(i+MIN_PIN) && (default_pinmode[i]!=OUTPUT)){
      serialPrint_P(PSTR("Pin "));
      Serial.print(i+MIN_PIN);
      serialPrintln_P(PSTR(" is Not High"));
    }      
  }    


  if(check_I2C()){
    serialPrintln_P(PSTR("TroubleShoot the problem then continue"));
  }
  else{
    Wire.begin(); 
    // read calibration and identification data from eeprom
    if( readCalBlock( eeprom, caldata ) ) {
      serialPrintln_P(PSTR(("valid calblock found, using content")));
      adc.setCal( caldata.cal_gain, caldata.cal_offset );
      amb.setOffset( caldata.K_offset );
    }
    else { // if there was a problem with EEPROM read, then use default values
      serialPrintln_P(PSTR(("# Failed to read EEPROM.  Using default calibration data. ")));
      adc.setCal( CAL_GAIN, UV_OFFSET );
      amb.setOffset( AMB_OFFSET );
    }   

  }

  display_menu();

  // write header to serial port

  fT[0].init( BT_FILTER ); // digital filtering on BT
  fT[1].init( ET_FILTER ); // digital filtering on ET

  
  delay( 1800 );
  nextLoop = 2000;
  reftime = 0.001 * nextLoop; // initialize reftime to the time of first sample
  first = true;
  
}

// -----------------------------------------------------------------
void loop() {
  float idletime;
  uint32_t thisLoop;

  // delay loop to force update on even LOOPTIME boundaries
  while ( millis() < nextLoop ) { // delay until time for next loop
      checkSerial(); // Has a command been received?
   }
  while(sample_cnt){
    sample_cnt--;
    thisLoop = millis(); // actual time marker for this loop
    timestamp = 0.001 * float( thisLoop ) - reftime; // system time, seconds, for this set of samples
    get_samples(); // retrieve values from MCP9800 and MCP3424
    if( first ) // use first samples for RoR base values only
      first = false;
    else {
      if(measure_diff)
        logger_diff();
      else
        logger(); // output results to serial port
    }

    for( int j = 0; j < channels_displayed; j++ ) {
     flast[j] = ftemps[j]; // use previous values for calculating RoR
     lasttimes[j] = ftimes[j];
    }

    idletime = LOOPTIME - ( millis() - thisLoop );
    // arbitrary: complain if we don't have at least 50mS left
    if (idletime < 50 ) {
      serialPrint_P(PSTR("# idle: "));
      Serial.println(idletime);
    }
  }
  nextLoop += LOOPTIME; // time mark for start of next update 
}

