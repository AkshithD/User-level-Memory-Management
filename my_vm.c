#include "my_vm.h"

//TODO: Define static variables and structs, include headers, etc.
#include <stdio.h>
#include <math.h>

#define PGSIZE 4096 //4KB
int page_offset_bits;
int page_table_bits;
int page_directory_bits;
char *physical_memory;
char *physical_memory_bitmap;
typedef struct {
    unsigned int frame_number; // Physical frame numbers
    int valid; // mapped or not
    int present; // in memory or not
} page_table_entry;

// Page Directory Entry Structure
typedef struct {
    page_table_entry *page_table; // Pointer to page table
} page_directory_entry;

// Global Page Directory
page_directory_entry *page_directory;

/*
 * SETTING A BIT AT AN INDEX
 * Function to set a bit at "index" bitmap
 */
static void set_bit_at_index(char *bitmap, int index)
{
    int byteIndex = index / 8;
    int bitIndex = index % 8;
    bitmap[byteIndex] |= (1 << bitIndex);
}


/*
 * GETTING A BIT AT AN INDEX
 * Function to get a bit at "index"
 */
static int get_bit_at_index(char *bitmap, int index)
{
    //Get to the location in the character bitmap array
    //Implement your code here
    int byteIndex = index / 8;
    int bitIndex = index % 8;
    return (bitmap[byteIndex] & (1 << bitIndex)) != 0;
}

void set_physical_mem(){
    int num_frames = MEMSIZE / PGSIZE;
    physical_memory = (char *)malloc(MEMSIZE);
    memset(physical_memory, 0, MEMSIZE);

    // 1 bit per frame bitmap for physical memory
    physical_memory_bitmap = (char *)malloc(num_frames / 8);
    memset(physical_memory_bitmap, 0, num_frames / 8);

    // Calculate bits required for page offset
    page_offset_bits = (int)log2(PGSIZE);
    
    // Calculate bits for page directory and page table
    int rest = 32 - page_offset_bits;
    page_directory_bits = (rest + 1) / 2;
    page_table_bits = rest / 2;

    // Calculate the number of entries in the page directory and page tables
    int num_page_directory_entries = 1 << page_directory_bits;
    int num_page_table_entries = 1 << page_table_bits;

    // Allocate memory for the page directory
    page_directory = (page_directory_entry *)malloc(num_page_directory_entries * sizeof(page_directory_entry));

    // Allocate memory for each page table and initialize page table entries
    for (int i = 0; i < num_page_directory_entries; i++) {
        page_directory[i].page_table = (page_table_entry *)malloc(num_page_table_entries * sizeof(page_table_entry));

        // Initialize page table entries (frame numbers)
        for (int j = 0; j < num_page_table_entries; j++) {
            page_directory[i].page_table[j].frame_number = NULL; 
            page_directory[i].page_table[j].valid = 0;
            page_directory[i].page_table[j].present = 0;
        }
    }
}

void * translate(unsigned int vp){
    //get page table index
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);

    //get frame number
    unsigned int frame_number = page_directory[page_directory_index].page_table[page_table_index].frame_number;
    if (frame_number == NULL) {
        return NULL; // vp not mapped
    }
    //get physical address
    unsigned int physical_address = (frame_number * PGSIZE) + page_offset;
    return physical_address;
}

int allocate_frame(unsigned int page_directory_index, unsigned int page_table_index){
    // Find a free frame
    int num_frames = MEMSIZE / PGSIZE;
    for (int i = 0; i < num_frames; i++) {
        if (get_bit_at_index(physical_memory_bitmap, i) == 0){
            set_bit_at_index(physical_memory_bitmap, i);
            page_directory[page_directory_index].page_table[page_table_index].frame_number = i;
            page_directory[page_directory_index].page_table[page_table_index].present = 1;
            page_directory[page_directory_index].page_table[page_table_index].valid = 1;
            return 0;
        }
    }
    return -1; // No free frame found
}

unsigned int page_map(unsigned int vp){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);

    // Check if the page table entry is valid
    if (page_directory[page_directory_index].page_table[page_table_index].valid == 0){
        if (allocate_frame(page_directory_index, page_table_index) == 0){
            return 0;
        } else {
            return 2;
        }
    }
    return 1;
}

void * t_malloc(size_t n) {
    //TODO: Finish
    unsigned int vp = 0; // Initialize virtual address
    int ret = 0;
    for (int i = 0; i < (1 << page_directory_bits); i++) {
        for (int j = 0; j < (1 << page_table_bits); j++) {
            if (page_directory[i].page_table[j].valid == 0) {
                vp = (i << (page_offset_bits + page_table_bits)) | (j << page_offset_bits); // Create new virtual address
                ret = page_map(vp); // Map the virtual address to physical address
                if (ret == 0) {
                    return (void *)vp; // Return the virtual address as a pointer
                } else if (ret == 2) {
                    return NULL; // Return NULL if no free frame is found
                }
            }
        }
    }
    return NULL; // Return NULL if no free space is found
}

int t_free(unsigned int vp, size_t n){
    //TODO: Finish
}

int put_value(unsigned int vp, void *val, size_t n){
    //TODO: Finish
}

int get_value(unsigned int vp, void *dst, size_t n){
    //TODO: Finish
}

void mat_mult(unsigned int a, unsigned int b, unsigned int c, size_t l, size_t m, size_t n){
    //TODO: Finish
}

void add_TLB(unsigned int vpage, unsigned int ppage){
    //TODO: Finish
}

int check_TLB(unsigned int vpage){
    //TODO: Finish
}

void print_TLB_missrate(){
    //TODO: Finish
}
