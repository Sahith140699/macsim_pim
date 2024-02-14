#include <stdio.h>
#include <stdint.h>
#include <zlib.h>

#include "trace_generator.h"

#define MEMORY_START 0x0
#define MEMORY_SIZE (32LL * (1LL << 30)) // size in GB, the LL denotes long long
#define MEMORY_END (32LL * (1LL << 30))

#define MATRIX_A_M 50
#define MATRIX_A_N 50
#define MATRIX_B_M 50
#define MATRIX_B_N 50

//++anurag
#define BANK_NUM 15
#define NUM_BANKS 16
#define ROWBUF_SIZE 2048
#define BID_SHIFT 11 // log2(ROWBUF_SIZE)
#define BID_MASK 0xF // BITMASK(log2(NUM_BANKS))
#define BANK_BASE_ADDR (BID_MASK & BANK_NUM) << BID_SHIFT
#define RID_SHIFT 4 // log2(NUM_BANKS)
/*
The format of the address is : 
| Row ID (Remaining Bits) | Bank ID (4 bits) | Column ID (11 bits) |

i.e. each row is populated for all the banks before moving to the next row
*/

#define RESULT_M MATRIX_A_M
#define RESULT_N MATRIX_B_N

//Align by row 
//#define MATRIX_A_START ((MEMORY_START>>BID_SHIFT)&BANK_NUM)<<BID_SHIFT
//#define MATRIX_A_END MATRIX_A_START + (MATRIX_A_M * MATRIX_A_N * 4 - 4)/ROWBUF_SIZE + (((MATRIX_A_M * MATRIX_A_N * 4 - 4)%ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT))
//#define MATRIX_B_START ((MATRIX_A_END >> (BID_SHIFT + RID_SHIFT))+1) << (BID_SHIFT + RID_SHIFT) //aligned to the next row
//#define MATRIX_B_END (MATRIX_B_START + MATRIX_B_M * MATRIX_B_N * 4 - 4)
//#define MATRIX_B_END MATRIX_B_START + (MATRIX_B_M * MATRIX_B_N * 4 - 4)/ROWBUF_SIZE + (((MATRIX_B_M * MATRIX_B_N * 4 - 4)%ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT))
//#define RESULT_START (MATRIX_B_END + 1)
//#define RESULT_START ((MATRIX_B_END >> (BID_SHIFT + RID_SHIFT))+1) << (BID_SHIFT + RID_SHIFT) //aligned to the next row
//#define RESULT_END (RESULT_START + RESULT_M * RESULT_N * 4 - 4)
//#define RESULT_END (RESULT_START + (RESULT_M * RESULT_N * 4 - 4)/ROWBUF_SIZE + (((RESULT_M * RESULT_N * 4 - 4)%ROWBUF_SIZE))<< (BID_SHIFT + RID_SHIFT))   


uint64_t MATRIX_A_START = ((MEMORY_START>>BID_SHIFT)|BANK_NUM)<<BID_SHIFT;
uint64_t MATRIX_A_END = MATRIX_A_START + (((MATRIX_A_M * MATRIX_A_N * 4 - 4)/ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT)) + ((MATRIX_A_M * MATRIX_A_N * 4 - 4)%ROWBUF_SIZE);
uint64_t MATRIX_B_START = (MATRIX_A_END  + (1 << (BID_SHIFT + RID_SHIFT)))>> (BID_SHIFT) << (BID_SHIFT);
uint64_t MATRIX_B_END = MATRIX_B_START + (((MATRIX_B_M * MATRIX_B_N * 4 - 4)/ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT)) + ((MATRIX_B_M * MATRIX_B_N * 4 - 4)%ROWBUF_SIZE);
uint64_t RESULT_START = (MATRIX_B_END + (1 << (BID_SHIFT + RID_SHIFT))) >> (BID_SHIFT) << (BID_SHIFT);
uint64_t RESULT_END = RESULT_START + (((RESULT_M * RESULT_N * 4 - 4)/ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT)) + ((RESULT_M * RESULT_N * 4 - 4)%ROWBUF_SIZE);

void print_trace_info(TraceInfo* t_info, FILE* file) {
    fprintf(file, "*** begin of the data structure ***\n");
    fprintf(file, "t_info->uop_opcode_num %d\n", t_info->uop_opcode_num);
    fprintf(file, "t_info->uop_opcode %s\n", t_info->uop_opcode);
    fprintf(file, "t_info->num_read_regs: %d\n", t_info->num_read_regs);
    fprintf(file, "t_info->num_dest_regs: %d\n", t_info->num_dest_regs);

    if (t_info->num_read_regs >= 1) {
        fprintf(file, "t_info->src0: %d : %s\n", t_info->src0, regs[t_info->src0]);
        if (t_info->num_read_regs > 1) {
            fprintf(file, "t_info->src1: %d : %s\n", t_info->src1, regs[t_info->src1]);
        }
    }
    if (t_info->num_dest_regs >= 1) {
        fprintf(file, "t_info->dst0: %d : %s\n", t_info->dst0, regs[t_info->dst0]);
        if (t_info->num_dest_regs > 1) {
            fprintf(file, "t_info->dst1: %d : %s\n", t_info->dst1, regs[t_info->dst1]);
        }
    }
    fprintf(file, "t_info->cf_type: %s\n", t_info->cf_type);
    fprintf(file, "t_info->has_immediate: %d\n", t_info->has_immediate);
    fprintf(file, "t_info->r_dir:%d\n", t_info->r_dir);
    fprintf(file, "t_info->has_st: %d\n", t_info->has_st);
    fprintf(file, "t_info->num_ld: %d\n", t_info->num_ld);
    fprintf(file, "t_info->mem_read_size: %llu\n", t_info->mem_read_size);
    fprintf(file, "t_info->mem_write_size: %llu\n", t_info->mem_write_size);
    fprintf(file, "t_info->is_fp: %d\n", t_info->is_fp);
    fprintf(file, "t_info->ld_vaddr1: %llx\n", t_info->ld_vaddr1);
    fprintf(file, "t_info->ld_vaddr2: %llx\n", t_info->ld_vaddr2);
    fprintf(file, "t_info->st_vaddr: %llx\n", t_info->st_vaddr);
    fprintf(file, "t_info->instruction_addr: %llx\n", t_info->instruction_addr);
    fprintf(file, "t_info->branch_target: %llx\n", t_info->branch_target);
    fprintf(file, "t_info->actually_taken: %d\n", t_info->actually_taken);
    fprintf(file, "t_info->write_flg: %d\n", t_info->write_flg);
    fprintf(file, "*** end of the data structure ***\n\n");
}

void write_trace_info_to_gz(TraceInfo* t_info, gzFile gzfile) {
    Inst_info *info = new Inst_info;
    info->num_read_regs = t_info->num_read_regs;
    info->num_dest_regs = t_info->num_dest_regs;
    
    if (t_info->num_read_regs >= 1) {
        info->src[0] = t_info->src0;
        if (t_info->num_read_regs > 1) {
            info->src[1] = t_info->src1;
        }
    }
    if (t_info->num_dest_regs >= 1) {
        info->dst[0] = t_info->dst0;
        if (t_info->num_dest_regs > 1) {
            info->dst[1] = t_info->dst1;
        }
    }

    info->cf_type = 0; // hardcoded to zero since there are no control flow instructions
    info->has_immediate = t_info->has_immediate;
    info->opcode = t_info->uop_opcode_num;
    info->has_st = t_info->has_st;
    info->is_fp = t_info->is_fp;
    info->write_flg = t_info->write_flg;
    info->num_ld = t_info->num_ld;
    info->size = 8;
    info->ld_vaddr1 = t_info->ld_vaddr1;
    info->ld_vaddr2 = t_info->ld_vaddr2;
    info->st_vaddr = t_info->st_vaddr;
    info->instruction_addr = t_info->instruction_addr;
    info->branch_target = t_info->branch_target;
    info->mem_read_size = t_info->mem_read_size;
    info->mem_write_size = t_info->mem_write_size;
    info->rep_dir = t_info->r_dir;
    info->actually_taken = t_info->actually_taken;
    gzwrite(gzfile, info, sizeof(Inst_info));
}

int main() {

    gzFile gz_outputFile = gzopen("output.raw", "wb");
    FILE *outputFile = fopen("output.txt", "w");

    printf("Memory Start: %#llx\n", (unsigned long long)MEMORY_START);
    printf("Memory Size: %lld\n", MEMORY_SIZE);
    printf("Memory End: %#llx\n", (unsigned long long)MEMORY_END);
    //printf all the bank info
    printf("Bank Num: %d\n", BANK_NUM);
    printf("Num Banks: %d\n", NUM_BANKS);
    printf("Rowbuf Size: %d\n", ROWBUF_SIZE);
    printf("BID Shift: %d\n", BID_SHIFT);
    printf("BID Mask: %d\n", BID_MASK);
    printf("Bank Base Address: %#llx\n", (unsigned long long)BANK_BASE_ADDR);

    TraceInfo t_info;
    t_info.cf_type = "NOT_CF";
    t_info.has_immediate = 0;
    t_info.r_dir = 0;
    t_info.has_st = 0;
    t_info.num_ld = 0;
    t_info.mem_read_size = 8;
    t_info.mem_write_size = 0;
    t_info.is_fp = 0;
    t_info.ld_vaddr1 = 0;
    t_info.ld_vaddr2 = 0;
    t_info.st_vaddr = 0;
    t_info.instruction_addr = 0x400000; // FIX
    t_info.branch_target = 0;
    t_info.actually_taken = 0;
    t_info.write_flg = 0;

    for (int i = 0; i < MATRIX_A_M; i++) {
        for (int j = 0; j < MATRIX_B_N; j++) {
            uint64_t result_address = RESULT_START + (i * RESULT_N * 4 + j * 4)%ROWBUF_SIZE + (((i * RESULT_N * 4 + j * 4)/ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT));

            // MOV Operation
            t_info.uop_opcode_num = 30; 
            t_info.uop_opcode = "DATAXFER";
            t_info.num_read_regs = 0;
            t_info.num_dest_regs = 1;
            t_info.dst0 = 9;
            t_info.src0 = 0;
            t_info.has_immediate = 1;
            t_info.has_st = 0;
            t_info.ld_vaddr1 = 0;
            t_info.ld_vaddr2 = 0;
            t_info.st_vaddr = 0;
            t_info.mem_read_size = 0;
            t_info.mem_write_size = 0;
            t_info.instruction_addr+=4; // FIX
            print_trace_info(&t_info, outputFile);
            write_trace_info_to_gz(&t_info, gz_outputFile);


            for (int k = 0; k < MATRIX_A_N; k++) {
                uint64_t matrix_a_address = MATRIX_A_START + (i * MATRIX_A_N * 4 + k * 4)%ROWBUF_SIZE + (((i * MATRIX_A_N * 4 + k * 4)/ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT));
                uint64_t matrix_b_address = MATRIX_B_START + (k * MATRIX_B_N * 4 + j * 4)%ROWBUF_SIZE + (((k * MATRIX_B_N * 4 + j * 4)/ROWBUF_SIZE)<< (BID_SHIFT + RID_SHIFT));
                
                
                printf("*************************************************\n");
                printf("MATRIX_A_START: %#llx\n", (unsigned long long)MATRIX_A_START);
                printf("MATRIX_A_END: %#llx\n", (unsigned long long)MATRIX_A_END);
                printf("MATRIX_B_START: %#llx\n", (unsigned long long)MATRIX_B_START);
                printf("MATRIX_B_END: %#llx\n", (unsigned long long)MATRIX_B_END);
                printf("RESULT_START: %#llx\n", (unsigned long long)RESULT_START);
                printf("RESULT_END: %#llx\n", (unsigned long long)RESULT_END);
                printf("---------\n");
                printf("Matrix A Address: %#llx\n", (unsigned long long)matrix_a_address);
                //printf("%lld\n", (i * MATRIX_A_N * 4 + k * 4)/ROWBUF_SIZE);
                printf("Matrix B Address: %#llx\n", (unsigned long long)matrix_b_address);
                //printf("%lld\n", (k * MATRIX_B_N * 4 + j * 4)/ROWBUF_SIZE);
                //printf("%lld\n", (k * MATRIX_B_N * 4 + j * 4)%ROWBUF_SIZE);
                

                // LOAD operation for matrix_a
                t_info.uop_opcode_num = 30;
                t_info.uop_opcode = "DATAXFER";
                t_info.num_read_regs = 0;
                t_info.num_dest_regs = 1;
                t_info.dst0 = 10;
                t_info.mem_read_size = 8;
                t_info.has_immediate = 0;
                t_info.num_ld = 1;
                t_info.ld_vaddr1 = matrix_a_address;
                t_info.mem_write_size = 0;
                t_info.instruction_addr+=4; // FIX

                print_trace_info(&t_info, outputFile);
                write_trace_info_to_gz(&t_info, gz_outputFile);

                // LOAD operation for matrix_b
                t_info.dst0 = 7;
                t_info.ld_vaddr1 = matrix_b_address;
                t_info.instruction_addr+=4; // FIX
                print_trace_info(&t_info, outputFile);
                write_trace_info_to_gz(&t_info, gz_outputFile);

                // MUL operation
                t_info.uop_opcode_num = 95;
                t_info.uop_opcode = "TR_MUL";
                t_info.num_read_regs = 2;
                t_info.num_dest_regs = 2;
                t_info.src0 = 7;
                t_info.src1 = 10;
                t_info.dst0 = 8;
                t_info.dst1 = 25;
                t_info.num_ld = 0;
                t_info.ld_vaddr1 = 0;
                t_info.mem_read_size = 0;
                t_info.instruction_addr+=4; // FIX

                print_trace_info(&t_info, outputFile);
                write_trace_info_to_gz(&t_info, gz_outputFile);

                // ADD operation

                t_info.uop_opcode_num = 13;
                t_info.uop_opcode = "BINARY";
                t_info.src0 = 9;
                t_info.src1 = 8;
                t_info.dst0 = 9;
                t_info.dst1 = 25;
                t_info.instruction_addr+=4; // FIX
                print_trace_info(&t_info, outputFile);
                write_trace_info_to_gz(&t_info, gz_outputFile);
            }
            
            // STORE operation for result
            t_info.uop_opcode_num = 30; 
            t_info.uop_opcode = "DATAXFER";
            t_info.st_vaddr = result_address;
            t_info.num_read_regs = 1;
            t_info.num_dest_regs = 0;
            t_info.src0 = 9;
            t_info.mem_write_size = 8;
            t_info.has_st = 1; 
            t_info.instruction_addr+=4; // FIX
            print_trace_info(&t_info, outputFile);
            write_trace_info_to_gz(&t_info, gz_outputFile);
        }

                // t_info.uop_opcode_num = 13;
                // t_info.uop_opcode = "BINARY";
                // t_info.num_read_regs = 2;
                // t_info.num_dest_regs = 2;
                // t_info.src0 = 9;
                // t_info.src1 = 8;
                // t_info.dst0 = 9;
                // t_info.dst1 = 25;
                // t_info.has_st = 0;
                // t_info.mem_write_size = 0;
                // t_info.instruction_addr+=4; // FIX
                // print_trace_info(&t_info, outputFile);
                // write_trace_info_to_gz(&t_info, gz_outputFile);
    }

    fclose(outputFile);
    gzclose(gz_outputFile);

    return 0;
}
