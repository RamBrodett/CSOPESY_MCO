#include "CLIController.h"
#include "Screen.h"
#include "ScreenManager.h"
#include <ctime>
#include <sstream>
#include <string>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <mutex> 

using namespace std;

// --- Singleton ---
CLIController* CLIController::instance = nullptr;
// Constructor (private for singleton pattern).
CLIController::CLIController() {};

// Defines the ANSI color codes for console output.
const string CLIController::COLOR_GREEN = "\033[38;2;180;180;180m";
const string CLIController::COLOR_RED = "\033[38;2;240;128;128m";
const string CLIController::COLOR_BLUE = "\033[38;5;37m";
const string CLIController::COLOR_RESET = "\033[0m";

// Initializes the singleton instance of the CLIController.
void CLIController::initialize() {
	if (!instance) instance = new CLIController();
}

// Returns the singleton instance of the CLIController.
CLIController* CLIController::getInstance() {
	return instance;
}

// Returns the current system time as a formatted string, thread-safe.
string CLIController::getTimestamp() const {  
	// Lock mutex to ensure thread safety for time operations.
    std::lock_guard<std::mutex> lock(timestampMutex);  

    time_t now = time(0);  
    tm localtm;  
    localtime_s(&localtm, &now); 

    stringstream ss;  
    ss << put_time(&localtm, "%m/%d/%Y, %I:%M:%S %p");  
    return ss.str();  
}

// Prints the main ASCII art header for the console.
void CLIController::printHeader() const {
	cout << COLOR_BLUE;
	cout << R"(
     _____         _____         _____         _____       _____         _____   
    /:/  /        /:/ _/_       /::\  \       /::\  \     /:/ _/_       /:/ _/_         ___
   /:/  /        /:/ /\  \     /:/\:\  \     /:/\:\__\   /:/ /\__\     /:/ /\  \       /|  |  
  /:/  /  ___   /:/ /::\  \   /:/  \:\  \   /:/ /:/  /  /:/ /:/ _/_   /:/ /::\  \     |:|  |  
 /:/__/  /\__\ /:/_/:/\:\__\ /:/__/ \:\__\ /:/_/:/  /  /:/_/:/ /\__\ /:/_/:/\:\__\    |:|  |  
 \:\  \ /:/  / \:\/:/ /:/  / \:\  \ /:/  / \:\/:/  /   \:\/:/ /:/  / \:\/:/ /:/  /  __|:|__|  
  \:\  /:/  /   \::/ /:/  /   \:\  /:/  /   \::/__/     \::/_/:/  /   \::/ /:/  /  /::::\  \
   \:\/:/  /     \/_/:/  /     \:\/:/  /     \:\  \      \:\/:/  /     \/_/:/  /   ----\:\  \
    \::/  /        /:/  /       \::/  /       \:\__\      \::/  /        /:/  /         \:\__\
     \/__/         \/__/         \/__/         \/__/       \/__/         \/__/           \/__/
)";

	cout << COLOR_RESET;
	cout << COLOR_GREEN;
	cout << R"(
======================================= CONSOLE BY ============================================
=============================== DAVID | DONALD | LUKE | RICHMOND ==============================
===============================================================================================
)";
	cout << COLOR_RESET;
	cout << COLOR_RED;
	cout << "\nType 'exit' to exit, 'help' for help in commands, 'clear' to clear the screen.\n";
	cout << COLOR_RESET;
}

// Draws the user interface for a specific process screen, showing its status.
void CLIController::drawScreen(const Screen& screen) const {
	cout << COLOR_BLUE << "=== Process Screen: " << screen.getName() << " ===" << COLOR_RESET << "\n";
	cout << "Process name     : " << screen.getName() << "\n";
	cout << "Instruction      : " << screen.getProgramCounter() << " / " << screen.getTotalInstructions() << "\n";
	cout << "Created at       : " << screen.getTimestamp() << "\n";

	if (screen.isFinished() && !screen.hasMemoryViolation()) {
		cout << "Status           : Finished at " << screen.getTimestampFinished() << "\n";
	}
	else if (screen.getIsRunning()) {
		cout << "Status           : Running on Core " << screen.getCoreID() << "\n";
	}
	else {
		cout << "Status           : Ready in queue\n";
	}

	cout << COLOR_GREEN << "\n(Type 'exit' to return to main menu)\n" << COLOR_RESET;

}

// Clears the console screen and redraws the current view (main header or process screen).
void CLIController::clearScreen() const {
#if defined(_WIN32) || defined(_WIN64)
	system("cls");
#else
	system("clear");
#endif

	auto current = ScreenManager::getInstance()->getCurrentScreen();
	if (current->getName() == "main") {
		printHeader();
	}
	else {
		drawScreen(*current);
	}
}

// Destroys the singleton instance to free memory.
void CLIController::destroy() {
	delete instance;
	instance = nullptr;
}
