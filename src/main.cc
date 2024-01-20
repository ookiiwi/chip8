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
#include <SDL_audio.h>
#include <math.h>
#include <stdio.h>
#include <sstream>
#include <cassert>
#include <vector>

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

#define TICK_COND(ticks, prev_ticks, speed, cond, statement) do {       \
    Uint64 delta = ticks - prev_ticks;                                  \
    if ((delta > (1000/speed)) && cond) {                               \
        statement                                                       \
        prev_ticks = ticks;                                             \
    }                                                                   \
} while (0);

#define TICK(ticks, prevTicks, speed, statement) TICK_COND(ticks, prevTicks, speed, 1, statement)

#define SET_KEY(keyword) do {                                           \
    int key = event.key.keysym.sym;                                     \
    if (key >= 0x30 && key <= 0x39)                 /* 0 to 9 */        \
        C8_##keyword##Key(context, key & 0x0F);                         \
    else if (key >= 0x60 && key <= 0x66)            /* A to F */        \
        C8_##keyword##Key(context, (key & 0x0F) + 0x9);                 \
} while(0);

void copy_c8_screenBuffer(SDL_Texture *texture, C8_Context *context) {
    void    *pixels;
    int      pitch;
    Uint32   *base;

    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    for(int row = 0; row < SCREEN_HEIGHT; ++row) {
        base = (Uint32*)((Uint8*)pixels + row * pitch);

        for(int col = 0; col < SCREEN_WIDTH; ++col) {
            int i = (row * SCREEN_WIDTH) + col;
            int color = (context->display[i] == 0 ? 0x00 : 0xFF);

            *base++ = (0xFF000000|(color << 16)|(color << 8)|color);
        }
    }

    SDL_UnlockTexture(texture);
}

const int AMPLITUDE = 28000;
const int SAMPLE_RATE = 44100;

struct Beeper {
    int              sample_nb;
    SDL_AudioSpec    want;
    SDL_AudioSpec    have;
    SDL_TimerID      timerID;
    bool             is_opened;
};

// ref: https://stackoverflow.com/a/45002609
void beep_callback(void *userdata, Uint8 *rawbuf, int bytes) {
    Sint16 *buffer = (Sint16*)rawbuf;
    int length = bytes / 2;
    int &sample_nb(*(int*)userdata);

    for (int i = 0; i < length; ++i, ++sample_nb) {
        double time = (double)sample_nb / (double)SAMPLE_RATE;
        buffer[i] = (Sint16)(AMPLITUDE * sin(2.0f * M_PI * 441.0f * time));
    }
}

Uint32 beep_stop_callback(Uint32 interval, void *param) {
    SDL_PauseAudio(1);
    return 0;
}

void beep(void *userdata) {
    Beeper &beeper = *(Beeper*)userdata;

    if (!beeper.is_opened) return;

    // play beep for 100 ms
    SDL_PauseAudio(0);
    beeper.timerID = SDL_AddTimer(100, beep_stop_callback, NULL);
}

int load_prgm(C8_Context *context, int argc, char **argv) {
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

    int rv = C8_LoadProgram(context, gamePath, configPath);
    C8_Error err = C8_GetError(context);

    if (err.err == C8_LOAD_CANNOT_OPEN_CONFIG) {
        fprintf(stderr, "Cannot open config: %s\n", configPath);
    }

    return rv;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    bool             bRunning;
    bool             c8_tick;
    int              opcode;
    C8_Context      _context;
    C8_Context      *context;
    C8_Profiler      profiler(_context);        // needs to be assigned here
    C8_Beeper        c8_beeper;
    Beeper           beeper;

    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_Rect         viewport;
    SDL_Rect         srcrect, dstrect;
    Uint64           ticks, prev_ticks, prev_draws, prev_timers_ticks;

    /* init components */
    bRunning            = true;
    c8_tick             = true;
    context             = &_context;
    ticks               = 0;
    prev_ticks          = 0;
    prev_draws          = 0;
    prev_timers_ticks   = 0;

    /* audio componenents */
    beeper.sample_nb        = 0;
    beeper.want.freq        = SAMPLE_RATE;
    beeper.want.format      = AUDIO_S16SYS;
    beeper.want.channels    = 1;
    beeper.want.samples     = 2048;
    beeper.want.callback    = beep_callback;
    beeper.want.userdata    = &beeper.sample_nb;
    beeper.timerID          = 0;
    beeper.is_opened        = true;

    c8_beeper.beep      = beep;
    c8_beeper.user_data = (void*)&beeper;

    if (SDL_OpenAudio(&beeper.want, &beeper.have) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to open audio: %s", SDL_GetError());
        beeper.is_opened = false;
    } else if (beeper.want.format != beeper.have.format) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to get the desired AudioSpec");
    }

    srcrect.w = SCREEN_WIDTH;
    srcrect.h = SCREEN_HEIGHT;
    srcrect.x = 0;
    srcrect.y = 0;
    dstrect.w = SCREEN_WIDTH  * PIXEL_SCALE;
    dstrect.h = SCREEN_HEIGHT * PIXEL_SCALE;
    dstrect.x = 0;
    dstrect.y = 0;

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

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
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

    C8_Reset(context, &c8_beeper);
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

    C8_ClearError(context);

    while(bRunning) {
        SDL_Event event;
        ticks   = SDL_GetTicks64();
        c8_tick = profiler.shouldStep();

        while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if( (SDL_QUIT == event.type) || 
                (SDL_KEYDOWN == event.type && SDLK_ESCAPE == event.key.keysym.sym) ) {
                bRunning = false;
                break;
            } else if (SDL_KEYDOWN == event.type) {
                SET_KEY(Set);
            } else if (SDL_KEYUP == event.type) {
                SET_KEY(Unset);
            }
        }

TICK_COND(ticks, prev_ticks, context->config.clockspeed, c8_tick,
        if ((opcode = C8_Tick(context)) <= 0) {
            bRunning = false;
            break;
        }
);

// Timers clocked at 60hz    
TICK_COND(ticks, prev_timers_ticks, 60.0, c8_tick,
        C8_UpdateTimers(context);
);

TICK(ticks, prev_draws, context->config.fps,
        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        profiler.render(c8_tick ? &opcode : nullptr);

        // Rendering
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_RenderClear(renderer);  // Needed as texture doesn't fill the renderer

        copy_c8_screenBuffer(texture, context);
        SDL_RenderCopy(renderer, texture, &srcrect, &dstrect);

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
);
    }

    if (beeper.is_opened) {
        SDL_RemoveTimer(beeper.timerID);
        SDL_CloseAudio();
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    cleanup(context, texture, renderer, window);
    SDL_Quit();
    
    return 0;
}