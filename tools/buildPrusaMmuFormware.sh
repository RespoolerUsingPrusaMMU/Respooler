#!bash
#
# This script will take the firmware.hex file and add the text "; device = mm-control" to the top of the file
# it also fixes the end of line character
#
# Run this script from this directory and it will move to the correct directory.
#

cd ../build/Spooler-Firmware/debug/
avr-objcopy -O ihex -R .eeprom firmware firmware.hex

{
    printf '; device = mm-control\n'
    cat firmware.hex
} > firmware-prusa.hex

cd -

