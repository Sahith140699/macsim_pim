#include <stdio.h>
#include <stdint.h>

#define MEMORY_START 0x0
#define MEMORY_SIZE (32LL * (1LL << 30)) // size in GB, the LL denotes long long
#define MEMORY_END (32LL * (1LL << 30))

#define MATRIX_A_M 1000
#define MATRIX_A_N 1000
#define MATRIX_B_M 1000
#define MATRIX_B_N 1000

#define RESULT_M MATRIX_A_M
#define RESULT_N MATRIX_B_N

#define MATRIX_A_START MEMORY_START
#define MATRIX_A_END (MATRIX_A_START + MATRIX_A_M * MATRIX_A_N * 4 - 4)
#define MATRIX_B_START (MATRIX_A_END + 1)
#define MATRIX_B_END (MATRIX_B_START + MATRIX_B_M * MATRIX_B_N * 4 - 1)
#define RESULT_START (MATRIX_B_END + 1)
#define RESULT_END (RESULT_START + RESULT_M * RESULT_N * 4 - 1)

int main() {
    FILE *outputFile = fopen("output.txt", "w");

    printf("Memory Start: %llx\n", (unsigned long long)MEMORY_START);
    printf("Memory Size: %lld\n", MEMORY_SIZE);
    printf("Memory End: %llx\n", (unsigned long long)MEMORY_END);

    for (int i = 0; i < MATRIX_A_M; i++) {
        for (int j = 0; j < MATRIX_B_N; j++) {
            fprintf(outputFile, "####################################\n");
            uint64_t result_address = RESULT_START + i * RESULT_N * 4 + j * 4;
            //fprintf(outputFile, "LOAD rcx, %llx\n", result_address);
            fprintf(outputFile, "MOV rcx , 0\n");
            
            for (int k = 0; k < MATRIX_A_N; k++) {
                uint64_t matrix_a_address = MATRIX_A_START + i * MATRIX_A_N * 4 + k * 4;
                uint64_t matrix_b_address = MATRIX_B_START + k * MATRIX_B_N * 4 + j * 4;
                
                fprintf(outputFile, "LOAD     rax , %llx\n", matrix_a_address);
                fprintf(outputFile, "LOAD     rbx , %llx\n", matrix_b_address);
                fprintf(outputFile, "MUL      rdx , rax, rbx\n");
                fprintf(outputFile, "ADD      rcx , rcx, rdx\n");
            }
            fprintf(outputFile, "STORE    rcx , %llx\n", result_address);
            fprintf(outputFile, "####################################\n");
        }
    }

    fclose(outputFile);
    return 0;
}
