#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"
#include <errno.h>

/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */

Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void *pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - 8;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header *sfHeader = (sf_header *)((char*)x - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");

  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in header is not 1!");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

//#
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//#

Test(sf_memsuite, Malloc_One_Page, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4080);
  int *y = sf_malloc(4);
  memset(x, 0, 0);
  memset(y, 0, 0);

  cr_assert(freelist_head->header.block_size<<4 == 4064, "Free block size not 4064");
  sf_footer *freelist_foot = (sf_footer*)((char*)freelist_head + (freelist_head->header.block_size<<4)-8);
  cr_assert(freelist_head->header.alloc == 0, "freelist_head alloc bit is not 0");
  cr_assert(freelist_head->header.alloc == freelist_foot->alloc, "Header and footer have diferent bits");
  cr_assert(freelist_head->header.block_size == freelist_foot->block_size, "Header and footer have diferent blocksize");
  cr_assert(freelist_head->header.splinter == freelist_foot->splinter, "Header and footer have diferent splinter bits");
}

Test(sf_memsuite, Malloc_4_one_pages_and_try_anther_malloc, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *a = sf_malloc(4080);
  int *b = sf_malloc(4080);
  int *c = sf_malloc(4080);
  int *d = sf_malloc(4080);

  memset(a, 0, 0);
  memset(b, 0, 0);
  memset(c, 0, 0);
  memset(d, 0, 0);

  cr_assert(freelist_head == NULL, "freelist_head is not null");

  int *e = sf_malloc(1);
  cr_assert(errno == ENOMEM, "errno not ENOMEM");
  cr_assert(e == NULL, "e is not NULL");
}

Test(sf_memsuite, Check_info_stats, .init = sf_mem_init, .fini = sf_mem_fini){
  info *infoptr = sf_malloc(sizeof(info));
  int *a = sf_malloc(32);
  int *b = sf_malloc(4);
  memset(b, 0, 0);
  double max  = (sizeof(info) + 36);
  double tempmax = max;

  //1
  char* aptr = (char*)a;
  a = sf_realloc(a,sizeof(int));
  *a = 20;
  tempmax = tempmax -32 + 4;
  if(tempmax > max)max=tempmax;
  sf_header *header = (sf_header *)((char*)a - 8);
  cr_assert(header->splinter == 1, "(1)Splitner is not 1");
  cr_assert((char*)aptr == (char*)a, "(1)a is not same address");


  //2
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 3, "(2)allocatedBlocks not 3");
  cr_assert(infoptr->splinterBlocks == 1, "(2)splinterBlocks not 1");
  cr_assert(infoptr->padding == 24, "(2)padding not 24");
  cr_assert(infoptr->coalesces == 0, "(2)coalesces not 0");
  cr_assert(infoptr->splintering == 16, "(2)splintering not 16");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(2)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization,(max/4096.0));

  //3
  char* bptr = (char*)b -8;
  sf_free(b);
  tempmax = tempmax -4;
  if(tempmax > max)max=tempmax;
  sf_info(infoptr);
  cr_assert((char*)freelist_head == (char*)bptr, "(3)freelist_head did not coalesce with b");
  cr_assert(infoptr->allocatedBlocks == 2, "(3)allocatedBlocks not 2");
  cr_assert(infoptr->splinterBlocks == 1, "(3)splinterBlocks not 1");
  cr_assert(infoptr->padding == 12, "(3)padding not 12");
  cr_assert(infoptr->coalesces == 1, "(3)coalesces not 1");
  cr_assert(infoptr->splintering == 16, "(3)splintering not 16");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(3)peakMemoryUtilization: %f not %f",infoptr->peakMemoryUtilization ,(max/4096.0));

  //4
  b = sf_malloc(128);
  tempmax = tempmax + 128;
  if(tempmax > max)max=tempmax;
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 3, "(4)allocatedBlocks not 3");
  cr_assert(infoptr->splinterBlocks == 1, "(4)splinterBlocks not 1");
  cr_assert(infoptr->padding == 12, "(4)padding not 12");
  cr_assert(infoptr->coalesces == 1, "(4)coalesces not 1");
  cr_assert(infoptr->splintering == 16, "(4)splintering not 16");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(4)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));
  cr_assert(*a == 20, "(4)a is not 20");

  //5
  a = sf_realloc(a, 128);
  tempmax = tempmax + 128 -sizeof(int);
  if(tempmax > max)max=tempmax;

  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 3, "(5)allocatedBlocks not 3");
  cr_assert(infoptr->splinterBlocks == 0, "(5)splinterBlocks not 0");
  cr_assert(infoptr->padding == 0, "(5)padding not 0");
  cr_assert(infoptr->coalesces == 1, "(5)coalesces not 1");
  cr_assert(infoptr->splintering == 0, "(5)splintering not 0");
  cr_assert(*a == 20, "(5)a is not 20");
  cr_assert(freelist_head->next != NULL, "(5)freelist_head next is null");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(5)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));

  //6
  a = sf_realloc(a,32);
  tempmax = tempmax -128 + 32;
  if(tempmax > max)max=tempmax;
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 3, "(6)allocatedBlocks not 3");
  cr_assert(infoptr->splinterBlocks == 0, "(6)splinterBlocks not 0");
  cr_assert(infoptr->padding == 0, "(6)padding not 0");
  cr_assert(infoptr->coalesces == 2, "(6)coalesces not 2");
  cr_assert(infoptr->splintering == 0, "(6))splintering not 0");
  cr_assert(*a == 20, "(6)a is not 20");
  cr_assert(freelist_head->next != NULL, "(6)freelist_head next is null");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(6)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));

  //7
  int *f = sf_malloc(64);
  tempmax = tempmax + 64;
  if(tempmax > max)max=tempmax;
  memset(f,0,0);
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 4, "(7)allocatedBlocks not 4");
  cr_assert(infoptr->splinterBlocks == 0, "(7)splinterBlocks not 0");
  cr_assert(infoptr->padding == 0, "(7)padding not 0");
  cr_assert(infoptr->coalesces == 2, "(7)coalesces not 2");
  cr_assert(infoptr->splintering == 0, "(7)splintering not 0");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(7)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));

  //8
  a = sf_realloc(a,sizeof(int));
  tempmax = tempmax - 32 + sizeof(int);
  if(tempmax > max)max=tempmax;
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 4, "(8)allocatedBlocks not 4");
  cr_assert(infoptr->splinterBlocks == 1, "(8)splinterBlocks not 1");
  cr_assert(infoptr->padding == 12, "(8)padding not 12");
  cr_assert(infoptr->coalesces == 2, "(8)coalesces not 2");
  cr_assert(infoptr->splintering == 16, "(8)splintering not 16");
  cr_assert(*a == 20, "(8)a is not 20");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(8)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));

  //9
  a = sf_realloc(a,24);
  tempmax = tempmax - 4 + 24;
  if(tempmax > max)max=tempmax;
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 4, "(9)allocatedBlocks not 4");
  cr_assert(infoptr->splinterBlocks == 0, "(9)splinterBlocks not 0");
  cr_assert(infoptr->padding == 8, "(9)padding not 8");
  cr_assert(infoptr->coalesces == 2, "(9)coalesces not 2");
  cr_assert(infoptr->splintering == 0, "(9)splintering not 0");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(9)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));
  cr_assert(*a == 20, "(9)a is not 20");

  //10
  int *g = sf_malloc(32);
  int *h = sf_malloc(32);
  int *i = sf_malloc(sizeof(int));
  int *j = sf_malloc(sizeof(int));
  tempmax = tempmax +32 +32 +sizeof(int) + sizeof(int);
  if(tempmax > max)max=tempmax;

  g = sf_realloc(g, 16);
  tempmax = tempmax + 16 - 32;
  if(tempmax > max)max=tempmax;
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 8, "(10)allocatedBlocks not 8");
  cr_assert(infoptr->splinterBlocks == 1, "(10)splinterBlocks not 1");
  cr_assert(infoptr->padding == 32, "(10)padding not 32");
  cr_assert(infoptr->coalesces == 2, "(10)coalesces not 2");
  cr_assert(infoptr->splintering == 16, "(10)splintering not 16");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(10)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));

  //11
  sf_free(i);
  tempmax -= sizeof(int);
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 7, "(11)allocatedBlocks not 7");
  cr_assert(infoptr->splinterBlocks == 1, "(11)splinterBlocks not 1");
  cr_assert(infoptr->padding == 20, "(11)padding not 20");
  cr_assert(infoptr->coalesces == 2, "(11)coalesces not 2");
  cr_assert(infoptr->splintering == 16, "(11)splintering not 16");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(11)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));

  //12
  h = sf_realloc(h, 2);
  tempmax = tempmax + 2 - 32;
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 7, "(12)allocatedBlocks not 7");
  cr_assert(infoptr->splinterBlocks == 1, "(12)splinterBlocks not 1");
  cr_assert(infoptr->padding == 34, "(12)padding not 34");
  cr_assert(infoptr->coalesces == 3, "(12)coalesces not 3");
  cr_assert(infoptr->splintering == 16, "(12)splintering not 16");
  cr_assert(infoptr->peakMemoryUtilization == (max/4096.0), "(12)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization, (max/4096.0));

  memset(j,0,0);
}

Test(sf_memsuite, Peak_util, .init = sf_mem_init, .fini = sf_mem_fini){
  info *infoptr = sf_malloc(sizeof(info));

  int *g = sf_malloc(2000);
  sf_info(infoptr);
  cr_assert(infoptr->peakMemoryUtilization == (2048.0/4096.0), "(1)peakMemoryUtilization: %f not %f",infoptr->peakMemoryUtilization,(2048.0/4096.0));

  int *h = sf_malloc(1024);
  sf_info(infoptr);
  cr_assert(infoptr->peakMemoryUtilization == (3072.0/4096.0), "(2)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization,(3072.0/4096.0));
  int *i = sf_malloc(2048);
  sf_info(infoptr);
  cr_assert(infoptr->peakMemoryUtilization == (5120.0/8192.0), "(3)peakMemoryUtilization %f not %f",infoptr->peakMemoryUtilization,(5120.0/8192.0));

  memset(g, 0, 0);
  memset(h, 0, 0);
  memset(i, 0, 0);
}

Test(sf_memsuite, Splinters_and_padding, .init = sf_mem_init, .fini = sf_mem_fini){
  info *infoptr = sf_malloc(sizeof(info));

  double tempmax = 0;
  double max = 0;

  int* a = sf_malloc(32);
  int* b = sf_malloc(4);
  tempmax += 32+4+48;
  if(max<tempmax)max=tempmax;
  a = sf_realloc(a, 14);
  tempmax += 14 -32;
  if(max<tempmax)max=tempmax;

  //1
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 3, "(1)Allocated blocks: %d not 2",infoptr->allocatedBlocks);
  cr_assert(infoptr->peakMemoryUtilization == max/(4096.0), "(1)peakMemoryUtilization: %f not %f"
            ,infoptr->peakMemoryUtilization,max/(4096.0));
  cr_assert(infoptr->splinterBlocks == 1, "(1)splinterBlocks: %d not 1", infoptr->splinterBlocks);
  cr_assert(infoptr->padding == 14, "(1)padding not 14");
  cr_assert(infoptr->coalesces == 0, "(1)coalesces not 0");
  cr_assert(infoptr->splintering == 1*16, "(1)splintering not 1");

  int* c = sf_malloc(32);
  int* d = sf_malloc(4);
  memset(d,0,0);
  tempmax += 32+4;
  if(max<tempmax)max=tempmax;

  sf_free(c);
  tempmax += -32;
  if(max<tempmax)max=tempmax;

  c = sf_malloc(16);
  tempmax += 16;
  if(max<tempmax)max=tempmax;

  //2
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 5, "(2)Allocated blocks: %d not 5",infoptr->allocatedBlocks);
  cr_assert(infoptr->peakMemoryUtilization == max/(4096.0), "(2)peakMemoryUtilization: %f not %f"
            ,infoptr->peakMemoryUtilization,max/(4096.0));
  cr_assert(infoptr->splinterBlocks == 2, "(2)splinterBlocks not 2");
  cr_assert(infoptr->padding == 26, "(2)padding not 26");
  cr_assert(infoptr->coalesces == 0, "(2)coalesces not 0");
  cr_assert(infoptr->splintering == 2*16, "(2)splintering not %d",2*16);

  c = sf_realloc(c, 1024);
  tempmax += 1024 -16;
  if(max<tempmax)max=tempmax;

  sf_free(b);
  tempmax += -4;
  if(max<tempmax)max=tempmax;

  //3
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 4, "(3)Allocated blocks: %d not 4",infoptr->allocatedBlocks);
  cr_assert(infoptr->peakMemoryUtilization == max/(4096.0), "(3)peakMemoryUtilization: %f not %f"
            ,infoptr->peakMemoryUtilization,max/(4096.0));
  cr_assert(infoptr->splinterBlocks == 1, "(3)splinterBlocks not 1");
  cr_assert(infoptr->padding == 14, "(3)padding not 14");
  cr_assert(infoptr->coalesces == 1, "(3)coalesces not 1");
  cr_assert(infoptr->splintering == 1*16, "(3)splintering not %d",1*16);


  int* e = sf_malloc(32);
  int* f = sf_malloc(4);
  tempmax += 32 + 4;
  if(max<tempmax)max=tempmax;

  sf_free(f);
  tempmax += -4;
  if(max<tempmax)max=tempmax;

  sf_realloc(e, 16);
  tempmax += 16 - 32;
  if(max<tempmax)max=tempmax;

  //4
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 5, "(4)Allocated blocks: %d not 5",infoptr->allocatedBlocks);
  cr_assert(infoptr->peakMemoryUtilization == max/(4096.0), "(4)peakMemoryUtilization: %f not %f"
            ,infoptr->peakMemoryUtilization,max/(4096.0));
  cr_assert(infoptr->splinterBlocks == 1, "(4)splinterBlocks not 1");
  cr_assert(infoptr->padding == 14, "(4)padding not 14");
  cr_assert(infoptr->coalesces == 2, "(4)coalesces not 2");
  cr_assert(infoptr->splintering == 1*16, "(4)splintering not %d",1*16);

  int* g = sf_malloc(32);
  int* h = sf_malloc(4);
  int* i = sf_malloc(48);
  tempmax += 4 + 32 + 48;
  if(max<tempmax)max=tempmax;

  i = sf_realloc(i,4080);
  tempmax += -48 + 4080;
  if(max<tempmax)max=tempmax;

  g = sf_realloc(g, 1023);
  tempmax += - 32 + 1023;
  if(max<tempmax)max=tempmax;

  h = sf_realloc(h, 128);
  tempmax += -4 + 128;
  if(max<tempmax)max=tempmax;

  //5
  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 8, "(5)Allocated blocks: %d not 8",infoptr->allocatedBlocks);
  cr_assert(infoptr->peakMemoryUtilization == max/(2.0 *4096.0), "(5)peakMemoryUtilization: %f not %f"
            ,infoptr->peakMemoryUtilization,max/(2.0 * 4096.0));
  cr_assert(infoptr->splinterBlocks == 1, "(5)splinterBlocks not 1");
  cr_assert(infoptr->padding == 15, "(5)padding:%d not 15",infoptr->padding);
  cr_assert(infoptr->coalesces == 4, "(5)coalesces:%d not 4", infoptr->coalesces);
  cr_assert(infoptr->splintering == 1*16, "(5)splintering not %d",1*16);
}

Test(sf_memsuite, Bunch_of_mallocs, .init = sf_mem_init, .fini = sf_mem_fini){
  info *infoptr = sf_malloc(sizeof(info));
  char* ptr = (char*)freelist_head;
  for(int i = 0; i < 510; i ++){
    sf_malloc(16);
  }

  sf_info(infoptr);
  cr_assert(infoptr->allocatedBlocks == 511, "Allocated blocks not 511");
  cr_assert(infoptr->peakMemoryUtilization == ((510.0*16.0)+48.0)/(4.0*4096.0), "peakMemoryUtilization: %f not %f"
            ,infoptr->peakMemoryUtilization,((510.0*16.0)+48.0)/(4.0*4096.0));
  cr_assert(infoptr->splinterBlocks == 0, "splinterBlocks not 0");
  cr_assert(infoptr->padding == 0, "padding not 0");
  cr_assert(infoptr->coalesces == 0, "coalesces not 0");
  cr_assert(infoptr->splintering == 0, "splintering not 0");

  int *nomemory = sf_malloc(16);
  cr_assert(nomemory == NULL, "nomemory is not null");
  cr_assert(errno == ENOMEM, "errno is not ENOMEM");
  cr_assert(freelist_head == NULL, "freelist_head is not null");

  for(int i = 1; i < 510; i+=2){
    sf_free(ptr + (i*32)+ 8);
  }
  for(int i = 0; i < 510; i+=2){
    sf_free(ptr + (i*32)+ 8);
  }
  sf_free(infoptr);
  cr_assert(freelist_head->header.block_size<<4 == 4096*4, "freelist_head block size not 4096");
}

Test(sf_memsuite, Free_right_coalesce, .init = sf_mem_init, .fini = sf_mem_fini){
  int* a = sf_malloc(4);
  int* b = sf_malloc(4);
  int* c = sf_malloc(4);
  int* d = sf_malloc(4);
  int* e = sf_malloc(4);
  int* f = sf_malloc(4);

  sf_free(b);
  sf_free(a);

  cr_assert(freelist_head->header.block_size<<4 == 64, "freelist_head blocksize not 64");

  sf_free(d);
  sf_free(e);
  sf_free(c);
  sf_free(f);

  cr_assert(freelist_head->header.block_size<<4 == 4096, "freelist_head block size not 4096");
  cr_assert(freelist_head->prev == NULL, "freelist_head->prev not null");
  cr_assert(freelist_head->next == NULL, "freelist_head->next not null");

}

Test(sf_memsuite, Free_all_filo, .init = sf_mem_init, .fini = sf_mem_fini){
  int* a = sf_malloc(4080);
  int* b = sf_malloc(4080);
  int* c = sf_malloc(4080);
  int* d = sf_malloc(4080);

  sf_free(d);
  cr_assert(freelist_head->header.block_size<<4 == 4096*1, "freelist_head block size not 4096");

  sf_free(c);
  cr_assert(freelist_head->header.block_size<<4 == 4096*2, "freelist_head block size not 4096");

  sf_free(b);
  cr_assert(freelist_head->header.block_size<<4 == 4096*3, "freelist_head block size not 4096");

  sf_free(a);
  cr_assert(freelist_head->header.block_size<<4 == 4096*4, "freelist_head block size not 4096");

  cr_assert(freelist_head->prev == NULL, "freelist_head->prev not null");
  cr_assert(freelist_head->next == NULL, "freelist_head->next not null");
}

Test(sf_memsuite, Free_all_inner_first, .init = sf_mem_init, .fini = sf_mem_fini){
  int* a = sf_malloc(4080);
  int* b = sf_malloc(4080);
  int* c = sf_malloc(4080);
  int* d = sf_malloc(4080);

  sf_free(b);
  cr_assert(freelist_head->header.block_size<<4 == 4096*1, "freelist_head block size not 4096");

  sf_free(c);
  cr_assert(freelist_head->header.block_size<<4 == 4096*2, "freelist_head block size not 4096");

  sf_free(a);
  cr_assert(freelist_head->header.block_size<<4 == 4096*3, "freelist_head block size not 4096");

  sf_free(d);
  cr_assert(freelist_head->header.block_size<<4 == 4096*4, "freelist_head block size not 4096");

  cr_assert(freelist_head->prev == NULL, "freelist_head->prev not null");
  cr_assert(freelist_head->next == NULL, "freelist_head->next not null");
}

Test(sf_memsuite, Realloc_to_same_size, .init = sf_mem_init, .fini = sf_mem_fini){
  int* a = sf_malloc(48);
  sf_varprint(a);
  int* b = sf_malloc(4);
  a = sf_realloc(a, 48);
  sf_varprint(a);
  a = sf_realloc(a, 22);
  sf_varprint(a);
  memset(b,0,0);

}

Test(sf_memsuite, Free_all_fifo, .init = sf_mem_init, .fini = sf_mem_fini){
  int* a = sf_malloc(4080);
  int* b = sf_malloc(4080);
  int* c = sf_malloc(4080);
  int* d = sf_malloc(4080);

  sf_free(a);
  cr_assert(freelist_head->header.block_size<<4 == 4096*1, "freelist_head block size not 4096");

  sf_free(b);
  cr_assert(freelist_head->header.block_size<<4 == 4096*2, "freelist_head block size not 4096");

  sf_free(c);
  cr_assert(freelist_head->header.block_size<<4 == 4096*3, "freelist_head block size not 4096");

  sf_free(d);
  cr_assert(freelist_head->header.block_size<<4 == 4096*4, "freelist_head block size not 4096");

  cr_assert(freelist_head->prev == NULL, "freelist_head->prev not null");
  cr_assert(freelist_head->next == NULL, "freelist_head->next not null");
}

Test(sf_memsuite, Test_all_bad_cases, .init = sf_mem_init, .fini = sf_mem_fini){
  int emptyHeap = sf_info(NULL);
  cr_assert(emptyHeap == -1, "emptyHeap is not -1");

  int* toBig = sf_malloc(4096 * 4 - 15);
  cr_assert(toBig == NULL, "toBig is not NULL");
  cr_assert(errno == ENOMEM, "errno not ENOMEM");

  int* zeroMalloc = sf_malloc(0);
  cr_assert(zeroMalloc == NULL, "zeroMalloc is not NULL");
  cr_assert(errno == EINVAL, "errno not EINVAL");

  char* invalidRealloc = sf_malloc(16);
  invalidRealloc = sf_realloc(invalidRealloc + 1, 100);
  cr_assert(invalidRealloc == NULL, "invalidRealloc is not null");
  cr_assert(errno == EINVAL, "errno not EINVAL");

  char* toBigRealloc = sf_malloc(16);
  toBigRealloc = sf_realloc(toBigRealloc, 4096 * 4 - 15);
  cr_assert(toBigRealloc == NULL, "toBigRealloc is not NULL");
  cr_assert(errno == ENOMEM, "errno not ENOMEM");

  char* zeroRealloc = sf_malloc(16);
  zeroRealloc = sf_realloc(zeroRealloc, 0);
  cr_assert(zeroRealloc == NULL, "zeroRealloc is not null");

  char* invalidFree = sf_malloc(16);
  sf_free(invalidFree+1);
  cr_assert(errno == EINVAL, "ernno not EINVAL");
}
