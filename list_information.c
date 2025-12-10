/**
 * list_information.c
 * Program that lists superblock and directory information for a QFS file system
 * CSC520 - Operating Systems
 * Jean LaFrance
 * 12/6/2025
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image file>\n", argv[0]);
        return 1;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 2;
    }

#ifdef DEBUG
    printf("Opened disk image: %s\n", argv[1]);
#endif

    // Read superblock
    superblock_t superblock;
    fread(&superblock, sizeof(superblock_t), 1, fp);

    // Ensure file system type is QFS
    if (superblock.fs_type != 0x51) {
        fprintf(stderr, "Invalid file system type.\n");
        fclose(fp);
        return 3;
    }

    // Output superblock info
    printf("Superblock Information\n");
    printf("Block Size: %u bytes\n", superblock.bytes_per_block);
    printf("Total Blocks: %u\n", superblock.total_blocks);
    printf("Free Blocks: %u\n", superblock.available_blocks);
    printf("Total Directory Entries: %u\n", superblock.total_direntries);
    printf("Free Directory Entries: %u\n\n", superblock.available_direntries);

    // Move pointer to start
    fseek(fp, sizeof(superblock_t), SEEK_SET);

    // Print directory information
    direntry_t direntry;
    int total_files = 0;
    printf("Directory Entries\n");
    for (int i = 0; i < superblock.total_direntries; i++) {
        if (fread(&direntry, sizeof(direntry_t), 1, fp) != 1) {
            fprintf(stderr, "Error: Could not read directory entry %d.\n", i);
            break;
        }

        if (direntry.filename[0] != '\0') {
            total_files++;

            // Bits 6:7 of permissions are file type
            int file_type = (direntry.permissions >> 6) & 0x03;
            char type_name[5] = "";
            if (file_type == 1) {
                strcpy(type_name, "JPG");
            } else if (file_type == 2) {
                strcpy(type_name, "PNG");
            } else {
                strcpy(type_name, "None");
            }

            printf(">%s\n", direntry.filename);
            printf("File Size: %u bytes\n", direntry.file_size);
            printf("File Type: %s\n", type_name);
            printf("Starting Block: %u\n", direntry.starting_block);
        }
    }
    if (total_files == 0) printf("No files found\n");

    fclose(fp);
    return 0;
}