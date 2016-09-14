/*
*Filename: accelerometer_driver.h
* Team : Sensors
* Authors:
* -Kyle Ross (rosskyle)
* -et all
* -
* -
* -
*
*
* Last edited: March 9, 2015
*
* Reference : https://github.com/kersny/chibios-stm32f3discovery/blob/master/main.c
*
*/

#include "ch.h"
#include "hal.h"
#include "test.h"
#include "shell.h"
#include "chprintf.h"
#include "arm_math.h"
#include "i2c.h"
#include "drivers.h"
#include <atoh.h>
#include <dtoa.h>
#include <chstreams.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

//REGISTERS
#define CTRL_REG1_A 0x20
//ACCELEROMETER INFO
#define ACCEL_ADDRESS 0x32

//Power modes
static uint8_t LOW_POWER = 0x19;
static uint8_t HIGH_POWER = 0x79;

void i2c_init(void);
void writeByteI2C(uint8_t addr, uint8_t reg, uint8_t val);
uint8_t readByteI2C(uint8_t addr, uint8_t reg);
void accelerometer_init(void);
void accelerometer_read(int16_t* data);
void accelerometer_power_mode(uint8_t mode);
