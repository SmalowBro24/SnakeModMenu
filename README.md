# SnakeModMenu — Snake.io 2.2.178 (ARM64)

This repository contains a first implementation of an **offline speed-control overlay** for the supplied Android split package. It targets the exact uploaded build, not arbitrary versions.

## Target package inspected

- Package: `com.amelosinteractive.snake`
- App version: `2.2.178`, version code `1750`
- Engine: Unity IL2CPP
- ABI: ARM64 (`arm64-v8a`)
- IL2CPP metadata: version `31`
- Original uploaded bundle SHA-256: `70f52affbb717f37e3abecef856a23ac4fcc19c24e828c080c8cd844e4048b94`

The uploaded file is a split-package archive containing `base.apk`, `split_config.arm64_v8a.apk`, `split_gpdeku.apk`, and `split_gpdeku.config.arm64_v8a.apk`. `global-metadata.dat` is in the base split; `libil2cpp.so` and `libmain.so` are in the ARM64 split.

## Implementation in this project

- `native/mod.cpp` resolves IL2CPP exports at runtime, locates `SnakeIO.SnakeController.get_Speed`, and hooks its method pointer using Dobby.
- The hook applies a configurable multiplier only when the runtime object is a `SnakeIO.PlayerSnakeController`, leaving other snakes untouched.
- `java/com/snakemod/` contains a small floating control panel (1.0x–5.0x) and an Android overlay permission flow.
- `scripts/patch_manifest.py` adds the overlay permission, service, and permission activity to the decoded base manifest.
- `scripts/patch_native_needed.py` makes Unity's `libmain.so` load `libsnakemod.so` from the same ARM64 split.
- `.github/workflows/build.yml` builds and packages the four APK splits, signs them with one temporary build key, and uploads an artifact.

## Confirmed speed-related metadata

The metadata inspection identified:

| Class | Method / field | Token | Meaning |
|---|---|---:|---|
| `SnakeIO.SnakeController` | `get_Speed()` | `0x0600358D` | Main speed getter |
| `SnakeIO.SnakeController` | `get_SpeedScaler()` | `0x06003591` | Additional speed scale |
| `SnakeIO.SnakeController` | `ActivateSpeedBurst(bool)` | `0x060035E5` | Built-in burst toggle |
| `SnakeIO.PlayerSnakeController` | `ActivateSpeedBurst(bool)` | `0x0600353C` | Player-specific burst toggle |
| `SnakeIO.Common.Prediction.SnakeStatePredictor`2` | `get_SpeedMultiplier()` | `0x0600015D` | Prediction speed multiplier |
| `SnakeIO.Common.Prediction.SnakeStatePredictor`2` | `ChangeSpeedMultiplier(float)` | `0x06000177` | Prediction multiplier setter |

`SnakeController.NORMAL_SPEED` and `BURST_SPEED` have metadata defaults of `50.0f` and `100.0f`. The implementation does not patch these constants; it intercepts the runtime speed getter so the slider can change the speed live.

## Build input required by GitHub Actions

GitHub blocks ordinary repository files larger than 100 MiB, so the original ~138 MiB split archive is not committed to Git. Upload the supplied archive renamed with a `.zip` extension as an asset on a GitHub Release with the tag **`source-apk`**. The archive is already a ZIP; only its filename extension changes. The workflow downloads the `.zip` asset and renames it internally to `original.apk+`.

Then run **Actions → Build SnakeModMenu APK set → Run workflow**. The artifact is `SnakeModMenu-apk-set` and contains all four re-signed APK splits in one `.apk+` archive.

## Important status

The project source files and the APK packaging workflow are now committed to the `main` branch. **The final APK has not yet been built or tested on a device.** Before running the build, upload the split-package archive renamed to `.zip` as an asset on a GitHub Release tagged `source-apk`. Publishing that release automatically triggers the workflow.

The workflow generates a temporary signing key on each run. Android treats the modded package as a different signature from the store-installed version, so uninstall the original before installing all four modded splits. A later build uses a new temporary key too; uninstall the previous modded build before installing that one.

## Metadata inspection tool

Run:

```bash
python tools/inspect_il2cpp.py "/path/to/Snake.io bundle.apk+"
```

This parser supports the inspected IL2CPP metadata version 31 and reports target methods/fields without modifying the APK.
