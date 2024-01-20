#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdlib.h>

#define GAME_NAME_MAX_LEN 63

typedef struct {
    char     game_name[GAME_NAME_MAX_LEN];
    int      wrapy;
    float    clockspeed;
    float    fps;
} Config;

int load_config(Config *config, const char *game_name, const char *path);

#ifdef __cplusplus
}
#endif

#endif