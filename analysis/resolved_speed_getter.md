# Resolved target: `SnakeIO.SnakeController.get_Speed`

Target build: Snake.io 2.2.178, ARM64, IL2CPP metadata v31.

- Metadata token: `0x0600358D`
- MethodDef index: `13708`
- CodeGenModule: `Assembly-CSharp.dll`
- Exact image-relative ELF virtual address in this build: `0x2664D48`
- Native function signature used by the hook: `float get_Speed(SnakeController* self, const MethodInfo* method)`

Disassembly confirms this is the real getter, not just a method name found in metadata. Its body selects 50/100 speed branches and includes a final speed addition before returning in `s0`.

The production hook resolves the MethodInfo at runtime using `il2cpp_domain_get_assemblies`, `il2cpp_assembly_get_image`, `il2cpp_class_from_name`, and `il2cpp_class_get_method_from_name`. It then hooks `MethodInfo.methodPointer` through Dobby. The numeric address above is diagnostic only and is not used as a patch constant.

## Validation still required

- Build the ARM64 shared library with the Android NDK.
- Confirm the IL2CPP method pointer resolves to the expected native function in-process.
- Confirm the hook installs without crashing and the UI multiplier affects only the local player.
- Install and test the resulting split APK set on a device.
