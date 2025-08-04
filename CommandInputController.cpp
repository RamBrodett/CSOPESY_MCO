#include "CommandInputController.h"
#include "ScreenManager.h"
#include "Kernel.h"
#include "CLIController.h"
#include "Scheduler.h"
#include "Instruction.h"
#include "MemoryManager.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <vector>
#include <algorithm>
#include <limits>
#include <iomanip>
#include <chrono> 
#include <random>
#include <functional>

using namespace std;

CommandInputController* CommandInputController::instance = nullptr;

CommandInputController::CommandInputController() {}

void CommandInputController::initialize() {
    if (!instance) instance = new CommandInputController();
}

CommandInputController* CommandInputController::getInstance() {
    return instance;
}

void CommandInputController::handleInputEntry() {
    auto currentScreen = ScreenManager::getInstance()->getCurrentScreen();

    cout << CLIController::COLOR_GREEN << (currentScreen->getName() + " > ") << CLIController::COLOR_RESET;
    string command;
    if (!cin) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    getline(cin, command);
    commandHandler(command);
}

void CommandInputController::destroy() {
    delete instance;
    instance = nullptr;
}

void CommandInputController::commandHandler(string command) {
    if (ScreenManager::getInstance()->getCurrentScreen()->getName() == "main") {
        if (command == "initialize") {
            if (Kernel::getInstance()->isConfigInitialized()) {
                cout << "Already initialized.\n";
            }
            else {
                Scheduler::initialize();
                Kernel::getInstance()->setConfigInitialized(true);
                Scheduler::getInstance()->start();
                cout << "Initialized successfully.\n";
            }
        }
        else if (command == "exit") {
            cout << "Exiting program...\n";
            Kernel::getInstance()->setRunningStatus(false);
        }
        else if (command == "help") {
            cout << "Available commands:\n";
            cout << "screen -s <name>    : Start new screen session\n";
            cout << "screen -r <name>    : Resume existing screen\n";
            cout << "screen -ls          : List all available screens\n";
            cout << "scheduler-start     : Start the process scheduler\n";
            cout << "scheduler-stop      : Stop the process scheduler\n";
            cout << "report-util         : Save a report to 'csopesy-log.txt'\n";
            cout << "process-smi         : Display system and memory summary\n"; //not implemented
            cout << "vmstat              : Display virtual memory statistics\n"; //not implemented
            cout << "clear               : Clear the screen\n";
            cout << "exit                : Exit program\n";
        }
        //else if (command == "debug-cycles") {
        //    if (Scheduler::getInstance()) {
        //        cout << "Current CPU Cycles: " << Scheduler::getInstance()->getCpuCycles() << endl;
        //    }
        //    else {
        //        cout << "Scheduler not initialized." << endl;
        //    }
        //}
        else if (command == "clear") {
            CLIController::getInstance()->clearScreen();
        }
        else if (!Kernel::getInstance()->isConfigInitialized()) {
            cout << "Initialize first" << endl;
            return;
        }
        else if (command.rfind("screen", 0) == 0) {
            stringstream ss(command);
            string token, subcommand, screenName, memorySizeStr;
            ss >> token;      // Consume "screen"
            ss >> subcommand; // Get subcommand

            if (subcommand == "-ls") {
                auto allScreens = ScreenManager::getInstance()->getAllScreens();
                vector<shared_ptr<Screen>> runningProcesses;
                vector<shared_ptr<Screen>> finishedProcesses;

                for (const auto& pair : allScreens) {
                    if (pair.first == "main") continue;
                    auto screen = pair.second;

                    if (screen->hasMemoryViolation()) {
                        // Extract time from timestamp
                        string timeOnly = screen->getMemoryViolationTime();
                        size_t timeStart = timeOnly.find(", ") + 2;
                        size_t timeEnd = timeOnly.find(" ", timeStart);
                        if (timeEnd != string::npos) {
                            timeOnly = timeOnly.substr(timeStart, timeEnd - timeStart);
                        }

                        cout << "Process " << screen->getName()
                            << " shut down due to memory access violation error that occurred at "
                            << timeOnly << ". " << screen->getMemoryViolationAddress()
                            << " invalid." << endl;
                    }
                    else if (screen->isFinished()) {
                        finishedProcesses.push_back(screen);
                    }
                    else {
                        runningProcesses.push_back(screen);
                    }
                }

                //sort by creation timestamp
                sort(runningProcesses.begin(), runningProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });
                sort(finishedProcesses.begin(), finishedProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });

                cout << "CPU utilization: " << (Scheduler::getInstance()->getUsedCores() * 100 / Scheduler::getInstance()->getAvailableCores()) << "%" << endl;
                cout << "Cores used: " << Scheduler::getInstance()->getUsedCores() << endl;
                cout << "Cores available: " << Scheduler::getInstance()->getAvailableCores() << endl;

                cout << "--------------------------------------------------------------------------------\n";
                cout << "Running processes:\n";
                if (runningProcesses.empty()) {
                    cout << " (None)\n";
                }
                else {
                    for (const auto& screen : runningProcesses) {
                        cout << left << setw(10) << screen->getName()
                            << " (" << screen->getTimestamp() << ")";

                        if (screen->getCoreID() != -1) {
                            cout << "\tCore: " << screen->getCoreID();
                        }

                        cout << "\t" << screen->getProgramCounter() << " / " << screen->getTotalInstructions() << "\n";
                    }
                }

                cout << "\nFinished processes:\n";
                if (finishedProcesses.empty()) {
                    cout << " (None)\n";
                }
                else {
                    for (const auto& screen : finishedProcesses) {
                        cout << left << setw(10) << screen->getName()
                            << " (" << screen->getTimestampFinished() << ")"
                            << "\tFinished"
                            << "\t" << screen->getProgramCounter() << " / " << screen->getTotalInstructions() << "\n";
                    }
                }
                cout << "--------------------------------------------------------------------------------\n";

            }
            else if (subcommand == "-s") {
                ss >> screenName >> memorySizeStr;
                if (screenName.empty() || memorySizeStr.empty()) {
                    cout << "Usage: screen -s <name> <memory_size>\n";
                    return;
                }
                else {
                    try {
                        int memSize = stoi(memorySizeStr);
                        if (memSize < 64 || memSize > 65536) { // Memory validation
                            cout << "Invalid memory allocation. Size must be between 64 and 65536 bytes.\n";
                        }
                        else if (ScreenManager::getInstance()->hasScreen(screenName)) {
                            cout << "Screen '" << screenName << "' already exists.\n";
                        }
                        else {
                            auto newInstructions = Scheduler::getInstance()->generateInstructionsForProcess(screenName, memSize);
                            auto newScreen = make_shared<Screen>(screenName, newInstructions, CLIController::getInstance()->getTimestamp());

                            // Setup memory in the MemoryManager first
                            MemoryManager::getInstance()->setupProcessMemory(screenName, memSize);

                            ScreenManager::getInstance()->registerScreen(screenName, newScreen);
                            Scheduler::getInstance()->addProcessToQueue(newScreen);
                            ScreenManager::getInstance()->switchScreen(screenName);
                            CLIController::getInstance()->clearScreen();
                        }
                    }
                    catch (const std::invalid_argument&) {
                        cout << "Invalid memory size provided. Please enter a number.\n";
                    }
                }
            }
            else if (subcommand == "-r") {
                ss >> screenName;
                if (screenName.empty()) {
                    cout << "Usage: screen -r <name>\n";
                }
                // Step 1: Check if the screen name exists at all.
                else if (!ScreenManager::getInstance()->hasScreen(screenName)) {
                    cout << "Process '" << screenName << "' not found.\n";
                }
                else {
                    auto screen = ScreenManager::getInstance()->getScreen(screenName);

                    // Step 2: Check for a memory violation (MO2 Specification).
                    if (screen->hasMemoryViolation()) {
                        string timeOnly = screen->getMemoryViolationTime();
                        size_t timeStart = timeOnly.find(", ") + 2;

                        cout << "Process " << screen->getName()
                            << " shut down due to memory access violation error that occurred at "
                            << timeOnly.substr(timeStart) << ". " << screen->getMemoryViolationAddress()
                            << " invalid." << endl;
                    }
                    // Step 3: Check if the process is finished normally (MO1 Specification).
                    else if (screen->isFinished()) {
                        cout << "Process '" << screenName << "' not found.\n";
                    }
                    // Step 4: If the process exists and is not finished, resume it.
                    else {
                        ScreenManager::getInstance()->switchScreen(screenName);
                        CLIController::getInstance()->clearScreen();
                    }
                }
            }
        }
        else if (command == "scheduler-start") {
            auto scheduler = Scheduler::getInstance();
            if (scheduler && scheduler->getSchedulerRunning()) {
                if (scheduler->getGeneratingProcesses()) {
                    cout << "Scheduler is already active.\n";
                }
                else {
                    cout << "Starting scheduler...\n";
                    scheduler->startProcessGeneration();
                }
            }
            else {
                cout << "Scheduler is not running. Please 'initialize' the kernel first.\n";
            }
        }
        else if (command == "scheduler-stop") {
            auto scheduler = Scheduler::getInstance();
            if (scheduler && scheduler->getSchedulerRunning()) {
                if (!scheduler->getGeneratingProcesses()) {
                    cout << "Scheduler is already stopped.\n";
                }
                else {
                    cout << "Stopping scheduler...\n";
                    scheduler->setGeneratingProcesses(false);
                }
            }
            else {
                cout << "Scheduler is not running.\n";
            }
        }
        else if (command == "report-util") {
            auto allScreens = ScreenManager::getInstance()->getAllScreens();
            vector<shared_ptr<Screen>> runningProcesses;
            vector<shared_ptr<Screen>> finishedProcesses;

            for (const auto& pair : allScreens) {
                if (pair.first == "main") continue;
                auto screen = pair.second;
                if (screen->getProgramCounter() >= screen->getTotalInstructions() && screen->getTotalInstructions() > 0) {
                    finishedProcesses.push_back(screen);
                }
                else {
                    runningProcesses.push_back(screen);
                }
            }

            //sort by creation timestamp for FCFS ordering
            sort(runningProcesses.begin(), runningProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });
            sort(finishedProcesses.begin(), finishedProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });

            ofstream logFile("csopesy-log.txt"); //overwrite text
            if (!logFile) {
                cout << "Failed to open report file.\n";
                return;
            }
            logFile << "CPU utilization: " << (Scheduler::getInstance()->getUsedCores() * 100 / Scheduler::getInstance()->getAvailableCores()) << "%" << endl;
            logFile << "Cores used: " << Scheduler::getInstance()->getUsedCores() << endl;
            logFile << "Cores available: " << Scheduler::getInstance()->getAvailableCores() << endl;
            logFile << "--------------------------------------------------------------------------------\n";
            logFile << "Running processes:\n";
            if (runningProcesses.empty()) {
                logFile << " (None)\n";
            }
            else {
                for (const auto& screen : runningProcesses) {
                    logFile << left << setw(10) << screen->getName()
                        << " (" << screen->getTimestamp() << ")";
                    if (screen->getCoreID() != -1) {
                        logFile << "\tCore: " << screen->getCoreID();
                    }
                    logFile << "\t" << screen->getProgramCounter() << " / " << screen->getTotalInstructions() << "\n";
                }
            }

            logFile << "\nFinished processes:\n";
            if (finishedProcesses.empty()) {
                logFile << " (None)\n";
            }
            else {
                for (const auto& screen : finishedProcesses) {
                    logFile << left << setw(10) << screen->getName()
                        << " (" << screen->getTimestampFinished() << ")"
                        << "\tFinished"
                        << "\t" << screen->getProgramCounter() << " / " << screen->getTotalInstructions() << "\n";
                }
            }
            logFile << "--------------------------------------------------------------------------------\n";
            logFile.close();
            cout << "Screen list report saved to 'csopesy-log.txt'.\n";
		}
        else if (command == "process-smi") {
            ScreenManager::getInstance()->displaySystemSmiSummary(); //TODO
        }
        else if (command == "vmstat") {
            ScreenManager::getInstance()->displayVmStat(); //TODO
        }
        else {
            cout << "Unknown command '" << command << "'. Type 'help' for available commands.\n";
        }
    }
    else { //if inside a specific process screen
        if (command == "exit") {
            ScreenManager::getInstance()->switchScreen("main");
            CLIController::getInstance()->clearScreen();
		}
		else if (command == "clear") {
			CLIController::getInstance()->clearScreen();
		}
		else if (command == "help") {
			cout << "Available commands:\n";
			cout << "exit                : Return to main console\n";
			cout << "clear               : Clear the screen\n";
			cout << "process-smi         : Display process SMI (State, Memory, and I/O)\n";
		}
        else if (command == "process-smi") {
            ScreenManager::getInstance()->displayProcessSMI();
        }
        else {
            cout << "Unknown command '" << command << "'. Type 'exit' to return to the main console.\n";
        }
    } 

}

void CommandInputController::startInputLoop() {
    while (Kernel::getInstance()->getRunningStatus()) {
        handleInputEntry(); // Your existing function already has the logic
    }
}
