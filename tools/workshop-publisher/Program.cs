using Steamworks;
using System.Text.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;

// Uses the Steamworks.NET/native libraries shipped by Pocketpair's official
// PalworldModUploader, not Steam credentials or a separately stored password.
public class Program
{
    const uint Palworld = 1623730;
    static T Await<T>(SteamAPICall_t call) where T : struct
    {
        bool done = false, failed = false;
        T value = default;
        using var result = CallResult<T>.Create((v, io) => { value = v; failed = io; done = true; });
        result.Set(call);
        var start = DateTime.UtcNow;
        var next = start.AddSeconds(30);
        while (!done)
        {
            SteamAPI.RunCallbacks();
            if ((DateTime.UtcNow - start).TotalMinutes > 10)
                throw new Exception("Steam response timed out; inspect existing item before retrying.");
            if (DateTime.UtcNow > next) { Console.WriteLine("Waiting for Steam response..."); next = DateTime.UtcNow.AddSeconds(30); }
            Thread.Sleep(50);
        }
        if (failed) throw new Exception("Steam I/O failure; inspect existing item before retrying.");
        return value;
    }
    static void Check(bool ok, string action) { if (!ok) throw new Exception(action + " rejected"); }
    static void Check(EResult result, string action)
    { if (result != EResult.k_EResultOK) throw new Exception(action + ": " + result); }
    static SteamUGCDetails_t Details(ulong id)
    {
        var r = Await<SteamUGCRequestUGCDetailsResult_t>(SteamUGC.RequestUGCDetails(new PublishedFileId_t(id), 0));
        Check(r.m_details.m_eResult, "Read item");
        return r.m_details;
    }
    static (string Cover, List<(string Name, string Url)> Extra) Media(ulong id)
    {
        var query = SteamUGC.CreateQueryUGCDetailsRequest(new[] { new PublishedFileId_t(id) }, 1);
        try {
            Check(SteamUGC.SetReturnAdditionalPreviews(query, true), "Request media");
            var result = Await<SteamUGCQueryCompleted_t>(SteamUGC.SendQueryUGCRequest(query));
            Check(result.m_eResult, "Query media");
            Check(SteamUGC.GetQueryUGCPreviewURL(query, 0, out string cover, 4096), "Read cover");
            var extra = new List<(string Name, string Url)>();
            for (uint i=0; i<SteamUGC.GetQueryUGCNumAdditionalPreviews(query, 0); ++i) {
                Check(SteamUGC.GetQueryUGCAdditionalPreview(query, 0, i, out string url, 4096,
                    out string name, 1024, out EItemPreviewType type), "Read gallery");
                extra.Add((name, url));
            }
            return (cover, extra);
        } finally { SteamUGC.ReleaseQueryUGCRequest(query); }
    }
    public static int Main(string[] args)
    {
        try
        {
            var init = SteamAPI.InitEx(out string error);
            if (init != ESteamAPIInitResult.k_ESteamAPIInitResult_OK) throw new Exception("Steam init: " + error);
            try
            {
                Check(SteamUser.BLoggedOn(), "Steam login");
                Console.WriteLine("Steam initialized; signed-in user available. App=" + SteamUtils.GetAppID());
                if (args.Length == 0 || args[0] == "probe") return 0;
                if (args[0] == "read")
                {
                    var d = Details(ulong.Parse(args[1]));
                    Console.WriteLine(JsonSerializer.Serialize(new { Id = d.m_nPublishedFileId.ToString(), Title = d.m_rgchTitle,
                        Result = d.m_eResult.ToString(), Visibility = d.m_eVisibility.ToString(), Bytes = d.m_nFileSize,
                        ConsumerApp = d.m_nConsumerAppID.ToString(), Description = d.m_rgchDescription }));
                    return 0;
                }
                if (args[0] == "media" && args.Length == 3)
                {
                    ulong mediaId = ulong.Parse(File.ReadAllText(args[2]).Trim());
                    var before = Details(mediaId);
                    Check(before.m_nConsumerAppID.m_AppId == Palworld && before.m_ulSteamIDOwner == SteamUser.GetSteamID().m_SteamID,
                        "Media ownership/app check");
                    var previous = Media(mediaId);
                    string folder = Path.GetFullPath(args[1]);
                    string[] names = { "flow-cover.jpg", "processor.jpg", "breeder.jpg" };
                    foreach (string name in names) {
                        var file = new FileInfo(Path.Combine(folder, name));
                        Check(file.Exists && file.Length > 0 && file.Length < 1000000, "Image size: " + name);
                    }
                    var edit = SteamUGC.StartItemUpdate(new AppId_t(Palworld), new PublishedFileId_t(mediaId));
                    Check(SteamUGC.SetItemPreview(edit, Path.Combine(folder, names[0])), "Set flow cover");
                    for (int i=1; i<names.Length; ++i) {
                        int index = previous.Extra.FindIndex(p => Path.GetFileName(p.Name) == names[i]);
                        string file = Path.Combine(folder, names[i]);
                        Check(index < 0 ? SteamUGC.AddItemPreviewFile(edit, file, EItemPreviewType.k_EItemPreviewType_Image)
                            : SteamUGC.UpdateItemPreviewFile(edit, (uint)index, file), "Set gallery: " + names[i]);
                    }
                    var mediaUpload = Await<SubmitItemUpdateResult_t>(SteamUGC.SubmitItemUpdate(edit,
                        "Images only: text-free process flow cover and both building previews; game files unchanged."));
                    Check(mediaUpload.m_eResult, "Media upload");
                    if (mediaUpload.m_bUserNeedsToAcceptWorkshopLegalAgreement) throw new Exception("Workshop legal agreement required.");
                    var after = Details(mediaId);
                    var media = Media(mediaId);
                    Check(after.m_nFileSize == before.m_nFileSize && after.m_rgchDescription == before.m_rgchDescription &&
                        after.m_rgchTitle == before.m_rgchTitle && after.m_eVisibility == before.m_eVisibility,
                        "Media-only readback");
                    Check(media.Extra.Exists(p => Path.GetFileName(p.Name) == names[1]) &&
                        media.Extra.Exists(p => Path.GetFileName(p.Name) == names[2]), "Both buildings readback");
                    Console.WriteLine(JsonSerializer.Serialize(new { Item=mediaId, Cover=media.Cover,
                        Gallery=media.Extra.ConvertAll(p => new { p.Name, p.Url }), Bytes=after.m_nFileSize }));
                    return 0;
                }
                if (args[0] == "describe" && args.Length == 3)
                {
                    ulong itemId = ulong.Parse(File.ReadAllText(args[2]).Trim());
                    var before = Details(itemId);
                    Check(before.m_nConsumerAppID.m_AppId == Palworld && before.m_ulSteamIDOwner == SteamUser.GetSteamID().m_SteamID,
                        "Description ownership/app check");
                    string description = File.ReadAllText(args[1]);
                    Check(!string.IsNullOrWhiteSpace(description), "Nonempty description");
                    var edit = SteamUGC.StartItemUpdate(new AppId_t(Palworld), new PublishedFileId_t(itemId));
                    Check(SteamUGC.SetItemDescription(edit, description), "Description");
                    var saved = Await<SubmitItemUpdateResult_t>(SteamUGC.SubmitItemUpdate(edit,
                        "Description only: clarify player instructions, scope and bilingual presentation; game files unchanged. / 僅更新中英說明與操作指引，模組檔案不變。"));
                    Check(saved.m_eResult, "Description update");
                    if (saved.m_bUserNeedsToAcceptWorkshopLegalAgreement) throw new Exception("Workshop legal agreement required.");
                    var after = Details(itemId);
                    Check(after.m_rgchDescription == description && after.m_rgchTitle == before.m_rgchTitle &&
                        after.m_nFileSize == before.m_nFileSize && after.m_eVisibility == before.m_eVisibility,
                        "Description-only readback");
                    Console.WriteLine("Verified description-only update; item=" + itemId + "; bytes=" + after.m_nFileSize);
                    return 0;
                }
                if (args[0] != "publish" || args.Length != 3) throw new Exception("Usage: publish PACKAGE RECEIPT | describe DESCRIPTION RECEIPT | read ID | probe");
                string package = Path.GetFullPath(args[1]), receipt = Path.GetFullPath(args[2]);
                using var info = JsonDocument.Parse(File.ReadAllText(Path.Combine(package, "Info.json")));
                string title = info.RootElement.GetProperty("ModName").GetString()!;
                string desc = File.ReadAllText(Path.Combine(package, "DESCRIPTION.txt"));
                string preview = Path.Combine(package, info.RootElement.GetProperty("Thumbnail").GetString()!);
                if (!File.Exists(preview) || new FileInfo(preview).Length >= 1_000_000) throw new Exception("Preview must exist and be under 1 MB");
                ulong id;
                if (File.Exists(receipt)) id = ulong.Parse(File.ReadAllText(receipt).Trim());
                else
                {
                    var created = Await<CreateItemResult_t>(SteamUGC.CreateItem(new AppId_t(Palworld), EWorkshopFileType.k_EWorkshopFileTypeCommunity));
                    Check(created.m_eResult, "Create item");
                    id = created.m_nPublishedFileId.m_PublishedFileId;
                    Directory.CreateDirectory(Path.GetDirectoryName(receipt)!);
                    File.WriteAllText(receipt, id.ToString());
                    Console.WriteLine("Created item " + id + "; receipt saved.");
                    if (created.m_bUserNeedsToAcceptWorkshopLegalAgreement) throw new Exception("Workshop legal agreement must be accepted by user; item " + id);
                }
                var existing = Details(id);
                Check(existing.m_nConsumerAppID.m_AppId == Palworld && existing.m_ulSteamIDOwner == SteamUser.GetSteamID().m_SteamID, "Item ownership/app check");
                if (!string.IsNullOrEmpty(existing.m_rgchTitle) && existing.m_rgchTitle != title)
                    throw new Exception("Receipt points to a differently titled item; refusing overwrite.");
                foreach (ulong dep in new ulong[] { 3625223587, 3625280368 })
                {
                    var dependency = Await<AddUGCDependencyResult_t>(SteamUGC.AddDependency(new PublishedFileId_t(id), new PublishedFileId_t(dep)));
                    if (dependency.m_eResult != EResult.k_EResultDuplicateRequest)
                        Check(dependency.m_eResult, "Add dependency " + dep);
                }
                var update = SteamUGC.StartItemUpdate(new AppId_t(Palworld), new PublishedFileId_t(id));
                Check(SteamUGC.SetItemTitle(update, title), "Title");
                Check(SteamUGC.SetItemDescription(update, desc), "Description");
                Check(SteamUGC.SetItemTags(update, new List<string> { "UE4SS", "PalSchema", "Gameplay" }), "Tags");
                Check(SteamUGC.SetItemContent(update, package), "Content");
                Check(SteamUGC.SetItemPreview(update, preview), "Preview");
                Check(SteamUGC.SetItemVisibility(update, ERemoteStoragePublishedFileVisibility.k_ERemoteStoragePublishedFileVisibilityPublic), "Visibility");
                var uploaded = Await<SubmitItemUpdateResult_t>(SteamUGC.SubmitItemUpdate(update,
                    "v0.3.17 preview: resume unstarted breeder settlement after normal world exit, reuse saved native drops, and reduce temporary object creation. Local regression passed; author confirmed processing after scoped recovery of old stuck eggs. Existing ambiguous receipts still require individual recovery; multiplayer remains unsupported. / 修正正常退出後結算無法續作，減少暫時物件。舊卡蛋復原後已確認可繼續處理；異常舊紀錄仍需個別處理。"));
                Check(uploaded.m_eResult, "Upload");
                if (uploaded.m_bUserNeedsToAcceptWorkshopLegalAgreement) throw new Exception("Upload received; user must accept Workshop legal agreement.");
                Console.WriteLine("Published https://steamcommunity.com/sharedfiles/filedetails/?id=" + id);
                var final = Details(id);
                Check(final.m_nFileSize > 0 && final.m_eVisibility == ERemoteStoragePublishedFileVisibility.k_ERemoteStoragePublishedFileVisibilityPublic, "Published readback");
                Console.WriteLine("Verified public item; bytes=" + final.m_nFileSize + "; title=" + final.m_rgchTitle);
                return 0;
            }
            finally { SteamAPI.Shutdown(); }
        }
        catch (Exception ex) { Console.Error.WriteLine(ex.Message); return 1; }
    }
}
