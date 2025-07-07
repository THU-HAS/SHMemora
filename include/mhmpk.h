#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sys/mman.h>

void mpk_begin(int pkey, int prot) {
    int success = pkey_set(pkey, prot);
    if(success  == -1){
        printf("pkey_set begin failed with pkey: %d\n", pkey);
    }
}

void mpk_end(int pkey) {
    int success = pkey_set(pkey, 3);
    if(success  == -1){
        printf("pkey_set 3 failed with pkey: %d\n", pkey);
    }
}
