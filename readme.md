# re4fix — Resident Evil 4 VR to flat Android

**re4fix** is an experimental compatibility and input layer that adapts the Meta Quest version of **Resident Evil 4** for flat-screen play on ARM64 Android devices. It provides a native `libre4fix.so` helper, touch controls, a Vulkan overlay, Android integration, and supporting build and research tools.

Project: [phys-winner/vr-flat-re4](https://github.com/phys-winner/vr-flat-re4).

Original game source: [Resident Evil 4 on the Meta Store](https://www.meta.com/en-gb/experiences/resident-evil-4/2637179839719680/). Obtain the game separately from its official source. This repository contains the adaptation's source code; game APKs, OBBs, engine binaries, extracted game assets, and saved games are excluded.

## Features

- Flat camera and Vulkan presentation adapted from the original VR rendering path.
- Touch movement and camera control, aiming, firing, weapon selection, reloading, and melee interactions.
- Contextual controls for inventory, pickups, Ashley commands, phone calls, maps, typewriters, and quick-time events.
- Gamepad support and optional gyroscope aiming.
- Mobile settings, startup/loading UI, cache selection, and save import/export through Android's document picker.
- Mobile control text for English, Russian, German, Latin American Spanish, French, and Japanese.
- Native and Java host fixtures, source checks, and tools for inspecting the original game's packages and native behavior.

## Status and compatibility

This is a source publication of an existing experimental port, not a standalone replacement game or a turnkey APK build. The native hooks, memory layouts, patch preimages, and input hashes target a specific Quest game revision: **version 2.0.1, build 201**. A different APK, engine binary, translation baseline, or asset revision requires verification and likely new offsets or preimages.

The native helper targets `arm64-v8a` with Android API 23 and a 16 KiB maximum ELF page size. The Android build scripts use a minimum SDK of 23 and a target SDK of 37. These build settings do not establish compatibility with every device: Vulkan drivers, Android lifecycle behavior, and individual gameplay interactions still require hardware testing.

## Source layout

| Path | Purpose |
| --- | --- |
| `analysis/re4fix.c` | Native entry point, engine hooks, input state, and integration |
| `analysis/re4fix_*.h` | Gameplay, camera, UI, rendering, and Android adapters |
| `analysis/re4fix_overlay.c` | Vulkan overlay renderer |
| `analysis/overlay/` | Overlay shaders, atlas generator, and generated project-owned C assets |
| `analysis/android/com/epicgames/ue4/` | Custom Java settings, cache, file-picker, loading, and save helpers |
| `analysis/mobile_locres.py`, `analysis/mobile_text_*.json` | Mobile localization replacements |
| `analysis/build_mobile_*.py` | Android-layer, startup, and embedded localization builders |
| `analysis/pak9.py`, `analysis/pak_writer.py`, `analysis/repack.py` | Package readers, writers, and APK replacement utility |
| `analysis/research/`, `analysis/test_touch.c` | Research tools, validation scripts, and host fixtures |

The original working-directory layout is retained so that relative paths used by the tools remain meaningful.

## Build prerequisites

The supplied build commands are for **Windows PowerShell**. Existing native host checks also use Windows compiler and SDK paths.

- Python **3.11 or later**.
- A JDK with `java` and `javac` on `PATH`; the existing tooling was used with JDK 24.
- Android SDK Platform **36**, Build Tools **36.0.0**, and NDK **27.2.12479018**.
- `apktool.jar` and `apksigner.jar` for Android packaging.
- Local APK signing material when producing a signed APK. The existing packaging scripts expect `testkey.pk8` and `testkey.x509.pem` beside those JARs. Supply your own development key and certificate; signing material is excluded from Git.
- Original game files and the intermediate inputs described below.

Set tool locations for your own machine:

```powershell
$env:ANDROID_SDK_ROOT = 'C:\Android\Sdk'
$env:JAVA_HOME = 'C:\Java\jdk-24'
$env:RE4_TOOLS_DIR = 'C:\Android\re4-tools'
$env:Path = "$env:JAVA_HOME\bin;$env:ANDROID_SDK_ROOT\platform-tools;$env:Path"
```

`ANDROID_SDK_ROOT` is used by native, Android, and SDK-dependent checks. `JAVA_HOME` is used by checks that invoke JDK executables directly. `RE4_TOOLS_DIR` replaces the original workstation's APK-tool directory.

The legacy ADB helper scripts require an explicitly selected device through `ANDROID_SERIAL` or `DEV`. Device serial numbers are not stored in the source. Those scripts retain historical package names and should be reviewed against the installed package before use. `finalize_mobile_release.py` is a historical QA tool with save-restoration operations; its device mode requires `ANDROID_SERIAL` and its exact local baseline files.

### Local game inputs

Several scripts consume intermediate files from the original restoration workspace. Those files are not distributed here:

- `apk_decoded/`: the canonical, already patched APKTool Android tree. `build_mobile_android.py` also reads the historical base APK `re4_p171i.apk`. A fresh decode of the store APK is not equivalent to that patched tree.
- `analysis/research/VR4_patch_en_original.locres`: the Russian fan-translation baseline whose SHA-256 is pinned in `mobile_locres.py`. This is a separate translation input, not an asset supplied by this repository.
- `analysis/research/localization_originals/<locale>_VR4.locres`: original localization resources for `en`, `de-DE`, `es-419`, `fr-FR`, and `ja-JP`.
- `analysis/research/graphics-evidence/VR4/Config/DefaultEngine.ini`: the original engine configuration used by the localization/startup builder.
- `analysis/research/splash_originals/`: original startup map/widget packages, their package and script metadata, and `receipt.json` as consumed by `build_mobile_startup.py`.
- Additional extracted native/package evidence and local QA captures for the historical research checks. Their required paths and hashes are recorded in the individual scripts.

These builders deliberately verify exact hashes and structures. Preserve those checks when preparing local inputs. This snapshot does not include an automated process to reconstruct all historical intermediate inputs from a store download.

### Native helper

From the repository root, first generate the embedded localization/startup package after preparing the required local inputs:

```powershell
python -B analysis/build_mobile_text.py
```

This creates `analysis/mobile_text_pak.h` and a local patch PAK. The header embeds game-derived localization and startup data, so it is excluded from Git along with the PAK. It is required by the current native source.

Then compile the helper:

```powershell
New-Item -ItemType Directory -Force analysis/build | Out-Null
$clang = Join-Path $env:ANDROID_SDK_ROOT 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
& $clang --target=aarch64-linux-android23 -O2 -fPIC -shared -fno-stack-protector `
    '-Wl,--no-undefined' '-Wl,-z,max-page-size=16384' `
    -o analysis/build/libre4fix.so analysis/re4fix.c analysis/re4fix_overlay.c `
    -llog -ldl -lm
if ($LASTEXITCODE -ne 0) { throw 'Native helper build failed' }
```

This builds the helper library only. To rebuild the Android layer and package a signed checkpoint, the historical tooling additionally needs the canonical patched Android tree, base APK, and signing material:

```powershell
python -B analysis/build_mobile_android.py
python -B analysis/research/build_native_snapshot.py p401 analysis/build/mobile-android-base.apk
```

`p401` is an example of a new, unused local checkpoint tag, not a published release. The checkpoint builder refuses to overwrite an existing snapshot or checkpoint APK.

`build_obb_splits.py` is an optional legacy whole-OBB packaging tool. Its input location, exact OBB hashes, and signing-certificate assertion are pinned to the original local setup; review them before using different files or signing material.

To regenerate the project-owned overlay headers, compile `overlay.vert` and `overlay.frag` to their corresponding `.spv` files with a Vulkan GLSL compiler, then run `make_atlas.py` from `analysis/overlay/`. The atlas generator requires Pillow. Generated overlay headers are included, so this step is unnecessary for an unchanged native build.

## Validation

The following checks exercise production Java code against host Android doubles without contacting a device:

```powershell
python -B analysis/research/check_mobile_gyro.py
python -B analysis/research/check_mobile_loading.py
```

The save-restoration test uses a fake device and does not call ADB:

```powershell
python -B analysis/research/test_finalize_mobile_release.py
```

Other `check_*.py` scripts may require the omitted game binaries, extracted metadata, historical APK checkpoints, or local captures. Native host fixtures additionally use the MSVC and Windows SDK versions named in those scripts. Host checks do not replace hardware acceptance testing.

## License and credits

Copyright (C) 2026 phys-winner and re4fix contributors.

The original re4fix code in this repository is licensed under the **GNU General Public License, version 3 only** (`GPL-3.0-only`). See [LICENSE](LICENSE) for the full terms. The project is provided without warranty.

Resident Evil 4, its game content, the original engine and platform components, and third-party translations retain their respective owners' rights and licenses. The re4fix license does not grant rights to those materials. This is an independent project and is not affiliated with or endorsed by Capcom, Armature, or Meta.
