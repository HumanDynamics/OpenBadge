//sdfsdf

#ifndef RTC_TIMING_H
#define RTC_TIMING_H

#include "nrf_drv_config.h"
#include "nrf_soc.h"
#include "nrf_drv_rtc.h"
#include "app_error.h"

#include "debug_log.h"

volatile bool countdownOver;  //set true when the countdown interrupt triggers
volatile bool ble_timeout;

/**
 * rtc event handler function
 * For extending the 24-bit hardware clock, by incrementing a larger number every time it overflows
 * And handling countdown timer end events
 */
void rtc_handler(nrf_drv_rtc_int_type_t int_type);
 
/**
 * initialize rtc
 */
void rtc_config(void);


/**
 * set a compare interrupt to occur a number of milliseconds from now (i.e. to break from sleep)
 * NOTE: Maximum countdown time is 130 seconds.
 */
void countdown_set(unsigned long ms);

/**
 * similar to countdown_set, but used to keep track of BLE connection timeout.
 */
void ble_timeout_set(unsigned long ms);


/**
 * returns 32768Hz ticks of RTC, extended to 43 bits  (from built-in 24 bits)
 */
unsigned long long ticks(void);

/**
 * emulate functionality of millis() in arduino
 * divide rtc count by 32768=2^15, get number of seconds since timer started
 * x1000 or 1000000 for millis, micros.  (~30.5us precision)
 */
unsigned long millis(void);

/**
 * emulate functionality of micros() in arduino
 */
unsigned long micros(void);


/**
 * get current timestamp (set with setTime) in seconds (most likely epoch time)
 * Inspired by Arduino Time library
 */
unsigned long now();

/**
 * get fractional part of timestamp in ms, i.e. returns 0-999.
 * For better time precision, while still keeping simple compatibility with epoch time
 * Corresponds to time when now() was last called, not actual current time!
 */
unsigned long nowFractional();

/**
 * set current timestamp (set with setTime)
 * In seconds, like Unix time, but extra parameter to set it partway through a full second.
 */
void setTimeFractional(unsigned long timestamp, unsigned long fractional);

/**
 * set current timestamp (set with setTime)
 * In seconds, like Unix time
 */
void setTime(unsigned long timestamp);




#endif //TIMING_H