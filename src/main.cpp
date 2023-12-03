#include <algorithm>
#include <time.h>

#include <EEPROM.h>

#include "BLE.h"
#include "Network.h"
#include "RelayController.h"

#define DEBUG 1

/**
 * TODO:
 * - Add checks for all!
 * - Настройка частоты передачи
 * - Включить и выключить передачу
 * - Count the number of connetion tries and handle infinity connetion
 * - Rename variables
 * - Try send data and only then reconnect to BLE server
 * - Refactor code:
 * 	 - move code parts to *.h/*.cpp files
 * 	 -
 */

BLE ble;

uint8_t hum_min = 53;
uint8_t hum_max = 80;

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

bool is_compressor_start = false;
bool is_relay_controlled_by_user = false;

/** TODO: Move this to RelayController */
bool inflow_low_state = false;
bool inflow_high_state = false;
bool exhaust_low_state = false;
bool exhaust_high_state = false;

void compare_hum();

struct Command
{
	std::string name;
	std::function<void(const std::string &)> handler;

	Command(std::string &&command_name, std::function<void(const std::string &)> command_handler)
	{
		name = command_name;
		handler = command_handler;
	}

	~Command() {}
};

/**
 * TODO: Split commands by user and sensor or something else
 */
// Command handler function list
void hum_handler(const std::string &message);
void curr_hum_handler(const std::string &message);
void error_handler(const std::string &message);
void relay_handler(const std::string &message);
void humidity_handler(const std::string &message);
void temperature_handler(const std::string &message);
void inflow_low_handler(const std::string &message);
void inflow_high_handler(const std::string &message);
void exhaust_low_handler(const std::string &message);
void exhaust_high_handler(const std::string &message);
void get_handler(const std::string &message);
void post_handler(const std::string &message);
void rand_handler(const std::string &message);
void start_handler(const std::string &message);
void stop_handler(const std::string &message);
void period_handler(const std::string &message);
void get_hub_handler(const std::string &message);
void set_wifi_handler(const std::string &message);
void wifi_connect_handler(const std::string &message);
void data_request_handler(const std::string &message);
void set_hub_id_handler(const std::string& message);
void set_sensor_id_handler(const std::string& message);
void set_url_handler(const std::string& message);
void set_establishmet_id_handler(const std::string& message);

void parse_message(const std::string &message);

void debug(const std::string &debug_info);

bool is_number(const std::string &s);

// Парсер написан так, что не должно быть команды, которая является
// началом другой команды, вроде get и get_hub. Команда get просто будет
// вызываться раньше.
static const std::vector<Command> command_list = {
	Command("curr_hum", curr_hum_handler),
	Command("Error", error_handler),
	Command("relay", relay_handler),
	Command("Humidity: ", humidity_handler),
	Command("Temperature: ", temperature_handler),
	Command("hum", hum_handler),
	Command("inflow_low", inflow_low_handler),
	Command("inflow_high", inflow_high_handler),
	Command("exhaust_low", exhaust_low_handler),
	Command("exhaust_high", exhaust_high_handler),
	Command("get", get_handler),
	Command("post", post_handler),
	Command("rand", rand_handler),
	Command("start", start_handler),
	Command("stop", stop_handler),
	Command("period", period_handler),
	Command("get_hub", get_hub_handler),
	Command("set_wifi", set_wifi_handler),
	Command("wifi_connect", wifi_connect_handler),
	Command("data_request", data_request_handler),
	Command("set_hub_id", set_hub_id_handler),
	Command("set_sensor_id", set_sensor_id_handler),
	Command("set_url", set_url_handler),
	Command("set_establisment_id", set_establishmet_id_handler)
};

Network network;

hw_timer_t *tim1;
hw_timer_t *tim2;

bool is_status_tim = false;
bool is_sensor_tim = false;

void IRAM_ATTR onStatusTimer()
{
	is_status_tim = true;
}

void IRAM_ATTR onSensorTimer() {
	is_sensor_tim = true;
}

/** TODO: Move this code to the right place */
TaskHandle_t wifi_connect_task_handle;
TaskHandle_t BLE_connect_task_handle;
TaskHandle_t input_serial_task_handle;

void wifi_task_connect(void *param)
{
	Serial.println("Connect to Wi-Fi");

	for (;;)
	{
		if (network.do_wifi_connect)
		{
			Serial.printf("Try to connect to Wi-Fi with ssid: %s and password: %s\n", 
						  network.wifi_cfg.ssid, network.wifi_cfg.pass);
			
			WiFi.mode(WIFI_STA);
			WiFi.begin(network.wifi_cfg.ssid, network.wifi_cfg.pass);

			uint8_t wifi_connection_tries = 0;
			// while (network.wifi_connect() == false)
			while (WiFi.status() != WL_CONNECTED)
			{
				Serial.printf(".");

				if (wifi_connection_tries >= Network::MAX_WIFI_CONNECTION_TRIES) {
					Serial.printf("ERROR: Couldn't connect to Wi-Fi network with ssid: %s and password: %s!\n"
								   "Please restart the device or set other Wi-Fi SSID and password!\n",
								   network.wifi_cfg.ssid, network.wifi_cfg.pass);
					wifi_connection_tries = 0;
					network.do_wifi_connect = false;
				}

				++wifi_connection_tries;

				vTaskDelay(500);
			};

			network.client = new WiFiClientSecure;

			if (network.client)
			{
				// set secure client without certificate
				network.client->setInsecure();
			}
			else
			{
				Serial.printf("ERROR: [HTTPS] Unable to connect\n");
			}

			Serial.println("Connected");
			Serial.println();

			network.do_wifi_connect = false;
		}

		// Check for Wi-Fi disconnetion
		// if (WiFi.status() != WL_CONNECTED) {
		// 	network.handle_disconnect();
		// }

		vTaskDelay(500);
	}
}

void BLE_task_connect(void *param)
{
	BLEDevice::init("DewPoint");
	Serial.println("Starting BLE work!");

	for (;;)
	{
		// If the flag "doConnect" is true then we have scanned for and found the desired
		// BLE Server with which we wish to connect.  Now we connect to it.  Once we are
		// connected we set the connected flag to be true.
		if (ble.do_BLE_connect == true)
		{
			Serial.println("Try to connect to BLE");
			uint8_t number_of_unsuccessful_connections = 0;
			while (ble.connectToServer() == false)
			{
				Serial.printf(".");

				// ++number_of_unsuccessful_connections;

				// if (number_of_unsuccessful_connections > 10) {
				// 	Serial.println("ERROR: Failed to connect to the server!");
				// 	break;
				// }

				vTaskDelay(500);
			};
			Serial.println();
			ble.do_BLE_connect = false;

			Serial.println("Connected!");
		}
		vTaskDelay(500);
	}
}

void task_input_serial(void *param)
{
	for (;;)
	{
		// Handle COM port input data
		if (Serial.available() >= 1)
		{
			char sym[Serial.available()];
			Serial.read(sym, Serial.available());

			parse_message(std::string(sym));
		}

		vTaskDelay(100);
	}
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Start program!");

	// Try to load data from EEPROM
	EEPROM.begin(4096);

	const uint16_t INIT_ADDR = 0;
	const uint8_t INIT_KEY = 123;

	// Load a default data to memory if it's first start
	if (EEPROM.read(INIT_ADDR) != INIT_KEY) {
		EEPROM.write(INIT_ADDR, INIT_KEY);

		uint16_t address_to_write = 1;

		WiFi_Config wifi_cfg;
		strcpy(wifi_cfg.ssid, "blank_ssid");
		strcpy(wifi_cfg.pass, "blank_pass");
		wifi_cfg.mode = WIFI_MODE_STA;

		EEPROM.put(address_to_write, wifi_cfg);

		address_to_write += sizeof(wifi_cfg);
		Serial.printf("DEBUG: Address to write: %d\n", address_to_write);

		RemoteServerConfig server_cfg;
		strcpy(server_cfg.url, "https://serverpd.ru");
		server_cfg.hub_id = 0;
		server_cfg.sensor_id = 0;
		server_cfg.establishment_id = 0;

		EEPROM.put(address_to_write, server_cfg);
		address_to_write += sizeof(server_cfg);
		Serial.printf("DEBUG: Address to write: %d\n", address_to_write);

		EEPROM.commit();
	}

	uint16_t address_to_read = 1;
	EEPROM.get(address_to_read, network.wifi_cfg);
	address_to_read += sizeof(network.wifi_cfg);

	EEPROM.get(address_to_read, network.server_cfg);

	Serial.printf("DEBUG: Readed data from EEPROM:\n"
				  "ssid: %s\n"
				  "pass: %s\n"
				  "url: %s\n"
				  "hub id: %d\n",
				  network.wifi_cfg.ssid,
				  network.wifi_cfg.pass,
				  network.server_cfg.url,
				  network.server_cfg.hub_id);

	xTaskCreate(wifi_task_connect, "wifi_connect", 4096, nullptr, 1, &wifi_connect_task_handle);
	xTaskCreate(BLE_task_connect, "task_connect", 4096, nullptr, 1, &BLE_connect_task_handle);
	xTaskCreate(task_input_serial, "task_input_serial", 4096, nullptr, 2, &input_serial_task_handle);

	delay(2000);

	// 1s timer
	// tim1 = timerBegin(0, 8000 - 1, true);
	// timerAttachInterrupt(tim1, &onStatusTimer, true);
	// timerAlarmWrite(tim1, 10000 - 1, true);
	// timerAlarmEnable(tim1);
	
	// Set timer to 5 mins
	// tim2 = timerBegin(1, 8000 - 1, true);
	// timerAttachInterrupt(tim2, &onSensorTimer, true);
	// timerAlarmWrite(tim2, 50000 - 1, true);
	// timerAlarmEnable(tim2);

	pinMode(22, OUTPUT);
	pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
	// if (is_status_tim)
	// {
		Serial.println("On hub status hanlder");
		// Check the current connection status
		if ((WiFi.status() == WL_CONNECTED))
		{
			network.GET_hub();
		}
		else
		{
			WiFi.reconnect();

			/** TODO: Add a timeout */
			while (WiFi.status() != WL_CONNECTED) {
				Serial.printf(".");
				delay(1000);
			}
		}
		Serial.println();

		delay(500);

	// 	is_status_tim = false;
	// }

	if (is_sensor_tim) {
		Serial.println("On sensor parameters handler");
		ble.p_serial_characteristic->writeValue("d");
		is_sensor_tim = false;
	}

	if (ble.is_data_from_BLE_received) {
		Serial.println("On BLE data recived handler");
		/** TODO: Нужно посылать значение, пока оно не будет принято.
		 * 	Возможно, даже складывать значения в очередь, пока всё не будет отправлено.
		 * 	Также со всеми запросами - нужно удостовериться, что запрос дошёл и, если нет,
		 * 	то отправлять запрос до тех пор, пока не получится отправить.
		 * 	Можно генерировать логи, что не было связи с сервером в какое-то время.
		 * 	Нужно настроить время на борту, чтобы привязывать логи к "бортовому" времени.
		*/
		
		// Check the current connection status
		if ((WiFi.status() == WL_CONNECTED))
		{
			network.POST_hum(ble.curr_hum_value);
			delay(100);
			network.POST_temp(ble.curr_temp_value);
		}
		ble.is_data_from_BLE_received = false;
	}
}

void compare_hum()
{
	if (is_compressor_start)
	{
		// pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, LOW);
		RelayController::on(RelayController::COMPRESSOR_RELAY);
	}
	else
	{
		// pinMode(RELAY_PIN, INPUT);
		RelayController::off(RelayController::COMPRESSOR_RELAY);
	}

	if (is_relay_controlled_by_user)
	{
		return;
	}

	if (ble.curr_hum_value > 80)
	{
		// Serial.println("ALARM!!!");
		// p_to_phone_characteristic->setValue("ALARM!!!");
	}

	if (ble.curr_hum_value < hum_min)
	{
		is_compressor_start = true;
	}

	if (ble.curr_hum_value > hum_max)
	{
		is_compressor_start = false;
	}
}

void hum_handler(const std::string &message)
{
	// Try to find the 'space' sym between 'min' and 'max' values of the
	// user input
	auto space_pos = message.find_last_of(' ');
	if (space_pos == std::string::npos or space_pos == 3 /* Exclude the first 'space' sym */)
	{
		ble.BLE_reply = "ERROR: Unknown command format!\n"
					"Command must looks like 'hum xx xx' where xx - a positive number from 0 to 100!\n";
		debug(ble.BLE_reply);
		return;
	}

	// Split min and max values
	std::string &&tmp_hum_min_str = message.substr(strlen("hum "), space_pos - strlen("hum "));
	std::string &&tmp_hum_max_str = message.substr(space_pos + 1, message.size() - space_pos);

	if (tmp_hum_max_str[0] == '-' or tmp_hum_min_str[0] == '-')
	{
		ble.BLE_reply = "ERROR: The humidity value borders must be a positive number!\n";
		debug(ble.BLE_reply);
		return;
	}

	uint16_t tmp_min_hum_value = 0;
	uint16_t tmp_max_hum_value = 0;

	if (is_number(tmp_hum_min_str))
	{
		tmp_min_hum_value = std::stoi(tmp_hum_min_str);
	}
	else
	{
		ble.BLE_reply = "ERROR: The minimum humidity value is not a number!\n";
		debug(ble.BLE_reply);
		return;
	}

	if (is_number(tmp_hum_max_str))
	{
		tmp_max_hum_value = std::stoi(tmp_hum_max_str);
	}
	else
	{
		ble.BLE_reply = "ERROR: The maximum humidity value is not a number!\n";
		debug(ble.BLE_reply);
		return;
	}

	if (tmp_min_hum_value > tmp_max_hum_value)
	{
		ble.BLE_reply = "ERROR: The lower border must be less than the higher border!\n";
		debug(ble.BLE_reply);
		return;
	}
	else if (tmp_max_hum_value > 100)
	{
		ble.BLE_reply = "ERROR: The maximum humidity value must be less or equal than 100!\n";
		debug(ble.BLE_reply);
		return;
	}

	hum_min = tmp_min_hum_value;
	hum_max = tmp_max_hum_value;

	ble.BLE_reply = "New humidity border values is " + std::to_string(hum_min) +
				" and " + std::to_string(hum_max) + '\n';
	Serial.print(ble.BLE_reply.c_str());
}

void curr_hum_handler(const std::string &message)
{
	if (ble.curr_hum_value == 0xFF)
		ble.BLE_reply = "ERROR: No humidity value from sensor!\n";
	else
		ble.BLE_reply = std::to_string(ble.curr_hum_value) + '\n';

	Serial.print(ble.BLE_reply.c_str());
}

void error_handler(const std::string &message)
{
	ble.BLE_reply = std::move(message);
	Serial.print(ble.BLE_reply.c_str());
}

void relay_handler(const std::string &message)
{
	std::string tmp = message.substr(6, message.size() - 6);

	Serial.printf("%s\n", tmp.c_str());

	if (tmp == "on")
	{
		is_compressor_start = true;
		is_relay_controlled_by_user = true;
		ble.BLE_reply = "Set the relay status to ON\n";
	}
	else if (tmp == "off")
	{
		is_compressor_start = false;
		is_relay_controlled_by_user = true;
		ble.BLE_reply = "Set the relay status to OFF\n";
	}
	else if (tmp == "auto")
	{
		is_relay_controlled_by_user = false;
		ble.BLE_reply = "The relay change its state automatically!\n";
	}
	else
	{
		ble.BLE_reply = "ERROR: Unknown argument!\n";
	}

	Serial.print(ble.BLE_reply.c_str());
}

/** TODO: Remake a algorythm - sensor sends a headher of the message,
 * humidity and temperature handlers will be united to one*/
void humidity_handler(const std::string &message)
{
	/** TODO: Now algorythm support only two-digit numbers, fix it */
	// std::string&& tmp_str = message.substr(strlen("Humidity: "), 2);
	std::string &&tmp_str = message.substr(strlen("Humidity: "), 2);
	if (is_number(tmp_str))
	{
		ble.curr_hum_value = std::stoi(tmp_str);
		ble.BLE_reply = message;
		debug(ble.BLE_reply);
	}
	else
	{
		ble.BLE_reply = "ERROR: Check the raw input from the sensor:\n" + message + '\n' + tmp_str;
		debug(ble.BLE_reply);
		return;
	}
}

void temperature_handler(const std::string &message)
{
	/** TODO: Now algorythm support only two-digit numbers, fix it */
	std::string &&tmp_str = message.substr(strlen("Temperature: "), 2);
	if (is_number(tmp_str))
	{
		ble.curr_temp_value = std::stoi(tmp_str);
	}
	else
	{
		ble.BLE_reply = "ERROR: Check the raw input from the sensor:\n" + message;
		debug(ble.BLE_reply);
		return;
	}
}

void inflow_low_handler(const std::string &message)
{
	uint8_t header_size = strlen("inflow_low ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	debug(tmp);

	if (tmp == "on")
	{
		if (inflow_high_state == true)
		{
			RelayController::off(RelayController::INFLOW_RELAY_HIGH);
			inflow_high_state = false;
		}

		RelayController::on(RelayController::INFLOW_RELAY_LOW);

		inflow_low_state = true;

		ble.BLE_reply = "Inflow low changed status to ON\n";
		debug(ble.BLE_reply);
	}
	else if (tmp == "off")
	{
		RelayController::off(RelayController::INFLOW_RELAY_LOW);

		inflow_high_state = false;

		ble.BLE_reply = "Inflow low changed status to OFF\n";
		debug(ble.BLE_reply);
	}
	else
	{
		ble.BLE_reply = "ERROR: Invalid argument!\n";
		debug(ble.BLE_reply);
	}
}

void inflow_high_handler(const std::string &message)
{
	uint8_t header_size = strlen("inflow_high ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	if (tmp == "on")
	{
		if (inflow_low_state == true)
		{
			RelayController::off(RelayController::INFLOW_RELAY_LOW);
			inflow_low_state = false;
		}

		RelayController::on(RelayController::INFLOW_RELAY_HIGH);

		inflow_high_state = true;

		ble.BLE_reply = "Inflow high changed status to ON\n";
		debug(ble.BLE_reply);
	}
	else if (tmp == "off")
	{
		RelayController::off(RelayController::INFLOW_RELAY_HIGH);

		inflow_high_state = false;

		ble.BLE_reply = "Inflow high changed status to OFF\n";
		debug(ble.BLE_reply);
	}
	else
	{
		ble.BLE_reply = "ERROR: Invalid argument!\n";
		debug(ble.BLE_reply);
	}
}

void exhaust_low_handler(const std::string &message)
{
	uint8_t header_size = strlen("exhaust_low ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	if (tmp == "on")
	{
		if (exhaust_high_state == true)
		{
			RelayController::off(RelayController::EXHAUST_RELAY_HIGH);
			exhaust_high_state = false;
		}

		RelayController::on(RelayController::EXHAUST_RELAY_LOW);

		exhaust_low_state = true;

		ble.BLE_reply = "Exhaust low changed status to ON\n";
		debug(ble.BLE_reply);
	}
	else if (tmp == "off")
	{
		RelayController::off(RelayController::EXHAUST_RELAY_LOW);

		exhaust_low_state = false;

		ble.BLE_reply = "Exhaust low changed status to OFF\n";
		debug(ble.BLE_reply);
	}
	else
	{
		ble.BLE_reply = "ERROR: Invalid argument!\n";
		debug(ble.BLE_reply);
	}
}

void exhaust_high_handler(const std::string &message)
{
	uint8_t header_size = strlen("exhaust_high ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	if (tmp == "on")
	{
		if (exhaust_low_state == true)
		{
			RelayController::off(RelayController::EXHAUST_RELAY_LOW);
			exhaust_low_state = false;
		}

		RelayController::on(RelayController::EXHAUST_RELAY_HIGH);

		exhaust_high_state = true;

		ble.BLE_reply = "Exhaust high changed status to ON\n";
		debug(ble.BLE_reply);
	}
	else if (tmp == "off")
	{
		RelayController::off(RelayController::EXHAUST_RELAY_HIGH);

		exhaust_high_state = false;

		ble.BLE_reply = "Exhaust high changed status to OFF\n";
		debug(ble.BLE_reply);
	}
	else
	{
		ble.BLE_reply = "ERROR: Invalid argument!\n";
		debug(ble.BLE_reply);
	}
}

void get_handler(const std::string &message)
{
	// Serial.print("ECHO: ");
	// Serial.println(message.c_str());

	// std::string args;
	// uint8_t header_size = strlen("get ");
	// if (header_size < message.size()) {
	// 	args = message.substr(header_size, message.size() - header_size);
	// }

	// String endpoint;

	// // Check the current connection status
	// if ((WiFi.status() == WL_CONNECTED))
	// {
	// 	HTTPClient http;

	// 	// http.begin(url);
	// 	http.begin("http://62.84.117.245:8000/hub/hub?establishment_id=1");
	// 	int httpCode = http.GET(); // Делаем запрос

	// 	if (httpCode > 0)
	// 	{
	// 		String payload = http.getString();
	// 		Serial.println(httpCode);
	// 		Serial.println(payload);
	// 	}
	// 	else
	// 	{
	// 		Serial.println("HTTP-request error");
	// 	}

	// 	http.end();
	// }
}

void post_handler(const std::string &message)
{
	// Serial.print("ECHO: ");
	// Serial.println(message.c_str());

	// std::string args;
	// uint8_t header_size = strlen("post ");
	// if (header_size < message.size())
	// {
	// 	args = message.substr(header_size, message.size() - header_size);
	// }

	// if (args == "log")
	// {
	// 	POST_log();
	// }
	// else if (args == "temp")
	// {
	// 	POST_temp();
	// }
	// else if (args == "hum")
	// {
	// 	POST_hum();
	// }
	// else
	// {
	// 	Serial.println("Unkwonw argument!");
	// }
}

void rand_handler(const std::string &message)
{
	Serial.println(random(1, 5));
}

void start_handler(const std::string &message)
{
	timerAlarmEnable(tim1);
}

void stop_handler(const std::string &message)
{
	timerAlarmDisable(tim1);
}

void period_handler(const std::string &message)
{
	uint8_t header_size = strlen("period ");
	std::string tmp = message.substr(header_size, (message.size() - 2) - header_size);

	Serial.println(tmp.c_str());

	if (is_number(tmp))
	{
		uint32_t timer_value_ms = std::stoi(tmp);
		timerAlarmWrite(tim1, timer_value_ms * 10 - 1, true);
		Serial.printf("Set timer period to %d ms\n", timer_value_ms);
	}
	else
	{
		Serial.println("ERROR: Argument must be a number!");
	}

	/** TODO: Calculate timer parameters */
}

void get_hub_handler(const std::string &message)
{
	network.GET_hub();
}

void set_wifi_handler(const std::string &message)
{
	std::string str = message;
	// Delete \r\n syms
	str.erase(std::remove(str.begin(), str.end(), '\n'), str.cend());
	str.erase(std::remove(str.begin(), str.end(), '\r'), str.cend());

	std::vector<std::string> args;
	size_t pos = str.find(' ');
	size_t initialPos = 0;

	// Decompose statement
	while( pos != std::string::npos ) {
	    args.push_back(str.substr(initialPos, pos - initialPos));
	    initialPos = pos + 1;

	    pos = str.find(' ', initialPos );
	}

	// Add the last one
	args.push_back(str.substr(initialPos, str.size() - initialPos));

	if (args.size() < 3) {
		Serial.println("ERROR: Too few argumets");
		return;
	}

	// if (args[1].size() > mem_wifi_ssid.size) {
	// 	Serial.printf("The SSID size must be less than %d\n", mem_wifi_ssid.size);
	// 	return;
	// }

	// if (args[2].size() > mem_wifi_pass.size) {
	// 	Serial.printf("The password size must be less than %d\n", mem_wifi_pass.size);
	// 	return;
	// }

	Serial.printf("Parsed SSID: %s and password: %s\n", args[1].c_str(), args[2].c_str());

	strcpy(network.wifi_cfg.ssid, args[1].c_str());
	strcpy(network.wifi_cfg.pass, args[2].c_str());

	EEPROM.put(1, network.wifi_cfg);
	EEPROM.commit();

	WiFi.reconnect();
	network.do_wifi_connect = true;
}

void wifi_connect_handler(const std::string &message)
{
	network.do_wifi_connect = true;
}

void data_request_handler(const std::string &message) {
	/** TODO: Check if we connected to BLE server */

	ble.p_serial_characteristic->writeValue("d");
}

/** TODO: Дописать тело функций */
void set_hub_id_handler(const std::string& message) {

}

void set_sensor_id_handler(const std::string& message) {

}

void set_url_handler(const std::string& message) {

}

void set_establishmet_id_handler(const std::string& message) {

}

/** TODO: This will be work only if the message starts with a command*/
void parse_message(const std::string &message)
{
	for (uint8_t i = 0; i < command_list.size(); ++i)
	{
		if (message.find(command_list[i].name) != std::string::npos)
		{
			command_list[i].handler(message);
			return;
		}
	}

	Serial.println("ERROR: Unknown command!");
}

#if DEBUG == 1
void debug(const std::string &debug_info)
{
	Serial.printf("DEBUG: %s\n", debug_info.c_str());
}
#else
void debug(const std::string &debug_info) {}
#endif

bool is_number(const std::string &s)
{
	return !s.empty() && std::find_if(s.begin(),
									  s.end(), [](unsigned char c)
									  { return !std::isdigit(c); }) == s.end();
}