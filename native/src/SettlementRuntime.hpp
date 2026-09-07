#pragma once
// Shared reflected/native settlement primitives. Does not register hooks.
// Default target: non-consuming preview. PRF_MANUAL_SETTLEMENT_TEST builds a
// separate, manually triggered irreversible test; neither target installs AI hooks.
// The unreflected ABI below is pinned to the locally inspected executable.
#define NOMINMAX
#include <Windows.h>
#undef TEXT
#include <DynamicOutput/Output.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/FMemory.hpp>
#include <Unreal/FWeakObjectPtr.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "SettlementTypes.hpp"
#include <filesystem>
#include <optional>
#include <map>

namespace {
using namespace RC;
using namespace RC::Unreal;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename T> T find(const wchar_t* path) {
    // UE4SS's fallback StaticFindObject traverses GUObjectArray. Reflection
    // functions/classes/default objects are repeatedly used by machine ticks.
    // Weak references remain safe across world unload and Blueprint GC.
    static thread_local std::map<std::wstring, FWeakObjectPtr, std::less<>> resolved;
    auto entry = resolved.find(path);
    if (entry != resolved.end())
        if (auto* value = entry->second.Get()) return static_cast<T>(value);
    auto result = UObjectGlobals::StaticFindObject<T>(nullptr, nullptr, path);
    require(result != nullptr, "Required reflection object is not loaded");
    resolved.insert_or_assign(path, FWeakObjectPtr{result});
    return result;
}

struct Parameters {
    UFunction* function;
    std::vector<std::byte> data;
    explicit Parameters(UFunction* fn) : function(fn), data(fn->GetParmsSize()) {
        for (TFieldIterator<FProperty> it{fn}; it; ++it)
            if ((*it)->HasAnyPropertyFlags(CPF_Parm)) (*it)->InitializeValue_InContainer(data.data());
    }
    ~Parameters() {
        for (TFieldIterator<FProperty> it{function}; it; ++it)
            if ((*it)->HasAnyPropertyFlags(CPF_Parm)) (*it)->DestroyValue_InContainer(data.data());
    }
    Parameters(const Parameters&) = delete;
    Parameters& operator=(const Parameters&) = delete;
    void set_world(UObject* world) {
        if (!world) return;
        auto* property = CastField<FObjectProperty>(function->GetPropertyByName(STR("WorldContextObject")));
        require(property != nullptr, "Missing WorldContextObject parameter");
        *property->ContainerPtrToValuePtr<UObject*>(data.data()) = world;
    }
};

UObject* object_call(UObject* object, const wchar_t* path, UObject* world = nullptr) {
    auto* fn = find<UFunction*>(path);
    auto* result = CastField<FObjectProperty>(fn->GetReturnProperty());
    require(result != nullptr, "Unexpected object return type");
    Parameters params{fn};
    params.set_world(world);
    object->ProcessEvent(fn, params.data.data());
    return *result->ContainerPtrToValuePtr<UObject*>(params.data.data());
}

bool bool_call(UObject* object, const wchar_t* path, UObject* world = nullptr) {
    auto* fn = find<UFunction*>(path);
    auto* result = CastField<FBoolProperty>(fn->GetReturnProperty());
    require(result != nullptr, "Unexpected bool return type");
    Parameters params{fn};
    params.set_world(world);
    object->ProcessEvent(fn, params.data.data());
    return result->GetPropertyValueInContainer(params.data.data());
}

bool live(UObject* object) {
    return object && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject |
        RF_BeginDestroyed | RF_FinishDestroyed)) && !object->IsUnreachable();
}

// These are the *game's* layouts, not UE4SS's configurable FName/FVector wrappers.
struct NativeName { uint32_t index, number; };
struct NativeLocation { double x{}, y{}, z{}; };
struct NativeDrop { NativeName item; int32_t count, padding; NativeLocation location; };
struct NativeDrops { NativeDrop* data{}; int32_t num{}, capacity{}; };
static_assert(sizeof(NativeDrop) == 0x28 && offsetof(NativeDrop, location) == 0x10);
static_assert(sizeof(NativeDrops) == 0x10);
using CalculateButcher = void (*)(UObject*, NativeDrops*, UObject*, const NativeLocation*, UObject*);

CalculateButcher resolve_native() {
    auto* base = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    require(dos && dos->e_magic == IMAGE_DOS_SIGNATURE, "Missing game PE header");
    auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    require(pe->Signature == IMAGE_NT_SIGNATURE && pe->FileHeader.TimeDateStamp == 1788339267 &&
        pe->OptionalHeader.SizeOfImage == 0xa011000, "Unsupported executable: native preview disabled");
    constexpr std::array<uint8_t, 32> signature{
        0x48,0x8b,0xc4,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,
        0x8d,0xa8,0xb8,0xfe,0xff,0xff,0x48,0x81,0xec,0x08,0x02,0x00,0x00,0x0f,0x29,0x70};
    require(std::memcmp(base + 0x2e7a830, signature.data(), signature.size()) == 0,
        "Native butcher entry does not match inspected game");
    return reinterpret_cast<CalculateButcher>(const_cast<std::byte*>(base) + 0x2e7a830);
}

struct RootedTemporary {
    UObject* object;
    explicit RootedTemporary(UClass* cls, UObject* outer) {
        FStaticConstructObjectParameters params{cls, outer};
        params.SetFlags = RF_Transient;
        object = UObjectGlobals::StaticConstructObject(params);
        require(live(object), "Temporary parameter construction failed");
        require(!object->IsRootSet(), "Unexpected pre-rooted temporary parameter");
        object->SetRootSet();
    }
    ~RootedTemporary() { object->ClearRootSet(); }
    RootedTemporary(const RootedTemporary&) = delete;
    RootedTemporary& operator=(const RootedTemporary&) = delete;
};

#include "ManualSettlement.hpp"

} // namespace
