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

# 4. 최종 생성될 실행 파일 위치
SERVER_TARGET = $(EXEC_DIR)/server
CLIENT_TARGET = $(EXEC_DIR)/client

# 5. 생성될 동적 라이브러리 목록
LIBS = $(LIB_OUT_DIR)/libled.so \
       $(LIB_OUT_DIR)/libbuzzer.so \
       $(LIB_OUT_DIR)/libsensor.so \
       $(LIB_OUT_DIR)/libsegment.so

# 6. 기본 실행 규칙 (all 호출 시 라이브러리와 서버, 클라이언트 모두 빌드)
all: create_dirs $(LIBS) $(SERVER_TARGET) $(CLIENT_TARGET)

create_dirs:
	@mkdir -p $(EXEC_DIR) $(LIB_OUT_DIR)

# 7. 장치별 동적 라이브러리(.so) 빌드 규칙 (기존과 동일)
$(LIB_OUT_DIR)/libled.so: $(LED_DIR)/led.c $(LED_DIR)/led.h
	$(CC) -fPIC -shared -o $@ $(LED_DIR)/led.c -I$(LED_DIR) $(WIRINGPI)

$(LIB_OUT_DIR)/libbuzzer.so: $(BUZZER_DIR)/buzzer.c $(BUZZER_DIR)/buzzer.h
	$(CC) -fPIC -shared -o $@ $(BUZZER_DIR)/buzzer.c -I$(BUZZER_DIR) $(WIRINGPI) $(PTHREAD)

$(LIB_OUT_DIR)/libsensor.so: $(SENSOR_DIR)/sensor.c $(SENSOR_DIR)/sensor.h
	$(CC) -fPIC -shared -o $@ $(SENSOR_DIR)/sensor.c -I$(SENSOR_DIR) -I$(LED_DIR) $(WIRINGPI)

$(LIB_OUT_DIR)/libsegment.so: $(SEGMENT_DIR)/segment.c $(SEGMENT_DIR)/segment.h
	$(CC) -fPIC -shared -o $@ $(SEGMENT_DIR)/segment.c -I$(SEGMENT_DIR) -I$(BUZZER_DIR) $(WIRINGPI) $(PTHREAD)

# 8. 메인 서버 빌드 규칙 🚀 [dlopen 방식으로 완전 변경]
# 의존성에서 $(LIBS)를 제거하여 서버와 라이브러리를 독립시킵니다.
$(SERVER_TARGET): $(CODE_DIR)/server/server.c
	$(CC) $(CFLAGS) -o $@ $(CODE_DIR)/server/server.c $(PTHREAD) $(WIRINGPI) -ldl

# 9. 클라이언트 빌드 규칙 (기존과 동일)
$(CLIENT_TARGET): $(CODE_DIR)/client/client.c
	$(CC) $(CFLAGS) -o $@ $< $(PTHREAD)

# 10. 정리 규칙
clean:
	rm -rf $(EXEC_DIR)
