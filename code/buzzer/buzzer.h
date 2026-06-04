#ifndef BUZZER_H
#define BUZZER_H

#include <pthread.h>

#define BUZZER_PIN  21
#define TOTAL       32

extern volatile int buzzer_state;
extern pthread_t buzzer_thread_id;
extern int notes[];

void buzzer_init();
void buzzer_on();
void buzzer_off();
void *music_play_thread(void *arg);
void play_warning_beep();

#endif
