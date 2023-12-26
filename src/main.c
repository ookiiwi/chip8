#include "chip8.h"

#include <stdio.h>
#include <assert.h>
#include <SDL.h>

#define PIXEL_SCALE 10
#define WINDOW_WIDTH  64*PIXEL_SCALE
#define WINDOW_HEIGHT 32*PIXEL_SCALE

void pixel_renderer(int pixel, int x, int y, void **userData) {
    int color;
    SDL_Renderer *ren;

    color = (pixel == 0 ? 0x00 : 0xFF); 
    ren = (SDL_Renderer*)(*userData);

    SDL_SetRenderDrawColor(ren, color, color, color, 0xFF);

    SDL_RenderDrawPoint(ren, x, y);
    SDL_RenderPresent(ren);
}

int load_prgm(struct chip8 *context, int argc, char **argv) {
    FILE *fp;
    char *filename = argv[1];

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return -1;
    }

    c8_load_prgm(context, fp);
    fclose(fp);

    return 0;
}

int main(int argc, char** argv) {
    int bRunning;
    struct chip8 _context;
    struct chip8 *context;
    WORD opcode;
    SDL_Window *win;
    SDL_Renderer *ren;

    /* init components */
    bRunning = 1;
    context = &_context;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;   
    }

    win = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (win == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (ren == NULL) {
        SDL_DestroyWindow(win);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetScale(ren, PIXEL_SCALE, PIXEL_SCALE);

    c8_reset(context, pixel_renderer, (void*)&ren);
    int rv = load_prgm(context, argc, argv);
    if (rv != 0 ) {
        c8_destroy(context);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        printf("load_prgm Error\n");
        SDL_Quit();
        return 1;
    }

    for (int i = USER_MEMORY_START; i < USER_MEMORY_END; i+=2) {
        if (context->memory[i] == 0 && context->memory[i+1] == 0) {
            break;
        }
        
        printf("0x%02hhx%02hhx\n", context->memory[i], context->memory[i+1]);
    }

    int tmp = 0;
    while(bRunning) {
        SDL_Event event;

        while(SDL_PollEvent(&event)) {
            if( (SDL_QUIT == event.type) || 
                (SDL_KEYDOWN == event.type && SDLK_ESCAPE == event.key.keysym.sym) ) {
                bRunning = 0;
                break;
            }
        }

        context->keyPressed = (SDL_KEYDOWN == event.type ? event.key.keysym.sym : -1);

        WORD opcode;
        opcode = c8_fetch(context);
        c8_decode(context, opcode);

        c8_error_t err;
        if ((err = c8_get_error(context)).err != C8_GOOD) {
            printf("c8_decode Error(%d): %s at %04x(%04x)\n", err.err, err.msg, context->pc, opcode);
            break;
        }
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    c8_destroy(context);
    
    return 0;
}