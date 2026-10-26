# 原版支解連續速度核對

範圍：只讀本機 EXE、SDK 與目前處理器程式。未修改 DLL、PAK、存檔或遊戲程序。

EXE SHA256：`fe3c15064524bae1947852467c4f92bc22469acc033a3d3c8031eab4324e41e8`，timestamp 1785940002。本文件 RVA 僅適用此版本。

## 原版靜態證據

- `RequiredConvertProcessTime` 反射描述指向參數元件 offset 0xa8；SDK 同時列出 ContainerSlotNum、ConvertQueueCapacity。
- 參數元件類別建立入口 0x29ae680 使用建構函式 0x29b2b10。
- 建構函式設定 offset 0xa0=10（容器容量）、0xa4=3（處理佇列容量）、0xa8=0x40a00000（float 5.0 秒）。
- 模型初始化 0x3007220：從建築元件讀取 0xa4 / 0xa8，存為模型 0x238 / 0x23c。不能將類別預設值直接宣稱為所有建築實例值。
- 更新入口 0x3014370（本函式跨 unwind 邊界，完整讀取 0x3014370:0x30144cf）：
  - 0x30143eb：每個 ConvertProcessArray 項目進度增加 delta / RequiredConvertProcessTime。
  - 0x3014403：比較完成門檻 1.0；完成呼叫 0x2ff81b0。
  - 0x3014419：檢查目前處理數小於 ConvertQueueCapacity。
  - 0x3014460：找最低進度項目；0x3014489 計算 1 / ConvertQueueCapacity。
  - 當最低進度達到該門檻（或目前為空）時，0x3014496 呼叫 0x2ff3670 補入下一隻。

結論：原版是重疊處理且錯開投入的流水式佇列，不是每隻完全結束後再開始下一隻。
若建築使用原生預設值，單隻延遲 5 秒，滿載穩態理論投入／出料間距約 5/3=1.67 秒，再加更新步長量化；不是每 5 秒才出一隻。這不是實測 FPS／出料秒數。

## 與目前 MOD 的差別

- ProcessorState 只有一個 processing / process_remaining，完整序列處理。
- disassembly_interval() 只讀參數元件類別預設值，沒有使用原版建築元件覆寫或佇列容量。
- EggProcessor 主處理每約 1 秒更新；選定下一隻的那次 advance 不消耗當輪秒數。上一隻結算完成才清除 processing，增加額外接續等待。
- 使用者本次遊戲紀錄約 6.0–6.2 秒一次，與 5 秒序列處理加排程空檔相符。

要對齊原版，需讀取原版建築實際的時間與佇列容量、採同一重疊／錯開規則。不能直接把每顆處理時間硬改成 1.67 秒：那會錯誤縮短第一隻延遲，也掩蓋佇列語意。每顆仍只能結算一次，動畫不作為結算時鐘。

## 尚未證實

- 原版 BP_BuildObject_DismantlingConveyor 的目前資產／實例是否覆寫 5 秒與容量 3。已存在本專案的唯讀萃取資產，但本輪未完成其未版本化屬性解碼。
- 原版實際同場連續出料時間（未進行遊戲內計時）。

證據產物：build/native-conveyor-parameters-20260907.txt、build/native-conveyor-timing-20260907.txt、build/native-conveyor-progress-20260907.txt、build/native-conveyor-tick-20260907.txt。其他候選函式掃描包含非目標類別，不應單凭相同 offset 認定用途。
