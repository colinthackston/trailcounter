/*********************************
Title- SPI loopback test
Author- Colin Thackston
Created on- 5/14/16
Last modified by- Colin Thackston
Last modified on- 6/1/16 14:38

Change log- changed DMA channel, added while loop with status flag
to poll until the transfer finished, everything working

*/


#include <stm32f30x.h>
#include <stm32f30x_spi.h>
#include <stm32f30x_rcc.h>
#include <stm32f30x_gpio.h>
#include <f3d_led.h>
#include <f3d_uart.h>
#include <stdio.h>
#include <f3d_rtc.h>


#define SPI_SLOW (uint16_t) SPI_BaudRatePrescaler_64
#define SPI_MEDIUM (uint16_t) SPI_BaudRatePrescaler_8
#define SPI_FAST (uint16_t) SPI_BaudRatePrescaler_2

uint8_t spiTxBuff[10] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x10};
uint8_t spiRxBuff[10] = {0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F};

//initializes clocks for GPIO, SPI, and DMA
void clock_init(){
  //GPIO clocks 
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE);

  //SPI clocks
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE); //SPI1
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);//SPI2

  //DMA clock 
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
}

//initialze pints 
void pin_init(){

  GPIO_InitTypeDef GPIO_InitStructure;
  
  //blink north LED
  GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  GPIO_Init(GPIOE,&GPIO_InitStructure);

  //**************SPI1 pin init (tx master)*****************
  //CS PE3
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOE,&GPIO_InitStructure);

  GPIO_SetBits(GPIOE, GPIO_Pin_3); //set CS pin high

  //SCK PA5, MOSI PA7
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOA,&GPIO_InitStructure);

  //alt functions
  GPIO_PinAFConfig(GPIOA,5,GPIO_AF_5);
  GPIO_PinAFConfig(GPIOA,7,GPIO_AF_5);

  
  //****************SPI2 pin init (rx slave)******************************

  //RX SPI2 (soft nss)
  //SCK PB13
  //MOSI PB15
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
  GPIO_Init(GPIOB,&GPIO_InitStructure);


  //alt functions
  GPIO_PinAFConfig(GPIOB,13,GPIO_AF_5);
  GPIO_PinAFConfig(GPIOB,15,GPIO_AF_5);

}

void spi_init(){
  //*********************SPI1 init (master) ************************

  SPI_InitTypeDef SPI_InitStructure;
  SPI_StructInit(&SPI_InitStructure);

  SPI_I2S_DeInit(SPI1);
  SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;

  SPI_Init(SPI1, &SPI_InitStructure);
  SPI_RxFIFOThresholdConfig(SPI1, SPI_RxFIFOThreshold_QF);
  SPI_Cmd(SPI1, ENABLE);

  //*******************SPI2 init (slave)*****************
  
  SPI_I2S_DeInit(SPI2);
  
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_RxOnly;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Slave;
  
  SPI_Init(SPI2, &SPI_InitStructure);
  SPI_RxFIFOThresholdConfig(SPI2, SPI_RxFIFOThreshold_QF);
  SPI_Cmd(SPI2, ENABLE);

  //****************************************************

}

//SPI -> memory
//RX using SPI2
void spi_rxbuff(void *rxbuff, unsigned count){
  DMA_InitTypeDef DMA_InitStructure;   
 

  DMA_Channel_TypeDef *rxChan = DMA1_Channel4;
  const uint32_t dmaflag = DMA1_FLAG_TC2;
  DMA_DeInit(rxChan);
  DMA_StructInit(&DMA_InitStructure);

  DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) (&(SPI2->DR)); //address
  DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_PeripheralInc  = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryDataSize  = DMA_MemoryDataSize_Byte;
  DMA_InitStructure.DMA_BufferSize  = count; //buffer size
  DMA_InitStructure.DMA_Mode  = DMA_Mode_Normal; //not set to circular
  DMA_InitStructure.DMA_Priority  = DMA_Priority_High;
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
  DMA_InitStructure.DMA_MemoryBaseAddr  =  rxbuff; //where we're receiving 
  DMA_InitStructure.DMA_MemoryInc  = DMA_MemoryInc_Enable;

  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC; //receiving 
  DMA_Init(rxChan, &DMA_InitStructure); 
  DMA_Cmd(rxChan, ENABLE);
  SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, ENABLE);  
}

//memory -> SPI
//TX using SPI1
void spi_txbuff(void *txbuff, unsigned count){
  DMA_InitTypeDef DMA_InitStructure;   
 
  DMA_Channel_TypeDef *txChan = DMA1_Channel3;
  const uint32_t dmaflag = DMA1_FLAG_TC2;
  DMA_DeInit(txChan);
  DMA_StructInit(&DMA_InitStructure);

  DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) (&(SPI1->DR)); //address
  DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_PeripheralInc  = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryDataSize  = DMA_MemoryDataSize_Byte;
  DMA_InitStructure.DMA_BufferSize  = count; //buffersize
  DMA_InitStructure.DMA_Mode  = DMA_Mode_Normal; //not set to circular
  DMA_InitStructure.DMA_Priority  = DMA_Priority_High;
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
  DMA_InitStructure.DMA_MemoryBaseAddr = txbuff; //what we're sending
  DMA_InitStructure.DMA_MemoryInc  = DMA_MemoryInc_Enable;
 
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST; //for sending
  DMA_Init(txChan, &DMA_InitStructure); 
  DMA_Cmd(txChan, ENABLE);
  SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);  
  
}

//main program loop
void main ()
{
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  
  int i =0, j=0;
  f3d_led_init();
  f3d_led_all_off();
  f3d_rtc_init();  
  clock_init();
  pin_init();  
  spi_init();
  f3d_uart_init();

  //actual SPI transfer using DMA
  while(1){
    f3d_led_all_off();
    GPIO_ResetBits(GPIOE, GPIO_Pin_3); //SET CS SPI1
    SPI_NSSInternalSoftwareConfig(SPI2, SPI_NSSInternalSoft_Reset); //CS SPI2 (software)
    
    spi_rxbuff(spiRxBuff, sizeof(spiRxBuff)); //start RX
    
    spi_txbuff(spiTxBuff, sizeof(spiTxBuff)); //start TX
    
    while (DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET) { ; } //wait until transfer is done
    

    SPI_NSSInternalSoftwareConfig(SPI2, SPI_NSSInternalSoft_Set); //CS SPI2 (software)
    GPIO_SetBits(GPIOE, GPIO_Pin_3); //reset CS
    
    DMA_DeInit(DMA1_Channel4); //Deinit DMA
    DMA_DeInit(DMA1_Channel3);
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Rx, DISABLE);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, DISABLE);
    
    int meep;
    for(meep=0;meep<10;meep++){ //checking if the RX buffer has changed 
      printf("%d, %x\n", meep, spiRxBuff[meep]);
      if(spiRxBuff[meep]==spiTxBuff[meep]){
	f3d_led_on(meep+2);
      }
    }
  }
}


#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {
/* Infinite loop */
/* Use GDB to find out why we're here */
  while (1);
}
#endif
