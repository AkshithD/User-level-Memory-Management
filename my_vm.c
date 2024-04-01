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
int num_frames = 4;
int num_frames_free = 0;
unsigned int mem_block = 0;
typedef struct {
    unsigned int frame_number; // Physical frame numbers
    int valid; // mapped or not
    int present; // in memory or not
    unsigned int mem_block; // memory block number
    int size; // size of the memory block
} page_table_entry;

// Page Directory Entry Structure
typedef struct {
    page_table_entry *page_table; // Pointer to page table
} page_directory_entry;

// Global Page Directory
page_directory_entry *page_directory;

typedef struct evicted_page{
    unsigned int vp;
    char *page_content;
    unsigned int mem_block;
    int size;
    struct evicted_page *next;
} evicted_page;

evicted_page *evicted_pages_head = NULL;
evicted_page *evicted_pages_tail = NULL;

typedef struct memory_frame{
    unsigned int vp;
    unsigned int mem_block;
    int size;
    struct memory_frame *next;
} memory_frame;

memory_frame *memory_frames_head = NULL;
memory_frame *memory_frames_tail = NULL;

static void set_bit_at_index(char *bitmap, int index);
static int get_bit_at_index(char *bitmap, int index);
static int allocate_frame(unsigned int page_directory_index, unsigned int page_table_index, int size);
static int allocate_mem_block(unsigned int vp, int size, evicted_page *curr);
static int evict_first_mem_block();
static int page_fault_handler(unsigned int vp, int size, int malloc_flag);
static int find_free_pages_in_virtual_memory(int num_pages, unsigned int *start_vp);

void set_physical_mem(){
    physical_memory = (void *)malloc(PGSIZE * num_frames);
    memset(physical_memory, 0, PGSIZE * num_frames);

    // 1 bit per frame bitmap for physical memory
    physical_memory_bitmap = (char *)malloc(num_frames / 8);
    memset(physical_memory_bitmap, 0, num_frames / 8);

    num_frames_free = num_frames;

    // Calculate bits required for page offset
    page_offset_bits = (int)log2(PGSIZE);
    
    // Calculate bits for page directory and page table
    int rest = 32 - page_offset_bits;
    page_table_bits = rest / 2;
    page_directory_bits = rest - page_table_bits;

    // Calculate the number of entries in the page directory and page tables
    int num_page_directory_entries = 1 << page_directory_bits;
    int num_page_table_entries = 1 << page_table_bits;
    // Allocate memory for the page directory
    page_directory = (page_directory_entry *)malloc(num_page_directory_entries * sizeof(page_directory_entry));
    if (page_directory == NULL) {
        printf("Failed to allocate memory for page directory\n");
        exit(1);
    }
    // Allocate memory for each page table and initialize page table entries
    for (int i = 0; i < num_page_directory_entries; i++) {
        page_directory[i].page_table = (page_table_entry *)malloc(num_page_table_entries * sizeof(page_table_entry));
        if (page_directory[i].page_table == NULL) {
            printf("Failed to allocate memory for page table\n");
            exit(1);
        }
        // Initialize page table entries
        for (int j = 0; j < num_page_table_entries; j++) {
            page_directory[i].page_table[j].valid = 0;
            page_directory[i].page_table[j].present = 0;
        }
    }
    page_directory[0].page_table[0].valid = 1;
}

void * translate(unsigned int vp){
    //get page table index
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);

    if (page_directory[page_directory_index].page_table[page_table_index].valid == 0) {
        return NULL; // vp not mapped
    }
    //get frame numberhead
    unsigned int frame_number = page_directory[page_directory_index].page_table[page_table_index].frame_number;
    //get physical address
    unsigned int physical_address = (frame_number * PGSIZE) + page_offset;
    return &physical_memory + physical_address;
}

int allocate_mem_block(unsigned int vp, int size, evicted_page *curr){
    // allocate physical mem for vp
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    for (int i = 0; i < size; i++) {
        if (allocate_frame(page_directory_index, page_table_index, curr->size) == -1) {
            return -1; // no free frame so page replacement needed
        }
        vp += PGSIZE;
    }
    // copy the content of the evicted page to the physical memory
    void * physical_address = translate(vp);
    if (physical_address == NULL) {
        return -1; // vp not mapped
    }
    // set up new node
    memory_frame *new_curr = (memory_frame *)malloc(sizeof(memory_frame));
    memcpy(physical_address, curr->page_content, curr->size);
    new_curr->vp = curr->vp;
    new_curr->mem_block = curr->mem_block;
    new_curr->size = curr->size;
    new_curr->next = NULL;
    // add the new mem block to the linked list
    if (memory_frames_head == NULL) {
        memory_frames_head = new_curr;
        memory_frames_tail = new_curr;
    } else {
        memory_frames_tail->next = new_curr;
        memory_frames_tail = new_curr;
    }
    return 0;
}

int evict_first_mem_block(){
    // get head of mem block to replaced
    memory_frame *evicting_node = memory_frames_head;
    memory_frames_head = memory_frames_head->next;
    if (memory_frames_head == NULL) {
        memory_frames_tail = NULL;
    }
    // get page directory and page table index of mem block being replaced
    unsigned int page_directory_index = (evicting_node->vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (evicting_node->vp >> page_offset_bits) & ((1 << page_table_bits) - 1);

    // physical address of the replaced mem block
    void *physical_address = translate(evicting_node->vp);
    if (physical_address == NULL) {
        return -1; // vp not mapped
    }
    evicted_page *curr = (evicted_page *)malloc(sizeof(evicted_page)); // create a new evicted page and set its values
    curr->vp = evicting_node->vp;
    curr->page_content = malloc(evicting_node->size); // copy the content of the replaced mem block
    memcpy(curr->page_content, physical_address, evicting_node->size);
    curr->mem_block = evicting_node->mem_block;
    curr->size = evicting_node->size;
    curr->next = NULL;
    if (evicted_pages_head == NULL) { // add the evicted page to the evicted ll
        evicted_pages_head = curr;
        evicted_pages_tail = curr;
    } else {
        evicted_pages_tail->next = curr;
        evicted_pages_tail = curr;
    }
    // free the mem block
    free(evicting_node);

    // clean the page table entries and physical mem of the evicted mem block
    unsigned int clean_vp = curr->vp;
    for (int i = 0; i < curr->size; i++) {
        page_directory[page_directory_index].page_table[page_table_index].present = 0;
        set_bit_at_index(physical_memory_bitmap, page_directory[page_directory_index].page_table[page_table_index].frame_number);
        clean_vp += PGSIZE;
        page_directory_index = (clean_vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
        page_table_index = (clean_vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    }
    return curr->size;
}

int page_fault_handler(unsigned int vp , int size, int malloc_flag){
    
    int pages_to_evict = size;
    while (pages_to_evict > 0) {
        int evicted_size = evict_first_mem_block();
        if (evicted_size == -1) {
            return -1; // vp not mapped
        }
        pages_to_evict -= evicted_size;
    }

    // if the new vp is already in the evicted pages, then we need to remove it from evicted pages and allocate it to mem.
    // go through the evicted pages and find the vp if it exists else allocate a new frame directly.
    if (malloc_flag == 0) { 
        evicted_page *curr = evicted_pages_head;
        evicted_page *prev = NULL;
        while (curr != NULL) {
            if (curr->vp == vp) {
                if (prev == NULL) {
                    evicted_pages_head = curr->next;
                } else {
                    prev->next = curr->next;
                }
                allocate_mem_block(vp, curr->size, curr);
                free(curr->page_content);
                free(curr);
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    return 0;
}

int allocate_frame(unsigned int page_directory_index, unsigned int page_table_index, int size){
    // Find a free frame
    for (int i = 1; i < num_frames; i++) { //start from 1 physical address not first frame
        if (get_bit_at_index(physical_memory_bitmap, i) == 0){
            set_bit_at_index(physical_memory_bitmap, i);
            page_directory[page_directory_index].page_table[page_table_index].frame_number = i;
            page_directory[page_directory_index].page_table[page_table_index].mem_block = mem_block;
            page_directory[page_directory_index].page_table[page_table_index].size = size;
            page_directory[page_directory_index].page_table[page_table_index].present = 1;
            page_directory[page_directory_index].page_table[page_table_index].valid = 1;
            return 0;
        }
    }
    return -1; // no free frame so page replacement needed
}

unsigned int page_map(unsigned int vp, int size){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);

    // Check if the page table entry is valid
    if (allocate_frame(page_directory_index, page_table_index, size) == 0){
        return 0;
    }
    return 1;
}

int find_free_pages_in_virtual_memory(int num_pages, unsigned int *start_vp) {
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

void print_memblock_list(){
    memory_frame *curr = memory_frames_head;
    if (curr == NULL) {
        printf("No memory blocks allocated\n");
    }
    while (curr != NULL) {
        printf("M: vp: %d, mem_block: %d, size: %d\n", curr->vp, curr->mem_block, curr->size);
        curr = curr->next;
    }
}

void print_evicted_pages(){
    evicted_page *curr = evicted_pages_head;
    if (curr == NULL) {
        printf("No evicted pages\n");
    }
    while (curr != NULL) {
        printf("E: vp: %d, mem_block: %d, size: %d\n", curr->vp, curr->mem_block, curr->size);
        curr = curr->next;
    }
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
            page_fault_handler(start_vp, num_pages - i, 1);
        }
        start_vp += PGSIZE;
    }
    // make new mem block and set it up
    memory_frame *new_curr = (memory_frame *)malloc(sizeof(memory_frame));
    new_curr->vp = output_vp;
    new_curr->mem_block = mem_block;
    mem_block++;
    new_curr->size = num_pages;
    new_curr->next = NULL;
    // add the new mem block to the linked list
    if (memory_frames_head == NULL) {
        memory_frames_head = new_curr;
        memory_frames_tail = new_curr;
    } else {
        memory_frames_tail->next = new_curr;
        memory_frames_tail = new_curr;
    }
    print_memblock_list();
    print_evicted_pages();
    return (void *)(uintptr_t)output_vp;
}

int free_mem_block(unsigned int vp){
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
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
        evicted_page *curr = evicted_pages_head;
        evicted_page *prev = NULL;
        while (curr != NULL) {
            if (curr->mem_block == page_directory[page_directory_index].page_table[page_table_index].mem_block) {
                if (prev == NULL) {
                    evicted_pages_head = curr->next;
                } else {
                    prev->next = curr->next;
                }
                free(curr->page_content);
                free(curr);
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
}

int t_free(unsigned int vp, size_t n){ // What do we do if the give a vp in the middle and dont free till end? so theres a gap in the mem block
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);
    if (page_directory[page_directory_index].page_table[page_table_index].valid == 0) {
        return -1; // Return -1 if the virtual address is not page-aligned or not mapped
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages to free (round up). gotta double check how to handle this

    if (page_directory[page_directory_index].page_table[page_table_index].size < num_pages) {
        return -1; // Return -1 if the size is greater than the allocated size
    }else if (page_directory[page_directory_index].page_table[page_table_index].size == num_pages) { // if equal then free the whole mem block
        free_mem_block(vp);
        mem_block--;
    }else{ // if less than, adjust the size of the mem block
        if(page_directory[page_directory_index].page_table[page_table_index].present == 1){
            memory_frame *curr = memory_frames_head;
            while (curr != NULL) {
                if (curr->mem_block == page_directory[page_directory_index].page_table[page_table_index].mem_block) {
                    curr->size -= num_pages;
                    break;
                }
                curr = curr->next;
            }
        }else{
            evicted_page *curr = evicted_pages_head;
            while (curr != NULL) {
                if (curr->mem_block == page_directory[page_directory_index].page_table[page_table_index].mem_block) {
                    curr->size -= num_pages;
                    break;
                }
                curr = curr->next;
            }
        }
    }
    // Free the pages
    for (int i = 0; i < num_pages; i++) {
        if (page_directory[page_directory_index].page_table[page_table_index].valid == 1) {
            page_directory[page_directory_index].page_table[page_table_index].valid = 0;
            page_directory[page_directory_index].page_table[page_table_index].present = 0;
            set_bit_at_index(physical_memory_bitmap, page_directory[page_directory_index].page_table[page_table_index].frame_number);
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
    if (page_directory[page_directory_index].page_table[page_table_index].valid == 0) {
        return -1; // vp not mapped
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages (round up)
    if (page_directory[page_directory_index].page_table[page_table_index].size < num_pages) {
        return -1; // Return -1 if the size is greater than the allocated size
    }
    if (page_directory[page_directory_index].page_table[page_table_index].present == 0) {
        page_fault_handler(vp, num_pages, 0);
    }
    for (int i = 0; i < num_pages; i++) {
        void* physical_address = translate(vp);
        if (physical_address == NULL) {
            return -1; // vp not mapped
        }
        memcpy(physical_address, val, n);
        vp += PGSIZE;
    }
    return 0;
}

int get_value(unsigned int vp, void *dst, size_t n){
    //TODO: Finish
    unsigned int page_directory_index = (vp >> (page_offset_bits + page_table_bits)) & ((1 << page_directory_bits) - 1);
    unsigned int page_table_index = (vp >> page_offset_bits) & ((1 << page_table_bits) - 1);
    unsigned int page_offset = vp & ((1 << page_offset_bits) - 1);
    if (page_directory[page_directory_index].page_table[page_table_index].valid == 0) {
        return -1; // vp not mapped
    }
    int num_pages = n / PGSIZE + (n % PGSIZE != 0); // Calculate the number of pages (round up)
    if (page_directory[page_directory_index].page_table[page_table_index].size < num_pages) {
        return -1; // Return -1 if the size is greater than the allocated size
    }
    if (page_directory[page_directory_index].page_table[page_table_index].present == 0) {
        page_fault_handler(vp, num_pages, 0);
    }
    for (int i = 0; i < num_pages; i++) {
        void* physical_address = translate(vp);
        if (physical_address == NULL) {
            return -1; // vp not mapped
        }
        memcpy(dst, physical_address, n);
        vp += PGSIZE;
    }
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
    if (page_directory[page_directory_index_a].page_table[page_table_index_a].valid == 0 || page_directory[page_directory_index_b].page_table[page_table_index_b].valid == 0 || page_directory[page_directory_index_c].page_table[page_table_index_c].valid == 0) {
        return; // vp not mapped
    }
    if (page_directory[page_directory_index_a].page_table[page_table_index_a].size < l * m || page_directory[page_directory_index_b].page_table[page_table_index_b].size < m * n || page_directory[page_directory_index_c].page_table[page_table_index_c].size < l * n) {
        return; // Return -1 if the size is greater than the allocated size
    }
    if (page_directory[page_directory_index_a].page_table[page_table_index_a].present == 0) {
        page_fault_handler(a, l * m, 0);
    }
    if (page_directory[page_directory_index_b].page_table[page_table_index_b].present == 0) {
        page_fault_handler(b, m * n, 0);
    }
    if (page_directory[page_directory_index_c].page_table[page_table_index_c].present == 0) {
        page_fault_handler(c, l * n, 0);
    }
    for (int i = 0; i < l; i++) {
        for (int j = 0; j < n; j++) {
            int sum = 0;
            for (int k = 0; k < m; k++) {
                int a_val;
                get_value(a + (i * m + k) * sizeof(int), &a_val, sizeof(int));
                int b_val;
                get_value(b + (k * n + j) * sizeof(int), &b_val, sizeof(int));
                sum += a_val * b_val;
            }
            put_value(c + (i * n + j) * sizeof(int), &sum, sizeof(int));
        }
    }
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
