#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdbool.h>

#define CHIP8_MEMORY_SIZE 4096
#define CHIP8_REGISTER_COUNT 16
#define CHIP8_STACK_SIZE 16
#define CHIP8_KEY_SIZE 16
#define CHIP_SCREEN_WIDTH 64
#define CHIP_SCREEN_HEIGHT 32
#define CHIP8_SCREEN_SIZE (CHIP_SCREEN_WIDTH * CHIP_SCREEN_HEIGHT)
#define CHIP8_PIXEL_SIZE 4
#define CHIP8_FONTSET_SIZE 80
#define CHIP8_PROGRAM_START_ADDRESS 0x200
#define MAX_GAME_SIZE (CHIP8_MEMORY_SIZE - CHIP8_PROGRAM_START_ADDRESS)

void chip8_init();
void chip8_load_rom(const char *rom_path);
void chip8_emulate_cycle();
void chip8_set_key(uint8_t key, bool state);
void chip8_tick();

#endif