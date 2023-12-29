#include "chip8.h"
#include "cleanup.hh"

#include <stdio.h>
#include <assert.h>
#include <SDL.h>

#define PIXEL_SCALE 10
#define WINDOW_WIDTH  64*PIXEL_SCALE
#define WINDOW_HEIGHT 32*PIXEL_SCALE

#define _EXTRACT_RENDERER_FROM_USER_DATA(userData)  SDL_Renderer *renderer = (SDL_Renderer*)(*userData)

void pixel_clear(void **userData) {
    _EXTRACT_RENDERER_FROM_USER_DATA(userData);

    SDL_RenderClear(renderer);
}

void pixel_render(void **userData) {
    _EXTRACT_RENDERER_FROM_USER_DATA(userData);

    SDL_RenderPresent(renderer);
}

void pixel_set(int pixel, int x, int y, void **userData) {
    int color;

    _EXTRACT_RENDERER_FROM_USER_DATA(userData);

    color = (pixel == 0 ? 0x00 : 0xFF); 

    SDL_SetRenderDrawColor(renderer, color, color, color, 0xFF);
    SDL_RenderDrawPoint(renderer, x, y);
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
    struct c8_renderer c8Renderer;  // TODO: replace by copying screenBuffer
    WORD opcode;
    SDL_Window   *window;
    SDL_Renderer *renderer;
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

    SDL_RenderSetScale(renderer, PIXEL_SCALE, PIXEL_SCALE);

    c8Renderer.clear = pixel_clear;
    c8Renderer.render = pixel_render;
    c8Renderer.set_pixel = pixel_set;
    c8Renderer.userData = (void**)&renderer;

    c8_reset(context, &c8Renderer);
    int rv = load_prgm(context, argc, argv);
    if (rv != 0 ) {
        cleanup(context, texture, surface, renderer, window);
        printf("load_prgm Error\n");
        SDL_Quit();
        return 1;
    }

    {
        SDL_Rect rect;
        rect.x = WINDOW_WIDTH;
        rect.y = 0;
        rect.w = 100;
        rect.y = WINDOW_HEIGHT;
        SDL_SetRenderDrawColor(renderer, 0xff, 0xf1, 0xff, 0xff);
        SDL_RenderFillRect(renderer, &rect);
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

    }

    cleanup(context, renderer, window);
    SDL_Quit();
    
    return 0;
}