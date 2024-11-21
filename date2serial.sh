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
