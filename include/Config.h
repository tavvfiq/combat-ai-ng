#pragma once

#include <SimpleIni.h>
#include <string>

namespace CombatAI
{
    // Configuration manager for the plugin
    class Config
    {
    public:
        struct GeneralSettings
        {
            bool enablePlugin = true;
            bool enableDebugLog = false;
            float processingInterval = 0.1f;
        };

        struct HumanizerSettings
        {
            float baseReactionDelayMs = 200.0f; // Base delay at level 1
            float reactionDelayReductionPerLevelMs = 2.0f; // Reduce delay by 2ms per level
            float minReactionDelayMs = 50.0f; // Minimum possible delay
            float reactionVarianceMs = 100.0f;
            
            float baseMistakeChance = 0.5f; // 50% chance at level 1
            float mistakeChanceReductionPerLevel = 0.01f; // Reduce chance by 1% per level
            float minMistakeChance = 0.0f; // Minimum mistake chance
            float bashCooldownSeconds = 3.0f;
            float dodgeCooldownSeconds = 2.0f;
            float jumpCooldownSeconds = 3.0f;
            // Action-specific mistake chance multipliers
            float bashMistakeMultiplier = 0.5f;
            float dodgeMistakeMultiplier = 1.5f;
            float jumpMistakeMultiplier = 1.5f;
            float strafeMistakeMultiplier = 1.2f;
            float powerAttackMistakeMultiplier = 1.0f;
            float attackMistakeMultiplier = 0.8f;
            float sprintAttackMistakeMultiplier = 1.2f;
            float retreatMistakeMultiplier = 0.3f;
            float backoffMistakeMultiplier = 0.5f;
            float advancingMistakeMultiplier = 0.7f;
            float flankingMistakeMultiplier = 1.0f;
        };

        struct DodgeSystemSettings
        {
            float dodgeStaminaCost = 10.0f;
            float iFrameDuration = 0.3f;
            bool enableStepDodge = false;
            bool enableDodgeAttackCancel = true;
        };

        struct DecisionMatrixSettings
        {
            float interruptMaxDistance = 150.0f;
            float interruptReachMultiplier = 1.2f;
            bool enableEvasionDodge = true;
            float evasionMinDistance = 250.0f;
            bool enableJumpEvasion = true;
            float jumpEvasionDistanceMin = 500.0f;
            float jumpEvasionDistanceMax = 1500.0f;
            float evasionJumpChance = 0.5f;
            float staminaThreshold = 0.2f;
            float healthThreshold = 0.3f;
            bool enableSurvivalRetreat = true;
            bool enableOffense = true;
            float offenseReachMultiplier = 1.0f;
            float sprintAttackMinDistance = 300.0f;
            float sprintAttackMaxDistance = 800.0f;
            float attackStaminaCost = 15.0f; // Stamina cost for normal attacks (not used for decision, only for reference)
            float powerAttackStaminaCost = 20.0f; // Stamina cost for power attacks
            float sprintAttackStaminaCost = 25.0f; // Stamina cost for sprint attacks (higher due to sprinting + attack)
            bool enablePowerAttackStaminaCheck = true; // If true, check stamina before allowing power attacks
            bool enableSprintAttackStaminaCheck = true; // If true, check stamina before allowing sprint attacks
            // Note: Normal attacks always allowed regardless of stamina (even if stamina is 0)
            // Stamina checks compare actual stamina value against stamina cost (not percentage thresholds)
        };

        struct PerformanceSettings
        {
            bool onlyProcessCombatActors = true;
            float cleanupInterval = 5.0f;
            std::uint32_t maxActorsPerFrame = 0;
            
            // Adaptive Processing Interval (LOD) settings
            float processingIntervalMid = 0.2f; // Slower updates for mid-range (1000-3000 units)
            float processingIntervalFar = 0.5f; // Slowest updates for far range (>3000 units)
            float distanceNear = 1000.0f; // Distance threshold for "Near" (uses processingInterval)
            float distanceMid = 3000.0f; // Distance threshold for "Mid" (uses processingIntervalMid)
        };

        struct ModIntegrationSettings
        {
            bool enableCPRIntegration = true;
            bool enableBFCOIntegration = true;
            bool enablePrecisionIntegration = true;
            bool enableTKDodgeIntegration = true;
        };

        struct ParrySettings
        {
            bool enableParry = true; // Enable parry mechanism (timed bash)
            float parryWindowStart = 0.05f; // Start bash this many seconds before attack hits (minimum)
            float parryWindowEnd = 0.25f; // Latest time to start bash before attack hits (maximum)
            float parryMinDistance = 50.0f; // Minimum distance for parry
            float parryMaxDistance = 200.0f; // Maximum distance for parry
            float parryBasePriority = 1.5f; // Base priority for parry opportunity
            float timingBonusMax = 0.3f; // Maximum bonus for perfect timing
            float earlyBashPenalty = 0.5f; // Penalty if bash too early
            float lateBashPenalty = 0.5f; // Penalty if bash too late
        };

        struct TimedBlockSettings
        {
            bool enableTimedBlock = true; // Enable timed block mechanism
            float timedBlockWindowStart = 0.05f; // Start block this many seconds before attack hits (minimum)
            float timedBlockWindowEnd = 0.33f; // End of timing window (matches mod's 0.33s window)
            float timedBlockMinDistance = 50.0f; // Minimum distance for timed block
            float timedBlockMaxDistance = 200.0f; // Maximum distance for timed block
            float timedBlockBasePriority = 1.5f; // Base priority for timed block opportunity
            float timedBlockTimingBonusMax = 0.3f; // Maximum bonus for perfect timing
            float timedBlockEarlyPenalty = 0.5f; // Penalty if block too early
            float timedBlockLatePenalty = 0.5f; // Penalty if block too late
        };

        static Config& GetInstance()
        {
            static Config instance;
            return instance;
        }

        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        // Load configuration from INI file
        bool Load(const std::string& a_filePath = "Data/SKSE/Plugins/EnhancedCombatAI.ini");

        // Get settings
        const GeneralSettings& GetGeneral() const { return m_general; }
        const HumanizerSettings& GetHumanizer() const { return m_humanizer; }
        const DodgeSystemSettings& GetDodgeSystem() const { return m_dodgeSystem; }
        const DecisionMatrixSettings& GetDecisionMatrix() const { return m_decisionMatrix; }
        const PerformanceSettings& GetPerformance() const { return m_performance; }
        const ModIntegrationSettings& GetModIntegrations() const { return m_modIntegrations; }
        const ParrySettings& GetParry() const { return m_parry; }
        const TimedBlockSettings& GetTimedBlock() const { return m_timedBlock; }

        // Check if plugin is enabled
        bool IsEnabled() const { return m_general.enablePlugin; }

    private:
        Config() = default;
        ~Config() = default;

        // Helper methods to read settings
        void ReadGeneralSettings(CSimpleIniA& a_ini);
        void ReadHumanizerSettings(CSimpleIniA& a_ini);
        void ReadDodgeSystemSettings(CSimpleIniA& a_ini);
        void ReadDecisionMatrixSettings(CSimpleIniA& a_ini);
        void ReadPerformanceSettings(CSimpleIniA& a_ini);
        void ReadModIntegrationSettings(CSimpleIniA& a_ini);
        void ReadParrySettings(CSimpleIniA& a_ini);
        void ReadTimedBlockSettings(CSimpleIniA& a_ini);

        GeneralSettings m_general;
        HumanizerSettings m_humanizer;
        DodgeSystemSettings m_dodgeSystem;
        DecisionMatrixSettings m_decisionMatrix;
        PerformanceSettings m_performance;
        ModIntegrationSettings m_modIntegrations;
        ParrySettings m_parry;
        TimedBlockSettings m_timedBlock;
    };
}
