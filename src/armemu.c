#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NREGS 16
#define STACK_SIZE 1024
#define SP 13
#define LR 14
#define PC 15

struct arm_state;

/* Assembly functions to emulate */
int quadratic_a(int x, int a, int b, int c);
int sum_array_a(int *array, int n);
int find_max_a(int *array, int n);
int fib_iter_a(int n);
int fib_rec_a(int n);
int strlen_a(char* a);
void print_stats(struct arm_state *state);

/* The complete machine state */
struct arm_state
{
    unsigned int regs[NREGS];
    unsigned int cpsr;
    unsigned char stack[STACK_SIZE];

    int branch_inst_count;
    int dp_inst_count;
    int mem_inst_count;
    int total_inst_count;
    int branch_taken;
    int branch_not_taken;

    struct cache *dmc;
};


struct cache
{
    struct cache_slot *slots;
    int hits;
    int misses;
    int requests;
    int size;
};

struct cache_slot
{
    unsigned int v;
    unsigned int tag;
};

void struct_init(struct cache *dmc)
{
    int i;

    printf("Size: %d\n", dmc->size);

    dmc->slots = (struct cache_slot *)malloc((sizeof(struct cache_slot)) * dmc->size);

    for(i = 0; i < dmc->size; i++)
    {
        dmc->slots[i].v = 0;
        dmc->slots[i].tag = 0;
    }

    dmc->hits = 0;
    dmc->misses = 0;
    dmc->requests = 0;
}

/* Initialize an arm_state struct with a function pointer and arguments */
void arm_state_init(struct arm_state *as, unsigned int *func,
                    unsigned int arg0, unsigned int arg1,
                    unsigned int arg2, unsigned int arg3) {
    int i;

    /* Zero out all registers */
    for(i = 0; i < NREGS; i++)
    {
        as->regs[i] = 0;
    }

    /* Zero out CPSR */
    as->cpsr = 0;

    /* Zero out the stack */
    for(i = 0; i < STACK_SIZE; i++)
    {
        as->stack[i] = 0;
    }

    /* Set the PC to point to the address of the function to emulate */
    as->regs[PC] = (unsigned int) func;

    /* Set the SP to the top of the stack (the stack grows down) */
    as->regs[SP] = (unsigned int) &as->stack[STACK_SIZE];

    /* Initialize LR to 0, this will be used to determine when the function has called bx lr */
    as->regs[LR] = 0;

    /* Initialize the first 4 arguments */
    as->regs[0] = arg0;
    as->regs[1] = arg1;
    as->regs[2] = arg2;
    as->regs[3] = arg3;
    
    as->branch_inst_count = 0;
    as->dp_inst_count = 0;
    as->mem_inst_count = 0;
    as->total_inst_count = 0;
    as->branch_taken = 0;
    as->branch_not_taken = 0;
}

/* Helper function to shift PC by the argument specifed in the 2nd value */
void shift_pc(struct arm_state *state, int shift)
{
    state->regs[PC] = state->regs[PC] + shift;
}

/* Helper functions for increasing analysis count */
void incrementDataProcessingCount(struct arm_state *state)
{
    state->dp_inst_count = state->dp_inst_count + 1;
    state->total_inst_count = state->total_inst_count + 1;
}

/* Helper functions for increasing memory count */
void incrementMemoryCount(struct arm_state *state)
{
    state->mem_inst_count = state->mem_inst_count + 1;
    state->total_inst_count = state->total_inst_count + 1;
}

/* Helper functions for increasing branch count */
void incrementBranchCount(struct arm_state *state)
{
    state->branch_inst_count = state->branch_inst_count + 1;
    state->total_inst_count = state->total_inst_count + 1;
}

/*---------- Boolean Functions ----------*/

/* Function to check op bits to ensure it is a data processing instruction */
bool is_dp_inst(unsigned int iw)
{
    unsigned int op = (iw >> 26) & 0b11;
    return (op == 0);
}

/* Separate data processing function to check for a mul instruction
 * given its differing location for opcode bits */
bool is_mul_inst(unsigned int iw)
{
    unsigned int op;
    unsigned int opcode;

    op = (iw >> 26) & 0b11;
    opcode = (iw >> 4) & 0b1111;

    return (op == 0) && (opcode == 0b1001);
}

/* Function to check op bits to ensure it is a memory instruction */
bool is_mem_inst(unsigned int iw)
{
    unsigned int op = (iw >> 26) & 0b11;
    return (op == 1);
}

/* Function to check bits to ensure it is a branch instruction */
bool is_b_inst(unsigned int iw)
{
    unsigned int op = (iw >> 25) & 0b111;
    return (op == 5);
}

/* Given function to check for branch and exchange */
bool is_bx_inst(unsigned int iw)
{
    unsigned int bx_code;

    bx_code = (iw >> 4) & 0x00FFFFFF;

    return (bx_code == 0b000100101111111111110001);
}

/* Function to check for immediate value */
bool is_imm(unsigned int iw) 
{
    return ((iw >> 25) & 0b1 == 1);
}

/* Function to check for op code value and compare to cpsr */
bool is_condition(struct arm_state *state) {
    unsigned int iw;
    int cond, cpsr_cond;

    iw = *((unsigned int *) state->regs[PC]);
    cond = iw >> 28;
    cpsr_cond = state->cpsr >> 28;

    if(cond == 0b0000)
    {
        return cpsr_cond == 0b0000;
    }
    else if(cond == 0b0001)
    {
        return cpsr_cond == 0b1011 || cpsr_cond == 0b1100;
    }
    else if(cond == 0b1011)
    {
        return cpsr_cond == 0b1011;
    }
    else if(cond == 0b1100)
    {
        return cpsr_cond == 0b1100;
    }
}

/*---------- Data Processing Emulation Functions ----------*/

void armemu_add(struct arm_state *state, unsigned int iw, unsigned int rd, unsigned int rn)
{
    unsigned int rm;

    if(is_imm(iw))
    {
        rm = iw & 0xFF;
        state->regs[rd] = state->regs[rn] + rm;
    }
    else
    {
        rm = iw & 0xF;
        state->regs[rd] = state->regs[rn] + state->regs[rm];
    }

    if(rd != PC) 
    {
        shift_pc(state, 4);
    }
}

void armemu_sub(struct arm_state *state, unsigned int iw, unsigned int rd, unsigned int rn)
{
    unsigned int rm;

    if(is_imm(iw))
    {
        rm = iw & 0xFF;
        state->regs[rd] = state->regs[rn] - rm;
    }
    else
    {
        rm = iw & 0xF;
        state->regs[rd] = state->regs[rn] - state->regs[rm];
    }

    if(rd != PC)
    {
        shift_pc(state, 4);
    }
}

void armemu_mov(struct arm_state *state, unsigned int iw, unsigned int rd, unsigned int rn)
{
    unsigned int rm;

    if(is_imm(iw))
    {
        rn = iw & 0xFF;
        state->regs[rd] = rn;
    }
    else
    {
        rn = iw & 0xF;
        state->regs[rd] = state->regs[rn];
    }

    if(rd != PC)
    {
        shift_pc(state, 4);
    }
}

void armemu_cmp(struct arm_state *state, unsigned int iw, unsigned int rd, unsigned int rn)
{
    int cmp, rm;

    /* Reset CPSR value */
    state->cpsr = 0;

    if(is_imm(iw))
    {
        rm = iw & 0xFF;
        cmp = state->regs[rn] - rm;
    }
    else
    {
        rm = iw & 0xF;
        cmp = state->regs[rn] - state->regs[rm];
    }

    /* Check comparision value and set cpsr*/
    if(cmp == 0)
    {
        state->cpsr = 0;
    }
    else if(cmp < 0)
    {
        state->cpsr = (0b1011 << 28);
    }
    else if(cmp > 0)
    {
        state->cpsr = (0b1100 << 28);
    }

    if(rd != PC)
    {
        shift_pc(state, 4);
    }
}

void armemu_mul(struct arm_state *state)
{
    unsigned int iw;
    unsigned int rd, rn, rs;

    iw = *((unsigned int *) state->regs[PC]);

    rd = (iw >> 16) & 0xF;
    rn = (iw >> 12) & 0xF;
       rs = (iw >> 8) & 0xF;

    state->regs[rd] = state->regs[rn] * state->regs[rs];

    if(rd != PC)
    {
        shift_pc(state, 4);
    }

    incrementDataProcessingCount(state);
}

/*---------- Branching Emulation Functions ----------*/

void armemu_b(struct arm_state *state)
{
    unsigned int iw;
    int offset;

    iw = *((unsigned int *) state->regs[PC]);
    
    /* Check the link bit to see if branching with a link */
    if((iw >> 24) & 0b1 == 1)
    {
        state->regs[LR] = state->regs[PC] + 4;
    }
    
    /* Grabbing the offset from the first 24 bits */
    offset = iw & 0xFFFFFF;
    offset = offset << 2;

    /* Shift to the end of the offset and check the value of its last bit 
     * if it starts with 1, we know it is a 2's complement number */
    if((iw >> 23) & 0b1 == 1)
    {
        offset = offset | 0b111111 << 26;
        offset = ~offset + 1;
        offset = 0 - offset;
    }

    shift_pc(state, 8 + offset);

    state->branch_taken = state->branch_taken + 1;
}

void armemu_bx(struct arm_state *state)
{
    unsigned int iw;
    unsigned int rn;

    iw = *((unsigned int *) state->regs[PC]);
    rn = iw & 0b1111;

    state->regs[PC] = state->regs[rn];

    state->branch_taken = state->branch_taken + 1;
    incrementBranchCount(state);
}

/*-------- Caching -------- */
int get_slot(int size, unsigned int address)
{
    return (address >> 2) & (size -1);
}

unsigned int get_tag(int size, unsigned int address)
{
    int log2 = 0;
    while(size)
    {
        size = size >> 1;
        log2 = log2+1;
    }

    return (address >> (2 + log2)) & ((2+log2) - 1);
}

unsigned int get_cache_vbit(struct cache * dmc, int slot)
{
    return dmc->slots[slot].v;
}
unsigned int get_cache_tag(struct cache * dmc, int slot)
{
    return dmc->slots[slot].tag;
}

void update_cache(struct cache * dmc, int slot, unsigned int tag)
{
    dmc->slots[slot].tag = tag;
    dmc->slots[slot].v = 1;
}

void simulate_cache(struct cache * dmc, unsigned int address)
{
    int slot = get_slot(dmc->size, address);
    unsigned int tag = get_tag(dmc->size, address);
    unsigned int v = get_cache_vbit(dmc, slot);
    unsigned int tagc = get_cache_tag(dmc, slot);

    if(!v)
    {
        dmc->misses++;
        update_cache(dmc, slot, tag);
    }
    else
    {
        if(tag == tagc)
        {
            dmc->hits++;
        }
        else
        {
            dmc->misses++;
            update_cache(dmc, slot, tag);
        }
    }
    dmc->requests++;
}


/*-------- Primary Functions -------- */

/* Function to execute a data processing instruction depending on the provided opcode */
void armemu_dp(struct arm_state *state)
{
    unsigned int rd, rn;
    unsigned int iw = *((unsigned int *) state->regs[PC]);
    unsigned int opcode = (iw >> 21) & 0b1111;

    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;

    switch(opcode)
    {
        case 0b0100:
            armemu_add(state, iw, rd, rn);
            break;
        case 0b0010:
            armemu_sub(state, iw, rd, rn);
            break;
        case 0b1010:
            armemu_cmp(state, iw, rd, rn);
            break;
        case 0b1101:
            armemu_mov(state, iw, rd, rn);
            break;
        default:
            break;
    }

    incrementDataProcessingCount(state);
}

/* Function to execute commands such as str and ldr */
void armemu_mem(struct arm_state *state)
{
    unsigned int rd, rn, rm, val;
    unsigned int iw = *((unsigned int *) state->regs[PC]);
    int offset;

    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;

    /* if the value at this bit is 1, we know
     * the offset is a register, so we set it accordingly */
    if((iw >> 25) & 0b1 == 1)
    {
        rm = iw & 0xF;
        offset = state->regs[rm];
    }
    /* Otherwise, retrieve the immediate value offset */
    else
    {
        offset = iw & 0xFFF;
    }
    
    /* Shift to the load/store bit & check whether we str or ldr */
    if((iw >> 20) & 0b1 == 1)
    {
        /* Shift to the byte/word bit & check if we are transferring a word or byte quantity */
        if((iw >> 22) & 0b1 == 1)
        {
            val = *((unsigned char *) (state->regs[rn] + offset));
        }
        else
        {
            val = *((unsigned int *) (state->regs[rn] + offset));
        }
        
        /* Store the value in the register */
        state->regs[rd] = val;
    }
    /* Otherwise, load the value from a register */
    else
    {
        val = state->regs[rd];
        *(unsigned int *) (state->regs[rn] + offset) = val;
    }

    if(rd != PC)
    {
        state->regs[PC] = state->regs[PC] + 4;
    }

    incrementMemoryCount(state);
}

void armemu_one(struct arm_state *state)
{
    unsigned int iw;
    
    iw = *((unsigned int *) state->regs[PC]);

    if(is_bx_inst(iw))
    {
        armemu_bx(state);
    }
    else if(is_mul_inst(iw))
    {
        armemu_mul(state);
    }
    else if(is_mem_inst(iw))
    {
        armemu_mem(state);
    }
    else if(is_dp_inst(iw))
    {
        armemu_dp(state);
    }
    else if(is_b_inst(iw))
    {
        if(is_condition(state))
        {
            armemu_b(state);
        }
        else
        {
            state->regs[PC] = state->regs[PC] + 4;
            state->branch_not_taken = state->branch_not_taken + 1;
        }
        incrementBranchCount(state);
    }

    simulate_cache(state->dmc, iw);
}

unsigned int armemu(struct arm_state *state)
{
    /* Execute instructions until PC = 0 */
    /* This happens when bx lr is issued and lr is 0 */
    while(state->regs[PC] != 0)
    {
        armemu_one(state);
    }

    return state->regs[0];
} 


/*-------- Printing functions for testing and/or debugging ---------*/

void arm_state_print(struct arm_state *as)
{
    int i;

    for(i = 0; i < NREGS; i++)
    {
        printf("reg[%d] = %d\n", i, as->regs[i]);
    }
    printf("cpsr = %X\n", as->cpsr);
}

void cache_statistics_print(struct cache *dmc)
{
    printf("\nCache Statistics:\n");
    printf("-----------------------------------\n");
    printf("Hits: %d (%.1f%)\n", dmc->hits, (double) dmc->hits / dmc->requests * 100);
    printf("Misses: %d (%.1f%)\n", dmc->misses, (double) dmc->misses / dmc->requests * 100);
    printf("Requests: %d\n", dmc->requests);
}

void cache_state_print(struct cache *dmc)
{
    int i;

    for(i = 0; i < dmc->size; i++)
    {
        printf("Slot[%d] v value = %d\n", i, dmc->slots[i].v);
        printf("Slot[%d] tag value = %d\n", i, dmc->slots[i].tag);
    }
}

/*-------- Testing functions ---------*/

void print_quadratic_tests(struct arm_state *state)
{
    unsigned int r;
    
    printf("\n----------------Begin Quadratic Tests------------------\n\n");

    printf("Non-Emulated (quadratic_a(1, 2, 3, 4)): %d\n", quadratic_a(1, 2, 3, 4));
    arm_state_init(state, (unsigned int *) quadratic_a, 1, 2, 3, 4);
    r = armemu(state);
    printf("Emulated (quadratic_a(1, 2, 3, 4)): %d\n", r);
    print_stats(state);

    printf("\n");

    printf("Non-Emulated (quadratic_a(0, 1, 2, 3)): %d\n", quadratic_a(0, 1, 2, 3));
    arm_state_init(state, (unsigned int *) quadratic_a, 0, 1, 2, 3);
    r = armemu(state);
    printf("Emulated (quadratic_a(0, 1, 2, 3)): %d\n", r);
    print_stats(state);

    printf("\n");

    printf("Non-Emulated (quadratic_a(-2, 5, 3, 10)): %d\n", quadratic_a(-2, 5, 3, 10));
    arm_state_init(state, (unsigned int *) quadratic_a, -2, 5, 3, 10);
    r = armemu(state);
    printf("Emulated (quadratic_a(-2, 5, 3, 10)): %d\n", r);
    print_stats(state);

    printf("\n");

    printf("Non-Emulated (quadratic_a(12, 2, 9, 3)): %d\n", quadratic_a(12, 2, 9, 3));
    arm_state_init(state, (unsigned int *) quadratic_a, 12, 2, 9, 3);
    r = armemu(state);
    printf("Emulated (quadratic_a(12, 2, 9, 3)): %d\n", r);
    print_stats(state);

    printf("\n");
}

void print_sum_array_tests(struct arm_state *state)
{
    unsigned int r;

    printf("\n----------------Begin Sum_Array Tests------------------\n\n");

    int sum_array[5] = {1, 2, 3, 4, 5};
    printf("Non-Emulated (sum_array_a([1, 2, 3, 4, 5], 5)): %d\n", sum_array_a(sum_array, 5));
    arm_state_init(state, (unsigned int *) sum_array_a, (unsigned int) sum_array, 5, 0, 0);
    r = armemu(state);
    printf("Emulated (sum_array_a([1, 2, 3, 4, 5], 5)): %d\n", r);
    print_stats(state);

    printf("\n");

    int sum_array1[5] = {0, 0, 0, 0, 1000};
    printf("Non-Emulated (sum_array_a([0, 0, 0, 0, 1000], 5)): %d\n", sum_array_a(sum_array1, 5));
    arm_state_init(state, (unsigned int *) sum_array_a, (unsigned int) sum_array1, 5, 0, 0);
    r = armemu(state);
    printf("Emulated (sum_array_a([0, 0, 0, 0, 1000], 5)): %d\n", r);
    print_stats(state);

    printf("\n");

    int sum_array2[5] = {10, 12, 14, 16, 18};
    printf("Non-Emulated (sum_array_a([10, 12, 14, 16, 18], 5)): %d\n", sum_array_a(sum_array2, 5));
    arm_state_init(state, (unsigned int *) sum_array_a, (unsigned int) sum_array2, 5, 0, 0);
    r = armemu(state);
    printf("Emulated (sum_array_a([10, 12, 14, 16, 18], 5)): %d\n", r);
    print_stats(state);

    printf("\n");

    int sum_array3[1000] = {[0 ... 999] = 1};
    printf("Non-Emulated (sum_array_a([1, 1, 1, 1, ...], 1000)): %d\n", sum_array_a(sum_array3, 1000));
    arm_state_init(state, (unsigned int *) sum_array_a, (unsigned int) sum_array3, 1000, 0, 0);
    r = armemu(state);
    printf("Emulated (sum_array_a([1, 1, 1, 1, ...], 1000)): %d\n", r);
    print_stats(state);

    printf("\n");
}

void print_find_max_tests(struct arm_state *state)
{
    unsigned int r;

    printf("\n----------------Begin Find_Max_Array Tests------------------\n\n");

    int find_max_array[5] = {1, 2, 3, 4, 5};
    printf("Non-Emulated (find_max_a([1, 2, 3, 4, 5], 5)): %d\n", find_max_a(find_max_array, 5));
    arm_state_init(state, (unsigned int *) find_max_a, (unsigned int) find_max_array, 5, 0, 0);
    r = armemu(state);
    printf("Emulated (find_max_a([1, 2, 3, 4, 5], 5)): %d\n", r);
    print_stats(state);

    printf("\n");

    int find_max_array1[5] = {1, 2, -3, 4, -5};
    printf("Non-Emulated (find_max_a([1, 2, -3, 4, -5], 5)): %d\n", find_max_a(find_max_array1, 5));
    arm_state_init(state, (unsigned int *) find_max_a, (unsigned int) find_max_array1, 5, 0, 0);
    r = armemu(state);
    printf("Emulated (find_max_a([1, 2, -3, 4, -5], 5)): %d\n", r);
    print_stats(state);

    printf("\n");

    int find_max_array2[5] = {0, 0, 0, 0, 1000};
    printf("Non-Emulated (find_max_a([0, 0, 0, 0, 1000], 5)): %d\n", find_max_a(find_max_array2, 5));
    arm_state_init(state, (unsigned int *) find_max_a, (unsigned int) find_max_array2, 5, 0, 0);
    r = armemu(state);
    printf("Emulated (find_max_a([0, 0, 0, 0, 1000], 5)): %d\n", r);
    print_stats(state);

    printf("\n");

    int find_max_array3[1000] = {[0 ... 250] = -1, [251 ... 500] = 0, [501 ... 750] = 1, [751 ... 998] = 2, [999] = 3 };
    printf("Non-Emulated (find_max_a([-1, -1, -1, ... 0, 0, 0, ... 1, 1, 1, ... 2, 2, 2, ... 3], 1000)): %d\n", find_max_a(find_max_array3, 1000));
    arm_state_init(state, (unsigned int *) find_max_a, (unsigned int) find_max_array3, 1000, 0, 0);
    r = armemu(state);
    printf("Emulated (find_max_a([-1, -1, -1, ... , 0, 0, 0, ... 1, 1, 1, ... 2, 2, 2, ... 3], 1000)): %d\n", r);
    print_stats(state);

    printf("\n");
}

void print_fib_iter_tests(struct arm_state *state)
{
    unsigned int r;

    printf("\n----------------Begin Fib_Iter Tests------------------\n\n");    
    
    int i;

    printf("Non-Emulated (fib_iter_a(20): ");
    for(i = 0; i < 20; i++)
    {
        if(i == 19)
        {
            printf("%d)\n", fib_iter_a(i));
        }
        else
        {
            printf("%d, ", fib_iter_a(i));
        }
    }

    printf("Emulated (fib_iter_a(20): ");
    for(i = 0; i < 20; i++)
    {
        arm_state_init(state, (unsigned int *) fib_iter_a, i, 0, 0, 0);
        if (i == 19)
        {
            printf("%d)\n", armemu(state));
        }
        else
        {
            printf("%d, ", armemu(state));
        }
    }
    print_stats(state);
    printf("\n");
}

void print_fib_rec_tests(struct arm_state *state)
{
    unsigned int r;

    printf("\n----------------Begin Fib_Rec Tests------------------\n\n");    
    
    int i;

    printf("Non-Emulated (fib_rec_a(20): ");
    for(i = 0; i < 20; i++)
    {
        if(i == 19)
        {
            printf("%d)\n", fib_rec_a(i));
        }
        else
        {
            printf("%d, ", fib_rec_a(i));
        }
    }

    printf("Emulated (fib_rec_a(20): ");
    for(i = 0; i < 20; i++)
    {
        arm_state_init(state, (unsigned int *) fib_rec_a, i, 0, 0, 0);
        if (i == 19)
        {
            printf("%d)\n", armemu(state));
        }
        else
        {
            printf("%d, ", armemu(state));
        }
    }
    print_stats(state);
    printf("\n");
}

void print_str_len_tests(struct arm_state *state)
{
    unsigned int r;

    printf("\n----------------Begin Str_Len Tests------------------\n\n");        
    char* word = "0123456789";
    printf("Non-Emulated (strlen_a('%s')): %d\n", word, strlen_a(word));
    arm_state_init(state, (unsigned int *) strlen_a, (unsigned int) word, 0, 0, 0);
    r = armemu(state);
    printf("Emulated (strlen_a('%s')) = %d\n", word, r);
    print_stats(state);

    printf("\n");

    char* word1 = "mouse";
    printf("Non-Emulated (strlen_a('%s')): %d\n", word1, strlen_a(word1));
    arm_state_init(state, (unsigned int *) strlen_a, (unsigned int) word1, 0, 0, 0);
    r = armemu(state);
    printf("Emulated (strlen_a('%s')) = %d\n", word1, r);
    print_stats(state);
    
    printf("\n");

    char* word2 = "";
    printf("Non-Emulated (strlen_a('%s')): %d\n", word2, strlen_a(word2));
    arm_state_init(state, (unsigned int *) strlen_a, (unsigned int) word2, 0, 0, 0);
    r = armemu(state);
    printf("Emulated (strlen_a('%s')) = %d\n", word2, r);
    print_stats(state);

    printf("\n");

    char* word3 = "madeline";
    printf("Non-Emulated (strlen_a('%s')): %d\n", word3, strlen_a(word3));
    arm_state_init(state, (unsigned int *) strlen_a, (unsigned int) word3, 0, 0, 0);
    r = armemu(state);
    printf("Emulated (strlen_a('%s')) = %d\n", word3, r);
    print_stats(state);

    printf("\n");
}


/* Function to print out dynamic analysis of emulation */
void print_stats(struct arm_state *state)
{
    printf("\nProgram Statistics:\n");
    printf("-----------------------------------\n");
    printf("Total number of dp instructions: %d (%.1f%)\n", state->dp_inst_count, (double) state->dp_inst_count / state->total_inst_count * 100);
    printf("Total number of memory instructions: %d (%.1f%)\n", state->mem_inst_count, (double) state->mem_inst_count/state->total_inst_count * 100);
    printf("total number of branch instructions: %d (%.1f%)\n", state->branch_inst_count, (double) state->branch_inst_count/state->total_inst_count * 100);
    printf("Total number of instructions: %d\n", state->total_inst_count);
    printf("Total number of branches taken: %d\n", state->branch_taken);
    printf("Total number of branches not taken: %d\n", state->branch_not_taken);

    /* Add cache information here */

    cache_statistics_print(state->dmc);
}

void parse_command_line(int argc, char **argv, struct cache *dmc)
{
    int i;

    if(argc < 2)
    {
        dmc->size = 8;
    }
    else
    {
        for(i = 0; i < argc; i++)
        {
            if(strcmp(argv[i], "-c") == 0)
            {
                if(argv[i+1] == NULL)
                {
                    perror("Provide an argument for the cache size\n");
                    exit(1);
                }
                int input = atoi(argv[i+1]);
                if(input < 8 || input > 1024 || ((input & (input-1)) != 0)) 
                {
                    perror("Cache size must be a power of 2 and less than 1024.\n");
                    exit(1);
                }
                dmc->size = atoi(argv[i+1]);
            }
        }
    }
}

int main(int argc, char **argv)
{
    struct arm_state state;
    struct cache dmc;
    unsigned int r;

    parse_command_line(argc, argv, &dmc);

    struct_init(&dmc);
    state.dmc = &dmc;
    
    print_quadratic_tests(&state); 
    print_sum_array_tests(&state);
    print_find_max_tests(&state);
    print_fib_iter_tests(&state);
    print_fib_rec_tests(&state);
    print_str_len_tests(&state);    

    return 0;
}

