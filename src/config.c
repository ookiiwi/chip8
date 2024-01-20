#include "config.h"
#include "ini.h"

#define MATCH(lhs, rhs) (strcmp(lhs, rhs) == 0)
#define MATCHV(s)       (MATCH(value, s)) 
#define BOOLEAN(value)  (MATCHV("true")|MATCHV("yes")|MATCHV("on")|MATCHV("1"))

static int config_handler(void *user, const char *section, const char *name, const char *value) {
    Config *config = (Config*)user;

    printf("found section: %s\n", section);
    if (strcmp(section, config->game_name) == 0) {
        if (MATCH(name, "wrapy")) {
            config->wrapy = BOOLEAN(value);
        } else if (MATCH(name, "clockspeed")) {
            config->clockspeed = atof(value);
        } else if (MATCH(name, "fps")) {
            config->fps = atof(value);
        } else {
            return -1;
        }
    }

    return 0;
}

int load_config(Config *config, const char *game_name, const char *path) {
    memset(config->game_name, 0, GAME_NAME_MAX_LEN);
    strncpy(config->game_name, game_name, GAME_NAME_MAX_LEN - 1); // leave last char as \0

    int rv = ini_parse(path, config_handler, (void*)config);
    if (rv < 0) {
        fprintf(stderr, "Failed to read config at %s\n", path);
    }

    return rv;
}
