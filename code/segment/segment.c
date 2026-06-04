#include <stdio.h>
#include <wiringPi.h>
#include "segment.h"
#include "buzzer.h"  // 0이 되었을 때 libbuzzer.so의 buzzer_on()을 호출하기 위함

// 전역 변수 정의
static volatile int segment_state = 0; 
pthread_t segment_thread_id;
volatile int segment_num = 0;

static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

static const int gpiopins[4] = {SEG_A_PIN, SEG_B_PIN, SEG_C_PIN, SEG_D_PIN};

static const int number[10][4] = {
    {0,0,0,0}, /* 0 */
    {0,0,0,1}, /* 1 */
    {0,0,1,0}, /* 2 */
    {0,0,1,1}, /* 3 */
    {0,1,0,0}, /* 4 */
    {0,1,0,1}, /* 5 */
    {0,1,1,0}, /* 6 */
    {0,1,1,1}, /* 7 */
    {1,0,0,0}, /* 8 */
    {1,0,0,1}  /* 9 */
};

void set_segment_state(int state) {
    pthread_mutex_lock(&state_mutex);
    segment_state = state;
    pthread_mutex_unlock(&state_mutex);
}

int get_segment_state(void) {
    int current_state;
    pthread_mutex_lock(&state_mutex);
    current_state = segment_state;
    pthread_mutex_unlock(&state_mutex);
    return current_state;
}

void segment_init()
{
    for (int i = 0; i < 4; i++) {
        pinMode(gpiopins[i], OUTPUT);
        digitalWrite(gpiopins[i], HIGH); // 초기 상태: 출력 차단(디코더 비활성화 상태)
    }
}

// 단일 숫자를 즉시 출력하는 내부/외부 겸용 함수
void segment_display(int num)
{
    if (num < 0 || num > 9) return;
    segment_num = num;

    for (int i = 0; i < 4; i++) {
        digitalWrite(gpiopins[i], number[num][i] ? HIGH : LOW);
    }
}

// 백그라운드 카운트다운 스레드 함수
void *segment_countdown_thread(void *arg)
{
    int start = *((int *)arg);
    
    for (int i = start; i >= 0; i--) {
        if (get_segment_state() == 0) break; // 중간에 segment_stop() 호출 시 루프 탈출
        
        segment_display(i);

        if (i == 0) {
            buzzer_on(); // 0이 되는 순간 노래/부저 재생 시작 (libbuzzer.so 연동)
            delay(1000); // 0을 잠시 보여줌
            break;
        }

        // 1초 대기하는 동안 멈춤 명령(segment_state == 0)을 빠르게 감지하기 위해 쪼개서 대기
        for (int j = 0; j < 10; j++) {
            if (get_segment_state() == 0) break;
            delay(100);
        }
    }

    // 종료 후 세그먼트 클리어 (모두 HIGH)
    for (int i = 0; i < 4; i++) {
        digitalWrite(gpiopins[i], HIGH);
    }
    
    set_segment_state(0);
    return NULL;
}

// 카운트다운 시작 함수 (기존 스레드가 돌고 있으면 정리 후 새로 생성)
void segment_countdown(int start_num)
{
    if (start_num > 9) start_num = 9;
    if (start_num < 0) start_num = 0;

    if (get_segment_state() == 1) {
        set_segment_state(0);
        pthread_join(segment_thread_id, NULL);
    }

    // 다른 스레드 함수 패턴과 맞추기 위해 힙 변수나 static 변수를 통해 값 전달
    static int pass_num;
    pass_num = start_num;

    set_segment_state(1);
    pthread_create(&segment_thread_id, NULL, segment_countdown_thread, (void *)&pass_num);
}

// 세그먼트 작동 정지 함수
void segment_stop()
{
    if (get_segment_state() == 1) {
        set_segment_state(0);
        pthread_join(segment_thread_id, NULL);
    }
    
    // 세그먼트 출력 초기화
    for (int i = 0; i < 4; i++) {
        digitalWrite(gpiopins[i], HIGH);
    }
}
