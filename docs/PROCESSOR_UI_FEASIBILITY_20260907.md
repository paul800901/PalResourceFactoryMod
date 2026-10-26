# 專屬倉庫介面：本機載入驗證

狀態：直接沿用原版打包資產的路線未通過；未安裝新 UI。

- 本機 SDK 的 PalHUDService.Push 與 PalHUDInGame.PushWidgetStackableUI 接受 WidgetClass、DispatchParameter；存在自選介面類別入口，但尚未驗證從蛋處理器互動端接線。
- UE 5.1 WidgetBlueprintGeneratedClass.cpp 的 FindWidgetTreeOwningClass 會向父類尋找布局。這只是引擎支援繼承的證據，不是原版箱子可成功作為可編輯父類的證據。
- 編輯器與 SDK 未附原版 WBP_IngameMenu_Chest 的可編輯資產。測試使用之前自使用者遊戲抽取的 uasset/uexp 副本，置於本專案 PRFUIProbe 暫存目錄。
- 預設載入失敗：unversioned package cannot safely load in editor。
- 僅在測試命令列編輯器程序設定 s.AllowUnversionedContentInEditor=1 與 cook.AllowCookedDataInEditorBuilds=1 後，仍有多個缺少的 UI／貼圖相依；反序列化 delegate 時在 FLinkerLoad.CreateExport 發生 Array index out of bounds: 254 from an array of size 224。不能把這個錯誤單獨歸因為引擎版本、缺少相依或 SDK 欄位，尚未隔離。
- 紀錄：build/processor-ui-probe.log。測試程序失敗，非遊戲崩潰；沒有成功建立子介面或可安裝 UI。
- 測試副本已移至 build/ui-probe-rejected-20260907，原 Content/PRFUIProbe 無資產，避免進入正常 cook。腳本的容許打包載入旗標改為需明確 -PRFAllowCookedUIProbe 才啟用，不更改專案或全域設定。
- 已安裝 DLL 讀回仍為 A1AE9A3FA17A82171BC4E8C48F79E02907B176D2BA7E6F9D863ADCECCE7B0BD3，未修改遊戲 MOD、其他專案或存檔。

結論：目前不能直接複製／繼承此打包 UI 來交付專屬介面。這不是證明所有專屬 UI 方案都不可能；完整重建依賴與反序列化契約屬於更大的工作，不能為改按鈕預設文字順手展開。較小替代方案為在原版 UI 建立／容器指定完成且繪製前設定本機器的標題與提示，仍須先核對該生命周期入口，不能繼續使用每秒覆寫或重用已標記狀態。
