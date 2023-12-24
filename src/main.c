#include "chip8.h"

#include <stdio.h>
#include <SDL.h>

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

    c8_reset(context);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;   
    }

    win = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
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
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    c8_destroy(context);
    
    return 0;
}