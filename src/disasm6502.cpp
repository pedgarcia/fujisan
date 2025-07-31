/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "disasm6502.h"
#include <QString>

// 6502 addressing modes
enum AddressMode {
    MODE_IMPLIED,      // No operand
    MODE_ACCUMULATOR,  // Operates on accumulator
    MODE_IMMEDIATE,    // #$nn
    MODE_ZERO_PAGE,    // $nn
    MODE_ZERO_PAGE_X,  // $nn,X
    MODE_ZERO_PAGE_Y,  // $nn,Y
    MODE_ABSOLUTE,     // $nnnn
    MODE_ABSOLUTE_X,   // $nnnn,X
    MODE_ABSOLUTE_Y,   // $nnnn,Y
    MODE_INDIRECT,     // ($nnnn)
    MODE_INDIRECT_X,   // ($nn,X)
    MODE_INDIRECT_Y,   // ($nn),Y
    MODE_RELATIVE      // Branch instructions
};

// Instruction info
struct InstructionInfo {
    const char* mnemonic;
    AddressMode mode;
    int bytes;  // Total instruction length including opcode
};

// 6502 instruction table indexed by opcode
static const InstructionInfo instructionTable[256] = {
    // 0x00-0x0F
    {"BRK", MODE_IMPLIED, 1},      {"ORA", MODE_INDIRECT_X, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"ORA", MODE_ZERO_PAGE, 2},    {"ASL", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"PHP", MODE_IMPLIED, 1},      {"ORA", MODE_IMMEDIATE, 2},    {"ASL", MODE_ACCUMULATOR, 1},  {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"ORA", MODE_ABSOLUTE, 3},     {"ASL", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0x10-0x1F
    {"BPL", MODE_RELATIVE, 2},     {"ORA", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"ORA", MODE_ZERO_PAGE_X, 2},  {"ASL", MODE_ZERO_PAGE_X, 2},  {"???", MODE_IMPLIED, 1},
    {"CLC", MODE_IMPLIED, 1},      {"ORA", MODE_ABSOLUTE_Y, 3},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"ORA", MODE_ABSOLUTE_X, 3},   {"ASL", MODE_ABSOLUTE_X, 3},   {"???", MODE_IMPLIED, 1},
    
    // 0x20-0x2F
    {"JSR", MODE_ABSOLUTE, 3},     {"AND", MODE_INDIRECT_X, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"BIT", MODE_ZERO_PAGE, 2},    {"AND", MODE_ZERO_PAGE, 2},    {"ROL", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"PLP", MODE_IMPLIED, 1},      {"AND", MODE_IMMEDIATE, 2},    {"ROL", MODE_ACCUMULATOR, 1},  {"???", MODE_IMPLIED, 1},
    {"BIT", MODE_ABSOLUTE, 3},     {"AND", MODE_ABSOLUTE, 3},     {"ROL", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0x30-0x3F
    {"BMI", MODE_RELATIVE, 2},     {"AND", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"AND", MODE_ZERO_PAGE_X, 2},  {"ROL", MODE_ZERO_PAGE_X, 2},  {"???", MODE_IMPLIED, 1},
    {"SEC", MODE_IMPLIED, 1},      {"AND", MODE_ABSOLUTE_Y, 3},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"AND", MODE_ABSOLUTE_X, 3},   {"ROL", MODE_ABSOLUTE_X, 3},   {"???", MODE_IMPLIED, 1},
    
    // 0x40-0x4F
    {"RTI", MODE_IMPLIED, 1},      {"EOR", MODE_INDIRECT_X, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"EOR", MODE_ZERO_PAGE, 2},    {"LSR", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"PHA", MODE_IMPLIED, 1},      {"EOR", MODE_IMMEDIATE, 2},    {"LSR", MODE_ACCUMULATOR, 1},  {"???", MODE_IMPLIED, 1},
    {"JMP", MODE_ABSOLUTE, 3},     {"EOR", MODE_ABSOLUTE, 3},     {"LSR", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0x50-0x5F
    {"BVC", MODE_RELATIVE, 2},     {"EOR", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"EOR", MODE_ZERO_PAGE_X, 2},  {"LSR", MODE_ZERO_PAGE_X, 2},  {"???", MODE_IMPLIED, 1},
    {"CLI", MODE_IMPLIED, 1},      {"EOR", MODE_ABSOLUTE_Y, 3},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"EOR", MODE_ABSOLUTE_X, 3},   {"LSR", MODE_ABSOLUTE_X, 3},   {"???", MODE_IMPLIED, 1},
    
    // 0x60-0x6F
    {"RTS", MODE_IMPLIED, 1},      {"ADC", MODE_INDIRECT_X, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"ADC", MODE_ZERO_PAGE, 2},    {"ROR", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"PLA", MODE_IMPLIED, 1},      {"ADC", MODE_IMMEDIATE, 2},    {"ROR", MODE_ACCUMULATOR, 1},  {"???", MODE_IMPLIED, 1},
    {"JMP", MODE_INDIRECT, 3},     {"ADC", MODE_ABSOLUTE, 3},     {"ROR", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0x70-0x7F
    {"BVS", MODE_RELATIVE, 2},     {"ADC", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"ADC", MODE_ZERO_PAGE_X, 2},  {"ROR", MODE_ZERO_PAGE_X, 2},  {"???", MODE_IMPLIED, 1},
    {"SEI", MODE_IMPLIED, 1},      {"ADC", MODE_ABSOLUTE_Y, 3},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"ADC", MODE_ABSOLUTE_X, 3},   {"ROR", MODE_ABSOLUTE_X, 3},   {"???", MODE_IMPLIED, 1},
    
    // 0x80-0x8F
    {"???", MODE_IMPLIED, 1},      {"STA", MODE_INDIRECT_X, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"STY", MODE_ZERO_PAGE, 2},    {"STA", MODE_ZERO_PAGE, 2},    {"STX", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"DEY", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},      {"TXA", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"STY", MODE_ABSOLUTE, 3},     {"STA", MODE_ABSOLUTE, 3},     {"STX", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0x90-0x9F
    {"BCC", MODE_RELATIVE, 2},     {"STA", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"STY", MODE_ZERO_PAGE_X, 2},  {"STA", MODE_ZERO_PAGE_X, 2},  {"STX", MODE_ZERO_PAGE_Y, 2},  {"???", MODE_IMPLIED, 1},
    {"TYA", MODE_IMPLIED, 1},      {"STA", MODE_ABSOLUTE_Y, 3},   {"TXS", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"STA", MODE_ABSOLUTE_X, 3},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    
    // 0xA0-0xAF
    {"LDY", MODE_IMMEDIATE, 2},    {"LDA", MODE_INDIRECT_X, 2},   {"LDX", MODE_IMMEDIATE, 2},    {"???", MODE_IMPLIED, 1},
    {"LDY", MODE_ZERO_PAGE, 2},    {"LDA", MODE_ZERO_PAGE, 2},    {"LDX", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"TAY", MODE_IMPLIED, 1},      {"LDA", MODE_IMMEDIATE, 2},    {"TAX", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"LDY", MODE_ABSOLUTE, 3},     {"LDA", MODE_ABSOLUTE, 3},     {"LDX", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0xB0-0xBF
    {"BCS", MODE_RELATIVE, 2},     {"LDA", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"LDY", MODE_ZERO_PAGE_X, 2},  {"LDA", MODE_ZERO_PAGE_X, 2},  {"LDX", MODE_ZERO_PAGE_Y, 2},  {"???", MODE_IMPLIED, 1},
    {"CLV", MODE_IMPLIED, 1},      {"LDA", MODE_ABSOLUTE_Y, 3},   {"TSX", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"LDY", MODE_ABSOLUTE_X, 3},   {"LDA", MODE_ABSOLUTE_X, 3},   {"LDX", MODE_ABSOLUTE_Y, 3},   {"???", MODE_IMPLIED, 1},
    
    // 0xC0-0xCF
    {"CPY", MODE_IMMEDIATE, 2},    {"CMP", MODE_INDIRECT_X, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"CPY", MODE_ZERO_PAGE, 2},    {"CMP", MODE_ZERO_PAGE, 2},    {"DEC", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"INY", MODE_IMPLIED, 1},      {"CMP", MODE_IMMEDIATE, 2},    {"DEX", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"CPY", MODE_ABSOLUTE, 3},     {"CMP", MODE_ABSOLUTE, 3},     {"DEC", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0xD0-0xDF
    {"BNE", MODE_RELATIVE, 2},     {"CMP", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"CMP", MODE_ZERO_PAGE_X, 2},  {"DEC", MODE_ZERO_PAGE_X, 2},  {"???", MODE_IMPLIED, 1},
    {"CLD", MODE_IMPLIED, 1},      {"CMP", MODE_ABSOLUTE_Y, 3},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"CMP", MODE_ABSOLUTE_X, 3},   {"DEC", MODE_ABSOLUTE_X, 3},   {"???", MODE_IMPLIED, 1},
    
    // 0xE0-0xEF
    {"CPX", MODE_IMMEDIATE, 2},    {"SBC", MODE_INDIRECT_X, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"CPX", MODE_ZERO_PAGE, 2},    {"SBC", MODE_ZERO_PAGE, 2},    {"INC", MODE_ZERO_PAGE, 2},    {"???", MODE_IMPLIED, 1},
    {"INX", MODE_IMPLIED, 1},      {"SBC", MODE_IMMEDIATE, 2},    {"NOP", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"CPX", MODE_ABSOLUTE, 3},     {"SBC", MODE_ABSOLUTE, 3},     {"INC", MODE_ABSOLUTE, 3},     {"???", MODE_IMPLIED, 1},
    
    // 0xF0-0xFF
    {"BEQ", MODE_RELATIVE, 2},     {"SBC", MODE_INDIRECT_Y, 2},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"SBC", MODE_ZERO_PAGE_X, 2},  {"INC", MODE_ZERO_PAGE_X, 2},  {"???", MODE_IMPLIED, 1},
    {"SED", MODE_IMPLIED, 1},      {"SBC", MODE_ABSOLUTE_Y, 3},   {"???", MODE_IMPLIED, 1},      {"???", MODE_IMPLIED, 1},
    {"???", MODE_IMPLIED, 1},      {"SBC", MODE_ABSOLUTE_X, 3},   {"INC", MODE_ABSOLUTE_X, 3},   {"???", MODE_IMPLIED, 1}
};

DisassembledInstruction disassemble6502(const unsigned char* memory, unsigned short address, unsigned short maxAddress)
{
    DisassembledInstruction result;
    
    if (address > maxAddress) {
        result.mnemonic = "???";
        result.operand = "";
        result.bytes = 1;
        result.opcode = 0;
        return result;
    }
    
    unsigned char opcode = memory[address];
    const InstructionInfo& info = instructionTable[opcode];
    
    result.opcode = opcode;
    result.mnemonic = info.mnemonic;
    result.bytes = info.bytes;
    
    // Format operand based on addressing mode
    switch (info.mode) {
        case MODE_IMPLIED:
        case MODE_ACCUMULATOR:
            result.operand = "";
            break;
            
        case MODE_IMMEDIATE:
            if (address + 1 <= maxAddress) {
                unsigned char value = memory[address + 1];
                result.operand = QString("#$%1").arg(value, 2, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_ZERO_PAGE:
            if (address + 1 <= maxAddress) {
                unsigned char addr = memory[address + 1];
                result.operand = QString("$%1").arg(addr, 2, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_ZERO_PAGE_X:
            if (address + 1 <= maxAddress) {
                unsigned char addr = memory[address + 1];
                result.operand = QString("$%1,X").arg(addr, 2, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_ZERO_PAGE_Y:
            if (address + 1 <= maxAddress) {
                unsigned char addr = memory[address + 1];
                result.operand = QString("$%1,Y").arg(addr, 2, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_ABSOLUTE:
        case MODE_INDIRECT:
            if (address + 2 <= maxAddress) {
                unsigned short addr = memory[address + 1] | (memory[address + 2] << 8);
                if (info.mode == MODE_INDIRECT) {
                    result.operand = QString("($%1)").arg(addr, 4, 16, QChar('0')).toUpper();
                } else {
                    result.operand = QString("$%1").arg(addr, 4, 16, QChar('0')).toUpper();
                }
            }
            break;
            
        case MODE_ABSOLUTE_X:
            if (address + 2 <= maxAddress) {
                unsigned short addr = memory[address + 1] | (memory[address + 2] << 8);
                result.operand = QString("$%1,X").arg(addr, 4, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_ABSOLUTE_Y:
            if (address + 2 <= maxAddress) {
                unsigned short addr = memory[address + 1] | (memory[address + 2] << 8);
                result.operand = QString("$%1,Y").arg(addr, 4, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_INDIRECT_X:
            if (address + 1 <= maxAddress) {
                unsigned char addr = memory[address + 1];
                result.operand = QString("($%1,X)").arg(addr, 2, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_INDIRECT_Y:
            if (address + 1 <= maxAddress) {
                unsigned char addr = memory[address + 1];
                result.operand = QString("($%1),Y").arg(addr, 2, 16, QChar('0')).toUpper();
            }
            break;
            
        case MODE_RELATIVE:
            if (address + 1 <= maxAddress) {
                signed char offset = (signed char)memory[address + 1];
                unsigned short targetAddr = address + 2 + offset;
                result.operand = QString("$%1").arg(targetAddr, 4, 16, QChar('0')).toUpper();
            }
            break;
    }
    
    // If we couldn't read enough bytes, mark as incomplete
    if (result.operand.isEmpty() && info.bytes > 1) {
        result.mnemonic = "???";
        result.bytes = 1;
    }
    
    return result;
}