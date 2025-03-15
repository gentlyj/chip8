#include "chip8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define unknown_opcode(op)                                                     \
    do                                                                         \
    {                                                                          \
        fprintf(stderr, "Unknown opcode: 0x%x\n", op);                         \
        fprintf(stderr, "kk: 0x%02x\n", kk);                                   \
        exit(42);                                                              \
    } while (0)

#ifdef DEBUG
#define p(...) printf(__VA_ARGS__);
#else
#define p(...)
#endif

#define IS_BIT_SET(byte, bit) (((0x80 >> (bit)) & (byte)) != 0x0)

#define FONTSET_ADDRESS 0x00
#define FONTSET_BYTES_PER_CHAR 5
unsigned char chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

uint16_t opcode;
uint8_t memory[CHIP8_MEMORY_SIZE];
uint8_t V[CHIP8_REGISTER_COUNT]; // 16 registers, V0-VF. VF is the carry flag.
uint16_t I;  // Index register. Used to store memory addresses.
uint16_t PC; // Program counter. Used to store the address of the next
             // instruction to be executed.
uint8_t frame_buffer[CHIP_SCREEN_HEIGHT]
                    [CHIP_SCREEN_WIDTH]; // 64x32 pixels. 1 bit per pixel. 0 is
                                         // black, 1 is white.
uint8_t delay_timer; // 8-bit timer. Decrements at a rate of 60Hz.
uint8_t sound_timer; // 8-bit timer. Beeps when it reaches 0. Decrements at a
                     // rate of 60Hz.
uint16_t stack[CHIP8_STACK_SIZE]; // 16-level stack. Used to store the address
                                  // of the next instruction to be executed.
uint16_t SP; // Stack pointer. Used to store the address of the next instruction
             // to be executed.
uint8_t keypad[CHIP8_KEY_SIZE]; // 16 keys. 0-9, A-F. 0 is not pressed, 1 is
                                // pressed.
bool draw_flag; // Set to true when the screen should be drawn. Cleared when the
                // screen is drawn.

void chip8_init()
{
    int i;

    PC = CHIP8_PROGRAM_START_ADDRESS; // Start at address 0x200.
    opcode = 0;                       // Reset the opcode.
    I = 0;                            // Reset the index register.
    SP = 0;                           // Reset the stack pointer.
    delay_timer = 0;                  // Reset the delay timer.

    memset(memory, 0, sizeof(memory));             // Clear the memory.
    memset(V, 0, sizeof(V));                       // Clear the registers.
    memset(stack, 0, sizeof(stack));               // Clear the stack.
    memset(keypad, 0, sizeof(keypad));             // Clear the keypad.
    memset(frame_buffer, 0, sizeof(frame_buffer)); // Clear the frame buffer.
    draw_flag = false;                             // Clear the draw flag.

    for (i = 0; i < sizeof(chip8_fontset); i++)
    {
        memory[FONTSET_ADDRESS + i] = chip8_fontset[i];
    }

    draw_flag = true;
    delay_timer = 0;   // Reset the delay timer.
    sound_timer = 0;   // Reset the sound timer.
    srand(time(NULL)); // Seed the random number generator.
}

void chip8_loadgame(char *game)
{
    FILE *fgame;

    fgame = fopen(game, "rb");

    if (NULL == fgame)
    {
        fprintf(stderr, "Unable to open game: %s\n", game);
        exit(42);
    }

    fread(&memory[0x200], 1, MAX_GAME_SIZE, fgame);

    fclose(fgame);
}

void chip8_emulate_cycle()
{
    uint8_t x;
    uint8_t y;
    uint8_t n;
    uint16_t nnn;
    uint8_t kk;
    int i;

    opcode = memory[PC] << 8 | memory[PC + 1]; // Fetch the opcode.
    x = (opcode & 0x0F00) >> 8;                // Get the x register.
    y = (opcode & 0x00F0) >> 4;                // Get the y register.
    n = (opcode & 0x000F);                     // Get the n register.
    nnn = opcode & 0x0FFF;                     // Get the nnn register.
    kk = opcode & 0x00FF;                      // Get the kk register.

    p("opcode: 0x%04x\n", opcode);

    switch (opcode & 0xF000)
    {
    case 0x0000:
        switch (kk)
        {
        case 0x00E0: // 00E0 - CLS - Clear the display.
            memset(frame_buffer, 0, sizeof(frame_buffer));
            draw_flag = true;
            PC += 2; // Increment the program counter by 2.
            break;
        case 0x00EE:          // 00EE - RET - Return from a subroutine.
            PC = stack[--SP]; // Pop the stack and set the program counter to
                              // the value.
            break;
        default: // 0nnn - SYS addr - Jump to a machine code routine at nnn.
            unknown_opcode(opcode); // Unknown opcode.
            break;
        }
        break;
    case 0x1000:  // 1nnn - JP addr - Jump to location nnn.
        PC = nnn; // Set the program counter to the value of nnn.
        break;
    case 0x2000:              // 2nnn - CALL addr - Call subroutine at nnn.
        stack[SP++] = PC + 2; // Push the current program counter to the stack
                              // and increment the stack pointer.
        PC = nnn;             // Set the program counter to the value of nnn.
        break;
    case 0x3000: // 3xkk - SE Vx, byte - Skip next instruction if Vx = kk.
        if (V[x] == kk)
        {            // If the value of Vx is equal to kk.
            PC += 2; // Increment the program counter by 2.
        }
        else
        {            // If the value of Vx is not equal to kk.
            PC += 4; // Increment the program counter by 4.
        }
        break;
    case 0x4000: // 4xkk - SNE Vx, byte - Skip next instruction if Vx != kk.
        if (V[x] != kk)
        {            // If the value of Vx is not equal to kk.
            PC += 2; // Increment the program counter by 2.
        }
        else
        {
            PC += 4; // Increment the program counter by 4.
        }
        break;
    case 0x5000: // 5xy0 - SE Vx, Vy - Skip next instruction if Vx = Vy.
        if (V[x] == V[y])
        {
            PC += 2; // Increment the program counter by 2.
        }
        else
        {
            PC += 4; // Increment the program counter by 4.
        }
        break;
    case 0x6000:   // 6xkk - LD Vx, byte - Set Vx = kk.
        V[x] = kk; // Set the value of Vx to kk.
        PC += 2;   // Increment the program counter by 2.
        break;
    case 0x7000:    // 7xkk - ADD Vx, byte - Set Vx = Vx + kk.
        V[x] += kk; // Set the value of Vx to Vx + kk.
        PC += 2;    // Increment the program counter by 2.
        break;
    case 0x8000: // 8xy0 - LD Vx, Vy - Set Vx = Vy.
        switch (n)
        {
        case 0x0000:
            V[x] = V[y]; // Set the value of Vx to Vy.
            PC += 2;     // Increment the program counter by 2.
            break;
        case 0x0001:      // 8xy1 - OR Vx, Vy - Set Vx = Vx | Vy.
            V[x] |= V[y]; // Set the value of Vx to Vx | Vy.
            PC += 2;      // Increment the program counter by 2.
            break;
        case 0x0002:      // 8xy2 - AND Vx, Vy - Set Vx = Vx & Vy.
            V[x] &= V[y]; // Set the value of Vx to Vx & Vy.
            PC += 2;      // Increment the program counter by 2.
            break;
        case 0x0003:      // 8xy3 - XOR Vx, Vy - Set Vx = Vx ^ Vy.
            V[x] ^= V[y]; // Set the value of Vx to Vx ^ Vy.
            PC += 2;      // Increment the program counter by 2.
            break;
        case 0x0004: // 8xy4 - ADD Vx, Vy - Set Vx = Vx + Vy, set VF = carry.
            if (V[y] > (0xFF - V[x]))
            { // If the value of Vy is greater than the maximum value that can
              // be stored in Vx.
                V[0xF] = 1; // Set the value of VF to 1.
            }
            else
            { // If the value of Vy is less than or equal to the maximum value
              // that can be stored in Vx.
                V[0xF] = 0; // Set the value of VF to 0.
            }
            V[x] += V[y]; // Set the value of Vx to Vx + Vy.
            PC += 2;      // Increment the program counter by 2.
            break;
        case 0x0005: // 8xy5 - SUB Vx, Vy - Set Vx = Vx - Vy, set VF = NOT
                     // borrow.
            if (V[x] > V[y])
            {               // If the value of Vx is greater than Vy.
                V[0xF] = 1; // Set the value of VF to 1.
            }
            else
            {               // If the value of Vx is less than or equal to Vy.
                V[0xF] = 0; // Set the value of VF to 0.
            }
            V[x] -= V[y]; // Set the value of Vx to Vx - Vy.
            PC += 2;      // Increment the program counter by 2.
            break;
        case 0x0006: // 8xy6 - SHR Vx {, Vy} - Set Vx = Vx SHR 1.
            V[0xF] = V[x] & 0x1;
            V[x] = V[x] >> 1; // Set the value of Vx to Vx SHR 1.
            PC += 2;          // Increment the program counter by 2.
            break;
        case 0x0007: // 8xy7 - SUBN Vx, Vy - Set Vx = Vy - Vx, set VF = NOT
                     // borrow.
            if (V[y] > V[x])
            {               // If the value of Vy is greater than Vx.
                V[0xF] = 1; // Set the value of VF to 1.
            }
            else
            {               // If the value of Vy is less than or equal to Vx.
                V[0xF] = 0; // Set the value of VF to 0.
            }
            V[x] = V[y] - V[x]; // Set the value of Vx to Vy - Vx.
            PC += 2;            // Increment the program counter by 2.
            break;
        case 0x000E: // 8xyE - SHL Vx {, Vy} - Set Vx = Vx SHL 1.
            V[0xF] = V[x] & 0x01;
            V[x] = V[x] << 1; // Set the value of Vx to Vx SHL 1.
            PC += 2;          // Increment the program counter by 2.
            break;
        default:                    // Unknown opcode.
            unknown_opcode(opcode); // Unknown opcode.
            break;
        }

        break;
    case 0x9000: // 9xy0 - SNE Vx, Vy - Skip next instruction if Vx != Vy.
        if (V[x] != V[y])
        {            // If the value of Vx is not equal to Vy.
            PC += 2; // Increment the program counter by 2.
        }
        break;
    case 0xA000: // Annn - LD I, addr - Set I = nnn.
        I = nnn; // Set the value of I to nnn.
        PC += 2; // Increment the program counter by 2.
        break;
    case 0xB000:         // Bnnn - JP V0, addr - Jump to location nnn + V0.
        PC = nnn + V[0]; // Set the program counter to nnn + V0.
        break;
    case 0xC000: // Cxkk - RND Vx, byte - Set Vx = random byte AND kk.
        V[x] = (rand() % 0xFF) & kk; // Set the value of Vx to a random byte AND
                                     // kk.
        PC += 2;                     // Increment the program counter by 2.
        break;
    case 0xD000:
    #if 0
        draw_sprite(V[x], V[y], n); // Draw a sprite at location (Vx, Vy) with a
                                    // height of n.
    #endif
        PC += 2;                    // Increment the program counter by 2.
        draw_flag = true;           // Set the draw flag to true.
        break;
    case 0xE000: // Ex9E - SKP Vx - Skip next instruction if key with the value
                 // of Vx is pressed .
        switch (kk)
            {
            case 0x9E: // Ex9E - SKP Vx - Skip next instruction if key with the
                       // value of Vx is pressed.
                if (keypad[V[x]] == 1)
                {            // If the key with the value of Vx is pressed.
                    PC += 2; // Increment the program counter by 2.
                }
                break;
            case 0xA1: // ExA1 - SKNP Vx - Skip next instruction if key with the
                       // value of Vx is not pressed.
                if (keypad[V[x]] == 0)
                {            // If the key with the value of Vx is not pressed.
                    PC += 2; // Increment the program counter by 2.
                }
                break;
            }
        break;
    case 0xF000: // Fx07 - LD Vx, DT - Set Vx = delay timer value.
        switch (kk)
        {
        case 0x07: // Fx07 - LD Vx, DT - Set Vx = delay timer value.
            V[x] = delay_timer; // Set the value of Vx to the value of the
                                // delay timer.
            PC += 2;            // Increment the program counter by 2.
            break;
        case 0x0A: // Fx0A - LD Vx, K - Wait for a key press, store the value
                   // of the key in Vx.
            while (true)
            {

                for (i = 0; i < CHIP8_KEY_SIZE; i++)
                {
                    if (keypad[i] == 1)
                    { // If the key with the value of Vx is pressed.
                        V[x] = i;
                        goto got_key_press;
                    }
                }
            }
        got_key_press:
            PC += 2; // Increment the program counter by 2.
            break;
        case 0x15:              // Fx15 - LD DT, Vx - Set delay timer = Vx.
            delay_timer = V[x]; // Set the value of the delay timer to the
                                // value of Vx.
            PC += 2;            // Increment the program counter by 2.
            break;
        case 0x18:              // Fx18 - LD ST, Vx - Set sound timer = Vx.
            sound_timer = V[x]; // Set the value of the sound timer to the
                                // value of Vx.
            PC += 2;            // Increment the program counter by 2.
            break;
        case 0x1E:     // Fx1E - ADD I, Vx - Set I = I + Vx.
            I += V[x]; // Set the value of I to I + Vx.
            PC += 2;   // Increment the program counter by 2.
            break;
        case 0x29: // Fx29 - LD F, Vx - Set I = location of sprite for digit Vx.
            I = V[x] * FONTSET_BYTES_PER_CHAR; // Set the value of I to the
                                               // location of the sprite for the
                                               // digit Vx.

            PC += 2; // Increment the program counter by 2.
            break;
        case 0x33: // Fx33 - LD B, Vx - Store BCD representation of Vx in memory
                   // locations I, I+1, and I+2.
            memory[I] = V[x] / 100; // Store the hundreds digit of Vx in memory
                                    // location I.

            memory[I + 1] = (V[x] / 10) % 10; // Store the tens digit of Vx in
                                              // memory location I+1.

            memory[I + 2] = V[x] % 10; // Store the ones digit of Vx in memory
                                       // location I+2.

            PC += 2; // Increment the program counter by 2.
            break;
        case 0x55: // Fx55 - LD [I], Vx - Store registers V0 through Vx in
                   // memory starting at location I.
            for (i = 0; i <= x; i++)
            {
                memory[I + i] = V[i];
            }
            I += x + 1; // Increment the value of I by x+1.
            PC += 2;    // Increment the program counter by 2.
            break;
        case 0x65: // Fx65 - LD Vx, [I] - Read registers V0 through Vx from
                   // memory starting at location I.
            for (i = 0; i <= x; i++)
            {
                V[i] = memory[I + i];
            }
            I += x + 1; // Increment the value of I by x+1.
            PC += 2;    // Increment the program counter by 2.
            break;
        default:                    // Unknown opcode.
            unknown_opcode(opcode); // Unknown opcode.
            break;
        }
        break;
    default:                    // Unknown opcode.
        unknown_opcode(opcode); // Unknown opcode.
        break;
    }
}

void chip8_tick(){
    if (delay_timer > 0) {
        --delay_timer;// Decrement the delay timer. 
    }
    if (sound_timer > 0) {
        --sound_timer; // Decrement the sound timer.
        if(sound_timer == 0){
            // TODO: Beep! 
        } 
    }
}