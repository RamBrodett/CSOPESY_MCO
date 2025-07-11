#include "CommandInputController.h"
#include "ScreenManager.h"
#include "Kernel.h"
#include "CLIController.h"
#include "Scheduler.h"
#include "Instruction.h"
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
                thread([]() {
                    Scheduler::getInstance()->start();
                }).detach();
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
        } else if (command.rfind("screen", 0) == 0) {
            stringstream ss(command);
            string token, subcommand, screenName;
            ss >> token;      // Consume "screen"
            ss >> subcommand; // Get subcommand

            if (subcommand == "-ls") {
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
                ss >> screenName;
                if (screenName.empty()) {
                    cout << "Usage: screen -s <name>\n";
                }
                else if (ScreenManager::getInstance()->hasScreen(screenName)) {
                    cout << "Screen '" << screenName << "' already exists.\n";
                }
                else {
                    // Use the new method to get randomized instructions
                    auto newInstructions = Scheduler::getInstance()->generateInstructionsForProcess(screenName);
                    auto newScreen = make_shared<Screen>(screenName, newInstructions, CLIController::getInstance()->getTimestamp());
                    ScreenManager::getInstance()->registerScreen(screenName, newScreen);
                    // Add the new screen to the scheduler's queue
                    Scheduler::getInstance()->addProcessToQueue(newScreen);
                    ScreenManager::getInstance()->switchScreen(screenName);
                    CLIController::getInstance()->clearScreen();
                }
            }
            else if (subcommand == "-r") {
                ss >> screenName;
                if (screenName.empty()) {
                    cout << "Usage: screen -r <name>\n";
                }
                else if (!ScreenManager::getInstance()->hasScreen(screenName)) {
                    cout << "No such screen named '" << screenName << "'.\n";
                }
                else {
					auto screen = ScreenManager::getInstance()->getScreen(screenName);
					if (screen->isFinished()) {
                        //check process if finished
						cout << "Screen '" << screenName << "' has already finished execution.\n";
					}
					else {
                        //change screen if not finished
                        ScreenManager::getInstance()->switchScreen(screenName);
                        CLIController::getInstance()->clearScreen();
					}

                }
            }
            else {
                cout << "Use proper screen commands.'screen -ls', 'screen -s <name>', or 'screen -r <name>'." << endl;
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