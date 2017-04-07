/*
   Main program for the virtual memory project.
   Make all of your modifications to this file.
   You may add or rearrange any code or data as you need.
   The header files page_table.h and disk.h explain
   how to use the page table and disk interfaces.
   */

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
int changeFrame = 0;
int page_faults = 0;
int disk_reads = 0;
int disk_writes = 0;
const char *method;
int *frame_table;
char *physmem;
struct disk *disk;

void page_fault_handler( struct page_table *pt, int page )
{
  int tframe;
  int tbits;
  int full = 0;
  // Get the information associated with page
  page_table_get_entry(pt, page, &tframe, &tbits);
  page_faults++;
  // Basic functionality
  if(tbits == 0) {
    int i = 0;
    int frame_num = page_table_get_nframes(pt);
    for(i = 0; i < frame_num; i++) {
      if(frame_table[i] == -1) {
        frame_table[i] = page;
        disk_read(disk, page, &physmem[i*PAGE_SIZE]);
        page_table_set_entry(pt, page, i, PROT_READ); 
        disk_reads++;
        break;
      }
      if(i == frame_num - 1) {
        full = 1;
      }
    }
  }
  else if(tbits == PROT_READ) {//Set the page to have write permissions
    page_table_set_entry(pt, page, tframe, PROT_READ|PROT_WRITE);  
  }
  //page_table_print(pt);

  if(!strcmp(method,"rand") && full == 1) {
    srand(time(NULL));
    //srand(page_table_get_npages(pt)); 
    int replace = rand()%page_table_get_npages(pt);
    int rpframe;
    int rpbits;
    page_table_get_entry(pt, replace, &rpframe, &rpbits);
    while(rpbits == 0){
      replace = rand() % page_table_get_npages(pt);
      page_table_get_entry(pt, replace, &rpframe, &rpbits);
    }
    int ntframe;
    int ntbits;
    page_table_get_entry(pt, page, &ntframe, &ntbits);
    disk_write(disk, replace, &physmem[rpframe*PAGE_SIZE]);
    disk_read(disk, page, &physmem[rpframe*PAGE_SIZE]);
    disk_reads++;
    disk_writes++;
    page_table_set_entry(pt, page, rpframe, PROT_READ);  
    page_table_set_entry(pt, replace, 0, 0);

    //printf("page fault on page #%d\n",page);
  }
  else if(!strcmp(method, "fifo") && full == 1) {
    int ntframe;
    int ntbits;
    int kickPage = frame_table[changeFrame];
    page_table_get_entry(pt, kickPage, &ntframe, &ntbits);
    disk_write(disk, kickPage, &physmem[ntframe*PAGE_SIZE]);
    disk_read(disk, page, &physmem[ntframe*PAGE_SIZE]);
    page_table_set_entry(pt, page, ntframe, PROT_READ);  
    page_table_set_entry(pt, kickPage, 0, 0); 
    int num_frames = page_table_get_nframes(pt);
    frame_table[changeFrame] = page;
    //printf("Frames: %d\n", num_frames);
    changeFrame = (changeFrame+1)%num_frames;
    //page_table_set_entry(pt, page, page, PROT_READ|PROT_WRITE);
    //printf("page fault on page #%d\n",page);
    //page_table_print(pt);
    //printf("\n");
    disk_reads++;
    disk_writes++;
  }

  else if(!strcmp(method, "custom") && full == 1) {
    //find dirty pages
    int i = 0;
    int frame_num = page_table_get_nframes(pt);
    int kickpage; 
    int ntframe;
    int ntbits;
    int dirty_found = 0;
    for(i = 0; i < frame_num; i++) {
      //search for any dirty bits
      page_table_get_entry(pt, frame_table[i], &ntframe, &ntbits);
      if(ntbits == 3){
        kickpage = frame_table[i];
        dirty_found = 1;
        break;
      }

    }
    if(dirty_found == 1){
      //use dirty bits
      disk_write(disk, kickpage, &physmem[ntframe*PAGE_SIZE]);
      disk_read(disk, page, &physmem[ntframe*PAGE_SIZE]);
      page_table_set_entry(pt, page, ntframe, PROT_READ);  
      page_table_set_entry(pt, kickpage, 0, 0); 
      disk_reads++;
      disk_writes++;
    }
    else{
      //use rand if no dirty bits
      srand(time(NULL));
      //srand(page_table_get_npages(pt)); 
      int replace = rand()%page_table_get_npages(pt);
      int rpframe;
      int rpbits;
      page_table_get_entry(pt, replace, &rpframe, &rpbits);
      while(rpbits == 0){
        replace = rand() % page_table_get_npages(pt);
        page_table_get_entry(pt, replace, &rpframe, &rpbits);
      }
      int npframe;
      int npbits;
      page_table_get_entry(pt, page, &npframe, &npbits);
      disk_write(disk, replace, &physmem[rpframe*PAGE_SIZE]);
      disk_read(disk, page, &physmem[rpframe*PAGE_SIZE]);
      disk_reads++;
      disk_writes++;
      page_table_set_entry(pt, page, rpframe, PROT_READ);  
      page_table_set_entry(pt, replace, 0, 0);
      
      
    }

    //page_table_set_entry(pt, page, page, PROT_READ|PROT_WRITE);
    //printf("page fault on page #%d\n",page);

  }

  else if(full == 1) {
    printf("Page_handler not found\n");
    exit(1);
  }

}
int isNumber(char number[]){
  int i=0;
  for (; number[i]!=0; i++){
    if(!isdigit(number[i]))
      return 0;
  }
  return 1;
}
int main( int argc, char *argv[] )
{
  if(argc!=5) {
    printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
    return 1;
  }
  if(!isNumber(argv[1])){
    printf("Error: npages must be a positive integer\n");
    return 1;

  }
  if(!isNumber(argv[2])){
    printf("Error: nframes must be a positive integer\n");
    return 1;

  }
  int npages = atoi(argv[1]);
  int nframes = atoi(argv[2]);
  method = argv[3];
  const char *program = argv[4];

  //frame_table stuff
  frame_table = malloc(sizeof(int)*nframes);
  if(frame_table == NULL){
    fprintf(stderr, "unable to malloc: %s\n", strerror(errno));
    return 1;
  }
  else{
    int i;
    for(i = 0; i < nframes; i++) {
      frame_table[i] = -1;
    }
  }

  disk = disk_open("myvirtualdisk",npages);
  if(!disk) {
    fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
    return 1;
  }


  struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
  if(!pt) {
    fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
    return 1;
  }

  char *virtmem = page_table_get_virtmem(pt);

  physmem = page_table_get_physmem(pt);

  if(!strcmp(program,"sort")) {
    sort_program(virtmem,npages*PAGE_SIZE);

  } else if(!strcmp(program,"scan")) {
    scan_program(virtmem,npages*PAGE_SIZE);

  } else if(!strcmp(program,"focus")) {
    focus_program(virtmem,npages*PAGE_SIZE);

  } else {
    fprintf(stderr,"unknown program: %s\n",argv[3]);
    return 1;
  }

  printf("Page Faults: %d\n", page_faults);
  printf("Disk Reads: %d\n", disk_reads);
  printf("Disk Writes: %d\n", disk_writes);

  page_table_delete(pt);
  disk_close(disk);

  return 0;
}
