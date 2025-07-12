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


	// --- LAUNCH INPUT THREAD ---
	// Launch a separate thread to handle user commands.
	std::thread inputThread(&CommandInputController::startInputLoop, CommandInputController::getInstance());

	// --- MAIN SIMULATION LOOP ---
	while (Kernel::getInstance()->getRunningStatus()) {
		auto scheduler = Scheduler::getInstance();
		if (scheduler && scheduler->getSchedulerRunning()) {
			scheduler->incrementCpuCycles();

			if (scheduler->getAlgorithm() == "rr") {
				int currentCycles = scheduler->getCpuCycles();
				int quantum = scheduler->getQuantumCycles(); 
				///cout << "DEBUG" << quantum << "+" << currentCycles << endl;
				if (quantum > 0 && currentCycles > 0 && currentCycles % quantum == 0) {
					//cout << "DEBUG: Main loop printing memory layout at cycle " << currentCycles << endl;
					MemoryManager::getInstance()->printMemoryLayout(currentCycles);
				}
			}
		}

		// The call to handleInputEntry() is now gone from this loop.

		// Add a small sleep to prevent the simulation from
		// consuming 100% of a CPU core. , 2 cpu cycles per second
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	//while (Kernel::getInstance()->getRunningStatus()) {
	//	auto scheduler = Scheduler::getInstance();
	//	if (scheduler && scheduler->getSchedulerRunning()) {
	//		// Increment the cycle on every loop
	//		scheduler->incrementCpuCycles();

	//		// Check for memory reporting on every cycle tick
	//		if (scheduler->getAlgorithm() == "rr") {

	//		}
	//	}


	//	CommandInputController::getInstance()->handleInputEntry();
	//	//Add a small sleep to prevent the loop from consuming 100% CPU
	//	//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//}

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