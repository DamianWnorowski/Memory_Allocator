/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include "sfhead.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
int page = 0;
char* start_of_heap = NULL;
static int allocatedBlocks = 0;
//static int splinterBlocks = 0;
static int paddingbytes = 0;
static int splintering = 0;
static int coalesces = 0;
static double peakMemoryUtilization = 0;
static double maxMemory = 0;

void *sf_malloc(size_t size) {
	if(size == 0){
		errno = EINVAL;
		return NULL;
	}
	if(size > 16368){
		errno = ENOMEM;
		return NULL;
	}

	//calculate if padding needed
	size_t padding =(size % 16);
	if(padding != 0){
		padding = 16 - padding;
	}
	size_t blocksize = (padding+size+16);
	char* free = NULL;

	//create the first page
	if(page == 0){
		free = sf_sbrk(4096);
		start_of_heap = free;
		fillfree(free,0, 4096);
		page++;
	}

	free = findfree(free, blocksize);
	//if there was no new page found increase the size of the page
	while(free == NULL && page < 4){
		morePage(1);
		page++;
		free = findfree(free, blocksize);
	}

	//return block found
	if(free != NULL){
		sf_header *header = (sf_header*) free;
		int splintersize = 0;
		int splinter = 0;
		if(header->splinter_size > 0){
			splintering ++;
			splintersize = header->splinter_size;
			blocksize += splintersize;
			splinter = 1;
		}
		paddingbytes += padding;
		allocatedBlocks++;
		peakMemoryUtilization += size;
		checkmaxmem(peakMemoryUtilization);
		fillhead(free, padding, splintersize, size, blocksize, splinter, 1);

		return free + 8;
	}
	errno = ENOMEM;
	return NULL;
}

char* morePage(int pages){
	//create a new page and combine if the last block is free
	char* newFree = sf_sbrk(pages*4096);
	fillfree(newFree,0,pages*4096);
	char* lastfree = newFree - 8;
	sf_free_header *headptr = (sf_free_header*) freelist_head;
	if(freelist_head == NULL){
		freelist_head = (sf_free_header*) newFree;
		freelist_head->next = NULL;
		freelist_head->prev = NULL;
		return newFree;
	}
	while(headptr){
		if((char*)headptr+(headptr->header.block_size<<4)-8 == lastfree){
			sf_header *head = (sf_header*) headptr;
			sf_header *foot = (sf_header*) newFree;
			fillfree((char*)head, 0, ((head->block_size << 4)+ (foot->block_size<<4)));
			coalesces++;
			return (char*)head;
		}
		headptr = headptr->next;
	}
	headptr = freelist_head;
	while(headptr->next){
		headptr= headptr->next;
	}
	sf_free_header *newfreeptr = (sf_free_header*) newFree;
	headptr->next = newfreeptr;
	newfreeptr->prev = headptr;
	newfreeptr->next = NULL;
	return newFree;
}

char* coalesce(char* free){
	sf_free_header *headptr = freelist_head;
	sf_free_header *head = (sf_free_header*) free;
	char* checkleft = 0;
	char* left = free;
	char* right = free + (head->header.block_size<<4);
	int ex = 0;
	int rightcoal = 0;
	//check if to the left is also empty block
	while(headptr && ex ==0){
		checkleft = (char*) headptr + (headptr->header.block_size<<4);
		if(left == checkleft){
			if(freelist_head == headptr){

			}
			if(head->next){
				headptr->next = head->next;
				head->next->prev = headptr;
			}
			else{
				headptr->next = NULL;
			}
			rightcoal = 1;
			coalesces++;
			fillfree((char*)headptr, 0, ((head->header.block_size << 4)+ (headptr->header.block_size<<4)));
			ex = 1;
		}
		else{
			headptr = headptr->next;
		}
	}

	ex = 0;
	headptr = freelist_head;
	//check if to the right is also empty block
	while(headptr && ex == 0){
		right = (char*)headptr + (headptr->header.block_size<<4);

		if((char*) headptr->next == right){
			head = headptr->next;
			if(head->next){
				headptr->next = head->next;
				head->next->prev = headptr;
			}
			else{
				headptr->next = NULL;
			}
			if(rightcoal == 0){
				coalesces++;
			}
			fillfree((char*)headptr, 0, ((head->header.block_size << 4)+ (headptr->header.block_size<<4)));
			ex = 1;
		}
		else{
			headptr = headptr->next;
		}
	}
	return free;
}

void checkmaxmem(int max){
	if(max > maxMemory){
		maxMemory = max;
	}
	return;
}

//Find free space in heap
char* findfree(char* addr, int blocksize){
	int size = 0;
	sf_free_header* ptr = freelist_head;
	//find if there is a block that fits exact otherwise get the best fit
	while(ptr){
		sf_header *head = (sf_header*) ptr;
		if(blocksize == head->block_size<<4){
			head->splinter_size = 0;
			if(ptr == freelist_head){
				if(ptr->next){
					freelist_head = ptr->next;
					freelist_head->prev = NULL;
				}
				else{
					freelist_head = NULL;
				}
			}
			else{
				if(ptr->next){
					ptr->next->prev = ptr->prev;
					ptr->prev->next = ptr->next;
				}
				else{
					ptr->prev->next = NULL;
				}
			}
			return (char*) ptr;
		}
		//change the size of the best fit to the lowest possible
		if(blocksize < head->block_size<<4){
			if(size == 0){
				size = (head->block_size<<4);
			}
			else if(size > head->block_size<<4){
				size = (head->block_size<<4);
			}
		}
		ptr = ptr->next;
	}
	//if there is a size big enough for the block;
	if(size != 0){
		ptr = freelist_head;
		while(ptr){
			int splint = 0;

			//check if splinter is needed
			if(((ptr->header.block_size<<4) - blocksize) < 32){
				splint = (ptr->header.block_size<<4) - blocksize;
			}

			//If size is exact, free it from the list
			if((ptr->header.block_size<<4) == size){

				if((((ptr->header.block_size<<4)-splint) - blocksize) == 0){

					//if the size is exact , freehead_list->next is new head of list
					//if freehead_list->next is null, freehead list is null
					if(ptr == freelist_head){
						if(freelist_head->next){
							freelist_head->next->prev = NULL;
							freelist_head= freelist_head->next;
						}
						else{
							freelist_head = NULL;
						}
					}
					else{
						ptr->prev->next = ptr->next;
						ptr->next->prev = ptr->prev;
					}
				}
				//if the address found is the head of the free list and not exact size,
				//reduce the size of the head
				else if(ptr == freelist_head){
					char* new = ((char*)freelist_head + blocksize);
					sf_free_header *newtemp = (sf_free_header*) new;
					//if freehead_list->next isnt null change the next->prev to new head list
					if(freelist_head->next != NULL){
						freelist_head->next->prev = (sf_free_header*) new;
						newtemp->next= freelist_head->next;
					}
					freelist_head = (sf_free_header*) new;
					fillfree(new, 0,(((ptr->header.block_size<<4)-splint) - blocksize));
				}
				//new free block is somewhere in the block,
				else{
					int newsize = (((ptr->header.block_size<<4)-splint) - blocksize);
					char* tempaddr = (char*)ptr + blocksize;
					sf_free_header *tempHead = (sf_free_header*) tempaddr;
					//if prev isnt null, change the next value else made it null
					if(ptr->prev){
						ptr->prev->next = tempHead;
						tempHead->prev = ptr->prev;
					}
					else{
						tempHead->prev = NULL;
					}
					//if next isnt null change next->prev to free addr otherwise null
					if(ptr->next){
						ptr->next->prev = tempHead;
						tempHead->next = ptr->next;
					}
					else{
						tempHead->next = NULL;
					}
					fillfree((char*)tempHead, 0 , newsize);
				}
				sf_header *header = (sf_header*)ptr;
				header->splinter_size = splint;
				return (char*) ptr;
			}
			ptr = ptr->next;
		}
	}
	return NULL;
}

//Fills header and foot then changes freelist
void fillhead(char* addr, int padding, int splinterSize, int size, int blockSize, int splinter, int alloc){
	sf_header *block;
	block = (sf_header* )addr;
	block->alloc = alloc;
	block->splinter = splinter;
	block->block_size = blockSize>>4;
	block->requested_size = size;
	block->splinter_size = splinterSize;
	block->padding_size = padding;
	fillfoot(addr, alloc, splinter, blockSize);
	return ;
}

void fillfree(char* addr, int size, int blocksize){
	sf_free_header *freehead = (sf_free_header*) addr;
	freehead->header.alloc= 0;
	freehead->header.block_size =(blocksize-size)>>4;

	if(page == 0){
		freehead->next = NULL;
		freehead->prev = NULL;
		freelist_head = freehead;
	}

	//add pointers
	fillfoot(addr, 0, 0, blocksize-size);
	return;
}

void fillfoot(char* addr, int alloc, int splinter, int blockSize){
	sf_footer *foot;
	addr += blockSize -8;
	foot = (sf_footer*) addr;
	foot->alloc = alloc;
	foot->splinter = splinter;
	foot->block_size = blockSize>>4;
	return;
}

void *sf_realloc(void *ptr, size_t size) {
	if(validptr(ptr) == 0){
		return NULL;
	}

	if(size > 16368){
		errno = ENOMEM;
		return NULL;
	}

	size_t padding =(size % 16);
	if(padding != 0){
		padding = 16 - padding;
	}
	int newBlock = size + padding + 16;
	int rightfree = 0, leftfree = 0;
	sf_free_header *free = freelist_head;
	sf_header *blockptr = (sf_header*)((char*) ptr -8);
	char*  rightadjacent = (char*)blockptr + (blockptr->block_size<<4);
	sf_free_header *adjacent_free = NULL;

	if(size == 0){
		sf_free(ptr);
		return NULL;
	}

	//check if adjacent blocks are free
	while(free){
		if(rightadjacent == (char*)free){
			rightfree = 1;
			adjacent_free = free;
		}
		if((char*)free + (free->header.block_size<<4) == (char*)blockptr){
			leftfree = 1;
			//adjacent_free = free;
		}
		free = free->next;
	}
	if(rightfree || leftfree){

	}

	if(newBlock == blockptr->block_size<<4){
		paddingbytes -= blockptr->padding_size;
		paddingbytes += padding;
		splintering -= blockptr->splinter;
		peakMemoryUtilization -= blockptr->requested_size;
		peakMemoryUtilization += size;
		checkmaxmem(peakMemoryUtilization);
		fillhead((char*)blockptr,padding,0,size,newBlock,0,1);
		return (char*)blockptr + 8;
	}
	//new size is smaller than old size
	//splitting block with splinter no free adjacent free blocks
	if(newBlock < blockptr->block_size<<4){
		paddingbytes -= blockptr->padding_size;
		paddingbytes += padding;
		splintering -= blockptr->splinter;
		peakMemoryUtilization -= blockptr->requested_size;
		peakMemoryUtilization += size;
		checkmaxmem(peakMemoryUtilization);
		int splinter = 0;
		splinter = ((blockptr->block_size<<4) - newBlock);
		sf_free_header *moveFree = (sf_free_header*)((char*)adjacent_free - splinter);
		if(splinter < 32){

			//if right block is free, coalesce with it
			if(rightfree == 1){
				coalesces ++;
				fillhead((char*)blockptr,padding,0,size,newBlock,0,1);
				fillfree((char*)moveFree, 0, (adjacent_free->header.block_size<<4)+splinter);
				if(adjacent_free == freelist_head){
					if(adjacent_free->next){
						moveFree->next = adjacent_free->next;
						moveFree->next->prev = moveFree;
					}
					else{
						moveFree->next = NULL;

					}
					freelist_head = moveFree;
				}
				else{
					if(adjacent_free->prev){
						moveFree->prev = adjacent_free->prev;
						moveFree->prev->next = moveFree;
					}
					if(adjacent_free->next){
						moveFree->next = adjacent_free->next;
						moveFree->next->prev = moveFree;
					}
					else{
						moveFree->next = NULL;
					}
				}
				return (char*)blockptr + 8;
			}
			splintering++;
			fillhead((char*)blockptr,padding,splinter,size,newBlock+splinter,1,1);
			return (char*)blockptr + 8;
		}
		if(rightfree == 1){
			splintering -= blockptr->splinter;
			coalesces ++;
			if(adjacent_free->prev){
				moveFree->prev = adjacent_free->prev;
				moveFree->prev->next = moveFree;
			}
			else{
				freelist_head = moveFree;
			}
			if(adjacent_free->next){
				moveFree->next = adjacent_free->next;
				moveFree->next->prev = moveFree;
			}
			fillfree((char*)moveFree, 0, (adjacent_free->header.block_size<<4)+splinter);
			fillhead((char*)blockptr,padding,0,size,newBlock,0,1);
			return (char*)blockptr + 8;;
		}
		moveFree = (sf_free_header*)((char*)blockptr + newBlock);
		fillfree((char*)moveFree, 0,splinter);
		findfreespot((char*) moveFree);
		fillhead((char*)blockptr,padding,0,size,newBlock,0,1);
		return (char*)blockptr + 8;
	}
	//check if right block is free for making a bigger block
	if(rightfree == 1){
		if((blockptr->block_size<<4) + (adjacent_free->header.block_size<<4) - newBlock == 0){
			paddingbytes -= blockptr->padding_size;
			paddingbytes += padding;
			peakMemoryUtilization -= blockptr->requested_size;
			peakMemoryUtilization += size;
			checkmaxmem(peakMemoryUtilization);

			splintering -= blockptr->splinter;
			fillhead((char*)blockptr,padding,0,size,newBlock,0,1);
			if(adjacent_free->next){
				if(adjacent_free->prev){
					adjacent_free->next->prev = adjacent_free->prev;
					adjacent_free->prev->next = adjacent_free->next;
				}
				else{
					adjacent_free->next->prev = NULL;
					freelist_head = adjacent_free->next;
				}
			}
			else if(adjacent_free->prev){
				adjacent_free->prev->next = NULL;
			}
			else{
				freelist_head = NULL;
			}
			return (char*)blockptr + 8;
		}
		//split the block, if the free split block is less than 32, its a splinter
		if((blockptr->block_size<<4) + (adjacent_free->header.block_size<<4) > newBlock){
			paddingbytes -= blockptr->padding_size;
			paddingbytes += padding;
			peakMemoryUtilization -= blockptr->requested_size;
			peakMemoryUtilization += size;
			checkmaxmem(peakMemoryUtilization);

			splintering -= blockptr->splinter;
			if((blockptr->block_size<<4) + (adjacent_free->header.block_size<<4) - newBlock < 32){
				splintering++;
				if(adjacent_free->next){
					if(adjacent_free->prev){
						adjacent_free->next->prev = adjacent_free->prev;
						adjacent_free->prev->next = adjacent_free->next;
					}
					else{
						adjacent_free->next->prev = NULL;
						freelist_head = adjacent_free->next;
					}
				}
				else if(adjacent_free->prev){
					adjacent_free->prev->next = NULL;
				}
				else{
					freelist_head = NULL;
				}

				int splinter = (blockptr->block_size<<4) + (adjacent_free->header.block_size<<4)-newBlock;
				fillhead((char*)blockptr,padding,splinter,size,newBlock+splinter,1,1);
				return (char*)blockptr + 8;
			}
			int newfreesize = (blockptr->block_size<<4) + (adjacent_free->header.block_size<<4) - newBlock;
			sf_free_header *changefree = (sf_free_header*)((char*)blockptr + newfreesize);
			fillfree((char*)changefree,0,newfreesize);
			fillhead((char*) blockptr, padding, 0, size, newBlock, 0, 1);
			if(adjacent_free->next){
				changefree->next = adjacent_free->next;
				adjacent_free->next->prev = changefree;
			}else{
				changefree->next = NULL;
			}
			if(adjacent_free->prev){
				changefree->prev = adjacent_free->prev;
				changefree->prev->next = changefree;
			}
			else{
				changefree->prev = NULL;
				freelist_head = changefree;
			}

			return (char*)blockptr + 8;
		}

	}
	char* newAddr = findfree((char*) blockptr, newBlock);

	while(newAddr == NULL && page < 4){
		morePage(1);
		page++;
		newAddr = findfree(newAddr, newBlock);
	}

	if(newAddr != NULL){
		sf_header *header = (sf_header*) newAddr;
		int splintersize = 0;
		int splinter = 0;
		if(header->splinter_size > 0){
			splintering++;
			splintersize = header->splinter_size;
			newBlock += splintersize;
			splinter = 1;
		}
		allocatedBlocks++;

		peakMemoryUtilization += size;
		peakMemoryUtilization -= blockptr->requested_size;
		checkmaxmem(peakMemoryUtilization);

		paddingbytes += padding;

		fillhead(newAddr, padding, splintersize, size, newBlock, splinter, 1);
		memcpy(newAddr+8, ptr, blockptr->requested_size);

		peakMemoryUtilization += blockptr->requested_size;
		sf_free(ptr);
		return newAddr + 8;
	}
	errno = ENOMEM;
	return NULL;
}

void sf_free(void* ptr) {
	if(validptr(ptr) == 0){
		return;
	}
	//header address
	char* addr = ((char*) ptr) - 8;
	//set block to free
	sf_free_header *newFree = (sf_free_header*) addr;
	newFree->header.alloc = 0;
	sf_footer *foot = (sf_footer*) (addr + (newFree->header.block_size<<4)-8);
	foot->alloc = 0;
	splintering -= newFree->header.splinter;
	paddingbytes -= newFree->header.padding_size;
	allocatedBlocks--;
	peakMemoryUtilization -= newFree->header.requested_size;
	checkmaxmem(peakMemoryUtilization);
	findfreespot((char*)newFree);
	return;
}


int validptr(char* ptr){
	if(ptr == NULL){
		errno = EINVAL;
		//printf("NULL\n");
		return 0;
	}

	//Check if ptr if out of range of heap
	char* end_of_heap = start_of_heap + (4096 * page);
	if(ptr < start_of_heap || ptr > end_of_heap || start_of_heap == NULL){
		errno = EINVAL;
		//printf("Out of heap range: %p\nStart: %p\nEnd: %p\n", (void*)ptr,(void*)start_of_heap,(void*)end_of_heap);
		return 0;
	}
	//Compares header and footer
	char* addr = ((char*) ptr) - 8;
	sf_header *checkhead = (sf_header*) addr;
	sf_footer *checkfoot = (sf_footer*)((char*)addr + (checkhead->block_size<<4)-8);
	if(checkhead->alloc == 0|| checkfoot->alloc == 0||
		(checkhead->splinter != checkfoot->splinter) ||
		(checkhead->block_size != checkfoot->block_size)){
			errno = EINVAL;
			//printf("invalid pointer\n");
			/*printf("\talloc: %d = %d \n",checkhead->alloc, checkfoot->alloc );
			printf("\tsplinter: %d = %d \n",checkhead->splinter, checkfoot->splinter );
			printf("\tsize: %d = %d \n",checkhead->block_size, checkfoot->block_size );*/
			return 0;
	}
	return 1;
}

void findfreespot(char* find){
	sf_free_header *headptr = (sf_free_header*) freelist_head;
	sf_free_header *newFree = (sf_free_header*) find;

	//freelist head is null then new free block is the head
	if(freelist_head == NULL){
		newFree->next=NULL;
		newFree->prev=NULL;
		freelist_head = newFree;
		return;
	}

	//if new free is less than freelist head, then it is the new head
	if(newFree < freelist_head){
		newFree->next = freelist_head;
		newFree->next->prev = newFree;
		newFree->prev = NULL;
		freelist_head = newFree;
		coalesce(find);
		return;
	}

	//Find the spot in the list
	while(headptr){
		//headptr->prev < newFree < headptr
		if(newFree < headptr && newFree > headptr->prev){
			newFree->prev = headptr->prev;
			headptr->prev->next = newFree;
			headptr->prev = newFree;
			newFree->next = headptr;
			coalesce(find);
			return;
		}//if next is null, new free is end of list;
		else if(headptr->next == NULL){
			headptr->next = newFree;
			newFree->prev = headptr;
			newFree->next = NULL;
			coalesce(find);
			return;
		}
		headptr = headptr->next;
	}
	return;
}

int sf_info(info* ptr) {
	if(ptr == NULL || page == 0){
		return -1;
	}
	ptr->allocatedBlocks = allocatedBlocks;
	ptr->splinterBlocks = splintering;
	ptr->padding = paddingbytes;
	ptr->splintering = splintering * 16;
	ptr->coalesces = coalesces;
	ptr->peakMemoryUtilization = maxMemory/(page*4096);
	return 0;
}
