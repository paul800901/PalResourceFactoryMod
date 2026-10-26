# PalSchema 資料層

此目錄放置 `buildings`、`translations` 與必要的 `paks`。

- `buildings/facilities.json` 已包含兩座建築的 1 級普通科技入口與設計進度說明。本輪登入隔離測試暫用木材 ×1、免供電，兩座都直接指向原版可互動 Blueprint。
- `paks/PalResourceFactory_P.pak` 保留為開發中的自製建築資產，但 0.3.5 登入隔離包不會複製或載入它；36 級子 Blueprint 曾以不完整的臨時父類烹煮，不能再進入遊戲測試包。
- native 以本 MOD 專用 Map Object ID 限定攔截範圍；原版掉落查詢與基地倉儲提交已接線，但仍必須通過全新測試世界驗證。
