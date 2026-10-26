# 技術盤點

## 已確認

- 本機 Palworld 使用新的根目錄 `Mods` 佈局，已有 UE4SS Experimental 與 PalSchema 0.6.3。
- PalSchema 可新增建築、科技與翻譯資料，並支援混合 PAK／Blueprint 資產。
- 原版標準建築資料只有四組建造材料欄位，本專案配方已壓成四種。
- `D:\PalSkillDPSAnalyzer` 是另一個有大量未提交變更的 MOD；本專案建立於 `D:\PalResourceFactoryMod`，不共用發布目錄。
- 本機可唯讀使用 Unreal Engine 5.1、Palworld Modding Kit 原始碼標頭與 UE4SS C++ 工具鏈；它們不是本專案的原始碼，也不會寫入本專案發行包。
- 本專案自己的 UE4SS `main.dll` 已能編譯並輸出必要的 `start_mod`／`uninstall_mod`；目前會先安裝禁止自訂建築走原版領取活體的安全攔截，再於完整反射契約成立時接上孵化完成、原版掉落查詢、序列間隔與基地倉儲提交。
- `unreal/PalResourceFactory.uproject` 與內容外掛已建立；共用 Pal Source／Plugins 以目錄連結唯讀引用，專案快取、Blueprint 與封裝輸出留在本專案。

## 原版參考建築（Palworld v1.0.3）

| 用途 | Map Object ID | Blueprint class | 已知關鍵資料 |
| --- | --- | --- | --- |
| 電能孵化器 | `ElectricHatchingPalEgg` | `BP_BuildObject_ElectricHatchingPalEgg` | 原版耗電 400/s |
| 大型電能孵化器 | `MultiElectricHatchingPalEgg` | `BP_BuildObject_MultiElectricHatchingPalEgg` | `AutoWorkAmountBySec`；原版耗電 800/s |
| 古代孵化場 | `MultiElectricHatchingPalEggWithBreed` | `BP_BuildObject_MultiElectricHatchingPalEggWithBreed` | `TargetItemIds`、`BreedRequiredRealTime`、`AutoWorkAmountBySec`、`ChestSlotNum` |
| 帕魯解體傳送帶 | `DismantlingConveyor` | `BP_BuildObject_DismantlingConveyor` | `ContainerSlotNum`、`ConvertQueueCapacity`、`RequiredConvertProcessTime` |

原版資產基底路徑：

`/Game/Pal/Blueprint/MapObject/BuildObject/`

## 建材內部 ID

| 顯示名稱 | 內部 ID |
| --- | --- |
| 木板 | `Processed_Wood` |
| 帕魯礦碎片 | `Pal_crystal_S` |
| 金屬錠 | `CopperIngot` |
| 釘子 | `MachineParts` |
| 精煉金屬錠 | `IronIngot` |
| 電路板 | `MachineParts2` |
| 極低溫冷卻介質 | `Bio_Coolant` |

## 已找到的遊戲端接口

- `UPalMapObjectConcreteModelBase::TryGetMapObjectId()` 可區分共用原版 Blueprint 的自訂建築列。
- `UPalMapObjectHatchingEggModelBase` 已提供蛋陣列、工作進度、能源更新及孵化完成事件。
- `UPalMapObjectMultiHatchingEggWithBreedModel` 已提供配種進度、兩名親代指派、蛋糕容器及停止原因。
- `UPalMapObjectConvertCharacterToItemParameterComponent::RequiredConvertProcessTime` 可作為原版支解間隔的目前版本來源，不需要建立傳送帶角色欄位與動畫。
- `UPalDatabaseCharacterParameter::GetDropItemData()` 可讀取當前版本帕魯掉落列；本 MOD 不複製掉落表，只依原版 `Rate`、`min`、`Max` 結算且不套用額外倍率。

## 不能只靠資料表完成的部分

- 54 格蛋輸入倉與直接提交基地共享倉儲的交易。
- 投入即鎖定、拆除銷毀與不可逆工作佇列。
- 兩顆蛋並行、蛋糕白名單與蔬菜蛋糕雙蛋語意。
- 原版孵化結果到目前版本掉落列、原版支解間隔與基地倉儲的封閉串接。
- 每筆獎勵只提交一次的存檔／多人一致性。

以上需要 Blueprint 資產與 UE4SS/native 執行邏輯，或等價的遊戲端擴充。

## 目前缺口

- 兩座獨立子 Blueprint 已完成烹煮與最小 PAK 封裝，但仍借用原版設備外觀與父類流程；尚未製作正式封閉機器模型與專用 UI。
- 54／2 格回傳、領取阻擋、掉落查詢、序列支解與基地倉儲寫入已實作，仍待目前遊戲版本實機確認反射欄位與複寫通知完全相符。
- 現行預覽版以孵化結果記錄清空作為持久化已消耗標記；專用伺服器重啟、斷線與多人精確一次提交仍未驗證，主要存檔不得使用。
- 蛋糕白名單、蔬菜蛋糕雙蛋、孵化時動態雙倍耗電及拆除時銷毀所有投入內容尚未接完遊戲端。
- 科技顯示調整為 1 級普通科技空位，分別消耗 3／4 點；26／36 級保留為建材與電力平衡的設計進度說明。

## 執行順序

1. 先完成不依賴遊戲資產的狀態機與破壞／重載測試。
2. 建立 PalSchema 建築與科技資料，暫用原版工業模型。
3. 接上 UE4SS/native 伺服器權威流程。
4. 在單人世界驗證，再驗證主機／加入端與專用伺服器。
5. 功能穩定後才製作或拼裝正式封閉機器模型。
