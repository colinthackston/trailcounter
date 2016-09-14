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

/*-----------------------------------things to do----------------------------------------------------
 * temp: maybe check out this -> http://www.micromouseonline.com/2009/05/26/simple-adc-use-on-the-stm32/
 * more temperature: http://www.chibios.org/dokuwiki/doku.php?id=chibios:articles:stm32f4_discovery_code
 * GPS reading: reading every 6 seconds, should per second, disable sentences
 * High resolution time comparison: prescalar 32,768kHz, we take counter when interrupt takes RTC time
-----------------------------------------------------------------------------------------------------*/

/*------------------------------------things done--------------------------------------------------------
 * semaphore done
 * Time in milliseconds resolutiom
 * SD card startup(connections etc.)
 * SD card read/write : http://chibios.sourceforge.net/html/group___s_d_c.html
 ----------------------------------------------------------------------------------------------------*/

#include "ch.h"
#include "hal.h"
#include "test.h"
#include "shell.h" 
#include "rtc.h"
#include "chprintf.h"
#include <chstreams.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "../ChibiOS/ext/fatfs/src/ff.h"
#include "mmc_spi.h"

#define UNUSED(x) (void)(x)
static ADCConfig adccfg = {0};
// Create buffer to store ADC results. This is
// one-dimensional interleaved array
#define ADC_BUF_DEPTH 1 // depth of buffer 
#define ADC_CH_NUM 1    // number of used ADC channels

const float V25 = 1.41;// when V25=1.41V at ref 3.3V
const float Avg_Slope = 0.0043; //avg_slope=4.3mV/C at ref 3.3V

static adcsample_t samples_buf[ADC_BUF_DEPTH * ADC_CH_NUM]; // results array

int temperature;            //temperature
int total_count = 0;      //edge triggered count
binary_semaphore_t bsem; //semaphore for thread
uint32_t curGPS;       //keeps current GPS time
uint32_t lastGPS = 0; //keeps track of previous GPS time
uint32_t lastRTC = 0; //keeps track of previous RTC time
uint32_t dayGPS = 0;  //how many days GPS has been counting
uint32_t dayRTC = 0;  //how many days RTC has been counting
uint32_t edgeTime = 0;  //time that each edge triggered

int tempList[80][2];

// Macro that reads the CD pin - checks if SD card is inserted
#define cardInserted() palReadPad(GPIOD, GPIOD_PIN9)


int leds[] = {
  GPIOE_LED3_RED,
  GPIOE_LED5_ORANGE,
  GPIOE_LED7_GREEN, 
  GPIOE_LED9_BLUE,
  GPIOE_LED10_RED,
  GPIOE_LED8_ORANGE,
  GPIOE_LED6_GREEN,
  GPIOE_LED4_BLUE
};

/*------------------------------------------------------------Tempurature Related----------------------------------------------------------*/
// Fill ADCConversionGroup structure fields
static const ADCConversionGroup adcgrpcfg = {
  FALSE,    
  1,     
  NULL,    
  NULL,    
  0,      /* CFGR */    
  0,      /* TR1 */
  ADC_CCR_VREFEN | ADC_CCR_TSEN, /* CCR */

  /* SMPR1 */
  {0,
   /* SMPR2 */
   ADC_SMPR2_SMP_AN16(ADC_SMPR_SMP_601P5)},

  /* SQR1 */
  {ADC_SQR1_NUM_CH(1) |
   ADC_SQR1_SQ1_N(ADC_CHANNEL_IN16),

   /* SQR2 */
   0,

   /* SQR3 */
   0,

   /* SQR4 */
   0},
};

/*------------------------------------------------------SD card related--------------------------------------------------------------*/
void die(temp);

int writeToSDCard(void);

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

void die(int rc) {
  chprintf((BaseSequentialStream*)&SD1, "failed with error: %d\n\r", rc);
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
  //if (can_write_to_card()) chprintf((BaseSequentialStream*)&SD1, "Wrote to SD card.\n\r");
}

static void cmd_writeSD(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  
  //  int i = 0;
  //  int curTime;
  // RTCDateTime curRTC;
  char tempData[100];

  //  rc = f_mount(&SDC_FS, "", 0);

  // if (rc) die(rc);

    /* rtcGetTime(&RTCD1, &curRTC); */
    /* curTime = curRTC.millisecond - curGPS; */
    /* if (curRTC.millisecond < curGPS){ */
    /*   curTime +=  86400000; */
    /* } */
    /* curTime -= last; */
    /* last = curTime; */

    sprintf(tempData, "hallo \n\r");
    appendToCard(&fil, "dataTest.txt", tempData, strlen(tempData));

}

/*--------------------------------------------------------Time related---------------------------------------------------------------------*/
//GPS baud configuration
static SerialConfig gpsCfg =
{
56700, // bit rate
};

static THD_WORKING_AREA(waShell,2048);
static thread_t *shelltp1;

/* Thread that blinks North LED as an "alive" indicator */
static THD_WORKING_AREA(waCounterThread,128);
static THD_FUNCTION(counterThread,arg) {
  UNUSED(arg);
  while (TRUE) {
    chThdSleepMilliseconds(250);
    palSetPad(GPIOE, leds[0]);
    chThdSleepMilliseconds(250);
    palClearPad(GPIOE, leds[0]);
  }
}

/* Thread that reads GPS stream and parse */
static THD_WORKING_AREA(waDataThread,128);
static THD_FUNCTION(dataThread,arg) {
  UNUSED(arg);
  int i;
  int j;
  char block[60];
  char x;
  uint32_t hour;
  uint32_t min;
  uint32_t sec;
  uint32_t milli;
  char* needle = "$GPGGA";

  sdRead(&SD2, block, 30);
  while (TRUE) {

    palSetPad(GPIOE, leds[1]);
    sdRead(&SD2, block + 30, 30);
    
    char temp = block[50];
    block[50] = 0;

    char* gpgga = strstr(block, needle);

    block[50] = temp;

    if (gpgga != 0) {
      gpgga[16] = 0;
      curGPS = atoi(gpgga + 7); 
      milli = atoi(gpgga+14) * 10;
      sec = (curGPS % 100) * 1000;
      curGPS /= 100;
      min = (curGPS % 100) * 60000;
      curGPS /= 100;
      hour = curGPS * 3600000;
      curGPS = (sec + min + hour + milli);

      gpgga[22] = 0;

      //chprintf((BaseSequentialStream*)&SD1,"%s\n\r", gpgga+7);
      //chprintf((BaseSequentialStream*)&SD1,"%d %d %d %d\n\r",hour, min, sec, milli );
      //chprintf((BaseSequentialStream*)&SD1,"%d\n\r", curGPS);
    }
    
    
    for (i = 0; i < 30; i++) {
      block[i] = block[i + 30];
    }

    
    //    printf("Temperature: %d%cC\r\n", temp, 176);
    palClearPad(GPIOE, leds[1]);

  }
}

/* RTC_time polling */
static THD_WORKING_AREA(waButtonThread,2048);
static THD_FUNCTION(buttonThread,arg) {
  UNUSED(arg);
  while (TRUE) {
    RTCDateTime curRTC;
    uint32_t curTime;
    int i;
    char tempData[100];
    chBSemWait(&bsem);
    /* poll RTC*/
    palSetPad(GPIOE, leds[4]);
    //chprintf((BaseSequentialStream*)&SD1,"interrupt\n\r");
    rtcGetTime(&RTCD1, &curRTC);
    
    if (lastGPS > curGPS ){
      dayGPS++;
    }
    if (lastRTC > curRTC.millisecond){
      dayRTC++;
    }
    
    curTime = curRTC.millisecond - curGPS;
    
    if (dayRTC < dayGPS){
      curTime +=  86400000;
    }

    adcStartConversion(&ADCD1, &adcgrpcfg, samples_buf, ADC_BUF_DEPTH);
    chThdSleepMilliseconds(100);
    temperature = (int)((V25 - ((3.3 * samples_buf[0])/ 4096.0))/Avg_Slope + 25);
    //chprintf((BaseSequentialStream*)&SD1,"curRTC milliseconds: %d\r\n", curRTC.millisecond);
    //chprintf((BaseSequentialStream*)&SD1,"curGPS: %d\r\n", curGPS);
    //chprintf((BaseSequentialStream*)&SD1,"curTime: %d\r\n", curTime);
    
    sprintf(tempData, "%.10d,%.10d,%.10d,%.03d,%d\n", curRTC.millisecond, curGPS, curTime, temperature, samples_buf[0]);
    //sprintf(tempData, "CAN PRINT\n");
    //chprintf((BaseSequentialStream*)&SD1,"%s\r\n", tempData);    


    appendToCard(&fil, "dataTest.txt", tempData, strlen(tempData));

    //chprintf((BaseSequentialStream*)&SD1,"Appended\r\n");    
    // appendToCard(&fil, "dataTest.txt", tempData, strlen(tempData)); 

    // cmd_writeSD(0,0,0);
    
    palClearPad(GPIOE, leds[4]);
    total_count += 1;
    //tempList[temperature+40][0] += 1;
    //tempList[temperature+40][1] += curTime;
  }
}


/*--------------------------------------------------------Commands------------------------------------------------------------------*/
/*This should be telling time, including millisecond*/
static void cmd_time(BaseSequentialStream *chp, int argc, char *argv[]){
  RTCDateTime time;
  struct tm ltime;
  char * finalTime;
  (void)argv;
  (void)argc;
  
  /* Attempting to get time here...*/
  rtcGetTime(&RTCD1, &time);
  chprintf(chp,"\n\r");
  rtcConvertDateTimeToStructTm(&time,&ltime, NULL);
  finalTime = asctime(&ltime);

  samples_buf[0] = 0x0055;
  adcStartConversion(&ADCD1, &adcgrpcfg, samples_buf, ADC_BUF_DEPTH);
  chThdSleepMilliseconds(100);
  temperature = (int)((V25 - ((3.3 * samples_buf[0])/ 4096.0))/Avg_Slope + 25);
  chprintf((BaseSequentialStream*)&SD1,"Converter: %d\r\n", samples_buf[0]);
  chprintf((BaseSequentialStream*)&SD1,"Temperature: %d\r\n", temperature);
  chprintf((BaseSequentialStream*)&SD1,"Count: %d\r\n", total_count);
  chprintf(chp,"Time: %.19s:%d %s temp: %3d\n\r", finalTime, time.millisecond%1000, &finalTime[strlen(finalTime) - 5], temperature);
}

/*set a starting ttime*/
void cmd_rtcSet(BaseSequentialStream *chp, int argc, char *argv[]) {
  RTCDateTime rTime;
  struct tm * ltime;
  time_t epoch;
  (void)argv;
  (void)chp;
  (void)argc;
  
  epoch = (time_t) atoi(argv[0]);
  //  epoch -= 5 * 3600; // Convert from UTC time to EDT time.
  ltime = localtime(&epoch);
  rtcConvertStructTmToDateTime(ltime, 0, &rTime);
  rtcSetTime(&RTCD1, &rTime);
}


/*send gps command to only send GPGGA sentence*/
void cmd_data(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  (void)chp;
  (void)argc;
  int i;
  for(i = 0; i < 80; i++){
    if (tempList[i][0] != 0){
      chprintf((BaseSequentialStream*)&SD1,"temp:%d %d %d\r\n", i-40, tempList[i][0], tempList[i][1]);
    }
  }
}

/*Commands */
static const ShellCommand commands[] = {
  {"time", cmd_time},
  {"settime", cmd_rtcSet},
  {"writeSD", cmd_writeSD},
  {"datadump", cmd_data},
  {NULL, NULL}
};

/*Shell*/
static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SD1,
  commands
};


/*---------------------------------------------------------ext callback------------------------------------------------------------------*/
/* Callback for time compare */
static void extcb(EXTDriver *extp, expchannel_t channel) {
  (void)extp;
  (void)channel;

  chSysLockFromISR();
  chBSemSignalI(&bsem);
  chSysUnlockFromISR();
}

/* EXT configure*/
EXTConfig trailExtcfg = {
  {
    // Enable interrupt on PA0 on button
    {EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART | EXT_MODE_GPIOA, extcb},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL}
  }
};


/*---------------------------------------------------handlers and setups------------------------------------------------------------*/
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
  chBSemObjectInit(&bsem, true);
  /*EXT init*/
   extStart(&EXTD1, &trailExtcfg); 

   /*adc start*/
   adcStart(&ADCD1, &adccfg);
   
   adcStartConversion(&ADCD1, &adcgrpcfg, &samples_buf[0], ADC_BUF_DEPTH);

  /*
   * Activates the serial driver 1 using the driver default configuration.
   * PC4(RX) and PC5(TX). The default baud rate is 9600.
   * PA3(RX) and PA2(TX). The default baud rate is 9600.
   */
   sdStart(&SD2, &gpsCfg);
   palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
   palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));
   
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
 
   chprintf((BaseSequentialStream*)&SD1, "\n\rUp and Running SD1\n\r");
   
   /* Initialize the command shell */ 
   shellInit();
   
  /* Initialize the SD card */ 
   initSDcard();
   rc = f_mount(&SDC_FS, "", 0);
   
   if (rc) die(rc);
   
   /* 
    *  setup to listen for the shell_terminated event. This setup will be stored in the tel  * event listner structure in item 0
    */
   chEvtRegister(&shell_terminated, &tel, 0);
   
   int* test = 0x0C;

   chprintf((BaseSequentialStream*)&SD1, "\n\r TEST VAL: 0x%x \n\r", *test);

   shelltp1 = shellCreate(&shell_cfg1, sizeof(waShell), NORMALPRIO);
   chThdCreateStatic(waCounterThread, sizeof(waCounterThread), NORMALPRIO+1, counterThread, NULL);
   chThdCreateStatic(waDataThread, sizeof(waDataThread), NORMALPRIO+1, dataThread, NULL);
   chThdCreateStatic(waButtonThread, sizeof(waButtonThread), NORMALPRIO+1, buttonThread, NULL);
   
  while (TRUE) {
    chEvtDispatch(fhandlers, chEvtWaitOne(ALL_EVENTS));
  }
}

