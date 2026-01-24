#!/bin/bash

# most likely lingering component from another test is loop
pkill loop

# Check if a parameter was provided
if [[ -z "$1" ]]; then
    echo "Error: required, frame size bytes."
    exit 1
fi

if [[ "$1" -ge 0 && "$1" -le 4095 ]]; then
    echo 0 > /sys/bus/platform/devices/80000000.hwaccel/en
    echo "$1" > /sys/bus/platform/devices/80000000.hwaccel/frame_size_bytes

    echo 1 > /sys/bus/platform/devices/80000000.hwaccel/en
 
    taskset -c 3  ./loop $1 &
else
    echo "Error: The parameter must be an integer between 16 and 406."
    exit 1
fi




