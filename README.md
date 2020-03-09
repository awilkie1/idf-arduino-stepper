# Setup Arduino-ESP32
1. Make sure you are using a **Python2**  environment in your terminal  
2. Install ESP-IDF 3.2.2 (if you have not already) 
	1. Check you current version
 `git describe --always --tags --dirty`
	2. If in doubt, delete your ESP-IDF folder and fresh install  
`git clone -b v3.2.2 --recursive https://github.com/espressif/esp-idf.git`
3. Bring ESP-IDF up to the latest Arduino-Compatible commit 
`cd esp-idf && git checkout -b 4a9f339447cd5b3143f90c2422d8e1e1da9da0a4`
4. Download this repository
`git clone --recursive https://github.com/oliverjc/idf-arduino-template.git`
5. `make menuconfig`  to configure the project
6. `make -j8` to build
