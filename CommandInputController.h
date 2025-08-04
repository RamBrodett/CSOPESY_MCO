#pragma once
#include "CLIController.h"
#include <string>


class CommandInputController {
public:
	// --- Singleton Access ---
	static void initialize();
	static CommandInputController* getInstance();
	static void destroy();

	// --- Input Loop ---
	void startInputLoop();

private:
	CommandInputController();
	void handleInputEntry();
	static CommandInputController* instance;
	void commandHandler(std::string command);
};