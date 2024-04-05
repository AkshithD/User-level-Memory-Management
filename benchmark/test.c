#include <stdio.h>
#include <stdlib.h>
#include "../my_vm.h"
#define L 3 // Number of rows in matrix a and c
#define M 2 // Number of columns in matrix a and rows in matrix b
#define N 4 // Number of columns in matrix b and c



int main() {
    unsigned int a = (unsigned int)t_malloc(L * M * sizeof(int));
    unsigned int b = (unsigned int)t_malloc(M * N * sizeof(int));
    unsigned int c = (unsigned int)t_malloc(L * N * sizeof(int));

    if (a == 0 || b == 0 || c == 0) {
        printf("Memory allocation failed\n");
        return -1;
    }

    // Initialize matrix a with values
    for (int i = 0; i < L; i++) {
        for (int j = 0; j < M; j++) {
            int value = i * M + j; // Example value for matrix a
            put_value(a + (i * M + j) * sizeof(int), &value, sizeof(int)); 
        }
    }

    // Initialize matrix b with values
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            int value = i * N + j; // Example value for matrix b
            put_value(b + (i * N + j) * sizeof(int), &value, sizeof(int));
        }
    }

    // Perform matrix multiplication
    mat_mult(a, b, c, L, M, N);

    // Display the contents of matrix c to verify the results
    for (int i = 0; i < L; i++) {
        for (int j = 0; j < N; j++) {
            int value;
            get_value(c + (i * N + j) * sizeof(int), &value, sizeof(int));
            printf("%d ", value);
        }
        printf("\n");
    }

    return 0;
}