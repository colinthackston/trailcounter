/*
*Filename: sdc_driver.c
* Team : Data Representation
* Authors:
* - Jason Sprinkle (jdsprink)
* -
* -
* -
* -
*
*
* Last edited: April 2, 2015
*
* References :
* - https://github.com/kersny/chibios-stm32f3discovery/blob/master/main.c
* - http://chibios.sourceforge.net/html/group___i2_c.html
*/

#include "sdc_driver.h"
#include <stdio.h>

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


// this will return a 1 if it wrote to a file successfully
// and will return a 0 if it failed to write to the card

// FIL *fp = a pointer to a file
// const TCHAR *path = a string - should be name of the file you want to change
// const void *buff = the string that you would like to append to the file
// UINT len = length of the buff string

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

// this concatenates unix time and person count into one handy dandy string
void unixPersonCountConcat(char *buff, int unix, int count) {
  sprintf(buff, "%010d %04d\n", unix, count);
}

void die(int rc) {
  chprintf((BaseSequentialStream*)&SD1, "failed with error: %d\n\r", rc);
}

int can_write_to_card(void) {
  int result;
  char buff[25];
  int unix_time = 1000000000;
  int test_count = 5;
  rc = f_mount(&SDC_FS, "", 0);
  if (rc) die(rc);
  unixPersonCountConcat(buff, unix_time, test_count);
  result = appendToCard(&fil, "test.txt", buff, strlen(buff));
  if (!result) return 1;
  else return 0;
}

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
  if (can_write_to_card()) chprintf((BaseSequentialStream*)&SD1, "Wrote to SD card.\n\r");
}
