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

CLIController* CLIController::instance = nullptr;

CLIController::CLIController() {};

const string CLIController::COLOR_GREEN = "\033[38;2;180;180;180m";
const string CLIController::COLOR_RED = "\033[38;2;240;128;128m";
const string CLIController::COLOR_BLUE = "\033[38;5;37m";
const string CLIController::COLOR_RESET = "\033[0m";

void CLIController::initialize() {
	if (!instance) instance = new CLIController();
}

CLIController* CLIController::getInstance() {
	return instance;
}

string CLIController::getTimestamp() const {  
    // Lock the mutex to ensure exclusive access to localtime  
    std::lock_guard<std::mutex> lock(timestampMutex);  

    time_t now = time(0);  
    tm localtm;  
    localtime_s(&localtm, &now); 

    stringstream ss;  
    ss << put_time(&localtm, "%m/%d/%Y, %I:%M:%S %p");  
    return ss.str();  
}

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

void CLIController::drawScreen(const Screen& screen) const {
	cout << COLOR_BLUE << "=== Process Screen: " << screen.getName() << " ===" << COLOR_RESET << "\n";
	cout << "Process name     : " << screen.getName() << "\n";
	cout << "Instruction      : " << screen.getProgramCounter() << " / " << screen.getTotalInstructions() << "\n";
	cout << "Created at       : " << screen.getTimestamp() << "\n";
	cout << COLOR_GREEN << "\n(Type 'exit' to return to main menu)\n" << COLOR_RESET;

}

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

void CLIController::destroy() {
	delete instance;
	instance = nullptr;
}
