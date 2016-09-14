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

/* Analog Input PA0 
   Analog Output PA4 
*/

#include "sdc_driver.h"
#include "nordic.h"
#include "transmit.h"
#include <stm32f30x_comp.h>
#include "shell.h"
#include "test.h"
#include "nrf24l01.h"
#include <chstreams.h>
#include "trail_counter_rtc_time.h"
#include "store_data.h"

// for RTC!
TrailRTC_t data_time;

// the FATFS global variable - we will use this for file transfer
FATFS SDC_FS;
FIL fil; /* a file object */

// some more stuff that is involved with the file system
FRESULT rc; /* Result code */
DIR dir; /* Directory object */
FILINFO fno; /* File information object */
UINT bw, br; /* bw = bytes written; br = bytes read */

// the mmc driver
MMCDriver MMCD1;

// some nordic stuff
#define UNUSED(x) (void)(x)
#define ERROR(chp, str) chprintf(chp, "Errored at lne %i in function %s. Error is: %s\r\n", __LINE__, __FUNCTION__, str);
#define BADINPUT(x) chprintf(chp, "Bad input. %s\r\n", x)

//static THD_WORKING_AREA(waShell,2048);

static thread_t *shelltp1;
// This is allocated in main

static THD_WORKING_AREA(recieverWorkingArea, 2048);

static msg_t receiverThread(void *arg) {
  UNUSED (arg);
  chRegSetThreadName("receiver");
  while(TRUE) {
    receive_thread_function();
    chSchDoYieldS();
  }
  return 0;
}

static void cmd_ch(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc == 0) print_ch();
  else if (argc == 1) {
    int chan = atoi(argv[0]);
    if (chan < 11 && chan > 0) set_ch(chan);
    else {
      BADINPUT("Channel must be between 1 and 10.");
      return;
    }
  } 
  else {
    BADINPUT("Invalid argument to ch");
    return;
  }
}

static void cmd_addr(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc == 0) print_addr();
  else if (argc == 1) {
    if (strlen((const char*)argv[0]) != 1) {
      BADINPUT("Address must be 1 character!");
      return;
    }
    else set_addr((uint8_t*) argv[0]);
  }
  else {
    BADINPUT("Too many arguments to addr!");
    return;
  }
}

char testBuff[20];
static void cmd_tx(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Invalid input to tx");
    return;
  }

  transmit_packet(*argv[0], (uint8_t *) argv[1], FALSE, FALSE);
}

static void cmd_tx_all(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Invalid input to tx");
    return;
  }
  if (beacon(*argv[0], (uint8_t *) argv[1])) {
    transmit_all(*argv[0], "tx.txt"); 
  } else {
    chprintf((BaseSequentialStream*)&SD1, "Beacon was not acked\n\r");
  }
}

static void cmd_beac(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Invalid input");
    return;
  }
  
  if (beacon(*argv[0], (uint8_t *) argv[1])) {
    chprintf((BaseSequentialStream*)&SD1, "Beacon was acked\n\r");
  } else {
    chprintf((BaseSequentialStream*)&SD1, "Beacon was not acked\n\r");
  }
}

static void cmd_eom(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Invalid input");
    return;
  }
  transmit_packet(*argv[0], (uint8_t *) argv[1], TRUE, FALSE);
}

static void cmd_store(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 1) {
    BADINPUT("Invalid input");
    return;
  }
  int peep_count = atoi(argv[0]);
  chprintf((BaseSequentialStream*)&SD1, "storing: %d\n\r", peep_count);
  store_data(peep_count);
}

static void cmd_settime(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 1) {
    BADINPUT("Invalid input");
    return;
  }
  time_t unix_time = (time_t) atoi(argv[0]);
  trailRtcSetFromEpoch(unix_time, &data_time);
  trailRtcPrintEpochTime((BaseSequentialStream*)&SD1, &data_time);
}

// ADCConfig structure for stm32 MCUs is empty
static ADCConfig adccfg = {0};

// Create buffer to store ADC results. This is
// one-dimensional interleaved array
#define ADC_BUF_DEPTH 1 // depth of buffer
#define ADC_CH_NUM 2    // number of used ADC channels
static adcsample_t samples_buf[ADC_BUF_DEPTH * ADC_CH_NUM]; // results array
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
   ADC_SQR1_SQ1_N(ADC_CHANNEL_IN1) |
   ADC_SQR1_SQ2_N(ADC_CHANNEL_IN2),
   
   /* SQR2 */
   0,
   /* SQR3 */
   0,
   /* SQR4 */
   0}
};

static void gpt_adc_trigger(GPTDriver *gpt_ptr) { 
  UNUSED(*gpt_ptr);
  dacConvertOne(&DACD1,samples_buf[0]);
  adcStartConversion(&ADCD1, &adcgrpcfg, samples_buf, ADC_BUF_DEPTH);
  palTogglePad(GPIOE, GPIOE_LED4_BLUE);
}
	
static GPTConfig gpt_adc_config = { 
  1000000,         // timer clock: 1Mhz 
  gpt_adc_trigger, // Timer callback function 
  0,
  0
};
	
	 
static THD_WORKING_AREA(waShell,2048);
static thread_t *shelltp1;
	  
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
  return 0;
}
			 
static const ShellCommand commands[] = {
  {"ch", cmd_ch},
  {"addr", cmd_addr},
  {"tx", cmd_tx},
  {"beac", cmd_beac},
  {"eom", cmd_eom},
  {"tx_all", cmd_tx_all},
  {"store", cmd_store},
  {"settime", cmd_settime},
  {NULL, NULL}
};
			  
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

static const DACConfig daccfg1 = {
  DAC_DHRM_12BIT_RIGHT,
  0
};

char buff[25];
char data_buff[25];
int unix_time = 1000000000;
int count = 5;
float data[3] = {3.1345, 345.91723, 18903.982734};

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
  driversInit();
  chSysInit();
  trailRtcInit(&data_time);
	  
  dacStart(&DACD1, &daccfg1);
	    
  //  arm_max_f32(myarray, 4, &max, &index); 
	    
  /*
   * Activates the serial driver 1 using the driver default configuration.
   * PC4(RX) and PC5(TX). The default baud rate is 9600.
   */
  sdStart(&SD1, NULL);
  palSetPadMode(GPIOC, 4, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOC, 5, PAL_MODE_ALTERNATE(7));
  adcStart(&ADCD1, &adccfg);
  
  // initialize the nordic
  initNordic();
  chprintf((BaseSequentialStream*)&SD1, "Initialized Nordic\n\r");
  print_ch();
  // let's initialize the mmc
  init_mmc(&MMCD1);
  chprintf((BaseSequentialStream*)&SD1, "Initialized mmc\n\r");

  if (cardInserted()) {
    chprintf((BaseSequentialStream*)&SD1, "A card is there \n\r");
    mmcConnect(&MMCD1);
  } else {
    chprintf((BaseSequentialStream*)&SD1, "No card to be found \n\r");
  }

  // let's see if we can write to the SD card
  rc = f_mount(&SDC_FS, "", 0);
  if (rc) die(rc);

  // MAAAAAXXXXX use this example on how to write to the SD card - pretty much 335 stuff
  // Steps that seem appropriate for your task
  // 1) make a data buff of characters
  // 2) sprintf(data_buff, "%d", data[0]) 
  // 3) appendToCard(&fil, "data.txt", data_buff, strlen(data_buff))
  // 4) repeat 2 and 3 for number of axes
  /* THIS CODE WORKS - if you wanna try this, name a file on your card tx.txt and uncomment this
  //*/
  int result;
  unixPersonCountConcat(buff, unix_time, count);
  result = appendToCard(&fil, "test.txt", buff, strlen(buff));
 
  if (result == 1) {
    chprintf((BaseSequentialStream*)&SD1, "wrote to sd card!\n\r");
    }
  
  /* Initialize the command shell */ 
  shellInit();
		    
  gptStart(&GPTD1, &gpt_adc_config);
		      
  // gptStartContinuous(&GPTD1, 227);
  gptStartContinuous(&GPTD1, 300);

  //Comparator_Config();

  /*
   *  setup to listen for the shell_terminated event. This setup will be stored in the tel  * event listner structure in item 0
   */
  chEvtRegister(&shell_terminated, &tel, 0);
  
  shelltp1 = shellCreate(&shell_cfg1, sizeof(waShell), NORMALPRIO);
  chThdCreateStatic(waCounterThread, sizeof(waCounterThread), NORMALPRIO+1, counterThread, NULL);
  // initialize the receiver thread
  chThdCreateStatic(recieverWorkingArea, sizeof(recieverWorkingArea), NORMALPRIO, receiverThread, NULL);
  while (TRUE) {
    chEvtDispatch(fhandlers, chEvtWaitOne(ALL_EVENTS));
  }
}
	
	
