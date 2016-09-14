
#include <transmit.h>
#include <nordic.h>
#include <stdio.h>
#include <time.h>

uint8_t to_send[30];
uint8_t date_buf[10];
uint8_t waste[1];
char str[30];
uint32_t date;
uint8_t count_buf[4];
char test_buf[16];
uint16_t peep_count;
int write_loc = 0;
int i;

// the FATFS global variable - we will use this for file transfer
FIL txFil;          /* a file object */
FIL rxFil;          /* a file object */
// some more stuff that is involved with the file system
FRESULT err;       /* Result code */
UINT byteR;      /* bw = bytes written; br = bytes read */

void transmit_all(uint8_t ID, const TCHAR *path) {
    /* Open a text file */
    err = f_open(&txFil, path, FA_READ);
    if (err) die(err);

    for(;;) {
      //*
      err = f_read(&txFil, date_buf, sizeof(date_buf), &byteR);
      if (err) die(err);
      if (!byteR) break;
      memset(str, 0, sizeof(str));
      strcpy(str, (char*)date_buf);
      //      chprintf((BaseSequentialStream*)&SD1, "Date? %s\r\n", str);
      date = (uint32_t) atoi(str);
      
      err = f_read(&txFil, waste, sizeof(waste), &byteR);
      if (err) die(err);      
      if (!byteR) break;

      //      f_lseek(&txFil, f_tell(&txFil)+1);

      err = f_read(&txFil, count_buf, sizeof(count_buf), &byteR);
      if (err) die(err);
      memset(str, 0, sizeof(str));
      strcpy(str, (char *) count_buf);
      //      chprintf((BaseSequentialStream*)&SD1, "Count? %s\r\n", str);
      peep_count = (uint16_t) atoi(str);

      err = f_read(&txFil, waste, sizeof(waste), &byteR);
      if (err) die(err);      
      if (!byteR) break;

      for (i=3; i>=0; i--) to_send[write_loc++] = (date >> (i*8)) & 0xFF;
      for (i=1; i>=0; i--) to_send[write_loc++] = (peep_count >> (i*8)) & 0xFF;
      if (write_loc == 30) {
	write_loc = 0;
	transmit_packet(ID, to_send, 30, FALSE, FALSE);
	memset(str, 0, sizeof(str));
	strcpy(str, (char*)to_send);
	//	for (i=0; i<(int)sizeof(to_send); i++) chprintf((BaseSequentialStream*)&SD1, "%d ", to_send[i]);
	chprintf((BaseSequentialStream*)&SD1, "\r\n");
	memset(to_send, 0, 30); // Reset serialOutBuf.
      }
      //*/
      
      /*/ we know this code works 
      // chprintf((BaseSequentialStream*)&SD1, "Here\r\n");
      memset(test_buf, 0, sizeof(test_buf));
      err = f_read(&txFil, test_buf, sizeof(test_buf), &byteR);
      if (err) die(err);
      else chprintf((BaseSequentialStream*)&SD1, "No err!\r\n");
      if (!byteR) break;

      transmit_packet(ID, (uint8_t *) test_buf, FALSE, FALSE);
      //memset(test_buf, 0, sizeof(test_buf));
      //*/
    }

    transmit_packet(ID, to_send, 30, TRUE, FALSE);
    //    chprintf((BaseSequentialStream*)&SD1, "EOM sent!\r\n");
    /* Close the txFile */
    f_close(&txFil);
}

int monthtoi(char* month) {
  if (!strcmp((const char*) month, (const char*) "Jan")) return 1;
  else if (!strcmp((const char*) month, (const char*) "Feb")) return 2;
  else if (!strcmp((const char*) month, (const char*) "Mar")) return 3;
  else if (!strcmp((const char*) month, (const char*) "Apr")) return 4;
  else if (!strcmp((const char*) month, (const char*) "May")) return 5;
  else if (!strcmp((const char*) month, (const char*) "Jun")) return 6;
  else if (!strcmp((const char*) month, (const char*) "Jul")) return 7;
  else if (!strcmp((const char*) month, (const char*) "Aug")) return 8;
  else if (!strcmp((const char*) month, (const char*) "Sep")) return 9;
  else if (!strcmp((const char*) month, (const char*) "Oct")) return 10;
  else if (!strcmp((const char*) month, (const char*) "Nov")) return 11;
  else if (!strcmp((const char*) month, (const char*) "Dec")) return 12;
  else return 0;
}

void decode_packet_and_write(char* filename, uint8_t* inbuffer) {
  int i, j, amount_to_shift;
  uint16_t peep_count;
  time_t date;
  uint8_t* buffer = inbuffer+2;
  char datestring[26];
  char to_return[131];
  char month[4];
  char day[3];
  char time[9];
  char year[5];
  for (i=0; i<5; i++) {
    date = 0;
    peep_count = 0;
    for (j=0; j<4; j++) {
      amount_to_shift = (3-j)*8;
      date |= buffer[i*6+j]<<amount_to_shift;
    }
    for (j=0; j<2; j++) {
      amount_to_shift = (1-j)*8;
      peep_count |= buffer[i*6+j+4]<<amount_to_shift;
    }
    //*/
    //    datestring = ctime(&date);
    strncpy(month, ctime(&date)+4, 3);
    month[3] = '\0';
    strncpy(day, ctime(&date)+8, 2);
    strncpy(time, ctime(&date)+11, 8);
    time[8] = '\0';
    strncpy(year, ctime(&date)+20, 4);
    //chprintf((BaseSequentialStream*)&SD1, "%02d/%02d/%d %s,%04d,\r\n", monthtoi(month), atoi(day), atoi(year), time, peep_count);
    sprintf(datestring, "%02d/%02d/%d %s,%04d,\n", monthtoi(month), atoi(day), atoi(year), time, peep_count);
    strncpy(&(to_return[i*26]), datestring, 26);
  }
  to_return[130] = '\0';
  //chprintf((BaseSequentialStream*)&SD1, "%s", to_return);
  err = appendToCard(&rxFil, filename, to_return, strlen(to_return));
  if (err) chprintf((BaseSequentialStream*)&SD1, "appendToCard failed with error code %d\n\r", err);
}
