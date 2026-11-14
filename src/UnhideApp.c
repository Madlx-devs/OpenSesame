// UnhideApp.c — fallback picker tool (UEFI application).
// One pick does Mannu's proven same-session recipe automatically:
//   1. flip SetupHideFeatures (offset 0) to 0x00 via the variable
//      service, preserving the variable's own attributes, and
//      verify by reading it back;
//   2. search every filesystem for Loader.efi (UMAF) and launch it
//      in the SAME session, so the browser draws the full hidden tree.
// If no Loader.efi is found, it says so and exits to the picker —
// the flag stays 0 for the rest of the session regardless.
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePath.h>

static EFI_GUID gSetupHideFeaturesGuid =
  { 0xF4D2228D, 0x2E49, 0x499D, { 0x9B, 0xBE, 0xDF, 0x51, 0xE5, 0x0A, 0xC7, 0x11 } };
static EFI_GUID gSfsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID gDpGuid  = EFI_DEVICE_PATH_PROTOCOL_GUID;

static EFI_SYSTEM_TABLE *gST;
static EFI_BOOT_SERVICES *gBS;

static void Print(CHAR16 *S) { gST->ConOut->OutputString(gST->ConOut, S); }

static void PrintHex8(UINT8 V)
{
  CHAR16 Buf[6];
  CHAR16 Hex[] = L"0123456789ABCDEF";
  Buf[0] = L'0'; Buf[1] = L'x';
  Buf[2] = Hex[(V >> 4) & 0xF]; Buf[3] = Hex[V & 0xF];
  Buf[4] = L' '; Buf[5] = L'\0';
  Print(Buf);
}

static UINTN StrLen16(CHAR16 *S) { UINTN N = 0; while (S[N]) N++; return N; }

static void CopyMem8(void *Dst, void *Src, UINTN N)
{
  UINT8 *D = (UINT8 *)Dst, *S = (UINT8 *)Src;
  for (UINTN i = 0; i < N; i++) D[i] = S[i];
}

// Flip the flag; returns TRUE if the flag reads back 0x00 afterwards.
static BOOLEAN FlipFlag(void)
{
  EFI_RUNTIME_SERVICES *RT = gST->RuntimeServices;
  UINT32 Attrs = 0, UseAttrs;
  UINTN Size = 1;
  UINT8 Before = 0xFF, After = 0xFF, Zero = 0x00;
  EFI_STATUS St;

  St = RT->GetVariable(L"SetupHideFeatures", &gSetupHideFeaturesGuid,
                       &Attrs, &Size, &Before);
  Print(L"SetupHideFeatures before: "); PrintHex8(Before); Print(L"\r\n");
  UseAttrs = (St == EFI_NOT_FOUND || Attrs == 0)
    ? (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
       EFI_VARIABLE_RUNTIME_ACCESS)
    : Attrs;
  if (!(St == EFI_SUCCESS && Before == 0x00)) {
    St = RT->SetVariable(L"SetupHideFeatures", &gSetupHideFeaturesGuid,
                         UseAttrs, 1, &Zero);
    if (EFI_ERROR(St)) { Print(L"SetVariable FAILED\r\n"); return FALSE; }
  }
  Size = 1;
  St = RT->GetVariable(L"SetupHideFeatures", &gSetupHideFeaturesGuid,
                       NULL, &Size, &After);
  Print(L"SetupHideFeatures after:  "); PrintHex8(After); Print(L"\r\n");
  return (St == EFI_SUCCESS && After == 0x00);
}

static BOOLEAN TryLaunchFromHandle(EFI_HANDLE FsHandle, EFI_HANDLE MyImage)
{
  static CHAR16 *Paths[] = {
    L"\\Loader.efi", L"\\EFI\\OC\\Tools\\Loader.efi", L"\\EFI\\UMAF\\Loader.efi"
  };
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfs;
  EFI_FILE_PROTOCOL *Root, *File;
  EFI_DEVICE_PATH_PROTOCOL *Dp;
  EFI_STATUS St;

  St = gBS->HandleProtocol(FsHandle, &gSfsGuid, (void **)&Sfs);
  if (EFI_ERROR(St)) return FALSE;
  St = Sfs->OpenVolume(Sfs, &Root);
  if (EFI_ERROR(St)) return FALSE;

  for (UINTN p = 0; p < 3; p++) {
    St = Root->Open(Root, &File, Paths[p], EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(St)) continue;
    File->Close(File);
    // Found it on this volume: build the full device path and launch.
    St = gBS->HandleProtocol(FsHandle, &gDpGuid, (void **)&Dp);
    if (EFI_ERROR(St)) { Root->Close(Root); return FALSE; }
    UINTN DpLen = 0;
    UINT8 *Walk = (UINT8 *)Dp;
    while (1) {
      UINT8 Type = Walk[DpLen], Sub = Walk[DpLen + 1];
      UINTN NodeLen = Walk[DpLen + 2] | ((UINTN)Walk[DpLen + 3] << 8);
      if (Type == 0x7F && Sub == 0xFF) break;
      DpLen += NodeLen;
    }
    UINTN FileNodeLen = 4 + (StrLen16(Paths[p]) + 1) * 2;
    UINT8 *Full = NULL;
    St = gBS->AllocatePool(EfiBootServicesData, DpLen + FileNodeLen + 4,
                           (void **)&Full);
    if (EFI_ERROR(St)) { Root->Close(Root); return FALSE; }
    CopyMem8(Full, Dp, DpLen);
    UINT8 *Fn = Full + DpLen;
    Fn[0] = 0x04; Fn[1] = 0x04;
    Fn[2] = (UINT8)(FileNodeLen & 0xFF); Fn[3] = (UINT8)(FileNodeLen >> 8);
    CopyMem8(Fn + 4, Paths[p], (StrLen16(Paths[p]) + 1) * 2);
    UINT8 *End = Fn + FileNodeLen;
    End[0] = 0x7F; End[1] = 0xFF; End[2] = 4; End[3] = 0;
    EFI_HANDLE Child = NULL;
    St = gBS->LoadImage(FALSE, MyImage, (EFI_DEVICE_PATH_PROTOCOL *)Full,
                        NULL, 0, &Child);
    gBS->FreePool(Full);
    Root->Close(Root);
    if (EFI_ERROR(St) || Child == NULL) return FALSE;
    Print(L"Launching Loader.efi ...\r\n");
    gBS->StartImage(Child, NULL, NULL);
    return TRUE;
  }
  Root->Close(Root);
  return FALSE;
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  gST = SystemTable; gBS = SystemTable->BootServices;
  Print(L"=== UnhideApp: same-session menu unhide ===\r\n");
  if (!FlipFlag()) {
    Print(L"Flag flip failed — stopping (nothing else changed).\r\n");
    return EFI_DEVICE_ERROR;
  }
  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0;
  EFI_STATUS St = gBS->LocateHandleBuffer(ByProtocol, &gSfsGuid, NULL,
                                          &Count, &Handles);
  if (!EFI_ERROR(St)) {
    for (UINTN i = 0; i < Count; i++) {
      if (TryLaunchFromHandle(Handles[i], ImageHandle)) {
        Print(L"Back from Loader.efi. Flag is still 0 this session.\r\n");
        return EFI_SUCCESS;
      }
    }
  }
  Print(L"Loader.efi not found on any volume.\r\n");
  Print(L"Flag is 0 NOW — launch UMAF from the picker in THIS session.\r\n");
  Print(L"Do NOT reboot first, or POST re-imposes 1.\r\n");
  return EFI_SUCCESS;
}
