
#define USE_CLIENTSSL true
#include <AsyncTelegram2.h>
// Timezone definition
#include <time.h>
#define MYTZ "EET-2EEST,M3.5.0,M10.5.0/3"

#include <EasyNTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
WiFiUDP udp;
// Устанавливаем поправку на часовой пояс для Украины (GMT+2)
EasyNTPClient ntpClient(udp, "pool.ntp.org", 2 * 60 * 60);

BearSSL::WiFiClientSecure client;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);

const IPAddress serverIP(172, 30, 40, 50);     // IP-адрес сервера Modbus
const IPAddress registratorIP(10, 70, 0, 28);  // IP-адрес сервера Modbus

const int serverPort = 502;  // Порт Modbus
int transactionId = 0;       // Уникальный идентификатор транзакции

AsyncTelegram2 myBot(client);
AsyncClient clientActive;
AsyncClient clientConstant;
AsyncClient clientRegistrator;
AsyncClient clientKGY;


const char *ssid = "***";                                        // SSID WiFi network
const char *pass = "***";                                        // Password  WiFi network
const char *token = "***";  // Telegram token
const char *channel = "***";
int64_t userid = ***;

String resultRegistrator;
String resultKGY;


uint16_t powerActive = 9999;
uint16_t trottlePosition = 9999;
uint16_t powerConstant = 9999;
float opPr = 9999;
int maxPower = 1560;
int reg = 10;

int hours;
int currentHour;
bool hourReport = false;
bool regulate = false;
bool alarm = false;
bool alarmMsg = false;
bool skipFirst = false;

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

  digitalWrite(LED_BUILTIN, true);
}

void loop() {

  static uint32_t hourTime = millis();
  if (millis() - hourTime > 60000 && hourReport) {
    hourTime = millis();
    currentHour = (ntpClient.getUnixTime() / 3600) % 24;
    blink();
  }

  static uint32_t ledTime = millis();
  if (millis() - ledTime > 5000 && regulate) {
    ledTime = millis();
    regulatePower();
    getDate();
    blink();
  }

  TBMessage msg;
  if (myBot.getNewMessage(msg) || (((currentHour - hours) == 1) || ((currentHour - hours) == -23)) && hourReport) {
    blink();
    if (msg.text == "/report_enable@KGY_operator_bot" || msg.text == "/report_enable") {
      hours = (ntpClient.getUnixTime() / 3600) % 24;
      currentHour = hours;
      blink();
      hourReport = true;
      myBot.sendToChannel(channel, "Опция отчетности включена", true);
    } else if (msg.text == "/report_disable@KGY_operator_bot" || msg.text == "/report_disable") {
      blink();
      hourReport = false;
      myBot.sendToChannel(channel, "Опция отчетности отключена", true);
    } else if (msg.text == "/regulate_enable@KGY_operator_bot" || msg.text == "/regulate_enable") {
      blink();
      regulate = true;
      myBot.sendToChannel(channel, "Опция регулирования включена", true);
    } else if (msg.text == "/regulate_disable@KGY_operator_bot" || msg.text == "/regulate_disable") {
      blink();
      regulate = false;
      myBot.sendToChannel(channel, "Опция регулирования отключена", true);
    } else if (msg.text == "/status" || msg.text == "/status@KGY_operator_bot" || (((currentHour - hours) == 1) || ((currentHour - hours) == -23)) && hourReport) {
      digitalWrite(LED_BUILTIN, false);
      if (!regulate) {
        getDate();
        delay(5000);
      }
      String message;
      message = resultKGY;
      message += resultRegistrator;
      //message += "UnixTime: " + String(currentHour - hours) + "\n";
      myBot.sendToChannel(channel, message, true);
      if((((currentHour - hours) == 1) || ((currentHour - hours) == -23)) && hourReport){
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
  WiFiClient client;
  if (!client.connect(serverIP, serverPort)) {
    Serial.println("Connection failed.");
    return;
  }
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
    (uint8_t)(power >> 8),            // Данные первый байт
    (uint8_t)(power & 0xFF)           // Данные второй байт
  };
  client.write(request, sizeof(request));
  client.stop();
  delay(100);
}
void regulatePower() {
  static uint32_t powerUpTime = millis();
  static uint32_t lastRegulate = millis();

  if(trottlePosition > 100 || trottlePosition < 0 ||  powerConstant > 1560 || powerConstant < 0 || powerActive > 2000 || powerActive < -200 || opPr > 40 || opPr < -5) {
      if(skipFirst){
        myBot.sendTo(userid, "trottlePosition = " + String(trottlePosition) + "\npowerConstant = " + String(powerConstant) + 
      "\npowerActive = " + String(powerActive) + "\nopPr = " + String(opPr));
      }
      skipFirst = true;
      return;
  }

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
    maxPower = 800;
    myBot.sendToChannel(channel, "КГУ остановленно!! \n Так и задумано?", true);
    blink();
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
      floatGTS = round(floatGTS * 10) / 10;

      uint32_t KY = ((uint32_t)response[21] << 24) | ((uint32_t)response[22] << 16) | ((uint32_t)response[23] << 8) | (uint32_t)response[24];
      float floatKY;
      memcpy(&floatKY, &KY, sizeof(floatKY));
      floatKY = round(floatKY * 10) / 10;

      uint32_t CH4_1 = ((uint32_t)response[25] << 24) | ((uint32_t)response[26] << 16) | ((uint32_t)response[27] << 8) | (uint32_t)response[28];
      float floatCH4_1;
      memcpy(&floatCH4_1, &CH4_1, sizeof(floatCH4_1));
      floatCH4_1 = round(floatCH4_1 * 10) / 10;

      uint32_t CH4_2 = ((uint32_t)response[29] << 24) | ((uint32_t)response[30] << 16) | ((uint32_t)response[31] << 8) | (uint32_t)response[32];
      float floatCH4_2;
      memcpy(&floatCH4_2, &CH4_2, sizeof(floatCH4_2));
      floatCH4_2 = round(floatCH4_2 * 10) / 10;

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
      (uint8_t)(9202 >> 8),             // Старший байт адреса регистра
      (uint8_t)(9202 & 0xFF),           // Младший байт адреса регистра
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
      trottlePosition = (((response[8] << 8) | response[9]) / 10);
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
      c->stop();
    }
  });
  clientKGY.onError([](void *arg, AsyncClient *c, int8_t error) {
    trottlePosition = 9999;
    powerConstant = 9999;
    powerActive = 9999;
    resultKGY = "Ошибка соединения с КГУ!!! \n Регулирование невозможно.\n";
    c->stop();
  });
}
