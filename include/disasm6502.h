/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef DISASM6502_H
#define DISASM6502_H

#include <QString>

struct DisassembledInstruction {
    QString mnemonic;      // Instruction mnemonic (e.g., "LDA")
    QString operand;       // Operand string (e.g., "#$FF", "$1234,X")
    int bytes;             // Total instruction length
    unsigned char opcode;  // The opcode byte
};

// Disassemble a single 6502 instruction at the given address
// memory: pointer to memory array
// address: current address to disassemble
// maxAddress: maximum valid address (usually 65535)
DisassembledInstruction disassemble6502(const unsigned char* memory, unsigned short address, unsigned short maxAddress = 65535);

#endif // DISASM6502_H