#!/usr/bin/env python3
"""Build and package GalaxyAndroid.apk for ARM64 Android devices."""
import glob
import os
import shutil
import subprocess
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))

# 1. Resolve Android SDK
SDK = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or os.path.expandvars(r"%LOCALAPPDATA%\Android\Sdk")

# 2. Resolve Build Tools (pick highest version)
bt_parent = os.path.join(SDK, "build-tools") if SDK else ""
BUILD_TOOLS_DIR = ""
if os.path.isdir(bt_parent):
    try:
        versions = sorted([v for v in os.listdir(bt_parent) if os.path.isdir(os.path.join(bt_parent, v))],
                          key=lambda s: [int(x) if x.isdigit() else x for x in s.split('.')])
        if versions:
            BUILD_TOOLS_DIR = os.path.join(bt_parent, versions[-1])
    except Exception:
        pass

AAPT2 = os.path.join(BUILD_TOOLS_DIR, "aapt2.exe") if os.name == "nt" else os.path.join(BUILD_TOOLS_DIR, "aapt2")
ZIPALIGN = os.path.join(BUILD_TOOLS_DIR, "zipalign.exe") if os.name == "nt" else os.path.join(BUILD_TOOLS_DIR, "zipalign")
APKSIGNER = os.path.join(BUILD_TOOLS_DIR, "apksigner.bat") if os.name == "nt" else os.path.join(BUILD_TOOLS_DIR, "apksigner")

# 3. Resolve android.jar (pick highest available API platform)
ANDROID_JAR = ""
platforms_dir = os.path.join(SDK, "platforms") if SDK else ""
if os.path.isdir(platforms_dir):
    candidates = sorted([p for p in os.listdir(platforms_dir) if os.path.exists(os.path.join(platforms_dir, p, "android.jar"))], reverse=True)
    if candidates:
        ANDROID_JAR = os.path.join(platforms_dir, candidates[0], "android.jar")

# 4. Resolve NDK
NDK_DIR = os.environ.get("ANDROID_NDK_HOME") or ""
if not NDK_DIR and SDK:
    ndk_parent = os.path.join(SDK, "ndk")
    if os.path.isdir(ndk_parent):
        ndk_candidates = sorted([v for v in os.listdir(ndk_parent) if os.path.isdir(os.path.join(ndk_parent, v))], reverse=True)
        if ndk_candidates:
            NDK_DIR = os.path.join(ndk_parent, ndk_candidates[0])

STRIP = ""
if NDK_DIR:
    strip_pattern = os.path.join(NDK_DIR, "toolchains", "llvm", "prebuilt", "*", "bin", "llvm-strip" + (".exe" if os.name == "nt" else ""))
    matches = glob.glob(strip_pattern)
    if matches:
        STRIP = matches[0]

BUILD = os.path.join(ROOT, "build-mobile")
OUT = os.path.join(ROOT, "out-mobile")
KEYSTORE = os.path.join(ROOT, "out", "debug.keystore")

def run(cmd):
    print("[RUN]", " ".join(cmd))
    subprocess.run(cmd, check=True)

def main():
    print("=== Packaging GalaxyAndroid.apk (com.openport.galaxyandroid) ===")
    print(f"  SDK: {SDK}")
    print(f"  Build Tools: {BUILD_TOOLS_DIR}")
    print(f"  Platform JAR: {ANDROID_JAR}")
    print(f"  NDK: {NDK_DIR}")
    print(f"  Strip: {STRIP}")

    os.makedirs(OUT, exist_ok=True)
    os.makedirs(os.path.join(ROOT, "out"), exist_ok=True)
    apk_dir = os.path.join(OUT, "apk")
    shutil.rmtree(apk_dir, ignore_errors=True)
    arm64_dir = os.path.join(apk_dir, "lib", "arm64-v8a")
    os.makedirs(arm64_dir, exist_ok=True)

    # 0. Ensure debug keystore exists
    if not os.path.exists(KEYSTORE):
        print("[KEYSTORE] Generating debug keystore...")
        keytool = shutil.which("keytool") or "keytool"
        subprocess.run([
            keytool, "-genkeypair", "-keystore", KEYSTORE,
            "-storepass", "android", "-keypass", "android",
            "-alias", "androiddebugkey", "-keyalg", "RSA", "-keysize", "2048",
            "-validity", "10000", "-dname", "CN=GalaxyAndroid Debug,O=Local,C=US"
        ], check=True)

    # 1. Compile resources with AAPT2
    res_zip = os.path.join(OUT, "res.zip")
    if os.path.exists(res_zip):
        os.remove(res_zip)
    run([AAPT2, "compile", "--dir", os.path.join(ROOT, "platform", "android", "res"), "-o", res_zip])

    base_apk = os.path.join(OUT, "base.apk")
    if os.path.exists(base_apk):
        os.remove(base_apk)
    run([
        AAPT2, "link",
        "-o", base_apk,
        "--manifest", os.path.join(ROOT, "platform", "mobile", "AndroidManifest.xml"),
        "-R", res_zip,
        "--auto-add-overlay",
        "-I", ANDROID_JAR,
        "--min-sdk-version", "30",
        "--target-sdk-version", "34"
    ])

    # 2. Copy and strip wrap.sh and native libraries
    wrap_src = os.path.join(ROOT, "platform", "android", "wrap.sh")
    with open(wrap_src, "rb") as f_in, open(os.path.join(arm64_dir, "wrap.sh"), "wb") as f_out:
        f_out.write(f_in.read().replace(b"\r\n", b"\n"))

    for lib in ["libgame.so", "libmain.so", "libreserve.so"]:
        src_lib = os.path.join(BUILD, lib)
        dst_lib = os.path.join(arm64_dir, lib)
        if os.path.exists(src_lib):
            if STRIP and os.path.exists(STRIP):
                run([STRIP, "--strip-unneeded", "-o", dst_lib, src_lib])
            else:
                shutil.copyfile(src_lib, dst_lib)
        else:
            print(f"[WARN] Native library {src_lib} not found in build directory.")

    # 3. Add lib folder into base.apk (store uncompressed so zipalign can page-align native libraries)
    with zipfile.ZipFile(base_apk, "a") as z:
        for root, _, files in os.walk(os.path.join(apk_dir, "lib")):
            for f in files:
                p = os.path.join(root, f)
                arc = os.path.relpath(p, apk_dir).replace("\\", "/")
                compress_type = zipfile.ZIP_STORED if f.endswith(".so") else zipfile.ZIP_DEFLATED
                z.write(p, arc, compress_type=compress_type)

    # 4. Zipalign (16-byte alignment required for modern 16KB/4KB page size compatibility)
    aligned_apk = os.path.join(OUT, "aligned.apk")
    if os.path.exists(aligned_apk):
        os.remove(aligned_apk)
    run([ZIPALIGN, "-f", "-p", "16", base_apk, aligned_apk])

    # 5. Apksigner (v1 + v2 + v3 signing)
    final_apk = os.path.join(OUT, "GalaxyAndroid.apk")
    if os.path.exists(final_apk):
        os.remove(final_apk)
    run([
        APKSIGNER, "sign",
        "--ks", KEYSTORE,
        "--ks-pass", "pass:android",
        "--key-pass", "pass:android",
        "--out", final_apk,
        aligned_apk
    ])

    # Cleanup temp files
    for temp in [base_apk, aligned_apk, res_zip]:
        if os.path.exists(temp):
            os.remove(temp)
    shutil.rmtree(apk_dir, ignore_errors=True)

    print(f"\n[SUCCESS] Built unified package: {final_apk} ({os.path.getsize(final_apk)} bytes)")

if __name__ == "__main__":
    main()
