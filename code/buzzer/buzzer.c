#include <wiringPi.h>
#include <softTone.h>
#include "buzzer.h" 

int notes[] = {
    391, 391, 440, 440, 391, 391, 329.63, 329.63,
    391, 391, 329.63, 329.63, 293.66, 293.66, 293.66, 0,
    391, 391, 440, 440, 391, 391, 329.63, 329.63,
    391, 329.63, 293.66, 329.63, 261.63, 261.63, 261.63, 0
};

// 전역 변수 정의
volatile int buzzer_state = 0; // 0: 정지, 1: 재생 중
pthread_t buzzer_thread_id;

// 실제 음악 재생 스레드 함수
void *music_play_thread(void *arg)
{
    int i;
    for (i = 0; i < TOTAL; ++i) {
        if (buzzer_state == 0) break;
        softToneWrite(BUZZER_PIN, notes[i]);
        delay(280);
    }
    softToneWrite(BUZZER_PIN, 0);
    buzzer_state = 0;
    return NULL;
}

void buzzer_init()
{
    softToneCreate(BUZZER_PIN);
}

// 부저 켜기 및 스레드 생성
void buzzer_on()
{
    if (buzzer_state == 1) {
        buzzer_state = 0;
        pthread_join(buzzer_thread_id, NULL);
    }
    buzzer_state = 1;
    pthread_create(&buzzer_thread_id, NULL, music_play_thread, NULL);
}

// 부저 끄기 및 스레드 정돈 
void buzzer_off()
{
    if (buzzer_state == 1) {
        buzzer_state = 0;
        pthread_join(buzzer_thread_id, NULL);
    }
    softToneWrite(BUZZER_PIN, 0);
}

void play_warning_beep()
{
    if (buzzer_state == 1) {
        buzzer_state = 0;
        pthread_join(buzzer_thread_id, NULL);
    }

    for (int i = 0; i < 3; i++) {
        softToneWrite(BUZZER_PIN, 2000);
        delay(100);
        softToneWrite(BUZZER_PIN, 0);
        delay(100);
    }
}
