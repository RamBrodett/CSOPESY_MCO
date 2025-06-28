#pragma once

#include "Screen.h"
#include <string>
#include <mutex>

class CLIController {
public:
	CLIController();
	/// --- Screen Output ---
	std::string getTimestamp() const;
	void printHeader() const;
	void drawScreen(const Screen& screen) const;
	void clearScreen() const;
	/// --- Initialization ---
	static void initialize();
	static CLIController* getInstance();
	static void destroy();
	// --- Colors ---
	static const std::string COLOR_GREEN;
	static const std::string COLOR_RED;
	static const std::string COLOR_BLUE;
	static const std::string COLOR_RESET;

private:
	static CLIController* instance;
	mutable std::mutex timestampMutex;
};