/*
*Filename: sdc_driver.c
* Team : Data Representation
* Authors:
* - Jason Sprinkle (jdsprink)
* - Keith Milhoan (kmilhoan)
* -
* -
* -
*
*
* Last edited: April 3, 2015
*
*/

#include "shared.h"
#include "mmc_spi.h"
#include "../ChibiOS/ext/fatfs/src/ff.h"
#include "chprintf.h"

// Macro that reads the CD pin - checks if SD card is inserted
#define cardInserted() palReadPad(GPIOD, GPIOD_PIN9)

int appendToCard(FIL *fp, const TCHAR *path, const void *buff, UINT len); 
void unixPersonCountConcat(char *buff, int unix, int count);
void die(int rc);
void initSDcard(void);
