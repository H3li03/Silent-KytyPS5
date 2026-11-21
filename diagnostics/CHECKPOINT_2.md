# CHECKPOINT 2 — Silent-KytyPS5 / Silent Hill: The Short Message
Date: 2026-11-21. Resume document for a NEW ChatGPT conversation. Read this entire file before proposing changes.

## Repository and safe working state
- Repository: https://github.com/H3li03/Silent-KytyPS5
- Working branch: `diagnostics/hdr-path-consistency-20261121`.
- Most recent code commit for this checkpoint: `890382d1be0be3de14246b72a88ab6e89a2a00af` (experimental modes 974 and 975).
- IMPORTANT: user has LOCAL UNCOMMITTED changes in `src/common/emulatorConfig.cpp` and `src/graphics/presentation/videoOut.cpp`, plus backups. Never use `git reset --hard`, `git clean`, force checkout, or overwrite local files. Use `git fetch` + `git merge --ff-only FETCH_HEAD` on the correct branch; check status and resolve conflicts without destroying local work.
- All prior logs and test logs MUST be preserved. Every new run gets its own timestamped folder; never overwrite/delete an existing log without first copying it to a unique archive path. This includes stdout, stderr, diagnostics, shader logs, pass dumps.
- User prefers concrete Italian instructions, ready-to-paste PowerShell, real rendering corrections rather than disabling draws or arbitrary clamping.

## Hardware and symptom
- Windows, AMD Radeon RX 6900 XT, AMD proprietary driver 26.10.1, Vulkan 1.4.315.
- Game: Silent Hill: The Short Message, title PPSA10112.
- Original symptom: large white/gray overexposure, much 3D absent; HDR on makes entire image white. SDR used for A/B.
- Vulkan RT format 97 = `R16G16B16A16_SFLOAT`; 122 = `B10G11R11_UFLOAT_PACK32`; also `R32G32B32A32_SFLOAT`.
- Earlier traces included missing vertex programs/dropped draws, resource-table synchronization issues, shader permutations (~749), no validation VUIDs. These may independently explain still-missing geometry. Do not claim blending is the sole root cause.
- Earlier experimental envs `KYTY_STENCIL_DEPTH_ALIAS`, `KYTY_CMASK_FAST_CLEAR`, `KYTY_DRAW_SEQUENCE_FAST`, `KYTY_GUEST_SDR_MODE` did not fix.

## Commit history and what was changed
All changes below were made on branch `diagnostics/hdr-path-consistency-20261121` in `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`:
1. `531092dea337e7607bbc4209a6c00a84131b91c3`: introduced opt-in `KYTY_FLOAT_RT_NO_BLEND=1`, suppress blending for float RT.
2. `7066a57e14c125d3b09b816653b5e3655b0e59e7`: diagnostic `kyty-float-blend-diagnostic.txt`, mode and power-of-two counters.
3. `50dd279ff84f194face3c0044233c42795c94634`: modes 0, 1, 97, 122.
4. `62f65e7d02e5a06f05ad1ef270b2d4a57d0607cb`: modes 971 (format 97, src=1 dst=4 op=0), 972 (format 97, src=1 dst=1 op=0).
5. `b5df923e40285dd781e2088f4b9b6284fcb3e8e5`: modes 9720 (slot 0) and 9721 (slot 1).
6. `283478b50d196c3b8b5e1dbe79ad3590b5924b24`: mode 973: keep blending but change color destination factor to `Prospero::BlendFactor::kOneMinusSrcAlpha` for FP16 format 97 and src=1 dst=1 op=0. Failed visually (mostly white), although >8192 adjustments were logged.
7. `890382d1be0be3de14246b72a88ab6e89a2a00af`: NEW opt-in modes 974 and 975, preserve all draws and blending. For format 97, enabled blend, `bc.color_srcblend==1 && bc.color_destblend==1 && bc.color_comb_fcn==0`:
   - 974: change color source factor to `Prospero::BlendFactor::kSrcAlpha`, leave destination as One; approximate RGB: src*srcAlpha + dst.
   - 975: source `kSrcAlpha`, destination `kOneMinusSrcAlpha`; approximate RGB: src*srcAlpha + dst*(1-srcAlpha).
   - Original mode 0 for these passes: src + dst.
   - Logs `mode974_adjusted=...` / `mode975_adjusted=...` at power-of-two counts, with slot and format. These are experimental equations, NOT verified PS5/Vulkan mapping fixes.
   - Important implementation detail: adjustments occur before `BlendAlphaRemapEnabled()` branch; inspect downstream remapping, blend factor conversion, `separate_alpha_blend`, pixel-shader alpha and final composition before claiming a root cause.

## Visual experiments and results
- Mode 1: white gone, almost all 3D missing; at 32768: blend_enabled=3097 suppressed=3097.
- Mode 97: white gone, almost all 3D missing; at 32768: blend_enabled=3120 suppressed=3120.
- Mode 122: still white; at 32768: blend_enabled=3134 suppressed=0.
- Mode 971: white gone, almost all 3D missing; at 65536: blend_enabled=14124 suppressed=1056.
- Mode 972: reduced white and MORE 3D elements visible (best of the suppression modes); at 32768: blend_enabled=2872 suppressed=1838.
- Mode 9720: white gone, almost all 3D missing; at 32768: blend_enabled=3104 suppressed=1557.
- Mode 9721: earlier visual errors reproduced; at 65536: blend_enabled=14574 suppressed=4827.
- Mode 973: still predominantly white, despite >8192 actual changes to destination factor, across slots 0 and 1.
- **LATEST BREAKTHROUGH, USER-CONFIRMED: BOTH 974 AND 975 show MORE 3D than usual and the image DOES NOT tend toward white.** These are the best current results and preserve blending. No user answer yet on which of 974 vs 975 looks more natural/has more geometry, or what remains visually incorrect. Ask to compare if useful, but don't delay principled code investigation.
- 974 actual run: exit code 0, `KYTY_FLOAT_RT_NO_BLEND raw=974 enabled=1 mode=974`; `mode974_adjusted=8192 slot=0 format=97`; `float_rt=65536 blend_enabled=14545 suppressed=0 slot=0 format=97`. Log folder `C:\Giochi\SilentKyty\Silent-KytyPS5\diagnostics\blend-fix-tests\20261121-161437-mode-974`.
- 975 actual run: exit code 0, `KYTY_FLOAT_RT_NO_BLEND raw=975 enabled=1 mode=975`; `mode975_adjusted=4096 slot=0 format=97`; `float_rt=32768 blend_enabled=3108 suppressed=0 slot=0 format=97`; mode adjusted both slots 0 and 1. Log folder `C:\Giochi\SilentKyty\Silent-KytyPS5\diagnostics\blend-fix-tests\20261121-161707-mode-975`.
- These runs have different durations, so counters are not directly comparable.
- Strong clue: weighting SOURCE contribution with source alpha improves white/overexposure, whereas weighting destination alone (973) did not. It may be a source alpha generation, factor translation, blend state, or intermediate RT/composition issue. Do not assume standard alpha blending is PS5-correct without verifying original guest state and conversion.
- User's precise description: `entrambi i test mostrano piu' immagini 3d del solito e l'immagine non tende al bianco`.

## Logs and user attachments
- Prior user-provided pasted runtime log (5328 lines) is in an earlier conversation attachment `Testo incollato(20261121-151151).txt`; in a NEW chat this attachment might not be accessible. Ask user to attach archived logs if necessary.
- A prior assistant offered a ZIP `archivio_log_silent_kyty_20261121.zip`; do NOT assume it is present in the Git repository or new chat. Actual 974 and 975 logs live on user's Windows disk at paths above.
- The initial PowerShell pipeline `& '.\kyty_emulator.exe' @argsGame *>&1 | Out-File ...` with `$ErrorActionPreference='Stop'` produced `NativeCommandError` on routine `GuestProtect` stderr. It was a PowerShell 5.1 stream-handling issue, NOT a game crash. Fixed by using `Start-Process -RedirectStandardOutput ... -RedirectStandardError ... -PassThru -Wait`; both modes exited 0.
- Launcher `kyty_launcher.exe` may NOT propagate the test env var; launch `kyty_emulator.exe` directly.
- `u59-preset.json` was NOT applied during direct runs; compare like with like, and do not silently change launch flags.
- The current diagnostic file `kyty-float-blend-diagnostic.txt` is opened with `"w"` at first pipeline creation, so archive/copy it to unique per-run folder before each launch and after completion; avoid reusing same name without archival.

## Paths and build
- Repo: `C:\Giochi\SilentKyty\Silent-KytyPS5`
- Build: `C:\Giochi\SilentKyty\Silent-KytyPS5\_Build\windows`
- Install: `C:\Giochi\SilentKyty\Silent-KytyPS5\_Build\windows\install`
- Emulator: `...\install\kyty_emulator.exe`
- Game: `C:/Giochi/Emulatori/Roms/Ps5/PPSA10112-app/eboot.bin`
- Log archive root: `C:\Giochi\SilentKyty\Silent-KytyPS5\diagnostics\blend-fix-tests`
- Visual Studio env is required for clang-cl build. In PowerShell 5.1:
```powershell
$vsdev = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat'
$vars = & cmd.exe /d /s /c "`"$vsdev`" -arch=amd64 -host_arch=amd64 >nul && set"
foreach ($line in $vars) {
    if ($line -match '^([^=][^=]*)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
$root = 'C:\Giochi\SilentKyty\Silent-KytyPS5'
$build = Join-Path $root '_Build\windows'
Set-Location $root
git status --short
git fetch origin diagnostics/hdr-path-consistency-20261121
git merge --ff-only FETCH_HEAD
cmake --build $build --config Release --target kyty_emulator --parallel 4
```
Check command exit codes; do not overwrite installed exe while processes run; back it up first. Copy build exe into install when build succeeds.
- Direct launch arguments (same for A/B): `--screen-width 1280 --screen-height 720 --user-name Kyty --user-id 1000 --present-mode Mailbox --gpu 0 --fullscreen --vulkan-validation true --shader-validation true --game C:/Giochi/Emulatori/Roms/Ps5/PPSA10112-app/eboot.bin`.
- Set `$env:KYTY_FLOAT_RT_NO_BLEND='974'` or `'975'` before Start-Process, restore `'0'` after; use Start-Process with separate stdout/stderr log files, wait, archive diagnostic, record exit code.

## Next engineering work
1. Preserve the working 974/975 code and all logs; don't revert or replace the successful modes.
2. Compare 974 vs 975 side by side at the same scene/time/settings; request screenshots and full log folders if available.
3. Inspect actual code path for guest PS5 blend enum mapping to Vulkan blend factors, alpha channel outputs, dual-source/alpha remap, MRT slot state, format 97 conversion, and later SDR/HDR presentation. In particular determine whether guest src=1 dst=1 op=0 genuinely denotes additive One/One and whether PS alpha is meaningful. Avoid replacing a legitimate additive pass globally without evidence.
4. Consider diagnostics that capture blend state and source alpha / target luminance on a small bounded sample, rather than disabling blending or dropping draws. Keep changes opt-in and log without truncation.
5. Separately investigate missing 3D draw paths (vertex program/resource binding), because the blend fix may not solve all geometry.
6. When new code is committed, record full commit SHA and append next checkpoint. Never imply files on the user's local disk were uploaded to Git automatically.

## GitHub resume instruction
In a fresh chat, say: 'Apri il file diagnostics/CHECKPOINT_2.md sul ramo diagnostics/hdr-path-consistency-20261121 del repository H3li03/Silent-KytyPS5 e riprendi da lì. Conserva tutte le modifiche locali e i log.' GitHub source file: https://github.com/H3li03/Silent-KytyPS5/blob/diagnostics/hdr-path-consistency-20261121/diagnostics/CHECKPOINT_2.md
