// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <memory>
#include <string>
#include <thread>

// This needs to be included before getopt.h because the latter #defines symbols used by it
#include "common/microprofile.h"

#ifdef _MSC_VER
#include <getopt.h>
#else
#include <getopt.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL.h>

#include "citra/config.h"
#include "citra/emu_window/emu_window_sdl2.h"
#include "citra/rebind_window/rebind_window_sdl2.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/loader/loader.h"
#include "core/settings.h"
#include "core/system.h"
#include "video_core/video_core.h"

static void PrintHelp(const char* argv0) {
    std::cout << "Usage: " << argv0
              << " [options] <filename>\n"
                 "-g, --gdbport=NUMBER  Enable gdb stub on port NUMBER\n"
                 "-l, --list-bindings   List the current keybindings\n"
                 "-r, --rebind=KEY      Rebind a key (see -l for key names)\n"
                 "-h, --help            Display this help and exit\n"
                 "-v, --version         Output version information and exit\n";
}

static void PrintVersion() {
    std::cout << "Citra " << Common::g_scm_branch << " " << Common::g_scm_desc << std::endl;
}

static void ListBindings(const Config& config) {
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        int code = Settings::values.input_mappings[Settings::NativeInput::All[i]];
        std::cout << Settings::NativeInput::Mapping[i] << ": "
                  << SDL_GetScancodeName(static_cast<SDL_Scancode>(code)) << std::endl;
    }
}

/// Show an SDL window to rebind the `index`-th key in Settings::NativeInput::All.
static void Rebind(int index, const std::string& config_filename) {
    RebindWindow_SDL2 rebind_window(index, config_filename);
    rebind_window.Run();
}

/// Application entry point
int main(int argc, char** argv) {
    Config config;
    int option_index = 0;
    bool use_gdbstub = Settings::values.use_gdbstub;
    u32 gdb_port = static_cast<u32>(Settings::values.gdbstub_port);
    char* endarg;
#ifdef _WIN32
    int argc_w;
    auto argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);

    if (argv_w == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to get command line arguments");
        return -1;
    }
#endif
    std::string boot_filename;

    static struct option long_options[] = {
        {"gdbport", required_argument, 0, 'g'}, {"list-bindings", no_argument, 0, 'l'},
        {"rebind", required_argument, 0, 'r'},  {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},       {0, 0, 0, 0},
    };

    while (optind < argc) {
        char arg = getopt_long(argc, argv, "g:lr:hv", long_options, &option_index);
        if (arg != -1) {
            switch (arg) {
            case 'g':
                errno = 0;
                gdb_port = strtoul(optarg, &endarg, 0);
                use_gdbstub = true;
                if (endarg == optarg)
                    errno = EINVAL;
                if (errno != 0) {
                    perror("--gdbport");
                    exit(1);
                }
                break;
            case 'l':
                ListBindings(config);
                return 0;
            case 'r':
                // Try to read a key name.
                for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
                    if (strcmp(Settings::NativeInput::Mapping[i], optarg) == 0) {
                        Rebind(i, config.GetConfigPath());
                        return 0;
                    }
                }

                LOG_CRITICAL(Frontend, "\"%s\" is not a valid key name. "
                                       "See `%s -l` for a list of key names.",
                             optarg, argv[0]);
                return -1;
            case 'h':
                PrintHelp(argv[0]);
                return 0;
            case 'v':
                PrintVersion();
                return 0;
            }
        } else {
#ifdef _WIN32
            boot_filename = Common::UTF16ToUTF8(argv_w[optind]);
#else
            boot_filename = argv[optind];
#endif
            optind++;
        }
    }

#ifdef _WIN32
    LocalFree(argv_w);
#endif

    Log::Filter log_filter(Log::Level::Debug);
    Log::SetFilter(&log_filter);

    MicroProfileOnThreadCreate("EmuThread");
    SCOPE_EXIT({ MicroProfileShutdown(); });

    if (boot_filename.empty()) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return -1;
    }

    log_filter.ParseFilterString(Settings::values.log_filter);

    // Apply the command line arguments
    Settings::values.gdbstub_port = gdb_port;
    Settings::values.use_gdbstub = use_gdbstub;
    Settings::Apply();

    std::unique_ptr<EmuWindow_SDL2> emu_window = std::make_unique<EmuWindow_SDL2>();

    std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(boot_filename);
    if (!loader) {
        LOG_CRITICAL(Frontend, "Failed to obtain loader for %s!", boot_filename.c_str());
        return -1;
    }

    boost::optional<u32> system_mode = loader->LoadKernelSystemMode();

    if (!system_mode) {
        LOG_CRITICAL(Frontend, "Failed to load ROM (Could not determine system mode)!");
        return -1;
    }

    System::Init(emu_window.get(), system_mode.get());
    SCOPE_EXIT({ System::Shutdown(); });

    Loader::ResultStatus load_result = loader->Load();
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Frontend, "Failed to load ROM (Error %i)!", load_result);
        return -1;
    }

    while (emu_window->IsOpen()) {
        Core::RunLoop();
    }

    return 0;
}
