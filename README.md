# ESP8266
OperatorESP8266 is the final version of the program. It contains code for connection between KTP, KGY and PTM59. All other versions are not updated to current functionality.

TelegramBot is Modbus TCP comunication program.
ESP8266 communicate with PLC via Modbus TCP async reqwest. ESP8266 receive commands via Telegram Bot. Implimenteited commands : 
- /report  - recive report every hour;
- /regulate - automatic regulate;
- /status - recive report;
- /base- disable FireBase.

NoTelegramBot is demo version vithout telegram bot function.

