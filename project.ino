#include <WiFi.h>
#include <WebServer.h>

// WiFi 설정
const char* ssid = "bssm_free";
const char* password = "bssm_free";

// 모터 핀
#define M1A_PIN 26
#define M1B_PIN 25

WebServer server(80);

int motorSpeed = 200;

// 점검 상태 관리
enum InspectionState {
  IDLE,
  FORWARD,
  STOP_PHASE,
  REVERSE,
  COMPLETE
};

InspectionState currentState = IDLE;
unsigned long stateStartTime = 0;

void setup() {
  Serial.begin(115200);
  
  // 모터 설정
  ledcAttach(M1A_PIN, 1000, 8);
  ledcAttach(M1B_PIN, 1000, 8);
  stopMotor();
  
  // WiFi 연결
  Serial.println("WiFi 연결 중...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi 연결 성공!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  
  // CORS 허용
  server.enableCORS(true);
  
  // API 엔드포인트
  server.on("/api/inspect", HTTP_GET, handleInspectRequest);
  server.on("/api/stop", HTTP_GET, handleStopRequest);
  server.on("/api/status", HTTP_GET, handleStatusRequest);
  
  server.begin();
  Serial.println("서버 시작!");
}

void loop() {
  server.handleClient();  // 웹 요청 처리
  updateInspection();     // 점검 프로세스 업데이트
}

// 모터 제어 함수
void stopMotor() {
  ledcWrite(M1A_PIN, 0);
  ledcWrite(M1B_PIN, 0);
}

void forwardMotor() {
  ledcWrite(M1A_PIN, motorSpeed);
  ledcWrite(M1B_PIN, 0);
}

void reverseMotor() {
  ledcWrite(M1A_PIN, 0);
  ledcWrite(M1B_PIN, motorSpeed);
}

// 점검 프로세스 업데이트 (Non-blocking)
void updateInspection() {
  if (currentState == IDLE || currentState == COMPLETE) {
    return;
  }
  
  unsigned long elapsed = millis() - stateStartTime;
  
  switch (currentState) {
    case FORWARD:
      if (elapsed >= 5000) {  // 5초 경과
        Serial.println("정방향 완료 → 정지");
        stopMotor();
        currentState = STOP_PHASE;
        stateStartTime = millis();
      }
      break;
      
    case STOP_PHASE:
      if (elapsed >= 3000) {  // 3초 경과
        Serial.println("정지 완료 → 역방향");
        reverseMotor();
        currentState = REVERSE;
        stateStartTime = millis();
      }
      break;
      
    case REVERSE:
      if (elapsed >= 5000) {  // 5초 경과
        Serial.println("역방향 완료 → 점검 종료");
        stopMotor();
        currentState = COMPLETE;
        // 1초 후 IDLE로 복귀
        delay(1000);
        currentState = IDLE;
      }
      break;
      
    default:
      break;
  }
}

// API: 점검 시작
void handleInspectRequest() {
  if (currentState != IDLE) {
    server.send(200, "application/json", 
      "{\"status\":\"error\",\"message\":\"already_running\"}");
    return;
  }
  
  Serial.println("점검 시작!");
  forwardMotor();
  currentState = FORWARD;
  stateStartTime = millis();
  
  server.send(200, "application/json", 
    "{\"status\":\"success\",\"message\":\"inspection_started\"}");
}

// API: 긴급 정지
void handleStopRequest() {
  Serial.println("긴급 정지!");
  stopMotor();
  currentState = IDLE;
  
  server.send(200, "application/json", 
    "{\"status\":\"success\",\"message\":\"stopped\"}");
}

// API: 상태 확인
void handleStatusRequest() {
  String state;
  
  switch (currentState) {
    case IDLE:
      state = "idle";
      break;
    case FORWARD:
      state = "forward";
      break;
    case STOP_PHASE:
      state = "paused";
      break;
    case REVERSE:
      state = "reverse";
      break;
    case COMPLETE:
      state = "complete";
      break;
    default:
      state = "unknown";
  }
  
  unsigned long elapsed = 0;
  if (currentState != IDLE && currentState != COMPLETE) {
    elapsed = millis() - stateStartTime;
  }
  
  String response = "{\"status\":\"" + state + "\",\"elapsed\":" + String(elapsed) + "}";
  server.send(200, "application/json", response);
}