README file for SPI loopback

Purpose- The purpose of the main code block is to demonstrate the functionality
of SPI loopback using DMA. There are two SPI channels (one master and one slave)
that communicate across DMA. Currently, it is just sending one uint8_t buffer
of 10 hex numbers. Find out how big the buffer can be still

Buffers- At lines 28 and 29 are the sending and receving buffers named 
accordingly. They can be increased in size to needed specifications. If you 
want to change the names of the buffers, be sure to update the addresses for 
the DMA exchage on lines 154 and 181

Clock initializations- clock_init initializes the clocks for GPIO, SPI and the 
DMA. These should not be altered. Only modify to add additional clocks for 
other GPIO devices

Pin layout- 
		MOSI | MISO | SCK | CS
		-----------------------
    SPI1- TX	PA7  | PA6  | PA5 | PE3 
    SPI2 -RX	PB15 | PB14 | PB13| software

Wire layout-
     connect PA5 and PB13 (clocks)
     connectt PA7 and PB15 (MOSI's)

When you configure the SPI2 as the slave, you want to hook up both MOSI's and 
not the MISO

DMA- SPI1 is the TX channel and SPI2 is the RX channel. Ensure the 
peripherial base address reflects this as well as the SPI_I2S_DMA 
commands. The channels for the DMA-SPI interface can be found
in the STM32 datasheet. 

Main loop- to start a transfer, reset both CS pins. The software CS pin is 
set/reset with the command:
	   SPI_NSSInternalSoftwareConfig(SPI2, SPI_NSSInternalSoft_Reset);
Once both of these are set, initialize the RX and then TX channels. Poll the
RX channel until completion and then set the CS pins again. After this the 
DMA channels can be turned off. The 'for' loop prints out the values of the RX
buffer and also slows down the DMA transfers. It makes using the Saleae easier
because it's not flooded with data

Testing- Testing the loopback is both easy and fun! The for loop at the bottom
of the main function prints the values out so you can see if the rxbuf and
txbuf are the same. It also turns on some of the LED lights in case you
don't want to run serialT.

Saleae instructions- 
       -Conncect PA7 and PB15 into the breadboard
       -Conncect the 0 (MOSI) wire from the saleae into the same line as those 2
       -(OPTIONAL) also connect wire 1 (MISO) into the same line
       -Connect 3 wire to PE3

Email any and all questions to cojthack@indiana.edu
Phone- 618-978-4185