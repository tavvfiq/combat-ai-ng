#include "pch.h"
#include "Hooks.h"
#include "CombatDirector.h"
#include "Config.h"

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    // Initialize SKSE
    SKSE::Init(a_skse);

    // Initialize logger
    auto logger = SKSE::log::logger();
    logger->info("CombatAI-NG v{} loading...", "0.0.1");

    // Load configuration
    auto& config = Config::GetInstance();
    config.Load();

    // Check if plugin is enabled
    if (!config.IsEnabled()) {
        logger->info("CombatAI-NG is disabled in configuration file");
        return true; // Plugin loaded but disabled
    }

    // Set debug log level if enabled
    if (config.GetGeneral().enableDebugLog) {
        logger->set_level(spdlog::level::debug);
        logger->debug("Debug logging enabled");
    }

    // Initialize CombatDirector with config
    CombatDirector::GetInstance().Initialize();

    // Install hooks
    CombatAI::Hooks::Install();

    logger->info("CombatAI-NG loaded successfully");

    return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() noexcept {
    SKSE::PluginVersionData v{};
    v.PluginVersion(REL::Version(0, 0, 1));
    v.PluginName("CombatAI-NG");
    v.AuthorName("bosn");
    v.UsesAddressLibrary(true);
    v.UsesStructsPost629(true);
    return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = "CombatAI-NG";
    a_info->version = REL::Version(0, 0, 1);

    if (a_skse->IsEditor()) {
        SKSE::log::logger()->critical("Loaded in editor, marking as incompatible");
        return false;
    }

    const auto ver = a_skse->RuntimeVersion();
    if (ver < SKSE::RUNTIME_SSE_1_5_97) {
        SKSE::log::logger()->critical("Unsupported runtime version {}", ver.string());
        return false;
    }

    return true;
}
