#include "ch.h"
#include "hal.h"
#include "test.h"
#include "shell.h" 
#include "chprintf.h"
#include <chstreams.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "../ChibiOS/ext/fatfs/src/ff.h"
#include "mmc_spi.h"

//global variables

int j;
int arrays_spot;
int MODE = 0;
int arrayuse = 0;
uint16_t amperage1[500];
uint16_t amperage2[500];  
char buf[11500];
int RUN_MODE1[500];
int RUN_MODE2[500];
binary_semaphore_t buff_sem;
uint16_t potential_0 = 0;
uint16_t potential_1 = 0;
uint16_t diff;


// the FATFS global variable - we will use this for file transfer
FATFS SDC_FS;
FIL fil; /* a file object */
FRESULT rc; /* Result code */
DIR dir; /* Directory object */
FILINFO fno; /* File information object */
UINT bw, br; /* bw = bytes written; br = bytes read */
static mutex_t sdMutex; /* mutex to safely access the SD card */
MMCDriver MMCD1; /* the mmc driver */
#define cardInserted() palReadPad(GPIOD, GPIOD_PIN9)
// the High speed spi config
static const SPIConfig highSdcSpiConfig = {
  NULL,
  GPIOC,
  GPIOC_PIN2,
  0,
  0
};

int appendToCard(FIL *fp, const TCHAR *path, const void *buff, UINT len);
// the low speed spi config
static const SPIConfig lowSdcSpiConfig = {
  NULL,
  GPIOC,
  GPIOC_PIN2,
  SPI_CR1_BR_2|SPI_CR1_BR_1,
  0
};

// initialize the mmc device over spi link 2
static const MMCConfig mmcConfig = {
  &SPID2,
  &lowSdcSpiConfig,
  &highSdcSpiConfig
};

void init_mmc(MMCDriver *mmcd) {
  palSetPadMode(GPIOB, 13, PAL_MODE_ALTERNATE(5));     // SCK. 
  palSetPadMode(GPIOB, 14, PAL_MODE_ALTERNATE(5));     // MISO.
  palSetPadMode(GPIOB, 15, PAL_MODE_ALTERNATE(5));     // MOSI.  

  // set the CS to push pull mode
  palSetPadMode(GPIOC, GPIOC_PIN2, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad(GPIOC, GPIOC_PIN2);

  // set the CD pin to "in" mode 
  palSetPadMode(GPIOD, 9, PAL_MODE_INPUT_PULLUP);
  palClearPad(GPIOD, GPIOD_PIN9);
 
  // set up the mmcObject and start it
  mmcObjectInit(mmcd);
  mmcStart(mmcd, &mmcConfig);
  chMtxObjectInit(&sdMutex);
}


// Lets configure our ADC first
static THD_WORKING_AREA(waShell,2048);

static thread_t *shelltp1;



// ADCConfig structure for stm32 MCUs is empty
static ADCConfig adccfg = {0};
#define UNUSED(x) (void)(x)

//defining the modes for run power.
// Create buffer to store ADC results. This is
// one-dimensional interleaved array
#define ADC_BUF_DEPTH 2 // depth of buffer
#define ADC_CH_NUM 2    // number of used ADC channels
static adcsample_t samples_buf[ADC_BUF_DEPTH * ADC_CH_NUM]; // results array

// Fill ADCConversionGroup structure fields
static const ADCConversionGroup adcgrpcfg = {
  FALSE,
  2,
  NULL,
  NULL,
  0,      /* CFGR */
  0,      /* TR1 */
  0,      /* CCR */
  /* SMPR1 */
  {ADC_SMPR1_SMP_AN1(ADC_SMPR_SMP_7P5) |
   ADC_SMPR1_SMP_AN2(ADC_SMPR_SMP_7P5),

   /* SMPR2 */
   0},

  /* SQR1 */
  {ADC_SQR1_NUM_CH(2) |
   ADC_SQR1_SQ1_N(ADC_CHANNEL_IN6) |
   ADC_SQR1_SQ2_N(ADC_CHANNEL_IN7),

   /* SQR2 */
   0,

   /* SQR3 */
   0,

   /* SQR4 */
   0}
};

// Thats all with configuration
/*
static void gpt_adc_trigger(void)  {
  //char float_array[32]; 
  // static unsigned short dac_val = 0;  
  //EvaluateNet(scale_input(samples_buf[0]));
     // dacConvertOne(&DACD1,scale_output(outputs[0]));

  // EvaluateNet(&ann, &inFifo, scale_input(samples_buf[0])); 
  // dacConvertOne(&DACD1,scale_output(outputs[0])); 


  // if (dac_val) { 
  //   dac_val = 0x000; 
  // } 
  // else { 
  //   dac_val = 0xFFF; 
  // } 
  // dacConvertOne(&DACD1,dac_val); 



  // if (outputs[0] > 0xFFF) { 
  //   outputs[0] = 0xFFF; 
  // } 

  //  dacConvertOne(&DACD1,outputs[0]);
  //  dacConvertOne(&DACD1,samples_buf[0]);
  //  dacConvertOne(&DACD1,0x3ff);
  adcStartConversion(&ADCD1, &adcgrpcfg, samples_buf, ADC_BUF_DEPTH);
 palTogglePad(GPIOE, GPIOE_LED4_BLUE);
}
*/


// Thread that blinks North LED as an "alive" indicator 
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

static int read_mode(void){
  int ones =  palReadPad(GPIOC, 6);
  int twos =  palReadPad(GPIOC, 7);
  int fours = palReadPad(GPIOD, 13);
  int eights = palReadPad(GPIOD, 14);
  int sixtn = palReadPad(GPIOD, 15);

  int total= ones + (twos<<1) + (fours<<2) + (eights<<3) + (sixtn<<4);
  
  return total;
}


static void collector(void) {
  //UNUSED(arg);
  palSetPad(GPIOC, 10);
  
  MODE = read_mode();
  potential_0 = samples_buf[0];
  potential_1 = samples_buf[1];
  diff = potential_0 - potential_1;

  if(arrayuse == 0){
    amperage1[arrays_spot] = diff;
    RUN_MODE1[arrays_spot] = MODE;
  }
  else{
    amperage2[arrays_spot] = diff;
    RUN_MODE2[arrays_spot] = MODE;
  }
  arrays_spot++;
  
  adcStartConversion(&ADCD1, &adcgrpcfg, samples_buf, ADC_BUF_DEPTH);

  if(arrays_spot == 500){
    if(arrayuse==1){
      palSetPad(GPIOE, GPIOE_LED4_BLUE);
    }
    else{
      palClearPad(GPIOE, GPIOE_LED4_BLUE);
    }
    arrayuse = !arrayuse;
    chBSemSignalI(&buff_sem);
    arrays_spot = 0;
  }
  palClearPad(GPIOC, 10);
//old code moved to different function

 /*
    for(j=0;j<100;j++){
      //data_collection_flag=0;

      MODE = read_mode();
      potential_0 = samples_buf[0];
      potential_1 = samples_buf[1];

      //d = (3.0/4096.0)* potential_0; //converting potential_0 (positive)
      //f = (3.0/4096.0)* potential_1; //converting potential_1 (ground)


      diff = potential_0 - potential_1; //difference between voltages
      
      if(arrayuse == 0){
        amperage1[j]=diff;
        RUN_MODE1[j]=MODE;//filling arrays
        arrayuse = 1;
      }
      else{
        amperage2[j]=diff;
        RUN_MODE2[j]=MODE;
        arrayuse = 0;
      }
      //diff = amperage1[j];
      //front = diff;
      //back = (diff-front)*10000000000;//printing shit yo
      
      chprintf((BaseSequentialStream*)&SD1, "Flag= %d\n\r", data_collection_flag);  
   
      // chprintf((BaseSequentialStream*)&SD1, "%d: %d.%d mA Mode is: %d \n\r", j, front, back, MODE);
      //chThdSleepMilliseconds(100);
  }*/
}

static GPTConfig gpt_adc_config = {
  1000000,         // timer clock: 1Mhz
  collector,
  //gpt_adc_trigger, // Timer callback function
  0,
  0
};

static THD_WORKING_AREA(waPrinterThread,4096);
static THD_FUNCTION(printerThread,arg) {
  UNUSED(arg);
  uint16_t* ampcurr;
  int* modecurr;
  /*
  uint16_t potential_0 = 0;
  uint16_t potential_1 = 0;
  uint16_t diff;
  */
  rc = f_mount(&SDC_FS, "", 0);

  if (rc) die(rc);

  while (TRUE) {
    chBSemWait(&buff_sem);

    if(arrayuse == 1){
      ampcurr = amperage1;
      modecurr = RUN_MODE1;
    } 
    else {
      ampcurr = amperage2;
      modecurr = RUN_MODE2;
    }

    chprintf((BaseSequentialStream*)&SD1, "CONVERT\n\r");

    char* tbuf = buf;

    int tbi;
    


    for (tbi = 0; tbi < 500; tbi++) {
      uint16_t* tbp = ampcurr + tbi;
      uint16_t x = *(tbp);
	    
      int* tmp= modecurr+tbi;
      int y = *(tmp);
      palSetPad(GPIOC, 11); 
      int len = sprintf(tbuf, "%04x %d\n", x, y);
      palClearPad(GPIOC, 11);
      chThdYield();
      tbuf += len;
    }
    

    chprintf((BaseSequentialStream*)&SD1, "LEN: %d\n", tbuf - buf);

    chprintf((BaseSequentialStream*)&SD1, "WRITE\n\r");


    palSetPad(GPIOC, 12);
    appendToCard(&fil, "data.txt", buf, tbuf - buf);
    palClearPad(GPIOC, 12);


    chprintf((BaseSequentialStream*)&SD1, "WRITE DONE\n\r");
  }
}
/*
static void cmd_read(BaseSequentialStream *chp, int argc, char *argv[]) {
  if(strcmp(argv[0],"start")==0){
    //start the reading
  }
  if(strcmp(argv[0], "stop")==0){
    //stop the reading
  }
}
*/
//static void cmd_dump(BaseSequentialStream *chp, int argc, char *argv[]) {

//}

static const ShellCommand commands[] = {
  //{"read". cmd_read},
  //{dump", cmd_dump},
  {NULL, NULL}

};

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


int main(void) {
  event_listener_t tel;
  chSysInit();
  halInit();
  chBSemObjectInit(&buff_sem, true);
  // Setup pins of our MCU as analog inputs
  palSetPadMode(GPIOC, 0, PAL_MODE_INPUT_ANALOG); // this is pc0 positive
  palSetPadMode(GPIOC, 1, PAL_MODE_INPUT_ANALOG); // this is pc1 negative

    //setting pins for reading power mode.
  palSetPadMode(GPIOC, 6, PAL_MODE_INPUT); // 1's   PC6 
  palSetPadMode(GPIOC, 7, PAL_MODE_INPUT); // 2's   PC7
  palSetPadMode(GPIOD, 13, PAL_MODE_INPUT);// 4's   PD13
  palSetPadMode(GPIOD, 14, PAL_MODE_INPUT);// 8's   PD14
  palSetPadMode(GPIOD, 15, PAL_MODE_INPUT);// 16's  PD15

  palSetPadMode(GPIOC, 10, PAL_MODE_OUTPUT_PUSHPULL);// 4's   PD13
  palSetPadMode(GPIOC, 11, PAL_MODE_OUTPUT_PUSHPULL);// 8's   PD14
  palSetPadMode(GPIOC, 12, PAL_MODE_OUTPUT_PUSHPULL);// 16's  PD15


  // Following 3 functions use previously created configuration
  // to initialize ADC block, start ADC block and start conversion.
  // &ADCD1 is pointer to ADC driver structure, defined in the depths of HAL.
  // Other arguments defined ourself earlier.
  //adcInit();

  adcStart(&ADCD1, &adccfg);
  adcStartConversion(&ADCD1, &adcgrpcfg, &samples_buf[0], ADC_BUF_DEPTH);


  sdStart(&SD1, NULL);
  palSetPadMode(GPIOC, 4, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOC, 5, PAL_MODE_ALTERNATE(7));

  initSDcard();

  shellInit();
  gptStart(&GPTD1, &gpt_adc_config);
  //gptStartContinuous(&GPTD1, 227);
  gptStartContinuous(&GPTD1,2000);

  //chEvtRegister(&shell_terminated, &tel, 0);
  // Thats all. Now your conversion run in background without assistance.
  shelltp1 = shellCreate(&shell_cfg1, sizeof(waShell), NORMALPRIO);
  chThdCreateStatic(waCounterThread, sizeof(waCounterThread), NORMALPRIO+1, counterThread, NULL);
  chThdCreateStatic(waPrinterThread, sizeof(waPrinterThread), NORMALPRIO+1, printerThread, NULL);
 // chThdCreateStatic(waCollectorThread, sizeof(waCollectorThread), NORMALPRIO+1, collectorThread, NULL);


  chprintf((BaseSequentialStream*)&SD1,"\n\r Up and running \n\r");
  while (TRUE) {
    chEvtDispatch(fhandlers, chEvtWaitOne(ALL_EVENTS));
  }
  return 0;
}
