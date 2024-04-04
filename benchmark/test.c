#include <stdio.h>
#include <stdlib.h>
#include "../my_vm.h"

int main() {
    // Allocate memory using t_malloc
    size_t mem_size = 10; // Size of memory to allocate
    void *ptr = t_malloc(mem_size);
    if (ptr == NULL) {
        printf("Error allocating memory\n");
        return 1;
    }

    // Set a value in the allocated memory
    int value_to_set = 42;
    if (put_value((unsigned int)ptr, &value_to_set, sizeof(int)) != 0) {
        printf("Error setting value in memory\n");
        t_free((unsigned int)ptr, mem_size); // Free allocated memory
        return 1;
    }

    // Get the value from the allocated memory and print it
    int retrieved_value;
    if (get_value((unsigned int)ptr, &retrieved_value, sizeof(int)) != 0) {
        printf("Error getting value from memory\n");
        t_free((unsigned int)ptr, mem_size); // Free allocated memory
        return 1;
    }

    printf("Retrieved value: %d\n", retrieved_value);

    // Free allocated memory
    if (t_free((unsigned int)ptr, mem_size) != 0) {
        printf("Error freeing memory\n");
        return 1;
    }

    return 0;
}