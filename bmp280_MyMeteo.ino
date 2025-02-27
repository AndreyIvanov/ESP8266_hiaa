#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <WiFiManager.h>

//#define DEBUG_SEND

#define BMP280_I2C_ADDRESS  0x76

Adafruit_BMP280 bmp; // I2C



#define postingInterval  5*60*1000 // интервал между отправками данных в миллисекундах (5 минут)
unsigned long lastConnectionTime = 0;           // время последней передачи данных
String HostName; //имя железки - выглядит как ESPAABBCCDDEEFF т.е. ESP+mac адрес.
String MACDevice;


void wifimanstart() { // Волшебная процедура начального подключения к Wifi.
  // Если не знает к чему подцепить - создает точку доступа ESP8266 и настроечную таблицу http://192.168.4.1
  // Подробнее: https://github.com/tzapu/WiFiManager
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setMinimumSignalQuality();
  if (!wifiManager.autoConnect("ESP8266")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  Serial.println("connected...");
}

float getTemp(int cMeasure) {
  float avgTemp = 0;
  float t = 0;
  for (int i = 1; i <= cMeasure; i++) {
    t = bmp.readTemperature();
    Serial.println(t);
    avgTemp += t;
    delay(100);
  }
  return avgTemp / cMeasure;
}

float getPress(int cMeasure) {
  float avgPress = 0;
  int cSuccess = 0;
  float t = 0;
  for (int i = 1; i <= cMeasure; i++) {
    t = bmp.readPressure();
    Serial.println(t / 133.3);
    if ((t / 133.3) >= 630 && (t / 133.3) <= 820) {
      avgPress += t;
      cSuccess++;
    }
    delay(200);
  }
  if (cSucces >= 1) {
    return avgPress / cSuccess;
  } else {
    return 0;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("MeteoTest by HIAA (ESP8266, BMP280)"));
  pinMode(LED_BUILTIN, OUTPUT);
  Wire.begin(D2, D1);

  while (bmp.begin(BMP280_I2C_ADDRESS) == 0)
  {
    delay(1000);
    Serial.print(".");
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X16,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */


  HostName = "ESP" + WiFi.macAddress();
  HostName.replace(":", "");
  WiFi.hostname(HostName);
  MACDevice = WiFi.macAddress();
  wifimanstart();
  Serial.println(WiFi.localIP()); Serial.println(MACDevice); Serial.print("Narodmon ID: "); Serial.println(HostName);
  lastConnectionTime = millis() - postingInterval + 15000; //первая передача на народный мониторинг через 15 сек.
  delay(5000);
}


bool SendToNarodmon() { // Собственно формирование пакета и отправка.
  WiFiClient client;

  String buf;
  digitalWrite(LED_BUILTIN, LOW);

  buf = "#" + MACDevice + "\n"; // заголовок
  buf = buf + "#TEMPC#" + String(getTemp(10)) + "#Температура BMP280\n"; //показания температуры
  long pre = getPress(15);
  if (pre > 0) {
    buf = buf + "#PRESS#" + String( pre / 133.3) + "#Давление BMP280\n"; //показания давления
  }
  buf = buf + "#RSSI#" + String(WiFi.RSSI()) + "#Сигнал WiFi\n"; //показания сигнала WiFi
  buf = buf + "##\n"; //окончание передачи

  if (!client.connect("narodmon.ru", 8283)) { // попытка подключения
    Serial.println("connection failed");
    return false; // не удалось;
  } else
  {
    client.print(buf); // и отправляем данные
    Serial.print(buf);
    while (client.available()) {
      String line = client.readStringUntil('\r'); // если что-то в ответ будет - все в Serial
      Serial.print(line);
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);

  return true; //ушло
}


void loop() {


  if (millis() - lastConnectionTime > postingInterval) { // ждем 5 минут и отправляем
    if (WiFi.status() == WL_CONNECTED) { // ну конечно если подключены
      if (SendToNarodmon()) {
        lastConnectionTime = millis();
      } else {
        lastConnectionTime = millis() - postingInterval + 15000;  //следующая попытка через 15 сек
      }
    } else {
      lastConnectionTime = millis() - postingInterval + 15000;  //следующая попытка через 15 сек
      Serial.println("Not connected to WiFi");
    }
  }  yield(); // что за команда - фиг знает, но ESP работает с ней стабильно и не глючит.
}
