#include <stdio.h>
#include <stdlib.h>
#include "../my_vm.h"

#define ARRAY_SIZE 4096

int main() {
    // allocate a
    int * a = (int*)t_malloc(ARRAY_SIZE * sizeof(int));
    if (a == NULL) {
        printf("A: Failed to allocate memory\n");
    }
    printf("A: Allocated memory at %p\n", a);
    int * b = (int*)t_malloc(ARRAY_SIZE * sizeof(int));
    if (b == NULL) {
        printf("B: Failed to allocate memory\n");
    }
    printf("B: Allocated memory at %p\n", b);
    int * c = (int*)t_malloc(ARRAY_SIZE * sizeof(int));
    if (c == NULL) {
        printf("C: Failed to allocate memory\n");
    }
    printf("C: Allocated memory at %p\n", c);
    for(int i = 0; i < ARRAY_SIZE; i++) {
        int value_to_set = i;
        if(put_value((unsigned int)(a + i), &value_to_set, sizeof(int)) != 0) {
            printf("A: Failed to put value\n");
            return 1;
        }
        if(put_value((unsigned int)(b + i), &value_to_set, sizeof(int)) != 0) {
            printf("B: Failed to put value\n");
            return 1;
        }
    }
    for(int i = 0; i < ARRAY_SIZE; i++) {
        int value_to_get;
        if(get_value((unsigned int)(a + i), &value_to_get, sizeof(int)) != 0) {
            printf("A: Failed to get value\n");
            return 1;
        }
        printf("A: expected %d, got %d\n", i, value_to_get);
        if(get_value((unsigned int)(b + i), &value_to_get, sizeof(int)) != 0) {
            printf("B: Failed to get value\n");
            return 1;
        }
        printf("B: expected %d, got %d\n", i, value_to_get);
    }

    // free a
    if(t_free((unsigned int)a, ARRAY_SIZE*sizeof(int)) != 0) {
        printf("Failed to free memory\n");
        return 1;
    }
    // free b
    if(t_free((unsigned int)b, ARRAY_SIZE*sizeof(int)) != 0) {
        printf("Failed to free memory\n");
        return 1;
    }
    
    return 0;
}