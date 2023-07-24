#include <cctype>

#include "parser.h"

extern struct ringbuf_t* BT_rb;
extern struct ringbuf_t* serial_rb;

extern uint8_t hum_min;
extern uint8_t hum_max;

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(), 
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

Parser::Parser() {
    init_BT_parser();
    init_serial_parser();
}

Parser::~Parser() {}

void Parser::init_BT_parser() {
    BT_rb = ringbuf_new(MAX_SIZE);

	std::vector<Command> commands;
	commands.emplace_back("", "Skip an empty message", CommandRoles::SERVICE_COMMAND);
    commands.emplace_back("Humidity:", "", CommandRoles::SERVICE_COMMAND);
    // commands.emplace_back("Temperature:", "", CommandRoles::SERVICE_COMMAND);

	BT_cli.add_commands(commands);
}

void Parser::init_serial_parser() {
    serial_rb = ringbuf_new(MAX_SIZE);

	std::vector<Command> commands;
	commands.emplace_back("", "Skip an empty message", CommandRoles::SERVICE_COMMAND);
	commands.emplace_back("help", "Show command list and short info about each command", CommandRoles::USER_COMMAND);
    commands.emplace_back("hum", "Set the border values to the humidity. Ex: hum 20 50", CommandRoles::USER_COMMAND);

	serial_cli.add_commands(commands);
}

void Parser::parse_serial() {
	// Check queue for new syms
	if (ringbuf_is_empty(serial_rb))
		return;

	// Add the sym and try to process a command
	char tmp_sym;
	ringbuf_memcpy_from(&tmp_sym, serial_rb, 1);
	curr_serial_command = serial_cli.process(tmp_sym);

	// Command handler
	if (curr_serial_command == "BUSY") return;
	else if (curr_serial_command == "NOT_A_COMMAND") 				unknown_command_handler();
	else if (curr_serial_command == "MAX_SIZE_REACHED") 			max_size_reached_handler();
	else if (curr_serial_command == "")								{}
	else if (curr_serial_command == "help") 						help_handler();
	else if (curr_serial_command == "hum") 						    hum_handler();
	else {
		Serial.print("ERROR: Undefined behavior in command parser module!\r\n"
					 "Check that all the commands have been described!\r\n");
	}

	serial_cli.args.clear();
	curr_serial_command = "BUSY";
}

void Parser::parse_BT(uint8_t& hum_value) {
	// Check queue for new syms
	if (ringbuf_is_empty(BT_rb))
		return;

	// Add the sym and try to process a command
	char tmp_sym;
	ringbuf_memcpy_from(&tmp_sym, BT_rb, 1);
	curr_BT_command = BT_cli.process(tmp_sym);

	// Command handler
	if (curr_BT_command == "BUSY") return;
    else if (curr_BT_command == "NOT_A_COMMAND") {}
    else if (curr_BT_command == "MAX_SIZE_REACHED") 	max_size_reached_handler();
    else if (curr_BT_command == "Humidity:")            raw_hum_data_handler(hum_value);
	else {
		Serial.print("ERROR: Undefined behavior in command parser module!\r\n"
					 "Check that all the commands have been described!\r\n");
	}

	BT_cli.args.clear();
	curr_BT_command = "BUSY";
}


void Parser::unknown_command_handler() {
    Serial.print("Unknown command!\n");
}

void Parser::max_size_reached_handler() {
	Serial.print("Max size reached!\n");
}

void Parser::help_handler() {
	bool is_show_all_commands = false;

	if (serial_cli.args[0] == "all")
		is_show_all_commands = true;

	std::string tmp_str;
	std::vector<Command> tmp = serial_cli.get_commands();
	tmp_str += "Commands:\n";
	for (auto command : tmp) {
		if (command.role == CommandRoles::USER_COMMAND or is_show_all_commands) {
			// Add command name
			tmp_str += "\t" + command.name;

			// Align commands description
			if (command.name.size() < 4) {
				tmp_str += "\t\t\t";
			}
			else if (command.name.size() >= 4 and command.name.size() < 8) {
				tmp_str += "\t\t";
			}
			else if (command.name.size() >= 8) {
				tmp_str += "\t";
			}

			// Add command description
			tmp_str += command.description + '\n';
		}
	}
	Serial.print((tmp_str + '\n').c_str());
}

void Parser::hum_handler() {
    if (serial_cli.args.size() != 2) {
        Serial.println("ERROR: Invalid number of arguments!");
    }

    if (!is_number(serial_cli.args[0])) {
        Serial.println("ERROR: The first value is not a number!");
    }

    if (!is_number(serial_cli.args[1])) {
        Serial.println("ERROR: The second value is not a number!");
    }

    hum_min = std::stoi(serial_cli.args[0]);
    hum_max = std::stoi(serial_cli.args[1]);

    Serial.printf("The humidity border values is %d and %d\n", hum_min, hum_max);
}

void Parser::raw_hum_data_handler(uint8_t& hum_value) {
    hum_value = std::stoi(BT_cli.args[0].substr(0, 2));
}

void Parser::raw_temp_data_handler() {

}