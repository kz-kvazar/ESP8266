#define USE_CLIENTSSL false

//#include <AsyncTelegram2.h>
#include <Adafruit_SleepyDog.h>
#include <ArduinoJson.h>

// Timezone definition
#include <time.h>
#define MYTZ "EET-2EEST,M3.5.0,M10.5.0/3"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266HTTPClient.h>
//BearSSL::WiFiClientSecure client;
//BearSSL::Session session;
//BearSSL::X509List certificate(telegram_cert);

#elif defined(ESP32)
#include <AsyncTCP.h>
#include <WiFi.h>
#include <WiFiClient.h>
#if USE_CLIENTSSL
#include <SSLClient.h>
#include "tg_certificate.h"
WiFiClient base_client;
SSLClient client(base_client, TAs, (size_t)TAs_NUM, A0, 1, SSLClient::SSL_ERROR);
#else
#include <WiFiClientSecure.h>
WiFiClientSecure client;
#endif
#endif

//AsyncTelegram2 myBot(client);

#include <NTPClient.h>
#include <WiFiUdp.h>


#include <UnixTime.h>

#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
//SET_LOOP_TASK_STACK_SIZE(16 * 1024);  // 16KB
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#elif __has_include(<WiFiNINA.h>)
#include <WiFiNINA.h>
#elif __has_include(<WiFi101.h>)
#include <WiFi101.h>
#elif __has_include(<WiFiS3.h>)
#include <WiFiS3.h>
#endif

#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

WiFiUDP udp;
UnixTime stamp(0);
// Устанавливаем поправку на часовой пояс для Украины (GMT+2)
NTPClient ntpClient(udp, "pool.ntp.org", 2 * 60 * 60);

const IPAddress serverIP(172, 30, 40, 50);     // IP-адрес сервера Modbus
const IPAddress registratorIP(10, 70, 0, 28);  // IP-адрес сервера Modbus
const IPAddress ktpIP(10, 70, 0, 27);

const int serverPort = 502;  // Порт Modbus
uint8_t transactionId = 0;   // Уникальный идентификатор транзакции
uint8_t transactionIdResponse = 0;

AsyncClient clientRegistrator;
AsyncClient clientKGY;
AsyncClient clientKTP;

// const char *ssid = "matrix";  // SSID WiFi network
// const char *pass = "777qaz777qaz";  // Password  WiFi network

const char *ssid = "TEDOMHOST";  // SSID WiFi network
const char *pass = "tedomhost";  // Password  WiFi network

// const char *ssid = "UMG-GUESTS";                                        // SSID WiFi network
// const char *pass = "C4aW2zWS";                                        // Password  WiFi network

// const char *token = "6604238506:AAE6ckDJVAkjN00sq1NMHzTXVMldnKyuq7A";  // Telegram token
// const char *channel = "-1001981116655";
// int64_t userid = 752684049;
#define DATABASE_URL "https://kgy-regulator-default-rtdb.europe-west1.firebasedatabase.app/"
#define DATABASE_SECRET "4cG7ag0EgtjdF92x2xKbymNP3wZcCpof2sMWeS05"

String resultRegistrator;
String resultKGY;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//unsigned long dataMillis = 0;


uint16_t powerActive = 9999;
float trottlePosition = 9999;
uint16_t powerConstant = 9999;
uint32_t totalGenerated = 0;
float CH4_1p = 9999;
float CH4_2p = 9999;
float CH4_KGY = 9999;
float wnding1Temp = 9999;
float wnding2Temp = 9999;
float wnding3Temp = 9999;
float bearing1Temp = 9999;
float bearing2Temp = 9999;
uint16_t l1N = 9999;
uint16_t l2N = 9999;
uint16_t l3N = 9999;
float gasTempIN = 9999;
float gasTempOUT = 9999;
float gtsPr = 9999;
float kgyPr = 9999;
float opPr = 9999;
int maxPower = 1560;
int appMaxPower = 1560;
int reg = 10;
uint64_t compareUnixTime = 0;
int day = 32;
int intPower = 0;
bool isPower = false;
long monthStartGenerated = 0;
int cleanOil = 0;
int avgTemp = 0;
float resTemp = 0;

int U12 = -255;
int U23 = -255;
int U31 = -255;


uint8_t request[12] = { 0 };
uint8_t requestWright[15] = { 0 };

int hours;
int currentHour;
//bool hourReport = false;
bool regulate = false;
bool skipFirst = false;
bool appRegulate = false;
bool firebase = true;
bool kgyLock = true;
bool ktpLock = true;
bool regLock = true;
bool isAlarm = false;
bool isStop = false;
bool engineIsRunning = false;
bool isMixError = false;
uint32_t epochTime = 0;
//String reset = "";  //String(ESP.getResetInfo()) + "\n";

const uint8_t LED_BUILTIN_ = 15;
const uint8_t resetPin = 5;

void setup() {
  pinMode(LED_BUILTIN_, OUTPUT);  // Настройка пина GPIO2 как выхода

  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, true);

  // initialize the //Serial
  Serial.begin(115200);

  delay(2000);  // wait for //Serial port to connect. Needed for native USB port only

  ////Serial.println("\nStarting TelegramBot...");

  WiFi.begin(ssid, pass);
  WiFi.mode(WIFI_STA);

  //delay(500);
  while (WiFi.status() != WL_CONNECTED) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        //Serial.println("[WiFi] SSID not found");
        break;
      case WL_CONNECT_FAILED:
        //Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
        return;
        break;
      case WL_CONNECTION_LOST:
        //Serial.println("[WiFi] Connection was lost");
        break;
      case WL_SCAN_COMPLETED:
        //Serial.println("[WiFi] Scan is completed");
        break;
      case WL_DISCONNECTED:
        //Serial.println("[WiFi] WiFi is disconnected");
        break;
      case WL_CONNECTED:
        //Serial.println("[WiFi] WiFi is connected!");
        //Serial.print("[WiFi] IP address: ");
        //Serial.println(WiFi.localIP());
        return;
        break;
      default:
        //Serial.print("[WiFi] WiFi Status: ");
        //Serial.println(WiFi.status());
        break;
    }
    digitalWrite(LED_BUILTIN_, true);
    //Serial.print('.');
    delay(3000);
    digitalWrite(LED_BUILTIN_, false);
    delay(3000);
  }

#ifdef ESP8266
  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  //Set certficate, session and some other base client properies
  //client.setSession(&session);
  //client.setTrustAnchors(&certificate);
  //client.setBufferSizes(1024, 1024);
#elif defined(ESP32)
  // Sync time with NTP
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#if USE_CLIENTSSL == false
  //client.setCACert(telegram_cert);
#endif
#endif

  // Set the Telegram bot properies
  // myBot.setUpdateTime(3000);
  // myBot.setTelegramToken(token);

  // Check if all things are ok
  // //Serial.print("\nTest Telegram connection... ");
  // myBot.begin() ? //Serial.println("OK") : //Serial.println("NOK");

  // char welcome_msg[48];
  // snprintf(welcome_msg, 128, "BOT @%s is online", myBot.getBotName());

  // // Send a message to specific user who has started your bot
  // myBot.sendTo(userid, welcome_msg);


  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(512 /* Rx buffer size in bytes from 512 - 16384 */, 512 /* Tx buffer size in bytes from 512 - 16384 */);
  Firebase.begin(&config, &auth);

  ntpClient.update();
  epochTime = ntpClient.getEpochTime();
  currentHour = (epochTime / 3600) % 24;
  hours = currentHour;

  digitalWrite(LED_BUILTIN_, false);
  int downMS = Watchdog.enable(22000);
}

void loop() {

  static uint32_t hourTime = millis();
  if (millis() - hourTime > 20000 && firebase /*&& hourReport*/) {
    blink();
    monthGenerated();
    pushToFirebase();
    firebaseReport();
    hourTime = millis();
  } else if (hourTime > millis()) {
    hourTime = millis();
    ntpClient.update();
    epochTime = ntpClient.getEpochTime();
  }

  static uint32_t ledTime = millis();
  if (millis() - ledTime > 1000 && (firebase || regulate || appRegulate)) {
    blink();
    Watchdog.reset();
    getDate();
    ledTime = millis();
  } else if (ledTime > millis()) {
    ledTime = millis();
    ntpClient.update();
    epochTime = ntpClient.getEpochTime();
  }

  if (engineIsRunning && powerActive == 0) {
    Serial.println("engine is stoped. sending report");
    WiFiClient client;
    HTTPClient http;
    http.begin(client, "http://worldtimeapi.org/api/timezone/Europe/Kiev");
    int httpCode = http.GET();
    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        String datetime = doc["datetime"].as<String>();
        if (datetime.length() > 13) {
          datetime = datetime.substring(0, datetime.length() - 13);
        } else {
          delay(30000);
          Serial.println("datetime.length() > 13 false");
        }
        engineIsRunning = false;
        Firebase.RTDB.setString(&fbdo, "now/engineStopTime/", datetime);
        Serial.println("now/engineStopTime/ sending!!!");
      } else {
        delay(30000);
        Serial.println("!error false");
      }
    } else {
      delay(30000);
      Serial.println("httpCode == HTTP_CODE_OK");
    }
    http.end();
  }
}

void blink() {
  digitalWrite(LED_BUILTIN_, false);
  delay(200);
  digitalWrite(LED_BUILTIN_, true);
}
void setPower(int power) {
  intPower = power;
  isPower = true;
}
bool checkValidData() {
  static uint32_t validTime = millis();

  if (trottlePosition > 100 || trottlePosition < -5 || powerConstant > 1560 || powerConstant < 0 || powerActive > 2000
      || powerActive < -300 || opPr > 40 || opPr < -5 || totalGenerated == 0 || cleanOil > 110 || cleanOil < -5 || avgTemp > 600 || avgTemp < -40
      || resTemp > 120 || resTemp < -40 || CH4_KGY > 110 || CH4_KGY < -10 || gasTempIN < -50 || gasTempIN > 900 || gasTempOUT < -50 || gasTempOUT > 900 || l1N > 600 || l1N < -10 || l2N > 600 || l2N < -10
      || l3N > 600 || l3N < -10 || wnding1Temp > 900 || wnding1Temp < -40 || wnding2Temp > 900 || wnding2Temp < -40 || wnding3Temp > 900 || wnding3Temp < -40
      || bearing1Temp > 900 || bearing1Temp < -40 || bearing2Temp > 900 || bearing2Temp < -40) {

    //Serial.println("\ntrottlePosition = " + String(trottlePosition) + "\npowerConstant = " + String(powerConstant) + "\npowerActive = " + String(powerActive) + "\nopPr = " + String(opPr) + "\ntotalGenerated = " + totalGenerated);
    return false;
  } else {
    return true;
  }
}
void regulatePower() {
  static uint32_t powerUpTime = millis();
  static long lastRegulate = millis();

  if (!checkValidData()) return;

  if (powerActive > 1 && opPr > 1 && !engineIsRunning) {
    engineIsRunning = true;
    Serial.println("engineIsRunning = true");
  }

  if (!regulate && !appRegulate) return;

  float wndAvg = wnding1Temp;
  if (wnding2Temp > wndAvg) wndAvg = wnding1Temp;
  if (wnding3Temp > wndAvg) wndAvg = wnding1Temp;

  if (powerActive > 0 && millis() > lastRegulate && millis() - lastRegulate > 20000) {
    (opPr > 7 && trottlePosition < 75 && CH4_KGY > 27.5f) ? reg = 20 : reg = 10;
    if ((opPr < 3 || avgTemp > 450 || avgTemp < 340 || trottlePosition > 96 || isMixError) && powerConstant != 810) {
      if (isMixError) {
        setPower(810);
      } else {
        (powerConstant > 1010) ? setPower(powerConstant - 200) : setPower(810);
      }
      lastRegulate = millis();
    } else if (opPr <= 4 || trottlePosition >= 90 || powerActive >= 1560 || powerConstant > maxPower || powerConstant > appMaxPower || resTemp >= 56.9f || wndAvg >= 72.9f || avgTemp < 380) {
      checkActPower();
      //checkThrottle();
      powerConstant > 820 ? setPower(powerConstant - 10) : setPower(810);
      lastRegulate = millis();

      if (resTemp >= 56.9f || wndAvg >= 72.9f) {
        Serial.println("millis = " + String(millis()));
        Serial.println("lastRegulate before =" + String(lastRegulate));
        lastRegulate = lastRegulate + 45000;
        Serial.println("lastRegulate after =" + String(lastRegulate));
      }

    } else if (opPr > 5 && ((powerConstant - powerActive) <= 50) && (maxPower - powerConstant >= reg) && trottlePosition < 90 && (appMaxPower - powerConstant >= reg) && resTemp < 56.4f && wndAvg < 72.7f && avgTemp > 395) {
      setPower(powerConstant + reg);
      lastRegulate = millis();
    } else if (opPr > 5 && trottlePosition < 80 && ((millis() - powerUpTime) >= 300000) && appMaxPower <= 1560 - reg) {
      appMaxPower += reg;
      powerUpTime = millis();
    }
  } else if (powerActive <= 0 && powerConstant != 800) {
    setPower(800);
    appMaxPower = 1560;
    maxPower = 1560;
    isStop = true;
  } else if (lastRegulate > millis() && lastRegulate - millis() > 50000) {
    Serial.println("reset timer = " + String(lastRegulate - millis() > 50000));
    Serial.println("lastRegulate - millis =" + String(lastRegulate - millis()));

    lastRegulate = millis();
    powerUpTime = millis();
  }
}
void checkActPower() {
  if (powerActive > 1560) {
    if (avgTemp > 375 && avgTemp < 425) {
      appMaxPower = powerConstant - 10;
    } else {
      appMaxPower -= 10;
    }
  }
}
void checkThrottle() {
  if (trottlePosition > 90 && trottlePosition < 96 && avgTemp > 370 && avgTemp < 425) appMaxPower = powerConstant - 10;
}
void getDate() {
  if (kgyLock == true) {
    sendKGYRequest();
    ////Serial.println("\nsendKGYRequest...");
  }
  if (regLock == true) {
    sendRegistratorRequest();
    ////Serial.println("\nsendRegistratorRequest...");
  }
  if (ktpLock == true) {
    sendKtpRequest();
  }
  if (avgTemp > 1000 || avgTemp < -40 || avgTemp == 0 || getTime() % 2 != 0) return;

  Firebase.RTDB.setInt(&fbdo, "avgTemp/" + String(0), avgTemp);
  Firebase.RTDB.setInt(&fbdo, "avgTemp/time", getTime());

  if (isStop) {
    isStop = false;
    //myBot.sendToChannel(channel, "КГУ остановленно!! \nТак и задумано?", true);
    Firebase.RTDB.setBool(&fbdo, "now/alarm", true);  //отправляет аларм на сервер если машина остановилась
    delay(2000);
    Firebase.RTDB.setBool(&fbdo, "now/alarm", false);
  }
}
void sendKtpRequest() {
  if (!clientKTP.connected() && !clientKTP.connect(ktpIP, serverPort)) {
    //Serial.println("\nclientRegistrator.connect ERROR...");
    resultRegistrator = "Ошибка соединения с КТП-10кВ !!!";
    return;
  } else {
    ktpLock = false;
    //Serial.println("\nclientRegistrator.onElseConnect...");
    transactionId = 23;
    //int startRegister = 402014;
    //int quantity = 3;

    request[0] = (transactionId >> 8);    // Старший байт Transaction ID
    request[1] = (transactionId & 0xFF);  // Младший байт Transaction ID
    request[2] = 0;
    request[3] = 0;  // Protocol ID (0 для Modbus TCP)
    request[4] = 0;
    request[5] = 6;                       // Длина данных
    request[6] = 1;                       // Адрес устройства Modbus
    request[7] = 3;                       // Код функции (чтение Holding Register)
    request[8] = (402014 - 400001) >> 8;  // Адрес старшего байта регистра
    request[9] = (402014 - 400001) & 0xFF;
    request[10] = 3 >> 8;    // Количество регистров (старший байт)
    request[11] = 3 & 0xFF;  // Количество регистров (младший байт)
    // Отправьте запрос
    clientKTP.write(reinterpret_cast<const char *>(request), sizeof(request));
  }
  // Создайте и отправьте запрос к регистратору
  clientKTP.onConnect([](void *arg, AsyncClient *c) {
    ktpLock = false;
    //Serial.println("\nclientRegistrator.onConnect...");
    // Убедитесь, что transactionId определен

    transactionId = 23;

    request[0] = (transactionId >> 8);    // Старший байт Transaction ID
    request[1] = (transactionId & 0xFF);  // Младший байт Transaction ID
    request[2] = 0;
    request[3] = 0;  // Protocol ID (0 для Modbus TCP)
    request[4] = 0;
    request[5] = 6;                       // Длина данных
    request[6] = 1;                       // Адрес устройства Modbus
    request[7] = 3;                       // Код функции (чтение Holding Register)
    request[8] = (402014 - 400001) >> 8;  // Адрес старшего байта регистра
    request[9] = (402014 - 400001) & 0xFF;
    request[10] = 3 >> 8;    // Количество регистров (старший байт)
    request[11] = 3 & 0xFF;  // Количество регистров (младший байт)
    // Отправьте запрос
    c->write(reinterpret_cast<const char *>(request), sizeof(request));
  });
  // Установите обработчик ответа от регистратора
  clientKTP.onData([](void *arg, AsyncClient *c, void *data, size_t len) {
    //Serial.println("\nclientRegistrator.onData...");
    uint8_t *response = static_cast<uint8_t *>(data);  // Приведение типа указателя
    // Проверьте, что в ответе достаточно данных перед извлечением
    if (len >= 8) {

      U12 = (response[9] << 8) | response[9 + 1];
      U23 = (response[9 + 1 * 2] << 8) | response[9 + 1 * 2 + 1];
      U31 = (response[9 + 2 * 2] << 8) | response[9 + 2 * 2 + 1];
    }
    //c->close(false);
    Serial.println("\nclientKtp.onData...");
    ktpLock = true;
  });
  clientKTP.onError([](void *arg, AsyncClient *c, int8_t error) {
    resultRegistrator = "Ошибка соединения с КТП-10кВ!!!";
    U12 = -255;
    U23 = -255;
    U31 = -255;
    Serial.println("\nclientKtp.onError ERROR...");
  });
  clientKTP.onDisconnect([](void *arg, AsyncClient *c) {
    ktpLock = true;
    Serial.println("\nclientKtp.onDisconect...");
  });
}

void sendRegistratorRequest() {
  if (!clientRegistrator.connected() && !clientRegistrator.connect(registratorIP, serverPort)) {
    //Serial.println("\nclientRegistrator.connect ERROR...");
    opPr = 9999;  // Установка значения в 9999 при ошибке соединения
    resultRegistrator = "Ошибка соединения с регистратором!!! \nРегулирование невозможно.";
    return;
  } else {
    regLock = false;
    //Serial.println("\nclientRegistrator.onElseConnect...");
    transactionId = 7;

    request[0] = (transactionId >> 8);    // Старший байт Transaction ID
    request[1] = (transactionId & 0xFF);  // Младший байт Transaction ID
    request[2] = 0;
    request[3] = 0;  // Protocol ID (0 для Modbus TCP)
    request[4] = 0;
    request[5] = 6;  // Длина данных
    request[6] = 1;  // Адрес устройства Modbus
    request[7] = 4;  // Код функции (чтение Holding Register)
    request[8] = 0;
    request[9] = 0;  // Старший и младший байты адреса регистра (0)
    request[10] = 0;
    request[11] = 12;  // Количество регистров для чтения (12)
    // Отправьте запрос
    clientRegistrator.write(reinterpret_cast<const char *>(request), sizeof(request));
  }
  // Создайте и отправьте запрос к регистратору
  clientRegistrator.onConnect([](void *arg, AsyncClient *c) {
    regLock = false;
    //Serial.println("\nclientRegistrator.onConnect...");
    // Убедитесь, что transactionId определен

    transactionId = 7;
    request[0] = (transactionId >> 8);    // Старший байт Transaction ID
    request[1] = (transactionId & 0xFF);  // Младший байт Transaction ID
    request[2] = 0;
    request[3] = 0;  // Protocol ID (0 для Modbus TCP)
    request[4] = 0;
    request[5] = 6;  // Длина данных
    request[6] = 1;  // Адрес устройства Modbus
    request[7] = 4;  // Код функции (чтение Holding Register)
    request[8] = 0;
    request[9] = 0;  // Старший и младший байты адреса регистра (0)
    request[10] = 0;
    request[11] = 12;  // Количество регистров для чтения (12)
    // Отправьте запрос
    c->write(reinterpret_cast<const char *>(request), sizeof(request));
  });
  // Установите обработчик ответа от регистратора
  clientRegistrator.onData([](void *arg, AsyncClient *c, void *data, size_t len) {
    Serial.println("\nclientRegistrator.onData...");
    uint8_t *response = static_cast<uint8_t *>(data);  // Приведение типа указателя
    // Проверьте, что в ответе достаточно данных перед извлечением
    if (len >= 32) {
      // Извлекаем данные из ответа правильным образом
      // uint32_t opPresher = ((uint32_t)response[9] << 24) | ((uint32_t)response[10] << 16) | ((uint32_t)response[11] << 8) | (uint32_t)response[12];
      // float floatOpPresher;
      // memcpy(&floatOpPresher, &opPresher, sizeof(floatOpPresher));
      // floatOpPresher = round(floatOpPresher * 10) / 10;
      // opPr = floatOpPresher;
      uint32_t opPresher = (static_cast<uint32_t>(response[9]) << 24) | (static_cast<uint32_t>(response[10]) << 16) | (static_cast<uint32_t>(response[11]) << 8) | static_cast<uint32_t>(response[12]);
      //opPr = round(*reinterpret_cast<float*>(&opPresher) * 100) / 100;
      memcpy(&opPr, &opPresher, sizeof(opPr));
      opPr = round(opPr * 100) / 100;

      uint32_t GTS = (static_cast<uint32_t>(response[17]) << 24) | (static_cast<uint32_t>(response[18]) << 16) | (static_cast<uint32_t>(response[19]) << 8) | static_cast<uint32_t>(response[20]);
      // float floatGTS;
      memcpy(&gtsPr, &GTS, sizeof(gtsPr));
      gtsPr = round(gtsPr * 100) / 100;
      //gtsPr = GTS / 10.0f;

      uint32_t KY = (static_cast<uint32_t>(response[21]) << 24) | (static_cast<uint32_t>(response[22]) << 16) | (static_cast<uint32_t>(response[23]) << 8) | static_cast<uint32_t>(response[24]);
      // float floatKY;
      memcpy(&kgyPr, &KY, sizeof(kgyPr));
      // kgyPr = round(floatKY * 10) / 10;
      kgyPr = round(kgyPr * 100) / 100;

      uint32_t CH4_1 = (static_cast<uint32_t>(response[25]) << 24) | (static_cast<uint32_t>(response[26]) << 16) | (static_cast<uint32_t>(response[27]) << 8) | static_cast<uint32_t>(response[28]);
      // float floatCH4_1;
      memcpy(&CH4_1p, &CH4_1, sizeof(CH4_1p));
      // CH4_1p = round(floatCH4_1 * 10) / 10;
      CH4_1p = round(CH4_1p * 100) / 100;

      uint32_t CH4_2 = (static_cast<uint32_t>(response[29]) << 24) | (static_cast<uint32_t>(response[30]) << 16) | (static_cast<uint32_t>(response[31]) << 8) | static_cast<uint32_t>(response[32]);
      // float floatCH4_2;
      memcpy(&CH4_2p, &CH4_2, sizeof(CH4_2p));
      // CH4_2p = round(floatCH4_2 * 10) / 10;
      CH4_2p = round(CH4_2p * 100) / 100;

      resultRegistrator = "Давление перед GTS: " + String(gtsPr) + " kPa\n";
      resultRegistrator += "Давление перед КГУ: " + String(kgyPr) + " kPa\n";
      resultRegistrator += "Давление после ОП: " + String(opPr) + " kPa\n";
      resultRegistrator += "СН4 ВНС-№1: " + String(CH4_1p) + " %\n";
      resultRegistrator += "СН4 ВНС-№2: " + String(CH4_2p) + " %\n";
    }
    //c->close(false);
    regLock = true;
  });
  clientRegistrator.onError([](void *arg, AsyncClient *c, int8_t error) {
    opPr = 9999;  // Установка значения в 9999 при ошибке соединения
    resultRegistrator = "Ошибка соединения с регистратором!!! \nРегулирование невозможно.";
    Serial.println("\nclientRegistrator.onError ERROR...");
  });
  clientRegistrator.onDisconnect([](void *arg, AsyncClient *c) {
    regLock = true;
    Serial.println("\nclientRegistrator.onDisconect...");
  });
}

void sendKGYRequest() {
  if (!clientKGY.connected() && !clientKGY.connect(serverIP, serverPort)) {
    //Serial.println("clientKGY.connect ERROR...");
    trottlePosition = 9999;
    powerConstant = 9999;
    powerActive = 9999;
    resultKGY = "Ошибка подключения к КГУ!!! \nРегулирование невозможно.";
    return;
  } else {
    //Serial.println("clientKGY.onElseConnect...");
    kgyLock = false;
    transactionId = 1;

    request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
    request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
    request[2] = 0;
    request[3] = 0;  // Protocol ID (0 для Modbus TCP)
    request[4] = 0;
    request[5] = 6;                       // Длина данных
    request[6] = 1;                       // Адрес устройства Modbus
    request[7] = 3;                       // Код функции (чтение Holding Register)
    request[8] = (uint8_t)(9204 >> 8);    // Старший байт адреса регистра
    request[9] = (uint8_t)(9204 & 0xFF);  // Младший байт адреса регистра
    request[10] = 0;
    request[11] = 1;  // Количество регистров для чтения (1)

    clientKGY.write(reinterpret_cast<const char *>(request), sizeof(request));
  }
  // Создайте и отправьте запрос к КГУ
  clientKGY.onConnect([](void *arg, AsyncClient *c) {
    //Serial.println("clientKGY.onConnect...");
    kgyLock = false;

    transactionId = 1;

    request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
    request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
    request[2] = 0;
    request[3] = 0;  // Protocol ID (0 для Modbus TCP)
    request[4] = 0;
    request[5] = 6;                       // Длина данных
    request[6] = 1;                       // Адрес устройства Modbus
    request[7] = 3;                       // Код функции (чтение Holding Register)
    request[8] = (uint8_t)(9204 >> 8);    // Старший байт адреса регистра
    request[9] = (uint8_t)(9204 & 0xFF);  // Младший байт адреса регистра
    request[10] = 0;
    request[11] = 1;  // Количество регистров для чтения (1)

    c->write(reinterpret_cast<const char *>(request), sizeof(request));
  });
  // Установите обработчик ответа от КГУ
  clientKGY.onData([](void *arg, AsyncClient *c, void *data, size_t len) {
    ////Serial.println("clientKGY.onData...");
    if (len >= 10) {
      uint8_t *response = static_cast<uint8_t *>(data);
      transactionIdResponse = 0;
      transactionIdResponse = (((response[0] << 8) | response[1]));
      //Serial.println("clientKGY.onData..." + String(transactionIdResponse));

      if (transactionIdResponse == 1) {
        trottlePosition = (((response[9] << 8) | response[10]) / 10.0f);
        resultKGY = "Положение дросселя КГУ: " + String(trottlePosition) + " %\n";

        transactionId = 2;

        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(8639 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(8639 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 2) {
        powerConstant = (response[9] << 8) | response[10];
        resultKGY += "Заданная мощность: " + String(powerConstant) + " kW\n";

        transactionId = 3;

        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(8202 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(8202 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)


        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 3) {
        powerActive = (response[9] << 8) | response[10];
        resultKGY += "Активная мощность: " + String(powerActive) + " kW\n";

        transactionId = 9;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(8235 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(8235 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 9) {
        short values = (response[9] << 8) | response[10];

        boolean bit7 = ((values >> 7) & 1) == 0;  // alarm bit
        boolean bit8 = ((values >> 8) & 1) == 0;  // error bit

        isAlarm = (bit7 | bit8);

        transactionId = 13;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9118 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9118 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      }

      else if (transactionIdResponse == 13) {
        short values = (response[9] << 8) | response[10];

        isMixError = (values >> 6);  // Mix alarm bit
        // Serial.print("MIX_error = ");
        // Serial.println(isMixError);

        transactionId = 6;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(8205 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(8205 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 2;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 6) {
        //totalGenerated = (static_cast<uint32_t>(response[9]) << 24) | (static_cast<uint32_t>(response[10]) << 16) | (static_cast<uint32_t>(response[11]) << 8) | static_cast<uint32_t>(response[12]);
        totalGenerated = ((uint32_t)response[9] << 24) | ((uint32_t)response[10] << 16) | ((uint32_t)response[11] << 8) | (uint32_t)response[12];

        transactionId = 4;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9620 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9620 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 4) {
        avgTemp = (response[9] << 8) | response[10];
        //if(avgTemp == 0) avgTemp = 1;

        //updateAvgTemp(avgTemp);

        transactionId = 8;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9157 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9157 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 8) {
        cleanOil = (response[9] << 8) | response[10];

        transactionId = 10;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9206 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9206 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 10) {
        resTemp = (response[9] << 8) | response[10];
        resTemp /= 10;

        transactionId = 11;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9179 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9179 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 11) {
        CH4_KGY = (((response[9] << 8) | response[10]) / 10.0f);

        transactionId = 12;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9172 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9172 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 12) {
        gasTempOUT = (((response[9] << 8) | response[10]) / 10.0f);

        transactionId = 23;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9174 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9174 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 23) {
        gasTempIN = (((response[9] << 8) | response[10]) / 10.0f);

        transactionId = 15;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(8195 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(8195 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 15) {
        l1N = (response[9] << 8) | response[10];
        // l2N = (response[11] << 8) | response[12];
        // l3N = (response[13] << 8) | response[14];

        transactionId = 16;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(8196 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(8196 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));

      } else if (transactionIdResponse == 16) {
        l2N = (response[9] << 8) | response[10];

        transactionId = 17;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(8197 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(8197 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 17) {
        l3N = (response[9] << 8) | response[10];

        transactionId = 18;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9225 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9225 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 18) {
        bearing1Temp = (((response[9] << 8) | response[10]) / 10.0f);

        transactionId = 19;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9226 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9226 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 19) {
        bearing2Temp = (((response[9] << 8) | response[10]) / 10.0f);

        transactionId = 20;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9221 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9221 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 20) {
        wnding1Temp = (((response[9] << 8) | response[10]) / 10.0f);

        transactionId = 21;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9222 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9222 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 21) {
        wnding2Temp = (((response[9] << 8) | response[10]) / 10.0f);

        transactionId = 22;
        request[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
        request[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
        request[2] = 0;
        request[3] = 0;  // Protocol ID (0 для Modbus TCP)
        request[4] = 0;
        request[5] = 6;                       // Длина данных
        request[6] = 1;                       // Адрес устройства Modbus
        request[7] = 3;                       // Код функции (чтение Holding Register)
        request[8] = (uint8_t)(9223 >> 8);    // Старший байт адреса регистра
        request[9] = (uint8_t)(9223 & 0xFF);  // Младший байт адреса регистра
        request[10] = 0;
        request[11] = 1;  // Количество регистров для чтения (1)

        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 22) {
        wnding3Temp = (((response[9] << 8) | response[10]) / 10.0f);

        regulatePower();

        if (isPower) {
          isPower = false;

          transactionId = 5;
          requestWright[0] = (uint8_t)(transactionId >> 8);    // Старший байт Transaction ID
          requestWright[1] = (uint8_t)(transactionId & 0xFF);  // Младший байт Transaction ID
          requestWright[2] = 0;
          requestWright[3] = 0;  // Protocol ID (0 для Modbus TCP)
          requestWright[4] = 0;
          requestWright[5] = 6;                       // Длина данных
          requestWright[6] = 1;                       // Адрес устройства Modbus
          requestWright[7] = 16;                      // Код функции (запись Holding Register)
          requestWright[8] = (uint8_t)(8639 >> 8);    // Старший байт адреса регистра
          requestWright[9] = (uint8_t)(8639 & 0xFF);  // Младший байт адреса регистра
          requestWright[10] = 0;
          requestWright[11] = 1;                           // Количество регистров для записи (1)
          requestWright[12] = 2;                           // Колличество байт данных
          requestWright[13] = (uint8_t)(intPower >> 8);    // Данные первый байт
          requestWright[14] = (uint8_t)(intPower & 0xFF);  // Данные второй байт

          c->write(reinterpret_cast<const char *>(requestWright), sizeof(requestWright));
        } else {
          //c->close(false);
          kgyLock = true;
          Serial.println("clientKGY.CLOSE no regulate...");
        }
      } else if (transactionIdResponse == 5) {
        //c->close(false);
        kgyLock = true;
        Serial.println("clientKGY.CLOSE after regulate...");
      }
    }
  });
  clientKGY.onError([](void *arg, AsyncClient *c, int8_t error) {
    trottlePosition = 9999;
    powerConstant = 9999;
    powerActive = 9999;
    totalGenerated = 0;
    resultKGY = "Ошибка получения данных с КГУ!!! \nРегулирование невозможно.\n";
    resultKGY += String(error) + " error code \n";
    Serial.println("clientKGY.onError ERROR...");
  });
  clientKGY.onDisconnect([](void *arg, AsyncClient *c) {
    kgyLock = true;
    Serial.println("clientKGY.onDisconect...");
  });
}
void pushToFirebase() {
  
  Firebase.RTDB.setFloat(&fbdo, "now/CH4_1", CH4_1p);
  Firebase.RTDB.setFloat(&fbdo, "now/CH4_2", CH4_2p);
  Firebase.RTDB.setFloat(&fbdo, "now/gtsPresher", gtsPr);
  Firebase.RTDB.setFloat(&fbdo, "now/kgyPresher", kgyPr);
  Firebase.RTDB.setBool(&fbdo, "now/alarm", isAlarm);

  if (U12 < 0 || U23 < 0 || U31 < 0) {
    Firebase.RTDB.setInt(&fbdo, "now/U12", -255);
    Firebase.RTDB.setInt(&fbdo, "now/U23", -255);
    Firebase.RTDB.setInt(&fbdo, "now/U31", -255);
  } else {
    Firebase.RTDB.setInt(&fbdo, "now/U12", U12);
    Firebase.RTDB.setInt(&fbdo, "now/U23", U23);
    Firebase.RTDB.setInt(&fbdo, "now/U31", U31);
  }

  if (opPr > 40 || opPr < -5) {
    Firebase.RTDB.setFloat(&fbdo, "now/opPresher", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/opPresher", opPr);
  }

  if (trottlePosition > 100 || trottlePosition < -5) {
    Firebase.RTDB.setFloat(&fbdo, "now/trottlePosition", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/trottlePosition", trottlePosition);
  }

  if (CH4_KGY > 110 || CH4_KGY < -10) {
    Firebase.RTDB.setFloat(&fbdo, "now/CH4_KGY", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/CH4_KGY", CH4_KGY);
  }

  if (wnding1Temp > 900 || wnding1Temp < -40 || wnding2Temp > 900 || wnding2Temp < -40 || wnding3Temp > 900 || wnding3Temp < -40) {
    Firebase.RTDB.setFloat(&fbdo, "now/wnding1Temp", -255);
    Firebase.RTDB.setFloat(&fbdo, "now/wnding2Temp", -255);
    Firebase.RTDB.setFloat(&fbdo, "now/wnding3Temp", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/wnding1Temp", wnding1Temp);
    Firebase.RTDB.setFloat(&fbdo, "now/wnding2Temp", wnding2Temp);
    Firebase.RTDB.setFloat(&fbdo, "now/wnding3Temp", wnding3Temp);
  }

  if (bearing1Temp > 900 || bearing1Temp < -40 || bearing2Temp > 900 || bearing2Temp < -40) {
    Firebase.RTDB.setFloat(&fbdo, "now/bearing1Temp", -255);
    Firebase.RTDB.setFloat(&fbdo, "now/bearing2Temp", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/bearing1Temp", bearing1Temp);
    Firebase.RTDB.setFloat(&fbdo, "now/bearing2Temp", bearing2Temp);
  }

  if (gasTempIN < -50 || gasTempIN > 900) {
    Firebase.RTDB.setFloat(&fbdo, "now/gasTempIN", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/gasTempIN", gasTempIN);
  }

  if (gasTempOUT < -50 || gasTempOUT > 900) {
    Firebase.RTDB.setFloat(&fbdo, "now/gasTempOUT", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/gasTempOUT", gasTempOUT);
  }

  if (l1N > 600 || l1N < -10 || l2N > 600 || l2N < -10 || l3N > 600 || l3N < -10) {
    Firebase.RTDB.setInt(&fbdo, "now/l1N", -255);
    Firebase.RTDB.setInt(&fbdo, "now/l2N", -255);
    Firebase.RTDB.setInt(&fbdo, "now/l3N", -255);
  } else {
    Firebase.RTDB.setInt(&fbdo, "now/l1N", l1N);
    Firebase.RTDB.setInt(&fbdo, "now/l2N", l2N);
    Firebase.RTDB.setInt(&fbdo, "now/l3N", l3N);
  }

  if (powerConstant > 1560 || powerConstant < 0) {
    Firebase.RTDB.setInt(&fbdo, "now/powerConstant", -255);
  } else {
    Firebase.RTDB.setInt(&fbdo, "now/powerConstant", powerConstant);
  }

  if (powerActive > 2000 || powerActive < -300) {
    Firebase.RTDB.setInt(&fbdo, "now/powerActive", -255);
  } else {
    Firebase.RTDB.setInt(&fbdo, "now/powerActive", powerActive);
  }

  if (totalGenerated == 0) {
    Firebase.RTDB.setInt(&fbdo, "now/totalActivePower", -255);
  } else {
    Firebase.RTDB.setInt(&fbdo, "now/totalActivePower", totalGenerated);
  }

  if (cleanOil > 110 || cleanOil < -5) {
    Firebase.RTDB.setInt(&fbdo, "now/cleanOil", -255);
  } else {
    Firebase.RTDB.setInt(&fbdo, "now/cleanOil", cleanOil);
  }

  if (avgTemp > 600 || avgTemp < -40) {
    Firebase.RTDB.setInt(&fbdo, "now/avgTemp", -255);
  } else {
    Firebase.RTDB.setInt(&fbdo, "now/avgTemp", avgTemp);
  }

  if (resTemp > 120 || resTemp < -40) {
    Firebase.RTDB.setFloat(&fbdo, "now/resTemp", -255);
  } else {
    Firebase.RTDB.setFloat(&fbdo, "now/resTemp", resTemp);
  }

  if (Firebase.RTDB.getInt(&fbdo, "now/UnixTime")) {
    if (fbdo.dataTypeEnum() == firebase_rtdb_data_type_integer) {
      uint32_t lastSeen = (fbdo.to<int>());
      if (lastSeen != compareUnixTime) {
        if (compareUnixTime != 0) {
          appRegulate = true;
        }
        compareUnixTime = lastSeen;
        if (Firebase.RTDB.getInt(&fbdo, "now/MaxPower")) {
          if (fbdo.dataTypeEnum() == firebase_rtdb_data_type_integer) {
            maxPower = (fbdo.to<int>());
          }
        }
      } else if (maxPower != 1560 || appMaxPower != 1560 || appRegulate != false) {
        appRegulate = false;
        appMaxPower = 1560;
        maxPower = 1560;
        //Firebase.RTDB.setInt(&fbdo, "/MaxPower", maxPower);
      }
    }
  }
  Firebase.RTDB.setInt(&fbdo, "now/serverUnixTime20", getTime());
}

void firebaseReport() {
  // if (!checkValidData()) {
  //   return;
  // }
  if (((currentHour - hours == 1 || (currentHour - hours) == -23)) && firebase) {
    stamp.getDateTime(getTime());
    String now = String(stamp.year) + "." + String(stamp.month) + "." + String(stamp.day) + "-" + String(stamp.hour) + ":00";
    String date = "/HourReport/" + String(ntpClient.getEpochTime());
    hours = currentHour;

    if (opPr > 40 || opPr < -5) {
      Firebase.RTDB.setFloat(&fbdo, date + "/opPresher", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/opPresher", opPr);
    }

    if (trottlePosition > 100 || trottlePosition < -5) {
      Firebase.RTDB.setFloat(&fbdo, date + "/trottlePosition", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/trottlePosition", trottlePosition);
    }

    if (CH4_KGY > 110 || CH4_KGY < -10) {
      Firebase.RTDB.setFloat(&fbdo, date + "/CH4_KGY", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/CH4_KGY", CH4_KGY);
    }

    if (wnding1Temp > 900 || wnding1Temp < -40 || wnding2Temp > 900 || wnding2Temp < -40 || wnding3Temp > 900 || wnding3Temp < -40) {
      Firebase.RTDB.setFloat(&fbdo, date + "/wnding1Temp", -255);
      Firebase.RTDB.setFloat(&fbdo, date + "/wnding2Temp", -255);
      Firebase.RTDB.setFloat(&fbdo, date + "/wnding3Temp", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/wnding1Temp", wnding1Temp);
      Firebase.RTDB.setFloat(&fbdo, date + "/wnding2Temp", wnding2Temp);
      Firebase.RTDB.setFloat(&fbdo, date + "/wnding3Temp", wnding3Temp);
    }

    if (bearing1Temp > 900 || bearing1Temp < -40 || bearing2Temp > 900 || bearing2Temp < -40) {
      Firebase.RTDB.setFloat(&fbdo, date + "/bearing1Temp", -255);
      Firebase.RTDB.setFloat(&fbdo, date + "/bearing2Temp", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/bearing1Temp", bearing1Temp);
      Firebase.RTDB.setFloat(&fbdo, date + "/bearing2Temp", bearing2Temp);
    }

    if (gasTempIN < -50 || gasTempIN > 900) {
      Firebase.RTDB.setFloat(&fbdo, date + "/gasTempIN", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/gasTempIN", gasTempIN);
    }

    if (gasTempOUT < -50 || gasTempOUT > 900) {
      Firebase.RTDB.setFloat(&fbdo, date + "/gasTempOUT", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/gasTempOUT", gasTempOUT);
    }

    if (l1N > 600 || l1N < -10 || l2N > 600 || l2N < -10 || l3N > 600 || l3N < -10) {
      Firebase.RTDB.setInt(&fbdo, date + "/l1N", -255);
      Firebase.RTDB.setInt(&fbdo, date + "/l2N", -255);
      Firebase.RTDB.setInt(&fbdo, date + "/l3N", -255);
    } else {
      Firebase.RTDB.setInt(&fbdo, date + "/l1N", l1N);
      Firebase.RTDB.setInt(&fbdo, date + "/l2N", l2N);
      Firebase.RTDB.setInt(&fbdo, date + "/l3N", l3N);
    }

    if (powerConstant > 1560 || powerConstant < 0) {
      Firebase.RTDB.setInt(&fbdo, date + "/powerConstant", -255);
    } else {
      Firebase.RTDB.setInt(&fbdo, date + "/powerConstant", powerConstant);
    }

    if (powerActive > 2000 || powerActive < -300) {
      Firebase.RTDB.setInt(&fbdo, date + "/powerActive", -255);
    } else {
      Firebase.RTDB.setInt(&fbdo, date + "/powerActive", powerActive);
    }

    if (totalGenerated == 0) {
      Firebase.RTDB.setInt(&fbdo, date + "/totalActivePower", -255);
    } else {
      Firebase.RTDB.setInt(&fbdo, date + "/totalActivePower", totalGenerated);
    }

    if (cleanOil > 110 || cleanOil < -5) {
      Firebase.RTDB.setInt(&fbdo, date + "/cleanOil", -255);
    } else {
      Firebase.RTDB.setInt(&fbdo, date + "/cleanOil", cleanOil);
    }

    if (avgTemp > 600 || avgTemp < -40) {
      Firebase.RTDB.setInt(&fbdo, date + "/avgTemp", -255);
    } else {
      Firebase.RTDB.setInt(&fbdo, date + "/avgTemp", avgTemp);
    }

    if (resTemp > 120 || resTemp < -40) {
      Firebase.RTDB.setFloat(&fbdo, date + "/resTemp", -255);
    } else {
      Firebase.RTDB.setFloat(&fbdo, date + "/resTemp", resTemp);
    }

    Firebase.RTDB.setFloat(&fbdo, date + "/CH4_1", CH4_1p);
    Firebase.RTDB.setFloat(&fbdo, date + "/CH4_2", CH4_2p);
    Firebase.RTDB.setFloat(&fbdo, date + "/gtsPresher", gtsPr);
    Firebase.RTDB.setFloat(&fbdo, date + "/kgyPresher", kgyPr);
    Firebase.RTDB.setInt(&fbdo, date + "/U12", U12);
    Firebase.RTDB.setInt(&fbdo, date + "/U23", U23);
    Firebase.RTDB.setInt(&fbdo, date + "/U31", U31);
    Firebase.RTDB.setString(&fbdo, date + "/date", now);
  }
}
void monthGenerated() {
  int currentDay = getDayOfMonthFromUnixTime();
  if (currentDay == 1 && day != currentDay && totalGenerated != 0) {
    Firebase.RTDB.setInt(&fbdo, "now/monthStartGenerated", totalGenerated);
    monthStartGenerated = totalGenerated;
    day = currentDay;
  }
  if (monthStartGenerated == 0) {
    if (Firebase.RTDB.getInt(&fbdo, "now/monthStartGenerated")) {
      if (fbdo.dataTypeEnum() == firebase_rtdb_data_type_integer) {
        monthStartGenerated = (fbdo.to<int>());
      }
    } else if (totalGenerated != 0) {
      Firebase.RTDB.setInt(&fbdo, "now/monthStartGenerated", totalGenerated);
    }
  }
}

int getDayOfMonthFromUnixTime() {
  stamp.getDateTime(getTime());
  return stamp.day;
}

uint32_t getTime() {
  uint32_t now = (millis() / 1000) + epochTime;
  currentHour = (now / 3600) % 24;
  if (now < 1701730982 || millis() > 86400000) {
    ESP.restart();
    delay(10000);
    digitalWrite(resetPin, LOW);   // Установить низкий уровень на D1 (RST)
    delay(100);                    // Подождать некоторое время
    digitalWrite(resetPin, HIGH);  // Установить высокий уровень на D1 (RST)
  }
  return now;
}