# 第二台外觀測試版 0.1.6

## 0.1.12 Canvas 目標修正（2026-09-08）

- 01:01 已重新確認無 Palworld 程序，備份並安裝，讀回 SHA256 一致：DB9C326C83ABE5B421FFEEDCF370D7C4C5F5BD0E15FEDA83F7C760A269BA90E1。備份：build-processor/canvas-target-backup-20260908-010140/main.dll。僅替換第二台 DLL，待實機驗收。

- 0.1.11 實機讀回：提示 UI 的 InteractiveObject=None；SetEgg 的 CachedActionInfo 正確，Visibility=SelfHitTestInvisible。Canvas 的「Interactive Object」才指向當前建築 BP_InteractableBox。Post 回呼計數 69。
- 改由提示 GetParent 所在面板的 Canvas 外層取得「Interactive Object」，核對所屬 actor 的精確 BuildObjectId 後隱藏 SetEgg。移除過窄事件名稱條件，排除本函式使用的 GetParent／SetVisibility 防止遞迴。
- 編譯及核心測試 5/5 通過。尚未安裝：程序 30280／21020 仍在執行；待關閉後替換。實機隱藏與 F/C 保留尚待驗收。

## 0.1.11 診斷準備（2026-09-08）

- 使用者實測 0.1.10 V 仍存在，日誌 READY 已載入且未報 INDICATOR_HIDE_FAILED；隱藏仍未完成。
- 增加一次性唯讀診斷：場景出現 SetEgg 提示時，讀回提示 UI／Canvas 的物件與介面參照、CachedActionInfo、Visibility，並記錄提示元件 ProcessEvent 回呼次數。用於區分回呼路徑未命中與實際物件關係不符，不把新診斷版當修復完成。
- 既有生產、V 操作封鎖不變。待關閉遊戲才能替換診斷版，靠近第二台後自動記錄，無額外按鍵。

## 後續 0.1.10 提示元件修正（2026-09-08）

- 使用者確認 0.1.9 V 提示仍在；當次日誌確認新版 READY，無 INDICATOR_HIDE_FAILED。上一版隱藏路徑實測未生效，不能宣稱完成。
- 唯讀提取目前遊戲 WBP_PalInteractiveObjectIndicatorCanvas/UI，確認 UI 的 CachedActionInfo、介面參照、UpdateVisible 與 SetActionInfo 等名稱。改為在提示 UI 更新後，以 SetEgg 及其互動框所屬建築的精確 ID，將該元件設為 Collapsed；取代舊 GetIndicatorInfo 回傳修改。
- 既有 V 操作封鎖不變，無生產／模型／F/C 改動。編譯及核心 5/5 通過，確認遊戲關閉後已備份安裝並讀回一致。尚待遊戲內確認提示消失及一般孵化器不受影響。

## 後續 0.1.9 隱藏 V 提示（2026-09-08）

- 使用者實測確認 0.1.8 按 V 不會進入蛋面板，但提示仍存在。
- 新版在第二台互動框 GetIndicatorInfo 回傳後，僅將 SetEgg 提示的 bValid 設為 false；不依 V 所在欄位硬編碼，也不處理其他建築。保留原有輸入封鎖、F/C 操作與裝置外進度。
- 編譯成功，既有核心測試 5/5 通過；這不代表遊戲內提示已驗證消失。
- 尚未安裝：檢查時 Palworld（30648）及 Palworld-Win64-Shipping（31708）仍在執行。待正常關閉後替換第二台 DLL；第一台及 PAK 不變。
- 2026-09-08 00:11 使用者關閉遊戲後，重新檢查無 Palworld 程序，已安裝並讀回確認 SHA256：C0AC3F33BDEE88A1B17E02496F79CF212A62E4EA31C5682215B8E3CFE4A43045。舊 DLL 備份於 build-processor/hide-v-backup-20260908-001138/main.dll。目前待遊戲內確認 V 提示消失及 F/C 保留。

## 後續 0.1.8 蛋面板入口停用

- 使用者已確認 0.1.7 自製模型與兩側按鈕可接受。裝置外蛋進度會依距離切換單顆摘要／完整面板，保留此行為。
- 使用者撤回 V 改名要求，改為禁止進入蛋面板，進度只看裝置外 UI。
- InputGuard 在原有 ProcessEvent 攔截中，針對自訂第二台的 SetEgg 類型攔截 OnStartTriggerInteract、OnTriggerInteract、OnTriggeringInteract、OnEndTriggerInteract。用遊戲反射列舉名稱辨識，不硬編碼列舉序號。
- 不封鎖其他互動類型，不修改模型、蛋糕容器、親代指派、裝置外 HUD、結算與原有禁止取回蛋／活體限制。V 提示文字本身未移除。
- 編譯及既有核心測試 5/5 通過，遊戲關閉時安裝並讀回確認 DLL 相符。實際 V 是否無法開啟，以及 F 指派／C 蛋糕入口仍正常，待遊戲內驗證；不能以核心測試代替此驗收。

## 後續 0.1.7 修正

- 實機 0.1.6 日誌 23:20:43 SKIN_FAILED Required reflection object is not loaded；畫面維持原版。查本地 Engine 標頭證實 Conv_StringToSoftObjectReference 不存在。改為 MakeSoftObjectPath(PathString) → Conv_SoftObjPathToSoftObjRef(SoftObjectPath) → LoadAsset_Blocking(Asset)。
- 使用者确认領取已被停用，並要求左側「設置所有帕魯蛋」同樣停用。左右按鈕各自保存及還原狀態。
- 0.1.6 去掉圖片資源會失去原透明遮罩，出現白色空塊。0.1.7 保留圖片資源，改筆刷 tint 並停止按鈕動畫，避免遮蓋文字。
- 編譯及核心 5/5 通過。確認遊戲程序已關閉後安裝，DLL SHA256 93CFA84EB52ECD8D66BE5C9EB54F7DD34C072A6EFD80D12C54004CA3B97516BD。上一 DLL 保留於 build-processor/skin-buttons-017-backup/main.dll；未更換 PAK、生產資料或第一台檔案。
- 0.1.7 尚待實機模型與兩側按鈕外觀驗證。

狀態：已安裝，可供遊戲內測試；尚未實機驗收。

## 本次修改

- MenuVisual：以菜單容器及 UpdateSlots 的容器 ID 辨識自訂第二台，補足上版只讀 MapObjectConcreteModel 未套用的路徑。停用領取按鈕，圖片移除原藍色材質並用中性灰色；選到一般孵化器時還原原圖片與啟用狀態。伺服器領取攔截不變。
- Skin：僅對已辨識的第二台模型取得原 Actor，替換 SM_EggHatchingMachineFuture 的網格，保留原配種／孵化／互動模組。載入既有雙親代艙 FBX 匯入產物，清除原覆蓋材質，套用底部原點與朝向；隱藏原裝飾光球、蛋顯示與完成光效。
- 不更換第一台 DLL／PAK、不修改 schema、存檔或结算紀錄。建造預覽仍取原 Blueprint；本輪替換已放置建築的機身，不宣稱建造預覽已自製化。

## 本地驗證

- Unreal 匯入成功：SM_PRF_Breeder，7 個碰撞體，原始匯入尺寸 365 × 247.72 × 316.5 公分。匯入有退化切線／近零副法線警告，尚待遊戲內確認表面光影。
- Cook 成功，20 個自製網格與材質檔；PAK mount 僅 PalResourceFactory/BreederVisual，不打包原版 Blueprint。
- Native 編譯成功，既有核心 CTest 5/5 通過。這些測試不驗證 UMG 動態外觀或實機網格顯示。
- 安裝前確認無 Palworld 程序，安裝後 SHA256 相符。
- DLL：71A8BFC239A084C58F3850ECFA0C8C3129B589EC0D83CD5C1B24BEB0EDCEF3FA。
- PAK：91AEE2F6A0F9E165E477EE71F7D7B963B950873284E77AC8445F04B342735326。
- 舊 DLL 備份：build-processor/breeder-visual-20260907-225110/previous-installed.dll。新增 PAK 位於 PalSchema/mods/PalResourceFactoryAncientBreeder/paks/PalResourceFactoryBreederVisual_P.pak。

## 使用者測試

1. 正常進入存檔，查看既有第二台是否顯示雙親代艙模型，檢查方向、地面高度、互動與可見光影。
2. 打開第二台蛋介面，尤其有孵化完成的蛋時，領取按鈕應灰色且無法取得活體。一般原版孵化器不應被套用灰色樣式。
3. 確認原本配種、孵化與出料仍持續。
4. 日誌檢查 CUSTOM_SKIN_APPLIED / SKIN_FAILED、OBTAIN_BUTTON_DISABLED / MENU_VISUAL_FAILED。視覺成功必須同時核對實際畫面，不以日誌單獨驗收。
