# Validation log

## Completed locally

- Inspected the uploaded split archive and verified the base and ARM64 split ZIP CRCs.
- Parsed the IL2CPP v31 metadata using `tools/inspect_il2cpp.py`.
- Confirmed the target `SnakeController.get_Speed` method token and resolved its implementation in the ARM64 ELF relocation/codegen tables.
- Disassembled the resolved native function at ELF virtual address `0x2664D48` and confirmed the 50/100 speed branches.
- Ran Python bytecode compilation for the metadata and APK packaging scripts.
- Ran the manifest patcher against a test manifest and verified that it inserts the required permission/activity/service entries.
- Ran a C++ syntax-only compile of `native/mod.cpp` using Clang and the JDK JNI headers plus stub declarations for Android logging and Dobby.

## Not yet validated

- Full Android NDK CMake build of `libsnakemod.so`.
- Runtime IL2CPP method pointer lookup and Dobby hook installation on the actual device.
- Java overlay launch and Android overlay permission flow on the actual game/device.
- Apktool rebuild, signing, split installation and in-game speed behavior.

No installable modified APK is produced by the source package alone. The GitHub Actions workflow is intended to perform the full packaging step once the project files are committed and the original split archive is uploaded as a Release asset tagged `source-apk`.
