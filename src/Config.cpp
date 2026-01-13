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
        ReadParrySettings(ini);
        ReadTimedBlockSettings(ini);

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
        m_humanizer.level1ReactionDelayMs = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "Level1ReactionDelayMs", m_humanizer.level1ReactionDelayMs));
        m_humanizer.level50ReactionDelayMs = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "Level50ReactionDelayMs", m_humanizer.level50ReactionDelayMs));
        m_humanizer.level1MistakeChance = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "Level1MistakeChance", m_humanizer.level1MistakeChance));
        m_humanizer.level50MistakeChance = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "Level50MistakeChance", m_humanizer.level50MistakeChance));
        m_humanizer.bashCooldownSeconds = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "BashCooldownSeconds", m_humanizer.bashCooldownSeconds));
        m_humanizer.dodgeCooldownSeconds = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "DodgeCooldownSeconds", m_humanizer.dodgeCooldownSeconds));
        m_humanizer.jumpCooldownSeconds = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "JumpCooldownSeconds", m_humanizer.jumpCooldownSeconds));
        
        // Action-specific mistake chance multipliers
        m_humanizer.bashMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "BashMistakeMultiplier", m_humanizer.bashMistakeMultiplier));
        m_humanizer.dodgeMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "DodgeMistakeMultiplier", m_humanizer.dodgeMistakeMultiplier));
        m_humanizer.jumpMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "JumpMistakeMultiplier", m_humanizer.jumpMistakeMultiplier));
        m_humanizer.strafeMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "StrafeMistakeMultiplier", m_humanizer.strafeMistakeMultiplier));
        m_humanizer.powerAttackMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "PowerAttackMistakeMultiplier", m_humanizer.powerAttackMistakeMultiplier));
        m_humanizer.attackMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "AttackMistakeMultiplier", m_humanizer.attackMistakeMultiplier));
        m_humanizer.sprintAttackMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "SprintAttackMistakeMultiplier", m_humanizer.sprintAttackMistakeMultiplier));
        m_humanizer.retreatMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "RetreatMistakeMultiplier", m_humanizer.retreatMistakeMultiplier));
        m_humanizer.backoffMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "BackoffMistakeMultiplier", m_humanizer.backoffMistakeMultiplier));
        m_humanizer.advancingMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "AdvancingMistakeMultiplier", m_humanizer.advancingMistakeMultiplier));
        m_humanizer.flankingMistakeMultiplier = static_cast<float>(a_ini.GetDoubleValue("Humanizer", "FlankingMistakeMultiplier", m_humanizer.flankingMistakeMultiplier));

        // Clamp values to valid ranges
        m_humanizer.baseReactionDelayMs = (std::max)(0.0f, m_humanizer.baseReactionDelayMs);
        m_humanizer.reactionVarianceMs = (std::max)(0.0f, m_humanizer.reactionVarianceMs);
        m_humanizer.level1ReactionDelayMs = (std::max)(0.0f, m_humanizer.level1ReactionDelayMs);
        m_humanizer.level50ReactionDelayMs = (std::max)(0.0f, m_humanizer.level50ReactionDelayMs);
        m_humanizer.level1MistakeChance = ClampValue(m_humanizer.level1MistakeChance, 0.0f, 1.0f);
        m_humanizer.level50MistakeChance = ClampValue(m_humanizer.level50MistakeChance, 0.0f, 1.0f);
        m_humanizer.bashCooldownSeconds = (std::max)(0.0f, m_humanizer.bashCooldownSeconds);
        m_humanizer.dodgeCooldownSeconds = (std::max)(0.0f, m_humanizer.dodgeCooldownSeconds);
        m_humanizer.jumpCooldownSeconds = (std::max)(0.0f, m_humanizer.jumpCooldownSeconds);
        
        // Clamp mistake multipliers to reasonable range (0.0 to 3.0)
        m_humanizer.bashMistakeMultiplier = ClampValue(m_humanizer.bashMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.dodgeMistakeMultiplier = ClampValue(m_humanizer.dodgeMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.jumpMistakeMultiplier = ClampValue(m_humanizer.jumpMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.strafeMistakeMultiplier = ClampValue(m_humanizer.strafeMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.powerAttackMistakeMultiplier = ClampValue(m_humanizer.powerAttackMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.attackMistakeMultiplier = ClampValue(m_humanizer.attackMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.sprintAttackMistakeMultiplier = ClampValue(m_humanizer.sprintAttackMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.retreatMistakeMultiplier = ClampValue(m_humanizer.retreatMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.backoffMistakeMultiplier = ClampValue(m_humanizer.backoffMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.advancingMistakeMultiplier = ClampValue(m_humanizer.advancingMistakeMultiplier, 0.0f, 3.0f);
        m_humanizer.flankingMistakeMultiplier = ClampValue(m_humanizer.flankingMistakeMultiplier, 0.0f, 3.0f);
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
        m_decisionMatrix.sprintAttackMinDistance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "SprintAttackMinDistance", m_decisionMatrix.sprintAttackMinDistance));
        m_decisionMatrix.sprintAttackMaxDistance = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "SprintAttackMaxDistance", m_decisionMatrix.sprintAttackMaxDistance));
        m_decisionMatrix.attackStaminaCost = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "AttackStaminaCost", m_decisionMatrix.attackStaminaCost));
        m_decisionMatrix.powerAttackStaminaCost = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "PowerAttackStaminaCost", m_decisionMatrix.powerAttackStaminaCost));
        m_decisionMatrix.sprintAttackStaminaCost = static_cast<float>(a_ini.GetDoubleValue("DecisionMatrix", "SprintAttackStaminaCost", m_decisionMatrix.sprintAttackStaminaCost));
        m_decisionMatrix.enablePowerAttackStaminaCheck = a_ini.GetBoolValue("DecisionMatrix", "EnablePowerAttackStaminaCheck", m_decisionMatrix.enablePowerAttackStaminaCheck);
        m_decisionMatrix.enableSprintAttackStaminaCheck = a_ini.GetBoolValue("DecisionMatrix", "EnableSprintAttackStaminaCheck", m_decisionMatrix.enableSprintAttackStaminaCheck);

        // Clamp values
        m_decisionMatrix.interruptMaxDistance = (std::max)(0.0f, m_decisionMatrix.interruptMaxDistance);
        m_decisionMatrix.interruptReachMultiplier = ClampValue(m_decisionMatrix.interruptReachMultiplier, 0.1f, 5.0f);
        m_decisionMatrix.jumpEvasionDistanceMin = (std::max)(0.0f, m_decisionMatrix.jumpEvasionDistanceMin);
        m_decisionMatrix.jumpEvasionDistanceMax = (std::max)(0.0f, m_decisionMatrix.jumpEvasionDistanceMax);
        m_decisionMatrix.evasionJumpChance = ClampValue(m_decisionMatrix.evasionJumpChance, 0.0f, 1.0f);
        m_decisionMatrix.staminaThreshold = ClampValue(m_decisionMatrix.staminaThreshold, 0.0f, 1.0f);
        m_decisionMatrix.healthThreshold = ClampValue(m_decisionMatrix.healthThreshold, 0.0f, 1.0f);
        m_decisionMatrix.offenseReachMultiplier = ClampValue(m_decisionMatrix.offenseReachMultiplier, 0.1f, 5.0f);
        m_decisionMatrix.sprintAttackMinDistance = (std::max)(0.0f, m_decisionMatrix.sprintAttackMinDistance);
        m_decisionMatrix.sprintAttackMaxDistance = (std::max)(0.0f, m_decisionMatrix.sprintAttackMaxDistance);
        m_decisionMatrix.attackStaminaCost = (std::max)(0.0f, m_decisionMatrix.attackStaminaCost);
        m_decisionMatrix.powerAttackStaminaCost = (std::max)(0.0f, m_decisionMatrix.powerAttackStaminaCost);
        m_decisionMatrix.sprintAttackStaminaCost = (std::max)(0.0f, m_decisionMatrix.sprintAttackStaminaCost);
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

    void Config::ReadParrySettings(CSimpleIniA& a_ini)
    {
        m_parry.enableParry = a_ini.GetBoolValue("Parry", "EnableParry", m_parry.enableParry);
        m_parry.parryWindowStart = static_cast<float>(a_ini.GetDoubleValue("Parry", "ParryWindowStart", m_parry.parryWindowStart));
        m_parry.parryWindowEnd = static_cast<float>(a_ini.GetDoubleValue("Parry", "ParryWindowEnd", m_parry.parryWindowEnd));
        m_parry.parryMinDistance = static_cast<float>(a_ini.GetDoubleValue("Parry", "ParryMinDistance", m_parry.parryMinDistance));
        m_parry.parryMaxDistance = static_cast<float>(a_ini.GetDoubleValue("Parry", "ParryMaxDistance", m_parry.parryMaxDistance));
        m_parry.parryBasePriority = static_cast<float>(a_ini.GetDoubleValue("Parry", "ParryBasePriority", m_parry.parryBasePriority));
        m_parry.timingBonusMax = static_cast<float>(a_ini.GetDoubleValue("Parry", "TimingBonusMax", m_parry.timingBonusMax));
        m_parry.earlyBashPenalty = static_cast<float>(a_ini.GetDoubleValue("Parry", "EarlyBashPenalty", m_parry.earlyBashPenalty));
        m_parry.lateBashPenalty = static_cast<float>(a_ini.GetDoubleValue("Parry", "LateBashPenalty", m_parry.lateBashPenalty));

        // Clamp values
        m_parry.parryWindowStart = (std::max)(0.0f, m_parry.parryWindowStart);
        m_parry.parryWindowEnd = (std::max)(m_parry.parryWindowStart, m_parry.parryWindowEnd);
        m_parry.parryMinDistance = (std::max)(0.0f, m_parry.parryMinDistance);
        m_parry.parryMaxDistance = (std::max)(m_parry.parryMinDistance, m_parry.parryMaxDistance);
        m_parry.parryBasePriority = (std::max)(0.0f, m_parry.parryBasePriority);
        m_parry.timingBonusMax = ClampValue(m_parry.timingBonusMax, 0.0f, 1.0f);
        m_parry.earlyBashPenalty = ClampValue(m_parry.earlyBashPenalty, 0.0f, 1.0f);
        m_parry.lateBashPenalty = ClampValue(m_parry.lateBashPenalty, 0.0f, 1.0f);
    }

    void Config::ReadTimedBlockSettings(CSimpleIniA& a_ini)
    {
        m_timedBlock.enableTimedBlock = a_ini.GetBoolValue("TimedBlock", "EnableTimedBlock", m_timedBlock.enableTimedBlock);
        m_timedBlock.timedBlockWindowStart = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockWindowStart", m_timedBlock.timedBlockWindowStart));
        m_timedBlock.timedBlockWindowEnd = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockWindowEnd", m_timedBlock.timedBlockWindowEnd));
        m_timedBlock.timedBlockMinDistance = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockMinDistance", m_timedBlock.timedBlockMinDistance));
        m_timedBlock.timedBlockMaxDistance = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockMaxDistance", m_timedBlock.timedBlockMaxDistance));
        m_timedBlock.timedBlockBasePriority = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockBasePriority", m_timedBlock.timedBlockBasePriority));
        m_timedBlock.timedBlockTimingBonusMax = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockTimingBonusMax", m_timedBlock.timedBlockTimingBonusMax));
        m_timedBlock.timedBlockEarlyPenalty = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockEarlyPenalty", m_timedBlock.timedBlockEarlyPenalty));
        m_timedBlock.timedBlockLatePenalty = static_cast<float>(a_ini.GetDoubleValue("TimedBlock", "TimedBlockLatePenalty", m_timedBlock.timedBlockLatePenalty));

        // Clamp values
        m_timedBlock.timedBlockWindowStart = (std::max)(0.0f, m_timedBlock.timedBlockWindowStart);
        m_timedBlock.timedBlockWindowEnd = (std::max)(m_timedBlock.timedBlockWindowStart, m_timedBlock.timedBlockWindowEnd);
        m_timedBlock.timedBlockMinDistance = (std::max)(0.0f, m_timedBlock.timedBlockMinDistance);
        m_timedBlock.timedBlockMaxDistance = (std::max)(m_timedBlock.timedBlockMinDistance, m_timedBlock.timedBlockMaxDistance);
        m_timedBlock.timedBlockBasePriority = (std::max)(0.0f, m_timedBlock.timedBlockBasePriority);
        m_timedBlock.timedBlockTimingBonusMax = ClampValue(m_timedBlock.timedBlockTimingBonusMax, 0.0f, 1.0f);
        m_timedBlock.timedBlockEarlyPenalty = ClampValue(m_timedBlock.timedBlockEarlyPenalty, 0.0f, 1.0f);
        m_timedBlock.timedBlockLatePenalty = ClampValue(m_timedBlock.timedBlockLatePenalty, 0.0f, 1.0f);
    }
}
