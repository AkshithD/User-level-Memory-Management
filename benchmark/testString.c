#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../my_vm.h"

int main() {
    set_physical_mem();

    // Allocate memory using t_malloc for a string
    size_t mem_size = 50; // Increase size for a string
    void *ptr = t_malloc(mem_size);
    if (ptr == NULL) {
        printf("Error allocating memory\n");
        return 1;
    }

    // Set a string value in the allocated memory
    char *value_to_set = "Hello, world!";
    if (put_value((unsigned int)ptr, value_to_set, strlen(value_to_set) + 1) != 0) { // Include null terminator
        printf("Error setting value in memory\n");
        t_free((unsigned int)ptr, mem_size); // Free allocated memory
        return 1;
    }

    // Get the string value from the allocated memory and print it
    char retrieved_value[50]; // Ensure this is large enough to hold the retrieved string
    if (get_value((unsigned int)ptr, retrieved_value, strlen(value_to_set) + 1) != 0) { // Include null terminator
        printf("Error getting value from memory\n");
        t_free((unsigned int)ptr, mem_size); // Free allocated memory
        return 1;
    }

    printf("Retrieved value: %s\n", retrieved_value);

    // Free allocated memory
    if (t_free((unsigned int)ptr, mem_size) != 0) {
        printf("Error freeing memory\n");
        return 1;
    }

    return 0;
}
