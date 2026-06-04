CC = gcc
CFLAGS = -Wall -O2
WIRINGPI = -lwiringPi
PTHREAD = -lpthread

# 2. 경로 기본 정의
CODE_DIR = code
EXEC_DIR = exec
LIB_OUT_DIR = $(EXEC_DIR)/lib

# 3. 장치별 소스 폴더 경로
LED_DIR     = $(CODE_DIR)/led
BUZZER_DIR  = $(CODE_DIR)/buzzer
SENSOR_DIR  = $(CODE_DIR)/sensor
SEGMENT_DIR = $(CODE_DIR)/segment

# 4. 최종 생성될 실행 파일 위치 (exec/ 밑으로 지정)
SERVER_TARGET = $(EXEC_DIR)/server
CLIENT_TARGET = $(EXEC_DIR)/client

# 5. 생성될 동적 라이브러리 목록 (exec/lib/ 밑으로 지정)
LIBS = $(LIB_OUT_DIR)/libled.so \
       $(LIB_OUT_DIR)/libbuzzer.so \
       $(LIB_OUT_DIR)/libsensor.so \
       $(LIB_OUT_DIR)/libsegment.so

# 6. 기본 실행 규칙 (출력 디렉터리 자동 생성 포함)
all: create_dirs $(LIBS) $(SERVER_TARGET) $(CLIENT_TARGET)

create_dirs:
	@mkdir -p $(EXEC_DIR) $(LIB_OUT_DIR)

# 7. 장치별 동적 라이브러리(.so) 빌드 규칙 (exec/lib/ 에 저장)
$(LIB_OUT_DIR)/libled.so: $(LED_DIR)/led.c $(LED_DIR)/led.h
	$(CC) -fPIC -shared -o $@ $(LED_DIR)/led.c -I$(LED_DIR) $(WIRINGPI)

$(LIB_OUT_DIR)/libbuzzer.so: $(BUZZER_DIR)/buzzer.c $(BUZZER_DIR)/buzzer.h
	$(CC) -fPIC -shared -o $@ $(BUZZER_DIR)/buzzer.c -I$(BUZZER_DIR) $(WIRINGPI) $(PTHREAD)

$(LIB_OUT_DIR)/libsensor.so: $(SENSOR_DIR)/sensor.c $(SENSOR_DIR)/sensor.h
	$(CC) -fPIC -shared -o $@ $(SENSOR_DIR)/sensor.c -I$(SENSOR_DIR) -I$(LED_DIR) $(WIRINGPI)

$(LIB_OUT_DIR)/libsegment.so: $(SEGMENT_DIR)/segment.c $(SEGMENT_DIR)/segment.h
	$(CC) -fPIC -shared -o $@ $(SEGMENT_DIR)/segment.c -I$(SEGMENT_DIR) -I$(BUZZER_DIR) $(WIRINGPI) $(PTHREAD)

# 8. 메인 서버 빌드 규칙 (exec/server 생성)
$(SERVER_TARGET): $(CODE_DIR)/server/server.c $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(CODE_DIR)/server/server.c \
		-I$(LED_DIR) -I$(BUZZER_DIR) -I$(SENSOR_DIR) -I$(SEGMENT_DIR) \
		-L$(LIB_OUT_DIR) -lled -lbuzzer -lsensor -lsegment $(PTHREAD) $(WIRINGPI) -Wl,-rpath,'$$ORIGIN/lib'

# 9. 클라이언트 빌드 규칙 (exec/client 생성)
$(CLIENT_TARGET): $(CODE_DIR)/client/client.c
	$(CC) $(CFLAGS) -o $@ $< $(PTHREAD)

# 10. 정리 규칙
clean:
	rm -rf $(EXEC_DIR)
