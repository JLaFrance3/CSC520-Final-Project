/*
 * write_file.c
 * Program that writes a file in the working directory to the disk image
 * CSC520 - Operating Systems
 * Author: Horacio Valdes
 * 12/10/25
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <file to add>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb+");
    if (!fp) {
        perror("fopen");
        return 2;
    }

    FILE *src = fopen(argv[2], "rb");
    if (!src) {
        perror("fopen");
        fclose(fp);
        return 3;
    }

    uint8_t header[8];
    size_t header_read = fread(header, 1, 8, src);
    rewind(src);

    int is_jpg = 0, is_png = 0;

    //Image setup
    // JPEG starts with FF D8
    if (header_read >= 2) {
        if (header[0] == 0xFF && header[1] == 0xD8) {
            is_jpg = 1;
        }
    }

    // PNG starts with 89 50 4E 47 0D 0A 1A 0A
    if (header_read >= 8) {
        if (header[0] == 0x89 && header[1] == 0x50 &&
            header[2] == 0x4E && header[3] == 0x47 &&
            header[4] == 0x0D && header[5] == 0x0A &&
            header[6] == 0x1A && header[7] == 0x0A) {
            is_png = 1;
        }
    }

    if (!is_jpg && !is_png) {
        fprintf(stderr, "Error: Only JPG or PNG image files may be written.\n");
        fclose(src);
        fclose(fp);
        return 99;
    }

    //Super block stuff

    superblock_t superblock;
    if (fread(&superblock, sizeof(superblock_t), 1, fp) != 1) {
        fprintf(stderr, "Failed to read superblock\n");
        fclose(src);
        fclose(fp);
        return 4;
    }

    if (superblock.fs_type != 0x51) {
        fprintf(stderr, "Invalid file system type\n");
        fclose(src);
        fclose(fp);
        return 5;
    }

    if (superblock.bytes_per_block < 3) {
        fprintf(stderr, "Invalid block size in superblock\n");
        fclose(src);
        fclose(fp);
        return 6;
    }

    // Determine source file size
    if (fseek(src, 0, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek source file\n");
        fclose(src);
        fclose(fp);
        return 7;
    }
    long file_size = ftell(src);
    if (file_size < 0) {
        fprintf(stderr, "Failed to determine input file size\n");
        fclose(src);
        fclose(fp);
        return 8;
    }
    rewind(src);

    // Compute required blocks (each block: 1 busy byte, data, 2-byte next pointer)
    size_t data_bytes_per_block = superblock.bytes_per_block - 3;
    size_t blocks_needed = (file_size == 0) ? 1 :
        (size_t)((file_size + data_bytes_per_block - 1) / data_bytes_per_block);

    if (superblock.available_direntries == 0) {
        fprintf(stderr, "No free directory entries available\n");
        fclose(src);
        fclose(fp);
        return 9;
    }

    if (superblock.available_blocks < blocks_needed) {
        fprintf(stderr, "Not enough free blocks available\n");
        fclose(src);
        fclose(fp);
        return 10;
    }

    // Locate free directory entry and ensure no duplicate name
    direntry_t direntry;
    long free_dir_offset = -1;
    fseek(fp, sizeof(superblock_t), SEEK_SET);
    for (uint8_t i = 0; i < superblock.total_direntries; i++) {
        long current_offset = ftell(fp);
        if (fread(&direntry, sizeof(direntry_t), 1, fp) != 1) {
            fprintf(stderr, "Failed to read directory entry\n");
            fclose(src);
            fclose(fp);
            return 11;
        }
        if (direntry.filename[0] != '\0' &&
            strncmp(direntry.filename, argv[2], sizeof(direntry.filename)) == 0) {
            fprintf(stderr, "File already exists in image\n");
            fclose(src);
            fclose(fp);
            return 12;
        }
        if (direntry.filename[0] == '\0' && free_dir_offset == -1) {
            free_dir_offset = current_offset;
        }
    }

    if (free_dir_offset == -1) {
        fprintf(stderr, "No free directory entry found\n");
        fclose(src);
        fclose(fp);
        return 13;
    }

    // Collect free blocks
    uint16_t *blocks = malloc(sizeof(uint16_t) * blocks_needed);
    if (!blocks) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(src);
        fclose(fp);
        return 14;
    }

    size_t found_blocks = 0;
    long data_region_offset = sizeof(superblock_t) +
                              (superblock.total_direntries * sizeof(direntry_t));
    for (uint16_t i = 0; i < superblock.total_blocks && found_blocks < blocks_needed; i++) {
        long block_offset = data_region_offset + ((long)i * superblock.bytes_per_block);
        uint8_t busy_flag;
        fseek(fp, block_offset, SEEK_SET);
        if (fread(&busy_flag, 1, 1, fp) != 1) {
            fprintf(stderr, "Failed to read block metadata\n");
            free(blocks);
            fclose(src);
            fclose(fp);
            return 15;
        }
        if (busy_flag == 0x00) {
            blocks[found_blocks++] = i;
        }
    }

    if (found_blocks < blocks_needed) {
        fprintf(stderr, "Insufficient free data blocks\n");
        free(blocks);
        fclose(src);
        fclose(fp);
        return 16;
    }

    uint8_t *data_buffer = malloc(data_bytes_per_block);
    if (!data_buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        free(blocks);
        fclose(src);
        fclose(fp);
        return 17;
    }

    // Write data into blocks
    size_t remaining = (size_t)file_size;
    for (size_t idx = 0; idx < blocks_needed; idx++) {
        memset(data_buffer, 0, data_bytes_per_block);
        size_t chunk = remaining > data_bytes_per_block ? data_bytes_per_block : remaining;
        if (chunk > 0 && fread(data_buffer, 1, chunk, src) != chunk) {
            fprintf(stderr, "Failed to read from source file\n");
            free(data_buffer);
            free(blocks);
            fclose(src);
            fclose(fp);
            return 18;
        }
        remaining -= chunk;

        long block_offset = data_region_offset + ((long)blocks[idx] * superblock.bytes_per_block);
        fseek(fp, block_offset, SEEK_SET);
        uint8_t busy = 0x01;
        fwrite(&busy, 1, 1, fp);
        fwrite(data_buffer, 1, data_bytes_per_block, fp);
        uint16_t next_block = (idx + 1 < blocks_needed) ? blocks[idx + 1] : 0xFFFF;
        fwrite(&next_block, sizeof(uint16_t), 1, fp);
    }

    // Prepare and write directory entry
    direntry_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    strncpy(new_entry.filename, argv[2], sizeof(new_entry.filename) - 1);
    new_entry.permissions = 0x00;
    new_entry.owner_id = 0x00;
    new_entry.group_id = 0x00;
    new_entry.starting_block = blocks[0];
    new_entry.file_size = (uint32_t)file_size;

    fseek(fp, free_dir_offset, SEEK_SET);
    fwrite(&new_entry, sizeof(new_entry), 1, fp);

    // Update superblock
    superblock.available_direntries--;
    superblock.available_blocks -= (uint16_t)blocks_needed;
    fseek(fp, 0, SEEK_SET);
    fwrite(&superblock, sizeof(superblock), 1, fp);

    free(data_buffer);
    free(blocks);
    fclose(src);
    fclose(fp);
    return 0;
}
