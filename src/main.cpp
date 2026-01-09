#include "pch.h"
#include "Hooks.h"
#include "CombatDirector.h"
#include "Config.h"
#include "Logger.h"

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface *a_skse)
{
    // Initialize SKSE
    SKSE::Init(a_skse);

    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder)
        SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto logFilePath = *logsFolder / std::format("{}.log", "CombatAI-NG");
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));

    // Initialize logger
    LOG_INFO("CombatAI-NG v{} loading...", "0.0.1");

    // Load configuration
    auto &config = CombatAI::Config::GetInstance();
    config.Load();

    // Check if plugin is enabled
    if (!config.IsEnabled())
    {
        LOG_INFO("CombatAI-NG is disabled in configuration file");
        return true; // Plugin loaded but disabled
    }

    // Set debug log level if enabled
    if (config.GetGeneral().enableDebugLog)
    {
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        LOG_DEBUG("Debug logging enabled");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }

    if (!SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
            switch (message->type) {
                case SKSE::MessagingInterface::kPostLoad:
                    // Initialize CombatDirector with config
                    CombatAI::CombatDirector::GetInstance().Initialize();

                    // Install hooks
                    CombatAI::Hooks::Install();
                    break;
            }
        })) {
            logger::error("Failed to register messaging listener");
    }

    LOG_INFO("CombatAI-NG loaded successfully");

    return true;
}
