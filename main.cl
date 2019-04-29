kernel void cl_main(global void* arg) {
    void (^block)() = ^{
        printf("Hello block\n");
    };
    block();
    printf("Sizeof block is %u", sizeof(block));
    printf("Block ptr is %08x\n", (void*)block);
}
