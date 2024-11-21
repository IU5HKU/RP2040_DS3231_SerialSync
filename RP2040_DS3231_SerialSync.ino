/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

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

See more at https://thingpulse.com

 * IU5HKU - 21/11/2024
 * 
 * 
 * This program works on a Waveshare pico zero board, but works as well over many others boards
 * also if they aren't a RP2040 based one, (with the necessaries mods) and with the DS1307 too
 * 
 * The source code needs these two libraries:
 *  - NeoPixelConnect Library by Alan Yorinks (https://github.com/MrYsLab/NeoPixelConnect)
 *  - DS3231 Library by Wickert, A. D., Sandell, C. T., Schulz, B., & Ng, G. H. C. (https://github.com/NorthernWidget/DS3231)
 *  and you can find both on ArduinoIDE Library Manager
 *  
 * It's an attempt to sync DS3231 time using only serial port and nothing else, there is a bash script
 * accompaining this file who sends at second 00 the epoch of the PC to the pico serial port.
 * Due to some lags from usb->pico->i2c->ds3231 registers, time will be not precise at 100%, rember that
 * ds3231 doesn't allow to set ms or whatever, you only know that after writing the "seconds" register it start to
 * use the new time.
 * 
 * 
 * Connection for RP2040 boards are:
 *  board  DS3231
 *  3v3    VCC  
 *  GND    GND  
 *  GPIO0  SDA
 *  GPIO1  SCL
 *  GPIO2  INT/SQW
 *  
 *  hope you can find this code useful for some of your projects
 *

  :LINUX:
  set correct time with this command:
  
    date +%s > /dev/ttyACM0
  
  or better using this bash script, that sends time through 
  serial line of choice when precise "00" seconds occour:
  
  #!/bin/bash
  
  test=true
  
  while $test; do
          d=$(date +%S)
          if [ $d -eq 0 ]; then
                  #echo $(($(date +%s) +$1)) > /dev/ttyACM0
                  date +%s > /dev/ttyACM0
                  echo ">date sent to /dev/ttyACM0"
                  #date +%S.%N
                  sleep 1
                  test=false
          else
                  sleep 0.001
          fi
  done
*/

#include <DS3231.h>
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include <Wire.h>

// this for the I2C interface to work with
#define GPIO0 0   //  SDA
#define GPIO1 1   //  SCL
#define GPIO2 2   //  INT/SQW input from DS3231

// this is for waveshare pico zero who have an addressable ws2812 rgb led onboard
#define MAXIMUM_NUM_NEOPIXELS 1

#include <NeoPixelConnect.h>

// Create an instance of NeoPixelConnect and initialize it
// to use GPIO pin 16 as the control pin
NeoPixelConnect p(16, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

DS3231 myRTC;     // The DS3231 object
time_t tstmp;
String inString;  // the string with "epoch" who came from the PC USB

datetime_t t;     // The PiPico rtc object
char datetime_buf[256];
char *datetime_str = &datetime_buf[0];

#define TIMEZONE  3600
#define PPS_1HZ   0

bool century = false;
bool h12Flag;
bool pmFlag;

// Interrupt signaling byte
volatile byte tick_1hz = 1;

// Convert unix epoch to human (and rp2040) readable format
// https://github.com/sidsingh78/EPOCH-to-time-date-converter
void epoch2datetime(time_t epoch)
{
    static unsigned char month_days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    static unsigned char week_days[7] = {4,5,6,0,1,2,3};
    //Thu=4, Fri=5, Sat=6, Sun=0, Mon=1, Tue=2, Wed=3
    
    unsigned char
    ntp_hour, ntp_minute, ntp_second, ntp_week_day, ntp_date, ntp_month, leap_days, leap_year_ind ;
    
    unsigned short temp_days;
    
    unsigned int ntp_year, days_since_epoch, day_of_year; 
    leap_days=0; 
    leap_year_ind=0;
  
    ntp_second = epoch%60;
    epoch /= 60;
    ntp_minute = epoch%60;
    epoch /= 60;
    ntp_hour  = epoch%24;
    epoch /= 24;
      
    days_since_epoch = epoch;      //number of days since epoch
    ntp_week_day = week_days[days_since_epoch%7];  //Calculating WeekDay
    
    ntp_year = 1970+(days_since_epoch/365); // ball parking year, may not be accurate!
    
    for (int i=1972; i<ntp_year; i+=4)      // Calculating number of leap days since epoch/1970
       if(((i%4==0) && (i%100!=0)) || (i%400==0)) leap_days++;
            
    ntp_year = 1970+((days_since_epoch - leap_days)/365); // Calculating accurate current year by (days_since_epoch - extra leap days)
    day_of_year = ((days_since_epoch - leap_days)%365)+1;
   
    if(((ntp_year%4==0) && (ntp_year%100!=0)) || (ntp_year%400==0))  
     {
       month_days[1]=29;     //February = 29 days for leap years
       leap_year_ind = 1;    //if current year is leap, set indicator to 1 
      }
          else month_days[1]=28; //February = 28 days for non-leap years 
 
          temp_days=0;
   
    for (ntp_month=0 ; ntp_month <= 11 ; ntp_month++) //calculating current Month
       {
           if (day_of_year <= temp_days) break; 
           temp_days = temp_days + month_days[ntp_month];
        }
    
    temp_days = temp_days - month_days[ntp_month-1]; //calculating current Date
    ntp_date = day_of_year - temp_days;

    // properly set the datetime struct used by rp2040
    t = {
      .year  = ntp_year,
      .month = ntp_month,
      .day   = ntp_date,
      .dotw  = ntp_week_day,
      .hour  = ntp_hour,
      .min   = ntp_minute,
      .sec   = ntp_second
    };
          
  return;
}

// Disable Alarms: Prevent Alarms by assigning a nonsensical 
// alarm minute (0xFF) value that cannot match the clock time,
// and an alarmBits value to activate "when minutes match".
void disableAlarms(void){
    
    //Alarm 1 -----------
    myRTC.turnOffAlarm(1);
    //              D,H,  M, s, flag,     day,  H12,  PM
    myRTC.setA1Time(0,0,0xFF,0,0b01100000,false,false,false);
    myRTC.turnOnAlarm(1);
    // clear Alarm 1 flag
    myRTC.checkIfAlarm(1);

    //Alarm 2 -----------
    myRTC.turnOffAlarm(2);
    //              D,H,  M,  flag,     day,  H12,  PM
    myRTC.setA2Time(0,0,0xFF,0b01100000,false,false,false);
    myRTC.turnOnAlarm(2);
    // clear Alarm 2 flag
    myRTC.checkIfAlarm(2);
}

void showTimeFormatted(time_t t) {
  char buffer[50];
  struct tm *ptm;
  ptm = gmtime (&t);
  const char * timeformat {"%a %F %X - weekday %w; CW %W"};
  strftime(buffer, sizeof(buffer), timeformat, ptm);
  Serial.print(buffer);
  Serial.print("\n");
}

// precise 1hz interrupt routine, keep it as short as possible
void isr_1hz() {
    // interrupt signals to loop
    tick_1hz = 1;
    return;
}

////////////
// SETUP  //
////////////
void setup() {
    // Start the serial port
    Serial.begin(115200);
    
    // Start the I2C interface
    Wire.setSDA(GPIO0); 
    Wire.setSCL(GPIO1); 
    Wire.begin();

    // turn the lit off
    p.neoPixelSetValue(0,0,0,0,true);

    // wait for input from DS3231 PPS
    pinMode(GPIO2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(GPIO2), isr_1hz, FALLING);

    disableAlarms();
    
    // enable 1Hz PPS output (from INT/SQW DS3231 pin)
    myRTC.enableOscillator(true, false, PPS_1HZ);
}

////////////
//  LOOP  //
////////////
void loop() {
    
    // If something is coming in on the serial line, it's
    // a time correction so set the clock accordingly.
    
    if (Serial.available()) {
        // read epoch from serial line, essentially a number like '1732202530'     
        inString = Serial.readString();
        tstmp = atoi(inString.c_str());
        
        // naive form of sanity check
        if (tstmp > 1700000000L){ 
          epoch2datetime(tstmp+TIMEZONE);
          // init the rp2040 rtc, just to have internal clock in sync with DS3231
          rtc_init();
          rtc_set_datetime(&t);
          sleep_us(64); //needed after rtc_init() command
        
          // feed UnixTimeStamp and don' t use localtime
          // change TIMEZONE to suit your timezone
          // add 1sec to the number, don't know why, but if you don't
          // then the clock isn't precisely in sync.
          myRTC.setEpoch(tstmp + (TIMEZONE+1), true);
          myRTC.setClockMode(false);  // set to 24h
  
          //Serial.print("tstmp = ");Serial.println(tstmp);
          //showTimeFormatted(tstmp);
        }
    }

    // if BOOT button is pressed sync RP2040 rtc with ds3231 clock
    if (BOOTSEL) {      
      // properly set the datetime struct used by rp2040
      // RTC: (0..6) 0 is Sunday
      // DS3231: (1..7) 1 is Sunday                 
      t = {
        .year  = 2000+myRTC.getYear(),
        .month = myRTC.getMonth(century),
        .day   = myRTC.getDate(),
        .dotw  = myRTC.getDoW()-1,
        .hour  = myRTC.getHour(h12Flag, pmFlag),
        .min   = myRTC.getMinute(),
        .sec   = myRTC.getSecond()
      };
            
      rtc_init();
      rtc_set_datetime(&t);
      sleep_us(64); //needed after rtc_init() command
 
      // Wait for BOOTSEL to be released
      while (BOOTSEL) {
        delay(1);
      }
    }

    if (tick_1hz) {
        //reset the interrupt flag
        tick_1hz = 0;

        // turn on green led (index,R,G,B,true)        
        p.neoPixelSetValue(0,0,255,0,true);
        
        // CLS escape sequence if you wanna use something else than serialmonitor
        //Serial.print("\033[0H\033[0J");
        
        // print the DS3231 time...
        Serial.print("DS3231: ");
        Serial.print(myRTC.getDate(), DEC);
        Serial.print("/");
        
        Serial.print(myRTC.getMonth(century), DEC);
        Serial.print("/");

        Serial.print("20");
        Serial.print(myRTC.getYear(), DEC);
        Serial.print(" ");
        
        Serial.print(myRTC.getDoW(), DEC);
        Serial.print(" ");
        
        // hour, minute, and second
        Serial.print(myRTC.getHour(h12Flag, pmFlag), DEC);
        Serial.print(":");
        Serial.print(myRTC.getMinute(), DEC);
        Serial.print(":");
        Serial.print(myRTC.getSecond(), DEC);
    
        Serial.print(" ");
        Serial.print("Temp ");
        Serial.print(myRTC.getTemperature(), 2);
        Serial.println();

        // ... and now the internal RTC time
        rtc_get_datetime(&t);
        datetime_to_str(datetime_str, sizeof(datetime_buf), &t);
        Serial.printf("RTC   : %s",datetime_str);
        Serial.println();

        // turn off the led
        p.neoPixelSetValue(0,0,0,0,true);
    }
}
