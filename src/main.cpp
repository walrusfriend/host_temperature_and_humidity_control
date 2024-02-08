#include <algorithm>
#include <time.h>

#include <EEPROM.h>

#include "BLE.h"
#include "Network.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
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
 * - Проверить, что нигде, кроме ф-ции compare_hum() не происходит управление реле
 * - Доделать сохранение и загрузку параметров сервера
 */

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

bool is_compressor_start = false;
bool is_relay_controlled_by_user = false;

/** TODO: Move this to RelayController */
bool inflow_low_state = false;
bool inflow_high_state = false;
bool exhaust_low_state = false;
bool exhaust_high_state = false;

void compare_hum();

void status_tim_function();
void sensor_tim_function();
void connect_to_wifi();
void sensor_data_send_to_remote_server();
void connect_to_BLE();
void check_COM_port();
void check_BLE_port();

struct Command
{
	std::string name;
	std::function<void(const std::string &)> handler;

	Command(std::string &&command_name, void(*command_handler)(const std::string &))
		: name(command_name)
		, handler(command_handler)
	{}

	~Command() {}
};

/**
 * TODO: Split commands by user and sensor or something else
 */
// Command handler function list
void relay_handler(const std::string &message);
void start_handler(const std::string &message);
void stop_handler(const std::string &message);
void set_wifi_handler(const std::string &message);
void wifi_connect_handler(const std::string &message);
void data_request_handler(const std::string &message);
void set_hub_id_handler(const std::string &message);
void set_sensor_id_handler(const std::string &message);
void set_url_handler(const std::string &message);
void set_establishment_id_handler(const std::string &message);
void ble_handler(const std::string& message);

void parse_message(const std::string &message);

void debug(const std::string &debug_info);

bool is_number(const std::string &s);

// Парсер написан так, что не должно быть команды, которая является
// началом другой команды, вроде get и get_hub. Команда get просто будет
// вызываться раньше.
static const std::vector<Command> command_list = {
	Command("relay", relay_handler),
	Command("start", start_handler),
	Command("stop", stop_handler),
	Command("set_wifi", set_wifi_handler),
	Command("wifi_connect", wifi_connect_handler),
	Command("data_request", data_request_handler),
	Command("set_hub_id", set_hub_id_handler),
	Command("set_sensor_id", set_sensor_id_handler),
	Command("set_url", set_url_handler),
	Command("ble", ble_handler),
	Command("set_establisment_id", set_establishment_id_handler)};

Network network;
// BLE ble;

hw_timer_t *status_timer;
hw_timer_t *sensor_timer;
hw_timer_t *BLE_timeout_timer;

bool is_status_tim = false;
bool is_sensor_tim = false;

void IRAM_ATTR onStatusTimer()
{
	is_status_tim = true;
}

void IRAM_ATTR onSensorTimer()
{
	is_sensor_tim = true;
}

void IRAM_ATTR onBLEtimeout()
{
	// if (ble.pClient->isConnected() == false)
	// {
	// 	ble.do_BLE_connect = true;
	// }

	timerAlarmDisable(BLE_timeout_timer);
}

void setup()
{
	Serial.begin(115200);
	Serial.println("INFO: Start program!");

	// UART port to BLE module
	Serial2.begin(9600);

	// Try to load data from EEPROM
	EEPROM.begin(4096);

	const uint16_t INIT_ADDR = 0;
	const uint8_t INIT_KEY = 123;

	// Load a default data to memory if it's first start
	if (EEPROM.read(INIT_ADDR) != INIT_KEY)
	{
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

	/** TODO: Create full load log */
	Serial.printf("DEBUG: Readed data from EEPROM:\n"
				  "ssid: %s\n"
				  "pass: %s\n"
				  "url: %s\n"
				  "hub id: %d\n",
				  network.wifi_cfg.ssid,
				  network.wifi_cfg.pass,
				  network.server_cfg.url,
				  network.server_cfg.hub_id);

	// BLEDevice::init("DewPoint");
	// Serial.println("INFO: Starting BLE work!");
	// 1s timer
	// This timer initiates reply to check hub state on the server
	status_timer = timerBegin(0, 8000 - 1, true);
	timerAttachInterrupt(status_timer, &onStatusTimer, true);
	timerAlarmWrite(status_timer, 100000 - 1, true);

	// Set timer to 30 secs
	// This timer initiates BLE transmission
	sensor_timer = timerBegin(1, 8000 - 1, true);
	timerAttachInterrupt(sensor_timer, &onSensorTimer, true);
	timerAlarmWrite(sensor_timer, 300000 - 1, true);

	// Set timer to 10 secs
	// This timer starts after we send a data to BLE and wait for response
	// If no response that check BLE connection and reconnect
	// BLE_timeout_timer = timerBegin(2, 8000 - 1, true);
	// timerAttachInterrupt(BLE_timeout_timer, &onBLEtimeout, true);
	// timerAlarmWrite(BLE_timeout_timer, 100000 - 1, true);

	pinMode(RelayController::COMPRESSOR_RELAY, OUTPUT);
}

void loop()
{
	if (network.do_wifi_connect)
		connect_to_wifi();

	if (is_status_tim)
		status_tim_function();

	if (is_sensor_tim)
		sensor_tim_function();

	check_COM_port();

	check_BLE_port();
}

void compare_hum()
{
	// // Forces compressor relay close
	// if (network.relay_status == false) {
	// 	Serial.println("INFO: Force change relay status to off");
	// 	RelayController::off(RelayController::COMPRESSOR_RELAY);
	// 	return;
	// }

	// if (BLE::block_relay == true) {
	// 	Serial.println("INFO: Relay off because BLE connection lost");
	// 	is_compressor_start = false;
	// }

	// if (ble.curr_hum_value > network.hum_max)
	// {
	// 	/** TODO: Activate a heater */
	// 	Serial.println("INFO: A Heater is activated");
	// }

	// if (ble.curr_hum_value < network.hum_min)
	// {
	// 	Serial.println("INFO: A relay is on");
	// 	is_compressor_start = true;
	// }
	
	// if (ble.curr_hum_value > network.hum_max)
	// {
	// 	Serial.println("INFO: A relay is off");
	// 	is_compressor_start = false;
	// }

	// if (is_compressor_start)
	// {
	// 	Serial.println("INFO: Relay status changed to ON");
	// 	RelayController::on(RelayController::COMPRESSOR_RELAY);
	// }
	// else
	// {
	// 	Serial.println("INFO: Relay status changed to OFF");
	// 	RelayController::off(RelayController::COMPRESSOR_RELAY);
	// }
}

void relay_handler(const std::string &message)
{
	// std::string tmp = message.substr(6, message.size() - 6);

	// Serial.printf("%s\n", tmp.c_str());

	// if (tmp == "on")
	// {
	// 	is_compressor_start = true;
	// 	is_relay_controlled_by_user = true;
	// 	ble.BLE_reply = "Set the relay status to ON\n";
	// }
	// else if (tmp == "off")
	// {
	// 	is_compressor_start = false;
	// 	is_relay_controlled_by_user = true;
	// 	ble.BLE_reply = "Set the relay status to OFF\n";
	// }
	// else if (tmp == "auto")
	// {
	// 	is_relay_controlled_by_user = false;
	// 	ble.BLE_reply = "The relay change its state automatically!\n";
	// }
	// else
	// {
	// 	ble.BLE_reply = "ERROR: Unknown argument!\n";
	// }

	// Serial.print(ble.BLE_reply.c_str());
}

void start_handler(const std::string &message)
{
	timerAlarmEnable(status_timer);
}

void stop_handler(const std::string &message)
{
	timerAlarmDisable(status_timer);
}

void set_wifi_handler(const std::string &message)
{
	Serial.println(message.c_str());
	size_t LF_pos = message.find('\n');
	std::string str = message.substr(0, LF_pos);

	Serial.println(str.c_str());

	// Delete \r\n syms
	str.erase(std::remove(str.begin(), str.end(), '\n'), str.cend());
	str.erase(std::remove(str.begin(), str.end(), '\r'), str.cend());

	std::vector<std::string> args;
	size_t pos = str.find(' ');
	size_t initialPos = 0;

	// Decompose statement
	while (pos != std::string::npos)
	{
		args.push_back(str.substr(initialPos, pos - initialPos));
		initialPos = pos + 1;

		pos = str.find(' ', initialPos);
	}

	// Add the last one
	args.push_back(str.substr(initialPos, str.size() - initialPos));

	if (args.size() < 3)
	{
		Serial.println("ERROR: Too few argumets");
		return;
	}

	if (args[1].size() > WIFI_SSID_SIZE) {
		Serial.printf("ERROR: The SSID size must be less than %d\n", WIFI_SSID_SIZE);
		return;
	}

	if (args[2].size() > WIFI_PASS_SIZE) {
		Serial.printf("ERROR: The password size must be less than %d\n", WIFI_PASS_SIZE);
		return;
	}

	Serial.printf("INFO: Parsed SSID: %s and password: %s\n", args[1].c_str(), args[2].c_str());

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

void data_request_handler(const std::string &message)
{
	/** TODO: Check if we connected to BLE server */

	// ble.p_serial_characteristic->writeValue("d");
}

/** TODO: Дописать тело функций */
void set_hub_id_handler(const std::string &message)
{
}

void set_sensor_id_handler(const std::string &message)
{
}

void set_url_handler(const std::string &message)
{
}

void set_establishment_id_handler(const std::string &message)
{
}

void ble_handler(const std::string& message) {
	Serial2.print(message.c_str());
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
	return !s.empty() &&
		   std::find_if(s.begin(),
						s.end(), [](unsigned char c)
						{ return !std::isdigit(c); }) == s.end();
}

void status_tim_function()
{
	// // Check the current connection status
	// if ((WiFi.status() == WL_CONNECTED))
	// {
	// 	Serial.println("INFO: On status handler");
	// 	network.GET_hub();
	// 	Serial.println();
	// }
	// else
	// {
	// 	network.handle_disconnect();
	// }

	// // Check an atmosphere params and the user input and control the compressor relay
	// compare_hum();

	// is_status_tim = false;
}

void sensor_tim_function()
{
	// Serial.println("INFO: On BLE send hanler");
	// // ble.p_serial_characteristic->writeValue("d");
	// is_sensor_tim = false;

	// timerAlarmEnable(BLE_timeout_timer);
}

void connect_to_wifi()
{
	/** TODO: Reconnect does not working */
	Serial.printf("INFO: Try to connect to Wi-Fi with ssid: %s and password: %s\n",
				  network.wifi_cfg.ssid, network.wifi_cfg.pass);

	if (WiFi.mode(WIFI_STA)) {
		Serial.println("INFO: Mode changed successfully!");
	}
	else {
		Serial.println("ERROR: Mode changing failed!");
	}

	uint8_t status = WiFi.begin(network.wifi_cfg.ssid, network.wifi_cfg.pass);

	Serial.printf("INFO: Initial status code - %d\n", status);

	uint8_t wifi_connection_tries = 0;
	bool is_connection_successful = true;

	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.printf("INFO: Wi-Fi status code - %d\n", status);

		if (wifi_connection_tries >= Network::MAX_WIFI_CONNECTION_TRIES)
		{
			Serial.printf("\nERROR: Couldn't connect to Wi-Fi network with ssid: %s and password: %s!\n"
						  "Please restart the device or set other Wi-Fi SSID and password!\n",
						  network.wifi_cfg.ssid, network.wifi_cfg.pass);
			wifi_connection_tries = 0;
			is_connection_successful = false;
			network.handle_disconnect();
			break;
		}

		++wifi_connection_tries;

		Serial.printf(".");
		delay(1000);
	};

	if (is_connection_successful)
	{
		if (network.client)
			network.client->setInsecure();
		else
			Serial.printf("ERROR: [HTTPS] Unable to connect\n");

		Serial.println("INFO: Wi-Fi connected");
		Serial.println();

		network.do_wifi_connect = false;

		timerAlarmEnable(status_timer);
		timerAlarmEnable(sensor_timer);
	}
}

void sensor_data_send_to_remote_server()
{
	// Serial.println("INFO: On BLE data received handler");
	// /** TODO: Нужно посылать значение, пока оно не будет принято.
	//  * 	Возможно, даже складывать значения в очередь, пока всё не будет отправлено.
	//  * 	Также со всеми запросами - нужно удостовериться, что запрос дошёл и, если нет,
	//  * 	то отправлять запрос до тех пор, пока не получится отправить.
	//  * 	Можно генерировать логи, что не было связи с сервером в какое-то время.
	//  * 	Нужно настроить время на борту, чтобы привязывать логи к "бортовому" времени.
	//  */

	// timerAlarmDisable(BLE_timeout_timer);

	// uint8_t step_value = (network.hum_max - network.hum_min) * 0.2;

	// if (step_value < HUMIDITY_SENSOR_ACCURACY)
	// 	step_value = HUMIDITY_SENSOR_ACCURACY;

	// if ((ble.curr_hum_value < network.hum_min + step_value) or
	// 	(ble.curr_hum_value > network.hum_max - step_value))
	// {
	// 	// Send requests every 5 secs
	// 	timerAlarmDisable(sensor_timer);
	// 	timerAlarmWrite(sensor_timer, 50000 - 1, true);
	// 	timerAlarmEnable(sensor_timer);
	// }
	// else
	// {
	// 	// Send requests every 30 secs
	// 	timerAlarmDisable(sensor_timer);
	// 	timerAlarmWrite(sensor_timer, 300000 - 1, true);
	// 	timerAlarmEnable(sensor_timer);
	// }

	// // Check an atmosphere params and the user input and control the compressor relay
	// compare_hum();

	// // Check the current connection status
	// if ((WiFi.status() == WL_CONNECTED))
	// {
	// 	network.POST_hum(ble.curr_hum_value);
	// 	delay(100);
	// 	network.POST_temp(ble.curr_temp_value);
	// 	Serial.println();
	// }
	// else
	// {
	// 	network.handle_disconnect();
	// }
	// ble.is_data_from_BLE_received = false;
}

void connect_to_BLE()
{
	Serial.println("INFO: Try to connect to BLE");
	// uint8_t number_of_unsuccessful_connections = 0;
	// while (ble.connectToServer() == false)
	// {
	// 	++number_of_unsuccessful_connections;

	// 	if (number_of_unsuccessful_connections > 10)
	// 	{
	// 		Serial.println("ERROR: Failed to connect to the BLE server!");
	// 		break;
	// 	}

	// 	delay(100);
	// };
	// Serial.println();
	// ble.do_BLE_connect = false;

	Serial.println("INFO: BLE connected!");
}

void check_COM_port()
{
	if (Serial.available() >= 1)
	{
		// char sym[Serial.available()];
		char sym[256];
		Serial.read(sym, Serial.available());

		parse_message(std::string(sym));
	}
}

void check_BLE_port() {
	if (Serial2.available() >= 1) {
		char sym[256];
		Serial2.read(sym, Serial2.available());

		Serial.print(sym);

		// parse_message(std::string(sym));
	}
}