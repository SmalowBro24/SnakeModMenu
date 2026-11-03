# Reverse-engineering notes — Snake.io 2.2.178

## Package integrity and layout

The outer uploaded archive contains four APKs plus bundle metadata and an icon:

- `base.apk` — application code, Unity assets, and `assets/bin/Data/Managed/Metadata/global-metadata.dat`.
- `split_config.arm64_v8a.apk` — ARM64 libraries, including `lib/arm64-v8a/libil2cpp.so` and `libunity.so`.
- `split_gpdeku.apk` — feature split.
- `split_gpdeku.config.arm64_v8a.apk` — ARM64 feature/config split.

The `base.apk` and ARM64 config split ZIP CRC checks passed. The package metadata identifies `com.amelosinteractive.snake`, version name `2.2.178`, version code `1750`.

## Runtime architecture

- Unity IL2CPP, metadata format version 31.
- Native target: ELF64, AArch64 (`arm64-v8a`).
- The metadata uses 36-byte method-definition records and 0x58-byte type-definition records.
- Relevant image assemblies include `Assembly-CSharp.dll` and `SnakeIO.Common.dll`.

## Candidate runtime targets

### `SnakeIO.SnakeController`

Fields include `NORMAL_SPEED`, `BURST_SPEED`, `SnakeControllerConfig`, `_SpeedBurstActive`, `_enterPortalSpeed`, and related movement state.

Methods identified from metadata:

- `get_Speed()` — token `0x0600358D`
- `get_SpeedScaler()` — token `0x06003591`
- `OnUpdate(float deltaTime)` — token `0x060035DA`
- `ActivateSpeedBurst(bool active)` — token `0x060035E5`
- `SetupDirection(float deltaTime)` — token `0x060035DC`

Default metadata values:

- `NORMAL_SPEED`: `50.0f`
- `BURST_SPEED`: `100.0f`

### `SnakeBoostController`

Fields include `_speedSnakeBoost`, `_speedBoostStarted`, `_speedBoostStartTime`, `_roamingSpeedBoostStarted`, and `_roamingSpeedBoostStartTime`.

Methods identified from metadata:

- `get_SpeedBoostEnabled()` — token `0x060006F1`
- `get_SpeedBoostProgress()` — token `0x060006F7`
- `ActivateSpeedBoost(boostOptions)` — token `0x06000702`
- `DeactivateSpeedBoost(updateStatus)` — token `0x0600070A`
- `DeactivateAllBoosts()` — token `0x06000708`

### `SnakeIO.Common.Prediction.SnakeStatePredictor`

- `get_LinearSpeed()` — token `0x06000153`
- `get_SpeedMultiplier()` — token `0x0600015D`
- `set_SpeedMultiplier(float)` — token `0x0600015E`
- `ChangeSpeedMultiplier(float)` — token `0x06000177`

This class may affect prediction/physics integration; it should be checked alongside `SnakeController`, not blindly patched.

## Native API availability

The ARM64 `libil2cpp.so` dynamic symbol table exports IL2CPP entry points including:

- `il2cpp_domain_get`
- `il2cpp_domain_get_assemblies`
- `il2cpp_assembly_get_image`
- `il2cpp_image_get_name`
- `il2cpp_class_from_name`
- `il2cpp_class_get_method_from_name`
- `il2cpp_thread_attach`

That makes runtime type/method resolution a plausible route. It does not yet identify a safe hook target or prove that a hook works. The next engineering step is to resolve the runtime `MethodInfo` for `SnakeIO.SnakeController.get_Speed`, inspect the method pointer/prologue, and test a reversible hook before integrating a menu.

## Hashes for reproducibility

- Uploaded bundle SHA-256: `70f52affbb717f37e3abecef856a23ac4fcc19c24e828c080c8cd844e4048b94`
- `global-metadata.dat` SHA-256: `09f89f2016acfb9d44a35ddd99b1431e84a07c1b1399e6f05ff544086d17b35b`
- `libil2cpp.so` SHA-256: `e3e2b8bf7f407b00d496d83eee3199f9de48b42d883a2e675d1b40a7176cbf49`

## Native speed getter resolution

The `Assembly-CSharp.dll` CodeGenModule entry and its method pointer table were matched through the ELF relocation records. For this exact uploaded ARM64 build, the `SnakeIO.SnakeController.get_Speed` pointer resolves to ELF virtual address `0x2664D48` (ASLR-independent image-relative address before runtime relocation). Disassembly at that address confirms the getter returns baseline values of `50.0f` and `100.0f` on the normal/burst branches and applies an additional runtime speed term before returning.

The implementation intentionally does **not** hardcode `0x2664D48`: it resolves `SnakeController.get_Speed` through the runtime IL2CPP API on launch, then hooks the returned method pointer. This avoids assuming the same function address for another app build. The hook's receiver check is limited to `SnakeIO.PlayerSnakeController`.
