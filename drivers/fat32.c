/* =============================================================================
 * GravityOS — FAT32 File System Driver
 * ============================================================================= */

#include "fat32.h"
#include "../lib/stdio.h"

void fat32_init(void)
{
    /* TODO: Read Master Boot Record (MBR) from ATA PIO / IDE Controller */
    /* TODO: Parse Volume ID, FAT table offsets, and Root Directory Cluster */
    kprintf("[FAT32] Driver initialized. (ATA PIO / IDE controller not ready yet)\n");
}
