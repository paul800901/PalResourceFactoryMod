# 2026-09-08 performance / construction candidate

User observation: both machines present 11–14 FPS; removed 18+ FPS (23 captured).
Loss also occurs before construction finishes. These are observations, not a controlled GPU/CPU profile.

## Code findings and changes

- Repeated `find()` calls resolve identical reflection objects through UE4SS object search. Cache weak references per thread; expired references are resolved again. This removes repeated searches from per-machine updates without changing settlement results or authority.
- Neither runtime previously excluded unfinished actors before processing/container discovery. Require CurrentState >= Available (3) before processing. Native construction is untouched.
- Breeder read the entire egg container and completion list twice in one update; reuse the first snapshot within that update.
- Work balance looked up LevelCap without IncludeSuper; SDK places it in the parent technology row. Read in chain; retry at 3 seconds instead of 0.1 seconds while data is unavailable. Successful updates retain the existing BUILD_WORK reference/value log. Maxima still come from loaded native level 26/36 buildings, not guessed constants.
- Remove periodic diagnostic indicator scans from the update loop. Global skin/widget filters compare FNames rather than allocating names as strings. V egg lock remains unchanged.
- Replacement breeder mesh has different material slots from the original. Apply the native manager's Highlight/Building material to every replacement slot after the native state callback; restore mesh defaults when built. Tick handles direct state transitions. No new materials or asset packages.

## Validation boundary

Native compilation and existing 5 core tests pass. No live game FPS measurement or F/C interaction verification in this candidate. Construction hologram and BUILD_WORK values require live readback. Existing no-refund / native settlement guards remain active. No original game building IDs changed.

Versions: processor 0.3.13-construction-performance; breeder 0.1.15-construction-performance.
Only the two project DLLs are installed after checking Palworld is stopped. Previous DLLs are backed up in build-processor/performance-construction-20260908/backup.
