# 主機開房測試版（2026-09-20）

範圍：Windows Steam 遊戲內開啟多人世界，由開房主機執行原有自動處理。專用伺服器仍拒絕；遠端客戶端仍不結算。尚未完成本次遊戲內測試，不代表完整多人相容。

## 修改

- EggProcessor 與 AncientBreeder 允許 listen host 通過原有伺服器判斷。
- 共用 ManualSettlement 同步允許自訂機器在 listen host 結算；原版單槽孵化器的手動測試仍限單人。
- 保留原版孵化結果、支解掉落、容器扣除通知與地面出料。沒有新增公會、投入者或拾取權限。
- 保留現有執行檔／函式特徵驗證、結算紀錄、存檔與卸載處理。

## 本機驗證

- 兩個正式 native target 編譯成功，輸出在 `build-listen-native`。
- settlement_types、processor_state、async_disk、ancient_pending、breeder_state、facility_state 六組測試重新編譯並全部通過；這些測試不含實際網路或引擎行為。
- 兩個 DLL 均具有 start_mod / uninstall_mod；98 / 124 個 UE4SS imports 在安裝中的 UE4SS.dll 均存在。
- 目前遊戲 PE timestamp=1789097921、SizeOfImage=0xa011000，符合既有允許範圍。
- 遊戲關閉時已替換本機兩個工廠 DLL，備份及 SHA256 讀回在 `build/listen-host-20260920/installed-backup-220521/install-readback.json`。未動存檔、progress、settlements、PAK 或其他模組。

## 遊戲測試

先開多人測試世界，在供電正常時各測蛋處理機與繁殖場的一次完整出料、拾取／搬運及正常退出重進。日誌應出現 listen-host-test=true，不能僅以 READY 當作完成。

主機測試通過後，另測加入玩家的投入、顯示及拾取。主機單獨運作不證明遠端玩家同步已通過。晶石機另有純工作燈客戶端修正，其原版轉換流程不變。

本次沒有發布工作坊或 GitHub，也沒有變更公開版本及相容性宣稱。需要還原時，先關閉遊戲，再依 install-readback.json 將兩份 Backup 複製回各自 Target。
