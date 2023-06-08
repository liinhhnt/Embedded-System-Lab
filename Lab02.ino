#include "RTClib.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_SSD1306.h>
#include <string>
#include <EEPROM.h>

// EEPROM
#define EEPROM_SIZE 512
#define EEPROM_START 0

// Screen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Temp
#define ONE_WIRE_BUS D6

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Time
RTC_DS1307 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

unsigned long previousMillis = 0;
const long interval = 5000;
int address = 0;
int dataCount = 0;
bool isRunning = false; // trạng thái hoạt động của chương trình
void setupScreen()
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
}

void setupRTCDateTime()
{
#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void setup () {
  Serial.begin(9600);

  EEPROM.begin(EEPROM_SIZE);

  // Start up the library for temperature
  sensors.begin();

  setupScreen();

  setupRTCDateTime();
}

String getDate()
{
  String result;
  DateTime now = rtc.now();

  result += String(now.year(), DEC);
  result += '/';
  result += String(now.month(), DEC);
  result += '/';
  result += String(now.day(), DEC);
  result += " (";
  result += String(daysOfTheWeek[now.dayOfTheWeek()]);
  result += ")\n";
  result += String(now.hour(), DEC);
  result += ':';
  result += String(now.minute(), DEC);
  result += ':';
  result += String(now.second(), DEC);
  result += '\n';

  return result;
}

String getTemp()
{
  String result;
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC = sensors.getTempCByIndex(0);

  // Check if reading was successful
  if (tempC != DEVICE_DISCONNECTED_C)
  {
    result = "Temperature is:\n" + String(tempC) + " *C\n";
  }
  else
  {
    result = "Error: Could not read temperature data";
  }

  return result;
}

void printDateTemp()
{
  display.clearDisplay();
  display.setCursor(0, 0);
  String currentDate = getDate();
  String currentTemp = getTemp();
  display.println(currentTemp);
  display.println(currentDate);
  display.display();
}

void saveData()
{
  // Kiểm tra nếu đã đạt đến số lượng cặp giá trị tối đa (10)
  if (dataCount >= 10)
  {
    // Dời các giá trị cũ trong EEPROM để lưu dữ liệu mới
    for (int i = 0; i < 9; i++)
    {
      int sourceAddress = (i + 1) * (sizeof(float) + sizeof(unsigned long));
      int destAddress = (i) * (sizeof(float) + sizeof(unsigned long));

      float tempC;
      long timestamp;

      // Đọc giá trị nhiệt độ từ EEPROM
      EEPROM.get(sourceAddress, tempC);

      // Đọc giá trị thời gian từ EEPROM
      EEPROM.get(sourceAddress + sizeof(float), timestamp);

      // Lưu giá trị nhiệt độ vào EEPROM
      EEPROM.put(destAddress, tempC);

      // Lưu giá trị thời gian vào EEPROM
      EEPROM.put(destAddress + sizeof(float), timestamp);
    }
  }
  else
  {
    // Tăng số lượng cặp giá trị đã lưu
    dataCount++;
  }

  // Lấy giá trị nhiệt độ từ hàm getTemp()
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // Lấy giá trị thời gian từ hàm getDate()
  DateTime now = rtc.now();

  // Lưu giá trị nhiệt độ vào EEPROM
  int tempCAddress = (dataCount - 1) * (sizeof(float) + sizeof(unsigned long));
  EEPROM.put(tempCAddress, tempC);

  // Lưu giá trị thời gian vào EEPROM
  int dateAddress = tempCAddress + sizeof(float);
  EEPROM.put(dateAddress, now.unixtime());

  // Ghi dữ liệu vào EEPROM
  EEPROM.commit();
}



void readData()
{
  Serial.println("Reading data from EEPROM:");

  for (int i = 0; i < dataCount; i++)
  {
    int tempCAddress = i * (sizeof(float) + sizeof(unsigned long));
    int dateAddress = tempCAddress + sizeof(float);

    float tempC;
    unsigned long timestamp;

    // Đọc giá trị nhiệt độ từ EEPROM
    EEPROM.get(tempCAddress, tempC);

    // Đọc giá trị thời gian từ EEPROM
    EEPROM.get(dateAddress, timestamp);

    // Chuyển đổi thời gian từ timestamp thành DateTime
    DateTime dateTime = DateTime(timestamp);

    // In giá trị nhiệt độ và thời gian lên Serial Monitor
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.print(" °C, Time: ");
    Serial.println(dateTime.timestamp());
  }
}

void process() {
  // Gửi liên tục giá trị nhiệt độ và thời gian về PC
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    saveData(); // lưu cặp giá trị nhiệt độ - thời gian vào EEPROM
    previousMillis = currentMillis;
    printDateTemp();
  }
}

void loop () {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Xóa ký tự xuống dòng và khoảng trắng thừa
    if (command == "STAT") isRunning = true;
    else if (command == "STOP") isRunning = false;
    else if (command == "READ") {
      // Lấy 10 dữ liệu gần nhất từ EEPROM và gửi về PC
      readData();
    }
    else Serial.println("Invalid command");
  }
  if (isRunning) process();
//  unsigned long currentMillis = millis();
//  if (currentMillis - previousMillis >= interval) {
//    saveData();
//    previousMillis = currentMillis;
//    printDateTemp();
//    readData();
//  }
}
