#ifndef CHIP8_H
#define CHIP8_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "c8_helper.h"

#define VF (BYTE)0xF
#define MEMORY_SIZE_IN_BYTES 0xFFF
#define USER_MEMORY_START 0x200
#define USER_MEMORY_END   0xE9F
#define USER_MEMORY_SIZE_IN_BYTES (USER_MEMORY_END-USER_MEMORY_START)
#define REGISTER_COUNT 16
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCREEN_BUFFER_SIZE_IN_BITS  ( SCREEN_WIDTH * SCREEN_HEIGHT)
#define SCREEN_BUFFER_SIZE_IN_BYTES ( SCREEN_BUFFER_SIZE_IN_BITS / 8 )

typedef void (*C8_PixelSetter)(int pixel, int x, int y, void **userData);
typedef void (*C8_PixelClearer)(void **userData);
typedef void (*C8_PixelRenderer)(void **userData);

typedef enum {
    C8_GOOD,
    C8_STACKOVERFLOW,
    C8_STACKUNDERFLOW,
    C8_REFUSED_MEM_ACCESS,
    C8_LOAD_CANNOT_OPEN_FILE,
    C8_LOAD_CANNOT_OPEN_CONFIG,
    C8_LOAD_MEMORY_BUFFER_TOO_LARGE,
    C8_DECODE_INVALID_OPCODE,
    C8_CLEAR_SCREEN
} c8_error;

typedef struct {
    c8_error err;
    char *msg;
} c8_error_t;

typedef struct {
    char     name[63];
    int      wrapY;
    float    frameRate;
} c8_config_t;

/**
 * @brief Chip8 context
*/
typedef struct {
    BYTE                 *memory;                                        // 4KiB memory
    BYTE                 *registers;                                     // 8-bit registers V0 to VF
    WORD                  sp;                                            // stack pointer. 12 levels of nesting (0xEA0-0xEFF)
    WORD                  addressI;                                      // only 12 lowers bits used
    WORD                  pc;                                            // program counter
    BYTE                 *screenBuffer;                                  // uint32 array in order to comply to SDL_Texture pixels array format
    BYTE                  delayTimer;                                    // Both timers count at 60hz until reaching 0
    BYTE                  soundTimer;
    int                   keyPressed;                                    // 0-F (-1 when depressed)
    c8_error_t            m_error;
    c8_config_t           config;
} chip8_t;

// font data
#define FONT_WIDTH 4
#define FONT_HEIGHT 5
static const BYTE font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10,
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80,
};

/* Error handling */
c8_error_t c8_get_error(chip8_t *context);
void       c8_set_error(chip8_t *context, c8_error_t error);
void       c8_clr_error(chip8_t *context);

/* Setup */
void c8_reset(chip8_t *context);
void c8_destroy(chip8_t *context);
int  c8_load_prgm(chip8_t *context, const char *path, const char *configPath);

/* Fetch-decode */
int  c8_tick(chip8_t *context);     // Return opcode on success
WORD c8_fetch(chip8_t *context);
void c8_decode(chip8_t *context, WORD opcode);

/* Memory access */
void write_memory(chip8_t *context, WORD address, BYTE data);
int  read_memory(chip8_t *context, WORD address);

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
void c8_opcode00E0(chip8_t *context, WORD opcode);    // clear screen
void c8_opcode00EE(chip8_t *context, WORD opcode);    // return
void c8_opcode1NNN(chip8_t *context, WORD opcode);    // Jump to address NNN
void c8_opcode2NNN(chip8_t *context, WORD opcode);    // Call subroutine at NNN

// Skip next instruction if (VX == NN)
#define c8_opcode3XNN(context, opcode)      _C8_SKIP_IF_CMP_XNN(=)
// Skip next instruction if (VX != NN)
#define c8_opcode4XNN(context, opcode)      _C8_SKIP_IF_CMP_XNN(!)
// Skip next instruction if (VX == VY)
#define c8_opcode5XY0(context, opcode)      _C8_SKIP_IF_CMP_XYN(=)

// Assign NN to VX
#define c8_opcode6XNN(context, opcode)      _C8_SET_VX_WITH_NN()
// Add NN to VX (carry flag unchanged)
#define c8_opcode7XNN(context, opcode)      _C8_SET_VX_WITH_NN(+)    
// Assign VY to VX
#define c8_opcode8XY0(context, opcode)      _C8_SET_VX_WITH_Y()
// Assign (VX OR VY) to VX  (bitwise or)
#define c8_opcode8XY1(context, opcode)      _C8_SET_VX_WITH_Y(|)
// Assign (VX AND VY) to VX (bitwise and)
#define c8_opcode8XY2(context, opcode)      _C8_SET_VX_WITH_Y(&)
// Assign (VX XOR VY) to VX
#define c8_opcode8XY3(context, opcode)      _C8_SET_VX_WITH_Y(^)

// Substract VY to VX. VF set if no borrow
#define c8_opcode8XY5(context, opcode)      _C8_SUB_BORROW(X, Y)
// Assign (VY - VX) to VX. VF is no borrow flag
#define c8_opcode8XY7(context, opcode)      _C8_SUB_BORROW(Y, X)

void c8_opcode8XY4(chip8_t *context, WORD opcode);    // Add VY to VX. VF set if carry
void c8_opcode8XY6(chip8_t *context, WORD opcode);    // Store LSB in VF and right shift VX by 1
void c8_opcode8XYE(chip8_t *context, WORD opcode);    // Store MSB in VF and left shift VX by 1

// Skip next instruction if (VX != VY)
#define c8_opcode9XY0(context, opcode)   _C8_SKIP_IF_CMP_XYN(!)

void c8_opcodeANNN(chip8_t *context, WORD opcode);    // Assign address NNN to I
void c8_opcodeBNNN(chip8_t *context, WORD opcode);    // Jump to address NNN + V0
void c8_opcodeCXNN(chip8_t *context, WORD opcode);    // Assign (rand number AND NN) to VX
void c8_opcodeDXYN(chip8_t *context, WORD opcode);    // Draw sprite of height N pixels at coord (VX, VY). Read sprite data from memory at address in I.

// Skip next instruction if key in VX pressed
#define c8_opcodeEX9E(context, opcode)  _C8_SKIP_IF_CMP_TO_X(NN, =, context->keyPressed)    // NN necessary but dummy 
// Skip next instruction if key in VX is not pressed
#define c8_opcodeEXA1(context, opcode)  _C8_SKIP_IF_CMP_TO_X(NN, !, context->keyPressed)

void c8_opcodeFX07(chip8_t *context, WORD opcode);    // Assign delay timer's value to VX
void c8_opcodeFX0A(chip8_t *context, WORD opcode);    // Wait for any key press and store it in VX (blocking)

// Set VX to delay timer
#define c8_opcodeFX15(context, opcode)  _ASSIGN(NN, context->delayTimer, context->registers[X], )
// Assign sound timer's value to VX
#define c8_opcodeFX18(context, opcode)  _ASSIGN(NN, context->soundTimer, context->registers[X], )
// Add VX to I. VF unchanged
#define c8_opcodeFX1E(context, opcode)  _ASSIGN(NN, context->addressI,   context->registers[X], +)
// Set I to sprite address for the characheter in VX
#define c8_opcodeFX29(context, opcode)  _ASSIGN(NN, context->addressI,   context->registers[X]*FONT_HEIGHT, )

void c8_opcodeFX33(chip8_t *context, WORD opcode);    // Store BCD representation of VX (hundreds at I, tens at I+1 and ones at I+2)
void c8_opcodeFX55(chip8_t *context, WORD opcode);    // Dump V0 to VX (included) in memory, starting at address I (I unchanged)
void c8_opcodeFX65(chip8_t *context, WORD opcode);    // Load memory content in V0 to VX (included), starting at address I (I unchanged)

#ifdef __cplusplus
}
#endif

#endif
