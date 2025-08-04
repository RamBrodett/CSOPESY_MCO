#include "Screen.h"
#include "CLIController.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <numeric>
#include <stack>
#include "Scheduler.h"
using namespace std;

// Default constructor for creating placeholder screens (like 'main')
Screen::Screen()
    : name(""), instructions({}), timestamp(CLIController::getInstance()->getTimestamp()),
    programCounter(0), cpuCoreID(-1), isRunning(false), memoryViolationOccurred(false) {
}

// Constructor for creating a new process with a name, instructions, and creation timestamp.
Screen::Screen(string name, std::vector<Instruction> instructions, string timestamp)
    : name(name), instructions(instructions), timestamp(timestamp), programCounter(0),
    cpuCoreID(-1), isRunning(false), memoryViolationOccurred(false) {
}

// --- Getters ---
string Screen::getName() const { return name; }
int Screen::getProgramCounter() const { return programCounter; }
int Screen::getTotalInstructions() const { return static_cast<int>(instructions.size()); }
string Screen::getTimestamp() const { return timestamp; }
string Screen::getTimestampFinished() const { return timestampFinished; }
int Screen::getCoreID() const { return cpuCoreID; }
bool Screen::getIsRunning() const { return isRunning; }
bool Screen::isFinished() const {
    return programCounter >= getTotalInstructions() || hasMemoryViolation();
}

std::vector<std::string> Screen::getOutputBuffer() const {
    std::lock_guard<std::mutex> lock(outputMutex);
    return outputBuffer;
}

std::vector<std::string> Screen::flushOutputBuffer() {
    lock_guard<mutex> lock(outputMutex);
    std::vector<std::string> flushedOutput;
    std::swap(flushedOutput, outputBuffer);
    return flushedOutput;
}

// --- Setters---
void Screen::setName(string name) { this->name = name; }
void Screen::setTimestampFinished(string ts) { timestampFinished = ts; }
void Screen::setProgramCounter(int pc) { programCounter = pc; }
void Screen::setInstructions(const std::vector<Instruction>& instructions) {
    this->instructions = instructions;
}
void Screen::setTimestamp(const string& ts) { timestamp = ts; }
void Screen::setCoreID(int id) { cpuCoreID = id; }
void Screen::setIsRunning(bool running) { isRunning = running; }

// --- Private Helper Methods ---
void Screen::addOutput(const std::string& message) {
    lock_guard<mutex> lock(outputMutex);
    outputBuffer.push_back(message);
}

uint16_t Screen::getOperandValue(const Operand& op) {
    if (op.isVariable) {
        ensureSymbolTableLoaded(); 
        if (hasMemoryViolation()) return 0; 

        if (variables.find(op.variableName) == variables.end()) {
            return 0;
        }
        return variables[op.variableName];
    }
    return op.value;
}

void Screen::setVariableValue(const std::string& name, uint16_t value) {
    ensureSymbolTableLoaded(); 
    if (hasMemoryViolation()) return; 

    variables[name] = value;
}

void Screen::executeInstructionList(const std::vector<Instruction>& instructionList) {
    for (const auto& instruction : instructionList) {

        if (hasMemoryViolation()) return;

        // --- Busy-wait delay ---
        int delay = Scheduler::getInstance()->getDelayPerExec();
        if (delay > 0) {
            for (volatile int d = 0; d < delay; ++d) { /* busy-wait */ }
        }

        switch (instruction.type) {
        case InstructionType::DECLARE:
            if (canDeclareVariable()) {
                setVariableValue(instruction.operands[0].variableName, getOperandValue(instruction.operands[1]));
            }
            break;
        case InstructionType::ADD:
            setVariableValue(instruction.operands[0].variableName, getOperandValue(instruction.operands[1]) + getOperandValue(instruction.operands[2]));
            break;
        case InstructionType::SUBTRACT:
            setVariableValue(instruction.operands[0].variableName, getOperandValue(instruction.operands[1]) - getOperandValue(instruction.operands[2]));
            break;
        case InstructionType::PRINT: {
            string output = instruction.printMessage;
            if (!instruction.operands.empty() && instruction.operands[0].isVariable) {
                const auto& varName = instruction.operands[0].variableName;
                string placeholder = "%" + varName + "%";
                size_t pos = output.find(placeholder);
                if (pos != string::npos) {
                    output.replace(pos, placeholder.length(), to_string(getOperandValue(instruction.operands[0])));
                }
            }
            string timestamp = CLIController::getInstance()->getTimestamp();
            string formattedLog = "(" + timestamp + ") Core:" + to_string(cpuCoreID) + " \"" + output + "\"";
            addOutput(formattedLog);
            break;
        }
        case InstructionType::READ: {
            uint16_t address = instruction.memoryAddress;
            uint16_t value;
            if (MemoryManager::getInstance()->readMemory(name, address, value)) {
                if (canDeclareVariable() || variables.find(instruction.operands[0].variableName) != variables.end()) {
                    setVariableValue(instruction.operands[0].variableName, value);
                }
            }
            else {
                triggerMemoryViolation(address);
                return;
            }
            break;
        }
        case InstructionType::WRITE: {
            uint16_t address = instruction.memoryAddress;
            uint16_t value = getOperandValue(instruction.operands[0]);

            if (!MemoryManager::getInstance()->writeMemory(name, address, value)) {
                triggerMemoryViolation(address);
                return;
            }
            break;
        }

        case InstructionType::SLEEP:
            this_thread::sleep_for(chrono::milliseconds(getOperandValue(instruction.operands[0])));
            break;
        case InstructionType::FOR: {
            uint16_t repeats = getOperandValue(instruction.operands[0]);
            for (uint16_t i = 0; i < repeats; ++i) {
               
                executeInstructionList(instruction.innerInstructions);
            }
            break;
        }
        }
    }
}


// Executes the process's instructions for a given number of cycles (quantum).
void Screen::execute(int quantum) {
    if (isFinished()) return;
    setIsRunning(true);

    // Determines how many top-level instructions to run
    int instructionsToExecute = (quantum == -1) ? (getTotalInstructions() - programCounter) : quantum;

    for (int i = 0; i < instructionsToExecute && !isFinished(); ++i) {
        const auto& instruction = instructions[programCounter];

        // --- Busy-wait delay ---
        int delay = Scheduler::getInstance()->getDelayPerExec();
        if (delay > 0) {
            for (volatile int d = 0; d < delay; ++d) { /* busy-wait */ }
        }

        switch (instruction.type) {

        case InstructionType::DECLARE:
        case InstructionType::ADD:
        case InstructionType::SUBTRACT:
        case InstructionType::PRINT:
        case InstructionType::SLEEP:
        case InstructionType::READ:
        case InstructionType::WRITE:
            
            executeInstructionList({ instruction });
            break;

        case InstructionType::FOR: {
            uint16_t repeats = getOperandValue(instruction.operands[0]);
            for (uint16_t j = 0; j < repeats; ++j) {
                executeInstructionList(instruction.innerInstructions);
            }
            break;
        }

        }
     
        programCounter++;
    }

    if (isFinished()) {
        setTimestampFinished(CLIController::getInstance()->getTimestamp());
        setIsRunning(false);
    }
};

bool Screen::hasMemoryViolation() const {
    return memoryViolationOccurred;
}

std::string Screen::getMemoryViolationAddress() const {
    return memoryViolationAddress;
}

std::string Screen::getMemoryViolationTime() const {
    return memoryViolationTime;
}

bool Screen::canDeclareVariable() const {
    return variables.size() < MAX_VARIABLES;
}

int Screen::getVariableCount() const {
    return static_cast<int>(variables.size());
}

// Triggers a memory violation, recording the address and time, and stopping the process.
void Screen::triggerMemoryViolation(uint16_t address) {
    stringstream ss;
    ss << "0x" << hex << uppercase << address;
    memoryViolationAddress = ss.str();
    memoryViolationTime = CLIController::getInstance()->getTimestamp();
    memoryViolationOccurred = true;
    setTimestampFinished(memoryViolationTime);
    setIsRunning(false);
}

// Ensures the page containing the symbol table (address 0x0) is loaded into memory.
void Screen::ensureSymbolTableLoaded() {
    if (hasMemoryViolation()) return; 

    uint16_t dummy_value; 
    if (!MemoryManager::getInstance()->readMemory(name, 0x0, dummy_value)) {
        triggerMemoryViolation(0x0);
    }
}