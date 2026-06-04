#include <wiringPi.h>
#include <softTone.h>
#include "buzzer.h" 

int notes[SONG_COUNT][TOTAL] = {
    // [0] 학교종 
    {
        391, 391, 440, 440, 391, 391, 329.63, 329.63,
        391, 391, 329.63, 329.63, 293.66, 293.66, 293.66, 0,
        391, 391, 440, 440, 391, 391, 329.63, 329.63,
        391, 329.63, 293.66, 329.63, 261.63, 261.63, 261.63, 0
    },
    // [1] 비행기
    {
        329.63, 293.66, 261.63, 293.66, 329.63, 329.63, 329.63, 0,
        293.66, 293.66, 293.66, 0, 329.63, 391, 391, 0,
        329.63, 293.66, 261.63, 293.66, 329.63, 329.63, 329.63, 329.63,
        293.66, 293.66, 329.63, 293.66, 261.63, 261.63, 261.63, 0
    },
    // [2] 징글벨 
    {
        329.63, 329.63, 329.63, 0, 329.63, 329.63, 329.63, 0,
        329.63, 391, 261.63, 293.66, 329.63, 0, 0, 0,
        349.23, 349.23, 349.23, 349.23, 349.23, 329.63, 329.63, 329.63,
        329.63, 293.66, 293.66, 329.63, 293.66, 0, 391, 0
    }
};

// 전역 변수 정의
volatile int buzzer_state = 0; // 0: 정지, 1: 재생 중
pthread_t buzzer_thread_id;

// 실제 음악 재생 스레드 함수
void *music_play_thread(void *arg)
{
    int song_idx = (int)(long)arg;
    if(song_idx < 0 || song_idx >= SONG_COUNT) song_idx = 0;

    int i;
    for (i = 0; i < TOTAL; ++i) {
        if (buzzer_state == 0) break;
        softToneWrite(BUZZER_PIN, notes[song_idx][i]);
        delay(250);
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
void buzzer_on(int song_idx)
{
    if (buzzer_state == 1) {
        buzzer_state = 0;
        pthread_join(buzzer_thread_id, NULL);
    }
    buzzer_state = 1;
    pthread_create(&buzzer_thread_id, NULL, music_play_thread, (void*)(long)song_idx);
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
