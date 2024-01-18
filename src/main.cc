#include "chip8.h"
#include "c8_disassembler.h"
#include "c8_def.h"
#include "util.h"
#include "c8_profiler.hh"
#include "cleanup.hh"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL.h>
#include <stdio.h>
#include <sstream>
#include <cassert>
#include <vector>

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

void copy_c8_screenBuffer(SDL_Texture *texture, chip8_t *context) {
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
                    base = ((Uint8*)pixels) + (4 * ((y + j) * VIEWPORT_WIDTH + x + i));
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

int load_prgm(chip8_t *context, int argc, char **argv) {
    std::string dummy;
    const char *prgmPath, *gamePath, *configPath;

    if (argc < 2) {
        return 2;
    }

    prgmPath = argv[0];
    gamePath = argv[1];

    if (argc >= 3) {
        configPath = argv[2];
    } else {
        // TODO: handle any OS path
        int i = last_index(gamePath, '/');

        if (i > 0) {
            dummy = std::string(gamePath, i+1) + "config.cfg";
            configPath = dummy.c_str();
        } else {
            configPath = NULL;
        }
    }

    int rv = c8_load_prgm(context, gamePath, configPath);
    c8_error_t err = c8_get_error(context);

    if (err.err == C8_LOAD_CANNOT_OPEN_CONFIG) {
        fprintf(stderr, "Cannot open config: %s\n", configPath);
    }

    return rv;
}

int main(int argc, char** argv) {
    int              bRunning, c8ShouldTick;
    chip8_t         _context;
    chip8_t         *context;
    int              opcode;
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_Rect         viewport;
    Uint64           prevTicks, ticks, delta;
    C8_Profiler      profiler(_context);        // needs to be assigned here

    /* init components */
    bRunning = 1;
    context = &_context;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;   
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, windowFlags);
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        cleanup(window);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    if (texture == NULL) {
        cleanup(renderer, window);
        printf("SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    c8_reset(context);
    int rv = load_prgm(context, argc, argv);
    if (rv != 0) {
        cleanup(context, texture, renderer, window);
        fprintf(stderr, "load_prgm Error: %d\n", rv);
        SDL_Quit();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    viewport.x = 0;
    viewport.y = 0;
    viewport.w = VIEWPORT_WIDTH;
    viewport.h = VIEWPORT_HEIGHT;

    c8_clr_error(context);

    while(bRunning) {
        SDL_Event event;
        ticks = SDL_GetTicks64();
        delta = ticks - prevTicks;
        c8ShouldTick = (delta > (1000/600.0)) && profiler.shouldStep();

        while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if( (SDL_QUIT == event.type) || 
                (SDL_KEYDOWN == event.type && SDLK_ESCAPE == event.key.keysym.sym) ) {
                bRunning = 0;
                break;
            } else if (SDL_KEYDOWN == event.type) {
                int key = event.key.keysym.sym;

                if (key >= 0x30 && key <= 0x39)                 // 0 to 9
                    context->keyPressed = key & 0x0F;
                else if (key >= 0x60 && key <= 0x66)            // A to F
                    context->keyPressed = (key & 0x0F) + 0x9;
            } else if (SDL_KEYUP == event.type) {
                context->keyPressed = -1;
            }
        }

        if (c8ShouldTick) {
            if ((opcode = c8_tick(context)) <= 0) {
                bRunning = false;
                break;
            }

            prevTicks = ticks;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        profiler.render(c8ShouldTick ? &opcode : nullptr);

        // Rendering
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_RenderClear(renderer);  // Needed as texture doesn't fill the renderer

        copy_c8_screenBuffer(texture, context);
        SDL_RenderCopy(renderer, texture, NULL, &viewport);

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    cleanup(context, texture, renderer, window);
    SDL_Quit();
    
    return 0;
}