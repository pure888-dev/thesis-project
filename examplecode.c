#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <EEPROM.h>

// WiFi Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Telegram Bot Configuration
const String BOT_TOKEN = "YOUR_BOT_TOKEN";
const String CHAT_ID = "YOUR_CHAT_ID";
const String telegram_api = "https://api.telegram.org/bot" + BOT_TOKEN;

// Pin Definitions
#define TRIG_PIN D1        // Ultrasonic Trigger
#define ECHO_PIN D2        // Ultrasonic Echo
#define PIR_PIN D3         // PIR Motion Sensor
#define HALL_PIN D4        // Hall Effect Sensor (กล่องเปิด/ปิด)
#define SERVO_PIN D5       // Servo Motor สำหรับล็อค
#define LED_RED D6         // LED แดง (มีพัสดุ)
#define LED_GREEN D7       // LED เขียว (พร้อมใช้งาน)
#define BUZZER_PIN D8      // Buzzer

// System Variables
Servo lockServo;
bool packageDetected = false;
bool boxOpened = false;
bool systemArmed = true;
unsigned long lastMotionTime = 0;
unsigned long lastNotificationTime = 0;
String currentOTP = "";
int failedAttempts = 0;

// Distance measurement
long duration;
int baseDistance = 0;  // ระยะทางเมื่อกล่องว่าง
int currentDistance = 0;
const int PACKAGE_THRESHOLD = 5; // ห่างจาก baseline 5cm = มีพัสดุ

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  
  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(HALL_PIN, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  lockServo.attach(SERVO_PIN);
  lockServo.write(0); // ล็อคเริ่มต้น
  
  // Connect to WiFi
  connectWiFi();
  
  // Calibrate baseline distance
  calibrateDistance();
  
  // Initialize system
  digitalWrite(LED_GREEN, HIGH);
  sendTelegramMessage("🟢 Smart Package Box System เริ่มทำงาน!\n📍 ตำแหน่ง: หน้าบ้าน\n⚡ สถานะ: พร้อมรับพัสดุ");
  
  Serial.println("Smart Package Box System Started!");
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  // Read sensors
  bool motionDetected = digitalRead(PIR_PIN);
  bool boxClosed = digitalRead(HALL_PIN); // LOW = ปิด, HIGH = เปิด
  currentDistance = measureDistance();
  
  // ตรวจจับการเข้าใกล้
  if (motionDetected && systemArmed) {
    handleMotionDetection();
  }
  
  // ตรวจจับการวางพัสดุ
  if (boxClosed && !packageDetected) {
    checkPackageDelivery();
  }
  
  // ตรวจจับการเปิดกล่อง
  if (!boxClosed && packageDetected) {
    handleBoxOpened();
  }
  
  // ตรวจจับการหยิบพัสดุ
  if (boxClosed && packageDetected && boxOpened) {
    checkPackagePickup();
  }
  
  // Check for Telegram commands
  checkTelegramMessages();
  
  delay(1000);
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

int measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;
  
  return distance;
}

void calibrateDistance() {
  Serial.println("Calibrating baseline distance...");
  long total = 0;
  for (int i = 0; i < 10; i++) {
    total += measureDistance();
    delay(100);
  }
  baseDistance = total / 10;
  Serial.println("Baseline distance: " + String(baseDistance) + " cm");
}

void handleMotionDetection() {
  unsigned long currentTime = millis();
  
  // ป้องกันการแจ้งเตือนซ้ำเร็วเกินไป
  if (currentTime - lastMotionTime > 30000) { // 30 วินาที
    lastMotionTime = currentTime;
    
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    tone(BUZZER_PIN, 1000, 500);
    
    sendTelegramMessage("🚶‍♂️ ตรวจพบการเข้าใกล้กล่องรับพัสดุ!\n⏰ เวลา: " + getCurrentTime() + "\n🔔 กรุณาเตรียมตัวรับการแจ้งเตือนการส่งพัสดุ");
    
    delay(2000);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
  }
}

void checkPackageDelivery() {
  int distanceDiff = baseDistance - currentDistance;
  
  if (distanceDiff >= PACKAGE_THRESHOLD) {
    packageDetected = true;
    boxOpened = false;
    
    // Lock the box
    lockServo.write(90); // ล็อค
    
    // Visual and audio feedback
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    tone(BUZZER_PIN, 2000, 1000);
    
    // Generate OTP
    currentOTP = generateOTP();
    
    String message = "📦 มีพัสดุส่งมาแล้ว!\n";
    message += "⏰ เวลา: " + getCurrentTime() + "\n";
    message += "📏 ระยะทางตรวจจับ: " + String(currentDistance) + " cm\n";
    message += "🔒 กล่องถูกล็อคอัตโนมัติแล้ว\n";
    message += "🔑 รหัสปลดล็อค: " + currentOTP + "\n";
    message += "💡 ส่งรหัสมาเพื่อปลดล็อคกล่อง";
    
    sendTelegramMessage(message);
    
    Serial.println("Package delivered! OTP: " + currentOTP);
  }
}

void handleBoxOpened() {
  if (!boxOpened) {
    boxOpened = true;
    sendTelegramMessage("🔓 กล่องถูกเปิดแล้ว\n⏰ เวลา: " + getCurrentTime() + "\n📋 กรุณาหยิบพัสดุของคุณ");
  }
}

void checkPackagePickup() {
  int distanceDiff = baseDistance - currentDistance;
  
  if (distanceDiff < PACKAGE_THRESHOLD) {
    packageDetected = false;
    boxOpened = false;
    currentOTP = "";
    failedAttempts = 0;
    
    // Unlock permanently until next package
    lockServo.write(0);
    
    // Reset LED status
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    tone(BUZZER_PIN, 3000, 500);
    
    sendTelegramMessage("✅ พัสดุถูกหยิบเรียบร้อยแล้ว!\n⏰ เวลา: " + getCurrentTime() + "\n🟢 ระบบพร้อมรับพัสดุถัดไป");
    
    Serial.println("Package picked up successfully!");
  }
}

String generateOTP() {
  String otp = "";
  for (int i = 0; i < 6; i++) {
    otp += String(random(0, 10));
  }
  return otp;
}

void checkTelegramMessages() {
  WiFiClientSecure client;
  client.setInsecure();
  
  if (client.connect("api.telegram.org", 443)) {
    String url = "/bot" + BOT_TOKEN + "/getUpdates?offset=-1";
    
    client.print("GET " + url + " HTTP/1.1\r\n");
    client.print("Host: api.telegram.org\r\n");
    client.print("Connection: close\r\n\r\n");
    
    String response = "";
    while (client.connected() || client.available()) {
      if (client.available()) {
        response += client.readString();
      }
    }
    
    if (response.indexOf("\"text\"") > 0) {
      processCommand(response);
    }
  }
}

void processCommand(String response) {
  // Extract message text (simplified parsing)
  int textStart = response.indexOf("\"text\":\"") + 8;
  int textEnd = response.indexOf("\"", textStart);
  String command = response.substring(textStart, textEnd);
  command.trim();
  
  Serial.println("Received command: " + command);
  
  if (command == "/status") {
    String status = "📊 สถานะระบบ Smart Package Box\n\n";
    status += "🔋 WiFi: เชื่อมต่อแล้ว (" + WiFi.localIP().toString() + ")\n";
    status += "📦 พัสดุ: " + (packageDetected ? "มีพัสดุในกล่อง" : "ไม่มีพัสดุ") + "\n";
    status += "🔒 ล็อค: " + (packageDetected ? "ล็อคอยู่" : "ปลดล็อคอยู่") + "\n";
    status += "📏 ระยะทาง: " + String(currentDistance) + " cm\n";
    status += "⏰ เวลาปัจจุบัน: " + getCurrentTime();
    
    sendTelegramMessage(status);
  }
  else if (command == "/unlock" && packageDetected) {
    sendTelegramMessage("🔑 กรุณาใส่รหัส OTP เพื่อปลดล็อค\nรหัสปัจจุบัน: " + currentOTP);
  }
  else if (command == currentOTP && packageDetected) {
    lockServo.write(0); // ปลดล็อค
    sendTelegramMessage("🔓 ปลดล็อคสำเร็จ!\nกล่องพร้อมเปิดได้แล้ว");
    tone(BUZZER_PIN, 3000, 200);
    delay(300);
    tone(BUZZER_PIN, 3000, 200);
  }
  else if (command.length() == 6 && packageDetected) {
    failedAttempts++;
    if (failedAttempts >= 3) {
      sendTelegramMessage("🚨 ใส่รหัสผิด 3 ครั้ง!\nระบบล็อคชั่วคราว 5 นาที\n📞 หากเป็นเจ้าของ กรุณาติดต่อ Admin");
      delay(300000); // ล็อค 5 นาที
      failedAttempts = 0;
    } else {
      sendTelegramMessage("❌ รหัส OTP ไม่ถูกต้อง\nเหลือโอกาส " + String(3 - failedAttempts) + " ครั้ง");
    }
  }
  else if (command == "/help") {
    String help = "📚 คำสั่งที่ใช้ได้:\n\n";
    help += "/status - ดูสถานะระบบ\n";
    help += "/unlock - ขอรหัสปลดล็อค\n";
    help += "ใส่รหัส OTP 6 หลัก - ปลดล็อคกล่อง\n";
    help += "/help - ดูคำสั่งทั้งหมด\n\n";
    help += "🔧 พัฒนาโดย: ทีมนักเรียน\n";
    help += "📅 Version: 1.0";
    
    sendTelegramMessage(help);
  }
}

void sendTelegramMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure();
  
  if (client.connect("api.telegram.org", 443)) {
    String url = "/bot" + BOT_TOKEN + "/sendMessage";
    String payload = "{\"chat_id\":\"" + CHAT_ID + "\",\"text\":\"" + message + "\"}";
    
    client.print("POST " + url + " HTTP/1.1\r\n");
    client.print("Host: api.telegram.org\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: " + String(payload.length()) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    client.print(payload);
    
    Serial.println("Message sent: " + message);
  }
}

String getCurrentTime() {
  // ใช้ NTP หรือ RTC ในการใช้งานจริง
  unsigned long currentMillis = millis();
  int seconds = (currentMillis / 1000) % 60;
  int minutes = (currentMillis / (1000 * 60)) % 60;
  int hours = (currentMillis / (1000 * 60 * 60)) % 24;
  
  return String(hours) + ":" + String(minutes) + ":" + String(seconds);
}
