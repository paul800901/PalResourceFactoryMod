# Breeder construction actor correction — 0.1.16

User verified both machines working, 500 displayed work each, and 73–76 FPS in supplied screenshots after the prior update. Live BUILD_WORK logs name ElectricGenerator (level 26) and MedicalPalBed_04 (level 36), both 50000 internal work.

Remaining defect: custom holographic B-menu preview, original construction actor, custom completed actor. Log contains SKIN_STATE 1 and 3 but no 2.

Cause: the previous constructed(model) gate excluded the new construction actor from skin.refresh as well as production. Move the existing visual-only refresh before that gate in discover; remove its duplicate in update. Production admission still requires construction completion. Do not change processor, materials, balancing, power or settlement.

Native compilation and five existing core tests pass. Construction-phase visual behavior still requires an in-game test. Only the breeder DLL is replaced with a backup after verifying the game is stopped.
