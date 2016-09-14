/************************
  ChibiOS required trademark copyright crap?
 
*/

#include "trail_counter_rtc_time.h"
#include <string.h>

#define DELAY_BETWEEN_STORE 1

/* A function that stores the information about number
   of people who pass by.
   

   Arguments: count - the count of people who walked by 
       in the most recent event.
   Returns: 0 - on success
            1 - if the RTC fails
	    2 - if storage fails
  */

char to_card[16];
extern TrailRTC_t trail_rtc;
extern FIL fil;
time_t old_time = 0;
int count_this_hour = 0;

int store_data(int *count){ // passes the pointer to the count
  trailRtcUpdate(&trail_rtc);
  chprintf((BaseSequentialStream*)&SD1, "epoch_time: %010d\n\r", (int) trail_rtc.epoch_time);
  count_this_hour += *count;
  /* if it has been an hour since the last write, */
  if (trail_rtc.epoch_time - old_time > DELAY_BETWEEN_STORE) {
    sprintf(to_card, "%d %04d\n", (int) trail_rtc.epoch_time, count_this_hour);
    appendToCard(&fil, "text.txt", to_card, strlen(to_card));
    old_time = trail_rtc.epoch_time;
    count_this_hour = 0;
    *count = 0; // clear the count data
  }
  else chprintf((BaseSequentialStream*)&SD1, "Storing in RAM\n\r");
  //*/
  //sprintf(to_card, "%010d %04d\n", (int) trail_rtc.epoch_time, count);
  //appendToCard(&fil, "text.txt", to_card, strlen(to_card));
  return 0;
}
