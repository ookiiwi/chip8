#ifndef C8_HELPER_H
#define C8_HELPER_H

typedef unsigned char BYTE;
typedef unsigned short WORD;

#define C8_OPCODE_SELECT_OP(opcode)    ((opcode & 0xF000) >> 12)
#define C8_OPCODE_SELECT_NNN(opcode)   (opcode & 0x0FFF)
#define C8_OPCODE_SELECT_NN(opcode)    (opcode & 0x00FF)
#define C8_OPCODE_SELECT_X(opcode)     ((opcode & 0x0F00) >> 8)
#define C8_OPCODE_SELECT_Y(opcode)     ((opcode & 0x00F0) >> 4)
#define C8_OPCODE_SELECT_N(opcode)     (opcode & 0x000F)

#define C8_OPCODE_SELECT_IDS_2(opcode, id1, id2)                                                \
    BYTE id1, id2;                                                                              \
    id1 = C8_OPCODE_SELECT_##id1(opcode);                                                       \
    id2 = C8_OPCODE_SELECT_##id2(opcode)

#define C8_OPCODE_SELECT_IDS_3(opcode, id1, id2, id3)                                           \
    BYTE id3;                                                                                   \
    C8_OPCODE_SELECT_IDS_2(opcode, id1, id2);                                                   \
    id3 = C8_OPCODE_SELECT_##id3(opcode)

#define C8_OPCODE_SELECT_IDS_X(opcode, id1, id2, id3, FUNC, ...) FUNC
#define C8_OPCODE_SELECT_IDS_MACRO_CHOOSER(...)                                                 \
    C8_OPCODE_SELECT_IDS_X(__VA_ARGS__, C8_OPCODE_SELECT_IDS_3, C8_OPCODE_SELECT_IDS_2, )
#define C8_OPCODE_SELECT_IDS(...) C8_OPCODE_SELECT_IDS_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define C8_OPCODE_SELECT_XNN(opcode)  C8_OPCODE_SELECT_IDS(opcode, X, NN)
#define C8_OPCODE_SELECT_XYN(opcode)  C8_OPCODE_SELECT_IDS(opcode, X, Y, N)

#endif