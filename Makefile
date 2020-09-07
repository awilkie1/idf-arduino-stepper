#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := firmware
EXTRA_COMPONENT_DIRS := ./components/

# Note that the IDF path is inherited by the IDF path found in in the ESP_ADF directory's makefile
# Comment out following line if using ADF
include $(IDF_PATH)/make/project.mk

# Uncomment the Following line to use ADF
# include $(ADF_PATH)/project.mk

# EXTRA_COMPONENT_DIRS := /Users/ojc/esp/projects/idf-3.3-arduino-template/components
# include $(IDF_PATH)/make/project.mk

