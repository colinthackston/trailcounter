#include <stm32f30x.h> // Pull in include files for F30x standard drivers
#include <f3d_uart.h>
#include <f3d_gyro.h>
#include <f3d_lcd_sd.h>
#include <f3d_rtc.h>
#include <ff.h>
#include <diskio.h>
#include <stdio.h>

void die (FRESULT rc) {
  printf("Failed with rc=%u.\n", rc);
  while (1);
}

FATFS Fatfs;  /* File system object */
FIL Fil;  /* File object */
BYTE Buff[128]; /* File read buffer */

int main(void) {
  char footer[20];
  int count=0;
  int i;

  FRESULT rc; /* Result code */
  DIR dir;  /* Directory object */
  FILINFO fno;  /* File information object */
  UINT bw, br;
  unsigned int retval;

  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  f3d_uart_init();
  f3d_gyro_init();
  f3d_lcd_init();
  f3d_delay_init();
  f3d_rtc_init();

  int16_t a[3] = { 0 };
  printf("ID: %x", getID());
  delay(1000);
  
  /*while(1) {
    f3d_gyro_getdata(a);
    printf("ID: %x", getID());
    //printf("x: %05x, y: %05x, z: %05x \n", a[0], a[1], a[2]);
    printf("x: %05d, y: %05d, z: %05d \n", a[0], a[1], a[2]);
  }*/

  f_mount(0, &Fatfs); /* Register volume work area (never fails) */

  printf("\nOpen an existing file (message.txt).\n");
    rc = f_open(&Fil, "message.txt", FA_READ | FA_WRITE);
  if (rc) die(rc);
 
  printf("\nType the file content.\n");
  for (;;) {
    rc = f_read(&Fil, Buff, sizeof Buff, &br);  /* Read a chunk of file */
    if (rc || !br) break; /* Error or end of file */
    for (i = 0; i < br; i++)  /* Type the data */
      putchar(Buff[i]);
  }
  if (rc) die(rc);

  printf("\nAdd onto the text of data \n");
  rc = f_write(&Fil, "THIS IS NOT A TEST\n", 19, &bw);
  if (rc) die(rc);
  printf("%u bytes written.\n", bw);
  
  printf("\nClose the file.\n");
  rc = f_close(&Fil);
  if (rc) die(rc);
  
  




  char words[26000];
  char namebuf[9];
  int steve = 0;
  int stark = 0;
  for(steve = 0; steve < 26000; steve++) {
    if(steve%20 == 0) {
      words[steve] = '\n';
    } else {
      words[steve] = 'c';
    }
  }

  for(steve = 0; steve < 5; steve++) {
    sprintf(namebuf, "HELLO%d.TXT", steve);
    printf("\nCreate a new file (hello%d.txt).\n", steve);
    rc = f_open(&Fil, namebuf, FA_WRITE | FA_CREATE_ALWAYS);
    if (rc) die(rc);
  
    for(stark = 0; stark < 20; stark++) {
      //printf("\nWrite a text data. \n");
      rc = f_write(&Fil, &words, 26000, &bw);
      if (rc) die(rc);
      //printf("%u bytes written.\n", bw);
    }  
  
    printf("\nClose the file.\n");
    rc = f_close(&Fil);
    if (rc) die(rc);
  }
  





  printf("\nOpen root directory.\n");
  rc = f_opendir(&dir, "");
  if (rc) die(rc);
  
  printf("\nDirectory listing...\n");
  for (;;) {
    rc = f_readdir(&dir, &fno); /* Read a directory item */
    if (rc || !fno.fname[0]) break; /* Error or end of dir */
    if (fno.fattrib & AM_DIR)
      printf(" <dir> %s\n", fno.fname);
    else
      printf("%8lu %s\n", fno.fsize, fno.fname);
  }
  if (rc) die(rc);
  
  printf("\nTest completed.\n");

  rc = disk_ioctl(0,GET_SECTOR_COUNT,&retval);
  printf("%d %d\n",rc,retval);

  while (1);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {
/* Infinite loop */
/* Use GDB to find out why we're here */
  while (1);
}
#endif

/* main.c ends here */
