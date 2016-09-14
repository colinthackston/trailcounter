/* f3d_gyro.h */

/* Code: */
#include <stm32f30x.h>

#define L3G_Sensitivity_250dps   (float) 114.285f
#define L3G_Sensitivity_500dps   (float) 57.1429f
#define L3G_Sensitivity_2000dps   (float) 14.285f

#define GYRO_CS_LOW() GPIO_ResetBits(GPIOE, GPIO_Pin_3)
#define GYRO_CS_HIGH() GPIO_SetBits(GPIOE, GPIO_Pin_3)
#define GYRO_READIN_LOW() GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define GYRO_READIN_HIGH() GPIO_SetBits(GPIOB, GPIO_Pin_0)

void f3d_gyro_interface_init();
void f3d_gyro_init(void);
uint8_t getID(void);
static uint8_t f3d_gyro_sendbyte(uint8_t);
void f3d_gyro_getdata(int16_t *pfData);
void accel_write_register(uint8_t address, uint8_t *data, uint8_t dataSize);
uint8_t * accel_read_register(uint8_t address, uint8_t dataSize);

/* f3d_gyro.h ends here */
