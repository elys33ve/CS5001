# various utility systems based on the discontinued 3eg 8-channel tmoip board

Makefile parameters with defaults:

##################
# probably don't need to change
##################
BASEPATH?=~/projects
SDCARD?=/media/${USER}/boot
BOOTGEN?=~/local/Xilinx/Vitis/2022.1/bin/bootgen
KERNELPATH?=~/projects/xilinx/linux-zynqmp
UBOOTPATH?=~/projects/xilinx/u-boot-zynqmp

##################
# platform specific variables
##################
PATFORM?=bert

##################
Examples
##################
(edit setpaths.sh with the desired values)
source setpaths.sh

make update
make boot.bin
make bootscripts
make install_sd
