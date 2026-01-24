# mkimage -c none -A arm -T script -d test.cmd test.scr
#

echo Quasonix utils-3eg

echo loading u-boot scripts
env set read_environment 0x600000;

fatload mmc $mmcsel:1 $read_environment read_environment.scr

source $read_environment

echo init script complete

