#include "chip8.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <stdio.h>

#define SET_ERROR(error) c8_set_error(context, error)
#define GET_ERROR        c8_get_error(context)
#define RETURN_ON_ERROR  if (GET_ERROR != C8_GOOD) return

/* Error handling */
c8_error c8_get_error(struct chip8 *context) { return context->m_error; }
void     c8_set_error(struct chip8 *context, c8_error error) { context->m_error = error; }

/* Setup */

void c8_reset(struct chip8 *context) {
    context->memory         = (BYTE*)calloc(MEMORY_SIZE_IN_BYTES, sizeof(BYTE));
    context->registers      = (BYTE*)calloc(REGISTER_COUNT, sizeof(BYTE));
    context->screenBuffer   = (BYTE*)calloc(SCREEN_BUFFER_SIZE_IN_BYTES, sizeof(BYTE));
    context->sp             = USER_MEMORY_END + 1;
    context->addressI       = 0;
    context->pc             = USER_MEMORY_START;
    context->addressI       = USER_MEMORY_START;
    context->delayTimer     = 0;
    context->soundTimer     = 0;
    context->m_error        = C8_GOOD;
}

void c8_destroy(struct chip8 *context) {
    free(context->memory);
    free(context->registers);
    free(context->screenBuffer);
}

void c8_load_memory(struct chip8 *context, BYTE *buffer, size_t bufferSize) {
    if (bufferSize > USER_MEMORY_SIZE_IN_BYTES) {
        SET_ERROR(C8_LOAD_MEMORY_BUFFER_TOO_LARGE);
        return;
    }

    memset((void*)(context->memory + USER_MEMORY_START), 0, USER_MEMORY_SIZE_IN_BYTES);
    memcpy((void*)(context->memory + USER_MEMORY_START), (void*)buffer, bufferSize);
}

/* Fetch-decode */

WORD c8_fetch(struct chip8 *context) {
    WORD res;

    res  = (context->memory[context->pc++] << 8);
    res |=  context->memory[context->pc++];

    return res;
}

void c8_decode(struct chip8 *context, WORD opcode) {
    switch(C8_OPCODE_SELECT_OP(opcode)) {
        case 0x0:
            switch(opcode) {
                case 0x00E0: c8_opcode00E0(context, opcode); break;
                case 0x00EE: c8_opcode00EE(context, opcode); break;
                default: SET_ERROR(C8_DECODE_INVALID_OPCODE); return;
            } 

            break;
        case 0x1: c8_opcode1NNN(context, opcode); break;
        case 0x2: c8_opcode2NNN(context, opcode); break;
        case 0x3: c8_opcode3XNN(context, opcode); break;
        case 0x4: c8_opcode4XNN(context, opcode); break;
        case 0x5: c8_opcode5XY0(context, opcode); break;
        case 0x6: c8_opcode6XNN(context, opcode); break;
        case 0x7: c8_opcode7XNN(context, opcode); break;
        case 0x8:
            switch(C8_OPCODE_SELECT_N(opcode)) {
                case 0x0: c8_opcode8XY0(context, opcode); break;
                case 0x1: c8_opcode8XY1(context, opcode); break;
                case 0x2: c8_opcode8XY2(context, opcode); break;
                case 0x3: c8_opcode8XY3(context, opcode); break;
                case 0x4: c8_opcode8XY4(context, opcode); break;
                case 0x5: c8_opcode8XY5(context, opcode); break;
                case 0x6: c8_opcode8XY6(context, opcode); break;
                case 0x7: c8_opcode8XY7(context, opcode); break;
                case 0xE: c8_opcode8XYE(context, opcode); break;
                default: SET_ERROR(C8_DECODE_INVALID_OPCODE); return;
            }

            break;
        case 0x9: c8_opcode9XY0(context, opcode); break;
        case 0xA: c8_opcodeANNN(context, opcode); break;
        case 0xB: c8_opcodeBNNN(context, opcode); break;
        case 0xC: c8_opcodeCXNN(context, opcode); break;
        case 0xD: c8_opcodeDXYN(context, opcode); break;
        case 0xE: 
            switch(C8_OPCODE_SELECT_NN(opcode)) {
                case 0x9E: c8_opcodeEX9E(context, opcode); break;
                case 0xA1: c8_opcodeEXA1(context, opcode); break;
                default: SET_ERROR(C8_DECODE_INVALID_OPCODE); return;
            }

            break;
        case 0xF: 
            switch(C8_OPCODE_SELECT_NN(opcode)) {
                case 0x07: c8_opcodeFX07(context, opcode); break;
                case 0x0A: c8_opcodeFX0A(context, opcode); break;
                case 0x15: c8_opcodeFX15(context, opcode); break;
                case 0x18: c8_opcodeFX18(context, opcode); break;
                case 0x1E: c8_opcodeFX1E(context, opcode); break;
                case 0x29: c8_opcodeFX29(context, opcode); break;
                case 0x33: c8_opcodeFX33(context, opcode); break;
                case 0x55: c8_opcodeFX55(context, opcode); break;
                case 0x65: c8_opcodeFX55(context, opcode); break;
                default: SET_ERROR(C8_DECODE_INVALID_OPCODE); return;
            }

            break;
        default: SET_ERROR(C8_DECODE_INVALID_OPCODE); return;
    }
}

/* Memory access */
#define CHECK_AUTHORIZED_MEM_ACCESS(address, res) \
    if (address < USER_MEMORY_START || address > USER_MEMORY_END) { SET_ERROR(C8_REFUSED_MEM_ACCESS); return res; }

void write_memory(struct chip8 *context, WORD address, BYTE data) {
    CHECK_AUTHORIZED_MEM_ACCESS(address,)

    context->memory[address] = data;
}

int read_memory(struct chip8 *context, WORD address) {
    CHECK_AUTHORIZED_MEM_ACCESS(address, 0)
    
    return context->memory[address];
}

/* Instructions */

void c8_opcode00E0(struct chip8 *context, WORD opcode) {
    void* rv = memset((void*)context->screenBuffer, 0, SCREEN_BUFFER_SIZE_IN_BYTES);
    if (rv == NULL) SET_ERROR(C8_CLEAR_SCREEN);
}

void c8_opcode00EE(struct chip8 *context, WORD opcode) {
    // Error: Stackunderflow
    if (context->sp == 0xEA0) SET_ERROR(C8_STACKUNDERFLOW);

    context->pc  =  context->memory[--context->sp];         // get lower nibble
    context->pc |= (context->memory[--context->sp] << 8);   // get upper nibble
}

void c8_opcode1NNN(struct chip8 *context, WORD opcode) {
    int nnn = C8_OPCODE_SELECT_NNN(opcode);

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, )
    context->pc = nnn;
}

void c8_opcode2NNN(struct chip8 *context, WORD opcode) {
    // Error: Stackoverflow
    if (context->sp >= 0xEFF) {
        SET_ERROR(C8_STACKOVERFLOW);
        return;
    }

    context->memory[context->sp++] = (context->pc >> 8);        // assign upper nibble
    context->memory[context->sp++] = (context->pc & 0x00FF);    // assign lower nibble
    context->pc = C8_OPCODE_SELECT_NNN(opcode);
}

void c8_opcode8XY4(struct chip8 *context, WORD opcode) {
    int res;
    
    C8_OPCODE_SELECT_XYN(opcode);

    res = context->registers[X] + context->registers[Y];
    context->registers[X] = res;                        // implicitly truncated to 8 bits
    context->registers[VF] = (res & 0x100) >> 8;        // carry flag if 9nth bit set
}


void c8_opcode8XY6(struct chip8 *context, WORD opcode) {
    C8_OPCODE_SELECT_XYN(opcode);

    context->registers[VF] = (context->registers[X] & 0x1); // get LSB
    context->registers[X] >>= 1;
}

void c8_opcode8XYE(struct chip8 *context, WORD opcode) {
    C8_OPCODE_SELECT_XYN(opcode);

    context->registers[VF] = (context->registers[X] & 0x80) >> 7; // get MSB
    context->registers[X] <<= 1;
}

void c8_opcodeANNN(struct chip8 *context, WORD opcode) {
    WORD nnn = C8_OPCODE_SELECT_NNN(opcode);

    CHECK_AUTHORIZED_MEM_ACCESS(nnn,);

    context->addressI = nnn;
}

void c8_opcodeBNNN(struct chip8 *context, WORD opcode) {
    WORD nnn;

    nnn  = C8_OPCODE_SELECT_NNN(opcode);
    nnn += context->registers[0];

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, )

    context->pc = nnn;
}

void c8_opcodeCXNN(struct chip8 *context, WORD opcode) {
    // TODO: gen rand
}

void c8_opcodeDXYN(struct chip8 *context, WORD opcode) {
    int addressI, bufIndex;

    C8_OPCODE_SELECT_XYN(opcode);

    addressI = context->addressI;
    bufIndex = X + Y * SCREEN_WIDTH;

    context->registers[VF] = 0; // reset VF
    
    for (int i  = 0; i < N; ++i) {
        for (int j = 0; j < 8; ++j) {
            assert(bufIndex < SCREEN_BUFFER_SIZE_IN_BYTES); // can be considered as scroll

            int spritePixel, screenPixel, newPixel;
            
            spritePixel = read_memory(context, addressI);   RETURN_ON_ERROR;
            screenPixel = context->screenBuffer[bufIndex];
            newPixel = spritePixel ^ screenPixel;

            context->screenBuffer[bufIndex] = newPixel;     // set new pixel
            context->registers[VF] |= newPixel;             // if set, never unset

            ++bufIndex;
            ++addressI; // get next pixel
        }
    }
}

void c8_opcodeFX07(struct chip8 *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);
    
    context->registers[X] = context->delayTimer;
}

void c8_opcodeFX0A(struct chip8 *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);

    --context->pc; // lock pc

    if(context->keyPressed != -1) {
        context->registers[X] = context->keyPressed;
        ++context->pc;  // unlock pc
    }
}

void c8_opcodeFX33(struct chip8 *context, WORD opcode) {
    WORD i;
    int res, rv;

    C8_OPCODE_SELECT_XNN(opcode);

    i   = context->addressI;
    res = (context->registers[X] & 0x7);

    write_memory(context, i,   (res >> 2) & 0x1);  RETURN_ON_ERROR;
    write_memory(context, i+1, (res >> 1) & 0x1);  RETURN_ON_ERROR;
    write_memory(context, i+2, res & 0x1);         RETURN_ON_ERROR;
}

void c8_opcodeFX55(struct chip8 *context, WORD opcode) {
    WORD addressI = context->addressI;

    C8_OPCODE_SELECT_XNN(opcode);

    assert(X < REGISTER_COUNT);

    for (int i = 0; i <= X; ++i) {
        write_memory(context, addressI++, context->registers[i]);
        RETURN_ON_ERROR;
    }
}

void c8_opcodeFX65(struct chip8 *context, WORD opcode) {
    WORD addressI = context->addressI;

    C8_OPCODE_SELECT_XNN(opcode);

    assert(X < REGISTER_COUNT);

    for (int i = 0; i <= X; ++i) {
        int tmp = read_memory(context, addressI++); RETURN_ON_ERROR;
        context->registers[i] = tmp;
    }
}

