#include "VirtualMemory.h"

#include <cstdio>
#include <cassert>

int main(int argc, char **argv) {
    VMinitialize();
    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        printf("writing to %llu\n", (long long int) i);
        VMwrite(5 * i * PAGE_SIZE, i*100);
    }

    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        word_t value;
        VMread(5 * i * PAGE_SIZE, &value);
        printf("reading from %llu %d\n", (long long int) i, value);
        assert(uint64_t(value) == i*100);
    }
    printf("success\n");

    return 0;
}

// even simpler :)
//int main(int argc, char *argv[]){
//  VMinitialize();
//  printf("after init\n");
//  word_t save;
//  save = 555;
//  VMwrite(3, save);
//
//  uint64_t addresses[TABLES_DEPTH + 1]; // + 1 for offset
//  split_virtual_address (0b10011001010101110011, addresses);
//  printf ("after split\n");
//
//  word_t * place_holder;
//  VMread(3, place_holder);
//  printf("%d", *place_holder);
//}