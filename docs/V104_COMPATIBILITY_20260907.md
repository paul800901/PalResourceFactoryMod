# Palworld v1.0.4 相容性定位

本輪範圍：第一台蛋資源處理器；第二台繁殖實作暫停。只保留既有處理／投影／投入功能，不新增功能。

## 新版基準

- Steam BuildID：25094871。
- EXE SHA256：44B6295E70AA37B83D1C42CE1DCF865A7FFADCD300298B0E49A02BAD8EB83443。
- PE timestamp：1788339267；SizeOfImage：0xa011000；檔案大小：161802312。
- 舊版 timestamp 1785940002、SizeOfImage 0x9fad000 的地址不可繼續使用。

## 原生入口

|用途|舊 RVA|新 RVA|
|---|---|---|
|原版支解計算|0x2e733b0|0x2e7a830|
|孵化保存資料生成|0x3026700|0x3035620|
|地面出料|0x2f99740|0x2fa5ed0|
|容器變更規劃|0x2fb44c0|0x2fc0ff0|
|容器變更提交|0x2fa8ef0|0x2fb5680|
|容器存取檢查|0x2fa3460|0x2fafbf0|
|動態蛋資料清除|0x2ea74d0|0x2ed8c80|
|原生預設操作選項|0x93e4988|0x940ca10|

原版新支解場 0x3006f20 先呼叫 0x2e7a830，再使用六參數 0x2fa5ed0 出料。新孵化領取路徑 0x303c070 在 0x303d3ad 載入預設選項，再呼叫原生消耗 0x2fb98f0；該消耗流程呼叫新動態清除 0x2ed8c80。

`tools/compare_entry_evidence.py` 比對完整函式反組譯，僅正規化重定位的 RIP 資料地址與外部呼叫／跳轉地址，保留欄位、常數與函式內相對分支。外部地址正規化不能單獨證明被呼叫函式的身分，另以以上原版呼叫鏈交叉核對。

- plan 273、apply 326、access 179、dispose 31、ground 109、consume 166 條指令結構相符。
- hatch 110 條：一處遊戲內部欄位 0x8c0 → 0x910。
- butcher 270 條：一處遊戲內部欄位 0x1468 → 0x14c0。
- 上述兩個欄位由新版原生函式存取，不由 MOD 複製舊偏移。
- 新版原生孵化領取仍使用 0x370 大小的保存結構；蛋保存資料仍由 +0x78 讀取。MOD 的反射欄位／大小檢查保留，未放寬。

證據位於 `build/v104-*-entry*.txt`、`build/v104-converter-chain.txt`、`build/v104-claim-complete.txt`、`build/v104-consume-plan.txt`、`build/v104-cleanup-candidates.txt`。旧版對照位於 `build/direct-settlement-20260906/`。

## 驗證與限制

- 第一台版本：0.3.10-v104-compat-local-test。
- 已完成原生 DLL 本機編譯；既有 CTest 5/5 通過。這些不是新版遊戲內驗證。
- 已在遊戲關閉時替換第一台安裝 DLL，來源與安裝讀回 SHA256 均為 `4C45741E9C59F393313151330B9D8C65FD4E85EF24B1EB1F22124AE7DF67239A`。
- 舊 DLL 備份 `build-processor/baseline-0.3.9-20260907/main.dll` 與替換前安裝檔一致：`B8294DBC9F7F2F457921C0AF70FDC98A949AFB49A60D6B7D6100F413AED62F19`。此備份是舊遊戲版本的基準，不代表能用於新版 EXE。
- 保留 PE 與函式前綴檢查、伺服器權威限制、每顆蛋結算紀錄及不可返還規則。
- 不修改 PAK、PalSchema JSON、存檔、既有處理紀錄或其他 MOD。
- 尚未驗證：新版 UE4SS 實際載入、第一台完整處理與重進、非零孵化時間、多人遊戲、第二台新版探針。
- 下一步由使用者啟動測試世界，以少量蛋測試全部投入、孵化／支解／出料與處理中正常退出重進，再讀本輪 UE4SS 日誌。
