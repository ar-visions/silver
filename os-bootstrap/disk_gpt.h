#ifndef DISK_GPT_H
#define DISK_GPT_H
// the installer image's table: EFI at 1 MB, the rest Linux
int disk_gpt(const char* img, long long sectors, long long efi_sectors);
#endif
