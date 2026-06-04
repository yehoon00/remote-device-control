#include <wiringPi.h>
#include <softPwm.h>
#include "led.h"

int pwm_val = 255;
int led_state = 0;

void led_init()
{
    softPwmCreate(LED_PIN, 0, 255);
}

void led_on()
{
    led_state = 1;
    softPwmWrite(LED_PIN, pwm_val);
}

void led_off()
{
    led_state = 0;
    softPwmWrite(LED_PIN, 0);
}

void set_brightness(int level)
{
    if (level == 3)      pwm_val = 255;
    else if (level == 2) pwm_val = 170;
    else if (level == 1) pwm_val = 85;
    else                 pwm_val = 0;

    if (led_state == 1)
        softPwmWrite(LED_PIN, pwm_val);
}
