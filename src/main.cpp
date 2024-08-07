#define DEBUG 1
#define ONLY_WIFI 1

#include "main.h"

#include <algorithm>
#include <time.h>

#if ONLY_WIFI == 0
#include "BLE.h"
#endif

#include "Network.h"
#include "RelayController.h"
#include "Calendar.h"


/**
 * TODO:
 * - Add checks for all!
 * - Настройка частоты передачи
 * - Включить и выключить передачу
 * - Count the number of connection tries and handle infinity connection
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
 * 
 * - Create COM port class (maybe)
 */

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

bool is_compressor_start = false;
bool is_relay_controlled_by_user_through_COM = false;

void compare_hum();

void status_tim_function();
void ntp_tim_function();
void check_COM_port();

#if ONLY_WIFI == 0
void ble_timeout();
void check_BLE_port();
#endif

struct Command
{
	std::string name;
	void(*handler)(const std::string&);

	/** TODO: Replace string with string_view */
	Command(std::string &&command_name, void(*command_handler)(const std::string &))
		: name(command_name)
		, handler(command_handler)
	{}

	~Command() {}
};

#if ONLY_WIFI == 0
struct BLE_Command
{
	String name;
	void(*handler)(const std::string&);

	BLE_Command(String &&command_name, void(*command_handler)(const std::string &))
		: name(command_name)
		, handler(command_handler)
	{}

	~BLE_Command() {}
};
#endif

/**
 * TODO: Split commands by user and sensor or something else
 */
// Command handler function list
void set_wifi_handler(const std::string &message);
void set_hub_id_handler(const std::string &message);
void set_sensor_id_handler(const std::string &message);
void set_url_handler(const std::string &message);
void set_establishment_id_handler(const std::string &message);

#if ONLY_WIFI == 0
void ble_handler(const std::string& message);
void ble_data_handler(const std::string& message);
void ble_answer_handler(const std::string& message);
void sensor_freq_request_handler(const std::string& message);
void ble_error_handler(const std::string& message);
#endif

void parse_message(const std::string &message);

void debug(const std::string &debug_info);

bool is_number(const std::string &s);

// Парсер написан так, что не должно быть команды, которая является
// началом другой команды, вроде get и get_hub. Команда get просто будет
// вызываться раньше.
static const std::vector<Command> command_list = {
	Command("set_wifi", set_wifi_handler),
	Command("set_hub_id", set_hub_id_handler),
	Command("set_sensor_id", set_sensor_id_handler),
	Command("set_url", set_url_handler),
	Command("set_establishment_id", set_establishment_id_handler),
	#if ONLY_WIFI == 0
	Command("ble", ble_handler),
	#endif
};

#if ONLY_WIFI == 0
static const std::vector<BLE_Command> ble_command_list = {
	BLE_Command("d:", ble_data_handler),
	BLE_Command("OK", ble_answer_handler),
	BLE_Command("REQ", sensor_freq_request_handler),
	BLE_Command("e:", ble_error_handler)
};
#endif

Network network;

#if ONLY_WIFI == 0
std::unique_ptr<BLE> ble;
String ble_input;
hw_timer_t *BLE_timeout_timer;
bool is_ble_timeout = false;
#endif

SensorParameters actual_sensor_params;
UserDefinedParameters user_defined_sensor_params;

hw_timer_t *status_timer;
bool is_status_tim = false;

void IRAM_ATTR onStatusTimer()
{
	is_status_tim = true;
}

#if ONLY_WIFI == 0
void IRAM_ATTR onBLEtimeout()
{
	is_ble_timeout = true;
}
#endif

hw_timer_t *ntp_timer;
bool is_ntp_tim = false;

void IRAM_ATTR onNtpTimer() {
	is_ntp_tim = true;
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

	if (hidden_pass.size() < 8) {
		Serial.println("ERROR: Password size must be greater or equal to 8!");
		/** TODO: Handler error */
	}

	for (uint8_t i = 0; i < hidden_pass.size(); ++i)
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
	constexpr uint8_t STATUS_TIMER_SEC = 5;
	constexpr uint32_t STATUS_TIMER_COUNTER = STATUS_TIMER_SEC * 10000;
	status_timer = timerBegin(0, 8000 - 1, true);
	timerAttachInterrupt(status_timer, &onStatusTimer, true);
	timerAlarmWrite(status_timer, STATUS_TIMER_COUNTER - 1, true);

#if ONLY_WIFI == 0
	// This timer starts after we send a data to BLE and wait for response
	// If no response that check BLE connection and reconnect
	// constexpr uint8_t BLE_TIMEOUT_TIMER_SEC = 30;
	constexpr uint8_t BLE_TIMEOUT_TIMER_SEC = 180;
	constexpr uint32_t BLE_TIMEOUT_TIMER_COUNTER = BLE_TIMEOUT_TIMER_SEC * 10000;
	BLE_timeout_timer = timerBegin(1, 8000 - 1, true);
	timerAttachInterrupt(BLE_timeout_timer, &onBLEtimeout, true);
	timerAlarmWrite(BLE_timeout_timer, BLE_TIMEOUT_TIMER_COUNTER - 1, true);
	timerAlarmEnable(BLE_timeout_timer);
	ble = std::make_unique<BLE>();
#endif

	constexpr uint8_t NTP_TIMER_SEC = 10;
	constexpr uint32_t NTP_TIMER_COUNTER = NTP_TIMER_SEC * 10000;
	ntp_timer = timerBegin(2, 8000 - 1, true);
	timerAttachInterrupt(ntp_timer, &onNtpTimer, true);
	timerAlarmWrite(ntp_timer, NTP_TIMER_COUNTER, true);
	timerAlarmEnable(ntp_timer);

	pinMode(RelayController::COMPRESSOR_RELAY, INPUT);
	pinMode(LED_BUILTIN, OUTPUT);

	// Calendar calendar;

	network.connect_to_wifi();
}

void loop()
{
	// network.ntp_client.update();

// 	if (network.do_wifi_connect)
// 		network.connect_to_wifi();

// 	if (is_status_tim)
// 		status_tim_function();

	if (is_ntp_tim)
		ntp_tim_function();

// #if ONLY_WIFI == 0
// 	if (is_ble_timeout)
// 		ble_timeout();

// 	check_BLE_port();
// #endif
// 	check_COM_port();
}

void compare_hum()
{
	if (is_relay_controlled_by_user_through_COM == false) {
		if (actual_sensor_params.hum < user_defined_sensor_params.hum_min)
		{
			is_compressor_start = true;
		}
		
		if (actual_sensor_params.hum > user_defined_sensor_params.hum_max)
		{
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
		Serial.println("INFO: A relay is on");
		RelayController::on(RelayController::COMPRESSOR_RELAY);
	}
	else
	{
		Serial.println("INFO: A relay is off");
		RelayController::off(RelayController::COMPRESSOR_RELAY);
	}
}

/// @brief Change the SSID and password of the network we are connecting to
/// @param message The handler arguments - expecting "SSID password"
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

	network.change_wifi_cfg(args[1], args[2]);
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

#if ONLY_WIFI == 0
/**
 * This function takes words from the COM port and send them to BLE
 */
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

void ble_data_handler(const std::string& message) {
	timerRestart(BLE_timeout_timer);

	// Read syms from COM port until \n
	uint8_t ble_message[15];

	Serial2.readBytes(ble_message, 10);

	std::string new_ble_message((char*)ble_message);

	Serial.println("DEBUG: In BLE data handler function");

	/** TODO: Delete me */
	Serial.println((char*)ble_message);
	
	// Extract temp value
	auto start_temp_pos = new_ble_message.find('t');
	auto end_temp_pos = new_ble_message.find('h');
	auto end_hum_pos = new_ble_message.find('b');
	auto end_bat_pos = new_ble_message.find('\n');

	if (start_temp_pos == std::string::npos)  {
		Serial.println("ERROR: Couldn't find the start position of the temperature value in BLE message!");
		network.POST_log("ERROR", "Couldn't find the start position of the temperature value in BLE message from sensor!");
		return;
	}

	if (end_temp_pos == std::string::npos) {
		Serial.println("ERROR: Couldn't find the end position of the temperature value in BLE message!");
		network.POST_log("ERROR", "Couldn't find the end position of the temperature value in BLE message from sensor!");
		return;
	}

	String str_parsed_temp = new_ble_message.substr(start_temp_pos + 1, end_temp_pos - start_temp_pos - 1).c_str();

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

	if (start_hum_pos == std::string::npos)  {
		Serial.println("ERROR: Couldn't find the start position of the humidity value in BLE message!");
		network.POST_log("ERROR", "Couldn't find the start position of the humidity value in BLE message from sensor!");
		return;
	}

	String str_parsed_hum = new_ble_message.substr(start_hum_pos + 1, end_hum_pos - start_hum_pos - 1).c_str();

	if (str_parsed_hum.length() == 0) {
		Serial.println("ERROR: Hum string size is 0!");
		return;
	}

	Serial.print("DEBUG: Parsed string hum from original message: ");
	Serial.println(str_parsed_hum);

	int parsed_hum = str_parsed_hum.toInt();
	Serial.print("DEBUG: Parsed int hum value from string hum: ");
	Serial.println(parsed_hum);

	// Extract battery value
	auto start_bat_pos = end_hum_pos;

	if (start_bat_pos == std::string::npos) {
		Serial.println("ERROR: Couldn't find the start position of the battery charge value in BLE message!");
		network.POST_log("ERROR", "Couldn't find the start position of the battery charge value in BLE message from sensor!");
		return;
	}

	String str_parsed_bat = new_ble_message.substr(start_bat_pos + 1, end_bat_pos - start_bat_pos - 1).c_str();

	if (str_parsed_bat.length() == 0) {
		Serial.println("ERROR: Bat string size is 0!");
		return;
	}

	Serial.print("DEBUG: Parsed string bat from original message: ");
	Serial.println(str_parsed_bat);

	int parsed_bat = str_parsed_bat.toInt();
	Serial.print("DEBUG: Parsed int bat value from string bat: ");
	Serial.println(parsed_bat);

	actual_sensor_params.temp = parsed_temp;
	actual_sensor_params.hum = parsed_hum;
	actual_sensor_params.battery_charge = parsed_bat;

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
		network.POST_log("INFO", "The temperature and the humidity data has been sent");
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
		network.POST_log("ERROR", "The humidity sensor is not available!");
	}
}
#endif

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

	network.get_status(user_defined_sensor_params);

	// Check an atmosphere params and the user input and control the compressor relay
	compare_hum();

	is_status_tim = false;
}

void ntp_tim_function() {
	// Sync the time
	// network.ntp_client.update();
	Serial.printf("NTP server updated:\n"
					  "\tis update: %d\n"
					  "\tis received: %d\n"
					  "\tday: %d\n"
					  "\thour: %d\n"
					  "\tmin: %d\n"
					  "\tsec: %d\n"
					  "\tformatted time: %s\n\n",
					  network.ntp_client.update(),
					  network.ntp_client.isTimeSet(),
					  network.ntp_client.getDay(),
					  network.ntp_client.getHours(),
					  network.ntp_client.getMinutes(),
					  network.ntp_client.getSeconds(),
					  network.ntp_client.getFormattedTime());

	// Update schedule
	network.update_schedule(Calendar::list);

	Serial.println("Schedule updated - here's calendar list after updating:");
	for (auto unit : Calendar::list) {
		String day_list;
		for (auto day : unit.days)
			day_list += String(day) + ' ';

		Serial.printf("\tid: %d\n"
					  "\tstart time: %d:%d\n"
					  "\tstop time: %d:%d\n"
					  "\tdays: %s\n",
					  unit.id,
					  unit.start.hour, unit.start.min,
					  unit.stop.hour, unit.stop.min,
					  day_list.c_str());
	}


	Calendar::Time current_time { network.ntp_client.getHours(), network.ntp_client.getMinutes() };
	uint8_t current_day = network.ntp_client.getDay();

	bool is_time_in_schedule = false;
	for (auto unit : Calendar::list) {
		if (unit.consist(current_time) and unit.days[current_day]) {
			is_time_in_schedule = true;
		}
	}

	if (is_time_in_schedule)
		Serial.println("The current time is included in the schedule");
	else
		Serial.println("The current time is NOT included in the schedule");

	is_ntp_tim = false;
}

#if ONLY_WIFI == 0
void ble_timeout() {
	/** TODO: Нужно выключать реле наглухо до тех пор, пока не возобновится связь по BLE
	 * Например, можно выставлять флаг, который будет запрещать изменять состояние реле
	 * или всегда тянуть вниз
	*/
	// Disable compressor relay
	RelayController::off(RelayController::COMPRESSOR_RELAY);

	Serial.print("ERROR: There is no connection to the sensor");
	network.POST_log("ERROR", "There is no connection to the sensor");

	is_ble_timeout = false;

	// Check connection between host and BLE module
	Serial.println("INFO: Check connection between the host and the BLE module");
	network.POST_log("INFO", "Check connection between the host and the BLE module");
	ble->check_connection();

	Serial.println("INFO: Restart the BLE timeout timer");
	network.POST_log("INFO", "Restart the BLE timeout timer");
	timerRestart(BLE_timeout_timer);
}
#endif

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

#if ONLY_WIFI == 0
void check_BLE_port() {
	if (Serial2.available() >= 1) {
		// char buff[256];

		// uint16_t size = Serial2.available();
		// Serial2.read(buff, size);

		// buff[size] = '\0';

		/** TODO: Delete this debug print */
		// Serial.println("\nDEBUG: Data from BLE:");
		// Serial.println(buff);

		// parse_message(std::string(buff));


		/** TODO: How to skip OK+xxx messages from BLE? */

		/**
		 * Read chars one by one and parse a message only 
		 * if the current sym is '\n'
		*/

		char sym = Serial2.read();

		ble_input += sym;

		/**
		 * Если команда начинается с текущего сообщения, то проверяем совпадение
		 * размера
		 * Если совпадает и размер, то выполняем команду, если размер меньше,
		 * то продолжаем
		 * Если не было ни одного совпадения, то очищаем строку
		*/

		bool is_ble_command_overlap = false;

		for (const BLE_Command& command : ble_command_list) {
			if (command.name.startsWith(ble_input)) {
				is_ble_command_overlap = true;

				if (command.name == ble_input) {
					command.handler("");
					ble_input.clear();
				}
			}
		}

		if (is_ble_command_overlap == false) {
			ble_input.clear();
		}
	}
}
#endif