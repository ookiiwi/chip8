#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <iostream>

extern "C" {
#include "chip8.h"
};

#define REQUIRE_C8_PC_EQ(val)       REQUIRE(context->pc == val)
#define REQUIRE_NO_C8_ERROR         REQUIRE(c8_get_error(context) == C8_GOOD)
#define REQUIRE_C8_ERROR(err)       REQUIRE(c8_get_error(context) == err)
#define CLEAR_C8_ERROR              c8_set_error(context, C8_GOOD)
#define C8_DECODE(opcode)           c8_decode(context, opcode)
#define C8_DECODE_ERR(opcode, err)  C8_DECODE(opcode); REQUIRE_C8_ERROR(err)
#define C8_DECODE_NO_ERR(opcode)    C8_DECODE(opcode); REQUIRE_NO_C8_ERROR

bool check_all_zeros(BYTE *arr, size_t arrSize) {
    int res = 0;
    for (int i = 0; i < arrSize; ++i)
            res |= arr[i];
    return res == 0;
} 

TEST_CASE( "Helper macros" ) {
    SECTION( "select xyn" ) {
        C8_OPCODE_SELECT_XYN(0x1234);
        REQUIRE(X == 2);
        REQUIRE(Y == 3);
        REQUIRE(N == 4);
    }
}

TEST_CASE( "Setup" ) {
    struct chip8 _context;
    struct chip8 *context;

    context = &_context;

    c8_reset(context);

    SECTION( "Reset" ) {
        REQUIRE(check_all_zeros(context->memory, MEMORY_SIZE_IN_BYTES));
        REQUIRE(check_all_zeros(context->registers, REGISTER_COUNT * sizeof(BYTE)));
        REQUIRE(check_all_zeros(context->screenBuffer, SCREEN_BUFFER_SIZE_IN_BYTES));
        REQUIRE(context->sp == USER_MEMORY_END+1);
        REQUIRE_C8_PC_EQ(USER_MEMORY_START);
        REQUIRE(context->addressI == USER_MEMORY_START);
        REQUIRE(context->delayTimer == 0);
        REQUIRE(context->soundTimer == 0);
    }

    SECTION( "Load memory" ) {
        BYTE buf[USER_MEMORY_SIZE_IN_BYTES];

        memset((void*)buf, 1, USER_MEMORY_SIZE_IN_BYTES);

        c8_load_memory(context, buf, USER_MEMORY_SIZE_IN_BYTES+1);
        REQUIRE_C8_ERROR(C8_LOAD_MEMORY_BUFFER_TOO_LARGE);
        CLEAR_C8_ERROR;

        c8_load_memory(context, buf, USER_MEMORY_SIZE_IN_BYTES);
        REQUIRE_NO_C8_ERROR;
        REQUIRE(check_all_zeros(context->memory + USER_MEMORY_START, USER_MEMORY_SIZE_IN_BYTES) == false);
    }

    c8_destroy(context);
}

TEST_CASE( "Fetch" ) {
    struct chip8 _context;
    struct chip8 *context;
    int pc_start;
    BYTE prgm[] = {
        0x61, 0x02, // V1  = 0x02
        0x71, 0x02  // V1 += 0x02;
    };

    context = &_context;

    c8_reset(context);
    pc_start = context->pc;

    c8_load_memory(context, prgm, 4);
    REQUIRE_NO_C8_ERROR;

    REQUIRE(c8_fetch(context) == ((prgm[0] << 8) | prgm[1]));
    REQUIRE_C8_PC_EQ(pc_start+2);

    c8_destroy(context);
}

TEST_CASE( "Opcode emulation" ) {
    struct chip8 _context;
    struct chip8 *context;

    context = &_context;
    c8_reset(context);
    
    SECTION( "00E0 -- Clear screen" ) {
        memset((void*)(context->screenBuffer), 0xFF, SCREEN_BUFFER_SIZE_IN_BYTES);
        
        C8_DECODE_NO_ERR(0x00E0);
        REQUIRE(check_all_zeros(context->screenBuffer, SCREEN_BUFFER_SIZE_IN_BYTES));
    }

    SECTION( "00EE -- return" ) {
        WORD sp_start = context->sp;

        // push address 0x201 on stack
        context->memory[context->sp++] = context->pc >> 8;
        context->memory[context->sp++] = context->pc & 0x00FF;
        context->pc = 0x222;

        REQUIRE_C8_PC_EQ(0x222);
        C8_DECODE_NO_ERR(0x00EE);
        REQUIRE_C8_PC_EQ(0x200);
        REQUIRE(context->sp == sp_start);

        // Stackunderflow
        C8_DECODE_ERR(0x00EE, C8_STACKUNDERFLOW);
    }

    SECTION( "1NNN -- Jump to address NNN" ) {
        C8_DECODE_NO_ERR(0x1222);
        REQUIRE_C8_PC_EQ(0x222);

        C8_DECODE_ERR(0x1FFF, C8_REFUSED_MEM_ACCESS);
    }

    SECTION( "2NNN -- Call subroutine at NNN" ) {
        WORD sp_start = context->sp;
        WORD pc_start = context->pc;

        // Check call
        C8_DECODE_NO_ERR(0x2222);
        REQUIRE(context->sp == (sp_start + 2));
        REQUIRE_C8_PC_EQ(0x222);

        // Return
        C8_DECODE_NO_ERR(0x00EE);
        REQUIRE_C8_PC_EQ(pc_start);

        context->sp = 0xFFFF;
        C8_DECODE_ERR(0x2222, C8_STACKOVERFLOW);
    }

    // TODO: replace by SKIP_IF_CMP_XNN macro test
    SECTION( "3XNN - 4XNNN -- Skip next if Vx == NN" ) {
        WORD pc_start = context->pc;

        // Don't skip
        C8_DECODE_NO_ERR(0x3111);
        REQUIRE_C8_PC_EQ(pc_start);
        
        // Skip
        C8_DECODE_NO_ERR(0x3100);
        REQUIRE_C8_PC_EQ(pc_start+1);
    }

    // TODO: replace by SKIP_IF_CMP_XYN macro test
    SECTION( "5XY0 - 9XY0 -- Skip next if Vx == NN" ) {
        WORD pc_start = context->pc;

        // Skip
        C8_DECODE_NO_ERR(0x5120);
        REQUIRE_C8_PC_EQ(pc_start+1);
        
        // Don't skip
        context->registers[1] = 0x01;
        C8_DECODE_NO_ERR(0x5120);
        REQUIRE_C8_PC_EQ(pc_start+1);
    }

    SECTION( "6XNN to 8XY3 -- Assign, BitOp and Math" ) {
        // 6XNN
        C8_DECODE_NO_ERR(0x6111);
        REQUIRE(context->registers[1] == 0x11);

        // 7XNN
        C8_DECODE_NO_ERR(0x7111);
        REQUIRE(context->registers[1] == (0x11 * 2));

        // 8XY1
        C8_DECODE_NO_ERR(0x8211);
        REQUIRE(context->registers[2] == (0x11 * 2));
    }

    SECTION ( "8XY4 -- Add Vy to Vx" ) {
        context->registers[1] = 0xFF;
        context->registers[2] = 0xFF;
        context->registers[3] = 0x01;
        context->registers[4] = 0x01;

        // Carry -- 0xFF + 0xFF
        C8_DECODE_NO_ERR(0x8124);
        REQUIRE(context->registers[1]  == 0xFE);
        REQUIRE(context->registers[VF] == 0x01);

        // No carry -- 0x01 + 0x01
        C8_DECODE_NO_ERR(0x8344);
        REQUIRE(context->registers[3]  == 0x02);
        REQUIRE(context->registers[VF] == 0x00);
    }

    SECTION( "8XY5 - 8XY7 -- Sub and assign to Vx" ) {
        context->registers[1] = 0x01;
        context->registers[2] = 0x02;
        context->registers[3] = 0x02;
        context->registers[4] = 0x01;

        // Borrow -- 0x01 - 0x02
        C8_DECODE_NO_ERR(0x8125);
        REQUIRE(context->registers[1]  == 0x01);
        REQUIRE(context->registers[VF] == 0x00);

        // No borrow -- 0x02 - 0x01
        C8_DECODE_NO_ERR(0x8345);
        REQUIRE(context->registers[3]  == 0x01);
        REQUIRE(context->registers[VF] == 0x01);
    }

    SECTION( "8XY6 - 8XYE -- Right/Left shift and assign LSB/MSB to VF" ) {
        context->registers[1] = 0x03;
        context->registers[2] = 0xF0;

        // 8XY6
        C8_DECODE_NO_ERR(0x8106);
        REQUIRE(context->registers[VF] == 0x01);
        REQUIRE(context->registers[1]  == 0x01);

        // 8XYE
        C8_DECODE_NO_ERR(0x820E);
        REQUIRE(context->registers[VF] == 0x01);
        REQUIRE(context->registers[2]  == 0xE0);
    }

    SECTION( "ANNN -- Set address register I" ) {
        C8_DECODE_NO_ERR(0xA222);
        REQUIRE(context->addressI == 0x222);

        C8_DECODE_ERR(0xAFFF, C8_REFUSED_MEM_ACCESS);
    }

    SECTION( "BNNN -- Jump to address (NNN + V0)" ) {
        context->registers[0] = 0x22;

        C8_DECODE_NO_ERR(0xB200);
        REQUIRE_C8_PC_EQ(0x222);

        C8_DECODE_ERR(0xBFFF, C8_REFUSED_MEM_ACCESS);
    }

    SECTION( "Input" ) {
        // EX9E
        context->keyPressed = 0x01;
        context->registers[1] = 0x01;
        C8_DECODE_NO_ERR(0xE19E);
        REQUIRE_C8_PC_EQ(USER_MEMORY_START+1);

        //FX0A
    }

    SECTION( "DYXN -- display sprite" ) {
        // TODO: test
    }

    SECTION( "BCD" ) {
        int i;
        
        i = context->addressI;
        context->registers[1] = 0xF;

        C8_DECODE_NO_ERR(0xF133);
        REQUIRE(read_memory(context, i)   == 0x1);
        REQUIRE(read_memory(context, i+1) == 0x1);
        REQUIRE(read_memory(context, i+2) == 0x1);
    }

    SECTION( "Dump/Load registers" ) {
#define ITER for (int i = USER_MEMORY_START, j = 0; i < USER_MEMORY_START + REGISTER_COUNT; ++i, ++j)
        
        ITER {
            context->memory[i] = j;
        }

        // FX65 -- Load
        C8_DECODE_NO_ERR(0xFF65);
        ITER {
            REQUIRE(context->registers[j] == context->memory[i]);
        }

        // FX55 -- Dump
        memset((void*)(context->memory+USER_MEMORY_START), 0, USER_MEMORY_SIZE_IN_BYTES);
        C8_DECODE_NO_ERR(0xFF55);
        ITER {
            REQUIRE(context->registers[j] == context->memory[i]);
        }
#undef ITER
    }

    c8_destroy(context);
}