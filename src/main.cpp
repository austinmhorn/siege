#include "core/game.hpp"

#include <SDL3/SDL_main.h>

int main(int, char**) {
    siege::Game game;
    return game.run();
}

