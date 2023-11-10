#define USE_CLIENTSSL false

#include <AsyncTelegram2.h>
#include <Adafruit_SleepyDog.h>

// Timezone definition
#include <time.h>
#define MYTZ "EET-2EEST,M3.5.0,M10.5.0/3"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
BearSSL::WiFiClientSecure client;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);

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

AsyncTelegram2 myBot(client);

#include <NTPClient.h>
#include <WiFiUdp.h>


#include <UnixTime.h>

#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
SET_LOOP_TASK_STACK_SIZE(16 * 1024);  // 16KB
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

const int serverPort = 502;  // Порт Modbus
int transactionId = 0;       // Уникальный идентификатор транзакции
uint8_t transactionIdResponse = 0;

AsyncClient clientRegistrator;
AsyncClient clientKGY;

const char *ssid = "***";                                        // SSID WiFi network
const char *pass = "***";                                        // Password  WiFi network
const char *token = "***";  // Telegram token
const char *channel = "***";
int64_t userid = ***;
#define DATABASE_URL "***"
#define DATABASE_SECRET "***"

String resultRegistrator;
String resultKGY;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//unsigned long dataMillis = 0;
uint8_t count = 0;

uint16_t powerActive = 9999;
uint16_t trottlePosition = 9999;
uint16_t powerConstant = 9999;
uint32_t totalGenerated = 0;
float CH4_1p = 9999;
float CH4_2p = 9999;
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

const uint8_t ARRAY_SIZE = 45;
int temp[ARRAY_SIZE] = { 0 };
uint8_t request[] = { 0 };

int hours;
int currentHour;
bool hourReport = false;
bool regulate = false;
bool skipFirst = false;
bool appRegulate = false;
bool firebase = true;
bool kgyLock = true;
bool regLock = true;
bool isAlarm = false;
uint32_t epochTime = 0;
//String reset = "";  //String(ESP.getResetInfo()) + "\n";

#define LED_BUILTIN 2
// This sets Arduino Stack Size - comment this line to use default 8K stack size

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);  // Настройка пина GPIO2 как выхода
  // initialize the Serial
  Serial.begin(9600);
  Serial.println("\nStarting TelegramBot...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  delay(500);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }

#ifdef ESP8266
  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  //Set certficate, session and some other base client properies
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
#elif defined(ESP32)
  // Sync time with NTP
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#if USE_CLIENTSSL == false
  client.setCACert(telegram_cert);
#endif
#endif

  // Set the Telegram bot properies
  myBot.setUpdateTime(3000);
  myBot.setTelegramToken(token);

  // Check if all things are ok
  Serial.print("\nTest Telegram connection... ");
  myBot.begin() ? Serial.println("OK") : Serial.println("NOK");

  char welcome_msg[48];
  snprintf(welcome_msg, 128, "BOT @%s is online", myBot.getBotName());

  // Send a message to specific user who has started your bot
  myBot.sendTo(userid, welcome_msg);


  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(512 /* Rx buffer size in bytes from 512 - 16384 */, 512 /* Tx buffer size in bytes from 512 - 16384 */);
  Firebase.begin(&config, &auth);

  ntpClient.update();
  epochTime = ntpClient.getEpochTime();
  currentHour = (epochTime / 3600) % 24;
  hours = currentHour;

  digitalWrite(LED_BUILTIN, false);
  int downMS = Watchdog.enable(20000);
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
  }

  static uint32_t ledTime = millis();
  if (millis() - ledTime > 1000 && (firebase || regulate || appRegulate)) {
    //regulatePower();
    blink();
    getDate();
    ledTime = millis();
    Watchdog.reset();
  } else if (ledTime > millis()) {
    ledTime = millis();
    ntpClient.update();
    epochTime = ntpClient.getEpochTime();
  }

  TBMessage msg;
  if (myBot.getNewMessage(msg) || ((currentHour - hours == 1 || currentHour - hours == -23) && hourReport)) {
    blink();
    if (msg.text == "/report@KGY_operator_bot" || msg.text == "/report") {
      blink();
      if (hourReport) {
        hourReport = false;
        myBot.sendToChannel(channel, "Опция отчетности отключена", true);
      } else {
        hours = currentHour;
        hourReport = true;
        myBot.sendToChannel(channel, "Опция отчетности включена", true);
      }
    } else if (msg.text == "/regulate@KGY_operator_bot" || msg.text == "/regulate") {
      blink();
      if (regulate) {
        regulate = false;
        myBot.sendToChannel(channel, "Опция регулирования отключена", true);
      } else {
        regulate = true;
        myBot.sendToChannel(channel, "Опция регулирования включена", true);
      }
    } else if (msg.text == "/base@KGY_operator_bot" || msg.text == "/base") {
      if (firebase) {
        firebase = false;
        myBot.sendToChannel(channel, "FireBase деактивирован", true);
      } else {
        firebase = true;
        myBot.sendToChannel(channel, "FireBase активирован", true);
      }
    } else if (msg.text == "/status" || msg.text == "/status@KGY_operator_bot" || ((currentHour - hours == 1 || currentHour - hours == -23) && hourReport)) {
      digitalWrite(LED_BUILTIN, false);
      if (!regulate && !firebase && !appRegulate) {
        getDate();
        delay(5500);
      }
      String message;
      stamp.getDateTime(getTime());
      message = String(stamp.year) + "." + String(stamp.month) + "." + String(stamp.day) + "-" + String(stamp.hour) + ":" + String(stamp.minute) + ":" + String(stamp.second) + "\n";
      // message = uxTaskGetStackHighWaterMark(NULL) + "\n";
      //message += reset;
      message += resultKGY;
      message += resultRegistrator;
      if (monthStartGenerated != 0) {
        message += "Техническая генерация: " + String((totalGenerated - monthStartGenerated) / 1000) + " MW\n";
      }
      message += "Максимальная мощность: " + String(appMaxPower) + "\n";
      if (appRegulate) message += "Удаленное регулирование включено\n";
      if (isAlarm) message += "-Обнаружена ошибка КГУ-";
      myBot.sendToChannel(channel, message, true);
      if ((currentHour - hours == 1 || currentHour - hours == -23) && hourReport) {
        hours = currentHour;
      }
      digitalWrite(LED_BUILTIN, true);
    }
  }
}

void blink() {
  digitalWrite(LED_BUILTIN, false);
  delay(200);
  digitalWrite(LED_BUILTIN, true);
}
void setPower(int power) {
  intPower = power;
  isPower = true;
}
bool checkValidData() {
  static uint32_t validTime = millis();

  if (trottlePosition > 100 || trottlePosition < -5 || powerConstant > 1560 || powerConstant < 0 || powerActive > 2000 || powerActive < -300 || opPr > 40 || opPr < -5 || totalGenerated == 0) {
    Serial.println("\ntrottlePosition = " + String(trottlePosition) + "\npowerConstant = " + String(powerConstant) + "\npowerActive = " + String(powerActive) + "\nopPr = " + String(opPr) + "\ntotalGenerated = " + totalGenerated);
    return false;
  } else {
    return true;
  }
}
void regulatePower() {
  static uint32_t powerUpTime = millis();
  static uint32_t lastRegulate = millis();

  if ((!regulate && !appRegulate) || !checkValidData()) return;

  if (powerActive > 0 && millis() - lastRegulate > 20000) {
    (opPr > 6 && powerActive < 1250) ? reg = 20 : reg = 10;
    if (opPr < 3 || avgTemp > 470) {
      (powerConstant > 1000) ? setPower(powerConstant - 100) : setPower(900);
      lastRegulate = millis();
    } else if (opPr < 4 || trottlePosition > 90 || powerActive > 1560 || powerConstant > maxPower || powerConstant > appMaxPower) {
      checkActPower();
      checkThrottle();
      powerConstant > 1000 ? setPower(powerConstant - 10) : setPower(900);
      lastRegulate = millis();
    } else if (opPr > 5 && ((powerConstant - powerActive) <= 50) && (maxPower - powerConstant >= reg) && trottlePosition < 90 && (appMaxPower - powerConstant >= reg)) {
      setPower(powerConstant + reg);
      lastRegulate = millis();
    } else if (opPr > 5 && trottlePosition < 80 && ((millis() - powerUpTime) >= 300000) && maxPower - appMaxPower >= 10 && appMaxPower <= 1550) {
      appMaxPower += 10;
      powerUpTime = millis();
    }
  } else if (powerActive <= 0 && powerConstant != 800) {
    setPower(800);
    appMaxPower = 1560;
    maxPower = 1560;
    myBot.sendToChannel(channel, "КГУ остановленно!! \nТак и задумано?", true);

  } else if (lastRegulate > millis()) {
    lastRegulate = millis();
  }
}
void checkActPower() {
  if (powerActive > 1560) appMaxPower = powerConstant - 10;
}
void checkThrottle() {
  if (trottlePosition > 90) appMaxPower = powerConstant - 10;
}
void getDate() {
  if (kgyLock == true) {
    sendKGYRequest();
    //Serial.println("\nsendKGYRequest...");
  }
  if (regLock == true) {
    sendRegistratorRequest();
    //Serial.println("\nsendRegistratorRequest...");
  }
}
void sendRegistratorRequest() {
  if (!clientRegistrator.connected() && !clientRegistrator.connect(registratorIP, serverPort)) {
    Serial.println("\nclientRegistrator.connect ERROR...");
    opPr = 9999;  // Установка значения в 9999 при ошибке соединения
    resultRegistrator = "Ошибка соединения с регистратором!!! \nРегулирование невозможно.";
    return;
  } else {
    regLock = false;
    Serial.println("\nclientRegistrator.onElseConnect...");
    transactionId = 7;

    request[] = {
      static_cast<uint8_t>(transactionId >> 8),    // Старший байт Transaction ID
      static_cast<uint8_t>(transactionId & 0xFF),  // Младший байт Transaction ID
      0, 0,                                        // Protocol ID (0 для Modbus TCP)
      0, 6,                                        // Длина данных
      1,                                           // Адрес устройства Modbus
      4,                                           // Код функции (чтение Holding Register)
      0, 0,                                        // Старший и младший байты адреса регистра (0)
      0, 12                                        // Количество регистров для чтения (12)
    };
    // Отправьте запрос
    clientRegistrator.write(reinterpret_cast<const char *>(request), sizeof(request));
  }
  // Создайте и отправьте запрос к регистратору
  clientRegistrator.onConnect([](void *arg, AsyncClient *c) {
    regLock = false;
    Serial.println("\nclientRegistrator.onConnect...");
    // Убедитесь, что transactionId определен
    
    transactionId = 7;
    request[] = {
      static_cast<uint8_t>(transactionId >> 8),    // Старший байт Transaction ID
      static_cast<uint8_t>(transactionId & 0xFF),  // Младший байт Transaction ID
      0, 0,                                        // Protocol ID (0 для Modbus TCP)
      0, 6,                                        // Длина данных
      1,                                           // Адрес устройства Modbus
      4,                                           // Код функции (чтение Holding Register)
      0, 0,                                        // Старший и младший байты адреса регистра (0)
      0, 12                                        // Количество регистров для чтения (12)
    };
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
      opPr = round(*reinterpret_cast<uint32_t*>(&response[9]) * 10.0f) / 10.0f;

      // uint32_t GTS = ((uint32_t)response[17] << 24) | ((uint32_t)response[18] << 16) | ((uint32_t)response[19] << 8) | (uint32_t)response[20];
      // float floatGTS;
      // memcpy(&floatGTS, &GTS, sizeof(floatGTS));
      // gtsPr = round(floatGTS * 10) / 10;
      gtsPr = round(*reinterpret_cast<uint32_t*>(&response[17]) * 10.0f) / 10.0f;
      
      // uint32_t KY = ((uint32_t)response[21] << 24) | ((uint32_t)response[22] << 16) | ((uint32_t)response[23] << 8) | (uint32_t)response[24];
      // float floatKY;
      // memcpy(&floatKY, &KY, sizeof(floatKY));
      // kgyPr = round(floatKY * 10) / 10;
      kgyPr = round(*reinterpret_cast<uint32_t*>(&response[21]) * 10.0f) / 10.0f;
      
      // uint32_t CH4_1 = ((uint32_t)response[25] << 24) | ((uint32_t)response[26] << 16) | ((uint32_t)response[27] << 8) | (uint32_t)response[28];
      // float floatCH4_1;
      // memcpy(&floatCH4_1, &CH4_1, sizeof(floatCH4_1));
      // CH4_1p = round(floatCH4_1 * 10) / 10;
      CH4_1p = round(*reinterpret_cast<uint32_t*>(&response[25]) * 10.0f) / 10.0f;

      // uint32_t CH4_2 = ((uint32_t)response[29] << 24) | ((uint32_t)response[30] << 16) | ((uint32_t)response[31] << 8) | (uint32_t)response[32];
      // float floatCH4_2;
      // memcpy(&floatCH4_2, &CH4_2, sizeof(floatCH4_2));
      // CH4_2p = round(floatCH4_2 * 10) / 10;
      CH4_2p = round(*reinterpret_cast<uint32_t*>(&response[25]) * 10.0f) / 10.0f;

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
    Serial.println("clientKGY.connect ERROR...");
    trottlePosition = 9999;
    powerConstant = 9999;
    powerActive = 9999;
    resultKGY = "Ошибка подключения к КГУ!!! \nРегулирование невозможно.";
    return;
  } else {
    Serial.println("clientKGY.onElseConnect...");
    kgyLock = false;
    transactionId = 1;
    request[] = {
      (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
      (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
      0, 0,                             // Protocol ID (0 для Modbus TCP)
      0, 6,                             // Длина данных
      1,                                // Адрес устройства Modbus
      3,                                // Код функции (чтение Holding Register)
      (uint8_t)(9204 >> 8),             // Старший байт адреса регистра
      (uint8_t)(9204 & 0xFF),           // Младший байт адреса регистра
      0, 1                              // Количество регистров для чтения (1)
    };
    clientKGY.write(reinterpret_cast<const char *>(request), sizeof(request));
  }
  // Создайте и отправьте запрос к КГУ
  clientKGY.onConnect([](void *arg, AsyncClient *c) {
    Serial.println("clientKGY.onConnect...");
    kgyLock = false;
    transactionId = 1;
    request[] = {
      (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
      (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
      0, 0,                             // Protocol ID (0 для Modbus TCP)
      0, 6,                             // Длина данных
      1,                                // Адрес устройства Modbus
      3,                                // Код функции (чтение Holding Register)
      (uint8_t)(9204 >> 8),             // Старший байт адреса регистра
      (uint8_t)(9204 & 0xFF),           // Младший байт адреса регистра
      0, 1                              // Количество регистров для чтения (1)
    };

    c->write(reinterpret_cast<const char *>(request), sizeof(request));
  });
  // Установите обработчик ответа от КГУ
  clientKGY.onData([](void *arg, AsyncClient *c, void *data, size_t len) {
    Serial.println("clientKGY.onData...");
    if (len >= 10) {
      uint8_t *response = static_cast<uint8_t *>(data);
      transactionIdResponse = 0;
      transactionIdResponse = (((response[0] << 8) | response[1]));
      if (transactionIdResponse == 1) {
        trottlePosition = (((response[9] << 8) | response[10]) / 10);
        resultKGY = "Положение дросселя КГУ: " + String(trottlePosition) + " %\n";

        transactionId = 2;
        request[] = {
          (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
          (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
          0, 0,                             // Protocol ID (0 для Modbus TCP)
          0, 6,                             // Длина данных
          1,                                // Адрес устройства Modbus
          3,                                // Код функции (чтение Holding Register)
          (uint8_t)(8639 >> 8),             // Старший байт адреса регистра
          (uint8_t)(8639 & 0xFF),           // Младший байт адреса регистра
          0, 1                              // Количество регистров для чтения (1)
        };
        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 2) {
        powerConstant = (response[9] << 8) | response[10];
        resultKGY += "Заданная мощность: " + String(powerConstant) + " kW\n";

        transactionId = 3;
        request[] = {
          (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
          (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
          0, 0,                             // Protocol ID (0 для Modbus TCP)
          0, 6,                             // Длина данных
          1,                                // Адрес устройства Modbus
          3,                                // Код функции (чтение Holding Register)
          (uint8_t)(8202 >> 8),             // Старший байт адреса регистра
          (uint8_t)(8202 & 0xFF),           // Младший байт адреса регистра
          0, 1                              // Количество регистров для чтения (1)
        };
        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 3) {
        powerActive = (response[9] << 8) | response[10];
        resultKGY += "Активная мощность: " + String(powerActive) + " kW\n";

        transactionId = 9;
        request[] = {
          (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
          (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
          0, 0,                             // Protocol ID (0 для Modbus TCP)
          0, 6,                             // Длина данных
          1,                                // Адрес устройства Modbus
          3,                                // Код функции (чтение Holding Register)
          (uint8_t)(8235 >> 8),             // Старший байт адреса регистра
          (uint8_t)(8235 & 0xFF),           // Младший байт адреса регистра
          0, 2                              // Количество регистров для чтения (1)
        };
        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 9) {
        short values = (response[9] << 8) | response[10];

        boolean bit7 = ((values >> 7) & 1) == 0;  // alarm bit
        boolean bit8 = ((values >> 8) & 1) == 0;  // error bit

        isAlarm = (bit7 | bit8);

        transactionId = 6;
        request[] = {
          (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
          (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
          0, 0,                             // Protocol ID (0 для Modbus TCP)
          0, 6,                             // Длина данных
          1,                                // Адрес устройства Modbus
          3,                                // Код функции (чтение Holding Register)
          (uint8_t)(8205 >> 8),             // Старший байт адреса регистра
          (uint8_t)(8205 & 0xFF),           // Младший байт адреса регистра
          0, 2                              // Количество регистров для чтения (1)
        };
        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 6) {
        totalGenerated = ((uint32_t)response[9] << 24) | ((uint32_t)response[10] << 16) | ((uint32_t)response[11] << 8) | (uint32_t)response[12];

        transactionId = 4;
        request[] = {
          (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
          (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
          0, 0,                             // Protocol ID (0 для Modbus TCP)
          0, 6,                             // Длина данных
          1,                                // Адрес устройства Modbus
          3,                                // Код функции (чтение Holding Register)
          (uint8_t)(9620 >> 8),             // Старший байт адреса регистра
          (uint8_t)(9620 & 0xFF),           // Младший байт адреса регистра
          0, 2                              // Количество регистров для чтения (1)
        };
        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 4) {
        avgTemp = (response[9] << 8) | response[10];
        updateAvgTemp(avgTemp);
        transactionId = 8;
        request[] = {
          (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
          (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
          0, 0,                             // Protocol ID (0 для Modbus TCP)
          0, 6,                             // Длина данных
          1,                                // Адрес устройства Modbus
          3,                                // Код функции (чтение Holding Register)
          (uint8_t)(9157 >> 8),             // Старший байт адреса регистра
          (uint8_t)(9157 & 0xFF),           // Младший байт адреса регистра
          0, 2                              // Количество регистров для чтения (1)
        };
        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 8) {
        cleanOil = (response[9] << 8) | response[10];

        transactionId = 10;
        request[] = {
          (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
          (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
          0, 0,                             // Protocol ID (0 для Modbus TCP)
          0, 6,                             // Длина данных
          1,                                // Адрес устройства Modbus
          3,                                // Код функции (чтение Holding Register)
          (uint8_t)(9206 >> 8),             // Старший байт адреса регистра
          (uint8_t)(9206 & 0xFF),           // Младший байт адреса регистра
          0, 2                              // Количество регистров для чтения (1)
        };
        c->write(reinterpret_cast<const char *>(request), sizeof(request));
      } else if (transactionIdResponse == 10) {
        resTemp = (response[9] << 8) | response[10];
        resTemp /= 10;

        regulatePower();

        if (isPower) {
          isPower = false;
          transactionId = 5;
          request[] = {
            (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
            (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
            0, 0,                             // Protocol ID (0 для Modbus TCP)
            0, 6,                             // Длина данных
            1,                                // Адрес устройства Modbus
            16,                               // Код функции (чтение Holding Register)
            (uint8_t)(8639 >> 8),             // Старший байт адреса регистра
            (uint8_t)(8639 & 0xFF),           // Младший байт адреса регистра
            0, 1,                             // Количество регистров для записи (1)
            2,                                // Колличество байт данных
            (uint8_t)(intPower >> 8),         // Данные первый байт
            (uint8_t)(intPower & 0xFF)        // Данные второй байт
          };
          c->write(reinterpret_cast<const char *>(request), sizeof(request));
        } else {
          //c->close(false);
          kgyLock = true;
          Serial.println("clientKGY.CLOSE no regulate...");
        }
      } else if (transactionId == 5) {
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
  if (!checkValidData()) {
    return;
  }

  Firebase.RTDB.setFloat(&fbdo, "now/opPresher", opPr);
  Firebase.RTDB.setFloat(&fbdo, "now/trottlePosition", trottlePosition);
  Firebase.RTDB.setInt(&fbdo, "now/powerConstant", powerConstant);
  Firebase.RTDB.setInt(&fbdo, "now/powerActive", powerActive);
  Firebase.RTDB.setFloat(&fbdo, "now/CH4_1", CH4_1p);
  Firebase.RTDB.setFloat(&fbdo, "now/CH4_2", CH4_2p);
  Firebase.RTDB.setFloat(&fbdo, "now/gtsPresher", gtsPr);
  Firebase.RTDB.setFloat(&fbdo, "now/kgyPresher", kgyPr);
  Firebase.RTDB.setInt(&fbdo, "now/totalActivePower", totalGenerated);
  Firebase.RTDB.setBool(&fbdo, "now/alarm", isAlarm);
  Firebase.RTDB.setInt(&fbdo, "now/cleanOil", cleanOil);
  Firebase.RTDB.setInt(&fbdo, "now/avgTemp", avgTemp);
  Firebase.RTDB.setFloat(&fbdo, "now/resTemp", resTemp);
  Firebase.RTDB.setInt(&fbdo, "now/serverUnixTime20", getTime());

  //updateAvgTemp(avgTemp);
  // for (int i = 0; i < ARRAY_SIZE; ++i) {
  //   Firebase.RTDB.setInt(&fbdo, "avgTemp/" + String(i), temp[i]);
  // }

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
}
void firebaseReport() {
  if (!checkValidData()) {
    return;
  }
  if (((currentHour - hours == 1 || (currentHour - hours) == -23)) && firebase) {
    stamp.getDateTime(getTime());
    String now = String(stamp.year) + "." + String(stamp.month) + "." + String(stamp.day) + "-" + String(stamp.hour) + ":00";
    String date = "/HourReport/" + String(ntpClient.getEpochTime());
    hours = currentHour;

    Firebase.RTDB.setFloat(&fbdo, date + "/opPresher", opPr);
    Firebase.RTDB.setFloat(&fbdo, date + "/trottlePosition", trottlePosition);
    Firebase.RTDB.setInt(&fbdo, date + "/powerConstant", powerConstant);
    Firebase.RTDB.setInt(&fbdo, date + "/powerActive", powerActive);
    Firebase.RTDB.setFloat(&fbdo, date + "/CH4_1", CH4_1p);
    Firebase.RTDB.setFloat(&fbdo, date + "/CH4_2", CH4_2p);
    Firebase.RTDB.setFloat(&fbdo, date + "/gtsPresher", gtsPr);
    Firebase.RTDB.setFloat(&fbdo, date + "/kgyPresher", kgyPr);
    Firebase.RTDB.setInt(&fbdo, date + "/totalActivePower", totalGenerated);
    Firebase.RTDB.setString(&fbdo, date + "/date", now);
    Firebase.RTDB.setInt(&fbdo, date + "/cleanOil", cleanOil);
    Firebase.RTDB.setInt(&fbdo, date + "/avgTemp", avgTemp);
    Firebase.RTDB.setFloat(&fbdo, date + "/resTemp", resTemp);
  }
}
void monthGenerated() {
  int currentDay = getDayOfMonthFromUnixTime();
  if (currentDay == 1 && day != currentDay && totalGenerated != 0) {
    Firebase.RTDB.setInt(&fbdo, "now/monthStartGenerated", totalGenerated);
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
  return now;
}

void updateAvgTemp(int avgTemp) {
  if(temp > 1000 || temp < -40) return;
  if (temp[0] != 0) {
    for (int i = ARRAY_SIZE - 1; i > 0; --i) {
      temp[i] = temp[i - 1];
    }
  }
  if(avgTemp == 0) avgTemp = 1;
  temp[0] = avgTemp;
  sendAvgTempToFirebase();
}
void sendAvgTempToFirebase(){
  if(count == ARRAY_SIZE){
    count = 0;
    for (int i = 0; i < ARRAY_SIZE; ++i) {
    Firebase.RTDB.setInt(&fbdo, "avgTemp/" + String(i), temp[i]);
      }
  }
  count++;
}
