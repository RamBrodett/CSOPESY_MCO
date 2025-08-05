#include "CLIController.h"
#include "Kernel.h"
#include "CommandInputController.h"
#include "ScreenManager.h"
#include "Scheduler.h"
#include "Screen.h"
#include "Instruction.h" 
#include "MemoryManager.h"
#include <memory>
#include <iostream>
#include <vector> 
#include <thread>

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

	// Launch a separate thread to handle user commands.
	std::thread inputThread(&CommandInputController::startInputLoop, CommandInputController::getInstance());

	// Main simulation loop
	while (Kernel::getInstance()->getRunningStatus()) {
		auto scheduler = Scheduler::getInstance();
		if (scheduler && scheduler->getSchedulerRunning()) {
			scheduler->incrementCpuCycles();

			// Check for idle state
			if (scheduler->getProcessQueueSize() == 0) { // You will need to add getProcessQueueSize() to Scheduler
				scheduler->incrementIdleCpuTicks();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		else {
			// If the scheduler is not running, we can consider this idle time as well.
			if (scheduler) scheduler->incrementIdleCpuTicks();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

	cout << "Main loop exited. Waiting for input thread to join..." << endl;
	// Wait for the input handling thread to finish its execution.
	// This is critical to prevent the program from terminating prematurely.
	if (inputThread.joinable()) {
		inputThread.join();
	}

	cout << "Input thread joined. Shutting down scheduler..." << std::endl;
	auto scheduler = Scheduler::getInstance();
	if (scheduler) {
		scheduler->stop(); // This will join the scheduler's own threads.
	}

	cout << "Scheduler shut down. Cleaning up resources." << endl;

	CommandInputController::destroy();
	CLIController::destroy();
	ScreenManager::destroy();
	MemoryManager::destroy();
	Kernel::destroy();
	return 0;
}