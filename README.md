# RP2040_DS3231_SerialSync
Set the right time to a DS3231/DS1307 through serial port

This program works on a Waveshare pico zero board, but works as well over many others boards
also if they aren't a RP2040 based one, (with the necessaries mods) and with the DS1307 too
 
The source code needs these two libraries:
  - NeoPixelConnect Library by Alan Yorinks (https://github.com/MrYsLab/NeoPixelConnect)
  - DS3231 Library by Wickert, A. D., Sandell, C. T., Schulz, B., & Ng, G. H. C. (https://github.com/NorthernWidget/DS3231)
and you can find both on ArduinoIDE Library Manager
  
It's an attempt to sync DS3231 time using only serial port and nothing else, there is a bash script
accompaining this file who sends at second 00 the epoch of the PC to the pico serial port.
Due to some lags from usb->pico->i2c->ds3231 registers, time will be not precise at 100%, rember that
ds3231 doesn't allow to set ms or whatever, you only know that after writing the "seconds" register it start to
use the new time.
 
 
Connection for RP2040 boards are:
```javascript
  board  DS3231
  3v3    VCC  
  GND    GND  
  GPIO0  SDA
  GPIO1  SCL
  GPIO2  INT/SQW
```
hope you can find this code useful for some of your projects


For LINUX users:

  set correct time with this command:
  
    date +%s > /dev/ttyACM0
  
  or better using the included bash script, that sends time through 
  serial line of choice when precise "00" seconds occour:

```javascript  
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
```
