#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>
#include <time.h>

// Вводим имя и пароль точки доступа
const char *ssid = "AKADO-9E4C";
const char *password = "90704507";

const String endpoint = "http://api.openweathermap.org/data/2.5/weather?q=Moscow,ru,pt&APPID=";
const String key = "256d7e87b980dc930dc4f460308892e1";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

void handleReceivedMessage(String message)
{
	StaticJsonDocument<1500> doc; // Memory pool. Поставил наугад для демонстрации

	DeserializationError error = deserializeJson(doc, message);

	// Test if parsing succeeds.
	if (error)
	{
		Serial.print(F("deserializeJson() failed: "));
		Serial.println(error.c_str());
		return;
	}

	Serial.println();
	Serial.println("----- DATA FROM OPENWEATHER ----");

	const char *name = doc["name"];
	Serial.print("City: ");
	Serial.println(name);

	int timezone = doc["timezone"];
	Serial.print("Timezone: ");
	Serial.println(timezone);

	int humidity = doc["main"]["humidity"];
	Serial.print("Humidity: ");
	Serial.println(humidity);

	uint16_t sea_level = doc["main"]["sea_level"];
	Serial.print("Sea level: ");
	Serial.println(sea_level);

	Serial.println("------------------------------");
}

void setup()
{
	Serial.begin(115200);

	// подключаемся к Wi-Fi сети
	WiFi.begin(ssid, password);

	/* TODO: Написать нормальный обработчик команд */

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println("Соединяемся с Wi-Fi..");
	}

	Serial.println("Соединение с Wi-Fi установлено");

	bool success = Ping.ping("developer.alexanderklimov.ru", 3);

	if (!success)
	{
		Serial.println("Ping failed");
		return;
	}

	Serial.println("Ping succesful.");

	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop()
{
	// выполняем проверку подключения к беспроводной сети
	if ((WiFi.status() == WL_CONNECTED))
	{ // Check the current connection status
		// создаем объект для работы с HTTP
		HTTPClient http;
		// подключаемся к веб-странице OpenWeatherMap с указанными параметрами
		http.begin(endpoint + key);
		int httpCode = http.GET(); // Делаем запрос
		
		// проверяем успешность запроса
		if (httpCode > 0)
		{ // Check for the returning code
			// выводим ответ сервера
			String payload = http.getString();
			Serial.println(httpCode);
			// Serial.println(payload);
			handleReceivedMessage(payload);
		}
		else
		{
			Serial.println("Ошибка HTTP-запроса");
		}

		struct tm timeinfo;
		if (getLocalTime(&timeinfo))
		{
			char time_str[16];
			strftime(time_str, 16, "%H:%M:%S", &timeinfo);
			Serial.println(time_str);
		}

		http.end(); // освобождаем ресурсы микроконтроллера
	}
	delay(5000);
}