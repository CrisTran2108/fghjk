// Thêm thư viện LCD_I2C
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

Servo servo; 

char* ssid = "Vuong Phuong Thao 2";
char* password = "thao210794";

IPAddress server(192, 168, 0, 80);    // the fix IP address of the server
WiFiClient client;

// Khai báo các chân nối với cảm biến, relay và button
#define SERVO_PIN 16 // Động cơ rèm tự động
#define RELAY_PIN 14 // digital 0
#define BUZZ_PIN 15
#define FAN_PIN 0
#define PIR_PIN 12 // digital 1
#define GAS_PIN 13
#define LIGHT_PIN A0 // analog A0
#define LIGHT_MIN1 50 // ngưỡng ánh sáng coi là tối để bật đèn, tùy chỉnh theo nhu cầu
#define LIGHT_MIN2 60 // ngưỡng ánh sáng coi là tối để mở rèm, tùy chỉnh theo nhu cầu
#define LIGHT_ON_DURATION 3000 // 3s, thời gian tắt đèn sau khi không còn chuyển động

#define DHT_PIN 2
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

byte degree[8] = {
  0B01110,
  0B01010,
  0B01110,
  0B00000,
  0B00000,
  0B00000,
  0B00000,
  0B00000
};

int lcdColumns = 16;
int lcdRows = 2;
int pos = 0; // Thiet lap vi tri ban dau cho dong co servo

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

int lightStatus = 0;
boolean relayStatus = false;
boolean gasStatus = false;
boolean buzzStatus = false;
boolean fanStatus = false;
boolean curtainStatus = false;
unsigned long lastMotionDetected = 0; // lưu thời gian lần cuối phát hiện chuyển động

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.begin(9600);
  dht.begin();
  // Thiết lập chế độ hoạt động của các chân
  pinMode(PIR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(DHT_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  servo.attach(SERVO_PIN);

  // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();

  lcd.print("Nhiet do: ");
  lcd.setCursor(0, 1);
  lcd.print("Do am: ");

  lcd.createChar(1, degree);
}

void loop() {
  // 1. hiển thị giá trị nhiệt độ, độ ẩm lên LCD
  float t = dht.readTemperature();
  float h = dht.readHumidity();


  Serial.print("Nhiet do:");
  Serial.println(t);
  Serial.print("Do am:");
  Serial.println(h);
  if (isnan(t) || isnan(h)) { // Kiểm tra xem thử việc đọc giá trị có bị thất bại hay không. Hàm isnan bạn xem tại đây http://arduino.vn/reference/isnan
  }
  else {
    lcd.setCursor(10, 0);
    lcd.print(round(t));
    lcd.write(1);
    lcd.print("C");

    lcd.setCursor(10, 1);
    lcd.print(round(h));
    lcd.print("%");
  }

  // 2. Đèn nhà vệ sinh sáng khi có người đi qua, sau 3s sẽ tự động tắt
  // Đọc trạng thái ánh sáng hiện tại từ quang trở và đổi ra %
  lightStatus = analogRead(A0) * 100 / 1024;
  Serial.println("Độ sáng môi trường");
  Serial.println(lightStatus);
  //  Serial.print("Trang thai anh sang hien tai: "); Serial.print(lightStatus); Serial.println("%");
  int motionDetected = digitalRead(PIR_PIN);
  if (motionDetected) {
    Serial.println("Phat hien chuyen dong");
    // Nếu trời tối hơn mức quy định và đèn đang tắt thì bật đèn
    if (lightStatus > LIGHT_MIN1 && !relayStatus) {
      Serial.println("Phat hien chuyen dong va troi toi, yeu cau bat den");
      digitalWrite(RELAY_PIN, HIGH);
      relayStatus = true;
    }
    lastMotionDetected = millis(); // Lưu thời gian lần cuối phát hiện chuyển động
  }
  // Nếu đèn đang bật, kiểm tra để tắt nếu không có chuyển động sau 1 thời gian quy định
  if (relayStatus) {
    unsigned long current = millis();
    if (current - lastMotionDetected > LIGHT_ON_DURATION) {
      Serial.println("Khong phat hien chuyen dong sau mot thoi gian, yeu cau tat den");
      digitalWrite(RELAY_PIN, LOW);
      relayStatus = false;
    }
  }

  // 3.Cảnh báo rò khí gas
  gasStatus = !digitalRead(GAS_PIN);
  Serial.println(digitalRead(13));
  Serial.println(gasStatus);
  if (gasStatus) {
    Serial.println("Có rò khí gas, bật báo động...");

    // khi phat hien có rò gas bật còi báo động, quạt, mở cửa sổ bếp
    digitalWrite(BUZZ_PIN, HIGH);
    digitalWrite(FAN_PIN, HIGH);
    fanStatus = true;
    buzzStatus = true;
    delay(3000);
  } else {
    Serial.println("Không phát hiện rò khí gas");
    digitalWrite(BUZZ_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);
    fanStatus = false;
    buzzStatus = false;
  }
  delay(2000);
  // 4. Rèm tự động đóng/mở theo độ sáng môi trường
  if (( lightStatus > LIGHT_MIN2) && (pos == 0)) // trời sáng
  {
    curtainStatus = false;
    for (pos = 0; pos < 180; pos += 1) // cho servo quay từ 0->179 độ
    { // mỗi bước của vòng lặp tăng 1 độ
      servo.write(pos);              // xuất tọa độ ra cho servo
      delay(15);                       // đợi 15 ms cho servo quay đến góc đó rồi tới bước tiếp theo
    }
    digitalWrite(SERVO_PIN, LOW);
  }
  if ((lightStatus <= LIGHT_MIN2) && (pos == 180)) // trời tối
  {
    curtainStatus = true;
    for (pos = 180; pos >= 1; pos -= 1) // cho servo quay từ 179-->0 độ
    {
      servo.write(pos);              // xuất tọa độ ra cho servo
      delay(15);                       // đợi 15 ms cho servo quay đến góc đó rồi tới bước tiếp theo
    }
    digitalWrite(SERVO_PIN, LOW);
  }

  // 5. Gửi dữ liệu tới NodeMCU trung tâm

  String json = "{\"temperature\":";
  json += dht.readTemperature();
  json += ",";
  json += "\"humidity\":";
  json += dht.readHumidity();
  json += ",";
  json += "\"gas_state\":";
  if (fanStatus) {
    json += "1"; // có rò khí gas
  }
  else {
    json += "0";
  }
  json += ",";
  json += "\"pir-state\":";
  if (relayStatus) {
    json += "1"; // bật đèn nhà tắm
  }
  else json += "0";
  json += "}";

  client.connect(server, 80);   // Connection to the server
  Serial.println(".");
  client.println(json);
  String answer = client.readStringUntil('\r');   // receives the answer from the sever
  Serial.println("from server: " + answer);
  client.flush();
  delay(2000);  
}
