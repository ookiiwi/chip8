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

#define MEMORY_SIZE_IN_BYTES 0xFFF
#define USER_MEMORY_START 0x200
#define USER_MEMORY_END   0xE9F
#define USER_MEMORY_SIZE_IN_BYTES (USER_MEMORY_END-USER_MEMORY_START)
#define REGISTER_COUNT 16
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCREEN_BUFFER_SIZE_IN_BITS  ( SCREEN_WIDTH * SCREEN_HEIGHT)
#define SCREEN_BUFFER_SIZE_IN_BYTES ( SCREEN_BUFFER_SIZE_IN_BITS / 8 )

#define DEFAULT_CLOCKSPEED 800.0
#define DEFAULT_FPS 60.0
#define DEFAULT_WRAPY 1

typedef struct _C8_Context C8_Context;
typedef void (*C8_KeyChangeNotifier)(C8_Context*, int);
typedef void (*C8_BeepCallback)(void *);

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
} C8_ErrorEnum;

typedef struct {
    C8_ErrorEnum err;
    char *msg;
} C8_Error;

typedef struct {
    char     name[63];
    int      wrapY;
    float    clockspeed;
    float    fps;
} C8_Config;

typedef struct {
    C8_BeepCallback beep;
    void            *user_data;
} C8_Beeper;

/**
 * @brief Chip8 context
*/
struct _C8_Context {
    BYTE                 *memory;                                        // 4KiB memory
    BYTE                 *registers;                                     // 8-bit registers V0 to VF
    WORD                  sp;                                            // stack pointer. 12 levels of nesting (0xEA0-0xEFF)
    WORD                  addressI;                                      // only 12 lowers bits used
    WORD                  pc;                                            // program counter
    BYTE                 *display;                                       // uint32 array in order to comply to SDL_Texture pixels array format
    BYTE                  delay_timer;                                   // Both timers count at 60hz until reaching 0
    BYTE                  sound_timer;
    C8_Error              m_error;
    C8_Config             config;
    int                   m_keys[16];
    C8_KeyChangeNotifier  m_on_set_key;
    int                   is_running;
    WORD                  last_opcode;
    C8_Beeper            *beeper;
};

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

/* Macro shortcuts to access registers */
#define VX (context->registers[X])
#define VY (context->registers[Y])
#define VF (context->registers[0xF])

/* Error handling */
C8_Error C8_GetError(C8_Context *context);
void     C8_SetError(C8_Context *context, C8_Error error);
void     C8_ClearError(C8_Context *context);

/* Setup */
void C8_Reset(C8_Context *context, C8_Beeper *beeper);
void C8_Destroy(C8_Context *context);
int  C8_LoadProgram(C8_Context *context, const char *path, const char *config_path);

/* Fetch-decode */
int  C8_Tick(C8_Context *context);                 // Return opcode on success
void C8_UpdateTimers(C8_Context *context);
WORD C8_Fetch(C8_Context *context);
void C8_Decode(C8_Context *context, WORD opcode);

/* Key handling */
void C8_SetKey(C8_Context *context, int key);
void C8_UnsetKey(C8_Context *context, int key);

#define INCREMENENT_PC  context->pc += 2
#define DECREMENENT_PC  context->pc -= 2

#define _C8_SKIP_IF_X(field, cond) do {                                                         \
    C8_OPCODE_SELECT_X##field(opcode);                                                          \
    if ( cond ) { INCREMENENT_PC; }                                                             \
} while(0);

#define _C8_SKIP_IF_CMP_TO_X(field, eqPrefix, rhs) _C8_SKIP_IF_X(field, (VX eqPrefix##= rhs))
#define _C8_SKIP_IF_CMP_XNN(eqPrefix) _C8_SKIP_IF_CMP_TO_X(NN, eqPrefix, NN)
#define _C8_SKIP_IF_CMP_XYN(eqPrefix) _C8_SKIP_IF_CMP_TO_X(YN, eqPrefix, VY)

#define _C8_SET_VX(field, rhs, assignPrefix) do {                                               \
    C8_OPCODE_SELECT_X##field(opcode);                                                          \
    VX assignPrefix##= rhs;                                                                     \
} while(0)

#define _C8_SET_VX_WITH_NN(assign_prefix) _C8_SET_VX(NN, NN, assign_prefix)
#define _C8_SET_VX_WITH_Y(assign_prefix)  _C8_SET_VX(YN, VY, assign_prefix)

#define _C8_SUB_BORROW(reg_num1, reg_num2) do {                                                 \
    int res, no_borrow, a, b;                                                                   \
    C8_OPCODE_SELECT_XYN(opcode);                                                               \
    a = (context)->registers[reg_num1];                                                         \
    b = (context)->registers[reg_num2];                                                         \
    res = a - b;                                                                                \
    no_borrow = (res >= 0);                                                                     \
    VX  = no_borrow ? res : -res; /* implicitly truncate to 8 bits */                           \
    VF  = no_borrow;              /* no borrow flag */                                          \
} while(0)

// Select X<field> from opcode and process <lhs> <pref>= <rhs>
#define _ASSIGN(field, lhs, rhs, assign_prefix) do {                                            \
    C8_OPCODE_SELECT_X##field(opcode);                                                          \
    lhs assign_prefix##= rhs;                                                                   \
} while(0)


/* Instructions */
void C8_Opcode00E0(C8_Context *context, WORD opcode);    // clear screen
void C8_Opcode00EE(C8_Context *context, WORD opcode);    // return
void C8_Opcode1NNN(C8_Context *context, WORD opcode);    // Jump to address NNN
void C8_Opcode2NNN(C8_Context *context, WORD opcode);    // Call subroutine at NNN

// Skip next instruction if (VX == NN)
#define C8_Opcode3XNN(context, opcode)      _C8_SKIP_IF_CMP_XNN(=)
// Skip next instruction if (VX != NN)
#define C8_Opcode4XNN(context, opcode)      _C8_SKIP_IF_CMP_XNN(!)
// Skip next instruction if (VX == VY)
#define C8_Opcode5XY0(context, opcode)      _C8_SKIP_IF_CMP_XYN(=)

// Assign NN to VX
#define C8_Opcode6XNN(context, opcode)      _C8_SET_VX_WITH_NN()
// Add NN to VX (carry flag unchanged)
#define C8_Opcode7XNN(context, opcode)      _C8_SET_VX_WITH_NN(+)    
// Assign VY to VX
#define C8_Opcode8XY0(context, opcode)      _C8_SET_VX_WITH_Y()
// Assign (VX OR VY) to VX  (bitwise or)
#define C8_Opcode8XY1(context, opcode)      _C8_SET_VX_WITH_Y(|)
// Assign (VX AND VY) to VX (bitwise and)
#define C8_Opcode8XY2(context, opcode)      _C8_SET_VX_WITH_Y(&)
// Assign (VX XOR VY) to VX
#define C8_Opcode8XY3(context, opcode)      _C8_SET_VX_WITH_Y(^)

// Substract VY to VX. VF set if no borrow
#define C8_Opcode8XY5(context, opcode)      _C8_SUB_BORROW(X, Y)
// Assign (VY - VX) to VX. VF is no borrow flag
#define C8_Opcode8XY7(context, opcode)      _C8_SUB_BORROW(Y, X)

void C8_Opcode8XY4(C8_Context *context, WORD opcode);    // Add VY to VX. VF set if carry
void C8_Opcode8XY6(C8_Context *context, WORD opcode);    // Store LSB in VF and right shift VX by 1
void C8_Opcode8XYE(C8_Context *context, WORD opcode);    // Store MSB in VF and left shift VX by 1

// Skip next instruction if (VX != VY)
#define C8_Opcode9XY0(context, opcode)   _C8_SKIP_IF_CMP_XYN(!)

void C8_OpcodeANNN(C8_Context *context, WORD opcode);    // Assign address NNN to I
void C8_OpcodeBNNN(C8_Context *context, WORD opcode);    // Jump to address NNN + V0
void C8_OpcodeCXNN(C8_Context *context, WORD opcode);    // Assign (rand number AND NN) to VX
void C8_OpcodeDXYN(C8_Context *context, WORD opcode);    // Draw sprite of height N pixels at coord (VX, VY). Read sprite data from memory at address in I.

// Skip next instruction if key in VX pressed
#define C8_OpcodeEX9E(context, opcode)  _C8_SKIP_IF_X(NN, context->m_keys[VX] != 0)    // NN necessary but dummy 
// Skip next instruction if key in VX is not pressed
#define C8_OpcodeEXA1(context, opcode)  _C8_SKIP_IF_X(NN, context->m_keys[VX] == 0)

void C8_OpcodeFX07(C8_Context *context, WORD opcode);    // Assign delay timer's value to VX
void C8_OpcodeFX0A(C8_Context *context, WORD opcode);    // Wait for any key press and store it in VX (blocking)

// Set VX to delay timer
#define C8_OpcodeFX15(context, opcode)  _ASSIGN(NN, context->delay_timer, context->registers[X], )
// Assign sound timer's value to VX
void C8_OpcodeFX18(C8_Context *context, WORD opcode);
// Add VX to I. VF unchanged
#define C8_OpcodeFX1E(context, opcode)  _ASSIGN(NN, context->addressI,   context->registers[X], +)
// Set I to sprite address for the characheter in VX
#define C8_OpcodeFX29(context, opcode)  _ASSIGN(NN, context->addressI,   context->registers[X]*FONT_HEIGHT, )

void C8_OpcodeFX33(C8_Context *context, WORD opcode);    // Store BCD representation of VX (hundreds at I, tens at I+1 and ones at I+2)
void C8_OpcodeFX55(C8_Context *context, WORD opcode);    // Dump V0 to VX (included) in memory, starting at address I (I unchanged)
void C8_OpcodeFX65(C8_Context *context, WORD opcode);    // Load memory content in V0 to VX (included), starting at address I (I unchanged)

#ifdef __cplusplus
}
#endif

#endif
