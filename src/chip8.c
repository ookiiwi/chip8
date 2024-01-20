#include "chip8.h"
#include "c8_decode.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "ini.h"

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
c8_error_t c8_get_error(chip8_t *context) { return context->m_error; }
void       c8_set_error(chip8_t *context, c8_error_t error) { context->m_error = error; }
void       c8_clr_error(chip8_t *context) { c8_set_error(context, (c8_error_t){ C8_GOOD, "" }); }

/* Setup */

void c8_reset(chip8_t *context, c8_beeper_t *beeper) {
    context->memory         = (BYTE*)calloc(MEMORY_SIZE_IN_BYTES, sizeof(BYTE));
    context->registers      = (BYTE*)calloc(REGISTER_COUNT, sizeof(BYTE));
    context->screenBuffer   = (BYTE*)calloc(SCREEN_BUFFER_SIZE_IN_BITS, sizeof(BYTE));
    context->sp             = USER_MEMORY_END + 1;
    context->pc             = USER_MEMORY_START;
    context->addressI       = 0;
    context->delayTimer     = 0;
    context->soundTimer     = 0;
    context->m_error        = (c8_error_t){ C8_GOOD, "" };
    context->config         = (c8_config_t){ "", DEFAULT_WRAPY, DEFAULT_CLOCKSPEED, DEFAULT_FPS };
    context->m_on_set_key   = NULL;
    context->isRunning      = 1;
    context->beeper         = beeper;

    // preset keys to 0
    memset(context->m_keys, 0, sizeof context->m_keys);

    // preload font
    memcpy((void*)context->memory, (void*)font, sizeof font / sizeof font[0]);
}

void c8_destroy(chip8_t *context) {
    free(context->memory);
    free(context->registers);
    free(context->screenBuffer);
}

static int config_handler(void *user, const char *section, const char *name, const char *value) {
    c8_config_t *config = (c8_config_t*)user;

    if (strcmp(section, config->name) == 0) {
        if (strcmp(name, "wrapY") == 0) {
            config->wrapY = atoi(value);
        } else if (strcmp(name, "clockspeed") == 0) {
            config->clockspeed = atof(value);
        } else if (strcmp(name, "fps") == 0) {
            config->fps = atof(value);
        } else {
            return -1;
        }
    }

    return 0;
}

int c8_load_prgm(chip8_t *context, const char *path, const char *configPath) {
    FILE *fp;
    size_t sz;
    int filenameStart;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        SET_ERROR(C8_LOAD_CANNOT_OPEN_FILE);
        return -1;
    }
    
    fseek(fp, 0L, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    if (sz > USER_MEMORY_SIZE_IN_BYTES) {
        SET_ERROR(C8_LOAD_MEMORY_BUFFER_TOO_LARGE);
        fclose(fp);
        return -1;
    }

    memset((void*)(context->memory + USER_MEMORY_START), 0, USER_MEMORY_SIZE_IN_BYTES);
    fread((void*)(context->memory + USER_MEMORY_START), sz, 1, fp);
    
    fclose(fp);

    if (configPath != NULL && (filenameStart = last_index(path, '/')) >= 0 && filenameStart < strlen(path)) {
        strcpy(context->config.name, (path + filenameStart + 1));

        // load config
        if (ini_parse(configPath, config_handler, &(context->config)) < 0) {
            SET_ERROR(C8_LOAD_CANNOT_OPEN_CONFIG);  // Non-fatal error
        }
    }

    return 0;
}

/* Fetch-decode */

int c8_tick(chip8_t *context) {
    WORD opcode;
    c8_error_t err;

    if (!context->isRunning) {
        return context->lastOpcode;
    }

    opcode = c8_fetch(context);
    c8_decode(context, opcode);

    if ((err = c8_get_error(context)).err != C8_GOOD) {
        fprintf(stderr, "c8_decode Error(%d): %s at %04x(%04x)\n", err.err, err.msg, context->pc, opcode);
        return -1;
    }

    context->lastOpcode = opcode;
    return opcode;
}

void c8_updateTimers(chip8_t *context) {
    if (context->delayTimer) --context->delayTimer;
    if (context->soundTimer) {
        --context->soundTimer;
    }
}

WORD c8_fetch(chip8_t *context) {
    WORD res;

    res  = (context->memory[context->pc] << 8);
    res |=  context->memory[context->pc + 1];
    INCREMENENT_PC;

    return res;
}

C8_DECODE_FUNC_GEN(c8_decode_internal, chip8_t*, c8_opcode)

void c8_decode(chip8_t *context, WORD opcode) {
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

void write_memory(chip8_t *context, WORD address, BYTE data) {
    CHECK_AUTHORIZED_MEM_ACCESS(address, "Error accessing memory", );

    context->memory[address] = data;
}

int read_memory(chip8_t *context, WORD address) {
    CHECK_AUTHORIZED_MEM_ACCESS(address, "Error reading memory", 0)
    
    return context->memory[address];
}

/* Key handling */
void c8_set_key(chip8_t *context, int key)   { 
    context->m_keys[key] = 1; 

    if (context->m_on_set_key != NULL) {
        context->m_on_set_key(context, key);
    }
}

void c8_unset_key(chip8_t *context, int key) { context->m_keys[key] = 0; }

/* Instructions */

void c8_opcode00E0(chip8_t *context, WORD opcode) {
    void* rv = memset((void*)context->screenBuffer, 0, SCREEN_BUFFER_SIZE_IN_BITS);
    VF = 0;
    if (rv == NULL) SET_ERROR(C8_CLEAR_SCREEN);
}

void c8_opcode00EE(chip8_t *context, WORD opcode) {
    // Error: Stackunderflow
    if (context->sp == 0xEA0) SET_ERROR(C8_STACKUNDERFLOW);

    context->pc  =  context->memory[--context->sp];         // get lower nibble
    context->pc |= (context->memory[--context->sp] << 8);   // get upper nibble
}

void c8_opcode1NNN(chip8_t *context, WORD opcode) {
    int nnn = C8_OPCODE_SELECT_NNN(opcode);

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in 1NNN",)
    context->pc = nnn;
}

void c8_opcode2NNN(chip8_t *context, WORD opcode) {
    // Error: Stackoverflow
    if (context->sp >= 0xEFF) {
        SET_ERROR(C8_STACKOVERFLOW);
        return;
    }

    context->memory[context->sp++] = (context->pc >> 8);        // assign upper nibble
    context->memory[context->sp++] = (context->pc & 0x00FF);    // assign lower nibble
    context->pc = C8_OPCODE_SELECT_NNN(opcode);
}

void c8_opcode8XY4(chip8_t *context, WORD opcode) {
    int res;
    
    C8_OPCODE_SELECT_XYN(opcode);

    res = VX + VY;
    VX = res;                        // implicitly truncated to 8 bits
    VF = (res & 0x100) >> 8;        // carry flag if 9nth bit set
}


void c8_opcode8XY6(chip8_t *context, WORD opcode) {
    C8_OPCODE_SELECT_XYN(opcode);

    VF = (VX & 0x1); // get LSB
    VX >>= 1;
}

void c8_opcode8XYE(chip8_t *context, WORD opcode) {
    C8_OPCODE_SELECT_XYN(opcode);

    VF = (VX & 0x80) >> 7; // get MSB
    VX <<= 1;
}

void c8_opcodeANNN(chip8_t *context, WORD opcode) {
    WORD nnn = C8_OPCODE_SELECT_NNN(opcode);

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in ANNN",);

    context->addressI = nnn;
}

void c8_opcodeBNNN(chip8_t *context, WORD opcode) {
    WORD nnn;

    nnn  = C8_OPCODE_SELECT_NNN(opcode);
    nnn += context->registers[0];

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in BNNN",)

    context->pc = nnn;
}

void c8_opcodeCXNN(chip8_t *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);

    VX = (rand() & NN);
}

void c8_opcodeDXYN(chip8_t *context, WORD opcode) {
    int x, y;
    int px, py;
    int addressI;
    BYTE spritePixelRow;

    C8_OPCODE_SELECT_XYN(opcode);

    x = VX;
    y = VY;
    addressI = context->addressI;

    VF = 0; // reset VF

    // loop through 8*N sprite
    for (int row = 0; row < N; ++row) {
        spritePixelRow = read_memory(context, context->addressI + row);   RETURN_ON_ERROR;
         
        py = (y + row);
        
        if (context->config.wrapY) {
            py %= SCREEN_HEIGHT;
        } else if (py >= SCREEN_HEIGHT) {
            break;
        }

        for (int col = 0, curPixel = 0x80; col < 8; ++col, curPixel >>= 1) {
            px = (x + col) % SCREEN_WIDTH;
            
            if (spritePixelRow & curPixel) {
                int index = (py * SCREEN_WIDTH) + px;
                assert(index < SCREEN_BUFFER_SIZE_IN_BITS);

                // check collision
                if (context->screenBuffer[index] == 1) {
                    VF = 1;
                }

                context->screenBuffer[index] ^= 1;
            }
        }
    }
}

void c8_opcodeFX07(chip8_t *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);
    
    VX = context->delayTimer;
}

void wait_for_key(chip8_t *context, int key) {
    C8_OPCODE_SELECT_XNN(context->lastOpcode);

    context->isRunning = 1;
    VX = key;
    context->m_on_set_key = NULL;
}

void c8_opcodeFX0A(chip8_t *context, WORD opcode) {
    context->isRunning = 0;
    context->m_on_set_key = wait_for_key;
}

void c8_opcodeFX18(chip8_t *context, WORD opcode) { 
    _ASSIGN(NN, context->soundTimer, context->registers[X], );

    if (context->beeper != NULL) {
        context->beeper->beep(context->beeper->user_data);
    }
}

void c8_opcodeFX33(chip8_t *context, WORD opcode) {
    int res;

    C8_OPCODE_SELECT_XNN(opcode);

    res = VX;

    BYTE bcd[] = { res/100, (res/10) % 10, res % 10 };
    memcpy((void*)&context->memory[context->addressI], (void*)bcd, 3);
}

void c8_opcodeFX55(chip8_t *context, WORD opcode) {
    WORD addressI = context->addressI;

    C8_OPCODE_SELECT_XNN(opcode);

    assert(X < REGISTER_COUNT);

    for (int i = 0; i <= X; ++i) {
        write_memory(context, addressI++, context->registers[i]);
        RETURN_ON_ERROR;
    }
}

void c8_opcodeFX65(chip8_t *context, WORD opcode) {
    WORD addressI = context->addressI;

    C8_OPCODE_SELECT_XNN(opcode);

    assert(X < REGISTER_COUNT);

    for (int i = 0; i <= X; ++i) {
        int tmp = read_memory(context, addressI++); RETURN_ON_ERROR;
        context->registers[i] = tmp;
    }
}

