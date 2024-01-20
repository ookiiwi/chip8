#ifndef LOADER_HH
#define LOADER_HH

#include <string>
#include "config.h"
#include "chip8.h"

class C8Loader {
public:
    C8Loader() = default;
    int load(int argc, char **argv, Config &config, C8_Context &context);

    std::string prgm_path;
    std::string game_path;
    std::string config_path;
    std::string game_name;

private:
    int     m_parse_args(int argc, char **argv);
    int     m_load_config(Config &config);
    int     m_load_prgm(const Config &config, C8_Context &context);
};

#endif