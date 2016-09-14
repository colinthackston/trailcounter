
#include <nrf24l01.h>
#include <nordic.h>
#include <transmit.h>
#include <sdc_driver.h>

#define LISTENING_FOR_BEACON 1
#define RECEIVING_DATA 0

static int state = LISTENING_FOR_BEACON;

static NRF24L01Driver nrf24l01;
static mutex_t nrfMutex;
static uint8_t my_addr[5] = "SITEA";
static uint8_t unique_identifier = ME;

static const SPIConfig nrf24l01SPI = {
  NULL,
  GPIOC,
  GPIOC_PIN1,
  SPI_CR1_BR_2|SPI_CR1_BR_1|SPI_CR1_BR_0,
  0
};

static const NRF24L01Config nrf24l01Config = {
  &SPID3,
  GPIOC,
  GPIOC_PIN2
};

static void nrfExtCallback(EXTDriver *extp, expchannel_t channel);
static const EXTConfig extcfg = {
  {
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_FALLING_EDGE | EXT_CH_MODE_AUTOSTART | EXT_MODE_GPIOC, nrfExtCallback},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL}
  }
};

static void nrfExtCallback(EXTDriver *extp, expchannel_t channel) {
  UNUSED(extp);
  UNUSED(channel);
  nrf24l01ExtIRQ(&nrf24l01);
}

void initNRF24L01(NRF24L01Driver *nrfp) {
  nrf24l01EnableDynamicSize(nrfp);
  nrf24l01EnableDynamicPipeSize(nrfp, 0x3f);

  nrf24l01SetTXAddress(nrfp, my_addr);
  nrf24l01SetRXAddress(nrfp, 0, my_addr);
  nrf24l01SetPayloadSize(nrfp, 0, 32);
  nrf24l01SetChannel(nrfp, 3);

  nrf24l01FlushRX(nrfp);
  nrf24l01FlushTX(nrfp);
  nrf24l01ClearIRQ(nrfp, NRF24L01_RX_DR | NRF24L01_TX_DS | NRF24L01_MAX_RT);

  nrf24l01PowerUp(nrfp);
}

void receive_thread_function() {
  static int alt_bit = 0;
  uint8_t serialInBuf[32];
  uint8_t serialOutBuf[32];

  // memset(serialInBuf, 0, 32);
  // Grab the mutex and read from the channel. Timeout if we don't
  // receive anything within 10 milliseconds. After reading release the
  // mutex.
  chMtxLock(&nrfMutex);
  size_t s = chnReadTimeout(&nrf24l01.channels[0], serialInBuf, 32, MS2ST(10));
  chMtxUnlock(&nrfMutex);
  
  if (s) { // Read something
    if (state == LISTENING_FOR_BEACON) {
      if (!is_beacon(serialInBuf)) return;
      if (is_ack(serialInBuf)) return;
      alt_bit = get_abp(serialInBuf);
      chprintf((BaseSequentialStream*)&SD1, "Beacon received. Payload: %s\n\r", serialInBuf + 2);
      state = RECEIVING_DATA;
    } else if (state == RECEIVING_DATA) {
      if (is_beacon(serialInBuf)) return;
    }

    strncpy((char*)serialOutBuf, (char*)serialInBuf, 32); // just send back the same packet as an ack
    set_ack(); // except set message type to ack
      
    uint8_t temp_addr[5] = "SITEA";
    temp_addr[4] = get_ID(serialInBuf); // use the ID from the packet to determine sendr addr
    nrf24l01SetTXAddress(&nrf24l01, (uint8_t *)temp_addr); // Set the addr
    
    // let the other device switch to listen mode and then send an ack
    chThdSleepMicroseconds(500);
    chMtxLock(&nrfMutex); // Grab the mutex and send
    chnWriteTimeout(&nrf24l01.channels[0], serialOutBuf, sizeof(serialOutBuf), MS2ST(10));
    chMtxUnlock(&nrfMutex);

    // chprintf((BaseSequentialStream*)&SD1, "ack sent. control bits: 0b%d%d%d%d\n\r", (serialOutBuf[0]>>3)&1, (serialOutBuf[0]>>2)&1, (serialOutBuf[0]>>1)&1, serialOutBuf[0]&1);
   
    // if the packet is the one we are expecting, write to the sd card and toggle the expected alt_bit, otherwise
    // don't do anything
    if (alt_bit == get_abp(serialInBuf) && state == RECEIVING_DATA) {
      if (!is_beacon(serialInBuf) && strlen((char*) serialInBuf) > 2) { /* if it is a beacon, don't do the file write. also if it is empty, don't do the file write. */
	decode_packet_and_write("data.csv", serialInBuf);
      }
      alt_bit ^= 1;
    }
    
    // we put this state change down here so that it will still write eoms to the card
    if (is_eom(serialInBuf)) {
      state = LISTENING_FOR_BEACON;
      chprintf((BaseSequentialStream*)&SD1, "got eom\n\r");
    }

    // clean all of the buffers
    // memset(serialInBuf, 0, 32);
    // memset(serialOutBuf, 0, 32);
  }
}

void print_ch() {
  // chprintf((BaseSequentialStream*)&SD1, "Getting mutex...\n\r");
  chMtxLock(&nrfMutex);
  // chprintf((BaseSequentialStream*)&SD1, "Got mutex...\n\r");
  chprintf((BaseSequentialStream*)&SD1, "My channel is: %i\n\r", nrf24l01ReadRegister(&nrf24l01, NRF24L01_REG_RF_CH));
  chMtxUnlock(&nrfMutex);
  // chprintf((BaseSequentialStream*)&SD1, "Releasing mutex...\n\r");
}

void set_ch(int chan) {
  chMtxLock(&nrfMutex);
  nrf24l01SetChannel(&nrf24l01, chan);
  extChannelEnable(&EXTD1, chan);
  chMtxUnlock(&nrfMutex);
  print_ch();
}

void print_addr() {
  uint8_t temp[6] = "\0\0\0\0\0\0";
  chMtxLock(&nrfMutex);
  nrf24l01ReadAddressRegister(&nrf24l01, NRF24L01_REG_RX_ADDR_P0, temp);
  chMtxUnlock(&nrfMutex);
  chprintf((BaseSequentialStream*)&SD1, "My address is: %s\n\r", (char *)temp);
}

void set_addr(uint8_t* addr) {
  uint8_t tempaddr[5] = "SITE\0";
  tempaddr[4] = addr[0];
  unique_identifier = addr[0];
  chMtxLock(&nrfMutex);
  nrf24l01SetRXAddress(&nrf24l01, 0, tempaddr);
  chMtxUnlock(&nrfMutex);
  print_addr();
}

int transmit_packet(uint8_t ID, uint8_t* payload, int len, int is_eom, int is_beacon)
{
  int i;
  uint8_t tempaddr[5] = "SITEA";
  tempaddr[4] = ID;
  int payload_start = 2;
  static uint8_t a_bit = 0;
  uint8_t serialInBuf[32];
  uint8_t serialOutBuf[32];

  // chprintf((BaseSequentialStream*)&SD1, "tempaddr is: ");
  // for (i=0; i<5; i++) chprintf((BaseSequentialStream*)&SD1, "%c", tempaddr[i]);
  // chprintf((BaseSequentialStream*)&SD1, "\r\n");
  chMtxLock(&nrfMutex);
  nrf24l01SetTXAddress(&nrf24l01, tempaddr);
  chMtxUnlock(&nrfMutex);
  // memset(serialOutBuf, 0, 32);
  if (a_bit) set_abp();
  if (is_eom) set_eom();
  if (is_beacon) set_beacon();
  serialOutBuf[0] |= 0xF0;
  serialOutBuf[1] = unique_identifier;
  
  for (i=payload_start; i<payload_start+len; i++) {
    serialOutBuf[i] = payload[i-payload_start];
    if (i==31) break;
  }

  for (i=0; i<32; i++) {
    chprintf((BaseSequentialStream*)&SD1, "\t%d\t%d\t%c\n\r", i, serialOutBuf[i], serialOutBuf[i]);
  }

  // Try sending 5 times
  //chMtxLock(&nrfMutex); // Grab the mutex and try to send.
  for (i=1; i<=5; i++) {
    chMtxLock(&nrfMutex); // Grab the mutex and try to send.
    chnWriteTimeout(&nrf24l01.channels[0], serialOutBuf, sizeof(serialOutBuf), MS2ST(100));
    size_t s = chnReadTimeout(&nrf24l01.channels[0], serialInBuf, 32, MS2ST(20)); // Try to receive ack.
    chMtxUnlock(&nrfMutex);
    // we got an ack! :)
    if (s && is_ack(serialInBuf)) {
      if (get_abp(serialInBuf) == a_bit){
	//chMtxUnlock(&nrfMutex);
	chprintf((BaseSequentialStream*)&SD1, "ACK!\n\r");
	a_bit ^= 1; // toggle sequence bit.
	// memset(serialOutBuf, 0, 32); // Reset serialOutBuf.
	// memset(serialInBuf, 0, 32);
	return 1;
      } else {
	chprintf((BaseSequentialStream*)&SD1, "NACK!\n\r");
      }
    }
    // give it some time to wake up
    chThdSleepMilliseconds(1);
  }
  //chMtxUnlock(&nrfMutex);
  // If we got here, this means that we didn't get an ack from the
  // receiver. So spit out an error.
  chprintf((BaseSequentialStream*)&SD1, "No acknowledge received after 5 tries. Giving up...\n\r");
  a_bit ^= 1; // toggle sequence bit.
  // memset(serialOutBuf, 0, 32); // Reset serialOutBuf.
  return 0;
}

int beacon(uint8_t ID, uint8_t* message, int len) {
  return transmit_packet(ID, message, len, FALSE, TRUE);
}

void initNordic() {
  my_addr[4] = unique_identifier;

  /*
   * Setup the pins for the spi link on the GPIOB. This link connects to the Nordic.
   *
   */

  palSetPadMode(GPIOB, 3, PAL_MODE_ALTERNATE(6)); // SCK
  palSetPadMode(GPIOB, 4, PAL_MODE_ALTERNATE(6)); // MISO
  palSetPadMode(GPIOB, 5, PAL_MODE_ALTERNATE(6)); // MOSI
  palSetPadMode(GPIOC, GPIOC_PIN1, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad(GPIOC, GPIOC_PIN1);
  palSetPadMode(GPIOC, GPIOC_PIN2, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(GPIOC, GPIOC_PIN2);
  palSetPadMode(GPIOC, GPIOC_PIN3, PAL_MODE_INPUT_PULLUP);
  spiStart(&SPID3, &nrf24l01SPI);

  chMtxObjectInit(&nrfMutex);

  extStart(&EXTD1, &extcfg);
  nrf24l01ObjectInit(&nrf24l01);
  nrf24l01Start(&nrf24l01, &nrf24l01Config);
  
  extChannelEnable(&EXTD1, 3);

  initNRF24L01(&nrf24l01);
}
