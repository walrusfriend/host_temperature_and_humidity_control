/*
 * Class parses UART channel to commands
 * It adds a char to buffer, analyse the buffer string and return the current command state
 * TODO Write a manual "how to put this module to the project"
*/

#ifndef INC_UART_API_H_
#define INC_UART_API_H_

#include <string>
#include <cstring>
#include <vector>
//#include <map>
//#include <functional>

enum CommandRoles {
	SERVICE_COMMAND,
	USER_COMMAND,
	DEBUG_COMMAND
};

class Command {
public:
	Command(std::string name, std::string description, CommandRoles role/*, std::function<void(T&)> func*/) :
		name(name), description(description), role(role)/*, handler(func)*/ {};

	std::string name;
	std::string description;
	CommandRoles role;
//	void(*handler)();
//	std::function<void(T&)> handler;
};

class UART_API {
public:
	UART_API(const uint16_t maxSize);
	UART_API(std::vector<Command> &commands, const uint16_t maxSize);
	virtual ~UART_API();

public:
	// Container for an arguments of a command
	std::vector<std::string> args;

public:
	// Add a symbol to the buffer and analyze the string for end and max length
	std::string process(char sym);

	const std::vector<Command> get_commands();
	void add_commands(std::vector<Command>& commands);

private:
	// Find string
	std::string parse_command();

private:
	static constexpr uint8_t service_command_number = 3;
	uint32_t max_size;
	std::string command = "";
	std::vector<Command> command_list;
//	std::map<std::string, void(*)()> command_list;
};

#endif /* INC_UART_API_H_ */
