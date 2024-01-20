#include "audio.hh"
#include <iostream>

// ref: https://stackoverflow.com/a/45002609
static void beep_callback(void *userdata, Uint8 *rawbuf, int bytes) {
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

Beeper::Beeper() {
    sample_nb        = 0;
    want.freq        = SAMPLE_RATE;
    want.format      = AUDIO_S16SYS;
    want.channels    = 1;
    want.samples     = 2048;
    want.callback    = beep_callback;
    want.userdata    = &sample_nb;
    timerID          = 0;
    is_opened        = true;

    if (SDL_OpenAudio(&want, &have) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to open audio: %s", SDL_GetError());
        is_opened = false;
    } else if (want.format != have.format) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to get the desired AudioSpec");
    }

    std::cout << "start" << std::endl;
}

Beeper::~Beeper() {
    if (is_opened) {
        SDL_RemoveTimer(timerID);
        SDL_CloseAudio();
        std::cout << "stop" << std::endl;
    }
}

void Beeper::beep(void *userdata) {
    Beeper &beeper = *(Beeper*)userdata;

    if (!beeper.is_opened) return;

    // play beep for 100 ms
    SDL_PauseAudio(0);
    beeper.timerID = SDL_AddTimer(100, beep_stop_callback, NULL);
}