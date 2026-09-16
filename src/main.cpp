#include "core/game.hpp"
#include "core/launch_options.hpp"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <string_view>
#include <vector>

int main(const int argc, char** argv) {
    std::vector<std::string_view> arguments;
    if (argc > 1) {
        arguments.reserve(static_cast<std::size_t>(argc - 1));
    }
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    const siege::LaunchOptionsResult parsed =
        siege::parse_launch_options(arguments);
    if (!parsed) {
        std::fprintf(stderr,
                     "siege: %s\nusage: siege [--ai-difficulty easy|medium|hard] "
                     "[--ai-playstyle balanced|aggressive|defensive]\n",
                     parsed.error.c_str());
        return 2;
    }
    siege::Game game{siege::make_ai_profile(parsed.options.ai_difficulty,
                                            parsed.options.ai_playstyle)};
    return game.run();
}
