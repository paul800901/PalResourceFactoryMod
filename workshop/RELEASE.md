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
