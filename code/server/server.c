#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <wiringPi.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

#include "led.h"
#include "buzzer.h"
#include "sensor.h"
#include "segment.h"

#define TCP_PORT    5100
#define LOG_FILE    "server.log"

typedef struct {
    int sock;
    struct sockaddr_in addr;
} client_info_t;

// 스레드가 실행할 함수 프로토타입
void *client_handler(void *arg);
void write_log(const char *format, ...);

int main(int argc, char **argv)
{   
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    int ssock;
    socklen_t clen;
    struct sockaddr_in servaddr, cliaddr;

    if (daemon(1, 0) < 0) {
        perror("daemon()");
        return -1;
    }

    write_log("====== Daemon server started successfully. (Port: %d) ======", TCP_PORT);

    wiringPiSetupGpio();
    led_init();
    buzzer_init();
    sensor_init();
    segment_init();

    if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        write_log("ERROR: socket() creation failed");
        return -1;
    }

    int opt = 1;
    setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);

    if(bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        write_log("ERROR: bind() failed");
        return -1;
    }

    if(listen(ssock, 8) < 0) {
        write_log("ERROR: listen() failed");
        return -1;
    }

    clen = sizeof(cliaddr);

    while(1) {
        // 새로운 클라이언트 접속 수락
        int csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);
        if (csock < 0) {
            write_log("ERROR: accept() failed");
            continue;
        }

        client_info_t *cinfo = malloc(sizeof(client_info_t));
        cinfo->sock = csock;
        cinfo->addr = cliaddr;
       
        write_log("▶ [CONNECTED] Client connected from IP: %s, Port: %d",
                  inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        // 클라이언트 전용 스레드 생성
        pthread_t t_id;
        if (pthread_create(&t_id, NULL, client_handler, (void *)cinfo) != 0) {
            write_log("ERROR: pthread_create() failed");
            close(csock);
            free(cinfo);
        }
        
        // 스레드가 종료되면 자동으로 자원을 반환하도록 설정
        pthread_detach(t_id);
    }

    close(ssock);
    return 0;
}

void write_log(const char *format, ...) {
    FILE *fp = fopen(LOG_FILE, "a"); // Append 모드로 오픈
    if (fp == NULL) return;

    time_t timer = time(NULL);
    struct tm *t = localtime(&timer);
    
    // 타임스탬프 선두 전송
    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);

    fprintf(fp, "\n");
    fflush(fp); // 버퍼에 머물지 않고 즉시 파일에 쓰도록 강제
    fclose(fp);
}

// 클라이언트와 1:1로 통신하는 스레드 메인 함수
void *client_handler(void *arg) {
    client_info_t *cinfo = (client_info_t *)arg;
    int csock = cinfo->sock;
    struct sockaddr_in cliaddr = cinfo->addr;
    free(cinfo); // 동적 할당된 메모리 해제
    
    char mesg[BUFSIZ];
    int n;

    char cli_ip[32];
    int cli_port = ntohs(cliaddr.sin_port);
    strncpy(cli_ip, inet_ntoa(cliaddr.sin_addr), sizeof(cli_ip) - 1);
    cli_ip[sizeof(cli_ip) - 1] = '\0';

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
            write_log("[COMMAND] Client (%s:%d) requested disconnect.", cli_ip, cli_port);
            break;
        }
        
        write_log("[RECEIVED] Client (%s:%d) -> Command: \"%s\"", cli_ip, cli_port, mesg);

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
            snprintf(reply, sizeof(reply), "Invalid command request (%.8100s).\n", mesg);
        }

        if (write(csock, reply, strlen(reply)) <= 0) {
            break;
        }
    }
    
    led_off();
    pwm_val = 255;
    buzzer_off();
    segment_stop();

    if (current_client_sock == csock)
        sensor_off();

    write_log("◀ [DISCONNECTED] Client connection closed. (IP: %s, Port: %d)", cli_ip, cli_port);

    close(csock);
    return NULL;
}
