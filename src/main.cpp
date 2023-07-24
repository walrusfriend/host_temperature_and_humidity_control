#include "html_code.h"
// #include <WebServer.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "parser.h"

const char *ssid = "MAIN_NODE";
const char *pass = "qwertyqwerty";

// WebServer server(80); // Default HTTP port

// String header;

// String output_led_state = "off";

// IPAddress local_ip(192, 168, 0, 111);
// IPAddress local_gw(192, 168, 0, 1);
// IPAddress local_nm(255, 255, 255, 0);

int LEDB = LOW;
int counter = 0;

// void base() {
// 	server.send(200, "text.html", page);
// }

// void LEDBfunct() {
// 	LEDB = !LEDB;
// 	digitalWrite(LED_BUILTIN, LEDB);
// 	++counter;
// 	String str = "ON";
// 	if (LEDB == LOW) str == "OFF";
// 	server.send(200, "text/html", str);
// }

// void zeroFunct() {
// 	counter = 0;
// 	String str = String(counter);
// 	server.send(200, "text/html", str);
// }

// void countFunct() {
// 	String str = String(counter);
// 	server.send(200, "text/plain", str);
// }

unsigned long previousMillisReconnect;
bool SlaveConnected;
int recatt = 0;

// BT: Bluetooth availabilty check
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

String myName = "ESP32-BT-Master";
String slaveName = "ESP32-BT-Slave";
String MACadd = "C8:F0:9E:F8:03:E2";
uint8_t address[6] = {0xC8, 0xF0, 0x9E, 0xF8, 0x03, 0xE2};

BluetoothSerial SerialBT;

static constexpr uint8_t RELAY_PIN = 16;
uint8_t hum_min = 10;
uint8_t hum_max = 70;
uint8_t curr_hum = 50;
void compare_hum();

struct ringbuf_t* BT_rb;
struct ringbuf_t* serial_rb;
Parser parser;

void Bt_Status(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
	if (event == ESP_SPP_OPEN_EVT)
	{
		Serial.println("Client Connected");
		SlaveConnected = true;
		recatt = 0;
	}
	else if (event == ESP_SPP_CLOSE_EVT)
	{
		Serial.println("Client Disconnected");
		SlaveConnected = false;
	}
}

void SlaveConnect()
{
	Serial.println("Function BT connection executed");
	Serial.printf("Connecting to slave BT device named \"%s\" and MAC address \"%s\" is started.\n", slaveName.c_str(), MACadd.c_str());
	SerialBT.connect(address);
}

void printDeviceAddress()
{
	const uint8_t *point = esp_bt_dev_get_address();
	for (int i = 0; i < 6; i++)
	{
		char str[3];
		sprintf(str, "%02X", (int)point[i]);
		Serial.print(str);
		if (i < 5)
		{
			Serial.print(":");
		}
	}
}

void setup()
{
	Serial.begin(115200);


	// Host WiFi
	// WiFi.mode(WIFI_AP);
	// delay(1000);
	// WiFi.softAP(ssid, pass);
	// WiFi.softAPConfig(local_ip, local_gw, local_nm);

	// Serial.print("IP Address");
	// Serial.println(local_ip);

	// server.begin();
	// server.on("/", base);
	// server.on("/LEDBurl", LEDBfunct);
	// server.on("/zeroUrl", zeroFunct);
	// server.on("/countUrl", countFunct);

	SlaveConnected = false;

	SerialBT.register_callback(Bt_Status);
	SerialBT.begin(myName, true);

	printDeviceAddress();
	
	Serial.printf("The device \"%s\" started in master mode, make sure slave BT device is on!\n", myName.c_str());
	SlaveConnect();
}

void loop()
{
	if (!SlaveConnected)
	{
		if (millis() - previousMillisReconnect >= 10000)
		{
			previousMillisReconnect = millis();
			recatt++;
			Serial.print("Trying to reconnect. Attempt No.: ");
			Serial.println(recatt);
			Serial.println("Stopping Bluetooth...");
			SerialBT.end();
			Serial.println("Bluetooth stopped !");
			Serial.println("Starting Bluetooth...");
			SerialBT.begin(myName, true);
			Serial.printf("The device \"%s\" started in master mode, make sure slave BT device is on!\n", myName.c_str());
			SlaveConnect();
		}
	}

	if (Serial.available())
	{
		char ch = Serial.read();
		ringbuf_memcpy_into(serial_rb, &ch, 1);
		SerialBT.write(ch);
	}
	if (SerialBT.available())
	{
		char ch = SerialBT.read();
		ringbuf_memcpy_into(BT_rb, &ch, 1);
		Serial.write(ch);
	}

	parser.parse_BT(curr_hum);
	parser.parse_serial();

	compare_hum();

	// server.handleClient();

	// WiFiClient client = server.available(); // прослушка входящих клиентов
	// if (client)
	// {								   // Если подключается новый клиент,
	// 	Serial.println("New Client."); // выводим сообщение
	// 	String currentLine = "";
	// 	while (client.connected())
	// 	{ // цикл, пока есть соединение клиента
	// 		if (client.available())
	// 		{							// если от клиента поступают данные,
	// 			char c = client.read(); // читаем байт, затем
	// 			Serial.write(c);		// выводим на экран
	// 			header += c;
	// 			if (c == '\n')
	// 			{ // если байт является переводом строки
	// 				// если пустая строка, мы получили два символа перевода строки
	// 				// значит это конец HTTP-запроса, формируем ответ сервера:
	// 				if (currentLine.length() == 0)
	// 				{
	// 					// HTTP заголовки начинаются с кода ответа (напр., HTTP / 1.1 200 OK)
	// 					// и content-type, затем пустая строка:
	// 					client.println("HTTP/1.1 200 OK");
	// 					client.println("Content-type:text/html");
	// 					client.println("Connection: close");
	// 					client.println();

	// 					// Включаем или выключаем светодиоды
	// 					if (header.indexOf("GET /26/on") >= 0)
	// 					{
	// 						Serial.println("GPIO 26 on");
	// 						output_led_state = "on";
	// 						digitalWrite(LED_BUILTIN, HIGH);
	// 					}
	// 					else if (header.indexOf("GET /26/off") >= 0)
	// 					{
	// 						Serial.println("GPIO 26 off");
	// 						output_led_state = "off";
	// 						digitalWrite(LED_BUILTIN, LOW);
	// 					}
	// 					// Формируем веб-страницу на сервере
	// 					// client.println("<!DOCTYPE html><html>");
	// 					// client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
	// 					// client.println("<link rel=\"icon\" href=\"data:,\">");
	// 					// // CSS для кнопок
	// 					// // можете менять под свои нужды
	// 					// client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
	// 					// client.println(".button { background-color: #4CAF50; border:  none; color: white; padding: 16px 40px;");
	// 					// client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
	// 					// client.println(".button2 {background-color:#555555;}</style></head>");
	// 					// client.println("<body><h1>ESP32 Web Server</h1>");
	// 					// // Выводим текущее состояние кнопок
	// 					// client.println("<p>GPIO 26 - State " + output_led_state + "</p>");
	// 					// Если output26State сейчас off, то выводим надпись ON
	// 					// if (output_led_state == "off")
	// 					// {
	// 					// 	client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
	// 					// }
	// 					// else
	// 					// {
	// 					// 	client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
	// 					// }
	// 					// client.println("</body></html>");
	// 					// HTTP-ответ завершается пустой строкой
	// 					// client.println();
	// 					break;
	// 				}
	// 				else
	// 				{ // если получили новую строку, очищаем currentLine
	// 					currentLine = "";
	// 				}
	// 			}
	// 			else if (c != '\r')
	// 			{					  // Если получили что-то ещё кроме возврата строки,
	// 				currentLine += c; // добавляем в конец currentLine
	// 			}
	// 		}
	// 	}
	// 	// Очистим переменную
	// 	header = "";
	// 	// Закрываем соединение
	// 	client.stop();
	// 	Serial.println("Client disconnected.");
	// 	Serial.println("");
	// }
}

void compare_hum() {
	if (curr_hum > 80) {
		Serial.println("ALARM!!!");
	}

	if (curr_hum > hum_max or curr_hum < hum_min) {
		pinMode(RELAY_PIN, OUTPUT);
		digitalWrite(RELAY_PIN, LOW);
	}
	else {
		pinMode(RELAY_PIN, INPUT);
	}
}