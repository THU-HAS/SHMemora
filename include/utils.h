#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <x86intrin.h>
#include <fcntl.h>
#include <net/if.h>
#include <string.h>
#include <cinttypes>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <sys/ioctl.h>


uint64_t get_mac();
