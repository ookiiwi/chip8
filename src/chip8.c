#include "chip8.h"
#include "c8_decode.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define SET_ERROR_1(err)      c8_set_error(context, (c8_error_t){err, ""})
#define SET_ERROR_2(err, msg) c8_set_error(context, (c8_error_t){err, msg})

#define SET_ERROR_X(err, msg, FUNC, ...) FUNC
#define SET_ERROR_MACRO_CHOOSER(...)                                                 \
    SET_ERROR_X(__VA_ARGS__, SET_ERROR_2, SET_ERROR_1, )
#define SET_ERROR(...) SET_ERROR_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define GET_ERROR           c8_get_error(context)
#define RETURN_ON_ERROR  if (GET_ERROR.err != C8_GOOD) return

#define INCREMENENT_PC  context->pc += 2
#define DECREMENENT_PC  context->pc -= 2

/* Error handling */
c8_error_t c8_get_error(struct chip8 *context) { return context->m_error; }
void       c8_set_error(struct chip8 *context, c8_error_t error) { context->m_error = error; }

/* Setup */

void c8_reset(struct chip8 *context) {
    context->memory         = (BYTE*)calloc(MEMORY_SIZE_IN_BYTES, sizeof(BYTE));
    context->registers      = (BYTE*)calloc(REGISTER_COUNT, sizeof(BYTE));
    context->screenBuffer   = (BYTE*)calloc(SCREEN_BUFFER_SIZE_IN_BITS, sizeof(BYTE));
    context->sp             = USER_MEMORY_END + 1;
    context->addressI       = 0;
    context->pc             = USER_MEMORY_START;
    context->addressI       = USER_MEMORY_START;
    context->delayTimer     = 0;
    context->soundTimer     = 0;
    context->keyPressed     = -1;
    context->m_error        = (c8_error_t){ C8_GOOD, "" };

    // preload font
    memcpy((void*)context->memory, (void*)font, sizeof font / sizeof font[0]);
}

void c8_destroy(struct chip8 *context) {
    free(context->memory);
    free(context->registers);
    free(context->screenBuffer);
}

void c8_load_prgm(struct chip8 *context, FILE *fp) {
    size_t sz;
    
    fseek(fp, 0L, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    if (sz > USER_MEMORY_SIZE_IN_BYTES) {
        SET_ERROR(C8_LOAD_MEMORY_BUFFER_TOO_LARGE);
        return;
    }

    memset((void*)(context->memory + USER_MEMORY_START), 0, USER_MEMORY_SIZE_IN_BYTES);
    fread((void*)(context->memory + USER_MEMORY_START), sz, 1, fp);
}

/* Fetch-decode */

int c8_tick(struct chip8 *context) {
    WORD opcode;
    c8_error_t err;

    if (context->delayTimer) --context->delayTimer;
    if (context->soundTimer) {
        // TODO: beep
        --context->soundTimer;
    }

    opcode = c8_fetch(context);
    c8_decode(context, opcode);

    if ((err = c8_get_error(context)).err != C8_GOOD) {
        fprintf(stderr, "c8_decode Error(%d): %s at %04x(%04x)\n", err.err, err.msg, context->pc, opcode);
        return -1;
    }

    return opcode;
}

WORD c8_fetch(struct chip8 *context) {
    WORD res;

    res  = (context->memory[context->pc] << 8);
    res |=  context->memory[context->pc + 1];
    INCREMENENT_PC;

    return res;
}

C8_DECODE_FUNC_GEN(c8_decode_internal, struct chip8 *, c8_opcode)

void c8_decode(struct chip8 *context, WORD opcode) {
    #define SET_INVALID_OPCODE_ERROR(s) SET_ERROR(C8_DECODE_INVALID_OPCODE, s)

    int rv = c8_decode_internal(context, opcode);

    if(rv != 0) {
        char s[5];
        snprintf(s, 5, "%04x", rv);
        SET_INVALID_OPCODE_ERROR(s);
    }
}

/* Memory access */
#define CHECK_AUTHORIZED_MEM_ACCESS(address, msg, res) \
    if (address > USER_MEMORY_END) { SET_ERROR(C8_REFUSED_MEM_ACCESS, msg); return res; }

void write_memory(struct chip8 *context, WORD address, BYTE data) {
    CHECK_AUTHORIZED_MEM_ACCESS(address, "Error accessing memory", );

    context->memory[address] = data;
}

int read_memory(struct chip8 *context, WORD address) {
    CHECK_AUTHORIZED_MEM_ACCESS(address, "Error reading memory", 0)
    
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

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in 1NNN",)
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

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in ANNN",);

    context->addressI = nnn;
}

void c8_opcodeBNNN(struct chip8 *context, WORD opcode) {
    WORD nnn;

    nnn  = C8_OPCODE_SELECT_NNN(opcode);
    nnn += context->registers[0];

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in BNNN",)

    context->pc = nnn;
}

void c8_opcodeCXNN(struct chip8 *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);

    context->registers[X] = (rand() & NN);
}

void c8_opcodeDXYN(struct chip8 *context, WORD opcode) {
    int x, y;
    int px, py;
    int addressI, bufIndex;
    BYTE spritePixelRow;

    C8_OPCODE_SELECT_XYN(opcode);

    x = context->registers[X];
    y = context->registers[Y];
    addressI = context->addressI;

    context->registers[VF] = 0; // reset VF

    // loop through 8*N sprite
    for (int row  = 0; row < N; ++row) {
        assert(bufIndex < SCREEN_BUFFER_SIZE_IN_BYTES); // can be considered as scroll

        spritePixelRow = read_memory(context, context->addressI + row);   RETURN_ON_ERROR;
         
        py = (y + row) % SCREEN_HEIGHT;

        for (int col = 0, curPixel = 0x80; col < 8; ++col, curPixel >>= 1) {
            px = (x + col) % SCREEN_WIDTH;
            
            if (spritePixelRow & curPixel) {
                int index = (py * SCREEN_WIDTH) + px;
                assert(index < SCREEN_BUFFER_SIZE_IN_BITS);

                // check collision
                if (context->screenBuffer[index] == 1) {
                    context->registers[VF] = 1;
                }

                context->screenBuffer[index] ^= 1;
            }
        }
    }
}

void c8_opcodeFX07(struct chip8 *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);
    
    context->registers[X] = context->delayTimer;
}

void c8_opcodeFX0A(struct chip8 *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);

    DECREMENENT_PC; // lock pc

    if(context->keyPressed != -1) {
        context->registers[X] = context->keyPressed;
        INCREMENENT_PC;  // unlock pc
    }
}

void c8_opcodeFX33(struct chip8 *context, WORD opcode) {
    int res;

    C8_OPCODE_SELECT_XNN(opcode);

    res = context->registers[X];

    BYTE bcd[] = { res/100, (res/10) % 10, res % 10 };
    memcpy((void*)&context->memory[context->addressI], (void*)bcd, 3);
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

