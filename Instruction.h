#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>

//defines the types of operations a process can execute
enum class InstructionType {  
    PRINT,  
    DECLARE,  
    ADD,  
    SUBTRACT,  
    SLEEP,  
    FOR, 
    ENDFOR,
    READ,
	WRITE
};
//operands for an instruction
struct Operand {
    bool isVariable;
    std::string variableName;
    uint16_t value;
};
//single executable instruction within a process
struct Instruction {
    InstructionType type = InstructionType::PRINT;
    std::vector<Operand> operands;
    std::string printMessage = "";
    std::vector<Instruction> innerInstructions;
    uint16_t memoryAddress = 0;
};