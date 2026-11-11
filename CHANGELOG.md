# CatEngine Changelog

All notable changes to the CatEngine project are documented in this file.

---

## [Unreleased] - `feature/catengine-complete-implementation` - 2026-11-11

### Repository Reorganization & Toolchain Harmonization (Phase 0 & 1)
- **Repository Architecture Cleanup:**
  - Migrated extensive root analysis and audit documentation into dedicated subdirectories under `docs/` (`docs/architecture/`, `docs/audit/`, `docs/research/`, `docs/roadmap/`, `docs/developer/`, `docs/legacy/`).
  - Relocated maintenance scripts from `Bcore/scratch/` to `tools/scripts/maintenance/`.
  - Moved temporary APK analysis folders (`extracted_carrom/`, `scratch_se/`) to `tools/analysis/`.
  - Removed corrupted download placeholder `jadx-1.5.6.zip`.
  - Added comprehensive `.gitignore` preventing build outputs, temporary test artifacts, and copyrighted split APKs from being committed.
  - Authored modernized root `README.md`, `DEVELOPER_GUIDE.md`, and `BUILD_AND_INSTALLATION.md`.
- **Build System Normalization:**
  - Created root `settings.gradle` declaring Google Maven, JitPack, and Gradle Plugin Portal.
  - Normalized Android NDK to locally installed stable NDK `28.2.13676358`.
  - Enforced Android 15 16KB page-size alignment (`-Wl,-z,max-page-size=16384`) across all native compilation targets (`arm64-v8a` and `armeabi-v7a`).
  - Verified full clean build producing `CatEngine_1.0.0_debug.apk` (22.48 MB).

### Reverse Engineering & Device Verification (Phase 2)
- **Live Device APK Extraction:**
  - Extracted split APKs (`base.apk`, `split_config.arm64_v8a.apk`, `split_gpdeku.apk`, `split_gpdeku.config.arm64_v8a.apk`) from connected test device (`10ME8DF1CT0001Z`, Vivo V2356, Android 15/API 35).
  - Package confirmed as `com.miniclip.carrom` version 19.6.0 (versionCode 1491), compiling against Android 15 (SDK 35) and targeting Android 16 (SDK 36).
- **Native Binary & Hook Verification:**
  - Extracted live game library `libgame-CARROM-GooglePlay-Gold-Release-Module-1491.so` (SHA-256: `4CF2BA474FC9AEFDA142FBDCA46CF7B4D3EBF06A2114ECD8487A3E48448710B4`).
  - Verified function prologues and float coordinate signatures for all CatEngine native hooks in `catEngine.cpp` using `llvm-objdump`.

