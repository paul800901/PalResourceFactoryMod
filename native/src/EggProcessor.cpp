#include "SettlementRuntime.hpp"
#include "FacilityPower.hpp"
#include "ProcessorState.hpp"
#include "AsyncDisk.hpp"
#include <chrono>
#include <fstream>
#include <iterator>
#include <set>
#include <mutex>
#include <Unreal/FWeakObjectPtr.hpp>
#include <Unreal/Property/FStrProperty.hpp>

namespace {
#include "ProcessorNative.hpp"

template<class T> T argument(UFunction* fn, void* params, const wchar_t* name) {
    auto* field = fn->GetPropertyByName(name);
    require(field && field->GetElementSize() == sizeof(T), "Processor input RPC layout differs");
    T value{};
    std::memcpy(&value, field->ContainerPtrToValuePtr<void>(params), sizeof(value));
    return value;
}

std::string container_key(const st::Guid& id) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto word : id.words) out << std::setw(8) << word;
    return out.str();
}

#include "ProcessorDeposit.hpp"

struct ProgressFile {
    prf::processor::State state;
    std::filesystem::path path;
    std::string last_written;
    AsyncDisk disk;
    explicit ProgressFile(const st::Guid& id) : path(settlement_directory().parent_path() / "progress" / (container_key(id) + ".txt")) {
        if (!std::filesystem::exists(path)) return;
        std::ifstream file(path, std::ios::binary);
        require(file.good(), "Cannot read processor checkpoint");
        last_written.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
        state = prf::processor::State::parse(last_written, disassembly_interval());
    }
    void flush() {
        if (disk.busy() && !disk.poll().has_value()) return;
        const auto text = state.serialize();
        if (text == last_written) return;
        disk.start([destination = path, text] {
        std::filesystem::create_directories(destination.parent_path());
        auto pending = destination;
        pending += ".pending";
        HANDLE file = CreateFileW(pending.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        require(file != INVALID_HANDLE_VALUE, "Cannot write processor checkpoint");
        DWORD written{};
        const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
            written == text.size() && FlushFileBuffers(file);
        CloseHandle(file);
        require(ok, "Processor checkpoint flush failed");
        require(MoveFileExW(pending.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
            "Cannot commit processor checkpoint");
        return true;
        });
        last_written = text;
    }
    void finish() { disk.drain(); flush(); disk.drain(); }
};

struct PendingSettlement {
    FWeakObjectPtr model;
    int slot;
    std::string egg;
    ManualSettlement transaction;
    int phase{};
    // Last member: its worker joins before transaction destruction.
    AsyncDisk disk;
    PendingSettlement(UObject* object, int index, std::string identity) : model(object), slot(index), egg(std::move(identity)), transaction(object, index) {
        disk.start([this] { return transaction.load_or_claim(); });
    }
    bool poll() {
        const auto done = disk.poll();
        if (!done.has_value()) return false;
        if (phase == 0) {
            require(model.Get() != nullptr, "Processor expired before settlement");
            const auto started = std::chrono::steady_clock::now();
            if (!*done) calculate_result(model.Get(), slot, transaction);
            Output::send<LogLevel::Normal>(STR("[PRFProcessor] SETTLEMENT_CPU phase=result ms={}\n"),
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
            const bool reused = *done;
            phase = 1;
            disk.start([this, reused] {
                if (!reused) transaction.persist_result();
                transaction.persist_applying();
                return true;
            });
        } else if (phase == 1) {
            const auto started = std::chrono::steady_clock::now();
            transaction.attempt(true); // APPLYING is durable; all UObject work stays on Tick.
            Output::send<LogLevel::Normal>(STR("[PRFProcessor] SETTLEMENT_CPU phase=apply-and-queue-ground ms={}\n"),
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
            phase = 2;
            disk.start([this] { transaction.persist_ground_requested(); return true; });
        } else return true;
        return false;
    }
};

void lock_demolition_contents(UObject* model) {
    auto* module = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
    require(live(module), "Processor container module not ready");
    auto* field = CastField<FBoolProperty>(module->GetPropertyByNameInChain(STR("bDropItemAtDisposed")));
    require(field, "Processor demolition policy unavailable");
    field->SetPropertyValueInContainer(module, false);
}

class EggProcessor final : public CppUserModBase {
    enum class Request { Swap, Move, MoveContainer, Drop, Dispose, Filter, Sort };
    std::map<UFunction*, Request> requests;
    std::map<st::Guid, ProgressFile> progress;
    std::map<st::Guid, std::unique_ptr<PendingSettlement>> settlements;
    std::set<st::Guid> suspended;
    std::map<st::Guid, std::string> last_errors;
    Hook::GlobalCallbackId tick{Hook::ERROR_ID}, inputs{Hook::ERROR_ID};
    Hook::GlobalCallbackId unload{Hook::ERROR_ID};
    Hook::GlobalCallbackId constructed{Hook::ERROR_ID};
    UClass* storage_class{};
    std::atomic<UClass*> chest_class{};
    UFunction* chest_take_key{};
    UFunction* chest_take_click{};
    struct ChestView {
        FWeakObjectPtr widget;
        bool labelled{};
        FWeakObjectPtr target;
        bool failed{};
        std::wstring caption;
        std::vector<std::pair<FWeakObjectPtr, std::wstring>> labels;
    };
    bool refreshing_chests{};
    std::vector<ChestView> chest_views;
    std::mutex candidates_mutex;
    std::vector<FWeakObjectPtr> candidates;
    double elapsed{}, checkpoint_elapsed{}, ui_elapsed{};
    bool handling_rpc{};
    bool status_warning{};
    bool ready{};
    struct DisplayState {
        FWeakObjectPtr actor;
        int state{-1};
        std::array<int, 2> activity{};
    };
    std::map<st::Guid, DisplayState> displays;

    UObject* chest_processor(UObject* widget) {
        auto* target = optional_object_field(widget, STR("TargetContainer"));
        if (!live(target)) return nullptr;
        for (auto* model : processors()) {
            if (!facility_power::constructed(model)) continue;
            auto* module = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
            if (live(module) && object_call(module, STR("/Script/Pal.PalMapObjectItemContainerModule:GetContainer")) == target) return model;
        }
        return nullptr;
    }

    void refresh_chests() {
        if (refreshing_chests) return;
        refreshing_chests = true;
        struct Reset { bool& value; ~Reset() { value = false; } } reset{refreshing_chests};
        if (!chest_take_click) {
            if (!chest_class) chest_class = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr,
                STR("/Game/Pal/Blueprint/UI/UserInterface/IngameMenu/Chest/WBP_IngameMenu_Chest.WBP_IngameMenu_Chest_C"));
            if (!chest_class) return;
            const std::wstring prefix = L"/Game/Pal/Blueprint/UI/UserInterface/IngameMenu/Chest/WBP_IngameMenu_Chest.WBP_IngameMenu_Chest_C:";
            // INPUT_ACTION_ItemStorageFastAllGet is an action name, not a
            // UFunction. Keyboard/controller take-all routes through this model.
            chest_take_key = find<UFunction*>(STR("/Script/Pal.PalUIInventoryModel:TryMoveContainerToInventory"));
            chest_take_click = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, (prefix + L"BndEvt__WBP_IngameMenu_Chest_WBP_PalInvisibleButton_QuickMove_K2Node_ComponentBoundEvent_1_CommonButtonBaseClicked__DelegateSignature").c_str());
            require(chest_take_click, "Chest take-all click entry unavailable");
            std::vector<UObject*> existing;
            UObjectGlobals::FindAllOf(STR("WBP_IngameMenu_Chest_C"), existing);
            std::scoped_lock lock{candidates_mutex};
            for (auto* widget : existing) if (live(widget) && std::none_of(chest_views.begin(), chest_views.end(),
                [&](const auto& view) { return view.widget.Get() == widget; })) chest_views.push_back({FWeakObjectPtr{widget}, false});
        }
        require(chest_take_click, "Chest take-all click entry unavailable");
        std::vector<ChestView> views;
        { std::scoped_lock lock{candidates_mutex}; views = chest_views; }
        for (auto& view : views) {
            auto* widget = view.widget.Get();
            if (!live(widget)) continue;
            auto* target = optional_object_field(widget, STR("TargetContainer"));
            if (view.target.Get() != target) { view.failed = false; view.target = FWeakObjectPtr{target}; }
            if (view.failed) continue;
            try {
                const bool processor = chest_processor(widget) != nullptr;
                if (processor && (view.labels.empty() || !live(view.labels.front().first.Get()))) {
                    std::vector<UObject*> labels;
                    DepositLabelSearch search;
                    auto* panel = optional_object_field(widget, STR("Canvas_QuickMoveButton"));
                    deposit_label_tree(live(panel) ? panel : widget, labels, search, live(panel));
                    require(!labels.empty(), "Processor take-all label not found");
                    view.labels.clear();
                    view.caption = deposit_label();
                    for (auto* label : labels) view.labels.emplace_back(FWeakObjectPtr{label}, text_value(label));
                }
                if (processor) {
                    // Read cached TextBlocks only: repair native text reset on
                    // quick reopen, without rescanning the inventory widget tree.
                    for (const auto& [label, original] : view.labels)
                        if (live(label.Get()) && text_value(label.Get()) != view.caption)
                            set_text_value(label.Get(), view.caption.c_str());
                } else if (view.labelled) {
                    for (const auto& [label, original] : view.labels)
                        if (live(label.Get())) set_text_value(label.Get(), original.c_str());
                    view.labels.clear();
                }
                view.labelled = processor;
            } catch (const std::exception& error) {
                view.failed = true; // Do not repeat a failed search every UI tick.
                Output::send<LogLevel::Warning>(STR("[PRFProcessor] LABEL_UPDATE_STOPPED {}\n"), to_wstring(error.what()));
            }
            std::scoped_lock lock{candidates_mutex};
            for (auto& stored : chest_views) if (stored.widget.Get() == widget) stored = view;
        }
    }

    bool handle_chest_deposit(Hook::TCallbackIterationData<void>& call, UObject* widget, UFunction* fn, void* params) {
        if (!ready || (fn != chest_take_key && fn != chest_take_click) || !live(widget)) return false;
        UObject* model{};
        if (fn == chest_take_key) {
            auto* target = argument<UObject*>(fn, params, STR("fromContainer"));
            for (auto* candidate : processors()) {
                if (ManualSettlement::model_container(candidate).object == target) { model = candidate; break; }
            }
        } else if (widget->GetClassPrivate() == chest_class) model = chest_processor(widget);
        if (!model) return false;
        call.PreventOriginalFunctionCall();
        // Block the old outward operation even if the inward request fails.
        try { deposit_backpack_eggs(model, model); }
        catch (const std::exception& e) {
            Output::send<LogLevel::Error>(STR("[PRFProcessor] DEPOSIT_ALL_REFUSED {}\n"), to_wstring(e.what()));
        }
        return true;
    }

    void show_status(UObject* model, int state) {
        try {
            const auto source = ManualSettlement::model_container(model);
            const auto it = progress.find(source.id);
            const auto counts = it == progress.end() ? std::array<int, 2>{} : it->second.state.display_counts();
            const std::array<int, 2> activity{counts[0] > 0, counts[1] > 0};
            auto* actor = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetActor"));
            if (!live(actor)) return;
            auto& previous = displays[source.id];
            if (previous.actor.Get() == actor && previous.state == state && previous.activity == activity) return;
            if (previous.actor.Get()!=actor || previous.state!=state) {
                auto* energy=facility_power::module(model);
                Output::send<LogLevel::Normal>(STR("[PRFProcessor] STATUS state={} energy-module={} powered={}\n"),
                    state,live(energy),live(energy) && facility_power::available(model));
            }
            // Time-driven material animation runs independently of inventory changes.
            if (processor_status(actor, state, activity)) {
                previous.actor = FWeakObjectPtr{actor};
                previous.state = state;
                previous.activity = activity;
            }
        }
        catch (const std::exception& e) {
            if (!status_warning) Output::send<LogLevel::Warning>(STR("[PRFProcessor] STATUS_DISPLAY_UNAVAILABLE {}\n"), to_wstring(e.what()));
            status_warning = true;
        }
    }

    std::vector<UObject*> processors() {
        std::vector<FWeakObjectPtr> current;
        {
            std::scoped_lock lock{candidates_mutex};
            std::erase_if(candidates, [](const FWeakObjectPtr& weak) { return weak.Get() == nullptr; });
            current = candidates;
        }
        std::vector<UObject*> result;
        for (const auto& weak : current) {
            auto* model = weak.Get();
            if (is_processor(model)) result.push_back(model);
        }
        return result;
    }
    void reject_mutation(Hook::TCallbackIterationData<void>& call, UObject* context, UFunction* fn, void* params) {
        const auto found = requests.find(fn);
        if (!ready || found == requests.end() || handling_rpc) return;
        handling_rpc = true;
        struct Reset { bool& flag; ~Reset() { flag = false; } } reset{handling_rpc};
        // Only exact PRF input identities are inspected; other inventories,
        // original incubators, Pal actions and AI functions are untouched.
        for (auto* model : processors()) {
            const auto source = ManualSettlement::model_container(model);
            lock_demolition_contents(model);
            auto ours = [&](const st::SlotId& slot) { return slot.container == source.id; };
            auto occupied = [&](const st::SlotId& slot) {
                return ours(slot) && slot.index >= 0 && static_cast<size_t>(slot.index) < source.slots.size() &&
                    source.slots[slot.index].state.count > 0;
            };
            auto from_array = [&](const wchar_t* name) {
                auto* field = CastField<FArrayProperty>(fn->GetPropertyByName(name));
                require(field && field->GetInner()->GetElementSize() == sizeof(st::Consume), "Input movement array differs");
                FScriptArrayHelper values{field, field->ContainerPtrToValuePtr<void>(params)};
                for (int i = 0; i < values.Num(); ++i) {
                    st::Consume value{};
                    std::memcpy(&value, values.GetRawPtr(i), sizeof(value));
                    if (ours(value.slot)) return true;
                }
                return false;
            };
            bool block{};
            switch (found->second) {
            case Request::Swap:
                block = occupied(argument<st::SlotId>(fn, params, STR("SlotA"))) ||
                    occupied(argument<st::SlotId>(fn, params, STR("SlotB")));
                break;
            case Request::Move:
                block = from_array(STR("Froms")) || occupied(argument<st::SlotId>(fn, params, STR("To")));
                break;
            case Request::MoveContainer: block = from_array(STR("Froms")); break;
            case Request::Drop: block = from_array(STR("DropSlotAndNumArray")); break;
            case Request::Dispose: block = ours(argument<st::Consume>(fn, params, STR("SlotInfo")).slot); break;
            case Request::Filter: block = argument<st::Guid>(fn, params, STR("ContainerId")) == source.id; break;
            case Request::Sort:
                block = context == object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
                break;
            }
            if (block) {
                call.PreventOriginalFunctionCall();
                Output::send<LogLevel::Normal>(STR("[PRFProcessor] INPUT_LOCKED container={} action={}\n"),
                    to_wstring(container_key(source.id)), fn->GetName());
                return;
            }
        }
    }
    void update(double seconds) {
        checkpoint_elapsed += seconds;
        auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
        for (auto* model : processors()) {
            if (!facility_power::constructed(model)) continue;
            if (!bool_call(utility, STR("/Script/Pal.PalUtility:IsServer"), model)) continue;
            if (bool_call(utility, STR("/Script/Pal.PalUtility:IsDedicatedServer"), model) ||
                bool_call(utility, STR("/Script/Pal.PalUtility:IsOpenListenServer"), model)) continue;
            // A discovered model can precede its replicated/persistent container.
            auto* module = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
            if (!live(module)) continue;
            auto* container = object_call(module, STR("/Script/Pal.PalMapObjectItemContainerModule:GetContainer"));
            if (!live(container)) continue;
            const auto source_id = read_field<st::Guid>(container, STR("ID"), 0x38);
            try {
                const auto source = ManualSettlement::read_container(container);
                lock_demolition_contents(model);
                require(source.slots.size() == 54, "Dedicated processor input must have 54 slots");
                if (suspended.contains(source.id)) continue;
                auto* save = object_call(utility, STR("/Script/Pal.PalUtility:GetSaveGameManager"), model);
                if (!live(save) || bool_call(save, STR("/Script/Pal.PalSaveGameManager:IsWorldAutoSaving"))) continue;
                auto* gameplay = find<UObject*>(STR("/Script/Engine.Default__GameplayStatics"));
                if (bool_call(gameplay, STR("/Script/Engine.GameplayStatics:IsGamePaused"), model)) continue;
                auto [it, fresh] = progress.try_emplace(source.id, source.id);
                auto& file = it->second;
                auto& state = file.state;
                if (fresh) Output::send<LogLevel::Normal>(STR("[PRFProcessor] ATTACHED input=54 container={} power=200/400 hotkey=none\n"),
                    to_wstring(container_key(source.id)));
                bool inserted{};
                for (size_t i = 0; i < source.slots.size(); ++i)
                    if (source.slots[i].state.count == 0) state.empty(i);
                // Advance only eggs present at the previous observation. A new
                // input must not inherit time elapsed before it was inserted.
                const bool incubating = std::any_of(state.jobs.begin(), state.jobs.end(), [](const auto& job) {
                    return !job.egg.empty() && job.remaining > 0;
                });
                const bool powered = facility_power::update(model, incubating ? 400.0f : 200.0f);
                const auto due = powered ? state.advance(seconds, disassembly_interval(), disassembly_capacity()) : std::nullopt;
                for (size_t i = 0; i < source.slots.size(); ++i) {
                    const auto& slot = source.slots[i];
                    if (slot.state.count == 0) { state.empty(i); continue; }
                    const auto key = egg_key(slot.state.item.dynamic);
                    if (state.jobs[i].egg == key) continue;
                    auto* egg = dynamic_egg(slot);
                    const auto seconds_required = incubation_seconds(model, egg);
                    state.inserted(i, key, seconds_required);
                    inserted = true;
                    Output::send<LogLevel::Normal>(STR("[PRFProcessor] INPUT_LOCKED slot={} egg={} hatch-seconds={}\n"),
                        i, to_wstring(key), seconds_required);
                }
                if (inserted) file.flush();
                if (due && !settlements.contains(source.id)) {
                    file.flush();
                    settlements.emplace(source.id, std::make_unique<PendingSettlement>(model, static_cast<int>(*due), state.jobs[*due].egg));
                }
                if (checkpoint_elapsed >= 5.0) file.flush();
                const bool occupied = std::any_of(state.jobs.begin(), state.jobs.end(), [](const auto& job) { return !job.egg.empty(); });
                show_status(model, !powered ? 3 : (state.processing() ? 2 : (occupied ? 1 : 0)));
                last_errors.erase(source.id);
            } catch (const std::exception& error) {
                // A claimed native settlement must never be retried with a new
                // roll. Freeze this machine on failure; do not stall world AI.
                if (last_errors[source_id] != error.what()) {
                    Output::send<LogLevel::Error>(STR("[PRFProcessor] PAUSED container={} reason={}\n"),
                        to_wstring(container_key(source_id)), to_wstring(error.what()));
                    last_errors[source_id] = error.what();
                }
                suspended.insert(source_id);
                show_status(model, 3);
            }
        }
        if (checkpoint_elapsed >= 5.0) checkpoint_elapsed = 0;
    }
    void poll_settlements() {
        for (auto it = settlements.begin(); it != settlements.end();) {
            try {
                // Auto-save/pause may begin while disk work is pending. Resume
                // game mutations only after that boundary ends.
                auto* model = it->second->model.Get();
                if (live(model)) {
                    if (!facility_power::available(model)) { ++it; continue; }
                    auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
                    auto* save = object_call(utility, STR("/Script/Pal.PalUtility:GetSaveGameManager"), model);
                    if (live(save) && bool_call(save, STR("/Script/Pal.PalSaveGameManager:IsWorldAutoSaving"))) { ++it; continue; }
                    if (bool_call(find<UObject*>(STR("/Script/Engine.Default__GameplayStatics")), STR("/Script/Engine.GameplayStatics:IsGamePaused"), model)) { ++it; continue; }
                }
                const bool complete = it->second->poll();
                auto found = progress.find(it->first);
                if (it->second->phase >= 2 && found != progress.end()) {
                    // A new egg may enter the freed slot while the final receipt
                    // is flushing. Never erase its incubation progress.
                    if (found->second.state.jobs[it->second->slot].egg == it->second->egg) {
                        found->second.state.empty(it->second->slot);
                        found->second.flush();
                    }
                }
                if (!complete) { ++it; continue; }
                it = settlements.erase(it);
            } catch (const std::exception& e) {
                suspended.insert(it->first);
                Output::send<LogLevel::Error>(STR("[PRFProcessor] PAUSED container={} reason={} no-replay=true\n"),
                    to_wstring(container_key(it->first)), to_wstring(e.what()));
                it = settlements.erase(it);
            }
        }
    }
public:
    EggProcessor() {
        ModName = STR("PalResourceFactoryProcessor");
        ModVersion = STR("0.3.15-localization");
        ModDescription = STR("Dedicated 54-slot egg processor; original native hatch, butcher and ground output");
        ModAuthors = STR("Paulus");
    }
    ~EggProcessor() override {
        if (constructed != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(constructed));
        if (unload != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(unload));
        if (inputs != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(inputs));
        if (tick != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(tick));
        settlements.clear(); // Join disk-only writes, never finish game mutations on unload.
        for (auto& [_, file] : progress) { try { file.finish(); } catch (...) {} }
    }
    void on_unreal_init() override {
        try {
            SettlementEntries native;
            storage_class = find<UClass*>(STR("/Script/Pal.PalMapObjectItemStorageModel"));
            constructed = Hook::RegisterStaticConstructObjectPostCallback(
                [this](Hook::TCallbackIterationData<UObject*>& call, const FStaticConstructObjectParameters& params) {
                    if (!chest_class && params.Class && params.Class->GetName() == STR("WBP_IngameMenu_Chest_C")) chest_class = const_cast<UClass*>(params.Class);
                    if (chest_class && params.Class == chest_class) {
                        if (auto* widget = call.GetCurrentResolvedReturnValue()) {
                            std::scoped_lock lock{candidates_mutex};
                            std::erase_if(chest_views, [](const auto& view) { return view.widget.Get() == nullptr; });
                            chest_views.push_back({FWeakObjectPtr{widget}, false});
                        }
                        return;
                    }
                    if (params.Class != storage_class) return;
                    // Construction can happen off-thread. Queue a serial-checked
                    // weak reference only; model/world inspection waits for Tick.
                    auto* object = call.GetCurrentResolvedReturnValue();
                    if (!object) return;
                    std::scoped_lock lock{candidates_mutex};
                    if (std::none_of(candidates.begin(), candidates.end(), [&](const auto& weak) { return weak.Get() == object; }))
                        candidates.emplace_back(object);
                }, Hook::FCallbackOptions{false, true, ModName, STR("ProcessorModelDiscovery")});
            require(constructed != Hook::ERROR_ID, "Processor discovery callback unavailable");
            // A single startup catch-up scan, never a recurring whole-world scan.
            std::vector<UObject*> existing;
            UObjectGlobals::FindAllOf(STR("PalMapObjectItemStorageModel"), existing);
            {
                std::scoped_lock lock{candidates_mutex};
                for (auto* object : existing) if (object->GetClassPrivate() == storage_class && live(object)) {
                    const bool known = std::any_of(candidates.begin(), candidates.end(),
                        [&](const auto& weak) { return weak.Get() == object; });
                    if (!known) candidates.emplace_back(object);
                }
            }
            auto request = [&](const wchar_t* name, Request kind) {
                const auto path = std::wstring{L"/Script/Pal.PalNetworkItemComponent:"} + name;
                requests.emplace(find<UFunction*>(path.c_str()), kind);
            };
            request(L"RequestSwap_ToServer", Request::Swap);
            request(L"RequestMove_ToServer", Request::Move);
            request(L"RequestMoveToContainer_ToServer", Request::MoveContainer);
            request(L"RequestDrop_ToServer", Request::Drop);
            request(L"RequestDispose_ToServer", Request::Dispose);
            request(L"RequestChangeFilter_ToServer", Request::Filter);
            request(L"RequestChangeAllFilterCheck_ToServer", Request::Filter);
            request(L"RequestChangeAllFilterUncheck_ToServer", Request::Filter);
            requests.emplace(find<UFunction*>(STR("/Script/Pal.PalMapObjectItemContainerModule:RequestSortContainer_ServerInternal")), Request::Sort);
            // Check reflected argument layouts before accepting any input. A
            // changed RPC must not silently turn a locked egg into removable loot.
            for (const auto& [fn, kind] : requests) {
                auto size = [&](const wchar_t* field_name, size_t expected) {
                    auto* field = fn->GetPropertyByName(field_name);
                    require(field && field->GetElementSize() == expected, "Input RPC arguments changed; processor disabled");
                };
                if (kind == Request::Swap) { size(STR("SlotA"), 20); size(STR("SlotB"), 20); }
                if (kind == Request::Dispose) size(STR("SlotInfo"), 24);
                if (kind == Request::Filter) size(STR("ContainerId"), 16);
                if (kind == Request::Move) size(STR("To"), 20);
                if (kind == Request::Move || kind == Request::MoveContainer || kind == Request::Drop) {
                    auto* field = CastField<FArrayProperty>(fn->GetPropertyByName(kind == Request::Drop ? STR("DropSlotAndNumArray") : STR("Froms")));
                    require(field && field->GetInner()->GetElementSize() == 24, "Input RPC array changed; processor disabled");
                }
            }
            inputs = Hook::RegisterProcessEventPreCallback(
                [this](Hook::TCallbackIterationData<void>& call, UObject* context, UFunction* fn, void* params) {
                    try { if (!handle_chest_deposit(call, context, fn, params)) reject_mutation(call, context, fn, params); }
                    catch (const std::exception& e) {
                        Output::send<LogLevel::Error>(STR("[PRFProcessor] INPUT_CHECK_FAILED {}\n"), to_wstring(e.what()));
                    }
                }, Hook::FCallbackOptions{false, false, ModName, STR("ProcessorInputOnly")});
            require(inputs != Hook::ERROR_ID, "Input lock callback unavailable; automatic processing disabled");
            tick = Hook::RegisterEngineTickPostCallback(
                [this](Hook::TCallbackIterationData<void>&, UEngine*, float delta, bool idle) {
                    if (!ready || idle || !std::isfinite(delta) || delta <= 0) return;
                    poll_settlements();
                    ui_elapsed += delta;
                    if (ui_elapsed >= 0.1) {
                        ui_elapsed = 0;
                        try { refresh_chests(); }
                        catch (const std::exception& e) {
                            if (!status_warning) Output::send<LogLevel::Warning>(STR("[PRFProcessor] DEPOSIT_UI_UNAVAILABLE {}\n"), to_wstring(e.what()));
                            status_warning = true;
                        }
                    }
                    elapsed += std::min(static_cast<double>(delta), 0.25);
                    if (elapsed < 0.1) return;
                    const auto step = std::exchange(elapsed, 0.0);
                    try { update(step); }
                    catch (const std::exception& e) {
                        Output::send<LogLevel::Error>(STR("[PRFProcessor] UPDATE_REFUSED {}\n"), to_wstring(e.what()));
                    }
                }, Hook::FCallbackOptions{false, false, ModName, STR("ProcessorClock")});
            require(tick != Hook::ERROR_ID, "Processor clock unavailable");
            unload = Hook::RegisterLoadMapPreCallback(
                [this](Hook::TCallbackIterationData<bool>&, UEngine*, FWorldContext&, FURL, UPendingNetGame*, FString&) {
                    settlements.clear();
                    for (auto& [id, file] : progress) {
                        try { file.finish(); }
                        catch (const std::exception& e) {
                            Output::send<LogLevel::Error>(STR("[PRFProcessor] CHECKPOINT_FAILED container={} {}\n"),
                                to_wstring(container_key(id)), to_wstring(e.what()));
                        }
                    }
                    // Only value state is discarded. No callback unregisters
                    // itself while running (the UE4SS hook mutex is not recursive).
                    progress.clear(); suspended.clear(); last_errors.clear(); displays.clear(); elapsed = 0;
                    { std::scoped_lock lock{candidates_mutex}; chest_views.clear(); }
                    chest_class = nullptr; chest_take_key = nullptr; chest_take_click = nullptr;
                }, Hook::FCallbackOptions{false, false, ModName, STR("ProcessorCheckpoint")});
            require(unload != Hook::ERROR_ID, "Processor world-exit checkpoint callback unavailable");
            ready = true;
            Output::send<LogLevel::Warning>(STR("[PRFProcessor] READY version=0.3.15-localization scope=PRF_EggResourceProcessor power=200/400 solo-only=true game-verified=false breeder=disabled\n"));
        } catch (const std::exception& e) {
            Output::send<LogLevel::Error>(STR("[PRFProcessor] DISABLED {}\n"), to_wstring(e.what()));
        }
    }
};
} // namespace

extern "C" {
__declspec(dllexport) RC::CppUserModBase* start_mod() { return new EggProcessor(); }
__declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
