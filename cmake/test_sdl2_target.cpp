#include <SDL.h>

int main() {
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        return 1;
    }
    SDL_Quit();
    return 0;
}