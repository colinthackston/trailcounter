
#include <sdc_driver.h>

void decode_packet_and_write(char* filename, uint8_t* buffer);
void transmit_all(uint8_t ID, const TCHAR *path);
void die(int rc);
void initSD(void);
