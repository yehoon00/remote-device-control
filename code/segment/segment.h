#ifndef SEGMENT_H
#define SEGMENT_H

#include <pthread.h>

/* BCD 디코더 입력 핀 정의 */
#define SEG_A_PIN   23
#define SEG_B_PIN   18
#define SEG_C_PIN   15
#define SEG_D_PIN   14

extern pthread_t segment_thread_id;
extern volatile int segment_num;       // 현재 세그먼트에 표시 중인 숫자

/* 함수 프로토타입 */
void segment_init();
void segment_display(int num);
void segment_countdown(int start_num);
void segment_stop();
void *segment_countdown_thread(void *arg);

void set_segment_state(int state);
int get_segment_state(void);

#endif
