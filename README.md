# OpenSesame

Unlocks hidden advanced settings on AMI Aptio BIOS by flipping the `SetupHideFeatures` flag.

Tested on **Acer Aspire 3 A324-51 (BIOS DN21CV214)**. Successfully exposed advanced menus and allowed Hyper-Threading and VT-d settings to be changed and saved without flashing the BIOS.

## Files
- `UnhideDxe.efi` — unlocks menus on every boot.
- `UnhideApp.efi` — unlocks menus for the current session only.

## Quick Start

Requires a UEFI Shell and a FAT32 USB drive.

1. Check whether the variable exists:
   ```shell
   dmpstore SetupHideFeatures
   ```
2. Find your USB filesystem:
   ```shell
   map -r
   ```
3. Install the driver (replace `FS0:` if needed):
   ```shell
   bcfg driver add 0 FS0:\UnhideDxe.efi "OpenSesame"
   ```
4. Reboot and press **F2** to access advanced menus.

## Verify first (recommended)

Don't install the driver blind. Two well-known tools — **setup_var** (reads/writes single UEFI variable bytes from a shell) and **UMAF** (a setup browser that shows the full menu tree) — let you prove the flag controls your board first:

1. **Know your offsets.** Get them from your own BIOS (IFR extraction of your firmware image) — never copy another board's blindly. On the tested board (DN21CV214):
   - `SetupHideFeatures` (GUID `F4D2228D-2E49-499D-9BBE-DF51E50AC711`): the hide flag is the byte at offset **0x0** — `0x01` = menus hidden, `0x00` = menus shown.
   - `CpuSetup`: Hyper-Threading at **0x5**, CFG Lock at **0x43**.
   - `SaSetup`: VT-d at **0x7D**, DVMT Pre-Allocated at **0x7A**.
   Read your current bytes with setup_var before changing anything.
2. **Flip the flag once** with setup_var: set the `SetupHideFeatures` byte at offset 0x0 to `0x00`. This is a one-time test — nothing is installed.
3. **Verify with UMAF:** boot UMAF and open the setup browser. If the advanced menus (CPU Configuration, Chipset, …) now appear, the flag gates your board and the flip works.
4. **Only then proceed with UnhideDxe** (Quick Start, step 3) to make the unlock happen on every boot.

Note: on some firmware (including the tested board) setup_var can *read* the CpuSetup/SaSetup stores but writes to them are refused — that's fine. The settings themselves are changed in the F2 menus once unlocked, and saved by the BIOS.

**Warning:** Tested only on the board listed above. Incorrect BIOS settings may prevent booting. Know how to reset CMOS before proceeding.

**Download:** [OpenSesame v1.0](https://github.com/Madlx-devs/OpenSesame/releases)
