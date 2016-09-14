/* f3d_lcd_sd.c ---
*
* Filename: f3d_lcd_sd.c
* Description:
* Author: Bryce Himebaugh


* Maintainer: Gleb Alexeev | galexeev, Yangyang Zhang | zhang299
* Created: Thu Oct 24 05:18:36 2013
* Last edited: Friday, February 27th, 2015

/* Copyright (c) 2004-2007 The Trustees of Indiana University and
* Indiana University Research and Technology Corporation.
*
* All rights reserved.
*
* Additional copyrights may follow
*/

/* Code: */
#include <f3d_lcd_sd.h>
#include <f3d_delay.h>

void f3d_lcd_sd_interface_init(void) {
 /* vvvvvvvvvvv pin initialization for the LCD goes here vvvvvvvvvv*/
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
  
  GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15 ;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOB,13,GPIO_AF_5);
  GPIO_PinAFConfig(GPIOB,14,GPIO_AF_5);
  GPIO_PinAFConfig(GPIOB,15,GPIO_AF_5);

  //SD Card Init

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(GPIOD, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  
  GPIO_ResetBits(GPIOB, 8);
  
  
  /* ^^^^^^^^^^^ pin initialization for the LCD goes here ^^^^^^^^^^ */
 
  // Section 4.1 SPI2 configuration
  // Note: you will need to add some code in the last three functions
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2 , ENABLE);
  SPI_InitTypeDef SPI_InitStructure;
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_Init(SPI2, &SPI_InitStructure);
  SPI_RxFIFOThresholdConfig(SPI2, SPI_RxFIFOThreshold_QF);
  SPI_Cmd(SPI2, ENABLE);
}

void f3d_lcd_init(void) {
  const struct lcd_cmdBuf *cmd;

  f3d_lcd_sd_interface_init(); // Setup SPI2 Link and configure GPIO pins
}

int spiReadWrite(SPI_TypeDef *SPIx,uint8_t *rbuf, const uint8_t *tbuf, int cnt, uint16_t speed) {
  int i;
  int timeout;

  SPIx->CR1 = (SPIx->CR1&~SPI_BaudRatePrescaler_256)|speed;
  for (i = 0; i < cnt; i++){
    if (tbuf) {
      SPI_SendData8(SPIx,*tbuf++);
    }
    else {
      SPI_SendData8(SPIx,0xff);
    }
    timeout = 100;
    while (SPI_I2S_GetFlagStatus(SPIx,SPI_I2S_FLAG_RXNE) == RESET);
    if (rbuf) {
      *rbuf++ = SPI_ReceiveData8(SPIx);
    }
    else {
      SPI_ReceiveData8(SPIx);
    }
  }
  return i;
}

int spiReadWrite16(SPI_TypeDef *SPIx,uint8_t *rbuf, const uint16_t *tbuf, int cnt, uint16_t speed) {
  int i;
  
  SPIx->CR1 = (SPIx->CR1&~SPI_BaudRatePrescaler_256)|speed;
  SPI_DataSizeConfig(SPIx, SPI_DataSize_16b);

  for (i = 0; i < cnt; i++){
    if (tbuf) {
      // printf("data=0x%4x\n\r",*tbuf);
      SPI_I2S_SendData16(SPIx,*tbuf++);
    }
    else {
      SPI_I2S_SendData16(SPIx,0xffff);
    }
    while (SPI_I2S_GetFlagStatus(SPIx,SPI_I2S_FLAG_RXNE) == RESET);
    if (rbuf) {
      *rbuf++ = SPI_I2S_ReceiveData16(SPIx);
    }
    else {
      SPI_I2S_ReceiveData16(SPIx);
    }
  }
  SPI_DataSizeConfig(SPIx, SPI_DataSize_8b);

  return i;
}