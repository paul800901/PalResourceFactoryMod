# Processor 0.3.14: relog power registration

Status: compiled locally; five core CTest tests pass. Installed after verifying the game exited; installed DLL SHA256 A43674AC3C4FDB2C205527AF7E336EBF897CA7B57A44593C3D13AB198D162544 matches the build. Relog behavior is not game-verified yet.

Previous processor DLL preserved at build-processor/processor-before-0.3.14-backup.dll (SHA256 8A15D1F9663DCE2E8DA37BCD77530B768E7F22C6A498AB797D68710C8724F2A9). Installed breeder hash remains D3F88CC8521570F6CCB9A4EDE6F7B284AEA48A8E06EBC6FBBED96B38AD1174CB. No save or progress files changed.

Scope: orange PRF_EggResourceProcessor only. Keep installed breeder 0.1.16 unchanged.

Observed log: container 2e705e56481bde15f1946cb0e79d3628 was powered after new construction, then attached with powered=false after reload. Subsequent three egg inputs locked successfully, with no settlement failure. The problem precedes settlement.

Native inspection: Palworld 1.0.4.102642 executable SHA256 44b6295e70aa37b83d1c42ce1dcf865a7ffadcd300298b0e49a02bad8eb83443.

- Previously called RVA 0x3043000 -> 0x2d98b40 -> 0x2462720. This REMOVES matching consumer GUIDs; previous registration interpretation was incorrect.
- Correct registration RVA 0x303d750 -> 0x2d952b0 searches for the GUID and appends only when absent, then calls 0x303be20 to synchronize real base power state.
- The latter reads the native electric function state; it does not force power on or bypass energy accounting.

Change: use correct registration endpoint, validate its call target, and defer initial attachment until the camp's electric function exists. No periodic global object scan, no save edits, no refunds, no settlement changes. Normal 200 / incubating 400 power rates remain unchanged.

Required live check after installation: existing processor with eggs resumes while powered; stop/restart the game and verify it resumes again without rebuilding; confirm actual power loss pauses it. Core tests cannot establish the native lifecycle result.
