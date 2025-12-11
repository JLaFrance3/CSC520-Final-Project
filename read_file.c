/*
 * read_file.c
 * Program that reads a file from the disk image and copies it to the working directory
 * CSC520 - Operating Systems
 * Group: Aleena Graveline, Jean LaFrance, Horacio Valdes, Matthew Glennon
 * File worked on by: Aleena Graveline
 * 12/10/2025
 */
 
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <disk image file> <file to read> <output file>\n", argv[0]);
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
    
    //read superblock for block size and number of directory entries
    superblock_t sb;
	fseek(fp, 0, SEEK_SET);
	fread(&sb, sizeof(superblock_t), 1, fp);
	uint16_t blockSize = sb.bytes_per_block;
	uint8_t totalEntries = sb.total_direntries;
	
	//seek to beginning of directory entries
	fseek(fp, 32, SEEK_SET);
	uint16_t blockStart;
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
			//save the starting block
			blockStart = currentEntry.starting_block;
			found = 1;
			break;
		}
	}
	
	//prints error message and terminates program if file not found
	if(!found)
	{
		printf("FILE NOT FOUND.");
		return 1;
	}
	
	//create output file
	FILE *output = fopen(argv[3], "wb");
	
	//calculate how many blocks the file covers
	int numBlocksCovered = currentEntry.file_size/(blockSize-3) +
				(currentEntry.file_size % (blockSize-3) == 0) ? 0 : 1;
	//int offset = currentEntry.file_size % (blockSize-3);

	//go to the start block, skipping the is_busy byte
	fseek(fp, 32 + (255 * 32) + (blockStart * blockSize) + 1, SEEK_SET);
	int totalBytes = 0;
	while (totalBytes < currentEntry.file_size)
	{
		// Read 1 byte
		uint8_t buffer;
		fread(&buffer, sizeof(buffer), 1, fp);
		// Write 1 byte
		fwrite(&buffer, sizeof(buffer), 1, output);
		totalBytes++;
		// If totalBytes a multiple 0f blocksize-3
		if(totalBytes % (blockSize - 3) == 0)
		{
			// Read 2 byte address
			uint16_t address;
			fread(&address, sizeof(address), 1, fp);
			
			if(totalBytes < currentEntry.file_size)
			{
				// If more bytes to read, seek to address+1
				fseek(fp, 32 + (255 * 32) + (address * blockSize) + 1, SEEK_SET);
			}
		}
	}

	/*
		//set up buffer for file data
		uint8_t buffer[blockSize - 3];
		fread(buffer, sizeof(buffer), 1, fp);
		
		//write the data from the buffer to output
		fwrite(buffer, sizeof(buffer), 1, output);
		
		//get the current fileblock to find the next block the file is occupying
		fileblock_t currBlock;
		fseek(fp, -blockSize, SEEK_CUR);
		fread(&currBlock, blockSize, 1, fp);
		uint16_t nextBlock = currBlock.next_block;
		
		//go to the next file block
		fseek(fp, 32 + 255 * 32 + nextBlock * blockSize + 1, SEEK_SET); 
		totalBytes += (blockSize - 3);
	}

	if(numBlocksCovered == 1)
	{
		//set up buffer for file data
		uint8_t buffer[blockSize - 3];
		fread(buffer, sizeof(buffer), 1, fp);
		
		//write the data from the buffer to output
		fwrite(buffer, sizeof(buffer), 1, output);
	}
	else 
	{
		for(int i=1; i<numBlocksCovered; i++)
		{
			if(i != numBlocksCovered)
			{
				//set up buffer for file data
				uint8_t buffer[blockSize - 3];
				fread(buffer, sizeof(buffer), 1, fp);
				
				//write the data from the buffer to output
				fwrite(buffer, sizeof(buffer), 1, output);
				
				//get the current fileblock to find the next block the file is occupying
				fileblock_t currBlock;
				fseek(fp, -blockSize, SEEK_CUR);
				fread(&currBlock, blockSize, 1, fp);
				uint16_t nextBlock = currBlock.next_block;
				
				//go to the next file block
				fseek(fp, 32 + 255 * 32 + nextBlock * blockSize + 1, SEEK_SET); 
			}
			else 
			{
				//read and write the remainder of the data in the last block covered by the file
				uint8_t buffer[offset];
				fread(buffer, sizeof(buffer), 1, fp);
				fwrite(buffer, sizeof(buffer), 1, output);
			}
		}
	}
	*/
	
	//flush written file for safety
	fflush(output);
	
	//close files
    fclose(fp);
    fclose(output);
    return 0;
}
