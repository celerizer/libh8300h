H8_ROOT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

H8_SOURCES := \
  $(H8_ROOT_DIR)/device.c \
  $(H8_ROOT_DIR)/devices/buttons.c \
  $(H8_ROOT_DIR)/devices/eeprom.c \
  $(H8_ROOT_DIR)/devices/factory_control.c \
  $(H8_ROOT_DIR)/devices/led.c \
  $(H8_ROOT_DIR)/dma.c.c \
  $(H8_ROOT_DIR)/emu.c \
  $(H8_ROOT_DIR)/main.c \
  $(H8_ROOT_DIR)/rtc.c

H8_HEADERS := \
  $(H8_ROOT_DIR)/config.h \
  $(H8_ROOT_DIR)/device.h \
  $(H8_ROOT_DIR)/devices/buttons.h \
  $(H8_ROOT_DIR)/devices/eeprom.h \
  $(H8_ROOT_DIR)/devices/factory_control.h \
  $(H8_ROOT_DIR)/devices/led.h \
  $(H8_ROOT_DIR)/dma.h \
  $(H8_ROOT_DIR)/emu.h \
  $(H8_ROOT_DIR)/registers.h \
  $(H8_ROOT_DIR)/rom.h \
  $(H8_ROOT_DIR)/rtc.h \
  $(H8_ROOT_DIR)/system.h \
  $(H8_ROOT_DIR)/types.h
