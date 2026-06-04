#ifndef LED_H
#define LED_H

#define LED_PIN     12

extern int pwm_val;
extern int led_state;

void led_init();
void led_on();
void led_off();
void set_brightness(int level);

#endif
