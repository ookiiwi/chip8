#ifndef C8_DECODE_H
#define C8_DECODE_H

#define C8_DECODE_FUNC_GEN(funcName, contextType, opFunPrefix)                          \
    int funcName(contextType context, WORD opcode) {                                    \
        switch(C8_OPCODE_SELECT_OP(opcode)) {                                           \
            case 0x0:                                                                   \
                switch(opcode) {                                                        \
                    case 0x00E0: opFunPrefix##00E0(context, opcode); break;             \
                    case 0x00EE: opFunPrefix##00EE(context, opcode); break;             \
                    default: return opcode;                                             \
                }                                                                       \
                break;                                                                  \
            case 0x1: opFunPrefix##1NNN(context, opcode); break;                        \
            case 0x2: opFunPrefix##2NNN(context, opcode); break;                        \
            case 0x3: opFunPrefix##3XNN(context, opcode); break;                        \
            case 0x4: opFunPrefix##4XNN(context, opcode); break;                        \
            case 0x5: opFunPrefix##5XY0(context, opcode); break;                        \
            case 0x6: opFunPrefix##6XNN(context, opcode); break;                        \
            case 0x7: opFunPrefix##7XNN(context, opcode); break;                        \
            case 0x8:                                                                   \
                switch(C8_OPCODE_SELECT_N(opcode)) {                                    \
                    case 0x0: opFunPrefix##8XY0(context, opcode); break;                \
                    case 0x1: opFunPrefix##8XY1(context, opcode); break;                \
                    case 0x2: opFunPrefix##8XY2(context, opcode); break;                \
                    case 0x3: opFunPrefix##8XY3(context, opcode); break;                \
                    case 0x4: opFunPrefix##8XY4(context, opcode); break;                \
                    case 0x5: opFunPrefix##8XY5(context, opcode); break;                \
                    case 0x6: opFunPrefix##8XY6(context, opcode); break;                \
                    case 0x7: opFunPrefix##8XY7(context, opcode); break;                \
                    case 0xE: opFunPrefix##8XYE(context, opcode); break;                \
                    default: return opcode;                                             \
                }                                                                       \
                break;                                                                  \
            case 0x9: opFunPrefix##9XY0(context, opcode); break;                        \
            case 0xA: opFunPrefix##ANNN(context, opcode); break;                        \
            case 0xB: opFunPrefix##BNNN(context, opcode); break;                        \
            case 0xC: opFunPrefix##CXNN(context, opcode); break;                        \
            case 0xD: opFunPrefix##DXYN(context, opcode); break;                        \
            case 0xE:                                                                   \
                switch(C8_OPCODE_SELECT_NN(opcode)) {                                   \
                    case 0x9E: opFunPrefix##EX9E(context, opcode); break;               \
                    case 0xA1: opFunPrefix##EXA1(context, opcode); break;               \
                    default: return opcode;                                             \
                }                                                                       \
                break;                                                                  \
            case 0xF:                                                                   \
                switch(C8_OPCODE_SELECT_NN(opcode)) {                                   \
                    case 0x07: opFunPrefix##FX07(context, opcode); break;               \
                    case 0x0A: opFunPrefix##FX0A(context, opcode); break;               \
                    case 0x15: opFunPrefix##FX15(context, opcode); break;               \
                    case 0x18: opFunPrefix##FX18(context, opcode); break;               \
                    case 0x1E: opFunPrefix##FX1E(context, opcode); break;               \
                    case 0x29: opFunPrefix##FX29(context, opcode); break;               \
                    case 0x33: opFunPrefix##FX33(context, opcode); break;               \
                    case 0x55: opFunPrefix##FX55(context, opcode); break;               \
                    case 0x65: opFunPrefix##FX65(context, opcode); break;               \
                    default: return opcode;                                             \
                }                                                                       \
                break;                                                                  \
            default: return opcode;                                                     \
        }                                                                               \
        return 0;                                                                       \
    }

#endif