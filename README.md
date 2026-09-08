# Pal Resource Factory

獨立的 Palworld MOD：蛋資源處理機 + 資源繁殖場。自動孵化、依遊戲原版支解結果產生材料，掉落在機器前方供拾取或搬運。

[Steam 工作坊（兩台一包）](https://steamcommunity.com/sharedfiles/filedetails/?id=3797706995) · [下載 v0.3.15 預覽版](https://github.com/paul800901/PalResourceFactoryMod/releases/tag/v0.3.15)

**公開預覽版 v0.3.16。Windows Steam Palworld 1.0.4.102642。原有單人流程已實測；本次第二台蛋槽生命週期修正僅完成本機回歸，遊戲內驗證待補。**

本次修正：非同步結算期間不再使用消耗前快照重新排入蛋；閒置佇列依目前蛋槽清除舊身分。結算中的來源若改變，沿用單筆隔離而非整機停機，不重抽掉落、不刪除結算紀錄、不增加返還。結算等待期間佇列計時暫停，磁碟緩慢時吞吐量可能降低。
多人、專用伺服器及其他遊戲版本未驗證，不宣稱支援。原生程式使用此版本的遊戲位址，遊戲更新後請先停用並等待相容性確認。使用前備份存檔。

## 兩台機器

| 建築 | 用途 |
| --- | --- |
| 蛋資源處理機 | 54 格蛋倉；投入後自動孵化（速度加成 50%）與支解。一般耗電 200／秒，孵化時 400／秒。 |
| 資源繁殖場 | 指派一雄一雌親代並提供蛋糕；自動配種、孵化（速度加成 100%）與支解後代。一般耗電 500／秒，孵化時 1000／秒。 |

- 停電暫停，復電續行；兩台皆在基地內建造。
- 投入的蛋、處理中後代、已消耗蛋糕不可取回；拆除不返還處理內容。
- 尚未消耗的庫存蛋糕可以取回。原版建材返還與內容物返還是不同規則。
- 不複製掉落表，使用遊戲當下的孵化及支解結果；不提供後代選育或救回功能。
- 本預覽版保留測試過的科技設定：兩座科技目前都在等級 1，分別需要 3／4 科技點，並非最終解鎖平衡。建造工作量依原版 26／36 級建築資料調整，實測兩台皆為 500。

## 安裝

1. 訂閱並啟用 [UE4SS Experimental for Palworld](https://steamcommunity.com/sharedfiles/filedetails/?id=3625223587) 和 [PalSchema](https://steamcommunity.com/sharedfiles/filedetails/?id=3625280368)。本次使用 PalSchema 0.6.7。
2. 訂閱本 MOD 的工作坊項目，在遊戲「模組管理」啟用，完整重啟遊戲。兩台機器在同一個項目內。
3. 到科技頁解鎖，按 B 在建造選單尋找兩台機器。

工作坊連結與 ZIP 位於本儲存庫的 Releases。手動安裝 ZIP：遊戲關閉時，將包內的 `Mods` 資料夾合併到 `<Palworld>/Mods/NativeMods/UE4SS`。不要用整個資料夾替換既有 Mods，不要同時安裝手動版與工作坊版。

開發測試版使用者請保留存檔及原有狀態檔，不要為轉換安裝方式刪除它們。停用或移除前先備份，並在 MOD 仍啟用時拆除自訂建築；拆除會銷毀不可返還的處理內容。

## 語言與介面

介面自動跟隨遊戲語言，不需 F2、額外語言按鈕或 Windows 語言設定。改遊戲語言後完整重啟。

包含遊戲內建的 17 種語言：English、繁體中文、简体中文、日本語、Français、Italiano、Deutsch、Español、Português (Brasil)、Русский、한국어、Bahasa Indonesia、Español (Latinoamérica)、ไทย、Türkçe、Tiếng Việt、Polski。

範圍包括科技頁、B 建造選單、建築／庫存標題與「投入所有蛋」按鈕。原版按鈕沿用遊戲翻譯；機身 A／B、CAKE 等模型印字不變。**17 種語言的字型、排版與切換尚未逐一實機驗證，也未經各語言母語校閱。**

## 驗證狀態

作者單人實測通過：建造預覽／建造中／完成模型、兩台主要流程、正常重登、強制退出後重進、斷電復電、連續投入、取消／拆除、單次結算、未用蛋糕返還與已用蛋糕不返還。低幀率回歸問題在該測試環境已改善。

這些是有限實測，並非所有崩潰時點、硬體與存檔條件的保證。核心 5 項測試通過不等同多人或新版本遊戲驗證。工作坊包的全新安裝流程仍需獨立遊戲內驗證。

## 原始碼與建置

- `native/src/EggProcessor.cpp`：目前第一台原生模組（0.3.15-localization）。
- `native/src/AncientBreeder.cpp`：目前第二台原生模組（0.1.16-construction-actor）。
- `src/palschema/PalResourceFactoryProcessor`、`PalResourceFactoryAncientBreeder`：目前建築和翻譯資料。
- `src/localization/strings.json`：共用文字來源；`tools/generate_localizations.ps1 -Check` 檢查生成結果。
- `art/blender`、`unreal/Content/PalResourceFactory`：本專案模型、貼圖與 UE 資產。
- `src/core`、`tests`：核心狀態與回歸測試。其餘 native prototype/probe 目標保留供研究，不是發布模組，不要安裝。

核心測試需要 CMake 3.25+ 和 C++23 編譯器：

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

原生模組另需 MSVC x64、相符版本的 UE4SS 原始碼／生成標頭、UE4SS 匯入庫和其外部依賴。設定 `UE4SS_SOURCE_DIR`、`UE4SS_IMPORT_LIBRARY`、`EXTERNAL_ROOT` 後，明確建置兩個正式目標：

```powershell
cmake -S native -B build-processor -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUE4SS_SOURCE_DIR=<ue4ss-source> -DUE4SS_IMPORT_LIBRARY=<UE4SS.lib> -DEXTERNAL_ROOT=<dependencies>
cmake --build build-processor --target PalResourceFactoryProcessor PalResourceFactoryAncientBreeder
```

Unreal 資產需合法取得的 Palworld Modding Kit／UE 5.1 與 Blender。SDK、遊戲資產、工具鏈不隨儲存庫提供；既有開發腳本的本機路徑須依環境設定。`tools/package_workshop.ps1` 只封裝目前兩台的指定檔案，不包含原型、遊戲存檔或其他 MOD。

## License

本專案原創程式碼與機器美術採 [MIT](LICENSE)。遊戲與第三方權利不受此授權影響，詳見 [Third-party notices](THIRD_PARTY_NOTICES.md)。本專案並非 Pocketpair 官方產品。

## English

Two independent resource machines in one Workshop package: deposit eggs into the 54-slot Egg Resource Processor, or assign male/female parents and supply cake to the Resource Breeding Facility. Both automatically incubate and butcher using current native game results, then drop materials in front. Power loss pauses processing; power restoration resumes it.

Inputs are irreversible. Deposited eggs, offspring and consumed cake are not refunded. Only unused cake remains recoverable. This initial public preview retains level-1 technology unlocks for both machines; this is not final progression balance.

Requires UE4SS Experimental for Palworld and PalSchema. Tested only in single-player on Windows Steam Palworld **1.0.4.102642**. Multiplayer/dedicated servers and other builds are unverified. Back up saves. Version-specific native addresses require compatibility checks after game updates.

17 interface languages follow the game's language; restart after changing language. Technology/building menus and custom deposit text are localized. Model lettering is unchanged. All language layouts, new Workshop installation, and multiplayer have **not** been verified in game. Published as a preview, not a universal compatibility guarantee.
