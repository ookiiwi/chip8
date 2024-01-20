#include "loader.hh"
#include "c8_def.h"

#include <cstddef>
#include <algorithm>
#include <cstdio>

static const char *help_msg = " \
Usage: chip8 <GAME_PATH> [CONFIG_PATH] \
";

int C8Loader::load(int argc, char **argv, Config &config, C8_Context &context) {
    if (m_parse_args(argc, argv) != 0) {
        fprintf(stderr, "Usage: %s <GAME_PATH> [CONFIG_PATH]\n", argv[0]);

        return -1;
    }
    
    
    int c_rv = m_load_config(config);
    int rv = m_load_prgm(config, context);

    if (rv < 0) {
        fprintf(stderr, "Failed to load");
        return rv;
    }

    printf("GAME: %s\n", game_name.c_str());
    printf("CONFIG [%s]:\n\tfps: %f\n\twrapy: %d\n\tclockspeed: %f\n", 
            (c_rv < 0 ? "DEFAULT" : config_path.c_str()), config.fps, config.wrapy, config.clockspeed);

    return 0;
}

int C8Loader::m_parse_args(int argc, char **argv) {
    std::size_t found;

    if (argc < 2) {
        return -1;
    }

    prgm_path   = argv[0];
    game_path   = argv[1];

    found = game_path.find_last_of("/\\");
    game_name = game_path.substr(found+1);

    if (argc >= 3) {
        config_path = argv[2];
    } else {
        config_path = game_path.substr(0, found+1) + "config.cfg";
    }

    return 0;
}

int C8Loader::m_load_config(Config &config) {
    // Set to default
    config.fps          = DEFAULT_FPS;
    config.wrapy        = DEFAULT_WRAPY;
    config.clockspeed   = DEFAULT_CLOCKSPEED;

    int rv = load_config(&config, game_name.c_str(), config_path.c_str());

    return rv;
}

int C8Loader::m_load_prgm(const Config &config, C8_Context &context) {
    int rv       =  C8_LoadProgram(&context, game_path.c_str());
    C8_Error err =  C8_GetError(&context);

    if (err.err == C8_LOAD_CANNOT_OPEN_CONFIG) {
        fprintf(stderr, "Cannot open config: %s\n", config_path.c_str());
    }

    context.config.wrapy = config.wrapy;

    return rv;
}
