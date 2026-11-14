#!/bin/sh
# Build UnhideDxe.efi and UnhideApp.efi with an LLVM toolchain + EDK II /
# Mu BaseCore headers. Set LLVM_BIN to the directory holding clang and
# lld-link, and MU to a tree containing MdePkg and MdeModulePkg.
set -e
cd "$(dirname "$0")"
LLVM_BIN="${LLVM_BIN:-$HOME/workspace/tools/bin}"
MU="${MU:-$HOME/workspace/hackios/Mu-Silicium/Mu_Basecore}"
OUT="${OUT:-.}"
mkdir -p "$OUT/build"
CFLAGS="-ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -O1 -Wall -Wextra"
INCLUDES="-I$MU/MdePkg/Include -I$MU/MdePkg/Include/X64 -I$MU/MdeModulePkg/Include"

# --- UnhideDxe.efi (DXE driver, loaded via DriverOrder) ---
"$LLVM_BIN/clang" -target x86_64-pc-windows-msvc $CFLAGS $INCLUDES \
  -c src/UnhideDxe.c -o "$OUT/build/UnhideDxe.obj"
"$LLVM_BIN/lld-link" /subsystem:efi_boot_service_driver /entry:EfiMain /machine:x64 \
  /base:0 /dynamicbase /fixed:no /out:"$OUT/UnhideDxe.efi" "$OUT/build/UnhideDxe.obj"
sha256sum "$OUT/UnhideDxe.efi"

# --- UnhideApp.efi (fallback picker tool, UEFI application) ---
"$LLVM_BIN/clang" -target x86_64-pc-windows-msvc $CFLAGS $INCLUDES \
  -c src/UnhideApp.c -o "$OUT/build/UnhideApp.obj"
"$LLVM_BIN/lld-link" /subsystem:efi_application /entry:EfiMain /machine:x64 \
  /base:0 /dynamicbase /fixed:no /out:"$OUT/UnhideApp.efi" "$OUT/build/UnhideApp.obj"
sha256sum "$OUT/UnhideApp.efi"

