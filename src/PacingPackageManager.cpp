#include "PacingPackageManager.h"
#include "ActorUtils.h"
#include "Logger.h"
#include "ScriptHelper.h"
#include "pch.h"

namespace CombatAI
{
    // ---------------------------------------------------------------------------
    // VM callback: called by Papyrus after Add/RemovePackageOverride completes.
    // We use it to call EvaluatePackage() so Skyrim acts on the change immediately.
    // ---------------------------------------------------------------------------
    class PacingVMCallback : public RE::BSScript::IStackCallbackFunctor
    {
      public:
        explicit PacingVMCallback(RE::ActorHandle a_handle, bool a_isLock) : m_handle(a_handle), m_isLock(a_isLock) {}

        void operator()([[maybe_unused]] RE::BSScript::Variable a_result) override
        {
            auto ref = m_handle.get();
            auto actor = ref ? ref->As<RE::Character>() : nullptr;
            if (actor) {
                actor->EvaluatePackage();
                LOG_DEBUG("PacingPackageManager: EvaluatePackage on {:08X} ({})", actor->GetFormID(),
                          m_isLock ? "locked" : "unlocked");
            }
        }

        void SetObject([[maybe_unused]] const RE::BSTSmartPointer<RE::BSScript::Object> &) override {}

      private:
        RE::ActorHandle m_handle;
        bool m_isLock;
    };

    // ---------------------------------------------------------------------------
    bool PacingPackageManager::Initialize()
    {
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            LOG_WARN("PacingPackageManager: TESDataHandler not available");
            return false;
        }

        // templated LookupForm<TESPackage> handles the cast + FORMTYPE check
        m_package = dataHandler->LookupForm<RE::TESPackage>(kWYTCirclePackageLocalID, kWYTPluginName);
        if (!m_package) {
            LOG_INFO("PacingPackageManager: {} not found — WYT package pacing disabled", kWYTPluginName);
            return false;
        }

        LOG_INFO("PacingPackageManager: WYT_CirclePackage found (FormID 0x{:08X}) — package pacing enabled",
                 m_package->GetFormID());
        return true;
    }

    // ---------------------------------------------------------------------------
    void PacingPackageManager::LockEnemy(RE::Actor *a_actor)
    {
        if (!m_package || !a_actor) {
            return;
        }

        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return;
        }

        {
            Locker locker(m_lock);
            if (m_locked.count(formIDOpt.value())) {
                return; // already locked, avoid redundant Papyrus call
            }
            m_locked.insert(formIDOpt.value());
        }

        LOG_DEBUG("PacingPackageManager: locking {:08X}", formIDOpt.value());

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        callback.reset(new PacingVMCallback(a_actor->GetHandle(), true));

        // MakeFunctionArguments requires rvalue pointers — copy then move (matches WYT pattern)
        RE::Actor *actorPtr = a_actor;
        RE::TESPackage *pkgPtr = m_package;
        Script::DispatchStaticCall("ActorUtil", "AddPackageOverride", callback, std::move(actorPtr), std::move(pkgPtr),
                                   100, 1);
    }

    // ---------------------------------------------------------------------------
    void PacingPackageManager::UnlockEnemy(RE::Actor *a_actor)
    {
        if (!m_package || !a_actor) {
            return;
        }

        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return;
        }

        {
            Locker locker(m_lock);
            if (!m_locked.count(formIDOpt.value())) {
                return; // not locked, nothing to do
            }
            m_locked.erase(formIDOpt.value());
        }

        LOG_DEBUG("PacingPackageManager: unlocking {:08X}", formIDOpt.value());

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        callback.reset(new PacingVMCallback(a_actor->GetHandle(), false));

        // MakeFunctionArguments requires rvalue pointers — copy then move
        RE::Actor *actorPtr = a_actor;
        RE::TESPackage *pkgPtr = m_package;
        Script::DispatchStaticCall("ActorUtil", "RemovePackageOverride", callback, std::move(actorPtr),
                                   std::move(pkgPtr));
    }

    // ---------------------------------------------------------------------------
    void PacingPackageManager::UnlockAll()
    {
        if (!m_package) {
            return;
        }

        std::vector<RE::FormID> toUnlock;
        {
            Locker locker(m_lock);
            toUnlock.assign(m_locked.begin(), m_locked.end());
            m_locked.clear();
        }

        for (RE::FormID id : toUnlock) {
            auto actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (actor && !ActorUtils::SafeIsDead(actor)) {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
                callback.reset(new PacingVMCallback(actor->GetHandle(), false));
                RE::Actor *actorPtr = actor;
                RE::TESPackage *pkgPtr = m_package;
                Script::DispatchStaticCall("ActorUtil", "RemovePackageOverride", callback, std::move(actorPtr),
                                           std::move(pkgPtr));
            }
        }
        LOG_INFO("PacingPackageManager: UnlockAll released {} locked NPCs", toUnlock.size());
    }
} // namespace CombatAI
