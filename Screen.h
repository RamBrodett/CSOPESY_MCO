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
    Screen(std::string name, int virtualMemorySize, std::vector<Instruction> instructions, std::string timestamp);
    Screen();

    // --- Getters ---
    std::string getName() const;
    int getProgramCounter() const;
    int getTotalInstructions() const;
    std::string getTimestamp() const;
    std::string getTimestampFinished() const;
    int getCoreID() const;
    bool getIsRunning() const;
    bool isFinished() const;
    bool hasMemoryViolation() const; // New
    int getViolationAddress() const; // New
    std::string getViolationTimestamp() const; // New
    int getVirtualMemorySize() const; // New

    std::vector<std::string> getOutputBuffer() const;
    std::vector<std::string> flushOutputBuffer();




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

private:
    // Helper methods
    uint16_t getOperandValue(const Operand& op);
    void setVariableValue(const std::string& name, uint16_t value);
    void addOutput(const std::string& message);
    void executeInstructionList(const std::vector<Instruction>& instructionList);

    // --- Member variables ---
    std::string name;
    std::vector<Instruction> instructions;
    std::string timestamp;

    int programCounter; //index of instruction
    int cpuCoreID;
    std::string timestampFinished;
    bool isRunning; 


    // --- NEW: Helper for memory violation ---
    void triggerMemoryViolation(int address);

    // --- NEW: Process State & Memory ---
    int virtualMemorySize;
    bool memoryAccessViolation = false;
    int violationAddress = 0;
    std::string violationTimestamp;
    const int SYMBOL_TABLE_SIZE = 64;


    std::unordered_map<std::string, uint16_t> variables; //memory storage for the process's variables
    mutable std::mutex outputMutex; //protect concurrent access to the outputBUffer
    std::vector<std::string> outputBuffer; //buffer to store log messages from PRINT
};

