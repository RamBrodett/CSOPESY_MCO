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
		auto scheduler = Scheduler::getInstance();
		if (scheduler && scheduler->getSchedulerRunning()) {
			// Increment the cycle on every loop
			scheduler->incrementCpuCycles();

			// Check for memory reporting on every cycle tick
			if (scheduler->getAlgorithm() == "rr") {
				int currentCycles = scheduler->getCpuCycles();
				int quantum = scheduler->getQuantumCycles(); // You'll need to add a getter for this
				if (quantum > 0 && currentCycles > 0 && currentCycles % quantum == 0) {
					//cout << "DEBUG: Main loop printing memory layout at cycle " << currentCycles << endl;
					MemoryManager::getInstance()->printMemoryLayout(currentCycles);
				}
			}
		}


		CommandInputController::getInstance()->handleInputEntry();
		//Add a small sleep to prevent the loop from consuming 100% CPU
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	cout << "Main loop exited. Shutting down scheduler..." << std::endl;
	auto scheduler = Scheduler::getInstance();
	if (scheduler) {
		scheduler->stop();
	}
	//if (Kernel::getInstance()->getSchedulerThread().joinable()) {
	//	Kernel::getInstance()->getSchedulerThread().join();
	//}
	cout << "Scheduler shut down. Cleaning up resources." << endl;

	CommandInputController::destroy();
	CLIController::destroy();
	ScreenManager::destroy();
	MemoryManager::destroy();
	Kernel::destroy();
	return 0;
}