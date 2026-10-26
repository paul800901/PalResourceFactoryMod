# 施工與供電修正版（待遊戲驗證）

## 範圍

- 第一台蛋資源處理機：0.3.12-native-power。
- 第二台資源繁殖場：0.1.14-construction-power。
- 不改孵化、支解、不可返還與第二台已通過的原生配種流程。

## 修改

1. 第二台在 OnStartSimulation／OnRep_CurrentState 外觀事件前依自訂 BuildObjectId 套用模型，不再只等 concrete model 的更新。正常材質快取同步為自製模型材質，之後重套遊戲的施工狀態，避免清掉覆蓋材質就直接顯示完工外觀。
2. 建造工作量讀取目前載入的原版科技及建築列，分別取 26／36 級最大 RequiredBuildWorkAmount，僅覆寫兩台 PRF 建築列。保留科技顯示等級 1 的既有決定。不是固定秒數，會受玩家／帕魯工作速度影響。
3. 第一台缺少能源模組時，使用當前遊戲原生的模組建立、初始化與基地登記。沒有把供電判斷改成永遠通過。已有能源模組的第二台不走此補接路徑。
4. 第一台狀態改變時記錄能源模組及供電判定，便於區分缺電與處理失敗。

## 原生呼叫的本機研究依據

遊戲 EXE SHA256：44b6295e70aa37b83d1c42ce1dcf865a7ffadcd300298b0e49a02bad8eb83443。

- GetEnergyModule 反射 thunk 0x29b85c0 → 0x300a550 → 0x2fff0a0，從 concrete model 模組陣列找能源模組。
- 模組工廠 0x3007580：既有模組直接返回；不存在時以 concrete model 為 outer 建立 UObject 並加入 replicated module 陣列。
- 能源模組 vtable 0x7592fd0 的初始化 0x3039a70：讀取原版建築耗電欄位，取得所屬基地能源狀態。
- 登記 0x3043000 → 0x2d98b40：把所屬模型加入對應基地能源消費者清單。
- CanConsumeEnergy 0x302bfe0：以模組 CurrentState 判定，修正版不直接設定該狀態。
- 呼叫前比對三個入口的指令簽章；不適用其他遊戲版本的任意位址。

## 驗證界線

本地兩台 DLL 編譯通過；既有核心測試 5/5 通過。尚未在遊戲內驗證此修正版。
待檢查：第二台預覽與施工外觀連續、兩台新建造工作量、第一台有電時處理與停電暫停。
`[PRFBalance] BUILD_WORK` 會列出實際採用的原版參考建築及工作量；`ENERGY_MODULE_ATTACHED` 與 `STATUS` 供確認第一台補接結果。
