# 古代配種器完成出口核對

後續實作更新：獨立執行模組、ProcessEvent guard 與非同步結算已串接並編譯，仍未安裝／實機驗證；最新狀態及限制見 `BREEDER_IMPLEMENTATION_20260907.md` 頂部。下方未完成清單保留為本次靜態核對時點，不代表串接後的最新程式狀態。

狀態：静態反組譯與本機編譯，未實機執行新 adapter。版本 v1.0.4.102642；EXE SHA256 `44B6295E70AA37B83D1C42CE1DCF865A7FFADCD300298B0E49A02BAD8EB83443`。

## 現行 EXE 接點

| 接點 | RVA | 已看到的行為 |
| --- | --- | --- |
| OnFinishWorkInServer native thunk／body | 0x29d44f0／0x303f460 | 找到 WorkProgress 的紀錄，呼叫原版孵化 0x3035620，把結果移入完成 SaveParameter |
| 古代配種器完成 virtual +0x498 | 0x30505d0 | 更新耗電及啟用狀態；此 native body 不送活體入庫 |
| ObtainHatchedCharacter_ServerInternal thunk／body | 0x29d42e0／0x303c070 | 領取出口，0x3191f60／0x31920b0 轉交結果；清完成資料後消耗蛋槽 |
| OnUpdateContainerContentInServer thunk／body | 0x29d4960／0x3047070 | 查蛋槽；空槽且無同一蛋的新槽時，呼叫 virtual +0x470 移除完成紀錄 |
| 古代 model virtual +0x470 | 0x304a4b0 | 釋放 WorkProgress、清 egg 引用、解構完成 save、移除項目，呼叫同步更新 |

古代 model constructor 0x29e7f70 設 vtable RVA 0x75ce918；用此表確認 +0x470、+0x498，不能拿其他孵化 model 的 override 推定。

完成陣列 entry stride 0x398：SlotIndex +0xc、WorkProgress +0x10、PalEggData +0x18、HatchedCharacterSaveParameter +0x20（size 0x370）。實作透過反射定位，對已核對 layout 檢查；原生 save 使用 CopyScriptStruct，不能序列化原始指標 bytes。

蛋槽來自 GetItemContainerModule；蛋糕箱是另一個 BreedItemContainer。完成資料先複製到 transient individual parameter，再呼叫第一台同一個原版 butcher entry，不再次孵化或建立活體。

## 證據

`build/v104-ancient-hatch-finish-obtain.txt`、`v104-ancient-obtain-complete.txt`、`v104-ancient-constructors.txt`、`v104-ancient-finish-override.txt`、`v104-ancient-completion-notification.txt`、`v104-ancient-transfer-entry.txt`。

本次追加：`build/v104-ancient-container-update-entry.txt`、`v104-ancient-container-update-thunk.txt`、`v104-ancient-container-update-body.txt`、`v104-ancient-slot-removal.txt`。每份均有目前 EXE hash；清槽相關 body 已讀至返回。

## 未完成，禁止提升狀態

1. 第二台獨立建築及執行期領取攔截未部署。完成回呼 native body 不入庫，不等於已排除 Blueprint／間接呼叫；實機必須確認。
2. 新 adapter 的反射欄位、古代配種器生成的 dynamic egg identity 與完成紀錄配對尚未實機驗證；不符合就停止，不用 slot+CharacterID 代替唯一身份。
3. RNG durable claim／固定材料保存／原版扣蛋通知與讀回／原版落地材料交易尚未串接。只有函式並不構成防重複提交。
4. 領取與移出蛋的操作須在第二台接管時封鎖；不可只在出料後才清完成紀錄。親代與原版古代配種器不得受影響。
5. 先做普通／蔬菜蛋糕單批產線；嚴苛斷線測試依使用者要求延後。
