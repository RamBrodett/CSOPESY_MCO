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

    Screen(std::string name, std::vector<Instruction> instructions, std::string timestamp);
    Screen();

    // --- Getters ---
    std::string getName() const;
    int getProgramCounter() const;
    int getTotalInstructions() const;
    std::string getTimestamp() const;
    std::string getTimestampFinished() const;
    int getCoreID() const;
    bool getIsRunning() const;
    std::vector<std::string> flushOutputBuffer();
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

    void execute(int quantum = -1); // -1 for full execution

private:
    // Helper methods
    uint16_t getOperandValue(const Operand& op);
    void setVariableValue(const std::string& name, uint16_t value);
    void addOutput(const std::string& message);

    // --- Member variables reordered to fix warnings ---
    std::string name;
    std::vector<Instruction> instructions;
    std::string timestamp;

    int programCounter;
    int cpuCoreID;
    std::string timestampFinished;
    bool isRunning;

    std::unordered_map<std::string, uint16_t> variables;
    mutable std::mutex outputMutex;
    std::vector<std::string> outputBuffer;
};