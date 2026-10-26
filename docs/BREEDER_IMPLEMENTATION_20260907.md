# 第二台重建進度

## 0.1.4 領取／重載修正（本機編譯通過，待實機）

- 0.1.3 使用者實測：獲得所有孵化帕魯仍可領取；正常重進後不再支解。日誌 19:57:55 已 ATTACHED，19:58:00 因既有非 READY 收據觸發整機 PAUSED；不是漏掉建築發現。該錯誤未列蛋 ID，因此不能把最後一顆已出料蛋直接認定為衝突蛋。
- 舊 InputGuard 只使用父類同名 UFunction 與 ProcessEvent；現在從 concrete 的函式繼承鏈找目前 EXE 原生入口 0x29d42e0，攔截多槽孵化器的 ObtainHatchedCharacter_ServerInternal。唯讀 PE 檢查確認此 thunk 呼叫 0x303c070；直接 call 搜尋只找到此 thunk，不代表所有間接呼叫已實機覆蓋。
- 新入口僅阻擋 PRF_ResourceBreedingFacility，其餘原版機器呼叫原函式；阻擋時仍消耗 VM 引數並前進 bytecode，避免破壞 Blueprint 堆疊。原 ProcessEvent 與容器鎖仍保留。
- 第二台重新見到 GROUND_REQUESTED 的原蛋時，僅執行既有原版來源移除及完成資料清除／讀回，不重算 RNG、不再呼叫出料。其他非 READY 紀錄保留該蛋並隔離，不再令整機停止。READY 仍使用原固定結果；第一台的預設拒絕重播語意不變。
- 新日誌記錄衝突蛋 ID 與階段，以及 RESTORED_SOURCE_REMOVED／EGG_QUARANTINED；收據未刪除或改寫。這解決已知的整機暫停路徑，不宣稱所有崩潰情境無損；遊戲存檔與外部收據的具體落差仍需下一次實測讀回。
- 第二台 DLL 編譯及既有 CTest 5/5 通過；核心測試不執行 Unreal 原生入口或實機重載。測試步驟見 ANCIENT_SETTLEMENT_TEST.md。
- 20:19 安裝 0.1.4，前後確認遊戲程序未執行。DLL 讀回 SHA256 `0899769D413AC0B8EAECF643D28716C52F63D7359ACFD5AC2E8E912222A3DDF7`；舊 DLL／README／本輪失敗日誌備份至 `build-processor/ancient-obtain-reload-backup-20260907-201909-477`。第一台 DLL、第二台 schema、mods.txt hash 均未變；未啟動遊戲，實機結果待測。

## 0.1.3 支解整合已安裝，待實機

- 使用者提供自訂機器 UI 的十顆「孵化完成」截圖，確認原版前段已能跑到完成；未證明每批蛋糕消耗量或所有遺傳屬性。
- 使用者確認關閉，程序清單讀回無 Palworld/PalServer。啟用 `ancient_production_enabled`，版本 0.1.3-settlement-integration-test；不改交易實作、schema 或第一台。
- 編譯及 CTest 5/5 通過。EXE hash 符合 v1.0.4；第二台 DLL 安裝 SHA256 `646B476E24A781C24F7E59D4B7DA4173E21569D965F48B000B0AD230EBC3014C`。舊版先備份至 build-processor/ancient-settlement-backup-*；安裝後 hash 一致，第一台 DLL、第二台 schema、mods.txt 均未變。
- 本輪只完成安裝，不宣稱出料成功。下一步依 `ANCIENT_SETTLEMENT_TEST.md` 使用現存完成蛋測試，嚴苛斷線及第一台刀型仍延後。

## 自訂第二台空機實機讀回通過（2026-09-07 19:37）

- 使用者提供指派面板與空機 HUD 截圖，並回報「沒看到問題」。本輪可確認指派面板顯示自訂名稱及兩格位置、HUD 顯示未配對／蛋糕不足／電力不足；不能僅靠這些圖宣稱箱子所有操作通過。
- UE4SS.log 19:36:18 `[PRFAncient] ATTACHED egg-slots=10 ... settlement-enabled=false`。19:37:58 F7 精確讀到 BuildObjectId/MapObjectMasterDataId=`PRF_ResourceBreedingFacility`，wrapper/concrete/work 皆有效，工作 class=`PalWorkOnlyJoinInvisible`。
- 自訂三張資料列讀回：CommonTemp/None、兩名額、Electric/1200；蛋糕箱 5 格、蛋槽 10 格且不同容器，EGG_RECORDS=0、BreedProgressTime=0、BreedStoppedReason=NoPair。與空機畫面一致，非先前無效初始化狀態。
- 同次也讀到原版古代配種器；探針 `original-actors=2` 是相同 Blueprint class 的 actor 數，不是兩台原版。本次已由自訂 ID 分開辨識，不能混用原版十筆蛋紀錄。
- 目前日誌未見 PRFAncient START_REFUSED/ATTACH_REFUSED/PAUSED/DISABLED/INPUT_GUARD_FAILED 或 PRFBreederProbe REFUSED。沒有改遊戲檔案或啟用支解。
- 結論限於本次空機初始化與所示 UI 通過，不代表所有歷史轉圈成因皆已定位，也不代表完整生產／活體封鎖／重載通過。下一步為供電、兩親代及單顆蔬菜蛋糕的原版前段驗證；仍使用本版，支解停用，不手動投入蛋、不領取或移出後代。嚴苛斷線與第一台刀型延後。

## 第二台空機初始化測試已安裝（2026-09-07 19:20，尚未實機）

- 使用者確認完全關閉；程序清單讀回沒有 Palworld/PalServer，EXE hash 仍符合 v1.0.4 基線。
- `AncientBreeder.cpp` 0.1.2-initialization-only：編譯常數 `ancient_production_enabled=false`，保留 discovery/attach/input guard，但不呼叫 update 支解排程，不建立結算交易。原版配種仍可運作，所以本輪只測空機，禁止測試者投入親代／蛋糕／蛋。
- 依上一節原版實值修正的獨立 schema 與第二台 DLL 已首次安裝；不是重新安裝隔離版 0.3.5，沒有使用舊安裝器、combined schema 或自製 PAK。已確認的設定差異允許進行有限空機測試，不代表歷史故障根因全部排除。
- DLL SHA256 `F0FE078AB679621BE7F1FF7B9F19C877D252D75C2777CB3ECF260019BEE1EF61`；schema SHA256 `3012C39E3FF5CC9BAB322B7633DDC2FE35C776195D832B12135DF14033EC615C`，安裝後逐項讀回一致。
- mods.txt 只新增 `PalResourceFactoryAncientBreeder : 1`。原檔備份 `build-processor/ancient-init-install-20260907-192010-923/mods.txt`。第一台 DLL hash 仍為 `4C45741E9C59F393313151330B9D8C65FD4E85EF24B1EB1F22124AE7DF67239A`，未動其 schema/PAK。
- 編譯成功，CTest 5/5；這些不證明 Unreal 初始化成功。請依 `ANCIENT_INITIALIZATION_TEST.md` 用全新可丟棄單人世界建造空機、開關 UI、按 F7 讀回。不得稱為第二台生產完成。

## 原版初始化實值已取得（2026-09-07 19:08，schema 本地修正，未安裝）

- 遊戲 UE4SS.log 19:08:34 探針 0.3.2：`original-actors=1 writes=0 RNG=0`，三張原版資料列讀回完成，wrapper／concrete／work／cake container／egg container 均有效。工作物件實際為 `PalWorkOnlyJoinInvisible`。
- 原版指派列 `MultiElectricHatchingPalEggWithBreed_0`：WorkType=CommonTemp、WorkActionType=None、WorkSuitability=Anyone、WorkSuitabilityRank=1、WorkerMaxNum=2、AffectFullStomachValue=1、MultiRequiredRank1/2=1。先前 schema 的 AncientBreedFarm／CommonWork 並非原版實值，不能由 enum 名稱推斷應使用哪個工作類型。
- 原版建築列：RequiredEnergyType=Electric、ConsumeEnergySpeed=1200、TypeUIDisplay=PalTraining。獨立第二台 `breeder.json` 本地補齊這些設定，並修正上述指派值；不改 ID、Blueprint、測試成本／科技、第一台或遊戲安裝檔。
- 實際容器彼此獨立：蛋糕 5 格、孵化蛋 10 格；當時 EGG_RECORDS=10、BreedStoppedReason=SlotFull、BreedProgressTime=10。此為原版滿槽停機狀態，不是初始化失敗；不能將 5 格蛋糕箱誤當孵化容量，也不能把當時 10 筆紀錄全數宣稱已完成孵化。
- 原版 concrete 的 MenuUIWidgetClass 有效，DefaultConsumeEnergySpeed=1200、DefaultAutoWorkAmountBySec=1200。這些證據補足原版基準，**尚未證明自訂 ID 的轉圈根因全部排除，也未驗證第二台生產**。
- 下一步使用已取得的基準核對自訂建築初始化／封閉出口；無需再要求使用者重按同一個原版唯讀採集。第二台生產 DLL/schema 仍未安裝，第一台保持不動。

## 初始化鏈與原版資料列讀回（2026-09-07 17:12，探針已安裝，待實機）

- 從目前遊戲 `Pal-Windows.pak` 以 UnrealPak `-Extract`、`-Filter=*Multi*Hatching*Breed*` 抽取相關資產至 `build/v104-ancient-assets`；不再以舊 v1.0.3 提取物作為目前 Blueprint 證據。這次只有資產提取及字串檢視，沒有完成 Blueprint bytecode 反編譯，不能據此宣稱沒有硬編碼 ID。
- 本地 PalSchema 來源 `PalBuildingModLoader.cpp::Add/SetupBuildData/SetupAssignment` 對新 ID 初始化新資料列，再填 JSON 欄位；不是複製原版整列。因此相同 Blueprint 路徑不證明其他初始化條件一致。此為本地來源證據，仍須遊戲實際資料讀回；不能直接判定為轉圈根因。
- `AncientBreederReadback.hpp` 補上 actor → MapObjectModel → ConcreteModel、WorkeeModuleCache → TargetWork、完成蛋紀錄數、蛋糕容器及孵化容器分開讀回。容器只記錄有效性／槽數，不匯出後代 SaveParameter。
- 依實際 wrapper 的 MapObjectMasterDataId／BuildObjectId，只匯出已載入的原版 master/build/assignment 相符資料列；不載入資產、不新增／修改資料列、不扣糕、不呼叫生成、孵化、領取或支解。沿用手動 Ctrl+Alt+F7；尚未拿到這一版的實機輸出。
- 探針 `0.3.2-ancient-initialization-read-only` 乾淨重編通過；既有 CTest 5/5 通過，不涵蓋 Unreal 實機反射。安裝檔 SHA256：`14797D6AB6416EA97F46053BCDDAF206188277F621B3728DF9A42EE07CF71585`。
- 安裝前兩次確認 Palworld／PalServer 未執行；EXE SHA256 仍為 `44B6295E70AA37B83D1C42CE1DCF865A7FFADCD300298B0E49A02BAD8EB83443`。舊探針備份：`build-processor/ancient-initialization-backup-20260907-171254-539/main.dll`。新 DLL 已安裝並 hash 讀回一致；第一台 DLL 與 mods.txt hash 均未改。
- **第二台生產 DLL／schema 仍未安裝，舊轉圈原因仍未確認。** 下一次需在原版古代配種器旁按一次熱鍵，核對 LINK／WORK／CONTAINER／TABLES；不是再要求完成一次配種。第一台刀型及嚴苛斷線測試仍延後。
- 此次 `--clean-first` 清除 build-processor 先前編譯產物後僅重建探針；下方第二台 DLL hash 為歷史建置紀錄，要安裝前須重新編譯及驗證。

## Blueprint 子類發現修正（2026-09-07，未安裝）

- 16:46:26 探針實機讀回 `original-actors=1 writes=0 RNG=0`。原版 actor 的 ConcreteModelClass 為 `/Game/Pal/Blueprint/MapObject/ConcreteModel/BP_MapObjectMultiHatchingEggWithBreedModel.BP_MapObjectMultiHatchingEggWithBreedModel_C`，不是裸原生 class。
- `AncientBreeder.cpp` 的建構 post callback 由 `params.Class == model_class` 改為 `params.Class->IsChildOf(model_class)`（先判空）。修正啟動後建立及 LoadMap 清空候選後重新建立的 Blueprint 子類被漏掉的問題；不改第一台。
- 唯讀核對現有 UE4SS `UObjectGlobals.cpp` 的 FindAllOf 實作，已包含父類鏈，因此一次啟動掃描不需修改、不增加反覆全世界掃描。InputGuard 與 Adapter 原本用 IsA，已接受子類；僅有建構通知的精確比對需要修改。
- 建築 ID 仍嚴格限制 `PRF_ResourceBreedingFacility`；不因支援子類而接管原版機器。
- 本機 DLL 版本 `0.1.1-derived-model-local-unverified`，SHA256 `CE550874AF0D09BA995FE035B6A8BB4D43A94C42D0156BC815FB12F1A0A27FA0`，編譯及既有 CTest 5/5 通過。這些核心測試不執行 Unreal 子類通知；尚未安裝／實機驗證。
- 原版實機參數：AutoWorkAmountBySec=1200、BreedRequiredRealTime=10、ChestSlotNum=5、TargetItemIds=Cake/Cake02/Cake03/Cake04/Cake05。ChestSlotNum 屬蛋糕參數元件，不能當作孵化槽數；不能把 10 直接宣稱為實測整輪牆鐘耗時。
- 這次修正是新 native 模組的 discovery bug，不是旧 0.3.5 Blueprint／自訂 ID 轉圈問題的根因證明。該 schema 仍暫停安裝；下一個核對是實際 concrete model 的初始化／子 Blueprint 行為，而不是重建配種邏輯。

## 安裝前攔下已知 Blueprint 風險（2026-09-07）

核對 `tools/install_test_build.ps1` 與 `docs/TEST_BUILD.md` 發現：舊 0.3.5 已有原版孵化器 Blueprint 搭配自訂 ID 時轉圈及世界行為異常的實機紀錄。新第二台 schema 仍採同種搭配，不能因換成新 DLL 就推定此問題已消失。**第二台生產 DLL 與 schema 均未安裝；暫停該 schema 安裝，先取得原版初始化實值。**

已更新既有唯讀探針至 `0.3.1-ancient-blueprint-read-only`，增加 `AncientBreederReadback.hpp`。Ctrl+Alt+F7 時搜尋已載入的原版古代孵化場 actor，匯出 ConcreteModelClass、MapObjectId 及元件類別／已存在的相關參數；沒有建構、親代操作、扣糕、RNG、產蛋或結算。原牧場唯讀查詢保留。

目前 EXE hash 重新讀回符合 v1.0.4 基線。遊戲關閉下已備份並替換既有 `PalResourceFactoryBreederProbe/dlls/main.dll`，讀回 SHA256 `912026B782796D1FF7EB4C0CEA28A9C1F0E9F43BD6E09AA6A4EAA98A4C8EF114`。mods.txt 原本已啟用該探針，未改；第一台安裝 DLL hash 仍為 `4C45741E9C59F393313151330B9D8C65FD4E85EF24B1EB1F22124AE7DF67239A`。

待使用者在測試世界原版古代孵化場旁按 F7 組合鍵。无需親代或蛋糕；讀回 `original-actors>0` 才算取得參考，`0` 不能當成功。這次採集不證明前述故障根因已解決。

## 最新：獨立執行模組已編譯，未安裝（2026-09-07）

- `AncientBreeder.cpp`／`PalResourceFactoryAncientBreeder` 新增單機測試 DLL，僅辨識 `PRF_ResourceBreedingFacility`。建構通知發現 model，Tick 在原版完成孵化後才排入既有 ProcessorState 支解排程；原版配種、蛋糕及孵化不接管。
- `AncientBreederInputGuard.hpp` 已接 ProcessEvent pre callback：第二台後代單筆／全部領取及 server-internal 領取；已附掛第二台蛋槽的移動、交換、丟棄、銷毀、排序／filter 入口。蛋糕箱與親代不在此鎖定集合。原版設備 ID 不匹配即放行。
- `AncientBreederSettlement.hpp` 已串非同步 durable claim → 原版支解／固定材料 → durable APPLYING → 原版扣蛋／完成紀錄消失讀回 → 原版出料 → GROUND_REQUESTED。只在 Tick 接觸 UObject，磁碟工作不阻塞 Tick；錯誤暫停，不重抽／重放。
- `ManualSettlement.hpp` 共用結算器新增明確的第二台選項（預設 false），檢查原版 ancient model 及第二台 ID，解除該模式的 54 格固定限制；第一台既有呼叫仍限原 ID／54 格。第一台 EggProcessor.cpp 與安裝檔未改。乾淨重編第一台與新 adapter 通過。
- 第二台獨立 schema：`src/palschema/PalResourceFactoryAncientBreeder/buildings/breeder.json`，只有第二台 ID；引用原版古代配種器 Blueprint，不打包或啟用舊 combined schema／PAK。JSON 解析與單 ID 範圍檢查通過，引用資產／親代 UI 尚未實機確認。
- DLL：`build-processor/ancient-breeder/main.dll`，SHA256 `CBE4FFCB3D3CF6402F9B56A6E5FCC23823FE23D7F5F5DF599A237462C961AB55`；版本 `0.1.0-local-unverified`。既有 CTest 5/5 通過，不等於新 Unreal 流程實測。
- 已讀回第一台安裝 DLL hash 仍為 `4C45741E9C59F393313151330B9D8C65FD4E85EF24B1EB1F22124AE7DF67239A`。沒有替換遊戲檔案、沒有安裝第二台或啟動遊戲。

### 目前限制及下一步

1. 新 DLL 尚未實機載入。先確認原版 Blueprint 用獨立 schema 後仍保留親代名額、蛋糕效果與孵化參數，再測完整一批。
2. ProcessEvent guard 已實作，不宣稱涵蓋尚未實測的 native 直接呼叫／Blueprint 其他出口；需實機確認活體不可領取、蛋槽不可移出。attach 失敗不得視為封閉生產可用。
3. 同第一台，結算目前只允許本機單人；不啟動 dedicated／listen server 生產。
4. 保留原版孵化存檔；本機支解排程尚未保存經過秒數，重進時原版已完成的蛋會重新等待支解延遲。已進入 RNG／扣蛋交易的 receipt 則沿用防重放規則。不得宣稱斷線恢復或完整重載通過。
5. 原版孵化前段保留供電規則；孵化完成後的支解排程目前未另接供電暫停。此為測試版本限制，不是已確認的原版一致行為。
6. 下一步是獨立測試安裝與實機確認，不發布。第一台刀型延後，嚴苛斷線測試延後。

## 現行方向：沿用古代配種器（2026-09-07，本機呼叫層）

以下為前一階段記錄，執行模組與交易串接狀態以上方最新段落為準。

使用者已修正整合方向：保留原版古代配種器的親代、基地名額、扣糕、配種、後代遺傳、孵化與保存，只替換完成後領取活體的出口，接第一台的原版支解／材料流程。第一台不改；嚴苛斷線測試延後。

下方 `BreederGeneration.hpp`／`BreederOffspringSave.hpp`／`BreederInternalEgg.hpp` 及自訂配種狀態皆是暫停的舊路線，不是現行整合前置條件，不應繼續建立自己的扣糕、生成、內部蛋保存系統。

- 新增 `AncientBreederAdapter.hpp`：限定第二台 ID、原版古代配種器 model 與伺服器；從原版完成紀錄複製後代 SaveParameter，核對蛋槽的 dynamic identity，再呼叫現行原版支解。沒有再次生成／孵化、沒有活體 Actor／Palbox 登記、沒有複製掉落表。
- 獨立 OBJECT target `PalResourceFactoryAncientBreederAdapter`，不產生或安裝 DLL；呼叫端的持久化認領、領取攔截、扣蛋與出料還沒接上。函式的 claimed 契約不代表已實作 exactly-once。
- 已核對容器：`GetItemContainerModule:GetContainer` 是孵化蛋槽；不能用 `BreedItemContainer` 蛋糕箱代替。第一台 ManualSettlement 目前限制其專用 ID／54 格，不能直接拿來執行第二台，也未放寬它。
- 原版蛋槽更新會處理完成陣列移除、工作釋放及同步通知，後續應走原版扣蛋／通知，再讀回確認；不沿用隔離原型的手動清陣列。
- 證據及尚未驗證項目見 `ANCIENT_BREEDER_OUTLET_20260907.md`。目前只有編譯與既有核心測試，沒有遊戲內生產結果。

## 2026-09-07 後代保存與內部孵化呼叫層（未安裝／未實機驗證）

- `BreederOffspringSave.hpp` 使用 Unreal 的 SaveParameter 屬性匯出／匯入文字，並以原版 Identical 檢查往返一致性。保留突變旗標；不把含指標的 880-byte 結構直接寫入檔案。這只是可供批次存檔的 payload，實際磁碟提交尚未接入。
- 靜態核對 v1.0.4 `0x3035620`：先複製蛋 +0x78 的既有後代資料，只要 CharacterID 非空即返回，不走野生蛋隨機生成分支，也不讀動態蛋 ID。證據：`build/v104-breeder-internal-hatch.txt`。
- `BreederInternalEgg.hpp` 據此建立不登記、不進容器的 transient 蛋參數；使用既有原版孵化時間查詢及原版孵化入口。結果再核對與固定後代一致，保留突變旗標。沒有世界蛋、拾取或退款路徑。
- 原版世界蛋建構路徑另存 `build/v104-breeder-world-egg-creation.txt`、`v104-breeder-egg-parameters.txt`，未呼叫；不能誤認世界蛋建構等同箱內動態蛋登記。
- 獨立 OBJECT target 編譯及既有 5 項核心測試通過。這些測試不會執行 Unreal：文字往返相等、transient 蛋構造及孵化入口仍需實機验证。第一台來源與安裝、原版牧場均未修改，沒有新增可執行的生產 DLL。
- 下個缺口是將上述內部後代路徑接到第二台的獨立建築、扣糕交易／批次存檔和支解出料控制；不再以必須建立可領取的箱內蛋作為前提。

## 2026-09-07 v1.0.4 呼叫層實作（尚未接入生產）

以下舊章節含 v1.0.3 位址及舊探針版本，僅供追溯；現行入口依 `BREEDER_NATIVE_FLOW_20260907.md` 的 v1.0.4 區段。

- `BreederGeneration.hpp` 新增原版後代資料生成呼叫：原版初始化 0x2847780、突變判定 0x2dfcdb0、後代生成 0x2de98f0。已由目前 EXE 讀回 16-byte 入口與原版 0.01 比例常數；未在遊戲執行這些呼叫。
- 呼叫前限定 `PRF_ResourceBreedingFacility`、伺服器、有效工作及原版親代資格；不掛任何 hook，不操作原版牧場或第一台。
- 完整傳遞 28-byte 蛋糕效果；普通蛋糕沒有資料列時保留 null。批次核心改收原版提供的產蛋數，不再由蛋糕種類自行推定 1／2。
- 後代結果使用 Unreal 初始化／釋放，禁止複製含配置指標的原始 bytes 作為存檔。此結果尚不是登記在遊戲中的動態蛋，也不能直接送入第一台的蛋結算函式。
- 不呼叫會產生公開世界蛋及增加玩家統計的外層 0x3021510；目前尚未決定第二台是否另行保留該統計副作用。
- 驗證：獨立 OBJECT target `PalResourceFactoryBreederGeneration` 編譯通過；5 項本機核心測試全部通過。沒有新增安裝 DLL、沒有替換遊戲檔案、沒有生產測試。

仍缺：獨立第二台建築與親代操作、扣蛋糕的持久化交易、生成結果的遊戲原生保存／隱藏動態蛋登記，以及孵化支解出料整合。呼叫層註解要求的「先持久化認領」是尚待實作的呼叫端責任，不是目前已具備的 exactly-once 保證。不能稱為已完成單批生產或可用第二台。

## 已完成（僅本機）

- `native/src/BreederState.hpp`：一批普通／蔬菜蛋糕對應 1／2 顆原版生成的蛋；固定本批親代身份；不符合原版資格、換親代、停電暫停計時。
- 上一批尚未完成不得開新批；停止指令須等全部後代完成才允許釋放親代。
- 原版生成之前先進入 GenerationClaimed，序列化後重載不可重新觸發生成。實際扣蛋糕與持久化認領尚未接入遊戲，不可把本機狀態測試當作交易完成。
- 後代處理直接使用第一台 ProcessorState 的重疊支解排程；不修改第一台原始碼／DLL。
- `tests/breeder_state_test.cpp`：親代資格／更換、蛋糕批次大小、並行孵化、停止、重载、重複確認拒絕。連同原四項測試共五項通過。

## 原版入口證據

目前 EXE SHA256：FE3C15064524BAE1947852467C4F92BC22469ACC033A3D3C8031EAB4324E41E8。

- PalBreedingUtility:CanProceedBreeding，反射 thunk RVA 0x2883b20 呼叫 0x2ddb250。後者遍歷目前工作資格，檢查性別值 1 與 2；SDK EPalGenderType 對應 Male／Female。
- PalBreedingUtility:CalcBreedBuffRate，thunk RVA 0x2883a50 呼叫 0x2dd2e30，包含工作及基地加成，不可硬寫固定配種速度。
- PalMapObjectBreedFarmModel 的 CanProceedBreeding thunk RVA 0x29abc70 呼叫 0x2ff6c50。
- 證據：build/breeder-entry-audit-20260907.txt、breeder-native-thunks-20260907.txt、breeder-native-validation-20260907.txt。
- 尚未確認／呼叫真正的後代生成入口與蛋糕副作用邊界；不能自行編造後代資料或掉落。

## 獨立唯讀檢查

- 目標：PalResourceFactoryBreederProbe；產物在 build-processor/breeder-probe/main.dll。
- 2026-09-07 已安裝至 UE4SS/Mods/PalResourceFactoryBreederProbe/dlls/main.dll，mods.txt 獨立啟用；遊戲已退出後安裝。親代與計時唯讀查詢已完成實機驗證，見下節。
- 安裝 DLL SHA256：7870E4AEE6EEC08CA06611E08A561D3A46F551E8BFF2CA829576B93597FEAB97。原 mods.txt 備份：build-processor/breeder-probe-install-backup-20260907/mods.txt。
- 第一台 DLL 與 PAK 保持原基線，不啟用其他停用模組。
- 安裝後僅按 Ctrl+Alt+F7 時查詢原版配種牧場；只支援單機。
- 場景：原版配種牧場已分配一公一母。輸出 assigned、native-pair-eligible、原版 duration/progress/buff。
- 不改親代、不扣蛋糕、不生成蛋、不呼叫 RNG、不修改原版函式，不在每幀掃描。
- 此檢查只是核對介面，尚不是第二台可用建築。

## 接續工作

1. 親代／計時查詢已確認；依 BREEDER_NATIVE_FLOW_20260907.md 繼續核對後代生成 ABI、蛋糕效果與持久化邊界，尚不可直接呼叫未驗證位址。
2. 完成第二台獨立建築、親代操作、蛋糕白名單與隱藏孵化位。
3. 接上伺服器權威的扣蛋糕、生成認領、第一台原版結算與落地出料。
4. 單批普通蛋糕、蔬菜蛋糕兩顆、停止釋放親代與正常重載實測。

舊 `PalResourceFactoryNative` 及舊 facilities.json 不可直接重新安裝；它們包含已隔離的原型與第一台舊 ID 資料。

## 實機唯讀查詢 2026-09-07

- UE4SS.log 10:22:12.4783512 與 10:22:28.5188233：同一 PalMapObjectBreedFarmModel_2147423339，assigned=2、native-pair-eligible=true、duration=300、native-buff=4.5。
- progress 分別為 40.454166、112.454735；約 16.040 秒牆鐘時間增加 72.000569 進度秒，符合約 4.5 倍；不是已實測完整一輪的耗時。
- 兩次 DONE 均為 farms=1、cake-consumed=0、offspring-generated=0、parent-writes=0；僅代表檢查模組未做這些寫入，不代表原版牧場停止生產。
