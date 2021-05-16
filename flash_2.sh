#!/usr/local/bin/zsh
PORT="/dev/tty.SLAB_USBtoUART"

python /Users/ojc2/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port $PORT --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x49000 /Users/ojc2/esp/idf-arduino-stepper/build/ota_data_initial.bin 0x1000 /Users/ojc2/esp/idf-arduino-stepper/build/bootloader/bootloader.bin 0x50000 /Users/ojc2/esp/idf-arduino-stepper/build/firmware.bin 0x8000 /Users/ojc2/esp/idf-arduino-stepper/build/partitions.bin