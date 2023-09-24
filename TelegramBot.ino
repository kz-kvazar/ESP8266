
#define USE_CLIENTSSL true
#include <AsyncTelegram2.h>
// Timezone definition
#include <time.h>
#define MYTZ "EET-2EEST,M3.5.0,M10.5.0/3"

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>

#include <UnixTime.h>

#include <Arduino.h>
#include <FirebaseESP8266.h>

WiFiUDP udp;
UnixTime stamp(0);
// Устанавливаем поправку на часовой пояс для Украины (GMT+2)
NTPClient ntpClient(udp, "pool.ntp.org", 2 * 60 * 60);

BearSSL::WiFiClientSecure client;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);

const IPAddress serverIP(172, 30, 40, 50);     // IP-адрес сервера Modbus
const IPAddress registratorIP(10, 70, 0, 28);  // IP-адрес сервера Modbus

const int serverPort = 502;  // Порт Modbus
int transactionId = 0;       // Уникальный идентификатор транзакции

AsyncTelegram2 myBot(client);
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
int reg = 10;
uint64_t UnixTime = 0;
int day = 32;
int intPower = 0;
bool isPower = false;
long monthStartGenerated = 0;


int hours;
int currentHour;
bool hourReport = false;
bool regulate = false;
bool skipFirst = false;
bool appRegulate = false;
bool firebase = true;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  delay(500);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }

  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  //Set certficate, session and some other base client properies
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);

  // Set the Telegram bot properies
  myBot.setUpdateTime(3000);
  myBot.setTelegramToken(token);

  // Check if all things are ok
  Serial.print("\nTest Telegram connection... ");
  myBot.begin() ? Serial.println("OK") : Serial.println("NOK");

  char welcome_msg[128];
  snprintf(welcome_msg, 128, "BOT @%s is online", myBot.getBotName());

  // Send a message to specific user who has started your bot
  myBot.sendTo(userid, welcome_msg);


  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  Firebase.begin(&config, &auth);

  ntpClient.update();


  digitalWrite(LED_BUILTIN, true);
}

void loop() {

  static uint32_t hourTime = millis();
  if (millis() - hourTime > 20000 && firebase /*&& hourReport*/) {
    ntpClient.update();
    currentHour = ntpClient.getHours(); /*(ntpClient.getUnixTime() / 3600) % 24*/
    monthGenerated();
    pushToFirebase();
    hourTime = millis();
    //currentHour = (ntpClient.getUnixTime() / 3600) % 24;
    blink();
  } else if (hourTime > millis()) {
    hourTime = millis();
  }

  static uint32_t ledTime = millis();
  if (millis() - ledTime > 5500 && (firebase || regulate)) {
    //regulatePower();
    getDate();
    ledTime = millis();
    blink();
  } else if (ledTime > millis()) {
    ledTime = millis();
  }

  TBMessage msg;
  if (myBot.getNewMessage(msg) || (((currentHour - hours) == 1) || ((currentHour - hours) == -23)) && hourReport) {
    blink();
    if (msg.text == "/report@KGY_operator_bot" || msg.text == "/report") {
      blink();
      if (hourReport) {
        hourReport = false;
        myBot.sendToChannel(channel, "Опция отчетности отключена", true);
      } else {
        hours = ntpClient.getHours();  //(ntpClient.getUnixTime() / 3600) % 24;
        currentHour = hours;
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
        myBot.sendToChannel(channel, "FireBase отключена", true);
      } else {
        firebase = false;
        myBot.sendToChannel(channel, "FireBase активированн", true);
      }
      myBot.sendToChannel(channel, "Все опции деактивированны", true);
    } else if (msg.text == "/status" || msg.text == "/status@KGY_operator_bot" || ((currentHour - hours == 1 || currentHour - hours == -23) && hourReport)) {
      digitalWrite(LED_BUILTIN, false);
      if (!regulate && !firebase) {
        getDate();
        delay(5500);
      }
      String message;
      message = resultKGY;
      message += resultRegistrator;
      message += "Техническая генерация: " + String((totalGenerated - monthStartGenerated)/1000) + " MW";
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
  delay(200);
}
void setPower(int power) {
  // WiFiClient client;
  // if (!client.connect(serverIP, serverPort)) {
  //   Serial.println("Connection failed.");
  //   return;
  // }
  // transactionId = 5;
  // uint8_t request[] = {
  //   (uint8_t)(transactionId >> 8),    // Старший байт Transaction ID
  //   (uint8_t)(transactionId & 0xFF),  // Младший байт Transaction ID
  //   0, 0,                             // Protocol ID (0 для Modbus TCP)
  //   0, 6,                             // Длина данных
  //   1,                                // Адрес устройства Modbus
  //   16,                               // Код функции (чтение Holding Register)
  //   (uint8_t)(8639 >> 8),             // Старший байт адреса регистра
  //   (uint8_t)(8639 & 0xFF),           // Младший байт адреса регистра
  //   0, 1,                             // Количество регистров для записи (1)
  //   2,                                // Колличество байт данных
  //   (uint8_t)(power >> 8),            // Данные первый байт
  //   (uint8_t)(power & 0xFF)           // Данные второй байт
  // };
  // client.write(request, sizeof(request));
  // client.stop();
  // delay(100);
  intPower = power;
  isPower = true;
}
bool checkValidData() {
  if (trottlePosition > 100 || trottlePosition < 0 || powerConstant > 1560 || powerConstant < 0 || powerActive > 2000 || powerActive < -200 || opPr > 40 || opPr < -5 || totalGenerated == 0) {
    if (skipFirst) {
      myBot.sendTo(userid, "trottlePosition = " + String(trottlePosition) + "\npowerConstant = " + String(powerConstant) + "\npowerActive = " + String(powerActive) + "\nopPr = " + String(opPr) + "\ntotalGenerated = " + totalGenerated);
    }
    skipFirst = true;
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
    (opPr > 6 && powerActive < 1350) ? reg = 20 : reg = 10;
    if (opPr < 3) {
      (powerConstant > 1000) ? setPower(powerConstant - 100) : setPower(900);
      lastRegulate = millis();
      blink();
    } else if (opPr < 4 || trottlePosition > 90 || powerActive > 1560 || powerConstant > maxPower) {
      checkActPower();
      checkThrottle();
      powerConstant > 1000 ? setPower(powerConstant - 10) : setPower(900);
      lastRegulate = millis();
      blink();
    } else if (opPr > 5 && ((powerConstant - powerActive) <= 50) && ((powerConstant + 10) < maxPower) && trottlePosition < 90) {
      setPower(powerConstant + reg);
      lastRegulate = millis();
      blink();
    } else if (opPr > 5 && trottlePosition < 80 && ((millis() - powerUpTime) >= 300000) && maxPower <= 1550) {
      maxPower += 10;
      powerUpTime = millis();
      blink();
    }
  } else if (powerActive <= 0 && powerConstant != 800) {
    setPower(800);
    maxPower = 1560;
    myBot.sendToChannel(channel, "КГУ остановленно!! \n Так и задумано?", true);
    blink();
  } else if (lastRegulate > millis()) {
    lastRegulate = millis();
  }
}
void checkActPower() {
  if (powerActive > 1560) maxPower = powerConstant - 10;
}
void checkThrottle() {
  if (trottlePosition > 90) maxPower = powerConstant - 10;
}
void getDate() {
  sendRegistratorRequest();
  sendKGYRequest();
}
void sendRegistratorRequest() {
  if (!clientRegistrator.connect(registratorIP, serverPort)) {
    opPr = 9999;  // Установка значения в 9999 при ошибке соединения
    resultRegistrator = "Ошибка соединения с регистратором!!! \n Регулирование невозможно.";
    return;
  }

  // Создайте и отправьте запрос к регистратору
  clientRegistrator.onConnect([](void *arg, AsyncClient *c) {
    // Убедитесь, что transactionId определен
    uint16_t transactionId = 7;

    uint8_t request[] = {
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
    uint8_t *response = static_cast<uint8_t *>(data);  // Приведение типа указателя
    // Проверьте, что в ответе достаточно данных перед извлечением
    if (len >= 32) {
      // Извлекаем данные из ответа правильным образом
      uint32_t opPresher = ((uint32_t)response[9] << 24) | ((uint32_t)response[10] << 16) | ((uint32_t)response[11] << 8) | (uint32_t)response[12];
      float floatOpPresher;
      memcpy(&floatOpPresher, &opPresher, sizeof(floatOpPresher));
      floatOpPresher = round(floatOpPresher * 10) / 10;
      opPr = floatOpPresher;

      uint32_t GTS = ((uint32_t)response[17] << 24) | ((uint32_t)response[18] << 16) | ((uint32_t)response[19] << 8) | (uint32_t)response[20];
      float floatGTS;
      memcpy(&floatGTS, &GTS, sizeof(floatGTS));
      gtsPr = round(floatGTS * 10) / 10;

      uint32_t KY = ((uint32_t)response[21] << 24) | ((uint32_t)response[22] << 16) | ((uint32_t)response[23] << 8) | (uint32_t)response[24];
      float floatKY;
      memcpy(&floatKY, &KY, sizeof(floatKY));
      kgyPr = round(floatKY * 10) / 10;

      uint32_t CH4_1 = ((uint32_t)response[25] << 24) | ((uint32_t)response[26] << 16) | ((uint32_t)response[27] << 8) | (uint32_t)response[28];
      float floatCH4_1;
      memcpy(&floatCH4_1, &CH4_1, sizeof(floatCH4_1));
      CH4_1p = round(floatCH4_1 * 10) / 10;

      uint32_t CH4_2 = ((uint32_t)response[29] << 24) | ((uint32_t)response[30] << 16) | ((uint32_t)response[31] << 8) | (uint32_t)response[32];
      float floatCH4_2;
      memcpy(&floatCH4_2, &CH4_2, sizeof(floatCH4_2));
      CH4_2p = round(floatCH4_2 * 10) / 10;

      resultRegistrator = "Давление перед GTS: " + String(floatGTS) + " kPa\n";
      resultRegistrator += "Давление перед КГУ: " + String(floatKY) + " kPa\n";
      resultRegistrator += "Давление после ОП: " + String(floatOpPresher) + " kPa\n";
      resultRegistrator += "СН4 ВНС-№1: " + String(floatCH4_1) + " %\n";
      resultRegistrator += "СН4 ВНС-№2: " + String(floatCH4_2) + " %\n";
    }
    c->stop();
  });
  clientRegistrator.onError([](void *arg, AsyncClient *c, int8_t error) {
    opPr = 9999;  // Установка значения в 9999 при ошибке соединения
    resultRegistrator = "Ошибка соединения с регистратором!!! \n Регулирование невозможно.";
    c->stop();
  });
}
void sendKGYRequest() {
  if (!clientKGY.connect(serverIP, serverPort)) {
    Serial.println("Connection to KGY failed.");
    trottlePosition = 9999;
    powerConstant = 9999;
    powerActive = 9999;
    resultKGY = "Ошибка соединения с КГУ!!! \n Регулирование невозможно.";
    return;
  }
  // Создайте и отправьте запрос к КГУ
  clientKGY.onConnect([](void *arg, AsyncClient *c) {
    transactionId = 1;
    uint8_t request[] = {
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
    //uint8_t *response = static_cast<uint8_t *>(data);
    uint8_t *response = static_cast<uint8_t *>(data);
    //trottlePosition = (((response[8] << 8) | response[9]) / 10);
    uint8_t transactionId = (((response[0] << 8) | response[1]));
    if (transactionId == 1) {
      trottlePosition = (((response[9] << 8) | response[10]) / 10);
      resultKGY = "Положение дросселя КГУ: " + String(trottlePosition) + " %\n";

      transactionId = 2;
      uint8_t request[] = {
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
    } else if (transactionId == 2) {
      powerConstant = (response[9] << 8) | response[10];
      resultKGY += "Заданная мощность: " + String(powerConstant) + " kW\n";

      transactionId = 3;
      uint8_t request[] = {
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
    } else if (transactionId == 3) {
      powerActive = (response[9] << 8) | response[10];
      resultKGY += "Активная мощность: " + String(powerActive) + " kW\n";

      transactionId = 6;
      uint8_t request[] = {
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
    } else if (transactionId == 6) {
      totalGenerated = ((uint32_t)response[9] << 24) | ((uint32_t)response[10] << 16) | ((uint32_t)response[11] << 8) | (uint32_t)response[12];

      regulatePower();

      if (isPower) {
        isPower = false;
        transactionId = 5;
        uint8_t request[] = {
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
      }

      c->stop();
    }
  });
  clientKGY.onError([](void *arg, AsyncClient *c, int8_t error) {
    trottlePosition = 9999;
    powerConstant = 9999;
    powerActive = 9999;
    totalGenerated = 0;
    resultKGY = "Ошибка соединения с КГУ!!! \n Регулирование невозможно.\n";
    c->stop();
  });
}
void pushToFirebase() {
  if (!checkValidData()) {
    return;
  }
  Firebase.setFloat(fbdo, "/opPresher", opPr);
  Firebase.setFloat(fbdo, "/trottlePosition", trottlePosition);
  Firebase.setInt(fbdo, "/powerConstant", powerConstant);
  Firebase.setInt(fbdo, "/powerActive", powerActive);
  Firebase.setFloat(fbdo, "/CH4_1", CH4_1p);
  Firebase.setFloat(fbdo, "/CH4_2", CH4_2p);
  Firebase.setFloat(fbdo, "/gtsPresher", gtsPr);
  Firebase.setFloat(fbdo, "/kgyPresher", kgyPr);
  Firebase.setInt(fbdo, "/totalActivePower", totalGenerated);

  if (Firebase.getInt(fbdo, "/UnixTime")) {
    if (fbdo.dataTypeEnum() == firebase_rtdb_data_type_integer) {
      uint64_t lastSeen = (fbdo.to<int>());
      if (lastSeen != UnixTime) {
        UnixTime = lastSeen;
        appRegulate = true;
      } else {
        appRegulate = false;
      }
    }
  }
}
void monthGenerated() {
  int currentDay = getDayOfMonthFromUnixTime();
  if (currentDay == 1 && day != currentDay) {
    Firebase.setInt(fbdo, "/monthStartGenerated", totalGenerated);
    day = currentDay;
  }
  if (monthStartGenerated == 0) {
    if (Firebase.getInt(fbdo, "/monthStartGenerated")) {
      if (fbdo.dataTypeEnum() == firebase_rtdb_data_type_integer) {
        monthStartGenerated = (fbdo.to<int>());
      }
    }
  }
}
int getDayOfMonthFromUnixTime() {
  stamp.getDateTime(ntpClient.getEpochTime());
  return stamp.day;
}
