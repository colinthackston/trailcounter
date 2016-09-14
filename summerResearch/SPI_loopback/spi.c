#include <stm32f30x.h>
#include <stm32f30x_gpio.h>
#include <stm32f30x_rcc.h>
#include <stm32f30x_spi.h>
#include <glcdfont.h>
#include "spi.h"
#include <f3d_led.h>

static const uint16_t speeds[] = {
  [SPI_SLOW] = SPI_BaudRatePrescaler_64,
  [SPI_MEDIUM] = SPI_BaudRatePrescaler_8,
  [SPI_FAST] = SPI_BaudRatePrescaler_2};

void spiInit(SPI_TypeDef *SPIx)
{
  f3d_led_init();
  f3d_led_all_off();
  
  SPI_InitTypeDef SPI_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;

  GPIO_StructInit(&GPIO_InitStructure);
  SPI_StructInit(&SPI_InitStructure);

  if (SPIx == SPI2) { //SPI2 channel 
    f3d_led_on(5);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2 , ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB,&GPIO_InitStructure);
    
    GPIO_PinAFConfig(GPIOB,13,GPIO_AF_5);
    GPIO_PinAFConfig(GPIOB,14,GPIO_AF_5);
    GPIO_PinAFConfig(GPIOB,15,GPIO_AF_5);
    
  } 
  if (SPIx == SPI1) {  //  SPI1 channel
    // f3d_led_on(8);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA,&GPIO_InitStructure);     

    GPIO_PinAFConfig(GPIOA,5,GPIO_AF_5);
    GPIO_PinAFConfig(GPIOA,6,GPIO_AF_5);
    GPIO_PinAFConfig(GPIOA,7,GPIO_AF_5);
  }
  else{
    //f3d_led_on(7);
  }
  
  //**************************************//
  //************CS pin PE3***************//
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE);
  GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOE,&GPIO_InitStructure);

  GPIO_SetBits(GPIOE, GPIO_Pin_3);
  //*************************************//
  
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low; 
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = speeds[SPI_SLOW];
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(SPIx, &SPI_InitStructure);

  SPI_Cmd(SPIx, ENABLE);   
}

static int xchng_datablock(SPI_TypeDef *SPIx, int half, const void *tbuf, void *rbuf, unsigned count) {
  DMA_InitTypeDef DMA_InitStructure;
  uint16_t dummy[] = {0xffff};
  
  DMA_Channel_TypeDef *rxChan;
  DMA_Channel_TypeDef *txChan;
  uint32_t dmaflag;
  
  if (count & 1)
    return -1;
  
  if (SPIx == SPI1) {
    rxChan = DMA1_Channel2;
    txChan = DMA1_Channel3;
    dmaflag = DMA1_FLAG_TC2;
  }
  else if (SPIx == SPI2) {
    rxChan = DMA1_Channel4;
    txChan = DMA1_Channel5;
    dmaflag = DMA1_FLAG_TC4;
  }
  else
    return -1;
  
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(SPIx->DR));
  if (half) {
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
  }
  else {
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
  }
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_BufferSize = count;
  DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
  DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
  
  DMA_DeInit(rxChan);
  DMA_DeInit(txChan);
  
  if (rbuf) {
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)rbuf;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  }
  else {
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t) dummy;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
  }
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
  DMA_Init(rxChan, &DMA_InitStructure);

  if (tbuf) {
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)tbuf;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  }
  else {
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t) dummy;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
  }
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
  DMA_Init(txChan, &DMA_InitStructure);

  // Enable channels
  DMA_Cmd(rxChan, ENABLE);
  DMA_Cmd(txChan, ENABLE);

  // Enable SPI TX/RX request
  SPI_I2S_DMACmd(SPIx, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, ENABLE);

  // Wait for completion
  while (DMA_GetFlagStatus(dmaflag) == RESET) { ; }

  // Disable channels
  DMA_Cmd(rxChan, DISABLE);
  DMA_Cmd(txChan, DISABLE);
  SPI_I2S_DMACmd(SPIx, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, DISABLE);
  return count;
}

int spiReadWrite(SPI_TypeDef *SPIx, uint8_t *rbuf, 
     const uint8_t *tbuf, int cnt, uint16_t speed) {
  int i;
  SPIx->CR1 = (SPIx->CR1 & ~SPI_BaudRatePrescaler_256) | speed;

  if ((cnt > 4) && !(cnt & 1)) {
    return xchng_datablock(SPIx, 0, tbuf, rbuf , cnt);
  }
  else {
    for (i = 0; i < cnt; i++){
      SPI_SendData8(SPIx, tbuf ? *tbuf++ : 0xff);
      while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
      if (rbuf) {
    *rbuf++ = SPI_ReceiveData8(SPIx);
      } else  {
    SPI_ReceiveData8(SPIx);
      }
    }
    return i;
  }
}

int spiReadWrite16(SPI_TypeDef *SPIx, uint16_t *rbuf, 
           const uint16_t *tbuf, int cnt, uint16_t speed) {
  int i;
  SPIx->CR1 = (SPIx->CR1 & ~SPI_BaudRatePrescaler_256) | speed;
  SPI_DataSizeConfig(SPIx, SPI_DataSize_16b);
  if ((cnt > 4) && !(cnt & 3)) {
    i =  xchng_datablock(SPIx, 1, tbuf, rbuf , cnt);
  }
  else {
    for (i = 0; i < cnt; i++){
      SPI_I2S_SendData16(SPIx, tbuf ? *tbuf++ : 0xffff);
      while (SPI_I2S_GetFlagStatus(SPIx, SPI_I2S_FLAG_RXNE) == RESET);
      if (rbuf) {
    *rbuf++ = SPI_I2S_ReceiveData16(SPIx);
      } else {
    SPI_I2S_ReceiveData16(SPIx);
      }
    }
  }
  SPI_DataSizeConfig(SPIx, SPI_DataSize_8b);
  return i;
}
