/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <chip8.h>
#include <sys/time.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_AudioDeviceID audio_device;

#define PIXEL_SIZE 5

#define CLOCK_HZ 60
#define CLOCK_RATE_MS ((int) ((1.0 / CLOCK_HZ) * 1000 + 0.5))

#define BLACK 0
#define WHITE 255

#define SCREEN_ROWS (CHIP8_SCREEN_HEIGHT * PIXEL_SIZE)
#define SCREEN_COLS (CHIP8_SCREEN_WIDTH * PIXEL_SIZE)

static uint32_t pixels[CHIP8_SCREEN_HEIGHT * CHIP8_SCREEN_WIDTH]; // 像素缓冲区

extern uint8_t keypad[];
extern uint8_t frame_buffer[CHIP8_SCREEN_HEIGHT][CHIP8_SCREEN_WIDTH];
extern bool draw_flag;

struct timeval clock_prev;

void AudioCallback(void *userdata, Uint8 *stream, int len) {
    static int tone_phase = 0;
    const int amplitude = 28000; // 方波振幅
    
    Sint16 *samples = (Sint16*)stream;
    len /= sizeof(Sint16);
    
    for(int i = 0; i < len; i++) {
        samples[i] = (tone_phase < 100) ? amplitude : -amplitude; // 方波
        tone_phase = (tone_phase + 1) % 200; // 440Hz方波（假设采样率44100）
    }
}

int timediff_ms(struct timeval *end, struct timeval *start) {
    int diff =  (end->tv_sec - start->tv_sec) * 1000 +
                (end->tv_usec - start->tv_usec) / 1000;
    //printf("timediff = %d\n", diff);
    return diff;
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
#if 0
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", 320, 240, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
#else
    // 初始化窗口和渲染器
    window = SDL_CreateWindow("Chip8 Emulator", 
        CHIP8_SCREEN_WIDTH * PIXEL_SIZE, 
        CHIP8_SCREEN_HEIGHT * PIXEL_SIZE, 
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // 创建纹理用于像素渲染（RGBA8888格式）
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                               SDL_TEXTUREACCESS_STREAMING, 
                               CHIP8_SCREEN_WIDTH, CHIP8_SCREEN_HEIGHT);
    if (!texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // 初始化音频设备
    SDL_AudioSpec spec = {
        .freq = 44100,
        .format = SDL_AUDIO_S16,
        .channels = 1,
    };
#if 0
    audio_device = SDL_OpenAudioDevice(0, &spec);
    if (!audio_device) {
        SDL_Log("Couldn't open audio: %s", SDL_GetError());
    }
#endif
    
    // 初始化像素缓冲区为黑色
    memset(pixels, 0, sizeof(pixels));

    // 初始化Chip8
    chip8_init();
    // 加载ROM
    if (argc > 1) {
        chip8_load_game(argv[1]);
    }else {
        printf("Usage: %s <rom_path>\n", argv[0]);
        return SDL_APP_FAILURE;
    }
    
#endif

    return SDL_APP_CONTINUE;
}

int keymap(unsigned char k) {
    switch (k) {
        case '1': return 0x1;
        case '2': return 0x2;
        case '3': return 0x3;
        case '4': return 0xc;

        case 'q': return 0x4;
        case 'w': return 0x5;
        case 'e': return 0x6;
        case 'r': return 0xd;

        case 'a': return 0x7;
        case 's': return 0x8;
        case 'd': return 0x9;
        case 'f': return 0xe;
                  
        case 'z': return 0xa;
        case 'x': return 0x0;
        case 'c': return 0xb;
        case 'v': return 0xf;

        default:  return -1;
    }
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
        // 处理键盘事件
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        SDL_Keycode key = event->key.key;
        // 查找键位映射
        for (int i = 0; i < 16; i++) {
            if (key == keymap(i)) {
                keypad[i] = (event->type == SDL_EVENT_KEY_DOWN) ? 1 : 0;
                break;
            }
        }
    }

    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
#if 0
    const char *message = "Hello World!";
    int w = 0, h = 0;
    float x, y;
    const float scale = 2.0f;

    /* Center the message and scale it up */
    SDL_GetRenderOutputSize(renderer, &w, &h);
    SDL_SetRenderScale(renderer, scale, scale);
    x = ((w / scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(message)) / 2;
    y = ((h / scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2;

    /* Draw the message */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, x, y, message);
    SDL_RenderPresent(renderer);
#endif
    struct timeval clock_now;
    gettimeofday(&clock_now, NULL);

    chip8_emulate_cycle();

    if (draw_flag) {
        // draw();
        draw_flag = false;
    }

    if (timediff_ms(&clock_now, &clock_prev) >= CLOCK_RATE_MS) {
        chip8_tick();
        clock_prev = clock_now;
    }

    
    // 更新像素缓冲区
    // UpdateDisplay(dummy_display);
    
    // 更新纹理并渲染
    SDL_UpdateTexture(texture, NULL, pixels, CHIP8_SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    // 控制声音（假设有一个sound_timer变量）
    static int sound_timer = 0;
    if (sound_timer > 0) {
        // SDL_PlayAudioDevice(audio_device);
        sound_timer--;
    } else {
        SDL_PauseAudioDevice(audio_device);
    }

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}
