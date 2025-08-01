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

// --- NEW: Helper to parse instruction strings ---
Instruction parseInstruction(const string& line) {
    stringstream ss(line);
    string command;
    ss >> command;

    if (command == "DECLARE") {
        string varName;
        uint16_t value;
        ss >> varName >> value;
        return { InstructionType::DECLARE, {{true, varName, 0}, {false, "", value}}, "" };
    }
    if (command == "ADD") {
        string dest, src1, src2;
        ss >> dest >> src1 >> src2;
        return { InstructionType::ADD, {{true, dest, 0}, {true, src1, 0}, {true, src2, 0}}, "" };
    }
    if (command == "WRITE") {
        string addrStr;
        string valStr;
        ss >> addrStr >> valStr;
        int address = stoi(addrStr, nullptr, 16);
        // Check if valStr is a variable or literal
        if (isalpha(valStr[0])) {
            return { InstructionType::WRITE, {{false, "", (uint16_t)address}, {true, valStr, 0}}, "" };
        }
        else {
            return { InstructionType::WRITE, {{false, "", (uint16_t)address}, {false, "", (uint16_t)stoi(valStr)}}, "" };
        }
    }
    if (command == "READ") {
        string varName, addrStr;
        ss >> varName >> addrStr;
        int address = stoi(addrStr, nullptr, 16);
        return { InstructionType::READ, {{true, varName, 0}, {false, "", (uint16_t)address}}, "" };
    }
    if (command == "PRINT") {
        string content;
        getline(ss, content);
        // Basic parsing for print "message" + var
        content = content.substr(content.find_first_not_of(" \t")); // trim leading space
        string message = "";
        string varName = "";
        size_t plusPos = content.find('+');
        if (plusPos != string::npos) {
            message = content.substr(1, plusPos - 3); // "Result: "
            varName = content.substr(plusPos + 2); // varC
            return { InstructionType::PRINT, {{true, varName, 0}}, message + "%" + varName + "%" };
        }
        else {
            message = content.substr(1, content.size() - 2); // "Hello"
            return { InstructionType::PRINT, {}, message };
        }
    }
    return { InstructionType::PRINT, {}, "Invalid Instruction" }; // Default for errors
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
        stringstream ss(command);
        string keyword;
        ss >> keyword;

        // --- ADDED: Moved universal commands outside the 'initialized' check ---
        if (keyword == "exit") {
            cout << "Exiting program...\n";
            Kernel::getInstance()->setRunningStatus(false);
            return;
        }
        else if (keyword == "clear") {
            CLIController::getInstance()->clearScreen();
            return;
        }
        else if (keyword == "help") {
            // --- MODIFIED: Updated help text for new commands ---
            cout << "Available commands:\n";
            cout << "  initialize                         : Reads config.txt and prepares the OS.\n";
            cout << "  screen -s <name> <size>            : Create process with random instructions.\n";
            cout << "  screen -c <name> <size> \"<instr>\"  : Create process with custom instructions.\n";
            cout << "  screen -r <name>                   : View a running or finished process screen.\n";
            cout << "  screen -ls                         : List all processes and their status.\n";
            cout << "  scheduler-start                    : Start automatic batch process generation.\n";
            cout << "  scheduler-stop                     : Stop automatic batch process generation.\n";
            cout << "  process-smi                        : View memory and process summary.\n";
            cout << "  vmstat                             : View virtual memory statistics.\n";
            cout << "  clear                              : Clear the screen.\n";
            cout << "  exit                               : Exit the emulator.\n";
            return;
        }

        if (keyword == "initialize") {
            if (Kernel::getInstance()->isConfigInitialized()) {
                cout << "Already initialized.\n";
            }
            else {
                Scheduler::initialize();
                Kernel::getInstance()->setConfigInitialized(true);
                Scheduler::getInstance()->start();
                cout << "Initialized successfully.\n";
            }
            return; // --- ADDED: Return to prevent falling through
        }

        // --- All commands below this point require initialization ---
        if (!Kernel::getInstance()->isConfigInitialized()) {
            cout << "Initialize first" << endl;
            return;
        }

        if (keyword == "process-smi") {
            // --- MODIFIED: Implemented process-smi ---
            auto memManager = MemoryManager::getInstance();
            int totalFrames = memManager->getTotalFrameCount();
            int usedFrames = memManager->getUsedFrameCount();
            int pageSize = Scheduler::getInstance()->getPageSize();
            long long totalMem = (long long)totalFrames * pageSize;
            long long usedMem = (long long)usedFrames * pageSize;
            int memUtil = (totalMem > 0) ? (int)(100 * usedMem / totalMem) : 0;
            int cpuUtil = (Scheduler::getInstance()->getAvailableCores() > 0) ? (Scheduler::getInstance()->getUsedCores() * 100 / Scheduler::getInstance()->getAvailableCores()) : 0;

            cout << "| PROCESS-SMI V01.00 Driver Version: 01.00 |" << endl;
            cout << "CPU-Util: " << cpuUtil << "%" << endl;
            cout << "Memory Usage: " << usedMem << "KB / " << totalMem << "KB" << endl;
            cout << "Memory Util: " << memUtil << "%" << endl;
            cout << "================================================" << endl;
            cout << "Running processes and memory usage:" << endl;

            auto allScreens = ScreenManager::getInstance()->getAllScreens();
            for (const auto& pair : allScreens) {
                if (pair.first != "main" && !pair.second->isFinished()) {
                    cout << "  " << left << setw(15) << pair.first << right << setw(8) << pair.second->getVirtualMemorySize() << "KB" << endl;
                }
            }
        }
        else if (keyword == "vmstat") {
            // --- MODIFIED: Implemented vmstat ---
            auto memManager = MemoryManager::getInstance();
            auto scheduler = Scheduler::getInstance();
            int totalFrames = memManager->getTotalFrameCount();
            int usedFrames = memManager->getUsedFrameCount();
            int pageSize = scheduler->getPageSize();

            cout << setw(12) << (long long)totalFrames * pageSize << " K  total memory" << endl;
            cout << setw(12) << (long long)usedFrames * pageSize << " K  used memory" << endl;
            cout << setw(12) << (long long)(totalFrames - usedFrames) * pageSize << " K  free memory" << endl;
            cout << "------------------------------------" << endl;
            cout << setw(12) << scheduler->getIdleCpuTicks() << "   idle cpu ticks" << endl;
            cout << setw(12) << scheduler->getActiveCpuTicks() << "   active cpu ticks" << endl;
            cout << setw(12) << scheduler->getCpuCycles() << "   total cpu ticks" << endl;
            cout << "------------------------------------" << endl;
            cout << setw(12) << memManager->getPagedInCount() << "   pages paged in" << endl;
            cout << setw(12) << memManager->getPagedOutCount() << "   pages paged out" << endl;
        }
        else if (command.rfind("screen", 0) == 0) {
            stringstream ss_screen(command); // Use a different stringstream name to avoid conflicts
            string token, subcommand, screenName;
            ss_screen >> token;      // Consume "screen"
            ss_screen >> subcommand; // Get subcommand

            if (subcommand == "-ls") {
                // --- MODIFIED: Added memory fault check to your existing -ls logic ---
                auto allScreens = ScreenManager::getInstance()->getAllScreens();
                cout << "--------------------------------------------------------------------------------\n";
                cout << "Processes:\n";
                for (const auto& pair : allScreens) {
                    if (pair.first == "main") continue;
                    auto screen = pair.second;
                    cout << left << setw(15) << screen->getName();
                    if (screen->hasMemoryViolation()) { // Check for memory fault first
                        cout << "MEM_FAULT at " << screen->getViolationTimestamp()
                            << " (0x" << hex << screen->getViolationAddress() << dec << " invalid)";
                    }
                    else if (screen->isFinished()) {
                        cout << "Finished at " << screen->getTimestampFinished();
                    }
                    else if (screen->getIsRunning()) {
                        cout << "Running on Core " << screen->getCoreID();
                    }
                    else {
                        cout << "Waiting in queue";
                    }
                    cout << "\t(" << screen->getProgramCounter() << "/" << screen->getTotalInstructions() << " instructions)" << endl;
                }
                cout << "--------------------------------------------------------------------------------\n";
            }
            // --- ADDED: Logic for screen -c ---
            else if (subcommand == "-c") {
                string memSizeStr, instructionStr;
                ss_screen >> screenName >> memSizeStr;
                getline(ss_screen, instructionStr); // Get the rest of the line

                if (screenName.empty() || memSizeStr.empty() || instructionStr.empty()) {
                    cout << "Usage: screen -c <name> <size> \"<instructions>\"" << endl;
                    return;
                }
                instructionStr = instructionStr.substr(instructionStr.find_first_not_of(" \t"));
                if (instructionStr.front() == '"' && instructionStr.back() == '"') {
                    instructionStr = instructionStr.substr(1, instructionStr.size() - 2);
                }

                int memSize = stoi(memSizeStr);
                vector<Instruction> instructions;
                stringstream instr_ss(instructionStr);
                string single_instr;
                while (getline(instr_ss, single_instr, ';')) {
                    // --- THIS IS THE LINE TO CHANGE ---
                    if (!single_instr.empty() && !all_of(single_instr.begin(), single_instr.end(), [](unsigned char c) { return std::isspace(c); })) {
                        instructions.push_back(parseInstruction(single_instr));
                    }
                }

                MemoryManager::getInstance()->createProcessPageTable(screenName, memSize);
                auto newScreen = make_shared<Screen>(screenName, memSize, instructions, CLIController::getInstance()->getTimestamp());
                ScreenManager::getInstance()->registerScreen(screenName, newScreen);
                Scheduler::getInstance()->addProcessToQueue(newScreen);
                cout << "Process " << screenName << " created with " << memSize << "KB virtual memory and custom instructions." << endl;
            }
            else if (subcommand == "-s") {
                // --- MODIFIED: Added memory size parsing to your existing -s logic ---
                string memSizeStr;
                ss_screen >> screenName >> memSizeStr;
                if (screenName.empty() || memSizeStr.empty()) {
                    cout << "Usage: screen -s <name> <size>\n";
                }
                else if (ScreenManager::getInstance()->hasScreen(screenName)) {
                    cout << "Screen '" << screenName << "' already exists.\n";
                }
                else {
                    int memSize = stoi(memSizeStr);
                    auto newInstructions = Scheduler::getInstance()->generateInstructionsForProcess(screenName);

                    // --- CHANGED: These two lines are the key change ---
                    MemoryManager::getInstance()->createProcessPageTable(screenName, memSize);
                    auto newScreen = make_shared<Screen>(screenName, memSize, newInstructions, CLIController::getInstance()->getTimestamp());

                    ScreenManager::getInstance()->registerScreen(screenName, newScreen);
                    Scheduler::getInstance()->addProcessToQueue(newScreen);
                    cout << "Process " << screenName << " created with " << memSize << "KB virtual memory." << endl;
                }
            }
            else if (subcommand == "-r") {
                // --- MODIFIED: Added memory fault check to your existing -r logic ---
                ss_screen >> screenName;
                if (screenName.empty()) {
                    cout << "Usage: screen -r <name>\n";
                }
                else {
                    auto screen = ScreenManager::getInstance()->getScreen(screenName);
                    if (!screen) {
                        cout << "No such screen named '" << screenName << "'.\n";
                    }
                    else if (screen->hasMemoryViolation()) {
                        cout << "Process <" << screenName << "> shut down due to memory access violation error that occurred at "
                            << screen->getViolationTimestamp() << ". <0x" << hex << screen->getViolationAddress() << dec << "> invalid.\n";
                    }
                    else if (screen->isFinished()) {
                        cout << "Screen '" << screenName << "' has already finished execution.\n";
                    }
                    else {
                        ScreenManager::getInstance()->switchScreen(screenName);
                        CLIController::getInstance()->clearScreen();
                    }
                }
            }
            else {
                cout << "Use proper screen commands.'screen -ls', 'screen -s <name> <size>', or 'screen -r <name>'." << endl;
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
