#include "Screen.h"
#include "CLIController.h"
#include "Scheduler.h"
#include "MemoryManager.h" // Ensure MemoryManager is included for its functions
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

// Constructors

// Default constructor for creating placeholder screens (like 'main')
Screen::Screen()
    : name(""), instructions({}), timestamp(CLIController::getInstance()->getTimestamp()),
    programCounter(0), cpuCoreID(-1), isRunning(false), memoryViolationOccurred(false), next_variable_offset(0) {
}

// Constructor for creating a new process with a name, instructions, and creation timestamp.
Screen::Screen(std::string name, std::vector<Instruction> instructions, std::string timestamp)
    : name(name), instructions(instructions), timestamp(timestamp), programCounter(0),
    cpuCoreID(-1), isRunning(false), memoryViolationOccurred(false), next_variable_offset(0) {
}


// Getters

std::string Screen::getName() const { return name; }
int Screen::getProgramCounter() const { return programCounter; }
int Screen::getTotalInstructions() const { return static_cast<int>(instructions.size()); }
std::string Screen::getTimestamp() const { return timestamp; }
std::string Screen::getTimestampFinished() const { return timestampFinished; }
int Screen::getCoreID() const { return cpuCoreID; }
bool Screen::getIsRunning() const { return isRunning; }

bool Screen::isFinished() const {
    // A process is finished if its PC is past the end or a memory violation occurred.
    return (programCounter >= getTotalInstructions() && getTotalInstructions() > 0) || hasMemoryViolation();
}

std::vector<std::string> Screen::getOutputBuffer() const {
    std::lock_guard<std::mutex> lock(outputMutex);
    return outputBuffer;
}

bool Screen::hasMemoryViolation() const {
    return memoryViolationOccurred;
}

std::string Screen::getMemoryViolationAddress() const {
    return memoryViolationAddress;
}

std::string Screen::getMemoryViolationTime() const {
    return memoryViolationTime;
}

// Setters

void Screen::setCoreID(int id) { cpuCoreID = id; }
void Screen::setIsRunning(bool running) { isRunning = running; }

// Executes the process's instructions for a given number of cycles (quantum).
void Screen::execute(int quantum) {
    if (isFinished()) return;
    setIsRunning(true);

    int instructionsToExecute = (quantum == -1) ? (getTotalInstructions() - programCounter) : quantum;

    for (int i = 0; i < instructionsToExecute && !isFinished(); ++i) {
        if (programCounter >= instructions.size()) {
            break;
        }
        const auto& instruction = instructions[programCounter];

        int delay = Scheduler::getInstance()->getDelayPerExec();
        if (delay > 0) {
            for (volatile int d = 0; d < delay; ++d) { /* busy-wait */ }
        }

        executeInstructionList({ instruction });

        programCounter++;
    }

    // Check if finished *after* the loop
    if (programCounter >= getTotalInstructions() && !hasMemoryViolation()) {
        setTimestampFinished(CLIController::getInstance()->getTimestamp());
        setIsRunning(false);
    }
}

// Private Helper Methods

void Screen::addOutput(const std::string& message) {
    std::lock_guard<std::mutex> lock(outputMutex);
    outputBuffer.push_back(message);
}

// Check if there is space for a new variable in the 64-byte symbol table.
bool Screen::canDeclareVariable() const {
    // Each variable is 2 bytes, symbol table is 64 bytes max, so 32 variables. [cite: 128]
    return variable_offsets.size() < 32;
}

uint16_t Screen::getOperandValue(const Operand& op) {
    if (op.isVariable) {
        ensureSymbolTableLoaded();
        if (hasMemoryViolation()) return 0;

        if (variable_offsets.find(op.variableName) == variable_offsets.end()) {
            return 0; // Variable not found, return 0 as per spec. [cite: 128]
        }

        uint16_t address = variable_offsets.at(op.variableName);
        uint16_t value_from_mem = 0;

        if (!MemoryManager::getInstance()->readMemory(this->name, address, value_from_mem)) {
            triggerMemoryViolation(address);
            return 0;
        }
        return value_from_mem;
    }
    return op.value;
}

void Screen::setVariableValue(const std::string& name, uint16_t value) {
    ensureSymbolTableLoaded();
    if (hasMemoryViolation()) return;

    uint16_t address;

    if (variable_offsets.count(name)) {
        address = variable_offsets.at(name);
    }
    else {
        // New variable from a DECLARE instruction.
        if (next_variable_offset >= 64) {
            // Symbol table is full, ignore declaration. [cite: 128]
            return;
        }
        address = next_variable_offset;
        variable_offsets[name] = address;
        next_variable_offset += 2;// Each variable consumes 2 bytes. [cite: 128]
    }

    if (!MemoryManager::getInstance()->writeMemory(this->name, address, value)) {
        triggerMemoryViolation(address);
    }
}

void Screen::executeInstructionList(const std::vector<Instruction>& instructionList) {
    for (const auto& instruction : instructionList) {
        if (hasMemoryViolation()) return;

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
            std::string output = instruction.printMessage;
            if (!instruction.operands.empty() && instruction.operands[0].isVariable) {
                const auto& varName = instruction.operands[0].variableName;
                std::string placeholder = "%" + varName + "%";
                size_t pos = output.find(placeholder);
                if (pos != std::string::npos) {
                    output.replace(pos, placeholder.length(), std::to_string(getOperandValue(instruction.operands[0])));
                }
            }
            std::string timestamp = CLIController::getInstance()->getTimestamp();
            std::string formattedLog = "(" + timestamp + ") Core:" + std::to_string(cpuCoreID) + " \"" + output + "\"";
            addOutput(formattedLog);
            break;
        }
        case InstructionType::READ: {
            uint16_t address = instruction.memoryAddress;
            uint16_t value;
            if (MemoryManager::getInstance()->readMemory(name, address, value)) {
                // Check if we can declare a new variable OR if it already exists in the correct map.
                if (canDeclareVariable() || variable_offsets.count(instruction.operands[0].variableName)) {
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
            std::this_thread::sleep_for(std::chrono::milliseconds(getOperandValue(instruction.operands[0])));
            break;
        case InstructionType::FOR: {
            uint16_t repeats = getOperandValue(instruction.operands[0]);
            for (uint16_t i = 0; i < repeats; ++i) {
                if (hasMemoryViolation()) break; // Check for violation inside the loop
                executeInstructionList(instruction.innerInstructions);
            }
            break;
        }
        }
    }
}

void Screen::triggerMemoryViolation(uint16_t address) {
    if (memoryViolationOccurred) return; // Prevent multiple triggers

    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase << address;
    memoryViolationAddress = ss.str();
    memoryViolationTime = CLIController::getInstance()->getTimestamp();
    memoryViolationOccurred = true;
    setTimestampFinished(memoryViolationTime); // A memory violation also "finishes" the process
    setIsRunning(false);
}

void Screen::ensureSymbolTableLoaded() {
    if (hasMemoryViolation()) return;

    // Reading from address 0x0 forces the MemoryManager to load the first page
    // of the process (the symbol table) if it's not already present.
    uint16_t dummy_value;
    if (!MemoryManager::getInstance()->readMemory(name, 0x0, dummy_value)) {
        // This will trigger a page fault handled by the manager.
        // If it still fails after that (e.g., invalid logical address), a violation is triggered.
        if (!hasMemoryViolation()) { // Avoid double-triggering
            triggerMemoryViolation(0x0);
        }
    }
}

void Screen::setTimestampFinished(std::string ts) {
    // Only set the finish time if it hasn't been set yet (for memory violations)
    if (timestampFinished.empty()) {
        timestampFinished = ts;
    }
}