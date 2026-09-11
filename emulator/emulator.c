#include <SDL2/SDL.h>
#include <time.h>
#include "emulator.h"
#include "../shared/doom.h"
#include "../shared/bongo.h"

SDL_Window* window;
SDL_Event event;
SDL_Renderer* renderer;

int initSDL(SDL_Window** window, SDL_Renderer** renderer)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        printf("Failed to initialise\n");
        return 0;
    }

    *window = SDL_CreateWindow("QMK Emulator", 
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              EMULATOR_SCR_WIDTH,
                              EMULATOR_SCR_HEIGHT, 0);

    if (!*window)
    {
        printf("Failed to create window\n");
        return 0;
    }

    *renderer = SDL_CreateRenderer(*window, 0, 0);
    if (!*renderer)
    {
        printf("Failed to create renderer\n");
        return 0;
    }

    return 1;
}

int oled_write_pixel(int x, int y, bool white)
{
    // write_pixel(x, y, white);
    if (white) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    }

    SDL_RenderDrawPoint(renderer, x, y);
    return 1;
}

uint16_t timer_read() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t); // change CLOCK_MONOTONIC_RAW to CLOCK_MONOTONIC on non linux computers
    return t.tv_sec * 1000 + (t.tv_nsec + 500000) / 1000000;
}

uint32_t timer_read32() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t); // change CLOCK_MONOTONIC_RAW to CLOCK_MONOTONIC on non linux computers
    return t.tv_sec * 1000 + (t.tv_nsec + 500000) / 1000000;
}

uint32_t timer_elapsed32(uint32_t t) {
    return timer_read32() - t;
}

uint16_t timer_elapsed(uint16_t t) {
    return timer_read() - t;
}

int oled_set_cursor(int x, int y)
{
    return 1;
}

int oled_write(const char* str, int smth) {
    printf("%s\n", str);
    return 1;
}

int oled_write_P(const char* str, int smth) {
    return oled_write(str, smth);
}

int oled_write_raw_P(const char* c, size_t n)
{
    oled_write(c, n);
    return 1;
}

int oled_clear()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    return 1;
}

int get_current_wpm()
{
    return 1;
}

led_t host_keyboard_led_state()
{
    led_t res = { 1 };
    return res;
}

const char* get_u8_str(uint8_t val, char pad) {
    static char buf[4] = {0};
    snprintf(buf, 4, "%d", val);
    return buf;
};

const char* get_u16_str(uint16_t val, char pad) {
    static char buf[6] = {0};
    snprintf(buf, 6, "%d", val);
    return buf;
};

uint8_t pgm_read_byte(const void* addr)
{
    return *(uint8_t*) addr;
}

void render() {
    SDL_RenderPresent(renderer);
}

int main()
{
   if (!initSDL(&window, &renderer)) 
    {
        printf("Failed to initialise SDL!\n");
        goto clean;
    }

    int screen_mode = DOOM;
    doom_setup();
    controls doom_inputs = { 0, 0, 0, 0, 0 };
    while (1)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    goto clean;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_UP) {
                        // doom_update(doom_inputs);
                        // SDL_RenderPresent(renderer);
                        // SDL_Delay(100);
                        doom_inputs.u = true;
                    }

                    else if (event.key.keysym.sym == SDLK_DOWN)
                        doom_inputs.d = true;

                    else if (event.key.keysym.sym == SDLK_LEFT)
                        doom_inputs.l = true;

                    else if (event.key.keysym.sym == SDLK_RIGHT)
                        doom_inputs.r = true;

                    else if (event.key.keysym.sym == SDLK_SPACE)
                        doom_inputs.shoot = true;

                    else if (event.key.keysym.sym == SDLK_r)
                        doom_setup();

                    else if (event.key.keysym.sym == SDLK_ESCAPE)
                        screen_mode = (screen_mode + 1) % 3;
                    
                    break;

            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_UP)
                    doom_inputs.u = false;

                else if (event.key.keysym.sym == SDLK_DOWN)
                    doom_inputs.d = false;

                else if (event.key.keysym.sym == SDLK_LEFT)
                    doom_inputs.l = false;

                else if (event.key.keysym.sym == SDLK_RIGHT)
                    doom_inputs.r = false;

                else if (event.key.keysym.sym == SDLK_SPACE)
                    doom_inputs.shoot = false;
                break;
            }
        }

        switch (screen_mode) {
            case CAT:
                bongo_update();
                break;
    
            case DOOM:
                doom_update(doom_inputs);
                break;
            
            default:
                break;
        }
    }   

clean:
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return 0;
}