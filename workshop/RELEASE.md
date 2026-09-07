# v0.3.15 initial public preview

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
