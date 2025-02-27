#define OLED // убрать комментарий для OLED экрана
#define DEBUG_ENABLE // включение-выключение порта отладки

#include <Wire.h>
#include <Adafruit_BME280.h>

#ifdef OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledYes = false;
#endif

#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.println(x);
#else
#define DEBUG(x)
#endif

#include <WiFiManager.h>

#define BME280_I2C_ADDRESS  0x76
Adafruit_BME280 bmp; // I2C

#define postingInterval  300000 // интервал между отправками данных в миллисекундах (5 минут)
unsigned long lastConnectionTime = 0;           // время последней передачи данных
String HostName; //имя железки - выглядит как ESPAABBCCDDEEFF т.е. ESP+mac адрес.


void wifimanstart() { // Волшебная процедура начального подключения к Wifi.
  // Если не знает к чему подцепить - создает точку доступа ESP8266 и настроечную таблицу http://192.168.4.1
  // Подробнее: https://github.com/tzapu/WiFiManager
#ifdef OLED
  if (oledYes) {
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay(); // clear display
    display.setTextSize(1); // Draw 1X-scale text
    display.setCursor(0, 0);
    display.println("AutoConnect WiFi"); // text to display
    display.display();
  }
#endif
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setMinimumSignalQuality();
  if (!wifiManager.autoConnect("ESP8266")) {
    DEBUG("failed to connect and hit timeout");
#ifdef OLED
    if (oledYes) {
      display.println("failed to connect"); // text to display
      display.println("and hit timeout"); // text to display
      display.display();
    }
#endif
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  DEBUG("connected...");
#ifdef OLED
  if (oledYes) {
    display.println("connected..."); // text to display
  }
#endif
}


void setup() {

#ifdef DEBUG_ENABLE
  Serial.begin(9600);
#endif
  DEBUG(F("MeteoTest by HIAA (ESP8266)"));

  Wire.begin(D2, D1);

  while (bmp.begin(BME280_I2C_ADDRESS) == 0)
  {
    delay(1000);
#ifdef DEBUG_ENABLE
    Serial.print(".");
#endif
  }

  bmp.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X2,  // temperature
                  Adafruit_BME280::SAMPLING_X16, // pressure
                  Adafruit_BME280::SAMPLING_X1,  // humidity
                  Adafruit_BME280::FILTER_X16,
                  Adafruit_BME280::STANDBY_MS_1000 );
#ifdef OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    DEBUG(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  } else {
    oledYes = true;
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay(); // clear display
#endif

  HostName = "ESP" + WiFi.macAddress();
  HostName.replace(":", "");
  WiFi.hostname(HostName);
  wifimanstart();
  DEBUG(WiFi.localIP());
  DEBUG(WiFi.macAddress());
  DEBUG("Narodmon ID: " + HostName);
  lastConnectionTime = millis() - postingInterval + 15000; //первая передача на народный мониторинг через 15 сек.
#ifdef OLED
  display.setTextSize(1); // Draw 1X-scale text
  display.setCursor(0, 0);
  display.println("HostName"); // text to display
  display.setTextSize(1); // Draw 1X-scale text
  display.println(HostName); // text to display
  display.setTextSize(1); // Draw 1X-scale text
  display.println("LocalIP"); // text to display
  display.setTextSize(1); // Draw 1X-scale text
  display.println(WiFi.localIP()); // text to display
  display.display();
  delay(5000);
#endif
}


bool SendToNarodmon() { // Собственно формирование пакета и отправка.
  WiFiClient client;

#ifdef OLED
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay(); // clear display
  display.setTextSize(1); // Draw 1X-scale text
  oledDisplayCenter(F("sending data..."), 32);
  display.display();
  delay(2000);
  display.clearDisplay();
#endif

  String buf;
  float temp = bmp.readTemperature();
  float humidity = bmp.readHumidity();
  float dewpoint = temp - ((1 - (humidity / 100)) / 0.05);
  buf = "#" + WiFi.macAddress() + "\n"; // заголовок
  //buf = buf + "#LAT#54.958609\n"; // владелец
  //buf = buf + "#LON#73.411725\n"; // владелец

  buf = buf + "#TEMPC#" + String(temp) + "#Температура BME280\n"; //показания температуры
  buf = buf + "#HPA#" + String(bmp.readPressure() / 133.3) + "#Давление BME280\n"; //показания давления
  buf = buf + "#RH#" + String(humidity) + "#Влажность BME280\n"; //показания влажности
  buf = buf + "#DP#" + String(dewpoint) + "#Точка росы\n"; //точка росы
  buf = buf + "#DBM#" + String(WiFi.RSSI()) + "#Сигнал WiFi " + String(WiFi.SSID()) + "\n"; //показания сигнала WiFi
  buf = buf + "##\n"; //окончание передачи

  if (!client.connect("narodmon.ru", 8283)) { // попытка подключения
    DEBUG("connection failed");
    return false; // не удалось;
  } else
  {
    client.print(buf); // и отправляем данные
    DEBUG(buf);
    while (client.available()) {
      String line = client.readStringUntil('\r'); // если что-то в ответ будет - все в Serial
      DEBUG(line);
    }
  }
  return true; //ушло
}

#ifdef OLED
void oledDisplayCenter(String text, int currentY) {
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;
  display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  // display on horizontal
  display.setCursor((SCREEN_WIDTH - width) / 2, currentY);
  display.print(text); // text to display
}

void showData1() {
  float temp     = bmp.readTemperature();   // get temperature
  float humidity = bmp.readHumidity();
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay(); // clear display
  display.setTextSize(1); // Draw 1X-scale text
  oledDisplayCenter(F("Temp Celsius"), 0);
  display.setTextSize(2); // Draw 2X-scale text
  oledDisplayCenter(String(temp), 9);
  display.setTextSize(1); // Draw 1X-scale text
  oledDisplayCenter(F("Humidity %"), 32);
  display.setTextSize(2); // Draw 2X-scale text
  oledDisplayCenter(String(humidity), 41);
  display.display();
  delay(1500);
}
void showData2() {

  float pressure = bmp.readPressure();      // get pressure
  float altitude = bmp.readAltitude(1013.25);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay(); // clear display
  display.setTextSize(1); // Draw 1X-scale text
  oledDisplayCenter(F("Pressure mmHg"), 0);
  display.setTextSize(2); // Draw 2X-scale text
  oledDisplayCenter(String(pressure / 133.3), 9);
  display.setTextSize(1); // Draw 1X-scale text
  oledDisplayCenter(F("Altitude meter"), 32);
  display.setTextSize(2); // Draw 2X-scale text
  oledDisplayCenter(String(altitude), 41);
  display.display();
  delay(1500);
}
#endif

void loop() {
#ifdef OLED
      showData1();
      showData2();
#endif

  if (millis() - lastConnectionTime > postingInterval) { // ждем 5 минут и отправляем
    if (WiFi.status() == WL_CONNECTED) { // ну конечно если подключены
      if (SendToNarodmon()) {
        lastConnectionTime = millis();
      } else {
        lastConnectionTime = millis() - postingInterval + 15000;  //следующая попытка через 15 сек
      }
    } else {
      lastConnectionTime = millis() - postingInterval + 15000;  //следующая попытка через 15 сек
      DEBUG("Not connected to WiFi");
    }
  }  yield(); // что за команда - фиг знает, но ESP работает с ней стабильно и не глючит.
}
