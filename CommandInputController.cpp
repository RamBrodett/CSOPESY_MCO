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

    // After every command, check for and print new output from the current process screen
    if (currentScreen && currentScreen->getName() != "main") {
        auto messages = currentScreen->flushOutputBuffer();
        if (!messages.empty()) {
            for (const auto& msg : messages) {
                cout << CLIController::COLOR_BLUE << "[" << currentScreen->getName() << " output] " << msg << CLIController::COLOR_RESET << endl;
            }
        }
    }

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
                cout << "Initialized successfully.\n";
            }
        }
        else if (command == "exit") {
            cout << "Exiting program...\n";
            //get scheduler instance
            auto scheduler = Scheduler::getInstance();
            //check if scheduler is nullpointer
            if (scheduler && scheduler->getSchedulerRunning()) {
                scheduler->stop();
                if (Kernel::getInstance()->getSchedulerThread().joinable()) {
                    Kernel::getInstance()->getSchedulerThread().join();
                }
            }
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

                // MODIFIED: Sort by creation timestamp for FCFS ordering
                sort(runningProcesses.begin(), runningProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });
                sort(finishedProcesses.begin(), finishedProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });


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
                    vector<Instruction> defaultInstructions = {
                        {InstructionType::PRINT, {}, "Hello from new screen '" + screenName + "'!"},
                        {InstructionType::SLEEP, {{false, "", 100}}, ""}
                    };
                    auto newScreen = make_shared<Screen>(screenName, defaultInstructions, CLIController::getInstance()->getTimestamp());
                    ScreenManager::getInstance()->registerScreen(screenName, newScreen);
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
                    ScreenManager::getInstance()->switchScreen(screenName);
                    CLIController::getInstance()->clearScreen();
                }
            }
            else {
                cout << "Use proper screen commands.'screen -ls', 'screen -s <name>', or 'screen -r <name>'." << endl;
            }
        }
        else if (command == "scheduler-start") {
            if (Scheduler::getInstance()->getSchedulerRunning()) {
                cout << "Scheduler is already running.\n";
            }
            else {
                cout << "Starting scheduler and dummy process generator...\n";
                Kernel::getInstance()->getSchedulerThread() = thread([]() {
                    Scheduler::getInstance()->start();
                    });
            }
        }
        else if (command == "scheduler-stop") {
            if (!Scheduler::getInstance()->getSchedulerRunning() && !Kernel::getInstance()->getSchedulerThread().joinable()) {
                cout << "Scheduler is not running.\n";
                return;
            }
            cout << "Stopping scheduler...\n";
            Scheduler::getInstance()->stop();
            if (Kernel::getInstance()->getSchedulerThread().joinable()) {
                Kernel::getInstance()->getSchedulerThread().join();
            }
            cout << "Scheduler stopped successfully." << endl;
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

            // Sort by creation timestamp for FCFS ordering
            sort(runningProcesses.begin(), runningProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });
            sort(finishedProcesses.begin(), finishedProcesses.end(), [](const auto& a, const auto& b) { return a->getTimestamp() < b->getTimestamp(); });

            ofstream logFile("csopesy-log.txt"); // Overwrite mode
            if (!logFile) {
                cout << "Failed to open report file.\n";
                return;
            }

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
    }
    else { // We are inside a specific process screen
        if (command == "exit") {
            ScreenManager::getInstance()->switchScreen("main");
            CLIController::getInstance()->clearScreen();
        }
        else if (command == "process-smi") {
            ScreenManager::getInstance()->displayProcessSMI();
        }
        else {
            cout << "Unknown command '" << command << "'. Type 'exit' to return to the main console.\n";
        }
    } 

}