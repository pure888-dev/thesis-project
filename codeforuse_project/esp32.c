#include <WiFi.h>
#include <Servo.h>
#include <HTTPClient.h>

// Ultrasonic Sensor
const int trigPin = 5;
const int echoPin = 18;

// Servo Motor
Servo servo;
const int servoPin = 19;

// Switch Button
const int buttonPin = 23;

// LED
const int ledRed = 15;
const int ledYellow = 2;
const int ledGreen = 4;

// WiFi & Server
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverUrl = "http://your-server.com/api/upload_photo.php"; // อัปโหลดภาพ
const char* telegramBotToken = "BOT:TOKEN";
const char* chatID = "YOUR_CHAT_ID";

// ฟังก์ชันวัดระยะจาก Ultrasonic
long readDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledRed, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledGreen, OUTPUT);

  servo.attach(servoPin);
  servo.write(0);  // ปิดฝา

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected.");
}

void loop() {
  long distance = readDistanceCM();

  // ระยะที่บอกว่ามีพัสดุอยู่ (เช่น น้อยกว่า 10 ซม.)
  if (distance < 10) {
    digitalWrite(ledRed, HIGH);    // พัสดุเต็ม
    digitalWrite(ledYellow, LOW);
    digitalWrite(ledGreen, LOW);
  } else {
    digitalWrite(ledRed, LOW);
    digitalWrite(ledYellow, HIGH); // ยังวางพัสดุได้
    digitalWrite(ledGreen, LOW);
  }

  if (digitalRead(buttonPin) == LOW) {
    openBox();
  }

  delay(1000);
}

void openBox() {
  servo.write(90); // เปิดฝา
  delay(30000);    // เปิด 30 วินาที
  servo.write(0);  // ปิดฝา

  sendTelegramNotification();
  captureAndUploadPhoto();
}

void sendTelegramNotification() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=📦 กล่องเปิดเพื่อรับพัสดุแล้ว!";
    http.begin(url);
    int httpCode = http.GET();
    http.end();
  }
}

void captureAndUploadPhoto() {
  // ส่วนนี้ทำงานบน ESP32-CAM (โปรแกรมแยกต่างหาก)
}
