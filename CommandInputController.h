#pragma once

#include "CLIController.h"
#include <string>


class CommandInputController {
public:
	CommandInputController();
	void handleInputEntry();

	static void initialize();
	static CommandInputController* getInstance();
	static void destroy();
	void startInputLoop();

private:
	static CommandInputController* instance;
	void commandHandler(std::string command);
};