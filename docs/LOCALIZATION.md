# Interface localization

Scope: technology unlock page, B construction menu, building/inventory titles and the custom Deposit all eggs button. Native game labels keep native translations. Model lettering and gameplay are unchanged.

Source of truth: `src/localization/strings.json`. Each language contains processor name, breeder name, deposit button, processor description and breeder description. Both technology and construction descriptions use the same complete text, including irreversible input rules. Unused cake is recoverable; consumed cake is not.

Run `tools/generate_localizations.ps1` to generate PalSchema translation files and the native button header; `-Check` verifies generated output and required numeric values. The 17 languages match https://store.steampowered.com/app/1623730/?l=english .

PalSchema translations use its native `translations/<culture>` mechanism with `global` English fallback. Hardcoded Name/Description fields were removed to avoid overriding translations. Row IDs were checked against upstream PalBuildingModLoader.cpp. Reference: https://okaetsu.github.io/PalSchema/docs/dev/guides/translations/intro .

Language selection uses the game's current language, not Windows language. Restart the game after changing its language: PalSchema loads translation tables at GameInstanceInit. No per-frame culture or global-object scan was added.

Version: processor 0.3.15-localization. Breeder binary stays 0.1.16. Compilation and five core tests passed; the 17 translations require in-game font/layout and switching checks, and are not claimed to have native-speaker review.

Installed with game closed: 45 scoped files copied and hash-verified. Breeder DLL hash remains D3F88CC8521570F6CCB9A4EDE6F7B284AEA48A8E06EBC6FBBED96B38AD1174CB. Additional zh-TW, zh-CN and es-ES folders are culture aliases, not extra languages. No PAK, model, save or progress file changed.

The initial public preview explicitly labels language UI verification as pending. Source is MIT licensed. Release packaging excludes development backups, checkpoints, save-derived logs, Unreal SDK junctions, and third-party game/toolchain assets. See README and the release page for publication status.
