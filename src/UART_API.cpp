#include <UART_API.h>

// Constructor of the class
// @argument	size - max length of a command
UART_API::UART_API(const uint16_t size = 64) : max_size(size) {
	// Make a command list
	// Reserve first and second positions to BUSY and NOT_A_COMMAND states
	command_list.emplace_back("BUSY", "", CommandRoles::SERVICE_COMMAND);
	command_list.emplace_back("NOT_A_COMMAND", "", CommandRoles::SERVICE_COMMAND);
	command_list.emplace_back("MAX_SIZE_REACHED", "", CommandRoles::SERVICE_COMMAND);
//	command_list.insert({"BUSY", [](){}});
//	command_list.insert({"NOT_A_COMMAND", [](){}});
}

// Constructor of the class
// @argument 	commands - list of the commands to compare with
// @argument	size - max length of a command
UART_API::UART_API(std::vector<Command> &commands, const uint16_t size = 64) :
	max_size(size)
{
	// Make a command list
	// Reserve first and second positions to BUSY and NOT_A_COMMAND states
	command_list.emplace_back("BUSY", "", CommandRoles::SERVICE_COMMAND);
	command_list.emplace_back("NOT_A_COMMAND", "", CommandRoles::SERVICE_COMMAND);
	command_list.emplace_back("MAX_SIZE_REACHED", "", CommandRoles::SERVICE_COMMAND);

	// Add user's commands to the list
	if(commands.size() > 0)
		command_list.insert(command_list.end(), commands.begin(), commands.end());
}

UART_API::~UART_API() {
}

// Add a symbol to the buffer and analyse the string for end and max length
// @sym 	- sym that taken from UART
// @retval	- command
std::string UART_API::process(char sym) {
	// When we find a terminate symbol that stop the receiving and start the processing of a command
	if (sym == '\n') {
		return parse_command();
	}

	// Return NOT A COMMAND when current size bigger that max size
	if (command.size() > max_size) {
		command.clear();
		return "MAX_SIZE_REACHED";
	}

	// Usually \r meets with \n  in \r\n combination
	// Check \r before \n and skip it
	if (sym != '\r')
		command.push_back(sym); // Add sym to buffer

	// Return a busy state cause we're reading now
	return "BUSY";
}

// Get the list with commands
const std::vector<Command> UART_API::get_commands() {
	return std::vector<Command>(command_list.begin() + service_command_number, command_list.end());
}

// Add commands to the command list
void UART_API::add_commands(std::vector<Command>& commands) {
	for (auto user_command : commands) {
		// Check for duplications
		bool isExist = false;

		for (auto existing_command : command_list) {
			if (user_command.name == existing_command.name) {
				isExist = true;
				break;
			}
		}

		if (!isExist)
			command_list.push_back(user_command);
	}
}

// Tries to recognise a command in the buffer
// @retval	- command
std::string UART_API::parse_command() {
	// Clear arguments buffer
	args.clear();

	// Skip all spaces at the beginning of the message
	uint message_beginning = 0;
	while (command[message_beginning] == ' ')
		++message_beginning;


	// Parse string to the first space to split off a command and keys
	// (Most of commands looks like "command key1 key2 ..."
	uint command_position = message_beginning;
	while (command_position < command.size()) {
		if (command[command_position] == ' ')
			break;
		++command_position;
	}
	std::string str;
	str.append(command, message_beginning, (command_position - message_beginning));
	++command_position;

	// Save the arguments of the command
	std::string tmp;
	for (uint i = command_position; i < command.size(); ++i) {
		if (command[i] == ' ') {
			args.push_back(tmp);
			tmp.clear();
		}
		else
			tmp += command[i];
	}
	args.push_back(tmp);
	tmp.clear();

	// Try to find the command in the command list
	// Skip a few commands at the beginning because they are reserved to a service purposes
	for (uint i = service_command_number; i < command_list.size(); ++i) {
		if (str == command_list[i].name) {
			str.clear();
			command.clear();
			return command_list[i].name;
		}
	}
	str.clear();
	command.clear();
	return "NOT_A_COMMAND";
}
