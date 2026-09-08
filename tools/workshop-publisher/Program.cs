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
                if (args[0] != "publish" || args.Length != 3) throw new Exception("Usage: publish PACKAGE RECEIPT | read ID | probe");
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
                    "v0.3.16 preview: fix ancient breeder stale slot lifecycle across asynchronous settlement. Preserve once-only receipts. Local regression tested; in-game fix validation pending."));
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
