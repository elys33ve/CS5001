#!/bin/bash
echo $1 > /sys/bus/platform/devices/80000000.ddr_test/interframe_clks 
echo $1 > /sys/bus/platform/devices/80020000.ddr_test/interframe_clks 
echo $1 > /sys/bus/platform/devices/80040000.ddr_test/interframe_clks 
echo $1 > /sys/bus/platform/devices/80060000.ddr_test/interframe_clks 
echo $1 > /sys/bus/platform/devices/80080000.ddr_test/interframe_clks 
echo $1 > /sys/bus/platform/devices/800a0000.ddr_test/interframe_clks 
