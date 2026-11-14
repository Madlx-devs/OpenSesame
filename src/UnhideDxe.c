// UnhideDxe.c — DXE driver (loaded via DriverOrder) that re-flips the
// Acer/AMI OEM hide flag SetupHideFeatures (offset 0) back to 0x00 after
// POST has re-imposed 0x01, so the hidden Advanced/Chipset form links are
// drawn when the setup browser loads later in the same boot session.
//
// Facts it relies on (all proven on Mannu's A324-51 / DN21CV214):
//  - The Setup driver re-creates SetupHideFeatures = 0x01 every POST.
//  - A direct variable write of 0x00 IS accepted for this variable
//    (setup_var flipped it repeatedly; read-back verified in-session).
//  - The setup browser reads the flag when its forms load, so a write
//    that lands after the Setup driver's POST write and before the
//    browser opens wins for the whole session.
// DriverOrder drivers dispatch in BDS, after all FV DXE drivers
// (including Setup) — that ordering is the whole bet of this driver.
//
// The driver never blocks boot: every path returns EFI_SUCCESS.
// Diagnostics are left in our own variable "UnhideStat" (8 bytes):
//   [0]=GetVariable status (low byte)  [1]=value before
//   [2]=SetVariable status (low byte)  [3]=value after (verify read)
//   [4]=verify Get status (low byte)   [5]=attributes used (low byte)
//   [6..7]=signature 0x55 0xAA
#include <Uefi.h>

static EFI_GUID gSetupHideFeaturesGuid =
  { 0xF4D2228D, 0x2E49, 0x499D, { 0x9B, 0xBE, 0xDF, 0x51, 0xE5, 0x0A, 0xC7, 0x11 } };
static EFI_GUID gUnhideStatGuid =
  { 0x554E4844, 0x4944, 0x4558, { 0x55, 0x4E, 0x48, 0x49, 0x44, 0x45, 0x30, 0x31 } };

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_RUNTIME_SERVICES *RT = SystemTable->RuntimeServices;
  UINT32 Attrs = 0, UseAttrs;
  UINTN  Size;
  UINT8  Before = 0xFF, After = 0xFF, Zero = 0x00;
  EFI_STATUS GetSt, SetSt = EFI_NOT_READY, VerSt = EFI_NOT_READY;
  UINT8  Rec[8];

  Size = 1;
  GetSt = RT->GetVariable(L"SetupHideFeatures", &gSetupHideFeaturesGuid,
                          &Attrs, &Size, &Before);
  if (GetSt == EFI_NOT_FOUND || Attrs == 0) {
    UseAttrs = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
               EFI_VARIABLE_RUNTIME_ACCESS;
  } else {
    UseAttrs = Attrs;  // preserve the variable's own attributes exactly
  }

  if (GetSt == EFI_SUCCESS && Before == 0x00) {
    SetSt = EFI_SUCCESS;  // already unhidden; nothing to do
  } else {
    SetSt = RT->SetVariable(L"SetupHideFeatures", &gSetupHideFeaturesGuid,
                            UseAttrs, 1, &Zero);
  }

  Size = 1;
  VerSt = RT->GetVariable(L"SetupHideFeatures", &gSetupHideFeaturesGuid,
                          NULL, &Size, &After);

  Rec[0] = (UINT8)GetSt;  Rec[1] = Before;
  Rec[2] = (UINT8)SetSt;  Rec[3] = After;
  Rec[4] = (UINT8)VerSt;  Rec[5] = (UINT8)UseAttrs;
  Rec[6] = 0x55;          Rec[7] = 0xAA;
  RT->SetVariable(L"UnhideStat", &gUnhideStatGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS, 8, Rec);

  (void)ImageHandle;
  return EFI_SUCCESS;
}
