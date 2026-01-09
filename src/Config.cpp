#include "pch.h"
#include "Config.h"
#include "Logger.h"
#include <filesystem>

// Undefine Windows macros that conflict with std functions
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef clamp
#undef clamp
#endif

namespace CombatAI
{
    // Helper function to clamp values (avoids Windows macro issues)
    template<typename T>
    constexpr T ClampValue(const T& value, const T& minVal, const T& maxVal)
    {
        return (value < minVal) ? minVal : ((value > maxVal) ? maxVal : value);
    }
    bool Config::Load(const std::string& a_filePath)
    {
        CSimpleIniA ini;
        ini.SetUnicode();

        // Try to load the INI file
        SI_Error rc = ini.LoadFile(a_filePath.c_str());
        if (rc < 0) {
            LOG_WARN("Failed to load config file: {}. Using defaults.", a_filePath);
            return false;
        }

        LOG_INFO("Loading configuration from: {}", a_filePath);

        // Read all settings sections
        ReadGeneralSettings(ini);
        ReadHumanizerSettings(ini);
        ReadDodgeSystemSettings(ini);
        ReadDecisionMatrixSettings(ini);
        ReadPerformanceSettings(ini);
        ReadModIntegrationSettings(ini);

        LOG_INFO("Configuration loaded successfully");
        return true;
    }

    void Config::ReadGeneralSettings(CSimpleIniA& a_ini)
    {
        m_general.enablePlugin = a_ini.GetBoolValue("General", "EnablePlugin", m_general.enablePlugin);
        m_general.enableDebugLog = a_ini.GetBoolValue("General", "EnableDebugLog", m_general.enableDebugLog);
        m_general.processingInterval = static_cast<float>(a_ini.GetDoubleValue("General", "ProcessingInterval", m_general.processingInterval));

        // Clamp processing interval to reasonable values
        m_general.processingInterval = ClampValue(m_general.processingInterval, 0.01f, 1.0f);
    }

    void Config::ReadHumanizerSettings(CSimpleIniA& a_ini)
    {
        m_humanizer.baseReactionDelayMs = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "BaseReactionDelayMs", m_humanizer.baseReactionDelayMs));
        m_humanizer.reactionVarianceMs = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "ReactionVarianceMs", m_humanizer.reactionVarianceMs));
        m_humanizer.level1MistakeChance = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "Level1MistakeChance", m_humanizer.level1MistakeChance));
        m_humanizer.level50MistakeChance = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "Level50MistakeChance", m_humanizer.level50MistakeChance));
        m_humanizer.bashCooldownSeconds = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "BashCooldownSeconds", m_humanizer.bashCooldownSeconds));
        m_humanizer.dodgeCooldownSeconds = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "DodgeCooldownSeconds", m_humanizer.dodgeCooldownSeconds));

        // Clamp values to valid ranges
        m_humanizer.baseReactionDelayMs = (std::max)(0.0f, m_humanizer.baseReactionDelayMs);
        m_humanizer.reactionVarianceMs = (std::max)(0.0f, m_humanizer.reactionVarianceMs);
        m_humanizer.level1MistakeChance = ClampValue(m_humanizer.level1MistakeChance, 0.0f, 1.0f);
        m_humanizer.level50MistakeChance = ClampValue(m_humanizer.level50MistakeChance, 0.0f, 1.0f);
        m_humanizer.bashCooldownSeconds = (std::max)(0.0f, m_humanizer.bashCooldownSeconds);
        m_humanizer.dodgeCooldownSeconds = (std::max)(0.0f, m_humanizer.dodgeCooldownSeconds);
    }

    void Config::ReadDodgeSystemSettings(CSimpleIniA& a_ini)
    {
        m_dodgeSystem.dodgeStaminaCost = static_cast<float>(a_ini.GetDoubleValue("DodgeSystem", "DodgeStaminaCost", m_dodgeSystem.dodgeStaminaCost));
        m_dodgeSystem.iFrameDuration = static_cast<float>(a_ini.GetDoubleValue("DodgeSystem", "IFrameDuration", m_dodgeSystem.iFrameDuration));
        m_dodgeSystem.enableStepDodge = a_ini.GetBoolValue("DodgeSystem", "EnableStepDodge", m_dodgeSystem.enableStepDodge);
        m_dodgeSystem.enableDodgeAttackCancel = a_ini.GetBoolValue("DodgeSystem", "EnableDodgeAttackCancel", m_dodgeSystem.enableDodgeAttackCancel);

        // Clamp values
        m_dodgeSystem.dodgeStaminaCost = (std::max)(0.0f, m_dodgeSystem.dodgeStaminaCost);
        m_dodgeSystem.iFrameDuration = (std::max)(0.0f, m_dodgeSystem.iFrameDuration);
    }

    void Config::ReadDecisionMatrixSettings(CSimpleIniA& a_ini)
    {
        m_decisionMatrix.interruptMaxDistance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "InterruptMaxDistance", m_decisionMatrix.interruptMaxDistance));
        m_decisionMatrix.interruptReachMultiplier = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "InterruptReachMultiplier", m_decisionMatrix.interruptReachMultiplier));
        m_decisionMatrix.enableEvasionDodge = a_ini.GetBoolValue("DecisionMatrix", "EnableEvasionDodge", m_decisionMatrix.enableEvasionDodge);
        m_decisionMatrix.evasionDodgeChance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "EvasionDodgeChance", m_decisionMatrix.evasionDodgeChance));
        m_decisionMatrix.evasionMinDistance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "EvasionMinDistance", m_decisionMatrix.evasionMinDistance));
        m_decisionMatrix.enableJumpEvasion = a_ini.GetBoolValue("DecisionMatrix", "EnableJumpEvasion", m_decisionMatrix.enableJumpEvasion);
        m_decisionMatrix.jumpEvasionDistanceMin = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "JumpEvasionDistanceMin", m_decisionMatrix.jumpEvasionDistanceMin));
        m_decisionMatrix.jumpEvasionDistanceMax = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "JumpEvasionDistanceMax", m_decisionMatrix.jumpEvasionDistanceMax));
        m_decisionMatrix.evasionJumpChance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "EvasionJumpChance", m_decisionMatrix.evasionJumpChance));
        m_decisionMatrix.staminaThreshold = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "StaminaThreshold", m_decisionMatrix.staminaThreshold));
        m_decisionMatrix.healthThreshold = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "HealthThreshold", m_decisionMatrix.healthThreshold));
        m_decisionMatrix.enableSurvivalRetreat = a_ini.GetBoolValue("DecisionMatrix", "EnableSurvivalRetreat", m_decisionMatrix.enableSurvivalRetreat);
        m_decisionMatrix.enableOffense = a_ini.GetBoolValue("DecisionMatrix", "EnableOffense", m_decisionMatrix.enableOffense);
        m_decisionMatrix.offenseReachMultiplier = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "OffenseReachMultiplier", m_decisionMatrix.offenseReachMultiplier));
        m_decisionMatrix.powerAttackStaminaThreshold = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "PowerAttackStaminaThreshold", m_decisionMatrix.powerAttackStaminaThreshold));
        m_decisionMatrix.sprintAttackMinDistance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "SprintAttackMinDistance", m_decisionMatrix.sprintAttackMinDistance));
        m_decisionMatrix.sprintAttackMaxDistance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "SprintAttackMaxDistance", m_decisionMatrix.sprintAttackMaxDistance));
        m_decisionMatrix.sprintAttackStaminaThreshold = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "SprintAttackStaminaThreshold", m_decisionMatrix.sprintAttackStaminaThreshold));

        // Clamp values
        m_decisionMatrix.evasionDodgeChance = ClampValue(m_decisionMatrix.evasionDodgeChance, 0.0f, 1.0f);
        m_decisionMatrix.interruptMaxDistance = (std::max)(0.0f, m_decisionMatrix.interruptMaxDistance);
        m_decisionMatrix.interruptReachMultiplier = ClampValue(m_decisionMatrix.interruptReachMultiplier, 0.1f, 5.0f);
        m_decisionMatrix.jumpEvasionDistanceMin = (std::max)(0.0f, m_decisionMatrix.jumpEvasionDistanceMin);
        m_decisionMatrix.jumpEvasionDistanceMax = (std::max)(0.0f, m_decisionMatrix.jumpEvasionDistanceMax);
        m_decisionMatrix.evasionJumpChance = ClampValue(m_decisionMatrix.evasionJumpChance, 0.0f, 1.0f);
        m_decisionMatrix.staminaThreshold = ClampValue(m_decisionMatrix.staminaThreshold, 0.0f, 1.0f);
        m_decisionMatrix.healthThreshold = ClampValue(m_decisionMatrix.healthThreshold, 0.0f, 1.0f);
        m_decisionMatrix.offenseReachMultiplier = ClampValue(m_decisionMatrix.offenseReachMultiplier, 0.1f, 5.0f);
        m_decisionMatrix.powerAttackStaminaThreshold = ClampValue(m_decisionMatrix.powerAttackStaminaThreshold, 0.0f, 1.0f);
        m_decisionMatrix.sprintAttackMinDistance = (std::max)(0.0f, m_decisionMatrix.sprintAttackMinDistance);
        m_decisionMatrix.sprintAttackMaxDistance = (std::max)(0.0f, m_decisionMatrix.sprintAttackMaxDistance);
        m_decisionMatrix.sprintAttackStaminaThreshold = ClampValue(m_decisionMatrix.sprintAttackStaminaThreshold, 0.0f, 1.0f);
    }

    void Config::ReadPerformanceSettings(CSimpleIniA& a_ini)
    {
        m_performance.onlyProcessCombatActors = a_ini.GetBoolValue("Performance", "OnlyProcessCombatActors", m_performance.onlyProcessCombatActors);
        m_performance.cleanupInterval = static_cast<float>(a_ini.GetDoubleValue("Performance", "CleanupInterval", m_performance.cleanupInterval));
        m_performance.maxActorsPerFrame = static_cast<std::uint32_t>(a_ini.GetLongValue("Performance", "MaxActorsPerFrame", m_performance.maxActorsPerFrame));

        // Clamp values
        m_performance.cleanupInterval = (std::max)(0.1f, m_performance.cleanupInterval);
    }

    void Config::ReadModIntegrationSettings(CSimpleIniA& a_ini)
    {
        m_modIntegrations.enableCPRIntegration = a_ini.GetBoolValue("ModIntegrations", "EnableCPRIntegration", m_modIntegrations.enableCPRIntegration);
        m_modIntegrations.enableBFCOIntegration = a_ini.GetBoolValue("ModIntegrations", "EnableBFCOIntegration", m_modIntegrations.enableBFCOIntegration);
        m_modIntegrations.enablePrecisionIntegration = a_ini.GetBoolValue("ModIntegrations", "EnablePrecisionIntegration", m_modIntegrations.enablePrecisionIntegration);
        m_modIntegrations.enableTKDodgeIntegration = a_ini.GetBoolValue("ModIntegrations", "EnableTKDodgeIntegration", m_modIntegrations.enableTKDodgeIntegration);
    }
}
