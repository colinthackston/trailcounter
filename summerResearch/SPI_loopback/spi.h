#include <stm32f30x.h>
#include <stm32f30x_spi.h>
#include <stm32f30x_rcc.h>
#include <stm32f30x_gpio.h>
#include <glcdfont.h>

#ifndef SPI_H
#define SPI_H

enum spiSpeed { SPI_SLOW , SPI_MEDIUM, SPI_FAST };

void spiInit(SPI_TypeDef* SPIx);

static int xchng_datablock(SPI_TypeDef *SPIx, int half, const void *tbuf, void *rbuf, unsigned count);

int spiReadWrite16(SPI_TypeDef *SPIx, uint16_t *rbuf, 
		   const uint16_t *tbuf, int cnt, uint16_t speed);

int spiReadWrite16(SPI_TypeDef *SPIx, uint16_t *rbuf, const uint16_t *tbuf, int cnt, uint16_t speed);

#endif

