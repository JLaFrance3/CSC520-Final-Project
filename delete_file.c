#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {
    //Offset for moving through the bytes of information.  Since the superblock is always the first 32 bytes, the offset is 32
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <file to remove>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb+");
    if (!fp) {
        perror("fopen");
        return 2;
    }

#ifdef DEBUG
    printf("Opened disk image: %s\n", argv[1]);
#endif

    //read superblock for block size and number of directory entries
    superblock_t sb;
	fseek(fp, 0, SEEK_SET);
	fread(&sb, sizeof(superblock_t), 1, fp);
	uint16_t blockSize = sb.bytes_per_block;
	uint8_t totalEntries = sb.total_direntries;
    int blockStartIndex = 32 + (32*totalEntries);
	
    
	//seek to the next directory entry
	fseek(fp, 32, SEEK_SET);
    uint16_t nextBlock;
    uint32_t blocksToDelete;
    uint8_t buffer[32] = {0};
    uint8_t openByte = 0x00;
	uint8_t found = 0;
	direntry_t currentEntry;
    

    //iterate through directory entries to find requested file and save the block it starts in
	for(int i=0; i<totalEntries; i++)
	{
		//read the directory entry at the current position
		fread(&currentEntry, sizeof(direntry_t), 1, fp);
		
		//check if the directory entry is the requested file
		if(strcmp(currentEntry.filename, argv[2]) == 0)
		{
			//save the starting block and the number of blocks to open up
			nextBlock = currentEntry.starting_block;
            blocksToDelete = currentEntry.file_size;
			found = 1;

            //overwrite the directory entry with 0's to mark as empty
            fseek(fp,32 + (32*i), SEEK_SET);
            fwrite(&buffer, 1, sizeof(uint8_t) * 32, fp);

            //empty original directory entry :D
			break;
		}
	}

    
    //prints error message and terminates program if file not found
	if(!found)
	{
		printf("FILE NOT FOUND.");
		return 1;
	}

    //iterate through each block within the file, setting the first byte of each to 0x00, or open
    for(int i = 0; i < blocksToDelete; i++){
        //open block
        fseek(fp,blockStartIndex + (nextBlock* blockSize), SEEK_SET);
        fwrite(&openByte, 1, sizeof(uint8_t), fp);

        //read in the next block
        fseek(fp, blockStartIndex + (((nextBlock + 1) * blockSize)) - 2, SEEK_SET);
        fread(&nextBlock, sizeof(uint16_t), 1, fp);
    }
    

    //update the number of available blocks and entries, overwrite existing superblock on-file
    sb.available_blocks += blocksToDelete;
    sb.available_direntries += 1;
    fseek(fp, 0, SEEK_SET);
    fwrite(&sb, 1, sizeof(superblock_t), fp);

    
    //flush file for safety
	fflush(fp);
    fclose(fp);
    return 0;
}
