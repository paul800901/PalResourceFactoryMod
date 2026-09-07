# PRF 自製建築模型

2026-09-06，以既有 Blender 4.1.1 製作。這是可編輯的原創 3D 幾何與材質，不是概念圖片、原版模型改名或可運作的 MOD。沒有更動遊戲、存檔、既有 Blueprint 或執行期程式。

## 成品

- `PalResourceFactory_Facilities.blend`：兩座的並排展示主檔；包含分件模型、材質、攝影機與攝影棚。
- `models/SM_PRF_EggResourceProcessor.blend`：蛋資源處理機單獨原始檔。
- `models/SM_PRF_ResourceBreedingFacility.blend`：資源繁殖場單獨原始檔。
- `models/*.fbx`：兩份匯出模型，只有機身、獨立燈光表面與簡化碰撞形狀，不含攝影棚。
- `previews/`：並排、各自正面及背面實際 Blender 渲染，共五張。

## 外觀與比例

| 模型 | 寬 × 深 × 高 | 可見模型三角面 | 設計 |
| --- | --- | --- | --- |
| 蛋資源處理機 | 2.828 × 2.180 × 2.777 公尺 | 20,296 | 橘色投入漏斗、內部遮板、封閉艙門、側置馬達、散熱、狀態面板 |
| 資源繁殖場 | 3.650 × 2.477 × 3.165 公尺 | 35,100 | A/B 雙封閉親代艙、共同處理核心、單一蛋糕口、冷卻系統 |

象牙白烤漆、深灰機身、橘色低階機構、黃銅飾邊與青色燈條構成共同外觀。沒有外露刀片、帕魯、骨架或輸送帶動畫。下方是封閉傳輸接頭，不是可領取材料的輸出倉；蛋處理機的 54 格屬於後續介面／功能資料，不在外殼上做 54 個實體孔位。

## 匯出約定

- 單獨模型以底座中心為原點；地面 Z=0。公尺單位，正面 -Y，向上 +Z。
- FBX 可見幾何為兩個網格：`SM_PRF_...` 機身與 `SM_PRF_..._Status` 燈條。兩者原點相同。
- 四個／七個 `UCX_SM_PRF_..._NN` 簡化凸碰撞塊隨檔提供；碰撞屬於機身，燈條不需要碰撞。
- 八種簡單材質，不依賴外部圖片。`M_PRF_StatusCyan` 的顏色／亮度可供後续 Unreal 動態材質控制；目前只有靜態青色，未接工作狀態。
- 所有可見網格都有 UV0。這是各分件原有或自動展開的 UV，不是全模型無重疊 lightmap 或烘焙貼圖圖集。
- 可編輯分件在 `PRF_26_...` / `PRF_36_...` 集合；整併輸出與碰撞在預設隱藏的 `_EXPORT` 集合。不要把兩組同時顯示，否則會重疊。
- 目前沒有 LOD、貼圖烘焙、動態介面、開關門或機器震動動畫。

## 驗證與整合界線

已檢查五張實際渲染，並修正首輪面板遮擋及管路端點。`export_verification.json` 記錄 FBX 在 Blender 的重新匯入結果：兩個可見網格、UV、三角面數、尺寸、底部原點、材質與碰撞塊；單獨 `.blend` 也重新開檔檢查。

**未在 Unreal／Palworld 匯入或遊戲內驗證。** FBX 在 Blender 往返成功不代表 Unreal 的材質、碰撞、方向或建築可互動已通過。後續匯入時需確認公尺到公分的比例、保留兩個網格、重建對應材質／發光參數並檢查 UCX 碰撞。孵化、支解、親代管理、54 格輸入、耗電與材料結算仍是獨立程式整合工作。

## 重建

使用現有 Blender，不需安裝 Python 套件。這兩個命令只產生／更新本資料夾的自製美術輸出。

```powershell
& 'C:\Users\Paulus\Desktop\快速啟動\BlenderPortable64\App\Blender64\blender.exe' --background --factory-startup --python-exit-code 1 --python 'D:\PalResourceFactoryMod\art\blender\build_facilities.py'
& 'C:\Users\Paulus\Desktop\快速啟動\BlenderPortable64\App\Blender64\blender.exe' --background --factory-startup --python-exit-code 1 --python 'D:\PalResourceFactoryMod\art\blender\finalize_assets.py'
```

第一步建立幾何、匯出、渲染並保存主檔；第二步完成單獨 Blender 專案的開檔視角並做匯出往返檢查。渲染限制為 CPU 八執行緒，不呼叫遊戲或其 GPU 設定。
