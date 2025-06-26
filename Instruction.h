#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>

enum class InstructionType {  
    PRINT,  
    DECLARE,  
    ADD,  
    SUBTRACT,  
    SLEEP,  
    FOR, 
    ENDFOR 
};

struct Operand {
    bool isVariable;
    std::string variableName;
    uint16_t value;
};

struct Instruction {
    InstructionType type;
    std::vector<Operand> operands;
    std::string printMessage;
    std::vector<Instruction> innerInstructions;
};