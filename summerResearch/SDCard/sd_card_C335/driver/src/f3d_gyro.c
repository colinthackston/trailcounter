#include <f3d_gyro.h>
#include <stm32f30x.h>

void f3d_gyro_interface_init() {
  /**********************************************************************/
  /************** CODE HERE *********************************************/
  //You must configure and initialize the following 4 pins
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
 
  GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOA,&GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOE,&GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB,&GPIO_InitStructure);

  GPIO_PinAFConfig(GPIOA,5,GPIO_AF_5);
  GPIO_PinAFConfig(GPIOA,6,GPIO_AF_5);
  GPIO_PinAFConfig(GPIOA,7,GPIO_AF_5);



  GPIO_SetBits(GPIOE, GPIO_Pin_3);  //set the CS high
  
  /**********************************************************************/
   
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
  //SPI Initialization and configuration
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
  SPI_Init(SPI1, &SPI_InitStructure);
  SPI_RxFIFOThresholdConfig(SPI1, SPI_RxFIFOThreshold_QF);
  SPI_Cmd(SPI1, ENABLE);
}

//the init function to be called in your main.c
void f3d_gyro_init(void) {
  //
  //SETTING THE CONTROL REGISTERS
  f3d_gyro_interface_init();
  // CTRL1 Register
  // Bit 7:6 Data Rate: Datarate 0
  // Bit 5:4 Bandwidth: Bandwidth 3
  // Bit 3: Power Mode: Active
  // Bit 2:0 Axes Enable: X,Y,Z enabled
  uint8_t ctrl1;
  uint8_t ctrl2;
  uint8_t ctrl3;

  /* ctrl1 |= (uint8_t) (((uint8_t)0x00)|	
((uint8_t)0x30)|	
((uint8_t)0x08)|	
((uint8_t)0x07)); */
  // CTRL4 Register
  // Bit 7 Block Update: Continuous */
  // Bit 6 Endianess: LSB first */
  // Bit 5:4 Full Scale: 500 dps */
  /*ctrl4 |= (uint8_t) (((uint8_t)0x00)|	
((uint8_t)0x00)|	
((uint8_t)0x10));*/
  ctrl1 |= ((uint8_t) ((uint8_t) 0x01));

  ctrl2 |= ((uint8_t) ((uint8_t) 0x17)); //23 - 0x17

  ctrl3 |= ((uint8_t) ((uint8_t) 0x22)); //34 - 0x22

  accel_write_register(0x2A, &ctrl1, 1);
  accel_write_register(0x2C, &ctrl2, 1);
  accel_write_register(0x2D, &ctrl3, 1);


  uint8_t yoop = getID();

  //uint8_t hello;
  //f3d_gyro_read(&hello, 0x2A, 1);

  //accel_write_register(0x2A, 1); // INTMAP2 - Data Ready - 0000 0001
  //accel_write_register(0x2C, 23); // filter control - 2g - conservative filter - 400Hz -  0001 0111
  //accel_write_register(0x2C, 18); // filter control - 2g - conservative filter - 50Hz -  0001 0010
  // accel_write_register(0x2C, 19); // filter control - 2g - conservative filter - 100Hz -  0001 0011
  //accel_write_register(0x2D, 34);

}

/*sends the bytes*/
static uint8_t f3d_gyro_sendbyte(uint8_t byte) {
  /*********************************************************/
  /***********************CODE HERE ************************/
  
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET); 
  SPI_SendData8(SPI1, byte);
  
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
  return (uint8_t)SPI_ReceiveData8(SPI1);
  
  /********************** CODE HERE ************************/
  /*********************************************************/
}

void f3d_gyro_getdata(int16_t *pfData) {
  //Write: 0x0A
  //Read: 0x0B
  
  uint8_t xL = accel_read_register(0x0E, 1); 
  uint8_t xH = accel_read_register(0x0F, 1); 
  uint8_t yL = accel_read_register(0x10, 1);
  uint8_t yH = accel_read_register(0x11, 1);
  uint8_t zL = accel_read_register(0x12, 1);
  uint8_t zH = accel_read_register(0x13, 1);

  int16_t x;
  int16_t y;
  int16_t z;

  x = 0x0000 | xH;
  y = 0x0000 | yH;
  z = 0x0000 | zH;

  x <<= 8;
  y <<= 8;
  z <<= 8;
  
  x = x | xL;
  y = y | yL;
  z = z | zL;

  pfData[0] = x;
  pfData[1] = y;
  pfData[2] = z;
}

uint8_t getID(void) {
  //Write: 0x0A
  //Read: 0x0B
  return accel_read_register(0x00, 1);
}

uint8_t * accel_read_register(uint8_t address, uint8_t dataSize) {
  uint8_t cmd = 0x0B;
  uint8_t data[dataSize];
  uint8_t tempP = dataSize;
  GYRO_CS_LOW();
  //GYRO_READIN_LOW();
  while(tempP > 0x00) {
    f3d_gyro_sendbyte(cmd);
    f3d_gyro_sendbyte(address);
    data[dataSize - tempP] = f3d_gyro_sendbyte(0x00);
    tempP--;
  }
  //GYRO_READIN_HIGH();
  GYRO_CS_HIGH();
  return *data;
}

void accel_write_register(uint8_t address, uint8_t *data, uint8_t dataSize) {
  uint8_t cmd = 0x0A;
  uint8_t tempP = dataSize;
  GYRO_CS_LOW();
  //GYRO_READIN_LOW();
  while(tempP > 0x00) {
    f3d_gyro_sendbyte(cmd);
    f3d_gyro_sendbyte(address);
    f3d_gyro_sendbyte(data[dataSize-tempP]);
    tempP--;
  }
  //GYRO_READIN_HIGH();
  GYRO_CS_HIGH();
}