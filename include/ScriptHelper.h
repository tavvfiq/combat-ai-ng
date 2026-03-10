#pragma once

#include "pch.h"

// Thin wrapper around CommonLibSSE's Papyrus VM dispatch.
// Mirrors WaitYourTurn's ScriptHelper.h — allows calling Papyrus static
// functions (e.g. ActorUtil.AddPackageOverride) from native SKSE code.

namespace Script
{
    using VM = RE::BSScript::Internal::VirtualMachine;
    using ObjectPtr = RE::BSTSmartPointer<RE::BSScript::Object>;
    using CallbackPtr = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>;
    using Args = RE::BSScript::IFunctionArguments;

    // Dispatch a static Papyrus function call.
    // a_class  : Papyrus script class name (e.g. "ActorUtil")
    // a_fnName : function name (e.g. "AddPackageOverride")
    // a_callback: optional callback fired when VM finishes (may be null)
    // a_args...: arguments forwarded to RE::MakeFunctionArguments
    template <class... ArgsT>
    inline bool DispatchStaticCall(RE::BSFixedString a_class, RE::BSFixedString a_fnName, CallbackPtr a_callback,
                                   ArgsT &&...a_args)
    {
        auto vm = VM::GetSingleton();
        if (!vm) {
            return false;
        }
        auto args = RE::MakeFunctionArguments(std::forward<ArgsT>(a_args)...);
        bool result = vm->DispatchStaticCall(a_class, a_fnName, args, a_callback);
        delete args;
        return result;
    }
} // namespace Script
