#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TCP_PORT 5100

void print_menu() {
    printf("\n\n[ Device Control Menu ]\n");
    printf("1. LED ON          2. LED OFF          3. Set Brightness\n");
    printf("4. BUZZER ON       5. BUZZER OFF       6. SENSOR ON\n");
    printf("7. SENSOR OFF      8. SEGMENT DISPLAY  9. SEGMENT STOP\n");
    printf("0. Exit\n");
    printf("Select: ");
    fflush(stdout); // 메뉴가 화면에 즉시 밀려 나오도록 버퍼 비우기
}

void *receive_thread(void *arg) {
    int ssock = *((int *)arg);
    char mesg[BUFSIZ];

    while (1) {
        memset(mesg, 0, BUFSIZ);
        int n = recv(ssock, mesg, BUFSIZ - 1, 0);
        if (n <= 0) {
            printf("\n[Notice] Connection lost. Exiting program.\n");
            exit(0); // 수신 끊기면 클라이언트 전체 종료
        }
        mesg[n] = '\0';
        mesg[strcspn(mesg, "\r\n")] = '\0';

        if (strncmp(mesg, "[DATA]", 6) == 0) {
            printf("\n%s   ", mesg);
            fflush(stdout);
        }
        else {
            printf("\n%s", mesg);
            fflush(stdout);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int ssock;
    struct sockaddr_in servaddr;
    int choice;
    pthread_t r_thread;

    if(argc < 2) {
        printf("Usage : %s IP_ADDRESS\n", argv[0]);
        return -1;
    }

    if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &(servaddr.sin_addr.s_addr));
    servaddr.sin_port = htons(TCP_PORT);

    if(connect(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect()");
        return -1;
    }
    printf("Server Connected.\n");
    
    if (pthread_create(&r_thread, NULL, receive_thread, (void *)&ssock) != 0) {
        perror("Failed to create receive thread");
        close(ssock);
        return -1;
    }
    pthread_detach(r_thread);

    while(1) {
        print_menu();

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("[Warning] Please enter numbers only.\n");
            continue;
        }
        while (getchar() != '\n');

        char cmd[BUFSIZ] = "";

        switch(choice) {
            case 1: strcpy(cmd, "1"); break;
            case 2: strcpy(cmd, "2"); break;
            case 3:
                printf("Select Brightness Level\n");
                printf("1: MIN (33%%)   2: MID (66%%)   3: MAX (100%%)\n");
                printf("Select Level: ");
                fflush(stdout);

                char bright_val[10];
                if (fgets(bright_val, sizeof(bright_val), stdin) != NULL) {
                    bright_val[strcspn(bright_val, "\n")] = '\0';
                    snprintf(cmd, sizeof(cmd), "3 %s", bright_val);
                }
                break;
            case 4:
                printf("\nSelect Melody\n");
                printf("1: School Bell(학교종)   2: Airplane(비행기)   3: Jingle Bells(징글벨)\n");
                printf("Select Song: ");
                fflush(stdout);

                char song_val[10];
                if (fgets(song_val, sizeof(song_val), stdin) != NULL) {
                    song_val[strcspn(song_val, "\n")] = '\0';

                    int s_idx = atoi(song_val) - 1;
                    if (strlen(song_val) > 0 && s_idx >= 0 && s_idx <= 2) {
                        snprintf(cmd, sizeof(cmd), "4 %d", s_idx);
                    } else {
                        printf("[Warning] Invalid song selection. Defaulting to Butterfly.\n");
                        strcpy(cmd, "4 0");
                    }
                }
                break;
            case 5: strcpy(cmd, "5"); break;
            case 6: strcpy(cmd, "6"); break; // SENSOR ON 명령 전송
            case 7: strcpy(cmd, "7"); break; // SENSOR OFF 명령 전송
            case 8:
                {
                    int valid_input = 0;
                    char seg_val[10];
                    int input_num;

                    while (!valid_input) {
                        printf("Enter a number to display (0~9): ");
                        fflush(stdout);

                        if (fgets(seg_val, sizeof(seg_val), stdin) != NULL) {
                            seg_val[strcspn(seg_val, "\n")] = '\0';

                            if (strlen(seg_val) == 0) {
                                printf("[Warning] Input cannot be empty. Please try again.\n");
                                continue;
                            }

                            if (strlen(seg_val) != 1 || seg_val[0] < '0' || seg_val[0] > '9') {
                                printf("[Warning] Invalid input! Please enter a single digit between 0 and 9.\n");
                                continue;
                            }

                            input_num = atoi(seg_val);
                            if (input_num >= 0 && input_num <= 9) {
                                valid_input = 1; // 올바른 입력이므로 루프 탈출 조건 충족
                                snprintf(cmd, sizeof(cmd), "8 %s", seg_val);
                            } else {
                                printf("[Warning] Out of range! Please enter a number between 0 and 9.\n");
                            }
                        }
                    }
                }
                break;
            case 9: strcpy(cmd, "9"); break;
            case 0: strcpy(cmd, "exit"); break;
            default:
                printf("[Notice] Invalid number.\n");
                continue;
        }

        // 서버로 커맨드 쏘기
        if(send(ssock, cmd, strlen(cmd), 0) <= 0) {
            perror("send()");
            break;
        }

        if (choice == 0) {
            break; // 0번 누르면 루프 탈출 후 종료
        }

        usleep(100000); // UI 혼선을 줄이기 위한 미세한 대기
    }

    close(ssock);
    printf("Exiting client.\n");

    return 0;
}
