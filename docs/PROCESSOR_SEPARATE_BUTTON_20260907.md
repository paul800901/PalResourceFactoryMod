# 0.3.6 獨立投入按鈕測試候選

## 最新決策：0.3.8 回復改字方案

使用者明確取消新增按鈕路線。0.3.7 遊戲內未顯示新按鈕且開啟嚴重卡頓，紀錄為 `Required reflection object is not loaded`；失敗後未記錄完成狀態，會被介面事件持續重試。未量測耗時，不能將全部卡頓確定歸因於此。

0.3.8 已移除新按鈕建立、委派、灰化及 ProcessEventPost 介面重試。回復原按鈕與 X 投入背包蛋；原生箱子功能不改。只在 0.1 秒 UI 檢查更新原文字，快開重開時讀回已快取 TextBlock，遇到原文才重設；不每次搜尋整棵樹。單一容器的改字失敗會停止重試，容器改變後才重置。

編譯及 4 項核心測試通過，未安裝、未遊戲驗證。遊戲仍執行（PID 22380 / 22144）。孵化、支解與出料程式未改。
候選 `build-processor/processor/main.dll` SHA256：`E8DB3416922A0A2A6D06B8C970D718BEECCBCD1D7C12ACD7D4D84B3C1C237E2F`。

2026-09-07 01:41:02 安裝更新：使用者確認完全關閉，工具檢查安裝前後均無 Palworld 程序。0.3.7 已備份至 `build-processor/fast-label-backup-20260907-014102/main.dll`；只替換處理器 DLL 為 0.3.8，讀回 SHA256 與上述候選一致。未啟動遊戲，實際改字速度、投入與卡頓情況待遊戲測試。

以下保留歷史測試紀錄，不代表目前決策。

狀態：本機編譯與 4 項既有核心測試通過；2026-09-07 使用者確認完全關閉後，檢查無 Palworld 程序，已備份並安裝；尚未遊戲內驗證。

本輪只改 `native/src/EggProcessor.cpp`、`native/src/ProcessorDeposit.hpp` 的處理器箱子 UI。

- 移除覆寫原版 TextBlock 文字的流程。
- 在原按鈕左側新增獨立 CommonUI 控制項，文字於加入畫面前設定。
- 原「全部取走」面板停用、降透明度；原按鈕事件與 X 的移出請求均阻擋。
- 新按鈕沿用已實測的投入蛋伺服器 RPC；不修改孵化、支解、掉落與磁碟流程。
- 依 TargetContainer 切換顯示；一般箱子隱藏新按鈕並恢復原面板狀態。
- 胸箱事件後更新，1 秒檢查作為既有補查；同容器不重建控制項。
- 新 CommonUI 控制項以原按鈕的 WidgetClass 建立，OnButtonBaseClicked 綁定現有胸箱入口，保持一個 Button 參數的簽名；不以零參數 UMG Button 委派錯接。

產物：`build-processor/processor/main.dll`

SHA256：`0A01A1FD98437C1627A0978E00ABC4F2A490F0B40E174A48B747DEFB95850448`

安裝前 0.3.5 DLL 核對為 `A1AE9A3FA17A82171BC4E8C48F79E02907B176D2BA7E6F9D863ADCECCE7B0BD3`，已保存於 `build-processor/separate-button-backup-*` 本輪時間戳目錄。
已安裝 DLL 讀回符合上述 0.3.6 產物 SHA256。只替換處理器 DLL，未啟動遊戲。

遊戲內待驗證：

1. 原按鈕左側只有一個「投入所有蛋」，原字不改且灰化。
2. 新按鈕可投入背包蛋；原按鈕與 X 不取回、不投入。
3. 快速關閉重開十次，無新增重複、無漏顯、無明顯卡頓。
4. 開一般箱子，「全部取走」正常，新按鈕不出現。
5. 原生版型假設（CanvasPanelSlot 固定尺寸、原 CommonUI 成員）及委派連線仍需此輪遊戲驗證；失敗記錄 DEPOSIT_UI_UNAVAILABLE，不能以編譯通過宣稱 UI 可用。

本輪不重新打包 PAK，不修改其他 MOD，不發布。

## 0.3.6 實測失敗與 0.3.7 候選

使用者截圖確認無新按鈕、原按鈕未灰化。遊戲紀錄 2026-09-07 01:16:49：
`DEPOSIT_UI_UNAVAILABLE Quick-move parent is not a canvas`。
0.3.6 已載入，失敗來自布局條件，不是安裝未生效。

0.3.7 改為加入 Canvas_QuickMoveButton 本身，以負向水平錨點定位到左側，不要求其父節點為 CanvasPanel 或讀取首幀尺寸。原 CommonUI 控制項單獨停用，原文字單獨降透明度，不停用包含新增按鈕的整個畫布。

本機編譯與 4 項既有核心測試通過；尚未安裝、未遊戲驗證。遊戲仍執行，PID 17788 / 22744。
候選 DLL SHA256：`BC8B17B06BB3EED0D1C4CA9356C5F22A736015051306E1AD28B3E81B3D1DF1AB`。
仍需實測畫布類別檢查、左側溢出區域是否被裁切或攔截點擊、委派與快速重開。

2026-09-07 01:24:44：使用者確認完全關閉，安裝前後均確認無 Palworld 程序。已備份 0.3.6 至 `build-processor/button-canvas-backup-20260907-012444/main.dll`，只替換處理器 DLL 為 0.3.7，安裝讀回 SHA256 與上述候選一致。未啟動遊戲，遊戲內驗證待使用者測試。
