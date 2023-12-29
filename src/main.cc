#include "chip8.h"
#include "cleanup.hh"

#include <stdio.h>
#include <assert.h>
#include <SDL.h>

#define PIXEL_SCALE 10
#define WINDOW_WIDTH  SCREEN_WIDTH  * PIXEL_SCALE
#define WINDOW_HEIGHT SCREEN_HEIGHT * PIXEL_SCALE

void copy_c8_screenBuffer(SDL_Texture *texture, struct chip8 *context) {
    void    *pixels;
    int      pitch;
    Uint8   *base;

    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    for(int row = 0; row < SCREEN_HEIGHT; ++row) {
        int y = row * PIXEL_SCALE;

        for(int col = 0; col < SCREEN_WIDTH; ++col) {
            int x = col * PIXEL_SCALE;
            int i = (row * SCREEN_WIDTH) + col;
            int color = (context->screenBuffer[i] == 0 ? 0x00 : 0xFF);

            for(int i = 0; i < PIXEL_SCALE; ++i) {          // fill on x
                for (int j = 0; j < PIXEL_SCALE; ++j)  {    // fill on y
                    base = ((Uint8*)pixels) + (4 * ((y + j) * WINDOW_WIDTH + x + i));
                    base[0] = color;
                    base[1] = color;
                    base[2] = color;
                    base[3] = 0xFF;
                }
            }
        }
    }

    SDL_UnlockTexture(texture);
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
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    Uint64 prevTicks, ticks, delta;

    /* init components */
    bRunning = 1;
    context = &_context;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;   
    }

    window = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL) {
        cleanup(window);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (texture == NULL) {
        cleanup(renderer, window);
        printf("SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    c8_reset(context);
    int rv = load_prgm(context, argc, argv);
    if (rv != 0 ) {
        cleanup(context, texture, renderer, window);
        printf("load_prgm Error\n");
        SDL_Quit();
        return 1;
    }

    while(bRunning) {
        SDL_Event event;
        ticks = SDL_GetTicks64();
        delta = ticks - prevTicks;

        while(SDL_PollEvent(&event)) {
            if( (SDL_QUIT == event.type) || 
                (SDL_KEYDOWN == event.type && SDLK_ESCAPE == event.key.keysym.sym) ) {
                bRunning = 0;
                break;
            } else if (SDL_KEYDOWN == event.type) {
                int key = event.key.keysym.sym;

                if (key >= 0x30 && key <= 0x39)                 // 0 to 9
                    context->keyPressed = key & 0x0F;
                else if (key >= 0x60 && key <= 0x65)            // A to F
                    context->keyPressed = (key & 0x0F) + 0x9;
            } else if (SDL_KEYUP == event.type) {
                context->keyPressed = -1;
            }
        }


        if (delta > (1000/600.0)) {
            if (c8_tick(context) != 0) {
                break;
            }

            prevTicks = ticks;
        }

        // As screenBuffer overrides texture pixels, no need to call SDL_RenderClear
        copy_c8_screenBuffer(texture, context);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

    }

    cleanup(context, texture, renderer, window);
    SDL_Quit();
    
    return 0;
}