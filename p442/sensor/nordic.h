#include "ch.h"
#include "hal.h"
#include "test.h"
#include "shell.h"
#include "chprintf.h"
#include <chstreams.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define ME 'A'

#define get_abp(x) (((x)[0] >> 3) & 1)
#define set_abp() (serialOutBuf[0] |= 0x8) 
#define unset_abp() (serialOutBuf[0] &= 0xF7)

#define is_ack(x) (((x)[0] >> 2) & 1)
#define set_ack() (serialOutBuf[0] |= 0x4) 
#define unset_ack() (serialOutBuf[0] &= 0xFB)

#define is_eom(x) (((x)[0] >> 1) & 1)
#define set_eom() (serialOutBuf[0] |= 0x2) 
#define unset_eom() (serialOutBuf[0] &= 0xFD)

#define is_beacon(x) (((x)[0]) & 1)
#define set_beacon() (serialOutBuf[0] |= 0x1) 
#define unset_beacon() (serialOutBuf[0] &= 0xFE)

#define get_ID(x) ((uint8_t)((x)[1]))

#define UNUSED(x) (void)(x)

void receive_thread_function(void);
void print_ch(void);
void set_ch(int chan);
void print_addr(void);
void set_addr(uint8_t* addr);
int transmit_packet(uint8_t ID, uint8_t* payload, int len, int is_eom, int is_beacon);
int beacon(uint8_t ID, uint8_t* message, int len);

void initNordic(void);
