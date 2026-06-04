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
#include <dlfcn.h>

#define TCP_PORT    5100
#define LOG_FILE    "server.log"

typedef struct {
    int sock;
    struct sockaddr_in addr;
} client_info_t;

// 스레드가 실행할 함수 프로토타입
void *client_handler(void *arg);
void write_log(const char *format, ...);

void *led_handle = NULL;
void *buzzer_handle = NULL;
void *sensor_handle = NULL;
void *segment_handle = NULL;

void (*dyn_led_init)() = NULL;
void (*dyn_led_on)() = NULL;
void (*dyn_led_off)() = NULL;
void (*dyn_set_brightness)(int) = NULL;

void (*dyn_buzzer_init)() = NULL;
void (*dyn_buzzer_on)(int) = NULL;
void (*dyn_buzzer_off)() = NULL;

void (*dyn_sensor_init)() = NULL;
void (*dyn_sensor_on)(int) = NULL;
void (*dyn_sensor_off)() = NULL;

void (*dyn_segment_init)() = NULL;
void (*dyn_segment_countdown)(int) = NULL;
void (*dyn_segment_stop)() = NULL;

int *dyn_pwm_val = NULL;
int *dyn_current_client_sock = NULL;
int *dyn_song_count = NULL;

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

    led_handle = dlopen("./exec/lib/libled.so", RTLD_LAZY | RTLD_GLOBAL);
    buzzer_handle = dlopen("./exec/lib/libbuzzer.so", RTLD_LAZY | RTLD_GLOBAL);
    sensor_handle = dlopen("./exec/lib/libsensor.so", RTLD_LAZY | RTLD_GLOBAL);
    segment_handle = dlopen("./exec/lib/libsegment.so", RTLD_LAZY | RTLD_GLOBAL);

    if (!led_handle || !buzzer_handle || !sensor_handle || !segment_handle) {
        write_log("ERROR: dlopen failed. LED:%p, BUZZER:%p, SENSOR:%p, SEGMENT:%p", 
                  led_handle, buzzer_handle, sensor_handle, segment_handle);
        return -1;
    }

    dyn_led_init       = dlsym(led_handle, "led_init");
    dyn_led_on         = dlsym(led_handle, "led_on");
    dyn_led_off        = dlsym(led_handle, "led_off");
    dyn_set_brightness = dlsym(led_handle, "set_brightness");

    dyn_buzzer_init    = dlsym(buzzer_handle, "buzzer_init");
    dyn_buzzer_on      = dlsym(buzzer_handle, "buzzer_on");
    dyn_buzzer_off     = dlsym(buzzer_handle, "buzzer_off");

    dyn_sensor_init    = dlsym(sensor_handle, "sensor_init");
    dyn_sensor_on      = dlsym(sensor_handle, "sensor_on");
    dyn_sensor_off     = dlsym(sensor_handle, "sensor_off");

    dyn_segment_init      = dlsym(segment_handle, "segment_init");
    dyn_segment_countdown = dlsym(segment_handle, "segment_countdown");
    dyn_segment_stop      = dlsym(segment_handle, "segment_stop");

    dyn_pwm_val             = (int *)dlsym(led_handle, "pwm_val");
    dyn_current_client_sock = (int *)dlsym(sensor_handle, "current_client_sock");
    dyn_song_count          = (int *)dlsym(buzzer_handle, "song_count");

    // 초기화 수행
    wiringPiSetupGpio();
    if (dyn_led_init) dyn_led_init();
    if (dyn_buzzer_init) dyn_buzzer_init();
    if (dyn_sensor_init) dyn_sensor_init();
    if (dyn_segment_init) dyn_segment_init();

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

    if(led_handle) dlclose(led_handle);
    if(buzzer_handle) dlclose(buzzer_handle);
    if(sensor_handle) dlclose(sensor_handle);
    if(segment_handle) dlclose(segment_handle);

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

        if (strcmp(mesg, "1") == 0) {
            if (dyn_led_on) dyn_led_on();
            strcpy(reply_detail, "LED ON");
        } 
        else if (strcmp(mesg, "2") == 0) {
            if (dyn_led_off) dyn_led_off();
            strcpy(reply_detail, "LED OFF");
        } 
        else if (strncmp(mesg, "3 ", 2) == 0) {
            int level = atoi(mesg + 2); // "3 " 뒷부분 문자열을 숫자로 변환
            
            if (level >= 1 && level <= 3) {
                if (dyn_set_brightness) dyn_set_brightness(level);
                char *level_str = (level == 3) ? "MAX" : (level == 2) ? "MID" : "MIN";
                snprintf(reply_detail, sizeof(reply_detail), "Set LED Brightness to %s", level_str);
            } else {
                is_valid = 0;
            }
        }
        else if (strncmp(mesg, "4", 1) == 0) {
            int song_idx = 0;

            if (strncmp(mesg, "4 ", 2) == 0) {
                song_idx = atoi(mesg + 2);
            }

            int max_songs = dyn_song_count ? *dyn_song_count : 0;

            if (song_idx >= 0 && song_idx < max_songs) {
                if (dyn_buzzer_on) dyn_buzzer_on(song_idx);
                
                char *song_names[] = {"School Bell", "Airplane", "Jingle Bells"};
                snprintf(reply_detail, sizeof(reply_detail), "BUZZER ON (%s)", song_names[song_idx]);
            } else {
                is_valid = 0;
            }
        } 
        else if (strcmp(mesg, "5") == 0) {
            if (dyn_buzzer_off) dyn_buzzer_off();
            strcpy(reply_detail, "BUZZER OFF");
        } 
        else if (strcmp(mesg, "6") == 0) {
            if (dyn_sensor_on) dyn_sensor_on(csock);
            strcpy(reply_detail, "SENSOR ON");
        } 
        else if (strcmp(mesg, "7") == 0) {
            if (dyn_sensor_off) dyn_sensor_off();
            strcpy(reply_detail, "SENSOR OFF");
        } 
        else if (strncmp(mesg, "8 ", 2) == 0) {
            int num = atoi(mesg + 2); // "8 " 뒷부분 문자열을 숫자로 변환
            if (dyn_segment_countdown) dyn_segment_countdown(num);
            snprintf(reply_detail, sizeof(reply_detail), "SEGMENT displaying %d and counting down", num);
        } 
        else if (strcmp(mesg, "9") == 0) {
            if (dyn_segment_stop) dyn_segment_stop();
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
    
    if (dyn_led_off) dyn_led_off();
    if (dyn_pwm_val) *dyn_pwm_val = 255;
    if (dyn_buzzer_off) dyn_buzzer_off();
    if (dyn_segment_stop) dyn_segment_stop();

    if (dyn_current_client_sock && *dyn_current_client_sock == csock) {
        if (dyn_sensor_off) dyn_sensor_off();
    }

    write_log("◀ [DISCONNECTED] Client connection closed. (IP: %s, Port: %d)", cli_ip, cli_port);

    close(csock);
    return NULL;
}
