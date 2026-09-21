# v0.3.20 native package repair — 2026-09-21

The v0.3.19 Workshop package accidentally reused the v0.3.15 processor DLL.
That binary rejects Palworld 1.0.5 before registering the processing and input
locks, while the building data and materials still load. This exactly produces
a visible machine whose eggs remain removable and never process.

- Package version: `0.3.20-public-preview`.
- Target: Windows Steam Palworld `1.0.5.102999`.
- Packaging now rebuilds both native targets from current source before copying
  them; matching an old build to an old installed DLL is no longer sufficient.
- Local log readback confirms `READY`, `ATTACHED` and `powered=true`. The public
  tester's complete egg consumption/material output remains the runtime gate.
- Clean native build and all six core tests passed. Package copy verification
  covered 58 source files; the archive contains no UE4SS DLL, global mod list,
  saves, progress files or settlement receipts.
- Candidate SHA-256:

```text
F6805C627D6B9EC4B27A0169756401B62D4DB896783BFA076D66971DE39757A9  Processor main.dll
2020FBE135D84B50CF254FFDD2C497FF05CBA2F9D95EC4755FA8C72C59D73445  Breeder main.dll
196DA8B757DCA8D95B5A06DDED699F76DCDAF8F81E3EECC045F3ED331795AB85  public-v0.3.20 ZIP
```

Publication readback is recorded after upload.

# v0.3.19 forced DirectX 12 material fix — 2026-09-20

This release cooks both custom machines with the SM6 shader path required by
forced DirectX 12. It fixes the beige/checkerboard fallback materials observed
with `-dx12`; the author confirmed the intended appearance and normal operation
in local single-player on Windows Steam Palworld 1.0.5.102999.

- Package version: `0.3.19-public-preview`.
- GitHub tag: `v0.3.19` (prerelease).
- Workshop item: `3797706995`.
- Processor PAK SHA-256: `959F67B9D9C8B9BD2F37FB96151D5ECACBF0D2AF8884A6CF59929F42DD0DFFA4`.
- Breeder visual PAK SHA-256: `F5845101AF31E5021E9F974F0229BB678BE97B52E3D646AF71220A23FDF942D1`.
- Release ZIP SHA-256: `BE0F4B2C798E7CC75748A85F7ACDF065BCA265B5BC435C4C3DB95D1C244136AF`.
- Gameplay, native DLLs, data tables, saves and processing records are unchanged.
- Fresh Workshop installation, FPS, long-running operation, multiplayer and
  dedicated servers remain unverified.

# v0.3.18 compatibility recovery preview — 2026-09-15

This release corrects the executable identity check that rejected Windows Steam Palworld 1.0.5.102999 before either native module initialized. It retains the 1.0.4 identity and existing native signature checks. The author confirmed functional recovery in local single-player; this is not a full regression or fresh Workshop installation test.

- Package version: 0.3.18-public-preview.
- GitHub tag: v0.3.18 (prerelease).
- Workshop item: 3797706995.
- Target: Windows Steam Palworld 1.0.5.102999; local single-player recovery confirmation.
- Evidence: 17:03:53 both native modules logged READY; at 17:04:17 the custom breeder model was attached, both buildings were ATTACHED, and the processor power module reported powered=true. The author confirmed functional recovery and provided a screenshot showing the custom model and materials in front. This does not establish full machine regression, item-by-item or duplicate-commit behavior, FPS, long-running operation, multiplayer, dedicated servers, or a fresh Workshop installation.
- Processor DLL internal log version: 0.3.15-localization; SHA-256: C65F8C31435512FFFA4D22B6B490F7EECE0AEAEECB20D9283174705680EE57FA.
- Breeder DLL internal log version: 0.1.18-unload-continuation; SHA-256: 13D49B9515649C38342F774C942BF8C05718317554FBBE95727A17D3B84C30E9.
- The planned package excludes saves, processing records, and recovery scripts. Existing blocked eggs may need individual diagnosis. No new 1.0.4 in-game retest is claimed.
- Workshop change notes and GitHub release body: workshop/CHANGELOG.txt.

# v0.3.17 normal-exit continuation preview — 2026-09-11

Resource Breeding Facility work that has not started its native calculation or
source consumption is now retained on normal world exit. Reload reuses already
saved native drop results. Completed-slot checks no longer create temporary
individuals; only the actual butchering calculation creates one.

- Package: `0.3.17-public-preview`, same Workshop item `3797706995`.
- Target: Windows Steam Palworld `1.0.4.102642`, single-player.
- Processor: `0.3.15-localization`; breeder: `0.1.18-unload-continuation`.
- Required Workshop dependencies remain UE4SS `3625223587` and PalSchema `3625280368`.
- Six local core/fake-boundary tests and 17-language generation checks pass.
- The author confirmed continued processing after individual recovery of five
  old stuck eggs. That one-time local recovery is not shipped. Old ambiguous
  receipts remain isolated; this update does not automatically repair them.
- Long-running, FPS, fresh Workshop installation, multiplayer and arbitrary
  forced-exit/crash behavior are not established by this test.

Steam accepted the content update and returned public item size **4,530,112 bytes**.
A separate readback matched the complete v0.3.17 description. Publication is
verified; fresh installation and in-game behavior remain separate checks.

Package sources were checked against the installed files before removal of the
manual installation: 2 DLLs, 4 custom PAKs, both building definitions and language
data. There are 57 copied files plus generated Info.json and SHA256SUMS.txt.
No local recovery script, receipt, log, save, SDK or other mod is included.

```text
4BDF474E21502F7339BCB5E28AFBA962AC2EB65A79A160A74AFE84ADB1F3F206  Processor main.dll
97419CD36D29A3825F26CF6587EBC1B81A58DEFC5011B4B3F9018EC9B2480CB2  Breeder main.dll
979CD51F4FE345C7922EAC52ABEA40EF7CB24D6B144BF0A4DF5ECA656A454510  public-v0.3.17-20260911.zip
```

Older binaries cannot read the new continuation markers. Preserve settlement
state when switching installation methods; do not reset it as an update step.

## v0.3.15 initial public preview (historical)

- Repository: https://github.com/paul800901/PalResourceFactoryMod
- Workshop: https://steamcommunity.com/sharedfiles/filedetails/?id=3797706995
- Workshop item: `3797706995`; package: `PalResourceFactoryMod`
- Package version: `0.3.15-public-preview`
- Game target: Windows Steam Palworld `1.0.4.102642`
- Processor DLL: `0.3.15-localization`
- Breeder DLL: `0.1.16-construction-actor`
- Required Workshop dependencies: UE4SS `3625223587`, PalSchema `3625280368`

Steam returned successful content submission and public readback with a nonzero
file size. The anonymous public page exposes the title, subscription control
and both required dependencies. This verifies publication, not installation.

The release archive and Workshop package contain the same 2 DLLs, 4 custom
PAKs, both schema building definitions and 17-language translation data.
57 copied files were checked against their sources; `Info.json` and checksums
are generated separately. No game saves, checkpoints, logs, SDK or dependency
binaries are shipped. No other mod's DLL or configuration is included.

SHA256:

```text
163DA5D254F63335AFC2B18AEE3C04C1BF912398CA7890C0C38B7210377D1D03  Processor main.dll
D3F88CC8521570F6CCB9A4EDE6F7B284AEA48A8E06EBC6FBBED96B38AD1174CB  Breeder main.dll
E7DCA0CF56DFD7A836F5F00ADA4CF8EB32619EBB71DD627BE0130D01825184A7  PalResourceFactory-v0.3.15.zip
```

Five core tests and localization generation checks pass. Author-reported
single-player tests cover construction, processing, restart/forced exit,
power recovery, repeated input, cancel/demolition and cake refund rules.
17-language UI layout/switching, fresh Workshop installation and multiplayer
remain unverified. See README for the retained level-1 preview unlocks and
irreversible input warnings.
