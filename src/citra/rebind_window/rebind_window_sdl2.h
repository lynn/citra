// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>
#include "common/emu_window.h"

struct SDL_Window;

class RebindWindow_SDL2 {
public:
    RebindWindow_SDL2(int modifying_index, const std::string& config_filename);
    ~RebindWindow_SDL2();

    /// Polls window events
    void PollEvents();

    /// Run until the window is closed
    void Run();

    /// Render the keybind editor UI to `renderer`
    void Render();

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    /// Load keymap from configuration
    void ReloadSetKeymaps();

private:
    /// Called by PollEvents when a key is pressed.
    void OnKeyDown(int key);

    /// Which button are we modifying?
    int modifying_index;

    /// Where is the config file we're modifying?
    std::string config_filename;

    /// Is the window still open?
    bool is_open = true;

    /// Internal SDL2 render window
    SDL_Window* render_window;

    /// Internal SDL2 renderer
    SDL_Renderer* renderer;
};
