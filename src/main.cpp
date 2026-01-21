#include "pch.h"
#include "Hooks.h"
#include "CombatDirector.h"
#include "Config.h"
#include "Logger.h"
#include "CombatAIAPI.h"
#include "APIManager.h"

extern "C" DLLEXPORT void* SKSEAPI RequestPluginAPI(ECA_API::InterfaceVersion a_interfaceVersion)
{
    auto api = CombatAI::APIManager::GetSingleton();
    switch (a_interfaceVersion) {
    case ECA_API::InterfaceVersion::V1:
        // APIManager implements IVCombatAI1
        return static_cast<ECA_API::IVCombatAI1*>(api);
    }
    return nullptr;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface *a_skse)
{
    // Initialize SKSE
    SKSE::Init(a_skse);

    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder)
        SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto logFilePath = *logsFolder / std::format("{}.log", "EnhancedCombatAI");
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));

    // Initialize logger
    LOG_INFO("EnhancedCombatAI v{} loading...", "0.0.1");

    // Load configuration
    auto &config = CombatAI::Config::GetInstance();
    config.Load();

    // Check if plugin is enabled
    if (!config.IsEnabled())
    {
        LOG_INFO("EnhancedCombatAI is disabled in configuration file");
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
                case SKSE::MessagingInterface::kDataLoaded:
                    // Initialize CombatDirector with config after data is loaded
                    // kDataLoaded fires after TESDataHandler is available, which is required for mod detection
                    // This is the earliest safe point for CommonLibSSE (non-NG) where TESDataHandler is guaranteed to be available
                    // Initialize BEFORE installing hooks so CombatDirector is ready when hooks start calling ProcessActor
                    CombatAI::CombatDirector::GetInstance().Initialize();
                    
                    // Install hooks after initialization
                    // Hooks will immediately start calling ProcessActor/Update, so CombatDirector must be initialized first
                    CombatAI::Hooks::Install();
                    break;
            }
        })) {
            logger::error("Failed to register messaging listener");
    }

    LOG_INFO("EnhancedCombatAI loaded successfully");

    return true;
}
