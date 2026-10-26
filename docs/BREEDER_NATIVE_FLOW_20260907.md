# 原版配種完成流程靜態核對

## v1.0.4 接續定位（2026-09-07）

### 完整蛋糕效果查詢 0.3.0

- 新增 native/src/BreederEffects.hpp，僅第二台探針使用。經 PalUtility:GetGameSetting 與反射欄位 BreedingItemEffectDataAsset 取得遊戲實際效果資產。
- 原版牧場 0x301b534／0x301b5b7 從設定 +0x1df8 取得該資產；0x2de9110 完整 155-byte 函式是以道具 FName 查 map，回傳效果值或 null，沒有扣除或亂數路徑。探針保留 PE／前綴／map offset／結構大小檢查，複製原版完整 28-byte 效果並輸出各欄位。
- 另確認 0x2dfcdb0 不是可固定的旗標查詢：它將蛋糕 MutationRateBonusPercent（原版呼叫端先乘比例）加到設定 +0x1dd8，限制範圍後呼叫亂數。正式生成需在單次認領後走這個原版判定。
- 0x3021510 在突變成立時還會更新相關玩家的統計計數；因此直接使用資料生成入口時，不能宣稱等同整個原版包裝函式的所有副作用。
- 證據：build/v104-breeder-effects-inheritance.txt。
- BreederProbe 0.3.0 已編譯並於遊戲關閉時安裝，SHA256 F0C182DD64A689A8B9064AD7B9B6CFC97E48CC213B7CA43E3E123C916E949532。舊版備份 build-processor/breeder-probe-install-backup-20260907/main-0.2.0.dll。
- 既有 CTest 5/5 通過；新 CAKE_EFFECT 實值輸出尚未遊戲內驗證。按 Ctrl+Alt+F7，預期輸出 Cake03 的原版效果，不改親代、不扣蛋糕、不生成後代。第一台 DLL／PAK 未修改。

實機唯讀探針 14:39:18／14:39:20 已完成：assigned=2、native-pair-eligible=true、duration=300、native-buff=4.5；箱內 Cake03=1284，query-status=1、requested-count=1，世界蛋 tracked=0/capacity=20。保存結構 880 bytes（0x370），效果結構 28 bytes，BreedCount offset=16；探針 DONE 記錄未扣蛋糕、未生成、未寫親代。使用者另以原版道具畫面確認 Cake03 為蔬菜蛋糕，說明為每次產下 2 顆蛋。requested-count 指輸入蛋糕份數，不是後代數；效果實值尚待原版查詢接入，不能僅靠結構 offset 宣稱已讀得 BreedCount=2。

本節覆蓋下方舊版 RVA 的現行使用範圍。新版 EXE SHA256 為 44B6295E70AA37B83D1C42CE1DCF865A7FFADCD300298B0E49A02BAD8EB83443。

- 配種資格反射 thunk 0x2884530 → 0x2de32b0；速度 thunk 0x2884460 → 0x2ddae90。
- 牧場資格 thunk 0x29adfe0 → 0x3005700。
- 道具查詢 thunk 0x2884800 → 0x2de8fc0；後者包裝生產用 0x2de8ff0，兩者不可混用 ABI。
- 速度呼叫鏈定位新牧場更新 0x301b370；效果查詢 0x2de9110，扣道具 0x2df93e0。
- 新扣道具 0x2df93e0 呼叫已核對的原生容器消耗 0x2fb98f0。
- 新後代包裝 0x3021510：初始化 0x2847780 → 資料生成 0x2de98f0 → 成功才建世界蛋 0x30216b0 → 清理 0x2847cc0。
- 證據：build/v104-breeder-reflected-entries.txt、v104-breeder-thunks.txt、v104-breeder-flow-anchors.txt、v104-breeder-farm-tick.txt、v104-breeder-consume-generate.txt、v104-breeder-offspring-core.txt。

上述為靜態候選與呼叫鏈核對，尚未把生成入口接入第二台。下一步在新版執行已安裝的唯讀 BreederProbe 0.2.0，讀回親代、蛋糕查詢、效果欄位及保存結構；不扣除、不生成、不呼叫原生 RVA。嚴苛斷線／中斷測試依使用者指示延至第二台基本功能驗證完成後。

狀態：2026-09-07 本機 EXE 靜態分析；未執行下列原生生成或扣除入口，未修改遊戲安裝。第一台保持原狀。

EXE SHA256：fe3c15064524bae1947852467c4f92bc22469acc033a3d3c8031eab4324e41e8。下列均為此版本 RVA，不是可跨版本呼叫的公開 API。

## 已讀到的控制流程

- `0x300ca60` 為牧場計時／完成流程候選。它讀取模型的進度／目標值，以 `CalcBreedBuffRate` (`0x2dd2e30`) 乘 delta 累加；達標後才走以下路徑。欄位與先前實機 300／4.5 查詢相符。
- 先以剩餘蛋位容量、道具效果中的計數檢查整批是否放得下；不足則直接離開完成區段。
- `0x2de0ed0` 是反射 `FindConsumableBreedItem` 的入口；生產流程實際使用相鄰的 `0x2de0f00`，回傳額外道具識別資料。不能把兩者當成相同 ABI。
- 在 `0x300cc6a` 呼叫 `0x2df11b0`。該函式先選道具，再透過 `0x2facf40` 提交容器扣除；後者也出現在已研究的第一台原版扣蛋路徑。返回首位元組非零時，牧場不進入後代生成區段。
- 成功後再次以返回的道具 ID 查詢效果 (`0x2de1020`)，讀取效果 offset 0x10 的計數並逐隻呼叫 `0x3012b90`；完成迴圈後將牧場進度歸零。
- `0x3012b90` 初始化暫存存檔參數 (`0x2846f40`)，再呼叫 `0x2de1800`；成功才呼叫後續世界蛋建構路徑 `0x3012d30`，最後以 `0x2847480` 清理暫存參數。
- `0x2de1800` 從 Work 取得親代，至少兩個才繼續；傳入兩個親代、道具效果與輸出緩衝區至 `0x2e6c460`。尚未把其所有參數與副作用驗證成可安全直接呼叫的契約。

## 對第二台的直接影響

1. 不可用「自己選一種帕魯」替代原版後代生成。
2. 普通／蔬菜蛋糕白名單與 1／2 顆批次規格，不代表可丟棄原版蛋糕的其他效果。SDK 的 FPalBreedingItemEffectData 同時包含天賦加成、突變率、配種順位、技能繼承等欄位，生成時須傳完整原版效果。
3. 原版先扣道具，再生成後代；不能直接把整個完成函式反覆呼叫當作可重試交易。第二台仍須處理自己的單次認領與固定結果保存。
4. 已找到「資料生成」與「世界蛋建構」之間的分離點候選；可用於研究隱藏孵化位，但目前未證明只呼叫前半段完全無其他副作用。

## 證據與未完成部分

工具：tools/inspect_settlement_native.py；讀取本機 EXE，沒有載入或執行遊戲程式碼。

主要證據位於 build/：
- breeder-farm-tick-20260907.txt
- breeder-consume-generate-20260907.txt
- breeder-offspring-core-20260907.txt
- breeder-inheritance-core-20260907.txt

下一步是確認輸出結構生命週期、效果指標與呼叫 ABI，並核對生成過程是否註冊個體或寫入其他狀態；完成前不把 RVA 接入正式產線。此輪沒有新增實機操作要求。

## 後續核對與 0.2.0 檢查版

- `0x3012b90` 的暫存輸出使用 `0x2846f40` 初始化、`0x2847480` 清理；與第一台 `ProcessorNative.hpp` 已使用的 PalIndividualCharacterSaveParameter 生命週期相符，仍須在配種場景讀回結構大小。
- `0x2de1800` 候選呼叫形狀為 (Work, WorldContext, 單位元組旗標, 效果指標的位址, 已初始化的輸出參數位址)。其 bool 返回值決定是否建立世界蛋。旗標不能未經核對就當成任意常數；原版由 `0x2df4b80` 計算。
- `0x2e6c460` 會在資料庫 offset 0x1a8 缺少物件時建立／快取內部 UObject，並執行亂數與多個後代屬性函式。因此「生成預覽」並非唯讀，尚不呼叫。
- 新證據 `build/breeder-output-finalization-20260907.txt` 顯示後續尚有多個呼叫，還不能宣稱所有副作用已排除。
- 已編譯獨立 BreederProbe 0.2.0：只加原版 CanConsumeBreedItem 查詢、箱內物品／數量、SpawnedEggInstanceIds 與容量、後代結構大小、蛋糕效果欄位 offset／size。按键仍為 Ctrl+Alt+F7，不執行生成、扣除、原生 RVA 或亂數。
- 0.2.0 在確認遊戲程序已退出後更新安裝；尚未實機驗證。安裝 SHA256 為 0BE08C094AA88D8CC0CEAACD7418FC2B0C7B293F5F6749AC89FD0363A7C13DE1。舊 DLL 備份至 build-processor/breeder-probe-install-backup-20260907/main-0.1.0.dll；第一台 DLL／PAK 雜湊與基線一致。五項既有本機測試通過不代表新反射讀取已實測。
