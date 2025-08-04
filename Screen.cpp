#include "Screen.h"
#include "CLIController.h"
#include <fstream>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <numeric>
#include <stack>
#include "Scheduler.h"
using namespace std;
// Corrected Constructor to match header declaration order

Screen::Screen()
    : name(""), instructions({}), timestamp(CLIController::getInstance()->getTimestamp()),
    programCounter(0), cpuCoreID(-1), isRunning(false), memoryViolationOccurred(false) {
}

Screen::Screen(string name, std::vector<Instruction> instructions, string timestamp)
    : name(name), instructions(instructions), timestamp(timestamp), programCounter(0),
    cpuCoreID(-1), isRunning(false), memoryViolationOccurred(false) {
}

// --- Getters (definitions match the corrected header) ---
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

// --- Setters (definitions match the corrected header) ---
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
        ensureSymbolTableLoaded(); // ADD THIS LINE
        if (hasMemoryViolation()) return 0; // Check if the access failed

        if (variables.find(op.variableName) == variables.end()) {
            return 0;
        }
        return variables[op.variableName];
    }
    return op.value;
}

void Screen::setVariableValue(const std::string& name, uint16_t value) {
    ensureSymbolTableLoaded(); // ADD THIS LINE
    if (hasMemoryViolation()) return; // Check if the access failed

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
            // Note: No 'programCounter++' here, as this is for inner loops
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
                // Recursive call to handle nested loops
                executeInstructionList(instruction.innerInstructions);
            }
            break;
        }
        }
    }
}


void Screen::execute(int quantum) {
    if (isFinished()) return;
    setIsRunning(true);

    // Determine how many top-level instructions to run
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
            //for simple instructions, just execute them using the helper
            executeInstructionList({ instruction });
            break;

        case InstructionType::FOR: {
            uint16_t repeats = getOperandValue(instruction.operands[0]);
            for (uint16_t j = 0; j < repeats; ++j) {
                //call the helper to execute the inner instructions
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

void Screen::triggerMemoryViolation(uint16_t address) {
    stringstream ss;
    ss << "0x" << hex << uppercase << address;
    memoryViolationAddress = ss.str();
    memoryViolationTime = CLIController::getInstance()->getTimestamp();
    memoryViolationOccurred = true;
    setTimestampFinished(memoryViolationTime);
    setIsRunning(false);
}


void Screen::ensureSymbolTableLoaded() {
    if (hasMemoryViolation()) return; // Don't do anything if we already failed

    uint16_t dummy_value; // We don't care about the value, just the access attempt
    if (!MemoryManager::getInstance()->readMemory(name, 0x0, dummy_value)) {
        // If the read fails, it means an access violation occurred during the page fault.
        // The readMemory function itself doesn't cause the violation, but the fault handler might.
        // We'll trigger our violation handler here to be safe.
        triggerMemoryViolation(0x0);
    }
}