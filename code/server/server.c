#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <wiringPi.h>
#include <signal.h>

#include "led.h"
#include "buzzer.h"
#include "sensor.h"
#include "segment.h"

#define TCP_PORT    5100

// 스레드가 실행할 함수 프로토타입
void *client_handler(void *arg);

int main(int argc, char **argv)
{   
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    int ssock;
    socklen_t clen;
    struct sockaddr_in servaddr, cliaddr;

    if (daemon(0, 0) < 0) {
        perror("daemon()");
        return -1;
    }

    wiringPiSetupGpio();
    led_init();
    buzzer_init();
    sensor_init();
    segment_init();

    if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    int opt = 1;
    setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);

    if(bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        return -1;
    }

    if(listen(ssock, 8) < 0) {
        perror("listen()");
        return -1;
    }

    clen = sizeof(cliaddr);

    while(1) {
        // 새로운 클라이언트 접속 수락
        int csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);
        if (csock < 0) {
            perror("accept()");
            continue;
        }

        // malloc을 사용해 클라이언트 소켓 디스크립터를 동적 할당 (스레드 간 데이터 오염 방지)
        int *new_sock = malloc(sizeof(int));
        *new_sock = csock;

        // 클라이언트 전용 스레드 생성
        pthread_t t_id;
        if (pthread_create(&t_id, NULL, client_handler, (void *)new_sock) != 0) {
            perror("pthread_create() 실패");
            close(csock);
            free(new_sock);
        }
        
        // 스레드가 종료되면 자동으로 자원을 반환하도록 설정
        pthread_detach(t_id);
    }

    close(ssock);
    return 0;
}

// 클라이언트와 1:1로 통신하는 스레드 메인 함수
void *client_handler(void *arg) {
    int csock = *((int *)arg);
    free(arg); // 동적 할당된 메모리 해제
    
    char mesg[BUFSIZ];
    int n;

    while(1) {
        memset(mesg, 0, BUFSIZ);
        n = read(csock, mesg, BUFSIZ - 1);
        
        if (n <= 0) {
            if (n < 0) perror("read()");
            break; 
        }

        mesg[n] = '\0';
        mesg[strcspn(mesg, "\r\n")] = '\0'; // 개행 문자 제거

        if (strcasecmp(mesg, "exit") == 0) {
            break;
        }
        
        int is_valid = 1;
        char reply_detail[BUFSIZ] = "";

        // ────────────────────────────────────────────────────────
        // [수정] 문자열 파싱 없이 클라이언트가 보낸 데이터 그대로 비교
        // ────────────────────────────────────────────────────────
        if (strcmp(mesg, "1") == 0) {
            led_on();
            strcpy(reply_detail, "LED ON");
        } 
        else if (strcmp(mesg, "2") == 0) {
            led_off();
            strcpy(reply_detail, "LED OFF");
        } 
        // 3번이나 8번처럼 추가 데이터가 붙는 경우 (예: "3 50", "8 7")
        else if (strncmp(mesg, "3 ", 2) == 0) {
            int level = atoi(mesg + 2); // "3 " 뒷부분 문자열을 숫자로 변환
            
            if (level >= 1 && level <= 3) {
                set_brightness(level);

                char *level_str = (level == 3) ? "MAX" : (level == 2) ? "MID" : "MIN";
                snprintf(reply_detail, sizeof(reply_detail), "Set LED Brightness to %s", level_str);
            } else {
                is_valid = 0;
            }
        }
        else if (strcmp(mesg, "4") == 0) {
            buzzer_on();
            strcpy(reply_detail, "BUZZER ON");
        } 
        else if (strcmp(mesg, "5") == 0) {
            buzzer_off();
            strcpy(reply_detail, "BUZZER OFF");
        } 
        else if (strcmp(mesg, "6") == 0) {
            sensor_on(csock);
            strcpy(reply_detail, "SENSOR ON");
        } 
        else if (strcmp(mesg, "7") == 0) {
            sensor_off();
            strcpy(reply_detail, "SENSOR OFF");
        } 
        else if (strncmp(mesg, "8 ", 2) == 0) {
            int num = atoi(mesg + 2); // "8 " 뒷부분 문자열을 숫자로 변환
            segment_countdown(num);
            snprintf(reply_detail, sizeof(reply_detail), "SEGMENT displaying %d and counting down", num);
        } 
        else if (strcmp(mesg, "9") == 0) {
            segment_stop();
            strcpy(reply_detail, "SEGMENT STOP");
        } 
        else {
            is_valid = 0;
        }

        // 응답 전송
        char reply[BUFSIZ];
        if (is_valid) {
            snprintf(reply, sizeof(reply), "[Command %s] Processed successfully\n", reply_detail);
        } else {
            snprintf(reply, sizeof(reply), "Invalid command request (%s).\n", mesg);
        }

        if (write(csock, reply, strlen(reply)) <= 0) {
            perror("write()");
            break;
        }
    }
    
    led_off();
    pwm_val = 255;
    buzzer_off();
    segment_stop();

    if (current_client_sock == csock)
        sensor_off();

    close(csock);
    return NULL;
}
