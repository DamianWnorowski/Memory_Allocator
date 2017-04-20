#ifndef SFHEAD_H
#define SFHEAD_H
#include <errno.h>



void fillhead(char* addr, int padding, int splinterSize, int size, int blockSize, int splinter, int alloc);

void fillfoot(char* addr, int alloc, int splinter, int blockSize);

void fillfree(char* addr, int size, int blocksize);

char* findfree(char* addr, int blocksize);

char* morePage();

char* coalesce(char* free);

void findfreespot(char* free);

int validptr(char* ptr);

void printfreelist();
void checkmaxmem(int max);

#endif