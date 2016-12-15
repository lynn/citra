// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// #include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "citra/rebind_window/rebind_window_sdl2.h"
// #include "common/key_map.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
// #include "core/hle/service/hid/hid.h"
#include "citra/config.h"
#include "core/settings.h"
// #include "video_core/video_core.h"

const u32 fps = 60;
const u32 minimum_frame_time = 1000 / fps;
const int REBIND_WINDOW_WIDTH = 640;
const int REBIND_WINDOW_HEIGHT = 480;

void RebindWindow_SDL2::OnKeyDown(int key) {
    const char* key_name = Settings::NativeInput::Mapping[modifying_index];
    std::string scancode_name = SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
    std::cout << "Rebound " << key_name << " to " << scancode_name << "." << std::endl;
    FileUtil::SetINIKey(config_filename, "Controls", key_name, std::to_string(key));
    is_open = false;
}

bool RebindWindow_SDL2::IsOpen() const {
    return is_open;
}

RebindWindow_SDL2::RebindWindow_SDL2(int modifying_index, const std::string& config_filename)
    : modifying_index(modifying_index), config_filename(config_filename) {
    SDL_SetMainReady();

    // Initialize the window
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL2! Exiting...");
        exit(1);
    }

    std::string window_title = Common::StringFromFormat("Citra | %s-%s | Rebinding keys",
                                                        Common::g_scm_branch, Common::g_scm_desc);

    render_window = SDL_CreateWindow(window_title.c_str(),
                                     SDL_WINDOWPOS_UNDEFINED, // x position
                                     SDL_WINDOWPOS_UNDEFINED, // y position
                                     REBIND_WINDOW_WIDTH, REBIND_WINDOW_HEIGHT, SDL_WINDOW_SHOWN);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window! Exiting...");
        exit(1);
    }

    renderer = SDL_CreateRenderer(render_window, -1, SDL_RENDERER_ACCELERATED);

    if (renderer == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 renderer! Exiting...");
        exit(1);
    }
}

RebindWindow_SDL2::~RebindWindow_SDL2() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(render_window);
    SDL_Quit();
}

void RebindWindow_SDL2::PollEvents() {
    SDL_Event event;

    while (SDL_PollEvent(&event) && is_open) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                is_open = false;
            }
            break;
        case SDL_KEYDOWN:
            OnKeyDown(static_cast<int>(event.key.keysym.scancode));
            break;
        case SDL_QUIT:
            is_open = false;
            break;
        }
    }
}

void RebindWindow_SDL2::Render() {
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(renderer);
    // TODO: Ideally, this window should be an interactive editor, but at the very least,
    // display some sort of "press a key" text here. This requires a text rendering library
    // though, see issue #2313.
}

void RebindWindow_SDL2::Run() {
    Uint32 last_frame_time = 0;
    Uint32 frame_time;
    Uint32 time_delta;

    int current = Settings::values.input_mappings[Settings::NativeInput::All[modifying_index]];
    std::cout << "Press a key to bind to " << Settings::NativeInput::Mapping[modifying_index]
              << " (currently " << SDL_GetScancodeName(static_cast<SDL_Scancode>(current)) << ")..."
              << std::endl;

    while (is_open) {
        frame_time = SDL_GetTicks();

        PollEvents();

        time_delta = frame_time - last_frame_time;
        last_frame_time = frame_time;

        Render();
        SDL_RenderPresent(renderer);

        // Make sure each frame is at least `minimum_frame_time` milliseconds long.
        Uint32 ms = std::min(minimum_frame_time - (SDL_GetTicks() - frame_time), 0U);
        SDL_Delay(ms);
    }
}