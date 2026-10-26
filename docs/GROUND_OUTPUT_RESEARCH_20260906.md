# 原版支解地面出口：2026-09-06 本機程式證據

範圍：只替換 F9 單蛋測試的材料出口，不恢復故障建築／舊 runtime，不更改 AI，不複製掉落表。本記錄是目前 EXE 的靜態分析；不是已執行或遊戲內成功的證明。

## 版本

- EXE SHA256：`FE3C15064524BAE1947852467C4F92BC22469ACC033A3D3C8031EAB4324E41E8`。
- PE timestamp `1785940002`，SizeOfImage `0x9fad000`，image base `0x140000000`。
- 原始分析輸出：`build/direct-settlement-20260906/ground-drop-*.txt`；由本專案 `tools/inspect_settlement_native.py` 讀取遊戲二進位產生。

## 實際呼叫鏈

1. 原版支解處理 `RVA 0x2ff81b0` 呼叫已驗證的 `0x2e733b0` 取得 `FName + count + FVector` 清單，然後逐行呼叫 `0x2f99740`。
2. `0x2f99740` 的 ABI：`void(UObject* worldContext, FName-by-value, int32 count, FVector* location, FVector* releaseDirection, bool autoPickup)`。FName 為 8 bytes；位置與方向均為三個 double。原版傳入方向 `(0,0,1)`、`autoPickup=true`，與本版一致。
3. 該函式自行建立／複製原生陣列，透過 `0x2f992c0` → `0x2f98dd0` 建立原版暫存物品容器，不需要 MOD 自己仿造容器。
4. `0x2f99b20` 建立掉落初始化參數：TempContainerID、ReleaseDirection、bAutoPickup、PickupablePlayerUid，呼叫 MapObjectManager 生成入口 `0x306cf90`，並註冊原版回呼 `0x3050e30`。陣列、共享請求及暫存容器生命週期沿用原版。
5. 本機 SDK 宣告 `PalMapObjectDropItemModel` 繼承 `PalMapObjectPickableItemModelBase`，並實作 `PalMapObjectItemCollectableInterface`；搬運管理模組具有 `OnAvailableMapObjectConcreteModel`。這支持使用正常物品生成路徑接入拾取／搬運，**是否真的被搬運仍須實機確認**。

最高層入口無成功返回值，並使用非同步生成請求。本測試不自行持有請求、增加 AI 掛鉤或盲目重試，只記錄 `GROUND_REQUESTED`；不能把它寫成 `VERIFIED`。

## 本版差異及風險

- 保留完整原版後代資料、原版機率清單、單次認領、原版扣蛋及完成狀態清除。
- 移除箱子搜尋、分類過濾及入庫計畫。地面物品由原版建立與處理。
- 單顆孵化器測試出口暫定自身 +X 方向 200 cm、世界 +Z 60 cm；依模型旋轉計算。這不是兩座自製建築最終插槽座標。
- 生成要求前確認扣蛋讀回，但尚無跨「扣蛋＋非同步物品生成」的共同提交；中斷或原版拒絕生成可能使蛋損失。收據保守禁止重播，不宣稱自動恢復。
- 不在生成要求後立即請求存檔，以免把尚未建立的地面物品當成已保存；測試者確認材料拾取／搬運後再正常退出重進。
- 本次接受標準：看到正確地面物品、玩家能拾取、帕魯能搬進允許材料的箱子、未生成活帕魯、同一顆蛋沒有重複材料。
