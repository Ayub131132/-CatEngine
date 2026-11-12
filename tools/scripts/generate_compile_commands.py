import os
import json

ws = r"C:\Users\Tanmoy\OneDrive\Documents\-CatEngine"
bcore_cpp = os.path.join(ws, "Bcore", "src", "main", "cpp")
ndk_clangpp = r"C:/AndroidSDK/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe"
ndk_clang = r"C:/AndroidSDK/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe"
sysroot = r"C:/AndroidSDK/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/sysroot"

commands = []

for root, dirs, files in os.walk(bcore_cpp):
    for f in files:
        if f.endswith(".cpp") or f.endswith(".c"):
            file_path = os.path.join(root, f).replace("\\", "/")
            is_cpp = f.endswith(".cpp")
            compiler = ndk_clangpp if is_cpp else ndk_clang
            std_flag = "-std=c++20" if is_cpp else "-std=c17"
            
            args = [
                compiler,
                "--target=aarch64-linux-android24",
                f"--sysroot={sysroot}",
                "-isystem",
                f"{sysroot}/usr/include/c++/v1",
                "-isystem",
                f"{sysroot}/usr/include",
                "-isystem",
                f"{sysroot}/usr/include/aarch64-linux-android",
                std_flag,
                f"-I{bcore_cpp.replace('\\', '/')}",
                "-DANDROID",
                "-D__ANDROID__",
                "-D__aarch64__",
                "-D__ANDROID_API__=24",
                "-c",
                file_path
            ]
            
            commands.append({
                "directory": ws.replace("\\", "/"),
                "arguments": args,
                "file": file_path
            })

out_path = os.path.join(ws, "compile_commands.json")
with open(out_path, "w", encoding="utf-8") as fp:
    json.dump(commands, fp, indent=2)

print(f"Generated {out_path} with {len(commands)} entries.")
