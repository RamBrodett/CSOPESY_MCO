#include "CLIController.h"
#include "Kernel.h"
#include "CommandInputController.h"
#include "ScreenManager.h"
#include "Scheduler.h"
#include "Screen.h"
#include "Instruction.h" 
#include <memory>
#include <iostream>
#include <vector> 

using namespace std;

int main() {
	Kernel::initialize();
	ScreenManager::initialize();
	CLIController::initialize();
	CommandInputController::initialize();

	auto mainScreen = make_shared<Screen>("main", vector<Instruction>{}, CLIController::getInstance()->getTimestamp());
	ScreenManager::getInstance()->registerScreen("main", mainScreen);
	ScreenManager::getInstance()->switchScreen("main");
	CLIController::getInstance()->clearScreen();

	while (Kernel::getInstance()->getRunningStatus()) {
		CommandInputController::getInstance()->handleInputEntry();
	}

	cout << "Main loop exited. Shutting down scheduler..." << std::endl;
	Scheduler::getInstance()->stop();
	if (Kernel::getInstance()->getSchedulerThread().joinable()) {
		Kernel::getInstance()->getSchedulerThread().join();
	}
	cout << "Scheduler shut down. Cleaning up resources." << endl;

	CommandInputController::destroy();
	CLIController::destroy();
	ScreenManager::destroy();
	Kernel::destroy();
	return 0;
}