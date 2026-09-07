# Workshop publishing

Uses the same Steamworks.NET/native API and uploader App ID as the official
[Pocketpair PalworldModUploader](https://github.com/pocketpairjp/PalworldModUploader).
Steam must already be running and signed in to the publishing account.
No password is stored. Do not accept legal agreements on behalf of another user.

Place the official uploader's `Steamworks.NET.dll`, `steam_api64.dll` and
`steam_appid.txt` in `external/workshop-uploader` (not committed or distributed).
The official `steam_appid.txt` contains **4212470**, the uploader app; consumer
Workshop content is for Palworld **1623730**.

PowerShell 7 can compile and run the helper without a separate .NET SDK:

```powershell
pwsh -NoProfile -File tools/workshop-publisher/Publish.ps1 -Mode probe
pwsh -NoProfile -File tools/workshop-publisher/Publish.ps1 -Mode publish -PackageOrId <absolute-package-path> -Receipt <absolute-local-id-receipt>
pwsh -NoProfile -File tools/workshop-publisher/Publish.ps1 -Mode read -PackageOrId <workshop-id>
```

`publish` creates a new public item only when the receipt does not exist;
otherwise it checks ownership/app/title before updating that exact item. Keep
the receipt after partial failures to avoid creating duplicates. The helper
attaches the UE4SS and PalSchema dependencies and checks Steam's completion and
public readback. In-game installation still requires separate verification.

Only use publish with explicit authorization for that repository/package.
