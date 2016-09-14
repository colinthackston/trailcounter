
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
#include <trail_counter_rtc_time.h>
#include <sdc_driver.h>
#include <nordic.h>
#include <transmit.h>
#include <accelerometer_driver.h>
#include <stm32f30x_comp.h>
#include "shell.h"
#include "test.h"
#include "nrf24l01.h"
#include <chstreams.h>
#include "store_data.h"


#define UNUSED(x) (void)(x)
#define ERROR(chp, str) chprintf(chp, "Errored at lne %i in function %s. Error is: %s\r\n", __LINE__, __FUNCTION__, str);
#define BADINPUT(x) chprintf(chp, "Bad input. %s\r\n", x)

static THD_WORKING_AREA(waShell,2048);
static thread_t *shelltp1;

// Real Time Clock global variables, commands and threads
TrailRTC_t trail_rtc;

static void cmd_date(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  time_t unix_time;
  if (argc == 0) {
    goto ERROR;
  }
  if ((argc == 1) && (strcmp(argv[0], "get") == 0)){
    trailRtcUpdate(&trail_rtc);
    if (trail_rtc.epoch_time == -1) {
      chprintf(chp, "incorrect time in RTC cell\r\n");
    } else {
      trailRtcPrintEpochTime(chp, &trail_rtc);
      trailRtcPrintHumanReadable(chp, &trail_rtc);
      trailRtcPrintFATTime(chp, &trail_rtc);
    }
    return;
  }
  if ((argc == 2) && (strcmp(argv[0], "set") == 0)) {
    unix_time = atol(argv[1]);
    if (unix_time > 0) {
      trailRtcSetFromEpoch(unix_time, &trail_rtc);
      trailRtcPrintEpochTime((BaseSequentialStream*)&SD1, &trail_rtc);
      return;
    } else {
      goto ERROR;
    }
  } else {
    goto ERROR;
  }
ERROR:
  chprintf(chp, "Usage: date get\r\n");
  chprintf(chp, " date set N\r\n");
  chprintf(chp, "where N is time in seconds since Unix epoch\r\n");
  chprintf(chp, "you can get current N value from unix console by the command\r\n");
  chprintf(chp, "%s", "date +\%s\r\n");
  return;
}

// some global variables that will need to be set for the people counter
// thread
//Accelerometer
int people_count = 0;
//uint16_t data[3] = {0, 0, 0};
int16_t data[3] = {0, 0, 0};
//int16_t timedata[1000][3];
//uint8_t statusdata[3000];
int16_t walking[1000][2];
int state = 0, count = 0, scount = 0;
int people_flag = 0;
int data_ready = 0;
int j = 0, k=0;
uint8_t reg27 = 0;
char buffer[3000];
time_t current_time = 0;
time_t last_detection = 0; // start at 4 for the first time, will change after first person is detected

// Thread that counts people
static THD_WORKING_AREA(waReadyThready,128); //just cuz
static THD_FUNCTION(readyThready,arg) {
  (void)arg;
  while(TRUE) {
    uint8_t reg27 = 0;
    reg27 = readByteI2C(0x19, 0x27);

    if (1 & (reg27 >> 3)) {
      accelerometer_read(data);
      int16_t x = data[0];
      int16_t y = data[1];
      int16_t z = data[2];
      int16_t mag = (abs(x))+(abs(y))+(abs(z));
      
      if((mag < 800) || (mag > 1100)){
	people_flag = 1;
      } else {
	people_flag = 0;
      }

      if(people_flag){
	// up the people count and save both the count and the magnitude
	
	people_count++;
	walking[j][0] = people_count;
	walking[j][1] = mag;
	j++;
      }
    }

    // sleep for a bit - just to see what happens - Jason
    chThdSleepMilliseconds(100);
  }
}


static THD_WORKING_AREA(beaconWa, 128);
static THD_FUNCTION(beaconThread, arg){
  (void)arg;
  while (TRUE) {
    // this updates the current time rtc
    trailRtcUpdate(&trail_rtc);
    current_time = trail_rtc.epoch_time;

    // this attempts to beacon
    char test[4] = "test";
    if(people_flag && current_time - last_detection > 3) {
      // update last_detection
      chprintf((BaseSequentialStream*)&SD1, "one more person - time: %d\n\r", current_time - last_detection);
      trailRtcUpdate(&trail_rtc);
      last_detection = trail_rtc.epoch_time;
      
      // if we successfully beacon, transmit all of the data
      if (transmit_packet((uint8_t) 'A', (uint8_t *) test, strlen((char*) test), FALSE, TRUE)) {
	transmit_all((uint8_t) 'A', "text.txt");
      }
    }
    
    // take a little nap
    chThdSleepMilliseconds(100);
  }
  return 0;
}
 
// blinks to indicate that the RTC stuff has been initialized
static THD_WORKING_AREA(blinkWA, 128);
static THD_FUNCTION(blink_thd, arg){
  (void)arg;
  while (TRUE) {
    chThdSleepMilliseconds(100);
    palTogglePad(GPIOE, GPIOE_LED4_BLUE);
  }
  return 0;
}

static THD_WORKING_AREA(receiverWorkingArea, 2048);
static msg_t receiverThread(void *arg) {
  UNUSED (arg);
  chRegSetThreadName("receiver");
  while(TRUE) {
    receive_thread_function();
    chSchDoYieldS();
  }
  return 0;
}

// shell commands from the data team
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

static void cmd_tx(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Invalid input to tx");
    return;
  }
  transmit_packet(*argv[0], (uint8_t *) argv[1], strlen((char*) argv[1]), FALSE, FALSE);
}
static void cmd_tx_all(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Please provide a site and a beacon message.");
    return;
  }
  if (transmit_packet(*argv[0], (uint8_t *) argv[1], strlen((char*) argv[1]), FALSE, TRUE)) {
  //  if (beacon(*argv[0], (uint8_t *) argv[1], strlen((char*) argv[1]))) {
    transmit_all(*argv[0], "text.txt");
  }
}
static void cmd_beac(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Invalid input");
    return;
  }
  
  transmit_packet(*argv[0], (uint8_t *) argv[1], strlen((char*) argv[1]), FALSE, TRUE);
}
static void cmd_eom(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 2) {
    BADINPUT("Invalid input");
    return;
  }
  transmit_packet(*argv[0], (uint8_t *) argv[1], strlen((char*) argv[1]), TRUE, FALSE);
}
static void cmd_store(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 1) {
    BADINPUT("Invalid input");
    return;
  }
  int peep_count = atoi(argv[0]);
  chprintf((BaseSequentialStream*)&SD1, "storing: %d\n\r", peep_count);
  store_data(&peep_count);
}

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
  {"date", cmd_date},
  {"store", cmd_store},
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

void serialInit(void) {
 /*
   * Activates the serial driver 1 using the driver default configuration.
   * PC4(RX) and PC5(TX). The default baud rate is 9600.
   */
  
  palSetPadMode(GPIOC, 4, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOC, 5, PAL_MODE_ALTERNATE(7));
}

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
  sdStart(&SD1, NULL);
  serialInit();
  
  /* Initialize the command shell */ 
  shellInit();

  chprintf((BaseSequentialStream*)&SD1, "HERE\n\r");

  // initialize nordic
  initNordic();
  chprintf((BaseSequentialStream*)&SD1, "Initialized Nordic\n\r");

  //initialize the rtc
  trailRtcInit(&trail_rtc);
  chprintf((BaseSequentialStream*)&SD1, "Initialized RTC\n\r");
  
  // initialize the alarm system
  //trailRtcInitAlarmSystem();  
  //SET_ALARM;
  //chprintf((BaseSequentialStream*)&SD1, "Initialized RTC alarms\n\r");

  // initialize the sd card
  initSDcard();
  
  // initialize the i2c
  i2c_init();
  chprintf((BaseSequentialStream*)&SD1, "i2c initialized \n\r");
  
  // initialize the accelerometer
  accelerometer_init();
  chprintf((BaseSequentialStream*)&SD1, "Accelerometer initialized \n\r");
  
  /* setup to listen for the shell_terminated event. This setup will be stored in the tel  * event listner structure in item 0 */
  chEvtRegister(&shell_terminated, &tel, 0);
  
  shelltp1 = shellCreate(&shell_cfg1, sizeof(waShell), NORMALPRIO);
  chThdCreateStatic(waCounterThread, sizeof(waCounterThread), NORMALPRIO, counterThread, NULL);
  
  // initialize the blinker thread
  chThdCreateStatic(blinkWA, sizeof(blinkWA), NORMALPRIO, blink_thd, NULL);

  // initialize the receiver thread
  chThdCreateStatic(receiverWorkingArea, sizeof(receiverWorkingArea), NORMALPRIO, receiverThread, NULL);
  
  // initialize the accelerometer thread
  chThdCreateStatic(waReadyThready, sizeof(waReadyThready), NORMALPRIO, readyThready, NULL);

  // create the beacon thread
  chThdCreateStatic(beaconWa, sizeof(beaconWa), NORMALPRIO, beaconThread, NULL);

  while (TRUE) {
    chEvtDispatch(fhandlers, chEvtWaitOne(ALL_EVENTS));
    
  }

  return 0;
}
