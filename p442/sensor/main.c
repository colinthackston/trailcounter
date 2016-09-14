/*
  ChibiOS - Copyright (C) 2006-2014 Giovanni Di Sirio

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "test.h"
#include "shell.h" 
#include "chprintf.h"
#include <chstreams.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "../ChibiOS/ext/fatfs/src/ff.h"
#include "mmc_spi.h"


#define UNUSED(x) (void)(x)
// Macro that reads the CD pin - checks if SD card is inserted
#define cardInserted() palReadPad(GPIOD, GPIOD_PIN9)
static THD_WORKING_AREA(waShell,2048);

static thread_t *shelltp1;

//400Hz =  400 in 1 second
// 400 * 60 = 24,000
//10 min = 240,000 entries per axis
//

int currBuffer = 1;

#define ARRAY_LENGTH 25

int16_t xData1[ARRAY_LENGTH];
int16_t yData1[ARRAY_LENGTH];
int16_t zData1[ARRAY_LENGTH];
 
int16_t xData2[ARRAY_LENGTH];
int16_t yData2[ARRAY_LENGTH];
int16_t zData2[ARRAY_LENGTH];

int16_t xData3[ARRAY_LENGTH];
int16_t yData3[ARRAY_LENGTH];
int16_t zData3[ARRAY_LENGTH];

uint8_t btnData1[ARRAY_LENGTH];
uint8_t btnData2[ARRAY_LENGTH];
uint8_t btnData3[ARRAY_LENGTH];

char tempxyzData[20];
char final[500];

int count = 0;

int startWriting = 0; // 0 == nothing; 1 == data1; 2 == data2

int stop = 0;

uint8_t queue[10];
int head = 0;
int tail = 0;

int button = 0;

void writeToSD(void);

// the FATFS global variable - we will use this for file transfer
FATFS SDC_FS;
FIL fil; /* a file object */
FRESULT rc; /* Result code */
DIR dir; /* Directory object */
FILINFO fno; /* File information object */
UINT bw, br; /* bw = bytes written; br = bytes read */
static mutex_t sdMutex; /* mutex to safely access the SD card */
MMCDriver MMCD1; /* the mmc driver */

// the High speed spi config
static const SPIConfig highSdcSpiConfig = {
  NULL,
  GPIOC,
  GPIOC_PIN0,
  0,
  0
};

// the low speed spi config
static const SPIConfig lowSdcSpiConfig = {
  NULL,
  GPIOC,
  GPIOC_PIN0,
  SPI_CR1_BR_2|SPI_CR1_BR_1,
  0
};

// initialize the mmc device over spi link 2
static const MMCConfig mmcConfig = {
  &SPID2,
  &lowSdcSpiConfig,
  &highSdcSpiConfig
};

/* SPI configuration, sets up PortE Bit 3 as the chip select for the gyro */
static SPIConfig accel_cfg = {
  NULL,
  GPIOA,
  8,
  SPI_CR1_BR_2 | SPI_CR1_BR_1,
  0
};

// initializes the mmc
void init_mmc(MMCDriver *mmcd) {
  palSetPadMode(GPIOB, 13, PAL_MODE_ALTERNATE(5));     // SCK. 
  palSetPadMode(GPIOB, 14, PAL_MODE_ALTERNATE(5));     // MISO.
  palSetPadMode(GPIOB, 15, PAL_MODE_ALTERNATE(5));     // MOSI.  

  // set the CS to push pull mode
  palSetPadMode(GPIOC, GPIOC_PIN0, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad(GPIOC, GPIOC_PIN0);

  // set the CD pin to "in" mode 
  palSetPadMode(GPIOD, 9, PAL_MODE_INPUT_PULLUP);
  palClearPad(GPIOD, GPIOD_PIN9);
 
  // set up the mmcObject and start it
  mmcObjectInit(mmcd);
  mmcStart(mmcd, &mmcConfig);
  chMtxObjectInit(&sdMutex);
}



uint8_t accel_read_register (uint8_t address) {
  uint8_t receive_data;
  uint8_t cmd = 0x0B;

  //spiAcquireBus(&SPID1);               /* Acquire ownership of the bus.    */
  spiStart(&SPID1, &accel_cfg);         /* Setup transfer parameters.       */
  spiSelect(&SPID1);                   /* Slave Select assertion.          */
  spiSend(&SPID1, 1, &cmd);
  spiSend(&SPID1, 1, &address);        /* Send the address byte */
  spiReceive(&SPID1, 1,&receive_data); 
  spiUnselect(&SPID1);                 /* Slave Select de-assertion.       */
  //spiReleaseBus(&SPID1);               /* Ownership release.               */
  return (receive_data);
}

void accel_write_register (uint8_t address, uint8_t data) {
  uint8_t cmd = 0x0A;
  // address = address & (~0x80);         /* Clear the write bit (bit 7)      */
  //spiAcquireBus(&SPID1);               /* Acquire ownership of the bus.    */
  spiStart(&SPID1, &accel_cfg);         /* Setup transfer parameters.       */
  spiSelect(&SPID1);                   /* Slave Select assertion.          */
  spiSend(&SPID1, 1, &cmd);
  spiSend(&SPID1, 1, &address);        /* Send the address byte            */
  spiSend(&SPID1, 1, &data); 
  spiUnselect(&SPID1);                 /* Slave Select de-assertion.       */
  // spiReleaseBus(&SPID1);               /* Ownership release.               */
}

/* Thread that blinks North LED as an "alive" indicator */
static THD_WORKING_AREA(waCounterThread,128);
static THD_FUNCTION(counterThread,arg) {
  UNUSED(arg);
  while (TRUE) {
    palSetPad(GPIOE, GPIOE_LED3_RED);
    chThdSleepMilliseconds(500);
    palClearPad(GPIOE, GPIOE_LED3_RED);
    chThdSleepMilliseconds(500);
  }
}

/* Thread that blinks North LED as an "alive" indicator */
static THD_WORKING_AREA(waReadThread,128);
static THD_FUNCTION(readThread,arg) {
  UNUSED(arg);
  int flag = 1;
  int flag2 = 1;

  while(stop < 2500) {

    if ((accel_read_register(0x0B) & 65) == 65) {

      uint8_t xL = accel_read_register(0x0E);
      uint8_t xH = accel_read_register(0x0F);
      uint8_t yL = accel_read_register(0x10);
      uint8_t yH = accel_read_register(0x11);
      uint8_t zL = accel_read_register(0x12);
      uint8_t zH = accel_read_register(0x13);

      if (flag) {
	palSetPad(GPIOB, 1);
      } else {
	palClearPad(GPIOB, 1);
      }
      flag = !flag;

      int16_t x;
      int16_t y;
      int16_t z;

      x = xH<<8;
      y = yH<<8;
      z = zH<<8;
  
      x = x | xL;
      y = y | yL;
      z = z | zL;

      // chprintf((BaseSequentialStream*)&SD1, "%04x %04x %04x\n\r",(int) x, (int) y,(int)z);
      // chprintf((BaseSequentialStream*)&SD1, "%5d %5d %5d\n\r",x,y,z);

      if (!(palReadPad(GPIOA, 2))) {
	button = 1;
	palSetPad(GPIOE, GPIOE_LED4_BLUE);
      } else {
	button = 0;
	palClearPad(GPIOE, GPIOE_LED4_BLUE);
      }

      if (currBuffer == 1) {      // if currBuffer == 1
	xData1[count] = x;
	yData1[count] = y;
	zData1[count] = z; 
	btnData1[count] = button;

      } else if (currBuffer == 2) {
	xData2[count] = x;
	yData2[count] = y;
	zData2[count] = z;
	btnData2[count] = button;

      } else if (currBuffer == 3) {
	xData3[count] = x;
	yData3[count] = y;
	zData3[count] = z;
	btnData3[count] = button;
      }
    
      count++;

      if (count == ARRAY_LENGTH) {

	if (flag2) {
	  palSetPad(GPIOB, 2);
	} else {
	  palClearPad(GPIOB, 2);
	}
	flag2 = !flag2;
	count = 0;

	if (currBuffer == 1) {
	  currBuffer = 2;
	  queue[head] = 1;
	  head = (head + 1) % 10;
	
	} else if (currBuffer == 2) {
	  currBuffer = 3;
	  queue[head] = 2;
	  head = (head + 1) % 10;

	} else if (currBuffer == 3) {
	  currBuffer = 1;
	  queue[head] = 3;
	  head = (head + 1) % 10;
	}

      
      }
      stop++;
      if (stop == 2500) palSetPad(GPIOE, GPIOE_LED5_ORANGE);
    }
    chThdSleepMilliseconds(1);
  }
}


/* Thread that blinks North LED as an "alive" indicator */
static THD_WORKING_AREA(waWriteThread,4096);
static THD_FUNCTION(writeThread,arg) {
  rc = f_mount(&SDC_FS, "", 0);
  int i,j = 0;

  int finalCount = 0;


  while(1) {
    if (!(head == tail)) {
      
      int16_t* xdat;
      int16_t* ydat;
      int16_t* zdat;
      int8_t* btndat;
      
      if (queue[tail] == 1) {
	xdat = xData1;
	ydat = yData1;
	zdat = zData1;
	btndat = btnData1;
      }
      if (queue[tail] == 2) {
	xdat = xData2;
	ydat = yData2;
	zdat = zData2;
	btndat = btnData2;
      }
      if (queue[tail] == 3) {
	xdat = xData3;
	ydat = yData3;
	zdat = zData3;
	btndat = btnData3;
      }

      char* tempfinal = final;

      for (i = 0; i < 25; i++) {
	int len = sprintf(tempfinal, "%05d %05d %05d %d\n", xdat[i], ydat[i], zdat[i], btndat[i]);
	tempfinal += len;
      }
      //palSetPad(GPIOB, 0);
      palSetPad(GPIOE, GPIOE_LED5_ORANGE);
      appendToCard(&fil, "dataTest.txt", final, tempfinal - final);
      //palClearPad(GPIOB, 0);
      palClearPad(GPIOE, GPIOE_LED5_ORANGE);
      tail = (tail + 1) % 10;
    }
    chThdSleepMilliseconds(1);
  }
}

void die(int rc) {
  chprintf((BaseSequentialStream*)&SD1, "failed with error: %d\n\r", rc);
}

int appendToCard(FIL *fp, const TCHAR *path, const void *buff, UINT len) {
  // this will store the number of bytes written to a file
  int flag = 0;
  UINT bw;
  FRESULT temp;
  
  chMtxLock(&sdMutex);
  // open the file in write mode
  temp = f_open(fp, path, FA_WRITE|FA_OPEN_ALWAYS);
  if (temp) {
    die(temp);
    flag = temp;
  } 

  // use this once to make sure that we are at the end of the file
  temp = f_lseek(fp, f_size(fp));
  if (temp) {
    die(temp);
    flag = temp;
  } 

  // write the buffer to the file, if no bytes were written throw an error
  temp = f_write(fp, buff, len, &bw); 
  //  chprintf((BaseSequentialStream*)&SD1, "tried to write: %s\n\r", (char *)buff);
  if (temp || !bw) {
    die(temp);
    flag = temp;
  } 

  temp = f_close(fp);
  if (temp) {
    die(temp);
    flag = temp;
  }
  chMtxUnlock(&sdMutex);
  return flag;
}



/*
  int writeToSDCard(void) {
  int result;
  int i = 0;

  int xyzData[10];

  for (i = 0; i < 10; i++) {
  sprintf(xyzData[i], "%03d, %03d, %03d\n", xData[i], yData[i], zData[i]);
  chprintf((BaseSequentialStream*)&SD1, "number %d \n\r",i);
  }

  rc = f_mount(&SDC_FS, "", 0);
  if (rc) die(rc);
  result = appendToCard(&fil, "dataTest.txt", xyzData, strlen(xyzData));
  if (!result) return 1;
  else return 0;

  }
*/

void initSDcard(void) {
  init_mmc(&MMCD1);
  chprintf((BaseSequentialStream*)&SD1, "Initialized SD card\n\r");
  
  if (cardInserted()) {
    chprintf((BaseSequentialStream*)&SD1, "A card is there.\n\r");
    mmcConnect(&MMCD1);
  } else {
    chprintf((BaseSequentialStream*)&SD1, "No card to be found.\n\r");
  }
  // let's see if we can write to the SD card
  //if (can_write_to_card()) chprintf((BaseSequentialStream*)&SD1, "Wrote to SD card.\n\r");
}

static void cmd_myecho(BaseSequentialStream *chp, int argc, char *argv[]) {
  int32_t i;

  (void)argv;

  for (i=0;i<argc;i++) {
    chprintf(chp, "%s\n\r", argv[i]);
  }
}

static void cmd_read(BaseSequentialStream *chp, int argc, char *argv[]) {
  int count = 0;
  (void)argv;

  while(1) {
    
    // uint8_t x8 = accel_read_register(0x08);
    // uint8_t y8 = accel_read_register(0x09);
    // uint8_t z8 = accel_read_register(0x0A);

    uint8_t xL = accel_read_register(0x0E);
    uint8_t xH = accel_read_register(0x0F);
    uint8_t yL = accel_read_register(0x10);
    uint8_t yH = accel_read_register(0x11);
    uint8_t zL = accel_read_register(0x12);
    uint8_t zH = accel_read_register(0x13);
    int16_t x;
    int16_t y;
    int16_t z;

    x = xH<<8;
    y = yH<<8;
    z = zH<<8;
  
    x = x | xL;
    y = y | yL;
    z = z | zL;

    // chprintf((BaseSequentialStream*)&SD1, "%04x %04x %04x\n\r",(int) x, (int) y,(int)z);
    chprintf((BaseSequentialStream*)&SD1, "%5d %5d %5d\n\r",x,y,z);


    if (!currBuffer) {      // if currBuffer == 1
      xData1[count] = x;
      yData1[count] = y;
      zData1[count] = z;

    } else {
      xData2[count] = x;
      yData2[count] = y;
      zData2[count] = z;
    }
    
    count++;

    if (count == ARRAY_LENGTH) {
      count = 0;
      if (currBuffer) {
	currBuffer = 0;
	startWriting = 2;
	writeToSD();
      } else {
	currBuffer = 1;
	startWriting = 1;
	writeToSD();
      }

    
      chThdSleepMilliseconds(10);

  }

}
}


  static void cmd_dataread(BaseSequentialStream *chp, int argc, char *argv[]) {
    int32_t i;

    (void)argv;
  
    for (i=0;i<100;i++) {
      chprintf(chp, "%d,", xData1[i]);
      chprintf(chp, "%d,", yData1[i]);
      chprintf(chp, "%d\n\r", zData1[i]);
    }
  
  }

  static void cmd_writeSD(BaseSequentialStream *chp, int argc, char *argv[]) {

    (void)argv;

    int i = 0;

    char tempData[100];

    rc = f_mount(&SDC_FS, "", 0);

    if (rc) die(rc);
    /*
    for (i = 0; i < 100; i++) {
      sprintf(tempData, "%03d %03d %03d\n", xData1[i], yData1[i], zData1[i]);
      appendToCard(&fil, "dataTest.txt", tempData, strlen(tempData));
    }
    */
  }

  static const ShellCommand commands[] = {
    {"dataread", cmd_dataread},
    {"myecho", cmd_myecho},
    {"read", cmd_read},
    {"writeSD", cmd_writeSD},
    {NULL, NULL}
  };

  void writeToSD(void) {

    int i = 0;

    char tempxyzData[ARRAY_LENGTH];

    if (startWriting == 1) {

      rc = f_mount(&SDC_FS, "", 0);

      if (rc) die(rc);

      for (i = 0; i < ARRAY_LENGTH; i++) {
	sprintf(tempxyzData, "%03d %03d %03d\n", xData1[i], yData1[i], zData1[i]);
	appendToCard(&fil, "dataTest.txt", tempxyzData, strlen(tempxyzData));
      }

    } else if (startWriting == 2) {
      rc = f_mount(&SDC_FS, "", 0);

      if (rc) die(rc);

      for (i = 0; i < ARRAY_LENGTH; i++) {
	sprintf(tempxyzData, "%03d %03d %03d\n", xData2[i], yData2[i], zData2[i]);
	appendToCard(&fil, "dataTest.txt", tempxyzData, strlen(tempxyzData));
      }
    }
  }

  static const ShellConfig shell_cfg1 = {
    (BaseSequentialStream *)&SD1,
    commands
  };

  static void termination_handler(eventid_t id) {

    (void)id;
    chprintf((BaseSequentialStream*)&SD1, "Shell Died\n\r");

    if (shelltp1 && chThdTerminatedX(shelltp1)) {
      chThdWait(shelltp1);
      chprintf((BaseSequentialStream*)&SD1, "Restarting from termination handler\n\r");
      chThdSleepMilliseconds(100);
      shelltp1 = shellCreate(&shell_cfg1, sizeof(waShell), NORMALPRIO);
    }
  }

  static evhandler_t fhandlers[] = {
    termination_handler
  };

  /*
   * Application entry point.
   */

  int main(void) {
    event_listener_t tel;
    /*
     * System initializations.
     * - HAL initialization, this also initializes the configured device drivers
     *   and performs the board-specific initializations.
     * - Kernel initialization, the main() function becomes a thread and the
     *   RTOS is active.
     */
    halInit();
    chSysInit();

    /*
     * Activates the serial driver 1 using the driver default configuration.
     * PC4(RX) and PC5(TX). The default baud rate is 9600.
     */
    sdStart(&SD1, NULL);
    palSetPadMode(GPIOC, 4, PAL_MODE_ALTERNATE(7));
    palSetPadMode(GPIOC, 5, PAL_MODE_ALTERNATE(7));

    /* 
     *  Setup the pins for the spi link on the GPIOA. This link connects to the pressure sensor and the gyro.  
     * 
     */

    palSetPadMode(GPIOA, 5, PAL_MODE_ALTERNATE(5));     /* SCK. */
    palSetPadMode(GPIOA, 6, PAL_MODE_ALTERNATE(5));     /* MISO.*/
    palSetPadMode(GPIOA, 7, PAL_MODE_ALTERNATE(5));     /* MOSI.*/
    palSetPadMode(GPIOA, 8, PAL_MODE_OUTPUT_PUSHPULL);  /* accel sensor chip select */
    palSetPad(GPIOA, 8);                                /* Deassert the accel sensor chip select */

    palSetPadMode(GPIOA, 2, PAL_MODE_INPUT);

    palSetPadMode(GPIOB, 0, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(GPIOB, 1, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(GPIOB, 2, PAL_MODE_OUTPUT_PUSHPULL);

    palClearPad(GPIOB, 0);
    palClearPad(GPIOB, 1);
    palClearPad(GPIOB, 2);



    chprintf((BaseSequentialStream*)&SD1, "Accel Whoami Byte = 0x%02x\n\r",accel_read_register(0x00));
    // chprintf((BaseSequentialStream*)&SD1, "Accel Whoami Byte = 0x%02x\n\r",accel_read_register(0x0B));

    // accel_write_register(0x2C, 23); // filter control - 2g - conservative filter - 400Hz -  0001 0111
    //accel_write_register(0x2C, 18); // filter control - 2g - conservative filter - 50Hz -  0001 0010
    accel_write_register(0x2C, 19); // filter control - 2g - conservative filter - 100Hz -  0001 0011
    //accel_write_register(0x2D, 34); // power control - ultralow noise - measure mode - 0010 0010
    // accel_write_register(0x2D, 2); // power control - normal noise - measure mode - 0000 0010
    accel_write_register(0x2D, 18); // power control - low noise - measure mode - 0001 0010

    initSDcard();

    /* Initialize the command shell */ 
    shellInit();
    chThdSleepMilliseconds(100);

    /* 
     *  setup to listen for the shell_terminated event. This setup will be stored in the tel  * event listner structure in item 0
     */
    chEvtRegister(&shell_terminated, &tel, 0);

    shelltp1 = shellCreate(&shell_cfg1, sizeof(waShell), NORMALPRIO);
    chThdSleepMilliseconds(100);
    chThdCreateStatic(waCounterThread, sizeof(waCounterThread), NORMALPRIO+1, counterThread, NULL);
    chThdCreateStatic(waReadThread, sizeof(waReadThread), NORMALPRIO+1, readThread, NULL);
    chThdCreateStatic(waWriteThread, sizeof(waWriteThread), NORMALPRIO+1, writeThread, NULL);
 
    while (TRUE) {
      chEvtDispatch(fhandlers, chEvtWaitOne(ALL_EVENTS));
    }
  }


