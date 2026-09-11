#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define PSTR(a) (a)

#define PROGMEM
#define QK_KB_0 1
#define keyrecord_t

#ifdef RENDER_DEBUG
    #define EMULATOR_SCR_WIDTH 700
    #define EMULATOR_SCR_HEIGHT 700
#else
    #define EMULATOR_SCR_WIDTH 128  
    #define EMULATOR_SCR_HEIGHT 64
#endif

enum oled_state {
    OFF,
    CAT,
    DOOM
};

typedef struct led_t {
    int caps_lock;
} led_t;

static led_t led_usb_state = {1};
// static uint8_t curr_wpm = 0;

void render();
int oled_write_pixel(int, int, bool);
uint16_t timer_read();
uint32_t timer_read32();
uint16_t timer_elapsed(uint16_t);
uint32_t timer_elapsed32(uint32_t);
int oled_set_cursor(int, int);
int oled_write(const char*, int);
int oled_write_P(const char*, int);
int oled_write_raw_P(const char*, size_t);
int oled_clear();
int get_current_wpm();
led_t host_keyboard_led_state();
const char* get_u8_str(uint8_t, char);
const char* get_u16_str(uint16_t, char);
uint8_t pgm_read_byte(const void*);