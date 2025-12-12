/*
 * recover_files.c
 * Program that recovers all files stored in the disk image and writes
 * them to the working directory
 * CSC520 - Operating Systems
 * Group: Aleena Graveline, Jean LaFrance, Horacio Valdes, Matthew Glennon
 * Updated by: Jean LaFrance
 * 12/11/2025
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "qfs.h"

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filesystem_image>\n", argv[0]);
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

    // Read Superblock
    superblock_t sb;
    if (fread(&sb, sizeof(superblock_t), 1, fp) != 1) {
        fprintf(stderr, "Can't read superblock\n");
        return 1;
    }

    // Calculate data start point
    int dataStart = 8192; // superblock size + (total direntries * direntry size)

    // Allocate space for one block minus the header/footer bytes
    int dataSize = sb.bytes_per_block - 3;
    uint8_t *buffer = malloc(dataSize);

    // Iterate through blocks and look for JPEG start
    int fileCount = 0;
    for (int i = 0; i < sb.total_blocks; i++) {
        // Seek to the start of the block
        int currentBlockOffset = dataStart + (i * sb.bytes_per_block);
        fseek(fp, currentBlockOffset, SEEK_SET);

        // Read in bytes
        uint8_t busyByte;
        fread(&busyByte, 1, 1, fp);
        fread(buffer, dataSize, 1, fp);
        uint16_t nextBytes;
        fread(&nextBytes, 2, 1, fp);

        // #ifdef DEBUG
        //     if (buffer[0] != 0x00 && buffer[0] != 0x00 && i < 30) {
        //         printf("Signature: %02X %02X Offset: %d on Block %d...\n", buffer[0], buffer[1], currentBlockOffset, i);
        //     }
        // #endif

        // Check if signature is JPG
        if (buffer[0] == 0xFF && buffer[1] == 0xD8) {
            fileCount++;

            // Prepare output file
            char outputFileName[32]; // 32 is a bit big for file name but compiler was angry
            sprintf(outputFileName, "recovered_file_%d.jpg", fileCount);
            FILE *output = fopen(outputFileName, "wb");

            #ifdef DEBUG
                printf("Recovering %s from Block %d...\n", outputFileName, i);
            #endif

            // Look for all block associated with JPG file
            uint16_t currentBlockIndex = i;
            int isComplete = 0;
            while (!isComplete) {
                // Seek to the offset for current block. Skip header byte
                int currentOffset = dataStart + (currentBlockIndex * sb.bytes_per_block);
                fseek(fp, currentOffset, SEEK_SET);

                // Read in bytes
                uint8_t is_busy;
                fread(&is_busy, 1, 1, fp);      // Read header
                fread(buffer, dataSize, 1, fp); // Read data
                uint16_t nextBlock;
                fread(&nextBlock, 2, 1, fp);    // Read footer

                // Find end of jpg signature (0xFF 0xD9)
                int numBytes = dataSize;
                for (int j = 0; j < dataSize - 1; j++) {
                    if (buffer[j] == 0xFF && buffer[j+1] == 0xD9) {
                        numBytes = j + 2;
                        isComplete = 1;
                        break;
                    }
                }

                // Write data to output file
                fwrite(buffer, numBytes, 1, output);

                if (isComplete) break;

                // Ensure next block exists
                if (nextBlock >= sb.total_blocks) break;

                currentBlockIndex = nextBlock;
            }

            fclose(output);
        }
    }

    free(buffer);
    fclose(fp);
    return 0;
}