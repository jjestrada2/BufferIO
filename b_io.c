/**************************************************************
* Class:  CSC-415-03 Fall 2023
* Name: Juan Estrada
* Student ID: 923058731
* GitHub ID: jjestrada2
* Project: Assignment 5 – Buffered I/O
*
* File: b_io.c
*
* Description:
*
**************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"

#define MAXFCBS 20	//The maximum number of files open at one time


// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb
	{
	fileInfo * fi;	//holds the low level systems file info
	char* buffer;
	int byteRead;

	// Add any other needed variables here to track the individual open file

	//char fileInfo->fileName[64];		//filename
	//int fileInfo->fileSize;			//file size in bytes
	//int fileInfo->location;			//starting lba (block number) for the file data

	} b_fcb;
	
//static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;	

// Method to initialize our file system / file control blocks
// Anything else that needs one time initialization can go in this routine
void b_init ()
	{
	if (startup)
		return;			//already initialized

	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].fi = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free File Control Block FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].fi == NULL)
			{
			fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
			return i;		//Not thread safe but okay for this project
			}
		}

	return (-1);  //all in use
	}

// b_open is called by the "user application" to open a file.  This routine is 
// similar to the Linux open function.  	
// You will create your own file descriptor which is just an integer index into an
// array of file control blocks (fcbArray) that you maintain for each open file.  
// For this assignment the flags will be read only and can be ignored.

b_io_fd b_open (char * filename, int flags)
	{
	if (startup == 0) b_init();  //Initialize our system

	//*** TODO ***//  Write open function to return your file descriptor
	//				  You may want to allocate the buffer here as well
	//				  But make sure every file has its own buffer
	b_io_fd indxFil = b_getFCB();
	if(indxFil < 0)return -1;
	fileInfo* get_fi = GetFileInfo(filename);

	//Look for a place in my fcbArray to allocate  my FCB
	fcbArray[indxFil].fi = get_fi;
	
	//allocate buffer for the respective file
	fcbArray[indxFil].buffer =  malloc(B_CHUNK_SIZE);
	// This is where you are going to want to call GetFileInfo and b_getFCB
	fcbArray[indxFil].byteRead = 0;
	
	return indxFil;
	}



// b_read functions just like its Linux counterpart read.  The user passes in
// the file descriptor (index into fcbArray), a buffer where thay want you to 
// place the data, and a count of how many bytes they want from the file.
// The return value is the number of bytes you have copied into their buffer.
// The return value can never be greater then the requested count, but it can
// be less only when you have run out of bytes to read.  i.e. End of File	
int b_read (b_io_fd fd, char * buffer, int count)
	{
	//*** TODO ***//  
	// Write buffered read function to return the data and # bytes read
	// You must use LBAread and you must buffer the data in B_CHUNK_SIZE byte chunks.
		
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}

	// and check that the specified FCB is actually in use	
	if (fcbArray[fd].fi == NULL)		//File not open for this descriptor
		{
		return -1;
		}	

	//Check if I dont have to fetch anything to the buffer
	if(count <= 0) return count;

	//If I am in the last block to finish reading the file
	if(fcbArray[fd].byteRead + count > fcbArray[fd].fi->fileSize){
		count = fcbArray[fd].fi->fileSize - fcbArray[fd].byteRead;
	}

	//keep track of the bytes already in the buffer
	int bytesCopied = 0;

	//LBAread() read block to my inerbuffer
	int currBlock = fcbArray[fd].fi->location + fcbArray[fd].byteRead / B_CHUNK_SIZE;
	char * offset = fcbArray[fd].buffer + fcbArray[fd].byteRead % B_CHUNK_SIZE;
	int spaceLeftInBlock = B_CHUNK_SIZE - fcbArray[fd].byteRead % B_CHUNK_SIZE;

	LBAread(fcbArray[fd].buffer,1,currBlock);
	
	//If there's space left in the current block 
	
	if(spaceLeftInBlock!= 0){
		if(count < spaceLeftInBlock){
			memcpy(buffer, offset, count);
			bytesCopied += count;
			fcbArray[fd].byteRead += count; 
			return count;
		}else{
			/*The else statement run when the number of bytes 
			that I need to read is larger than the current block
			I finish to procces all the bytes outside the else statement*/
			memcpy(buffer,offset , spaceLeftInBlock);
			bytesCopied += spaceLeftInBlock;
			fcbArray[fd].byteRead += spaceLeftInBlock;
		}

		
	}

	/*Now that the block where I left the last read is done 
	I need to keep copying data to the userBuffer by chuncks of 512
	if (count - bytesCopied ) > B_CHUNCK that means I can fill out 
	in one block in one time */

	while(count - bytesCopied > B_CHUNK_SIZE){
		currBlock = fcbArray[fd].fi->location + fcbArray[fd].byteRead / B_CHUNK_SIZE; 
		offset = fcbArray[fd].buffer + fcbArray[fd].byteRead % B_CHUNK_SIZE;

		LBAread(fcbArray[fd].buffer,1,currBlock);
		memcpy(buffer,offset,B_CHUNK_SIZE);

		bytesCopied += B_CHUNK_SIZE;
		fcbArray[fd].byteRead += B_CHUNK_SIZE;
	}

	/*Finally I need to read very last block of data if any left*/
	if(bytesCopied == count) return bytesCopied;
	currBlock = fcbArray[fd].fi->location + fcbArray[fd].byteRead / B_CHUNK_SIZE; 
	offset = fcbArray[fd].buffer + fcbArray[fd].byteRead % B_CHUNK_SIZE;
	
	LBAread(fcbArray[fd].buffer,1,currBlock);
	memcpy(buffer,offset,count - bytesCopied);

	fcbArray[fd].byteRead += count - bytesCopied;
	bytesCopied += count - bytesCopied; 


	return bytesCopied;
	
	}
	


// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd)
	{
	//*** TODO ***//  Release any resources
	free(fcbArray[fd].buffer);
	fcbArray[fd].buffer = NULL;
	}
	
