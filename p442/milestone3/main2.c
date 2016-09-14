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
#include <stdlib.h>
#include <stdarg.h>
 
#define PWR_OFFSET               (PWR_BASE - PERIPH_BASE)

/* --- CR Register ---*/

/* Alias word address of DBP bit */
#define CR_OFFSET                (PWR_OFFSET + 0x00)
#define DBP_BitNumber            0x08
#define CR_DBP_BB                (PERIPH_BB_BASE + (CR_OFFSET * 32) + (DBP_BitNumber * 4))

/* Alias word address of PVDE bit */
#define PVDE_BitNumber           0x04
#define CR_PVDE_BB               (PERIPH_BB_BASE + (CR_OFFSET * 32) + (PVDE_BitNumber * 4))

#define MODE_STANDBY (palClearPad(GPIOD, 14))&&(palClearPad(GPIOD, 15));
#define MODE_STOP    (PalSetPad(GPIOD, 14))&&(palClearPad(GPIOD, 15));
#define MODE_SLEEP   (PalClearPad(GPIOD, 14))&&(palSetPad(GPIOD, 15));
#define MODE_RUNNING (PalSetPad(GPIOD, 14))&&(palSetPad(GPIOD, 15));
/* ------------------ PWR registers bit mask ------------------------ */

/* CR register bit mask */
#define CR_DS_MASK               ((uint32_t)0xFFFFFFFC)
#define CR_PLS_MASK              ((uint32_t)0xFFFFFF1F)

void modeswitch(int mode){
  if(mode == 0){
    palClearPad(GPIOD, 14);
    palClearPad(GPIOD, 15);
  }
  else if(mode == 1){
    palClearPad(GPIOD, 14);
    palSetPad(GPIOD, 15);
  }
}

void PWR_EnterSleepMode(uint8_t PWR_SLEEPEntry)
{
  /* Check the parameters */
  /* assert_param(IS_PWR_SLEEP_ENTRY(PWR_SLEEPEntry)); */
  
  /* Clear SLEEPDEEP bit of Cortex System Control Register */
  SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP_Msk);

  __WFI();
}


void PWR_EnterSTOPMode(uint32_t PWR_Regulator)
{
  uint32_t tmpreg = 0;
  
  /* Check the parameters */
  /* assert_param(IS_PWR_REGULATOR(PWR_Regulator)); */
  /* assert_param(IS_PWR_STOP_ENTRY(PWR_STOPEntry)); */
  
  /* Select the regulator state in STOP mode ---------------------------------*/
  tmpreg = PWR->CR;
  /* Clear PDDS and LPDSR bits */
  tmpreg &= CR_DS_MASK;
  
  /* Set LPDSR bit according to PWR_Regulator value */
  tmpreg |= PWR_Regulator;
  
  /* Store the new value */
  PWR->CR = tmpreg;
  
  /* Set SLEEPDEEP bit of Cortex System Control Register */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  
  __WFI();
  /* Reset SLEEPDEEP bit of Cortex System Control Register */
  SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP_Msk);  
}

void PWR_EnterSTANDBYMode(void)
{
  /* Clear Wakeup flag */
  PWR->CR |= PWR_CR_CWUF;
  
  /* Select STANDBY mode */
  PWR->CR |= PWR_CR_PDDS;
  
  /* Set SLEEPDEEP bit of Cortex System Control Register */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  
/* This option is used to ensure that store operations are completed */
#if defined ( __CC_ARM   )
  __force_stores();
#endif
  /* Request Wait For Interrupt */
  __WFI();
}


int main(void) {
  MODE_RUNNING;
  // event_listener_t tel;
  palSetPadMode(GPIOD, 14, PAL_MODE_INPUT_ANALOG);//seeting pins for reading power mode. 
  palSetPadMode(GPIOD, 15, PAL_MODE_INPUT_ANALOG);
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  //Commands used to force the board to enter sleep, stop, and standby modes. Code used to set it into a run state 
  //was simply the code for lab2. 
  halInit();
  chSysInit();
  
  //PWR_EnterSLEEPMode(0);
  //PWR_EnterSTOPMode(0);
  //MODE_STANDBY;
  //PWR_EnterSTANDBYMode();
  while(1){}
 }

