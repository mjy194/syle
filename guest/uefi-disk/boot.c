#include <efi.h>
#include <efilib.h>
#ifdef SYLE_DISK_GRAPHICS
#define SYLE_GRAPHICS_CONSOLE L"console=tty0 "
static EFI_STATUS graphics_probe(void){
 EFI_GUID guid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;EFI_GRAPHICS_OUTPUT_PROTOCOL*gop=0;
 EFI_STATUS s=uefi_call_wrapper(BS->LocateProtocol,3,&guid,0,(void**)&gop);
 if(EFI_ERROR(s)||!gop||!gop->Mode||gop->Mode->MaxMode<2)return EFI_UNSUPPORTED;
 s=uefi_call_wrapper(gop->SetMode,2,gop,1);if(EFI_ERROR(s))return s;
 EFI_GRAPHICS_OUTPUT_MODE_INFORMATION*i=gop->Mode->Info;
 if(!i||i->HorizontalResolution!=800||i->VerticalResolution!=600||i->PixelFormat!=PixelBlueGreenRedReserved8BitPerColor||i->PixelsPerScanLine<800||gop->Mode->FrameBufferSize<(UINT64)i->PixelsPerScanLine*600*4)return EFI_UNSUPPORTED;
 volatile UINT32*p=(void*)(UINTN)gop->Mode->FrameBufferBase;
 for(UINT32 y=0;y<600;y++)for(UINT32 x=0;x<800;x++)p[(UINTN)y*i->PixelsPerScanLine+x]=y<300?(x<400?0xff0000:0x00ff00):(x<400?0x0000ff:0xffffff);
 Print(L"SYLE_GOP_PATTERN_READY 800x600 BGRX\r\n");
 uefi_call_wrapper(BS->Stall,1,2000000);
 Print(L"SYLE_GOP_HANDOFF_READY\r\n");return EFI_SUCCESS;
}
#else
#define SYLE_GRAPHICS_CONSOLE L""
#endif
EFI_STATUS efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE*system){
 InitializeLib(image,system);
 Print(L"SYLE_DISK_EFI_LOADED\r\n");
 EFI_LOADED_IMAGE*loaded=0;EFI_STATUS status;
 status=uefi_call_wrapper(BS->HandleProtocol,3,image,&LoadedImageProtocol,(void**)&loaded);
 if(EFI_ERROR(status)||!loaded)return EFI_LOAD_ERROR;
 EFI_FILE_HANDLE root=LibOpenRoot(loaded->DeviceHandle),file=0;
 if(!root)return EFI_NOT_FOUND;
 status=uefi_call_wrapper(root->Open,5,root,&file,L"\\SYLE.TXT",EFI_FILE_MODE_READ,0);
 if(EFI_ERROR(status))return status;
 char bytes[64]={0};UINTN n=sizeof(bytes);
 status=uefi_call_wrapper(file->Read,3,file,&n,bytes);
 uefi_call_wrapper(file->Close,1,file);uefi_call_wrapper(root->Close,1,root);
 static const char expect[]="SYLE_FAT_DISK_READ_VERIFIED\n";
 if(EFI_ERROR(status)||n!=sizeof(expect)-1)return EFI_CRC_ERROR;
 for(UINTN i=0;i<n;i++)if(bytes[i]!=expect[i])return EFI_CRC_ERROR;
 Print(L"SYLE_DISK_FILE_VERIFIED\r\n");
#ifdef SYLE_DISK_GRAPHICS
 status=graphics_probe();if(EFI_ERROR(status)){Print(L"SYLE_GOP_FAILED %r\r\n",status);return status;}
#endif
#ifdef SYLE_DISK_LINUX
 EFI_DEVICE_PATH*path=FileDevicePath(loaded->DeviceHandle,L"\\EFI\\BOOT\\LINUX.EFI");EFI_HANDLE kernel=0;
 if(!path)return EFI_OUT_OF_RESOURCES;
 status=uefi_call_wrapper(BS->LoadImage,6,FALSE,image,path,NULL,0,&kernel);
 if(EFI_ERROR(status)){Print(L"SYLE_DISK_KERNEL_LOAD_FAILED %r\r\n",status);return status;}
 EFI_LOADED_IMAGE*k=0;status=uefi_call_wrapper(BS->HandleProtocol,3,kernel,&LoadedImageProtocol,(void**)&k);
 if(EFI_ERROR(status))return status;
 /* The configured ISA COM1 is independent of the PCI firmware console. */
#ifdef SYLE_DISK_ROOT
 static CHAR16 options[]=SYLE_GRAPHICS_CONSOLE L"console=ttyS0,115200n8 earlycon=uart8250,io,0x3f8,115200n8 nokaslr noapic acpi=off pci=conf1 root=/dev/vda2 rootfstype=ext2 rw rootwait init=/sbin/init";
#else
 static CHAR16 options[]=SYLE_GRAPHICS_CONSOLE L"console=ttyS0,115200n8 earlycon=uart8250,io,0x3f8,115200n8 nokaslr noapic acpi=off pci=off rdinit=/init";
#endif
 k->LoadOptions=options;k->LoadOptionsSize=sizeof options;
 Print(L"SYLE_DISK_LINUX_START_IMAGE\r\n");
 status=uefi_call_wrapper(BS->StartImage,3,kernel,NULL,NULL);
 Print(L"SYLE_DISK_KERNEL_RETURNED %r\r\n",status);return status;
#else
 return EFI_SUCCESS;
#endif
}
