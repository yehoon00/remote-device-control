#ifndef SENSOR_H
#define SENSOR_H

#include <pthread.h>

#define CDS_PIN     11

extern volatile int sensor_state;
extern pthread_t sensor_thread_id;
extern volatile int current_client_sock;

void sensor_init();
void sensor_on(int csock);
void sensor_off();
void *sensor_monitor_thread(void *arg);

#endif
