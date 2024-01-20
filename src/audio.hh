#ifndef AUDIO_HH
#define AUDIO_HH

#include <SDL.h>
#include <SDL_audio.h>

const int AMPLITUDE = 28000;
const int SAMPLE_RATE = 44100;

class Beeper {
public:
    Beeper();
    ~Beeper();

    static void beep(void *userdata);
public:
    int              sample_nb;
    SDL_AudioSpec    want;
    SDL_AudioSpec    have;
    SDL_TimerID      timerID;
    bool             is_opened;
};


#endif