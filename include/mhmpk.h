#ifndef MHMPK_H
#define MHMPK_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sys/mman.h>



#define PKEY_ENABLE_ALL 0x0
#define PKEY_DISABLE_ACCESS 0x1
#define PKEY_DISABLE_WRITE 0x2
#define SYS_pkey_mprotect 0x149
#define SYS_pkey_alloc 0x14a
#define SYS_pkey_free 0x14b

#define META_SIZE 4096 * 4096

#define FILENAME "/dev/dax0.0"
size_t REGION_SIZE = 4294967296;
size_t ALIGN_SIZE = 2097152;
size_t TOTAL_PAGES = 1024;
//size_t meta_data_offset = 3L * 1024 * 1024 * 1024;

struct meta_mhmpk
{
    size_t offset;
    size_t size;
    int pkey;
};


void *start_addr;

meta_mhmpk *meta_start;

void mhmpk_begin(int pkey, int prot) {
    int success = pkey_set(pkey, prot);
    if(success  == -1){
        printf("pkey_set begin failed with pkey: %d\n", pkey);
    }
}

void mhmpk_end(int pkey) {
    int success = pkey_set(pkey, 3);
    if(success  == -1){
        printf("pkey_set 3 failed with pkey: %d\n", pkey);
    }
}

// unsigned int read_pkru() {
//     return _rdpkru();
// }

// void print_pkey_permissions(unsigned int pkru) {
//     for (int pkey = 1; pkey < 16; ++pkey) {
//         int wd = (pkru >> (2 * pkey)) & 1;     // Write Disable
//         int ad = (pkru >> (2 * pkey + 1)) & 1; // Access Disable
//         std::cout << "pkey " << pkey << ": ";
//         if (ad) {
//             std::cout << "No Access (AD=1, WD=" << wd << ")";
//         } else if (wd) {
//             std::cout << "Read Only (WD=1)";
//         } else {
//             std::cout << "Read/Write Allowed";
//         }
//         std::cout << std::endl;
//     }
// }

void pkey_init_in_thread(){
    int pkey = -1;
    printf("pkey_init_begin: %d\n", pkey);
    for (int i = 1; i <= 15; i++) {
        pkey = i;
        mhmpk_end(pkey);
        printf("pkey_init_: %d\n", pkey);
    }
}

void* mhmpk_init(size_t meta_mpk_offset, int fd) {
    int pkey = -1;
    for (int i = 0; i <= 15; i++) {
        pkey = pkey_alloc(0, PKEY_ENABLE_ALL);
        mhmpk_end(pkey);
    }
    fd = open(FILENAME, O_RDWR);
    //int fd = open("./tmp.log", O_RDWR | O_CREAT);
    if (fd == -1) {
        perror("Error opening file!");
        exit(EXIT_FAILURE);
    }
    //ftruncate(fd, REGION_SIZE);
    void * addr = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    start_addr = addr;
    meta_start = (struct meta_mhmpk *)((char*)addr + meta_mpk_offset);
    if (addr == MAP_FAILED) {
        perror("Error mmaping file!");
        exit(EXIT_FAILURE); 
    }
    return addr;
}

void mhmpk_free() {
    for (int i = 1; i < 16; i++) {
        pkey_free(i);
    }
}

// void *mhmpk_malloc(int pkey, size_t n_blocks, int name) {
//     // printf("%p\n", addr);
//     if (addr + n_blocks * ALIGN_SIZE > start_addr + TOTAL_PAGES * ALIGN_SIZE) {
//         fprintf(stderr, "No space left!\n");
//         exit(EXIT_FAILURE);  // Exit with failure status
//     }
//     // int success = pkey_mprotect(addr, n_blocks * 4096, PROT_READ | PROT_WRITE, pkey);
//     int success = 0;
//     success = syscall(SYS_pkey_mprotect, addr, n_blocks * ALIGN_SIZE, PROT_READ | PROT_WRITE, pkey); 
//         // int success = mprotect(addr, n_blocks * 4096, PROT_READ | PROT_WRITE);
//     if (success != 0) {
//         fprintf(stderr, "pkey_mprotect failed with pkey: %d\n", pkey);
//         fprintf(stderr, "Error: %s\n", strerror(errno));
//         exit(EXIT_FAILURE);  // Exit with failure status
//     }

//     (meta_start + name)->stt = addr;
//     (meta_start + name)->size = n_blocks * ALIGN_SIZE;
//     (meta_start + name)->pkey = pkey;
//     addr = addr + n_blocks * ALIGN_SIZE;
//     return (meta_start + name)->stt;
// }

// void mhmpk_attach(int name) {
//     void *stt = (meta_start + name)->stt;
//     size_t size = (meta_start + name)->size;
//     int pkey = (meta_start + name)->pkey;
//     int success = pkey_mprotect(stt, size, PROT_READ | PROT_WRITE, pkey);
//     if (success != 0) {
//         fprintf(stderr, "pkey_mprotect failed with pkey: %d\n", pkey);
//         exit(EXIT_FAILURE);  // Exit with failure status
//     }
// }

void mhmpk_slice(size_t mem_offset, size_t size, int pkey, int name) {
    //void *stt = (void*)((char*)start_addr + mem_offset);
    // int success = syscall(SYS_pkey_mprotect, stt, size, PROT_READ | PROT_WRITE, pkey);
    // if (success != 0) {
    //     fprintf(stderr, "pkey_mprotect failed with pkey: %d\n", pkey);
    //     fprintf(stderr, "Error: %s\n", strerror(errno));
    //     exit(EXIT_FAILURE);  // Exit with failure status
    // }
    (meta_start + name)->offset = mem_offset;
    (meta_start + name)->size = size;
    (meta_start + name)->pkey = pkey;
    printf("name: %d, offset: %ld, size: %ld, pkey: %d\n", name, (meta_start + name)->offset, (meta_start + name)->size, (meta_start + name)->pkey);
 
    return;
}

void mhmpk_attach_process(int name, int pro) {
    void *stt = (void*)((char*)start_addr + (meta_start + name)->offset);
    size_t size = (meta_start + name)->size;
    printf("start_addr:%p, stt: %p, size:%ld\n", start_addr, stt, size);
    int pkey = (meta_start + name)->pkey;
    int success = pkey_mprotect(stt, size, pro, pkey);
    if (success != 0) {
        fprintf(stderr, "pkey_mprotect failed with pkey: %d\n", pkey);
        exit(EXIT_FAILURE);  // Exit with failure status
    }

    return;
}


void mhmpk_attach_thread(int name, int pro) {
    //void *stt = (void*)((char*)start_addr + (meta_start + name)->offset);
    size_t size = (meta_start + name)->size;
    int pkey = (meta_start + name)->pkey;
    mhmpk_begin(pkey, pro);
    return;
}


void mhmpk_blocks_slice(size_t * mem_offset, size_t * size, int * pkey, int count) {
    for (int i = 0; i < count; i++) {
        mhmpk_slice(mem_offset[i], size[i], pkey[i], i);
    }
    return;
}

// int main() {
//     mhmpk_init();
//     int pkey = 1;
//     char *stt = (char *)mhmpk_malloc(pkey, 1, 0);
//     printf("%p\n", stt);
//     mhmpk_begin(pkey, 0);
//     *stt = 13;
//     printf("test: %d\n", *(char *)stt);
//     mhmpk_end(pkey);
//     printf("test: %d\n", *(char *)stt);
//     printf("%d\n", pkey_get(pkey));

//     mhmpk_free();
//     return 0;
// }




#endif // MHMPK_H
