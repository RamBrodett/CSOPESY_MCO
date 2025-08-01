#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <mutex>
#include <memory>
#include "Instruction.h"

class Screen {
public:
    enum STATES {
        READY,
        WAITING,
        RUNNING,
        FINISHED
    };
    // -- Contructors --
    Screen(std::string name, std::vector<Instruction> instructions, std::string timestamp);
    Screen();

    // --- Getters ---
    std::string getName() const;
    int getProgramCounter() const;
    int getTotalInstructions() const; //casted from size_t to int
    std::string getTimestamp() const;
    std::string getTimestampFinished() const;
    int getCoreID() const;
    bool getIsRunning() const;
    std::vector<std::string> flushOutputBuffer(); //clears and returns output
    bool isFinished() const;
    std::vector<std::string> getOutputBuffer() const;

    // --- Setters ---
    void setName(std::string name);
    void setTimestampFinished(std::string timestampFinished);
    void setProgramCounter(int pc);
    void setInstructions(const std::vector<Instruction>& instructions);
    void setTimestamp(const std::string& ts);
    void setCoreID(int coreID);
    void setIsRunning(bool running);
    //if -1, then it is not round robin
    void execute(int quantum = -1); 

    // Add memory violation tracking
    bool hasMemoryViolation() const;
    std::string getMemoryViolationAddress() const;
    std::string getMemoryViolationTime() const;

    // Symbol table management (64 bytes = 32 variables max)
    bool canDeclareVariable() const;
    int getVariableCount() const;

private:
    // Helper methods
    uint16_t getOperandValue(const Operand& op);
    void setVariableValue(const std::string& name, uint16_t value);
    void addOutput(const std::string& message);
    void executeInstructionList(const std::vector<Instruction>& instructionList);

    // Memory violation tracking
    bool memoryViolationOccurred;
    std::string memoryViolationAddress;
    std::string memoryViolationTime;

    // Symbol table limit (32 variables max)
    static const int MAX_VARIABLES = 32;

    // Helper method for handling violations
    void triggerMemoryViolation(uint16_t address);

    // --- Member variables ---
    std::string name;
    std::vector<Instruction> instructions;
    std::string timestamp;

    int programCounter; //index of instruction
    int cpuCoreID;
    std::string timestampFinished;
    bool isRunning; 

    std::unordered_map<std::string, uint16_t> variables; //memory storage for the process's variables
    mutable std::mutex outputMutex; //protect concurrent access to the outputBUffer
    std::vector<std::string> outputBuffer; //buffer to store log messages from PRINT
};