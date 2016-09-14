/*
 *Filename: accelerometer_driver.c
 * Team : Sensors 
 * Authors:
 *  -Kyle Ross (rosskyle)
 *  -
 *  - 
 *  - 
 *  -
 * 
 * 
 * Last edited: March 9, 2015
 *
 * References : 
 * - https://github.com/kersny/chibios-stm32f3discovery/blob/master/main.c
 * - http://chibios.sourceforge.net/html/group___i2_c.html
 */


#include <ch.h>
#include <hal.h>
#include <stdlib.h>

#include <accelerometer_driver.h>



uint8_t sensor_test = 0;
//I2CDriver I2CD1;
static const I2CConfig i2c_config = {
  0x00902025, //voodoo magic
  0,
  0
};

//testing = 1;


void i2c_init(){

  if (sensor_test) {
    chprintf((BaseSequentialStream*)&SD1, "Initializing i2c\n\r");
  }

  //Setting appropriate pins/AF mode
  palSetPadMode(GPIOA, 5, PAL_MODE_ALTERNATE(4));
  palSetPadMode(GPIOA, 7, PAL_MODE_ALTERNATE(4));
  palSetPadMode(GPIOE, 3, PAL_MODE_ALTERNATE(4));

}

uint8_t readByteI2C(uint8_t addr, uint8_t reg){
  uint8_t data;
  i2cAcquireBus(&I2CD1);
  (void)i2cMasterTransmitTimeout(&I2CD1, addr, &reg, 1, &data, 1, TIME_INFINITE);
  i2cReleaseBus(&I2CD1);

  return data;

}

//writeByteI2C(0x19, 0x20, 0x97);
void writeByteI2C(uint8_t addr, uint8_t reg, uint8_t val){

  msg_t status = 0;
  uint8_t cmd[] = {reg, val};
  i2cAcquireBus(&I2CD1);
  status = i2cMasterTransmitTimeout(&I2CD1, addr, cmd, 2, NULL, 0, TIME_INFINITE);
  i2cReleaseBus(&I2CD1);
  if (status < 0) {
    chprintf((BaseSequentialStream*)&SD1, "I DID: %i\n\n", status); 
  }
}

void accelerometer_init(){

  //0x20 is Accelerometer. internal register 0x20
  uint8_t value = 0;
  i2cStart(&I2CD1, &i2c_config);

  //reading control register 1 instead of WHOAMI, since WHOAMI is unlisted.
  //should be 0x07 assuming the board previously had no power.
  /*
  unsigned char WHOAMI[1];
  i2cAcquireBus(&I2CD1);
  msg_t message = i2cMasterTransmitTimeout(&I2CD1, 0x19, 0x20, 1, &WHOAMI[0], 1 ,TIME_INFINITE);
  (void) message;
  i2cReleaseBus(&I2CD1);

  if (WHOAMI == 0x07) {
    chprintf((BaseSequentialStream*)&SD1, "Accelerometer booted from power down successfully \n\r");
  }
  */

  /* BEFORE CHANGES 3/29
  value = 0x27;
  writeByteI2C(0x19, 0x20, value);

  value = 0x40;
  writeByteI2C(0x19, 0x23, value);

  */
  //CHANGED 3/29
  value = 0x20 | 0x07;               //Normal / Low-Power mode (10 Hz),
  writeByteI2C(0x19, 0x20, value);  // Enable all axes.

  value = 0x08;                    //High Resolution Output Mode ENABLED
  writeByteI2C(0x19, 0x23, value); //Continuous update, Little Endian (LSB lower),
                                   //Full Scale Selection +- 2g. 

  value = 0x10|0x80;              //High Pass Filter Cutoff Freq 1
  writeByteI2C(0x19, 0x21, value);//FILTER MODE: NORMAL


  //value = 0x20 | 0x0A;
  value = 0x0A; // enables X, Y HIGH Threshold interrupts
  writeByteI2C(0x19, 0x30, value);

  value = 0x7F;//0111 1111 is the threshold.
  writeByteI2C(0x19, 0x32, value);
}

void accelerometer_read(int16_t *data){
  //*data = readByteI2C(0x32,//Registers for data);
  if (sensor_test) {
    chprintf((BaseSequentialStream*)&SD1, "Reading accelerometer\n\r");
  }
  uint8_t i;

  uint8_t start_reg = 0x28;
  uint8_t output[6];
  i2cAcquireBus(&I2CD1);
  for (i = 0; i < 6; i++) {
    msg_t message = i2cMasterTransmitTimeout(&I2CD1, 0x19, &start_reg, 1, &output[i], 1, TIME_INFINITE);
    start_reg++;
    (void)message;
  }
  i2cReleaseBus(&I2CD1);

  // Bitshifting of output[7]'s data,
  //and then converting that to readable numbers.
  //rlemke stole conversion from 335
  
  int16_t val_x = ((int16_t)((uint16_t)output[1] << 8) + output[0])/(uint8_t)16;
  int16_t val_y = ((int16_t)((uint16_t)output[3] << 8) + output[2])/(uint8_t)16;
  int16_t val_z = ((int16_t)((uint16_t)output[5] << 8) + output[4])/(uint8_t)16;
 
  //chprintf((BaseSequentialStream*)&SD1, " %5d,  %5d, %5d\n\r", val_x, val_y, val_z );
  
   
  /* Not sure about these conversions */
  data[0] = val_x;
  data[1] = val_y;
  data[2] = val_z;
  
}



void accelerometer_power_mode(uint8_t mode){

  if (mode == LOW_POWER) {
    if (sensor_test) {
      chprintf((BaseSequentialStream*)&SD1, "Accelerometer set to low power\n\r");
    }
    writeByteI2C(ACCEL_ADDRESS, CTRL_REG1_A, mode);
  } else if (mode == HIGH_POWER) {
    if (sensor_test) {
      chprintf((BaseSequentialStream*)&SD1, "Accelerometer set to high power\n\r");
    }
    writeByteI2C(ACCEL_ADDRESS, CTRL_REG1_A, mode);
  }
  else {
    if (sensor_test) {
      chprintf((BaseSequentialStream*)&SD1, "Accelerometer failed to set power mode\n\r");
    }
  }
  //For if Core Systems wants to turn to low-power/power down.
  //Power Down: mode = 0x00;
  //writeByteI2C(0x32, 0x20, mode);
  writeByteI2C(0x19, CTRL_REG1_A, mode);

}
