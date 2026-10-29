#include "ps2_runtime.h"
#include "games_database.h"
#include "runtime/ps2_native_iop.h"
#include "lib/ps2_diag.h"
#if defined(PS2X_ENABLE_DEBUG_UI) && !defined(PLATFORM_VITA)
#include "ps2_debug_panel.h"
#endif

#ifdef _DEBUG
#include "ps2_log.h"
#endif

#include <iostream>
#include <string>
#include <filesystem>
#include <exception>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#if defined(__ANDROID__)
#include <android/log.h>
#include <unistd.h>
#include <thread>
#include <cstdio>
#include <cstring>
#endif

namespace
{
#if defined(__ANDROID__)
    int g_logcatPipeFds[2]{-1, -1};
    std::thread g_logcatThread;

    void stopLogcatRedirect()
    {
        std::fflush(stdout);
        std::fflush(stderr);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        if (g_logcatPipeFds[1] >= 0)
        {
            close(g_logcatPipeFds[1]);
            g_logcatPipeFds[1] = -1;
        }
        if (g_logcatThread.joinable())
        {
            g_logcatThread.join();
        }
    }

    void redirectStdioToLogcat()
    {
        if (pipe(g_logcatPipeFds) != 0)
        {
            return;
        }

        setvbuf(stdout, nullptr, _IOLBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        dup2(g_logcatPipeFds[1], STDOUT_FILENO);
        dup2(g_logcatPipeFds[1], STDERR_FILENO);

        g_logcatThread = std::thread([]()
                                     {
                                         FILE *reader = fdopen(g_logcatPipeFds[0], "r");
                                         if (!reader)
                                         {
                                             return;
                                         }
                                         char line[1024];
                                         while (fgets(line, sizeof(line), reader))
                                         {
                                             size_t len = std::strlen(line);
                                             if (len > 0 && line[len - 1] == '\n')
                                             {
                                                 line[len - 1] = '\0';
                                             }
                                             __android_log_write(ANDROID_LOG_INFO, "ps2x", line);
                                         }
                                         fclose(reader);
                                         g_logcatPipeFds[0] = -1;
                                     });
        if (std::atexit(stopLogcatRedirect) != 0)
        {
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            close(g_logcatPipeFds[1]);
            g_logcatPipeFds[1] = -1;
            if (g_logcatThread.joinable())
            {
                g_logcatThread.join();
            }
        }
    }
#endif

    void setupTerminateLogger() // to help on release build crashs
    {
        std::set_terminate([]()
                           {
                               std::cerr << "[terminate] unhandled exception" << std::endl;
                               const std::exception_ptr ep = std::current_exception();
                               if (ep)
                               {
                                   try
                                   {
                                       std::rethrow_exception(ep);
                                   }
                                   catch (const std::system_error &e)
                                   {
                                       std::cerr << "[terminate] std::system_error code=" << e.code().value()
                                                 << " category=" << e.code().category().name()
                                                 << " message=" << e.what() << std::endl;
                                   }
                                   catch (const std::exception &e)
                                   {
                                       std::cerr << "[terminate] std::exception: " << e.what() << std::endl;
                                   }
                                   catch (...)
                                   {
                                       std::cerr << "[terminate] non-std exception" << std::endl;
                                   }
                               }
                               std::abort(); });
    }

    std::string normalizeGameId(const std::string &folderName)
    {
        std::string result = folderName;

        size_t underscore = result.find('_');
        if (underscore != std::string::npos)
            result[underscore] = '-';

        size_t dot = result.find('.');
        if (dot != std::string::npos)
            result.erase(dot, 1);

        std::ranges::transform(result, result.begin(), [](unsigned char character)
                               { return static_cast<char>(std::toupper(character)); });

        return result;
    }

    std::filesystem::path getExecutablePath(int argc, char *argv[])
    {
        if (argc >= 2 && argv[1] && argv[1][0] != '\0')
        {
            std::cout << "Using argv boot path" << std::endl;
            return std::filesystem::path(argv[1]);
        }
#if defined(PS2X_DEFAULT_BOOT_ELF)
        std::cout << "Using default boot file" << std::endl;
        const std::filesystem::path configuredPath = std::filesystem::path(PS2X_DEFAULT_BOOT_ELF);
#if defined(PLATFORM_VITA)
        return configuredPath;
#endif
        if (configuredPath.is_absolute())
        {
            return configuredPath;
        }
        return (std::filesystem::current_path() / configuredPath).lexically_normal();
#else
        throw std::runtime_error("Unable to determine executable path. Pass the guest ELF as argv[1] or define PS2X_DEFAULT_BOOT_ELF.");
#endif
    }
}

int main(int argc, char *argv[])
{
#if defined(__ANDROID__)
    redirectStdioToLogcat();
#endif
    setupTerminateLogger();

    try
    {
        std::filesystem::path pathObj = getExecutablePath(argc, argv);

        std::string filePathStr = pathObj.string();
        std::string elfName = pathObj.filename().string();
        std::string normalizedId = normalizeGameId(elfName);
        ps2x_diag::log("main: boot elf '%s' id=%s", filePathStr.c_str(), normalizedId.c_str());

        std::string windowTitle = "PS2-Recomp | ";
        const char *gameName = getGameName(normalizedId);

#if !defined(PLATFORM_VITA)
        if (gameName)
        {
            windowTitle += std::string(gameName) + " | " + elfName;
        }
        else
#endif
        {
            windowTitle += elfName;
        }

        PS2Runtime runtime;
#if defined(PS2X_ENABLE_DEBUG_UI) && !defined(PLATFORM_VITA)
        // This hook is to prevent leak rlimgui deps to recompiler etc
        PS2DebugPanel debugPanel;
        runtime.setDebugUiCallbacks(
            [](PS2Runtime &rt, void *userData)
            {
                (void)rt;
                static_cast<PS2DebugPanel *>(userData)->initialize();
            },
            [](PS2Runtime &rt, void *userData)
            {
                static_cast<PS2DebugPanel *>(userData)->draw(rt);
            },
            [](PS2Runtime &rt, void *userData)
            {
                (void)rt;
                static_cast<PS2DebugPanel *>(userData)->shutdown();
            },
            &debugPanel);
#endif
        if (!runtime.initialize(windowTitle.c_str()))
        {
            std::cerr << "Failed to initialize PS2 runtime" << std::endl;
            return 1;
        }

        // Local change: run every IOP module the game loads on the emulated IOP
        // ("*" wildcard) instead of relying on a per-game allowlist that only
        // the DQ8 parent project fills in.
        // SFIOP.IRX is opted out: the guest CL-file middleware asserts while
        // building its directory tree, and its stuck RPC server (sid 0xFF01)
        // would otherwise shadow the native CLFILE service. Leaving it unloaded
        // lets the native service own the sid.
        ps2_native_iop::setModules(runtime, {"*", "!SFIOP.IRX"});
        ps2x_diag::log("main: native IOP wildcard enabled (SFIOP.IRX excluded)");

        if (!runtime.loadELF(filePathStr))
        {
            std::cerr << "Failed to load ELF file: " << filePathStr << std::endl;
            return 1;
        }

        // Local patch: IoPaths::cdImage is never set upstream, so raw sector reads
        // from the disc fail. Take argv[2], then PS2X_CD_IMAGE, then the single
        // *.iso next to the boot ELF (Android has no argv).
        const char *cdImageArg = (argc >= 3) ? argv[2] : nullptr;
        std::filesystem::path cdImagePath;
        if (cdImageArg && cdImageArg[0] != '\0')
        {
            cdImagePath = cdImageArg;
        }
        // Local patch: a launcher that passes an unquoted path containing spaces
        // splits it across argv (e.g. "Eragon [SLES-54160] [FullRUS].iso"), so
        // re-join the remaining arguments and retry before falling back.
        std::error_code cdImageEc;
        if ((cdImagePath.empty() || !std::filesystem::exists(cdImagePath, cdImageEc)) && argc > 3)
        {
            std::string joined;
            for (int index = 2; index < argc; ++index)
            {
                if (!joined.empty())
                {
                    joined += ' ';
                }
                joined += argv[index];
            }
            if (std::filesystem::exists(joined, cdImageEc))
            {
                cdImagePath = joined;
            }
        }
        if (cdImagePath.empty())
        {
            if (const char *cdImageEnv = std::getenv("PS2X_CD_IMAGE"); cdImageEnv && cdImageEnv[0] != '\0')
            {
                cdImagePath = cdImageEnv;
            }
        }
        if (cdImagePath.empty())
        {
            std::error_code ec;
            const std::filesystem::path elfDirectory = pathObj.parent_path();
            if (!elfDirectory.empty())
            {
                for (const auto &entry : std::filesystem::directory_iterator(elfDirectory, ec))
                {
                    if (!entry.is_regular_file(ec))
                    {
                        continue;
                    }
                    std::string extension = entry.path().extension().string();
                    std::ranges::transform(extension, extension.begin(), [](unsigned char character)
                                           { return static_cast<char>(std::tolower(character)); });
                    if (extension == ".iso")
                    {
                        cdImagePath = entry.path();
                        break;
                    }
                }
            }
        }
        if (!cdImagePath.empty())
        {
            if (!std::filesystem::exists(cdImagePath))
            {
                std::cerr << "[main] disc image not found: " << cdImagePath.string() << std::endl;
            }
            else
            {
                PS2Runtime::IoPaths ioPaths = PS2Runtime::getIoPaths();
                ioPaths.cdImage = std::filesystem::absolute(cdImagePath).lexically_normal();
                PS2Runtime::setIoPaths(ioPaths);
                std::cout << "[main] disc image: " << ioPaths.cdImage.string() << std::endl;
                ps2x_diag::log("main: disc image '%s'", ioPaths.cdImage.string().c_str());
            }
        }

        runtime.run();

#ifdef _DEBUG
        ps2_log::print_saved_location();
#endif
        std::cout.flush();
        std::cerr.flush();
        std::_Exit(0);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[main] fatal exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "[main] fatal exception: unknown" << std::endl;
    }

    std::cout.flush();
    std::cerr.flush();
    std::_Exit(1);
}
