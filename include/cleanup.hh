/**
 * Credit: https://www.willusher.io/sdl2%20tutorials/2014/08/01/postscript-1-easy-cleanup
*/

#ifndef CLEANUP_HH
#define CLEANUP_HH

#include <utility>
#include <SDL.h>
#include "chip8.h"

template<typename T, typename... Args>
void cleanup(T *t, Args&&... args) {
    cleanup(t);
    cleanup(std::forward<Args>(args)...);
}

template<>
inline void cleanup<SDL_Window>(SDL_Window *window) {
    if (!window)    return;
    SDL_DestroyWindow(window);
}

template<>
inline void cleanup<SDL_Renderer>(SDL_Renderer *renderer) {
    if (!renderer)    return;
    SDL_DestroyRenderer(renderer);
}

template<>
inline void cleanup<SDL_Texture>(SDL_Texture *texture) {
    if (!texture)    return;
    SDL_DestroyTexture(texture);
}

template<>
inline void cleanup<SDL_Surface>(SDL_Surface *surface) {
    if (!surface)    return;
    SDL_FreeSurface(surface);
}

template<> void cleanup<chip8_t>(chip8_t *context) {
    if (!context)   return;
    c8_destroy(context);
}

#endif