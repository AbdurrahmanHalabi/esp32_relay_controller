#ifndef __LEDC_H__
#define __LEDC_H__

#include "inttypes.h"

extern uint8_t reading_stat;
extern uint8_t request_stat;

/*******************************************************
 *                Function Declarationss
 *******************************************************/
void leds_init();
void led_blink(int LED, int period);
void led_blink3(uint8_t chan, uint16_t fade_time);
void led_indicator_task(void *arg);

#endif /* __LEADC_H__ */
