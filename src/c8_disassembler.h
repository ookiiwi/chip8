#ifndef C8_DISASSEMBLER_H
#define C8_DISASSEMBLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "c8_helper.h"
#include "c8_decode.h"
#include <stdio.h>
#include <string.h>

// Don't forget to free returned string
#define C8_DISASSEMBLE_CORE(fmt, ...)   \
    char s[32];                         \
    snprintf(s, 32, fmt, __VA_ARGS__);  \
    *context = strdup(s);

#define FMT_HNNN(mnemonic, h)           C8_DISASSEMBLE_CORE("%s\tV%X, #%04X",   mnemonic, h,    C8_OPCODE_SELECT_NNN(opcode))
#define FMT_NNN_REG(mnemonic, reg)      C8_DISASSEMBLE_CORE("%s\t%s, #%04X",    mnemonic, reg,  C8_OPCODE_SELECT_NNN(opcode))
#define FMT_NNN(mnemonic)               C8_DISASSEMBLE_CORE("%s\t#%04X",        mnemonic,       C8_OPCODE_SELECT_NNN(opcode))
#define FMT_XNN(mnemonic)               C8_OPCODE_SELECT_XNN(opcode); C8_DISASSEMBLE_CORE("%s\tV%X, #%02X",     mnemonic, X, NN)
#define FMT_XYN(mnemonic)               C8_OPCODE_SELECT_XYN(opcode); C8_DISASSEMBLE_CORE("%s\tV%X, V%X, #%X",  mnemonic, X, Y, N)
#define FMT_XY(mnemonic)                C8_OPCODE_SELECT_XYN(opcode); C8_DISASSEMBLE_CORE("%s\tV%X, V%X",       mnemonic, X, Y)
#define FMT_XN(mnemonic)                C8_OPCODE_SELECT_XYN(opcode); C8_DISASSEMBLE_CORE("%s\tV%X, #%X",       mnemonic, X, N) 
#define FMT_X(mnemonic)                 C8_DISASSEMBLE_CORE("%s\tV%X",      mnemonic, C8_OPCODE_SELECT_X(opcode))
#define FMT_SX(mnemonic, s)             C8_DISASSEMBLE_CORE("%s\t%s, V%X",  mnemonic, s, C8_OPCODE_SELECT_X(opcode))
#define FMT_XS(mnemonic, s)             C8_DISASSEMBLE_CORE("%s\tV%X, %s",  mnemonic, C8_OPCODE_SELECT_X(opcode), s)

inline void c8_disassembleFX1E(char **context, WORD opcode) { FMT_SX("ADD", "I") }
inline void c8_disassemble7XNN(char **context, WORD opcode) { FMT_XNN("ADD") }
inline void c8_disassemble8XY4(char **context, WORD opcode) { FMT_XY("ADD") }
inline void c8_disassemble8XY2(char **context, WORD opcode) { FMT_XY("AND") }
inline void c8_disassemble2NNN(char **context, WORD opcode) { FMT_NNN("CALL") }
inline void c8_disassemble00E0(char **context, WORD opcode) { *context = strdup("CLS"); }
inline void c8_disassembleDXYN(char **context, WORD opcode) { FMT_XYN("DRW") }
inline void c8_disassemble1NNN(char **context, WORD opcode) { FMT_NNN("JP") }
inline void c8_disassembleBNNN(char **context, WORD opcode) { FMT_HNNN("JP", 0) }
inline void c8_disassembleFX33(char **context, WORD opcode) { FMT_SX("LD", "B") }
inline void c8_disassembleFX15(char **context, WORD opcode) { FMT_SX("LD", "DT") }
inline void c8_disassembleFX29(char **context, WORD opcode) { FMT_SX("LD", "F") }
inline void c8_disassembleANNN(char **context, WORD opcode) { FMT_NNN_REG("LD", "I") }
inline void c8_disassembleFX18(char **context, WORD opcode) { FMT_SX("LD", "ST") }
inline void c8_disassemble6XNN(char **context, WORD opcode) { FMT_XNN("LD") }
inline void c8_disassembleFX07(char **context, WORD opcode) { FMT_XS("LD", "DT") }
inline void c8_disassembleFX0A(char **context, WORD opcode) { FMT_XS("LD", "K") }
inline void c8_disassemble8XY0(char **context, WORD opcode) { FMT_XY("LD") }
inline void c8_disassembleFX65(char **context, WORD opcode) { FMT_XS("LD", "[I]") }
inline void c8_disassembleFX55(char **context, WORD opcode) { FMT_SX("LD", "[I]") }
inline void c8_disassemble8XY1(char **context, WORD opcode) { FMT_XY("OR") }
inline void c8_disassemble00EE(char **context, WORD opcode) { *context = strdup("RET"); }
inline void c8_disassembleCXNN(char **context, WORD opcode) { FMT_XNN("RND") }
inline void c8_disassemble3XNN(char **context, WORD opcode) { FMT_XNN("SE") }
inline void c8_disassemble5XY0(char **context, WORD opcode) { FMT_XY("SE") }
inline void c8_disassemble8XYE(char **context, WORD opcode) { FMT_X("SHL") }
inline void c8_disassemble8XY6(char **context, WORD opcode) { FMT_X("SHR") }
inline void c8_disassembleEX9E(char **context, WORD opcode) { FMT_X("SKP") }
inline void c8_disassembleEXA1(char **context, WORD opcode) { FMT_X("SKNP") }
inline void c8_disassemble4XNN(char **context, WORD opcode) { FMT_XNN("SNE") }
inline void c8_disassemble9XY0(char **context, WORD opcode) { FMT_XY("SNE") }
inline void c8_disassemble8XY5(char **context, WORD opcode) { FMT_XY("SUB") }
inline void c8_disassemble8XY7(char **context, WORD opcode) { FMT_XY("SUBN") }
inline void c8_disassemble8XY3(char **context, WORD opcode) { FMT_XY("XOR") }

inline C8_DECODE_FUNC_GEN(c8_dec, char **, c8_disassemble)

// Don't forget to free returned string if not NULL
inline char *c8_disassemble(WORD opcode) { 
    char *res = NULL;
    c8_dec(&res, opcode); 

    return res;
}

#ifdef __cplusplus
}
#endif

#endif