deps_config := \
	/Users/ojc/esp/esp-idf/components/app_trace/Kconfig \
	/Users/ojc/esp/esp-idf/components/aws_iot/Kconfig \
	/Users/ojc/esp/esp-idf/components/bt/Kconfig \
	/Users/ojc/esp/esp-idf/components/driver/Kconfig \
	/Users/ojc/esp/esp-idf/components/efuse/Kconfig \
	/Users/ojc/esp/esp-idf/components/esp32/Kconfig \
	/Users/ojc/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/Users/ojc/esp/esp-idf/components/esp_event/Kconfig \
	/Users/ojc/esp/esp-idf/components/esp_http_client/Kconfig \
	/Users/ojc/esp/esp-idf/components/esp_http_server/Kconfig \
	/Users/ojc/esp/esp-idf/components/esp_https_ota/Kconfig \
	/Users/ojc/esp/esp-idf/components/espcoredump/Kconfig \
	/Users/ojc/esp/esp-idf/components/ethernet/Kconfig \
	/Users/ojc/esp/esp-idf/components/fatfs/Kconfig \
	/Users/ojc/esp/esp-idf/components/freemodbus/Kconfig \
	/Users/ojc/esp/esp-idf/components/freertos/Kconfig \
	/Users/ojc/esp/esp-idf/components/heap/Kconfig \
	/Users/ojc/esp/esp-idf/components/libsodium/Kconfig \
	/Users/ojc/esp/esp-idf/components/log/Kconfig \
	/Users/ojc/esp/esp-idf/components/lwip/Kconfig \
	/Users/ojc/esp/esp-idf/components/mbedtls/Kconfig \
	/Users/ojc/esp/esp-idf/components/mdns/Kconfig \
	/Users/ojc/esp/esp-idf/components/mqtt/Kconfig \
	/Users/ojc/esp/esp-idf/components/nvs_flash/Kconfig \
	/Users/ojc/esp/esp-idf/components/openssl/Kconfig \
	/Users/ojc/esp/esp-idf/components/pthread/Kconfig \
	/Users/ojc/esp/esp-idf/components/spi_flash/Kconfig \
	/Users/ojc/esp/esp-idf/components/spiffs/Kconfig \
	/Users/ojc/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/Users/ojc/esp/esp-idf/components/unity/Kconfig \
	/Users/ojc/esp/esp-idf/components/vfs/Kconfig \
	/Users/ojc/esp/esp-idf/components/wear_levelling/Kconfig \
	/Users/ojc/esp/esp-idf/components/wifi_provisioning/Kconfig \
	/Users/ojc/esp/esp-idf/components/app_update/Kconfig.projbuild \
	/Users/ojc/Documents/Projects/idf-arduino-stepper/components/arduino/Kconfig.projbuild \
	/Users/ojc/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/Users/ojc/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/Users/ojc/Documents/Projects/idf-arduino-stepper/main/Kconfig.projbuild \
	/Users/ojc/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/Users/ojc/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
