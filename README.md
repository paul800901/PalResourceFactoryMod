# Pal Resource Factory

A standalone Palworld mod with two machines: the Egg Resource Processor and Resource Breeding Facility. They automate incubation and butchering, using the game's native results and dropping materials in front of each machine.

[Steam Workshop package](https://steamcommunity.com/sharedfiles/filedetails/?id=3797706995) · [v0.3.20 public preview](https://github.com/paul800901/PalResourceFactoryMod/releases/tag/v0.3.20)

**Public preview v0.3.20. Target: Windows Steam Palworld 1.0.5.102999; restores the current 1.0.5 native processor in the public package. Local startup/attachment is confirmed; the reporter's complete egg-to-material test is pending.**

The custom machine materials are now cooked for both the existing SM5 path and the SM6 path used by forced DirectX 12. This fixes the beige/checkerboard fallback materials seen with `-dx12`; both machines retained normal operation in the local single-player check. This is a scoped DX12 visual confirmation, not a complete regression. Existing blocked eggs may still need individual diagnosis.

## English

### The two machines

| Building | Use |
| --- | --- |
| Egg Resource Processor | 54 egg slots; automatic incubation (+50%) and butchering. Power: 200/s normally, 400/s while incubating. |
| Resource Breeding Facility | Assign one male and one female, supply cake; automatically breeds, incubates (+100%) and butchers the offspring. Power: 500/s normally, 1000/s while incubating. |

Both pause without electricity and resume when power returns. Materials drop in front for pickup or transport by Pals. The mod uses current native hatch and butchering results; it does not copy drop tables or increase quantity or rarity.

Both technologies unlock at level 1 to avoid overlapping other technology entries without rebuilding the technology interface. The processor costs 3 regular technology points; the breeding facility costs 4. Construction work is 500 for each machine.

- Processor materials: Wooden Planks ×10, Paldium Fragments ×30, Ingots ×20, Nails ×20.
- Breeding facility materials: Refined Ingots ×30, Circuit Boards ×10, Cryogenic Coolant ×10, Nails ×30.

### Installation and irreversible inputs

Subscribe to [UE4SS Experimental for Palworld](https://steamcommunity.com/sharedfiles/filedetails/?id=3625223587), [PalSchema](https://steamcommunity.com/sharedfiles/filedetails/?id=3625280368), and this mod. Enable all three in Mod Management, then fully restart the game. Unlock a machine on the technology page and press B to build it in your base.

For a manual ZIP install, close the game and merge the package's Mods folder into Palworld/Mods/NativeMods/UE4SS. Do not replace the existing Mods folder or install both the Workshop and manual copies. Keep your save and existing processing state files when changing installation methods. Before removing the mod, back up your save and dismantle its buildings while the mod is enabled.

Deposited eggs, offspring being processed, and consumed cake cannot be returned. Only unused stock cake is recoverable. Dismantling destroys the remaining processing contents.

### Languages and verification

The interface follows the game setting and includes 17 languages. Technology, build-menu, building and inventory text are localized; model lettering is unchanged. Fully restart after changing the game language. Individual language layouts and translations have not all been verified.

Languages: English, Traditional Chinese, Simplified Chinese, Japanese, French, Italian, German, Spanish, Brazilian Portuguese, Russian, Korean, Indonesian, Latin American Spanish, Thai, Turkish, Vietnamese and Polish.

The author confirmed recovery in a local single-player game on Palworld 1.0.5.102999. This does not establish full regression across all machine states, item-by-item or duplicate-commit behavior, long-running operation, FPS impact, multiplayer, dedicated servers, or a fresh Workshop installation. The package does not contain saves, processing records or recovery scripts. Back up your save.

### Development and license

The native sources are native/src/EggProcessor.cpp and native/src/AncientBreeder.cpp. Building and localization data are under src/palschema and src/localization; editable models and Unreal assets are under art/blender and unreal/Content/PalResourceFactory. Other native prototype and probe targets are research artifacts, not release modules.

A native build requires MSVC x64, matching UE4SS source/generated headers, its import library and external dependencies. Configure native with UE4SS_SOURCE_DIR, UE4SS_IMPORT_LIBRARY and EXTERNAL_ROOT, then build PalResourceFactoryProcessor and PalResourceFactoryAncientBreeder. Core tests require CMake 3.25+ and a C++23 compiler. From the repository root, run the core tests with:

~~~powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Configure and build the two native targets with:

~~~powershell
cmake -S native -B build-processor -G Ninja -DCMAKE_BUILD_TYPE=Release -DUE4SS_SOURCE_DIR=<ue4ss-source> -DUE4SS_IMPORT_LIBRARY=<UE4SS.lib> -DEXTERNAL_ROOT=<dependencies>
cmake --build build-processor --target PalResourceFactoryProcessor PalResourceFactoryAncientBreeder
~~~

Original code and machine art are MIT licensed. Game and third-party rights are separate; see THIRD_PARTY_NOTICES.md. This unofficial mod is not affiliated with Pocketpair.

### Issue reports

General users can report issues through [Discord: Palworld Mod Issue Reports](https://discord.gg/Swzj4UjejE) or this project's [GitHub Issues](https://github.com/paul800901/PalResourceFactoryMod/issues). Include the mod name, Palworld version, mod version, environment (single-player, multiplayer or dedicated server), reproduction steps, and relevant UE4SS.log excerpts. Do not share passwords, account information or full private paths.

## 繁體中文

### 版本與本次確認

公開預覽版 v0.3.20，目標為 Windows Steam 版 Palworld 1.0.5.102999；公開封包已恢復目前的 1.0.5 原生處理器。本機已確認啟動與掛載，回報者的完整蛋到材料測試尚待確認。

自訂機器材質現在同時包含既有 SM5 與強制 DirectX 12 使用的 SM6。這會修正 `-dx12` 下出現的米黃色／棋盤格備援材質；本機單人檢查中兩台機器仍正常運作。這是限定範圍的 DX12 畫面確認，不代表完整回歸。既有卡住的蛋仍可能需要個別診斷。

### 兩台機器

| 建築 | 用途 |
| --- | --- |
| 蛋資源處理機 | 54 格蛋倉；自動孵化（速度加成 50%）並支解。一般耗電 200／秒，孵化時 400／秒。 |
| 資源繁殖場 | 指派一雄一雌親代並供應蛋糕；自動配種、孵化（速度加成 100%）並支解後代。一般耗電 500／秒，孵化時 1000／秒。 |

兩台都會在停電時暫停、復電後續行；材料掉落在機器前方，可自行拾取或由帕魯搬運。模組使用遊戲當下的原版孵化與支解結果，不複製掉落表，也不提高數量或稀有率。

兩座科技刻意配置於等級 1，以避免與其他科技項目重疊，不必重做科技介面。蛋資源處理機需要普通科技點 3 點，資源繁殖場需要 4 點。兩台建築工作量皆為 500。

- 蛋資源處理機建材：木板 ×10、帕魯礦碎片 ×30、金屬錠 ×20、釘子 ×20。
- 資源繁殖場建材：精煉金屬錠 ×30、電路板 ×10、極低溫冷卻介質 ×10、釘子 ×30。

### 安裝與不可返還的投入

訂閱 [UE4SS Experimental for Palworld](https://steamcommunity.com/sharedfiles/filedetails/?id=3625223587)、[PalSchema](https://steamcommunity.com/sharedfiles/filedetails/?id=3625280368) 與本模組。在遊戲模組管理中啟用三個項目，再完整重啟遊戲。到科技頁解鎖機器，按 B 在據點內建造。

手動 ZIP 安裝時，先關閉遊戲，再將套件內 Mods 資料夾合併到 Palworld/Mods/NativeMods/UE4SS。不要取代既有 Mods 資料夾，也不要同時安裝工作坊版與手動版。轉換安裝方式時，請保留存檔與既有處理狀態檔。移除模組前，先備份存檔，並在模組仍啟用時拆除自訂建築。

已投入的蛋、處理中的後代與已消耗的蛋糕都不能取回；只有尚未消耗的庫存蛋糕可以取回。拆除會銷毀剩餘處理內容。

### 語言與驗證

介面自動跟隨遊戲設定，包含 17 種語言。科技、建造選單、建築與庫存文字已本地化；機身印字不變。切換遊戲語言後請完整重啟。各語言排版與翻譯尚未全部逐一驗證。

語言：English、繁體中文、简体中文、日本語、Français、Italiano、Deutsch、Español、Português (Brasil)、Русский、한국어、Bahasa Indonesia、Español (Latinoamérica)、ไทย、Türkçe、Tiếng Việt、Polski。

作者已在 Palworld 1.0.5.102999 本機單人遊戲確認功能恢復。這不代表所有機器情境、逐筆或防重複提交、長時間運轉、FPS 影響、多人、專用伺服器或全新工作坊安裝均已驗證。套件不包含存檔、處理紀錄或復原腳本。請先備份存檔。

### 開發與授權

原生程式位於 native/src/EggProcessor.cpp 與 native/src/AncientBreeder.cpp；建築與翻譯資料位於 src/palschema、src/localization；可編輯模型與 Unreal 資產位於 art/blender、unreal/Content/PalResourceFactory。其他 native prototype/probe 目標僅供研究，不是發布模組。

原生建置需要 MSVC x64、相符的 UE4SS 原始碼與生成標頭、匯入庫及外部相依。設定 UE4SS_SOURCE_DIR、UE4SS_IMPORT_LIBRARY、EXTERNAL_ROOT 後，建置 PalResourceFactoryProcessor 與 PalResourceFactoryAncientBreeder。核心測試需要 CMake 3.25 以上與 C++23 編譯器。在儲存庫根目錄執行核心測試：

~~~powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

設定並建置兩個原生 target：

~~~powershell
cmake -S native -B build-processor -G Ninja -DCMAKE_BUILD_TYPE=Release -DUE4SS_SOURCE_DIR=<ue4ss-source> -DUE4SS_IMPORT_LIBRARY=<UE4SS.lib> -DEXTERNAL_ROOT=<dependencies>
cmake --build build-processor --target PalResourceFactoryProcessor PalResourceFactoryAncientBreeder
~~~

原創程式碼與機器美術採 MIT 授權。遊戲及第三方權利另計，詳見 THIRD_PARTY_NOTICES.md。本模組為非官方產品，與 Pocketpair 無隸屬關係。

## 問題回報

一般使用者可透過 [Discord「帕魯模組問題回報」](https://discord.gg/Swzj4UjejE) 或本專案的 [GitHub Issues](https://github.com/paul800901/PalResourceFactoryMod/issues) 回報。請提供模組名稱、Palworld 版本、模組版本、環境（單人／多人／專用伺服器）、重現步驟及相關 UE4SS.log 片段。請勿公開密碼、帳號資料或完整私人路徑。
