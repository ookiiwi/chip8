#include "chip8.h"
#include "c8_decode.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "ini.h"

#define SET_ERROR_1(err)      C8_SetError(context, (C8_Error){err, ""})
#define SET_ERROR_2(err, msg) C8_SetError(context, (C8_Error){err, msg})

#define SET_ERROR_X(err, msg, FUNC, ...) FUNC
#define SET_ERROR_MACRO_CHOOSER(...)                                                 \
    SET_ERROR_X(__VA_ARGS__, SET_ERROR_2, SET_ERROR_1, )
#define SET_ERROR(...) SET_ERROR_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define GET_ERROR             C8_GetError(context)
#define RETURN_ON_ERROR       if (GET_ERROR.err != C8_GOOD) return

/* Memory access */
#define CHECK_AUTHORIZED_MEM_ACCESS(address, msg, res) \
    if (address > USER_MEMORY_END) { SET_ERROR(C8_REFUSED_MEM_ACCESS, msg); return res; }

void write_memory(C8_Context *context, WORD address, BYTE data) {
    CHECK_AUTHORIZED_MEM_ACCESS(address, "Error accessing memory", );

    context->memory[address] = data;
}

int read_memory(C8_Context *context, WORD address) {
    CHECK_AUTHORIZED_MEM_ACCESS(address, "Error reading memory", 0)
    
    return context->memory[address];
}

/* Error handling */
C8_Error C8_GetError(C8_Context *context) { return context->m_error; }
void     C8_SetError(C8_Context *context, C8_Error error) { context->m_error = error; }
void     C8_ClearError(C8_Context *context) { C8_SetError(context, (C8_Error){ C8_GOOD, "" }); }

/* Setup */

void C8_Reset(C8_Context *context, C8_Beeper *beeper) {
    context->memory         = (BYTE*)calloc(MEMORY_SIZE_IN_BYTES, sizeof(BYTE));
    context->registers      = (BYTE*)calloc(REGISTER_COUNT, sizeof(BYTE));
    context->display        = (BYTE*)calloc(SCREEN_BUFFER_SIZE_IN_BITS, sizeof(BYTE));
    context->sp             = USER_MEMORY_END + 1;
    context->pc             = USER_MEMORY_START;
    context->addressI       = 0;
    context->delay_timer    = 0;
    context->sound_timer    = 0;
    context->m_error        = (C8_Error){ C8_GOOD, "" };
    context->config         = (C8_Config){ "", DEFAULT_WRAPY, DEFAULT_CLOCKSPEED, DEFAULT_FPS };
    context->m_on_set_key   = NULL;
    context->is_running     = 1;
    context->beeper         = beeper;

    // preset keys to 0
    memset(context->m_keys, 0, sizeof context->m_keys);

    // preload font
    memcpy((void*)context->memory, (void*)font, sizeof font / sizeof font[0]);
}

void C8_Destroy(C8_Context *context) {
    free(context->memory);
    free(context->registers);
    free(context->display);
}

static int config_handler(void *user, const char *section, const char *name, const char *value) {
    C8_Config *config = (C8_Config*)user;

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

int C8_LoadProgram(C8_Context *context, const char *path, const char *configPath) {
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

int C8_Tick(C8_Context *context) {
    WORD opcode;
    C8_Error err;

    if (!context->is_running) {
        return context->last_opcode;
    }

    opcode = C8_Fetch(context);
    C8_Decode(context, opcode);

    if ((err = C8_GetError(context)).err != C8_GOOD) {
        fprintf(stderr, "c8_decode Error(%d): %s at %04x(%04x)\n", err.err, err.msg, context->pc, opcode);
        return -1;
    }

    context->last_opcode = opcode;
    return opcode;
}

void C8_UpdateTimers(C8_Context *context) {
    if (context->delay_timer) --context->delay_timer;
    if (context->sound_timer) {
        --context->sound_timer;
    }
}

WORD C8_Fetch(C8_Context *context) {
    WORD res;

    res  = (context->memory[context->pc] << 8);
    res |=  context->memory[context->pc + 1];
    INCREMENENT_PC;

    return res;
}

C8_DECODE_FUNC_GEN(C8_Decode_Internal, C8_Context*, C8_Opcode)

void C8_Decode(C8_Context *context, WORD opcode) {
    #define SET_INVALID_OPCODE_ERROR(s) SET_ERROR(C8_DECODE_INVALID_OPCODE, s)

    int rv = C8_Decode_Internal(context, opcode);

    if(rv != 0) {
        char s[5];
        snprintf(s, 5, "%04x", rv);
        SET_INVALID_OPCODE_ERROR(s);
    }
}

/* Key handling */
void C8_SetKey(C8_Context *context, int key)   { 
    context->m_keys[key] = 1; 

    if (context->m_on_set_key != NULL) {
        context->m_on_set_key(context, key);
    }
}

void C8_UnsetKey(C8_Context *context, int key) { context->m_keys[key] = 0; }

/* Instructions */

void C8_Opcode00E0(C8_Context *context, WORD opcode) {
    void* rv = memset((void*)context->display, 0, SCREEN_BUFFER_SIZE_IN_BITS);
    VF = 0;
    if (rv == NULL) SET_ERROR(C8_CLEAR_SCREEN);
}

void C8_Opcode00EE(C8_Context *context, WORD opcode) {
    // Error: Stackunderflow
    if (context->sp == 0xEA0) SET_ERROR(C8_STACKUNDERFLOW);

    context->pc  =  context->memory[--context->sp];         // get lower nibble
    context->pc |= (context->memory[--context->sp] << 8);   // get upper nibble
}

void C8_Opcode1NNN(C8_Context *context, WORD opcode) {
    int nnn = C8_OPCODE_SELECT_NNN(opcode);

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in 1NNN",)
    context->pc = nnn;
}

void C8_Opcode2NNN(C8_Context *context, WORD opcode) {
    // Error: Stackoverflow
    if (context->sp >= 0xEFF) {
        SET_ERROR(C8_STACKOVERFLOW);
        return;
    }

    context->memory[context->sp++] = (context->pc >> 8);        // assign upper nibble
    context->memory[context->sp++] = (context->pc & 0x00FF);    // assign lower nibble
    context->pc = C8_OPCODE_SELECT_NNN(opcode);
}

void C8_Opcode8XY4(C8_Context *context, WORD opcode) {
    int res;
    
    C8_OPCODE_SELECT_XYN(opcode);

    res = VX + VY;
    VX = res;                        // implicitly truncated to 8 bits
    VF = (res & 0x100) >> 8;        // carry flag if 9nth bit set
}


void C8_Opcode8XY6(C8_Context *context, WORD opcode) {
    C8_OPCODE_SELECT_XYN(opcode);

    VF = (VX & 0x1); // get LSB
    VX >>= 1;
}

void C8_Opcode8XYE(C8_Context *context, WORD opcode) {
    C8_OPCODE_SELECT_XYN(opcode);

    VF = (VX & 0x80) >> 7; // get MSB
    VX <<= 1;
}

void C8_OpcodeANNN(C8_Context *context, WORD opcode) {
    WORD nnn = C8_OPCODE_SELECT_NNN(opcode);

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in ANNN",);

    context->addressI = nnn;
}

void C8_OpcodeBNNN(C8_Context *context, WORD opcode) {
    WORD nnn;

    nnn  = C8_OPCODE_SELECT_NNN(opcode);
    nnn += context->registers[0];

    CHECK_AUTHORIZED_MEM_ACCESS(nnn, "Error in BNNN",)

    context->pc = nnn;
}

void C8_OpcodeCXNN(C8_Context *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);

    VX = (rand() & NN);
}

void C8_OpcodeDXYN(C8_Context *context, WORD opcode) {
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
                if (context->display[index] == 1) {
                    VF = 1;
                }

                context->display[index] ^= 1;
            }
        }
    }
}

void C8_OpcodeFX07(C8_Context *context, WORD opcode) {
    C8_OPCODE_SELECT_XNN(opcode);
    
    VX = context->delay_timer;
}

void wait_for_key(C8_Context *context, int key) {
    C8_OPCODE_SELECT_XNN(context->last_opcode);

    context->is_running = 1;
    VX = key;
    context->m_on_set_key = NULL;
}

void C8_OpcodeFX0A(C8_Context *context, WORD opcode) {
    context->is_running = 0;
    context->m_on_set_key = wait_for_key;
}

void C8_OpcodeFX18(C8_Context *context, WORD opcode) { 
    _ASSIGN(NN, context->sound_timer, context->registers[X], );

    if (context->beeper != NULL) {
        context->beeper->beep(context->beeper->user_data);
    }
}

void C8_OpcodeFX33(C8_Context *context, WORD opcode) {
    int res;

    C8_OPCODE_SELECT_XNN(opcode);

    res = VX;

    BYTE bcd[] = { res/100, (res/10) % 10, res % 10 };
    memcpy((void*)&context->memory[context->addressI], (void*)bcd, 3);
}

void C8_OpcodeFX55(C8_Context *context, WORD opcode) {
    WORD addressI = context->addressI;

    C8_OPCODE_SELECT_XNN(opcode);

    assert(X < REGISTER_COUNT);

    for (int i = 0; i <= X; ++i) {
        write_memory(context, addressI++, context->registers[i]);
        RETURN_ON_ERROR;
    }
}

void C8_OpcodeFX65(C8_Context *context, WORD opcode) {
    WORD addressI = context->addressI;

    C8_OPCODE_SELECT_XNN(opcode);

    assert(X < REGISTER_COUNT);

    for (int i = 0; i <= X; ++i) {
        int tmp = read_memory(context, addressI++); RETURN_ON_ERROR;
        context->registers[i] = tmp;
    }
}

