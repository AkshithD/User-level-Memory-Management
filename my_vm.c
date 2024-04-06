#include "my_vm.h"

//TODO: Define static variables and structs, include headers, etc.
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PGSIZE 4096 //4KB
int initialized = 0;
int page_offset_bits;
int page_table_bits;
int page_directory_bits;
void *physical_memory;
char *physical_memory_bitmap;
char *virtual_memory_bitmap;
int num_frames = MEMSIZE / PGSIZE;
int tlb_hits = 0;
int tlb_misses = 0;
typedef struct {
    unsigned int frame_number; // Physical frame numbers
    unsigned int size; // size of the memory block
} page_table_entry;

// Page Directory Entry Structure
typedef struct {
    page_table_entry *page_table; // Pointer to page table
} page_directory_entry;

typedef struct {
    unsigned int vp; // Virtual page number
    unsigned int pp; // Physical page number
} TLB_entry;

TLB_entry * TLB;

// Global Page Directory
page_directory_entry *page_directory;

void set_bit_at_index(char *bitmap, int index);
int get_bit_at_index(char *bitmap, int index);
int find_free_pages_in_virtual_memory(int num_pages, unsigned int *start_vp);

void set_physical_mem(){
    physical_memory = (void *)malloc(PGSIZE * num_frames);
    memset(physical_memory, 0, PGSIZE * num_frames);

    // 1 bit per frame bitmap for physical memory
    physical_memory_bitmap = (char *)malloc(num_frames / 8);
    memset(physical_memory_bitmap, 0, num_frames / 8);
    

    // Calculate bits required for page offset
    page_offset_bits = (int)log2(PGSIZE);
    
    // Calculate bits for page directory and page table
    int rest = 32 - page_offset_bits;
    int page_entries_in_page = PGSIZE / sizeof(page_table_entry);
    page_table_bits = (int)log2(page_entries_in_page);
    page_directory_bits = rest - page_table_bits;
    int frames_needed_for_page_table = 1 << page_directory_bits;

    // Allocate memory for the page directory from my physical memory
    page_directory = (page_directory_entry *)physical_memory;
    memset(page_directory, 0, PGSIZE);
    set_bit_at_index(physical_memory_bitmap, 0);

    // Calculate the number of entries in the page directory and page tables
    int num_page_directory_entries = 1 << page_directory_bits;
    int num_page_table_entries = 1 << page_table_bits;

    virtual_memory_bitmap = (char *)malloc(num_page_directory_entries * num_page_table_entries / 8);
    memset(virtual_memory_bitmap, 0, num_page_directory_entries * num_page_table_entries / 8);
    set_bit_at_index(virtual_memory_bitmap, 0);

    //init tlb
    TLB = (TLB_entry *)malloc(sizeof(TLB_entry) * TLB_ENTRIES);
}

page_table_entry * get_page_table(page_directory_entry *pd){
    if (pd->page_table == NULL) {
        for (int i = 0; i < num_frames; i++) {
            if (get_bit_at_index(physical_memory_bitmap, i) == 0) {
                pd->page_table = (page_table_entry *)(physical_memory + i * PGSIZE);
                memset(pd->page_table, 0, PGSIZE);
                set_bit_at_index(physical_memory_bitmap, i);
                break;
            }
        }
    }
    return pd->page_table;
}

void * translate(unsigned int vp){
    //check TLB
    if (check_TLB(vp) == 0) {
        return (physical_memory + TLB[vp % TLB_ENTRIES].pp);
    }

    //get page table index
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);

    if (get_bit_at_index(virtual_memory_bitmap, page_directory_index * (1 << page_table_bits) + page_table_index) == 0) {
        return NULL; // vp not mapped
    }
    //get frame numberhead
    unsigned int frame_number = get_page_table(&page_directory[page_directory_index])[page_table_index].frame_number;
    //get physical address
    unsigned int physical_address = (frame_number * PGSIZE) + page_offset;
    //add to TLB
    add_TLB(vp, physical_address);
    return (physical_memory + physical_address);
}

unsigned int page_map(unsigned int vp, int size){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);

    for (int i = 1; i < num_frames; i++) { //start from 1 physical address not first frame
        if (get_bit_at_index(physical_memory_bitmap, i) == 0){
            set_bit_at_index(physical_memory_bitmap, i);
            page_table_entry *page_table = get_page_table(&page_directory[page_directory_index]);
            page_table[page_table_index].frame_number = i;
            printf("physical frame address: %p at frame: %d\n", &physical_memory +page_table[page_table_index].frame_number * PGSIZE, page_table[page_table_index].frame_number);
            page_table[page_table_index].size = size;
            set_bit_at_index(virtual_memory_bitmap, page_directory_index * (1 << page_table_bits) + page_table_index);
            return 0;
        }
    }
    return 1; // Return 1 if there is no free physical memory
}

int find_free_pages_in_virtual_memory(int num_pages, unsigned int *start_vp) {
    //find continuous free pages in virtual memory
    int pages_found = 0;
    unsigned int first_fit = 0;
    for (int i = 0; i < (1 << page_directory_bits); i++) {
        for (int j = 0; j < (1 << page_table_bits); j++) {
            if (get_bit_at_index(virtual_memory_bitmap, i * (1 << page_table_bits) + j) == 0) {
                if (pages_found == 0) {
                    first_fit = (i << (page_offset_bits + page_table_bits)) | (j << page_offset_bits);
                }
                pages_found++;
                if (pages_found == num_pages) {
                    *start_vp = first_fit;
                    return 1;
                }
            } else {
                pages_found = 0;
            }
        }
    }
    return 0;
}

void * t_malloc(size_t n) {
    //TODO: Finish
    if (initialized == 0) {
        set_physical_mem();
        initialized = 1;
    }
    
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); 
    // make start_vp = first vp of the free pages in the if statement if its not null
    unsigned int start_vp;
    if (find_free_pages_in_virtual_memory(num_pages, &start_vp) == 0) {
        return NULL; // Return NULL if there are not enough free pages
    }

    unsigned int output_vp = start_vp;
    int ret;
    for (int i = 0; i < num_pages; i++) {
        ret = page_map(start_vp, num_pages);
        if (ret == 1) {
            return NULL; // Return NULL if the page is already mapped
        }
        start_vp += PGSIZE;
    }
    unsigned int temp_vp = output_vp;
    add_TLB(output_vp, page_directory[temp_vp >> (page_offset_bits + page_table_bits)].page_table[temp_vp >> page_offset_bits].frame_number * PGSIZE);

    for (int i = 0; i < num_frames; i++) {
        if (get_bit_at_index(physical_memory_bitmap, i) == 1) {
            printf("Physical memory bitmap at index %d: %d at %p\n", i, get_bit_at_index(physical_memory_bitmap, i), &physical_memory + i * PGSIZE);
        }
    }
    
    printf("Allocated %d pages starting from vp: %d\n", num_pages, output_vp);
    return (void *)output_vp;
}

int t_free(unsigned int vp, size_t n){ // What do we do if the give a vp in the middle and dont free till end? so theres a gap in the mem block
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);
    if (get_bit_at_index(virtual_memory_bitmap, page_directory_index * (1 << page_table_bits) + page_table_index) == 0) {
        return -1; // Return -1 if the virtual address is not mapped
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages to free (round up). gotta double check how to handle this

    if (get_page_table(&page_directory[page_directory_index])[page_table_index].size < num_pages) {
        return -1; // Return -1 if the size is greater than the allocated size
    }else if (get_page_table(&page_directory[page_directory_index])[page_table_index].size == num_pages) { // if equal then free the whole mem block
        get_page_table(&page_directory[page_directory_index])[page_table_index].size = 0;
    }else{ // if less than, adjust the size
        get_page_table(&page_directory[page_directory_index])[page_table_index].size -= num_pages;
    }

    // Free the pages
    for (int i = 0; i < num_pages; i++) {
        if (get_bit_at_index(virtual_memory_bitmap, page_directory_index * (1 << page_table_bits) + page_table_index) == 1) {
            set_bit_at_index(virtual_memory_bitmap, page_directory_index * (1 << page_table_bits) + page_table_index);
            set_bit_at_index(physical_memory_bitmap, get_page_table(&page_directory[page_directory_index])[page_table_index].frame_number);
            vp += PGSIZE;
            page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
            page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
        } else {
            return -1; // Return -1 if the page is not valid
        }
    }
    return 0; // Return 0 if the pages are freed successfully
}

int put_value(unsigned int vp, void *val, size_t n){ // does vp have to be page alligned
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);
    if (get_bit_at_index(virtual_memory_bitmap, page_directory_index * (1 << page_table_bits) + page_table_index) == 0) {
        return -1; // vp not mapped
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages (round up)
    if (val == NULL) {
        return -1; // Return -1 if the value is NULL
    }
    if (get_page_table(&page_directory[page_directory_index])[page_table_index].size < num_pages) {
        return -1; // Return -1 if the size is greater than the allocated size
    }
    size_t bytes_left = n;
    for (int i = 0; i < num_pages; i++) {
        void* physical_address = translate(vp);
        if (physical_address == NULL) {
            return -1; // vp not mapped
        }
        size_t bytes_to_copy = bytes_left < PGSIZE ? bytes_left : PGSIZE;
        memcpy(physical_address, (void *) val, bytes_to_copy);
        bytes_left -= bytes_to_copy;
        vp += bytes_to_copy;
        val = (void*)((char*)val + bytes_to_copy);
    }
    return 0;
}

int get_value(unsigned int vp, void *dst, size_t n){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);
    if (get_bit_at_index(virtual_memory_bitmap, page_directory_index * (1 << page_table_bits) + page_table_index) == 0) {
        return -1; // vp not mapped
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages (round up)
    if (dst == NULL) {
        return -1; // Return -1 if the value is NULL
    }
    if (get_page_table(&page_directory[page_directory_index])[page_table_index].size < num_pages) {
        return -1; // Return -1 if the size is greater than the allocated size
    }
    size_t bytes_left = n;
    void *dst_start = &dst;
    for (int i = 0; i < num_pages; i++) {
        void* physical_address = translate(vp);
        if (physical_address == NULL) {
            return -1; // vp not mapped
        }
        size_t bytes_to_copy = bytes_left < PGSIZE ? bytes_left : PGSIZE;
        memcpy(dst, *(&physical_address), bytes_to_copy);
        bytes_left -= bytes_to_copy;
        vp += bytes_to_copy;
        dst = (void*)((char*)dst + bytes_to_copy);
    }
    dst = dst_start;
    return 0;
}

void mat_mult(unsigned int a, unsigned int b, unsigned int c, size_t l, size_t m, size_t n){
    //very basic implementation so far.
    unsigned int page_directory_index_a = (a >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index_a = (a >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset_a = a & ((1 << page_offset_bits) - 1);
    unsigned int page_directory_index_b = (b >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index_b = (b >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset_b = b & ((1 << page_offset_bits) - 1);
    unsigned int page_directory_index_c = (c >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index_c = (c >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset_c = c & ((1 << page_offset_bits) - 1);
    if (get_bit_at_index(virtual_memory_bitmap, page_directory_index_a * (1 << page_table_bits) + page_table_index_a) == 0 || get_bit_at_index(virtual_memory_bitmap, page_directory_index_b * (1 << page_table_bits) + page_table_index_b) == 0 || get_bit_at_index(virtual_memory_bitmap, page_directory_index_c * (1 << page_table_bits) + page_table_index_c) == 0) {
        return; // vp not mapped
    }

    for (int i = 0; i < l; i++) {
        for (int j = 0; j < n; j++) {
            int sum = 0;
            for (int k = 0; k < m; k++) {
                int a_val;
                if(get_value(a + (i * m + k) * sizeof(int), &a_val, sizeof(int)) == -1){
                    return;
                }
                int b_val;
                if(get_value(b + (k * n + j) * sizeof(int), &b_val, sizeof(int)) == -1){
                    return;
                }
                sum += a_val * b_val;
            }
            if(put_value(c + (i * n + j) * sizeof(int), &sum, sizeof(int)) == -1){
                return;
            }
        }
    }
}

void add_TLB(unsigned int vpage, unsigned int ppage){
    //TODO: Finish
    TLB[vpage % TLB_ENTRIES].vp = vpage;
    TLB[vpage % TLB_ENTRIES].pp = ppage;
}

int check_TLB(unsigned int vpage){
    //TODO: Finish
    if (TLB[vpage % TLB_ENTRIES].vp == vpage) {
        tlb_hits++;
        return 0;
    }
    tlb_misses++;
    return -1;
}

void print_TLB_missrate(){
    //TODO: Finish
    if (tlb_hits + tlb_misses == 0) {
        printf("TLB miss rate: 0.00\n");
    } else {
        printf("TLB miss rate: %.2f\n", (double)tlb_misses / (tlb_hits + tlb_misses));
    }
}

/*
 * SETTING A BIT AT AN INDEX
 * Function to set a bit at "index" bitmap
 */
void set_bit_at_index(char *bitmap, int index)
{
    int byteIndex = index / 8;
    int bitIndex = index % 8;
    bitmap[byteIndex] |= (1 << bitIndex);
}

/*
 * GETTING A BIT AT AN INDEX
 * Function to get a bit at "index"
 */
int get_bit_at_index(char *bitmap, int index)
{
    //Get to the location in the character bitmap array
    //Implement your code here
    int byteIndex = index / 8;
    int bitIndex = index % 8;
    return (bitmap[byteIndex] & (1 << bitIndex)) != 0; 
}
