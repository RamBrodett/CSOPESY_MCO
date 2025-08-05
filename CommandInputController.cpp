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

// --- Singleton ---
CommandInputController* CommandInputController::instance = nullptr;

// Constructor (private for singleton pattern).
CommandInputController::CommandInputController() {}

// Initializes the singleton instance of the CommandInputController.
void CommandInputController::initialize() {
    if (!instance) instance = new CommandInputController();
}

// Returns the singleton instance of the CommandInputController.
CommandInputController* CommandInputController::getInstance() {
    return instance;
}


vector<Instruction> parseInstructions(const string& input);

// Parses a single token into a variable or a literal value Operand.
Operand parseOperand(const string& token) {
    if (isalpha(token[0])) { // It's a variable
        return { true, token, 0 };
    }
    else { // It's a literal value
        return { false, "", static_cast<uint16_t>(stoul(token)) };
    }
}

// Parses a string of semicolon-separated user commands into a vector of Instruction structs.
//vector<Instruction> parseInstructions(const string& input) {
//    vector<Instruction> instructions;
//    stringstream ss(input);
//    string instructionSegment;
//
//    // Split the input string by semicolons to process each instruction.
//    while (getline(ss, instructionSegment, ';')) {
//        instructionSegment.erase(0, instructionSegment.find_first_not_of(" \t\n\r"));
//        if (instructionSegment.empty()) continue;
//
//        stringstream instrStream(instructionSegment);
//        string command;
//        instrStream >> command;
//
//        Instruction newInstruction;
//        string token;
//        vector<string> tokens;
//
//        // Parsing logic for DECLARE, ADD, WRITE, READ, PRINT etc.
//        while (instrStream >> token) {
//            tokens.push_back(token);
//        }
//
//        if (command == "DECLARE") {
//            if (tokens.size() != 2) throw runtime_error("DECLARE requires 2 arguments.");
//            newInstruction.type = InstructionType::DECLARE;
//            newInstruction.operands.push_back({ true, tokens[0], 0 }); // Variable name
//            newInstruction.operands.push_back(parseOperand(tokens[1])); // Value
//        }
//        else if (command == "ADD" || command == "SUBTRACT") {
//            if (tokens.size() != 3) throw runtime_error(command + " requires 3 arguments.");
//            newInstruction.type = (command == "ADD") ? InstructionType::ADD : InstructionType::SUBTRACT;
//            newInstruction.operands.push_back({ true, tokens[0], 0 }); // Destination variable
//            newInstruction.operands.push_back(parseOperand(tokens[1])); // Operand 1
//            newInstruction.operands.push_back(parseOperand(tokens[2])); // Operand 2
//        }
//        else if (command == "WRITE") {
//            if (tokens.size() != 2) throw runtime_error("WRITE requires 2 arguments.");
//            newInstruction.type = InstructionType::WRITE;
//            newInstruction.memoryAddress = static_cast<uint16_t>(stoul(tokens[0], nullptr, 0)); // Parses hex (0x) or decimal
//            newInstruction.operands.push_back(parseOperand(tokens[1])); // Value to write
//        }
//        else if (command == "READ") {
//            if (tokens.size() != 2) throw runtime_error("READ requires 2 arguments.");
//            newInstruction.type = InstructionType::READ;
//            newInstruction.operands.push_back({ true, tokens[0], 0 }); // Destination variable
//            newInstruction.memoryAddress = static_cast<uint16_t>(stoul(tokens[1], nullptr, 0)); // Parses hex (0x) or decimal
//        }
//        else if (command == "PRINT") {
//            if (tokens.empty()) throw runtime_error("PRINT requires a message.");
//            newInstruction.type = InstructionType::PRINT;
//
//            string message = tokens[0];
//            for (size_t i = 1; i < tokens.size(); ++i) message += " " + tokens[i];
//            newInstruction.printMessage = message;
//
//            // Check if there is a variable to substitute
//            size_t startPos = message.find('%');
//            if (startPos != string::npos) {
//                size_t endPos = message.find('%', startPos + 1);
//                if (endPos != string::npos) {
//                    string varName = message.substr(startPos + 1, endPos - startPos - 1);
//                    // Add the variable as an operand so the executor can find it
//                    newInstruction.operands.push_back({ true, varName, 0 });
//                }
//            }
//        }
//        else {
//            throw runtime_error("Unknown instruction: " + command);
//        }
//        instructions.push_back(newInstruction);
//    }
//    return instructions;
//}
//

// Helper function (can be placed in CommandInputController.cpp)
bool isPowerOfTwo(int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}



std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r";
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

vector<Instruction> parseInstructions(const string& input) {
    vector<Instruction> instructions;
    stringstream ss(input);
    string instructionSegment;

    // Split the input string by semicolons to process each instruction.
    while (getline(ss, instructionSegment, ';')) {
        string trimmedSegment = trim(instructionSegment);
        if (trimmedSegment.empty()) continue;

        Instruction newInstruction;

        // Manually check if the instruction is PRINT to avoid stringstream errors.
        if (trimmedSegment.rfind("PRINT", 0) == 0) {
            newInstruction.type = InstructionType::PRINT;

            // Extract the arguments part of the string
            string args = trimmedSegment.substr(string("PRINT").length());
            string trimmedArgs = trim(args);

            // Validate and remove the outer parentheses
            if (trimmedArgs.length() < 2 || trimmedArgs.front() != '(' || trimmedArgs.back() != ')') {
                throw runtime_error("PRINT arguments must be enclosed in parentheses, e.g., PRINT(\"message\" + var).");
            }
            string innerArgs = trimmedArgs.substr(1, trimmedArgs.size() - 2);

            // --- REVISED AND FINAL PARSING LOGIC ---
            // Find the string literal first, as it's a more reliable anchor.
            size_t firstQuote = innerArgs.find('"');
            size_t lastQuote = innerArgs.rfind('"');

            if (firstQuote == std::string::npos || lastQuote == firstQuote) {
                throw std::runtime_error("PRINT statement must contain a string literal enclosed in double quotes.");
            }

            string stringLiteral = innerArgs.substr(firstQuote + 1, lastQuote - firstQuote - 1);

            // Find the '+' operator to locate the variable.
            size_t plusPos = innerArgs.find('+');
            if (plusPos == std::string::npos) {
                throw std::runtime_error("PRINT statement requires a '+' to concatenate string and variable.");
            }

            // Determine if the variable is before or after the string literal based on the '+' position.
            string variableName;
            if (plusPos > lastQuote) { // Case: "message" + var
                variableName = trim(innerArgs.substr(plusPos + 1));
                newInstruction.printMessage = stringLiteral + "%" + variableName + "%";
            }
            else { // Case: var + "message"
                variableName = trim(innerArgs.substr(0, plusPos));
                newInstruction.printMessage = "%" + variableName + "%" + stringLiteral;
            }

            if (variableName.empty()) {
                throw runtime_error("Variable name in PRINT statement cannot be empty.");
            }

            // The operand is always the variable to be substituted.
            newInstruction.operands.push_back({ true, variableName, 0 });

        }
        else {
            // --- Fallback to the original logic for all other commands ---
            stringstream instrStream(trimmedSegment);
            string command;
            instrStream >> command;

            string token;
            vector<string> tokens;
            while (instrStream >> token) {
                tokens.push_back(token);
            }

            if (command == "DECLARE") {
                if (tokens.size() != 2) throw runtime_error("DECLARE requires 2 arguments.");
                newInstruction.type = InstructionType::DECLARE;
                newInstruction.operands.push_back({ true, tokens[0], 0 });
                newInstruction.operands.push_back(parseOperand(tokens[1]));
            }
            else if (command == "ADD" || command == "SUBTRACT") {
                if (tokens.size() != 3) throw runtime_error(command + " requires 3 arguments.");
                newInstruction.type = (command == "ADD") ? InstructionType::ADD : InstructionType::SUBTRACT;
                newInstruction.operands.push_back({ true, tokens[0], 0 });
                newInstruction.operands.push_back(parseOperand(tokens[1]));
                newInstruction.operands.push_back(parseOperand(tokens[2]));
            }
            else if (command == "WRITE") {
                if (tokens.size() != 2) throw runtime_error("WRITE requires 2 arguments.");
                newInstruction.type = InstructionType::WRITE;
                newInstruction.memoryAddress = static_cast<uint16_t>(stoul(tokens[0], nullptr, 0));
                newInstruction.operands.push_back(parseOperand(tokens[1]));
            }
            else if (command == "READ") {
                if (tokens.size() != 2) throw runtime_error("READ requires 2 arguments.");
                newInstruction.type = InstructionType::READ;
                newInstruction.operands.push_back({ true, tokens[0], 0 });
                newInstruction.memoryAddress = static_cast<uint16_t>(stoul(tokens[1], nullptr, 0));
            }
            else {
                throw runtime_error("Unknown instruction: " + command);
            }
        }

        instructions.push_back(newInstruction);
    }
    return instructions;
}

// Prompts the user, reads a line of input, and passes it to the command handler.
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

// Destroys the singleton instance to free memory.
void CommandInputController::destroy() {
    delete instance;
    instance = nullptr;
}

// The core command router; directs string commands to the appropriate functions.
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

                        if (!isPowerOfTwo(memSize)) {
                            cout << "Invalid memory allocation. Size must be a power of 2.\n";
                            return;
                        }
                        
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
                // Check if the screen name exists at all.
                else if (!ScreenManager::getInstance()->hasScreen(screenName)) {
                    cout << "Process '" << screenName << "' not found.\n";
                }
                else {
                    auto screen = ScreenManager::getInstance()->getScreen(screenName);

                    // Check for a memory violation.
                    if (screen->hasMemoryViolation()) {
                        string timeOnly = screen->getMemoryViolationTime();
                        size_t timeStart = timeOnly.find(", ") + 2;

                        cout << "Process " << screen->getName()
                            << " shut down due to memory access violation error that occurred at "
                            << timeOnly.substr(timeStart) << ". " << screen->getMemoryViolationAddress()
                            << " invalid." << endl;
                    }
                    else {
                        ScreenManager::getInstance()->switchScreen(screenName);
                        CLIController::getInstance()->clearScreen();
                    }
                }
            }
            else if (subcommand == "-c") {
                string processName, memorySizeStr, instructionsStr;
                ss >> processName >> memorySizeStr;
                getline(ss, instructionsStr);
                size_t firstQuote = instructionsStr.find('"');
                size_t lastQuote = instructionsStr.rfind('"');

                if (processName.empty() || memorySizeStr.empty() || firstQuote == string::npos || lastQuote == string::npos || firstQuote == lastQuote) {
                    cout << "Usage: screen -c <name> <memory_size> \"<instructions>\"\n";
                    return;
                }

                instructionsStr = instructionsStr.substr(firstQuote + 1, lastQuote - firstQuote - 1);

                try {
                    int memSize = stoi(memorySizeStr);
                    if (!isPowerOfTwo(memSize)) {
                        cout << "Invalid memory allocation. Size must be a power of 2.\n";
                        return;
                    }
                    if (memSize < 64 || memSize > 65536) {
                        cout << "Invalid memory allocation. Size must be between 64 and 65536 bytes.\n";
                        return;
                    }
                    if (ScreenManager::getInstance()->hasScreen(processName)) {
                        cout << "Screen '" << processName << "' already exists.\n";
                        return;
                    }

                    // Call parser function
                    vector<Instruction> userInstructions = parseInstructions(instructionsStr);

                    // Validate instruction 
                    if (userInstructions.empty() || userInstructions.size() > 50) {
                        cout << "Invalid command: Instruction count must be between 1 and 50.\n";
                        return;
                    }

                    // Create and register the new screen with the parsed instructions
                    auto newScreen = make_shared<Screen>(processName, userInstructions, CLIController::getInstance()->getTimestamp());
                    MemoryManager::getInstance()->setupProcessMemory(processName, memSize);
                    ScreenManager::getInstance()->registerScreen(processName, newScreen);
                    Scheduler::getInstance()->addProcessToQueue(newScreen);

                    cout << "Process '" << processName << "' created successfully with " << userInstructions.size() << " instructions." << endl;

                }
                catch (const invalid_argument& e) {
                    cout << "Invalid memory size provided: " << e.what() << endl;
                }
                catch (const runtime_error& e) {
                    cout << "Error parsing instructions: " << e.what() << endl;
                }
            }
            else {
				cout << "Unknown screen command '" << subcommand << " \n";
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
            ScreenManager::getInstance()->displaySystemSmiSummary();
        }
        else if (command == "vmstat") {
            ScreenManager::getInstance()->displayVmStat(); 
        }
        else {
            cout << "Unknown command '" << command << "'. Type 'help' for available commands.\n";
        }
    }
    else { // if inside a specific process screen
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

// Starts the main input loop that continuously listens for user commands.
void CommandInputController::startInputLoop() {
    while (Kernel::getInstance()->getRunningStatus()) {
        handleInputEntry(); // Your existing function already has the logic
    }
}