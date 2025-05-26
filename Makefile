all : flash

TARGET:=main
TARGET_MCU:=CH32X035
CH32V003FUN ?= ./ch32fun/ch32fun

ADDITIONAL_C_FILES += $(filter-out $(TARGET).c, $(wildcard *.c))

include $(CH32V003FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean

format:
	clang-format --style=file -i *.c *.h

