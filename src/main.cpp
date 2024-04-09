#include <algorithm>
#include <time.h>

#include <EEPROM.h>

#include "main.h"
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
 * - Проверить, что нигде, кроме ф-ции compare_hum() не происходит управление реле
 * - Доделать сохранение и загрузку параметров сервера
 * 
 * - Нужно уменьшить время ожидания BLE до разрыва соединения, а то он думает, 
 * что соединение ещё есть и не даёт подключиться датчику после перезагрузки
 * или датчик должен отправлять метку разрыва сообщения с хостом BLE
 */

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

bool is_compressor_start = false;
bool is_relay_controlled_by_user_through_COM = false;

void compare_hum();

void status_tim_function();
void sensor_tim_function();
void ble_timeout();
void connect_to_wifi();
void sensor_data_send_to_remote_server();
void connect_to_BLE();
void check_COM_port();
void check_BLE_port();

struct Command
{
	std::string name;
	void(*handler)(const std::string&);

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
void ble_off_handler(const std::string& message);
void ble_on_handler(const std::string& message);
void ble_wakeup(const std::string& message);
void ble_data_handler(const std::string& message);
void ble_answer_handler(const std::string& message);
void sensor_freq_request_handler(const std::string& message);
void ble_error_handler(const std::string& message);
void set_freq_handler(const std::string& message);

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
	Command("set_establishment_id", set_establishment_id_handler),
	Command("off_ble", ble_off_handler),
	Command("on_ble", ble_on_handler),
	Command("wakeup_ble", ble_wakeup),
	Command("ble", ble_handler),
	Command("d:", ble_data_handler),
	Command("OK", ble_answer_handler),
	Command("REQ", sensor_freq_request_handler),
	Command("e:", ble_error_handler),
	Command("set_freq", set_freq_handler)
};

Network network;
std::unique_ptr<BLE> ble;

SensorParameters actual_sensor_params;
UserDefinedParameters user_defined_sensor_params;

hw_timer_t *status_timer;
hw_timer_t *BLE_timeout_timer;

bool is_status_tim = false;
bool is_ble_timeout = false;

void IRAM_ATTR onStatusTimer()
{
	is_status_tim = true;
}

void IRAM_ATTR onBLEtimeout()
{
	is_ble_timeout = true;
}

void setup()
{
	Serial.begin(115200);
	Serial.println("INFO: Start program!");

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

	/* Replace pass with '*' */
	std::string hidden_pass = network.wifi_cfg.pass;

	if (hidden_pass.size() < 1) {
		Serial.println("ERROR: Password size must be greater than 1!");
		/** TODO: Handler error */
	}

	for (uint8_t i = 0; i < hidden_pass.size() - 2; ++i)
		hidden_pass[i] = '*';

	/** TODO: Create full load log */
	Serial.printf("DEBUG: Read data from EEPROM:\n"
				  "ssid: %s\n"
				  "pass: %s\n"
				  "url: %s\n"
				  "hub id: %d\n",
				  network.wifi_cfg.ssid,
				//   network.wifi_cfg.pass,
				  hidden_pass.c_str(),
				  network.server_cfg.url,
				  network.server_cfg.hub_id);

	// This timer initiates reply to check hub state on the server
	constexpr uint8_t STATUS_TIMER_SEC = 1;
	constexpr uint32_t STATUS_TIMER_COUNTER = STATUS_TIMER_SEC * 10000;
	status_timer = timerBegin(0, 8000 - 1, true);
	timerAttachInterrupt(status_timer, &onStatusTimer, true);
	timerAlarmWrite(status_timer, STATUS_TIMER_COUNTER - 1, true);

	// This timer starts after we send a data to BLE and wait for response
	// If no response that check BLE connection and reconnect
	constexpr uint8_t BLE_TIMEOUT_TIMER_SEC = 30;
	constexpr uint32_t BLE_TIMEOUT_TIMER_COUNTER = BLE_TIMEOUT_TIMER_SEC * 10000;
	BLE_timeout_timer = timerBegin(1, 8000 - 1, true);
	timerAttachInterrupt(BLE_timeout_timer, &onBLEtimeout, true);
	timerAlarmWrite(BLE_timeout_timer, BLE_TIMEOUT_TIMER_COUNTER - 1, true);

	pinMode(RelayController::COMPRESSOR_RELAY, INPUT);
	pinMode(LED_BUILTIN, OUTPUT);

	connect_to_wifi();
	ble = std::make_unique<BLE>();
}

void loop()
{
	if (network.do_wifi_connect)
		connect_to_wifi();

	if (is_status_tim)
		status_tim_function();

	if (is_ble_timeout)
		ble_timeout();

	check_COM_port();
	check_BLE_port();
}

void compare_hum()
{
	if (is_relay_controlled_by_user_through_COM == false) {
		if (actual_sensor_params.hum < user_defined_sensor_params.hum_min)
		{
			Serial.println("INFO: A relay is on");
			is_compressor_start = true;
		}
		
		if (actual_sensor_params.hum > user_defined_sensor_params.hum_max)
		{
			Serial.println("INFO: A relay is off");
			is_compressor_start = false;

			/** TODO: Activate a heater */
			// Serial.println("INFO: A Heater is activated");
		}

		// Forces compressor relay close (off the compressor)
		if (user_defined_sensor_params.relay_status == false) {
			Serial.println("INFO: Force change relay status to off");
			is_compressor_start = false;
		}
	}

	// Reversed logic - if pin off - LED ON and vice versa
	if (is_compressor_start)
	{
		RelayController::on(RelayController::COMPRESSOR_RELAY);
	}
	else
	{
		RelayController::off(RelayController::COMPRESSOR_RELAY);
	}
}

/* Manual remote compressor relay control (legacy) */
void relay_handler(const std::string &message)
{
	std::string tmp = message.substr(6, message.size() - 6);

	Serial.printf("%s\n", tmp.c_str());

	if (tmp == "on")
	{
		is_compressor_start = true;
		is_relay_controlled_by_user_through_COM = true;
		Serial.println("Set the relay status to ON\n");
		network.POST_log("INFO: Relay has been manually switched by COM port to ON state\n");
	}
	else if (tmp == "off")
	{
		is_compressor_start = false;
		is_relay_controlled_by_user_through_COM = true;
		Serial.println("Set the relay status to OFF\n");
		network.POST_log("INFO: Relay has been manually switched by COM port to OFF state\n");
	}
	else if (tmp == "auto")
	{
		is_relay_controlled_by_user_through_COM = false;
		Serial.println("The relay change its state automatically!\n");
		network.POST_log("INFO: Relay has been manually switched by COM port to AUTO state\n");
	}
	else
	{
		Serial.println("ERROR: Unknown argument!\n");
	}
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
		Serial.println("ERROR: Too few arguments");
		return;
	}

	/** TODO: Add check for too short pass or ssid */

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
	Serial.println("DEBUG: BLE handler");

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

	Serial2.print(args[1].c_str());
}

void ble_off_handler(const std::string& message) {
	// ble->power(false);
}

void ble_on_handler(const std::string& message) {
	ble->power(true);
}

void ble_wakeup(const std::string& message) {
	ble->wake_up();
}

void ble_data_handler(const std::string& message) {
	timerRestart(BLE_timeout_timer);

	Serial.println("DEBUG: In BLE data handler function");
	
	// Extract temp value
	auto start_temp_pos = message.find('t');
	auto end_temp_pos = message.find('h');

	if (start_temp_pos == std::string::npos)  {
		Serial.println("ERROR: Couldn't find the start position of the temperature value in BLE message!");
		network.POST_log("ERROR: Couldn't find the start position of the temperature value in BLE message from sensor!");
		return;
	}

	if (end_temp_pos == std::string::npos) {
		Serial.println("ERROR: Couldn't find the end position of the temperature value in BLE message!");
		network.POST_log("ERROR: Couldn't find the end position of the temperature value in BLE message from sensor!");
		return;
	}

	String str_parsed_temp = message.substr(start_temp_pos + 1, end_temp_pos - start_temp_pos - 1).c_str();

	if (str_parsed_temp.length() == 0) {
		Serial.println("ERROR: Temp string size is 0!");
		return;
	}

	Serial.print("DEBUG: Parsed string temp from original message: ");
	Serial.println(str_parsed_temp);

	int parsed_temp = str_parsed_temp.toInt();
	Serial.print("DEBUG: Parsed int temp value from string temp: ");
	Serial.println(parsed_temp);

	// Extract hum value
	auto start_hum_pos = end_temp_pos;
	auto end_hum_pos = message.size();

	if (start_hum_pos == std::string::npos)  {
		Serial.println("ERROR: Couldn't find the start position of the humidity value in BLE message!");
		network.POST_log("ERROR: Couldn't find the start position of the humidity value in BLE message from sensor!");
		return;
	}

	String str_parsed_hum = message.substr(start_hum_pos + 1, end_hum_pos - start_hum_pos - 1).c_str();

	if (str_parsed_hum.length() == 0) {
		Serial.println("ERROR: Hum string size is 0!");
		return;
	}

	Serial.print("DEBUG: Parsed string hum from original message: ");
	Serial.println(str_parsed_hum);

	int parsed_hum = str_parsed_hum.toInt();
	Serial.print("DEBUG: Parsed int hum value from string hum: ");
	Serial.println(parsed_hum);

	actual_sensor_params.temp = parsed_temp;
	actual_sensor_params.hum = parsed_hum;

	uint8_t humidity_window_value = (user_defined_sensor_params.hum_max - 
						  user_defined_sensor_params.hum_min) * 0.2;

	if (humidity_window_value < HUMIDITY_SENSOR_ACCURACY)
		humidity_window_value = HUMIDITY_SENSOR_ACCURACY;

	// Check that actual hum is on the special window
	if (((actual_sensor_params.hum > (user_defined_sensor_params.hum_min - humidity_window_value)) and
		 actual_sensor_params.hum < (user_defined_sensor_params.hum_min + humidity_window_value)) or
		 actual_sensor_params.hum > (user_defined_sensor_params.hum_max - humidity_window_value) and 
		 actual_sensor_params.hum < (user_defined_sensor_params.hum_max + humidity_window_value))
	{
		// Send requests every 5 
		Serial2.print("S:1");
		ble->BLE_output_buff = "S:1";
		Serial.println("Send S:1 to BLE");
	}
	else {
		// Send requests every 30 secs
		Serial2.print("S:2");
		ble->BLE_output_buff = "S:2";
		Serial.println("Send S:2 to BLE");
	}

	compare_hum();

	// Check the current connection status
	if ((WiFi.status() == WL_CONNECTED))
	{
		network.POST_hum(actual_sensor_params.hum);
		delay(100);
		network.POST_temp(actual_sensor_params.temp);
		Serial.println();
	}
	else
	{
		/** TODO: Save a log to send later */
		network.handle_disconnect();
	}
}

void ble_answer_handler(const std::string& message) {
	/** TODO: Uncomment and implement */
	// Serial.println("\nDEBUG: BLE message arrived:");
	// Serial.print(message.c_str());
}

void sensor_freq_request_handler(const std::string& message) {
	// Serial2.print("REPLY!");
	delay(200);
	Serial2.print(ble->BLE_output_buff.c_str());
	Serial.printf("Send %s to sensor!", ble->BLE_output_buff.c_str());
}

void ble_error_handler(const std::string& message) {
	if (message.size() < 3) {
		Serial.println("ERROR: ble_error_handler - message size is to low!");
		return;
	}

	if (message[2] == '1') {
		// Disable compressor relay
		RelayController::off(RelayController::COMPRESSOR_RELAY);

		Serial.println("Sensor error - the humidity sensor is not available!");
		network.POST_log("ERROR: The humidity sensor is not available!");
	}
}

void set_freq_handler(const std::string& message) {
	// freq 
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
	Serial.println("INFO: Current status check");
	// Check the current connection status
	if ((WiFi.status() == WL_CONNECTED))
	{
		Serial.println("DEBUG: On status handler");
		network.GET_hub(user_defined_sensor_params);
		Serial.println();
		network.POST_log("INFO: A GET HUB request has been sent\n");
	}
	else
	{
		network.handle_disconnect();
	}

	// Check an atmosphere params and the user input and control the compressor relay
	compare_hum();

	is_status_tim = false;
}

void sensor_tim_function()
{
	// Serial.println("INFO: BLE check");
	// Serial.println("INFO: On BLE send handler");
	// // ble.p_serial_characteristic->writeValue("d");
	// is_sensor_tim = false;

	// timerAlarmEnable(BLE_timeout_timer);
}

void ble_timeout() {
	// Disable compressor relay
	RelayController::off(RelayController::COMPRESSOR_RELAY);

	Serial.print("ERROR: There is no connection to the sensor");
	network.POST_log("ERROR: There is no connection to the sensor");

	is_ble_timeout = false;

	timerRestart(BLE_timeout_timer);
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

		network.POST_log("INFO: Wi-Fi connected");
	}
}

void connect_to_BLE()
{
	// Serial.println("INFO: Try to connect to BLE");
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

	// Serial.println("INFO: BLE connected!");
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
		char buff[256];

		uint16_t size = Serial2.available();
		Serial2.read(buff, size);

		buff[size] = '\0';

		/** TODO: Delete this debug print */
		Serial.println("\nDEBUG: Data from BLE:");
		Serial.println(buff);

		parse_message(std::string(buff));
	}
}