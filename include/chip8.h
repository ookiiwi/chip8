#ifndef CHIP8_H
#define CHIP8_H

#include <stddef.h>
#include <stdio.h>

#define VF (BYTE)0xF
#define MEMORY_SIZE_IN_BYTES 0xFFF
#define USER_MEMORY_START 0x200
#define USER_MEMORY_END   0xE9F
#define USER_MEMORY_SIZE_IN_BYTES (USER_MEMORY_END-USER_MEMORY_START)
#define REGISTER_COUNT 16
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCREEN_BUFFER_SIZE_IN_BITS  ( SCREEN_WIDTH * SCREEN_HEIGHT )
#define SCREEN_BUFFER_SIZE_IN_BYTES ( SCREEN_BUFFER_SIZE_IN_BITS / 8 )

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef void (*C8_PixelRenderer)(int pixel, int x, int y, void **userData);

typedef enum {
    C8_GOOD,
    C8_STACKOVERFLOW,
    C8_STACKUNDERFLOW,
    C8_REFUSED_MEM_ACCESS,
    C8_LOAD_MEMORY_BUFFER_TOO_LARGE,
    C8_DECODE_INVALID_OPCODE,
    C8_CLEAR_SCREEN
} c8_error;

typedef struct {
    c8_error err;
    char *msg;
} c8_error_t;

/**
 * @brief Chip8 context
*/
struct chip8 {
    BYTE            *memory;               // 4KiB memory
    BYTE            *registers;            // 8-bit registers V0 to VF
    WORD             sp;                   // stack pointer. 12 levels of nesting (0xEA0-0xEFF)
    WORD             addressI;             // only 12 lowers bits used
    WORD             pc;                   // program counter
    BYTE            *screenBuffer;
    BYTE             delayTimer;           // Both timers count at 60hz until reaching 0
    BYTE             soundTimer;
    int              keyPressed;           // 0-F (-1 when depressed)
    c8_error_t       m_error;

    // functions
    C8_PixelRenderer m_renderer;
    void **m_renUserData;
};

/* Error handling */
c8_error_t c8_get_error(struct chip8 *context);
void       c8_set_error(struct chip8 *context, c8_error_t error);

/* Setup */
void c8_reset(struct chip8 *context, C8_PixelRenderer renderer, void **rendererUserData);
void c8_destroy(struct chip8 *context);
void c8_load_prgm(struct chip8 *context, FILE *fp);

/* Fetch-decode */
WORD c8_fetch(struct chip8 *context);
void c8_decode(struct chip8 *context, WORD opcode);

/* Memory access */
void write_memory(struct chip8 *context, WORD address, BYTE data);
int  read_memory(struct chip8 *context, WORD address);

/* Helper macros */
#define C8_OPCODE_SELECT_OP(opcode)    ((opcode & 0xF000) >> 12)
#define C8_OPCODE_SELECT_NNN(opcode)   (opcode & 0x0FFF)
#define C8_OPCODE_SELECT_NN(opcode)    (opcode & 0x00FF)
#define C8_OPCODE_SELECT_X(opcode)     ((opcode & 0x0F00) >> 8)
#define C8_OPCODE_SELECT_Y(opcode)     ((opcode & 0x00F0) >> 4)
#define C8_OPCODE_SELECT_N(opcode)     (opcode & 0x000F)

#define C8_OPCODE_SELECT_IDS_2(opcode, id1, id2)                                                \
    BYTE id1, id2;                                                                              \
    id1 = C8_OPCODE_SELECT_##id1(opcode);                                                       \
    id2 = C8_OPCODE_SELECT_##id2(opcode);

#define C8_OPCODE_SELECT_IDS_3(opcode, id1, id2, id3)                                           \
    BYTE id3;                                                                                   \
    C8_OPCODE_SELECT_IDS_2(opcode, id1, id2);                                                   \
    id3 = C8_OPCODE_SELECT_##id3(opcode);

#define C8_OPCODE_SELECT_IDS_X(opcode, id1, id2, id3, FUNC, ...) FUNC
#define C8_OPCODE_SELECT_IDS_MACRO_CHOOSER(...)                                                 \
    C8_OPCODE_SELECT_IDS_X(__VA_ARGS__, C8_OPCODE_SELECT_IDS_3, C8_OPCODE_SELECT_IDS_2, )
#define C8_OPCODE_SELECT_IDS(...) C8_OPCODE_SELECT_IDS_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define C8_OPCODE_SELECT_XNN(opcode) C8_OPCODE_SELECT_IDS(opcode, X, NN)
#define C8_OPCODE_SELECT_XYN(opcode)  C8_OPCODE_SELECT_IDS(opcode, X, Y, N)

#define _C8_SKIP_IF_CMP_TO_X(field, eqPrefix, rhs) do {                                         \
    C8_OPCODE_SELECT_X##field(opcode);                                                          \
    if ((context)->registers[X] eqPrefix##= rhs) {                                              \
        (context)->pc += 2;                                                                     \
    }                                                                                           \
} while(0)

#define _C8_SKIP_IF_CMP_XNN(eqPrefix) _C8_SKIP_IF_CMP_TO_X(NN, eqPrefix, NN)
#define _C8_SKIP_IF_CMP_XYN(eqPrefix) _C8_SKIP_IF_CMP_TO_X(YN, eqPrefix, (context)->registers[Y])

#define _C8_SET_VX(field, rhs, assignPrefix) do {                                               \
    C8_OPCODE_SELECT_X##field(opcode);                                                          \
    (context)->registers[X] assignPrefix##= rhs;                                                \
} while(0)

#define _C8_SET_VX_WITH_NN(assignPrefix) _C8_SET_VX(NN, NN, assignPrefix)
#define _C8_SET_VX_WITH_Y(assignPrefix)  _C8_SET_VX(YN, (context)->registers[Y], assignPrefix)

#define _C8_SUB_BORROW(regNum1, regNum2) do {                                                   \
    int res, noBorrow, a, b;                                                                    \
    C8_OPCODE_SELECT_XYN(opcode);                                                               \
    a = (context)->registers[regNum1];                                                          \
    b = (context)->registers[regNum2];                                                          \
    res = a - b;                                                                                \
    noBorrow = (res >= 0);                                                                      \
    (context)->registers[X]  = noBorrow ? res : -res; /* implicitly truncate to 8 bits */       \
    (context)->registers[VF] = noBorrow;              /* no borrow flag */                      \
} while(0)

// Select X<field> from opcode and process <lhs> <pref>= <rhs>
#define _ASSIGN(field, lhs, rhs, assignPrefix) do {                                             \
    C8_OPCODE_SELECT_X##field(opcode);                                                          \
    lhs assignPrefix##= rhs;                                                                    \
} while(0)


/* Instructions */
void c8_opcode00E0(struct chip8 *context, WORD opcode);    // clear screen
void c8_opcode00EE(struct chip8 *context, WORD opcode);    // return
void c8_opcode1NNN(struct chip8 *context, WORD opcode);    // Jump to address NNN
void c8_opcode2NNN(struct chip8 *context, WORD opcode);    // Call subroutine at NNN

// Skip next instruction if (VX == NN)
#define c8_opcode3XNN(context, opcode)      _C8_SKIP_IF_CMP_XNN(=);    
// Skip next instruction if (VX != NN)
#define c8_opcode4XNN(context, opcode)      _C8_SKIP_IF_CMP_XNN(!);    
// Skip next instruction if (VX == VY)
#define c8_opcode5XY0(context, opcode)      _C8_SKIP_IF_CMP_XYN(=);    

// Assign NN to VX
#define c8_opcode6XNN(context, opcode)      _C8_SET_VX_WITH_NN();
// Add NN to VX (carry flag unchanged)
#define c8_opcode7XNN(context, opcode)      _C8_SET_VX_WITH_NN(+);    
// Assign VY to VX
#define c8_opcode8XY0(context, opcode)      _C8_SET_VX_WITH_Y();    
// Assign (VX OR VY) to VX  (bitwise or)
#define c8_opcode8XY1(context, opcode)      _C8_SET_VX_WITH_Y(|);    
// Assign (VX AND VY) to VX (bitwise and)
#define c8_opcode8XY2(context, opcode)      _C8_SET_VX_WITH_Y(&);    
// Assign (VX XOR VY) to VX
#define c8_opcode8XY3(context, opcode)      _C8_SET_VX_WITH_Y(^);    

// Substract VY to VX. VF set if no borrow
#define c8_opcode8XY5(context, opcode)      _C8_SUB_BORROW(X, Y);    
// Assign (VY - VX) to VX. VF is no borrow flag
#define c8_opcode8XY7(context, opcode)      _C8_SUB_BORROW(Y, X);    

void c8_opcode8XY4(struct chip8 *context, WORD opcode);    // Add VY to VX. VF set if carry
void c8_opcode8XY6(struct chip8 *context, WORD opcode);    // Store LSB in VF and right shift VX by 1
void c8_opcode8XYE(struct chip8 *context, WORD opcode);    // Store MSB in VF and left shift VX by 1

// Skip next instruction if (VX != VY)
#define c8_opcode9XY0(context, opcode)   _C8_SKIP_IF_CMP_XYN(!);    

void c8_opcodeANNN(struct chip8 *context, WORD opcode);    // Assign address NNN to I
void c8_opcodeBNNN(struct chip8 *context, WORD opcode);    // Jump to address NNN + V0
void c8_opcodeCXNN(struct chip8 *context, WORD opcode);    // Assign (rand number AND NN) to VX
void c8_opcodeDXYN(struct chip8 *context, WORD opcode);    // Draw sprite of height N pixels at coord (VX, VY). Read sprite data from memory at address in I.

// Skip next instruction if key in VX pressed
#define c8_opcodeEX9E(context, opcode)  _C8_SKIP_IF_CMP_TO_X(NN, =, context->keyPressed);   // NN necessary but dummy 
// Skip next instruction if key in VX is not pressed
#define c8_opcodeEXA1(context, opcode)  _C8_SKIP_IF_CMP_TO_X(NN, !, context->keyPressed);    

void c8_opcodeFX07(struct chip8 *context, WORD opcode);    // Assign delay timer's value to VX
void c8_opcodeFX0A(struct chip8 *context, WORD opcode);    // Wait for any key press and store it in VX (blocking)

// Set VX to delay timer
#define c8_opcodeFX15(context, opcode)  _ASSIGN(NN, context->delayTimer, context->registers[X], );    
// Assign sound timer's value to VX
#define c8_opcodeFX18(context, opcode)  _ASSIGN(NN, context->soundTimer, context->registers[X], );    
// Add VX to I. VF unchanged
#define c8_opcodeFX1E(context, opcode)  _ASSIGN(NN, context->addressI,   context->registers[X], +);   
// Set I to sprite address for the characheter in VX
#define c8_opcodeFX29(context, opcode)  _ASSIGN(NN, context->addressI,   context->registers[X], );    

void c8_opcodeFX33(struct chip8 *context, WORD opcode);    // Store BCD representation of VX (hundreds at I, tens at I+1 and ones at I+2)
void c8_opcodeFX55(struct chip8 *context, WORD opcode);    // Dump V0 to VX (included) in memory, starting at address I (I unchanged)
void c8_opcodeFX65(struct chip8 *context, WORD opcode);    // Load memory content in V0 to VX (included), starting at address I (I unchanged)

#endif
