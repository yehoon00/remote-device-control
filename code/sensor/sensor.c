#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include "sensor.h"
#include "led.h"

volatile int sensor_state = 0;
pthread_t sensor_thread_id;
volatile int current_client_sock = -1;

void *sensor_monitor_thread(void *arg)
{
    int send_counter = 0;

    while (sensor_state == 1) {
        int cds_val = digitalRead(CDS_PIN);

        if (cds_val == HIGH) {
            led_on(); // libled.so 에 있는 함수 호출
        } else {
            led_off(); // libled.so 에 있는 함수 호출
        }

        if (send_counter >= 5) {
            char data_msg[BUFSIZ];
            snprintf(data_msg, sizeof(data_msg), "[DATA] Sensor Value: %d\n", cds_val);

            if (current_client_sock != -1) {
                if (write(current_client_sock, data_msg, strlen(data_msg)) <= 0) {
                    sensor_state = 0;
                    current_client_sock = -1;
                    break;
                }
            }
            send_counter = 0;
        }

        delay(200);
        send_counter++;
    }

    led_off();
    return NULL;
}

void sensor_init()
{
    pinMode(CDS_PIN, INPUT);
}

void sensor_on(int csock)
{
    if (sensor_state == 1) {
        sensor_state = 0;
        pthread_join(sensor_thread_id, NULL);
    }
    current_client_sock = csock;
    sensor_state = 1;
    pthread_create(&sensor_thread_id, NULL, sensor_monitor_thread, NULL);
}

void sensor_off()
{
    if (sensor_state == 1) {
        sensor_state = 0;
        pthread_join(sensor_thread_id, NULL);
        current_client_sock = -1;
    }
}
