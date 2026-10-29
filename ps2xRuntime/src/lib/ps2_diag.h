#pragma once

// Local diagnostic file log (not upstream). Android has no argv and the
// logcat redirect proved unreliable on device, so diagnostics go to a file
// next to the game data that can be pulled with adb.
//
// Path: /storage/emulated/0/Android/data/com.ps2x.runner/files/diag.log
// Override: PS2X_DIAG_LOG environment variable.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace ps2x_diag
{
    inline void log(const char *fmt, ...)
    {
        static std::mutex mutex;
        static FILE *file = nullptr;
        static bool tried = false;

        std::lock_guard<std::mutex> lock(mutex);
        if (!tried)
        {
            tried = true;
            const char *path = std::getenv("PS2X_DIAG_LOG");
            if (!path || !*path)
            {
                path = "/storage/emulated/0/Android/data/com.ps2x.runner/files/diag.log";
            }
            file = std::fopen(path, "wb");
        }
        if (!file)
        {
            return;
        }

        va_list args;
        va_start(args, fmt);
        std::vfprintf(file, fmt, args);
        va_end(args);
        std::fputc('\n', file);
        std::fflush(file);
    }
}
