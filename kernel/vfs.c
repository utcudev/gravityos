/* =============================================================================
 * GravityOS — Virtual File System (VFS) Routing
 * =============================================================================
 * Linux tarzı (/usr/bin) ve Windows tarzı (C:\Windows) dosya yollarını
 * ortak bir sisteme yönlendirir.
 * ============================================================================= */

#include "../lib/stdio.h"
#include "../lib/string.h"
#include <stdint.h>

#define MAX_PATH_LEN 256

/* Gelen dosya yolunu (Path) normalleştirir */
void vfs_normalize_path(const char* input_path, char* output_path)
{
    /* Windows tarzı ters bölü çizgisini (\) Linux düz bölü çizgisine (/) çevir */
    int j = 0;
    for (int i = 0; input_path[i] != '\0' && j < MAX_PATH_LEN - 1; i++) {
        if (input_path[i] == '\\') {
            output_path[j++] = '/';
        } else {
            output_path[j++] = input_path[i];
        }
    }
    output_path[j] = '\0';
}

/* Dosyayı bulup açmaya çalışır (Sanal sürücü taklidi) */
int vfs_open(const char* filename, int flags)
{
    char norm_path[MAX_PATH_LEN];
    vfs_normalize_path(filename, norm_path);

    kprintf("[VFS] Intercepted file open request: '%s'\n", norm_path);

    /* Windows programı kernel32 arıyorsa... */
    if (strstr(norm_path, "kernel32.dll") || strstr(norm_path, "USER32.DLL")) {
        kprintf("[VFS] Found request for Windows DLL. Routing to internal Win32 Shim.\n");
        return 999; /* Özel Handle ID */
    }

    /* Linux programı libc arıyorsa... */
    if (strstr(norm_path, "libc.so.6")) {
        kprintf("[VFS] Found request for Linux glibc. Routing to internal POSIX Shim.\n");
        return 998; /* Özel Handle ID */
    }

    /* Gerçek bir dosya okunacaksa FAT32 disk sürücüsüne yönlendirilecek (Şimdilik mock) */
    kprintf("[VFS] FAT32 Disk Not Ready. Cannot open: %s\n", norm_path);
    return -1;
}
