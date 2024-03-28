#include "my_vm.h"

//TODO: Define static variables and structs, include headers, etc.
#include <stdio.h>
#include <math.h>

#define PGSIZE 4096 //4KB
int initialized = 0;
int page_offset_bits;
int page_table_bits;
int page_directory_bits;
char *physical_memory;
char *physical_memory_bitmap;
unsigned int mem_block = 0;
typedef struct {
    unsigned int frame_number; // Physical frame numbers
    int valid; // mapped or not
    int present; // in memory or not
    unsigned int mem_block; // memory block number
} page_table_entry;

// Page Directory Entry Structure
typedef struct {
    page_table_entry *page_table; // Pointer to page table
} page_directory_entry;

// Global Page Directory
page_directory_entry *page_directory;

typedef struct {
    unsigned int vp;
    char *page_content;
    unsigned int mem_block;
    int size;
    evicted_page *next;
} evicted_page;

evicted_page *evicted_pages_head = NULL;
evicted_page *evicted_pages_tail = NULL;

typedef struct {
    unsigned int vp;
    unsigned int mem_block;
    int size;
    memory_frame *next;
} memory_frame;

memory_frame *memory_frames_head = NULL;
memory_frame *memory_frames_tail = NULL;

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

        // Initialize page table entries
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
    return -1; // no free frame so page replacement needed
}

unsigned int page_map(unsigned int vp){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);

    // Check if the page table entry is valid
    if (allocate_frame(page_directory_index, page_table_index) == 0){
        return 0;
    }
    return 1;
}

unsigned int find_free_pages_in_virtual_memory(int num_pages) {
    //find continuous free pages in virtual memory
    int pages_found = 0;
    unsigned int first_fit = 0;
    for (int i = 0; i < (1 << page_directory_bits); i++) {
        for (int j = 0; j < (1 << page_table_bits); j++) {
            if (page_directory[i].page_table[j].valid == 0) {
                if (pages_found == 0) {
                    first_fit = (i << (page_offset_bits + page_table_bits)) | (j << page_offset_bits);
                }
                pages_found++;
                if (pages_found == num_pages) {
                    return first_fit;
                }
            } else {
                pages_found = 0;
            }
        }
    }
    return NULL;
}

void * t_malloc(size_t n) {
    //TODO: Finish
    if (initialized == 0) {
        set_physical_mem();
        initialized = 1;
    }
    int num_pages = n / PGSIZE;
    int ret;
    memory_frame *curr = (memory_frame *)malloc(sizeof(memory_frame));
    curr->mem_block = mem_block;
    mem_block++;
    curr->next = NULL;
    if (memory_frames_head == NULL) {
        memory_frames_head = curr;
        memory_frames_tail = curr;
    } else {
        memory_frames_tail->next = curr;
        memory_frames_tail = curr;
    }

    unsigned int start_vp = find_free_pages_in_virtual_memory(num_pages);
    unsigned int output_vp = start_vp;
    if (start_vp == NULL) {
        return NULL; // no free space in virtual memory
    }
    for (int i = 0; i < num_pages; i++) {
        ret = page_map(start_vp);
        if (ret == 1) {
            return NULL; // no free frames in physical memory so page replacement needed
        }
        start_vp += PGSIZE;
    }
    return (void *)output_vp;
}

int t_free(unsigned int vp, size_t n){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);
    if (page_offset != 0) {
        return -1; // Return -1 if the virtual address is not page-aligned
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages to free (round up). gotta double check how to handle this

    if(page_directory[page_directory_index].page_table[page_table_index].present == 1){
        // remove the mem block from the linked list
        memory_frame *curr = memory_frames_head;
        memory_frame *prev = NULL;
        while (curr != NULL) {
            if (curr->mem_block == page_directory[page_directory_index].page_table[page_table_index].mem_block) {
                if (prev == NULL) {
                    memory_frames_head = curr->next;
                } else {
                    prev->next = curr->next;
                }
                free(curr); // what if the whole batch is not freed and only part of it is freed? gotta double check
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }else{
        // check in waiting linked list and free the mem block
    }
    for (int i = 0; i < num_pages; i++) {
        if (page_directory[page_directory_index].page_table[page_table_index].valid == 1) {
            page_directory[page_directory_index].page_table[page_table_index].valid = 0;
            page_directory[page_directory_index].page_table[page_table_index].present = 0;
            set_bit_at_index(physical_memory_bitmap, page_directory[page_directory_index].page_table[page_table_index].frame_number);
            page_directory[page_directory_index].page_table[page_table_index].frame_number = NULL;
            vp += PGSIZE;
            page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
            page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
        } else {
            return -1; // Return -1 if the page is not valid
        }
    }
    return 0; // Return 0 if the pages are freed successfully
}

int put_value(unsigned int vp, void *val, size_t n){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    if (page_directory[page_directory_index].page_table[page_table_index].valid == 0) {
        return -1; // vp not mapped
    }
    if (page_directory[page_directory_index].page_table[page_table_index].present == 0) {
        // page fault. get it physical mem
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages to free (round up)
    for (int i = 0; i < num_pages; i++) {
        unsigned int physical_address = translate(vp);
        if (physical_address == NULL) {
            return -1; // vp not mapped
        }
        memcpy(physical_memory + physical_address, val, n);
        vp += PGSIZE;
    }
    return 0;
}

int get_value(unsigned int vp, void *dst, size_t n){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    if (page_directory[page_directory_index].page_table[page_table_index].valid == 0) {
        return -1; // vp not mapped
    }
    if (page_directory[page_directory_index].page_table[page_table_index].present == 0) {
        // page fault. get it physical mem
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages to free (round up)
    for (int i = 0; i < num_pages; i++) {
        unsigned int physical_address = translate(vp);
        if (physical_address == NULL) {
            return -1; // vp not mapped
        }
        memcpy(dst, physical_memory + physical_address, n);
        vp += PGSIZE;
    }
    return 0;
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
