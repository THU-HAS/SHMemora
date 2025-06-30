#include "../include/gshmp.h"
#include "../include/utils.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <iostream>
#include <cassert>

int total_insection_time = 0;
int insection_count = 0;



int safe_memcmp(const void* a, const void* b, size_t len) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < len; ++i) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

gshm_upr::gshm_upr() {
    srand(time(NULL)); 
    host_id = get_mac();
    //host_id = 0;
    is_first = 0;
    use_batch = 0;
    batch_kvs = 16;
    available_slots = batch_kvs;
    next_free_offset = (size_t *)malloc(sizeof(size_t));
    *next_free_offset = DATA_SEG_START + host_id * DATA_PARTITION_SIZE;
    open_cxl_device();
}
gshm_upr::~gshm_upr() {

    close_cxl_device();
}




void *gshm_upr::get_data_vaddr_by_offset(size_t offset) {
    return (data_addr + offset);
}

int gshm_upr::open_cxl_device() {
    size_t length = (ZU(1) << 32);

    dev_fd = open(CXL_DAX_DEV, O_RDWR);
    if (dev_fd <= 0) {
        perror("file error\n");
        exit(-1);
    }

    void *cxl_mem_addr = NULL;
    cxl_mem_addr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0);
    if (cxl_mem_addr == MAP_FAILED) {
        perror("mmap");
        printf("ERROR: %d\n", errno);
        exit(-1);
    }

    data_addr = cxl_mem_addr + HASH_TABLE_SIZE * sizeof(HashEntry);



    fprintf(stdout, "CXL DEV is open\n");
    return 0;
}

int gshm_upr::close_cxl_device() {
    size_t length = (ZU(1) << 32);

 
    if (dev_fd > 0) {
        if (close(dev_fd) != 0) {
            perror("close dev_fd failed");
            return -1;
        }
        dev_fd = -1;
    }

    fprintf(stdout, "CXL DEV is closed\n");
    return 0;
}


bool gshm_upr::get(uint8_t* key, uint8_t* value) {
    //timing_stats.get_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    // uint64_t* entry = ((uint64_t*)get_data_vaddr_by_offset(KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE)) + hash_offset;
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);

    uint64_t entry = 0;

    read_cxl(entry_offset, sizeof(uint64_t), &entry);

    while (entry != 0) {
    
        if (compare_cxl(entry, 64, key) == 0) { 
            read_cxl(entry + 64, value_size, value); 
            return true;
        }
        read_cxl(entry + 64 + value_size, sizeof(uint64_t), &entry); // Move to the next entry (64 + 256 bytes)
    }
    // while (*entry != 0) {
    //     uint8_t* data = static_cast<uint8_t*>(get_data_vaddr_by_offset(*entry));
    //     if (memcmp(data, key, 64) == 0) { 
    //         memcpy(value, data + 64, 256); 
    //         return true;
    //     }
    //     entry = reinterpret_cast<uint64_t*>(data + 320); // Move to the next entry (64 + 256 bytes)
    // }
    //timing_stats.get_stop_stamp();
    return false;
}


bool gshm_upr::update(uint8_t* key, uint8_t* value) {
    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);


    uint64_t entry = 0;

    read_cxl(entry_offset, sizeof(uint64_t), &entry);

    while (entry != 0) {
        if (compare_cxl(entry, 64, key) == 0) {
            write_cxl(entry + 64, value_size, value);
            return true;
        }
        read_cxl(entry + 64 + value_size, sizeof(uint64_t), &entry); 
    }

    return false;
}


bool gshm_upr::insert(uint8_t* key, uint8_t* value) {
    //timing_stats.insert_p_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);

    uint64_t zero = 0;

    // Calculate partition range
    size_t partition_start = DATA_SEG_START + host_id * DATA_PARTITION_SIZE;
    size_t partition_end = partition_start + DATA_PARTITION_SIZE;
    if (use_batch) {
        if (!available_slots) {
            available_slots = batch_kvs - 1;
            size_t start_offset = *next_free_offset;
            *next_free_offset += kv_size * batch_kvs;
            write_cxl(start_offset, 64, key);
            write_cxl(start_offset + 64, value_size, value);
            write_cxl(start_offset + 64 + value_size, sizeof(uint64_t), &zero);
            write_cxl(entry_offset, sizeof(uint64_t), &start_offset);
    
            return true;
        }
        else {
            size_t start_offset = *next_free_offset + (batch_kvs - available_slots) * kv_size;
            available_slots--;
            write_cxl(start_offset, 64, key);
            write_cxl(start_offset + 64, value_size, value);
            write_cxl(start_offset + 64 + value_size, sizeof(uint64_t), &zero);
            write_cxl(entry_offset, sizeof(uint64_t), &start_offset);    
            return true;        
        }
    }
    else {
        if (*next_free_offset + entry_size > partition_end) {
            fprintf(stderr, "Partition is full, cannot allocate new entry\n");
            return false;
           // exit(-1);
        }

 
        write_cxl(*next_free_offset, 64, key);
        write_cxl(*next_free_offset + 64, value_size, value);
        write_cxl(*next_free_offset + 64 + value_size, sizeof(uint64_t), &zero);

        write_cxl(entry_offset, sizeof(uint64_t), next_free_offset);
        *next_free_offset += entry_size;

        //timing_stats.insert_p_stop_stamp();

        return true;
    }
}



void gshm_upr::load_trace_and_insert(const std::string& trace_file, gshm_upr * store) {
    std::ifstream infile(trace_file);
    std::string line;

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string op, table, key_str;
        if (!(iss >> op >> table >> key_str)) continue;

        if (op != "INSERT") continue;

        
        uint8_t key_buf[64] = {0};
        size_t key_len = key_str.size();
        if (key_len > 64) key_len = 64;
        std::memcpy(key_buf, key_str.data(), key_len);

       
        const std::string value_str = "dummy_value_for_testing";
        uint8_t value_buf[256] = {0};
        std::memcpy(value_buf, value_str.data(), std::min(value_str.size(), size_t(256)));

        
        store->insert_p_start_stamp();
        if (!(store->insert(key_buf, value_buf))) {
            std::cerr << "Insert failed for key: " << key_str << std::endl;
        }
        store->insert_p_stop_stamp();
    }
}


bool gshm_upr::put(uint8_t* key, uint8_t* value) {
    //timing_stats.put_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    // uint64_t* entry = ((uint64_t*)get_data_vaddr_by_offset(KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE)) + hash_offset;
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
    
    // Calculate the start and end of the partition for this node using DATA_PARTITION_SIZE
    size_t partition_start = DATA_SEG_START + host_id * DATA_PARTITION_SIZE;
    size_t partition_end = partition_start + DATA_PARTITION_SIZE;

    uint64_t entry = 0; // entry itself is also an offset
    read_cxl(entry_offset, sizeof(uint64_t), &entry);

    while (entry != 0) {
        
        if (compare_cxl(entry, 64, key) == 0) { 
            write_cxl(entry + 64, 256, value); 
            return true;
        }        
        read_cxl(entry + 320, sizeof(uint64_t), &entry);
    }

    // Ensure there is enough space in the partition for the new entry (328 bytes)
    if (*next_free_offset + 328 > partition_end) {
        fprintf(stderr, "Partition is full, cannot allocate new entry\n");
        return false;
    }

    // Allocate space for the new entry
   
    write_cxl(*next_free_offset, 64, key); // Write the key
    write_cxl(*next_free_offset + 64, 256, value); // Write the value
    uint64_t zero = 0;
    write_cxl(*next_free_offset + 320, sizeof(uint64_t), &zero); // Set next pointer to 0
    // uint8_t* new_entry = static_cast<uint8_t*>(get_data_vaddr_by_offset(*next_free_offset));
    // memcpy(new_entry, key, 64);      // Copy 64-byte key
    // memcpy(new_entry + 64, value, 256); // Copy 256-byte value
    // *reinterpret_cast<uint64_t*>(new_entry + 320) = 0; // Set next pointer to 0

    // Update the hash table to point to the new entry
    write_cxl(entry_offset, sizeof(uint64_t), next_free_offset); // Update the hash table with the new entry offset

    // Increment the next free offset
    *next_free_offset += 328; // Increment by the total size of the entry (328 bytes)
    //timing_stats.put_stop_stamp();
    return true;
}


// bool gshm_upr::put(uint8_t* key, uint8_t* value) {
//     uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
//     uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
//     uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
//     shkey_t sk_hash = acquire(entry_offset, sizeof(uint64_t), 3);

//     size_t partition_start = DATA_SEG_START + host_id * DATA_PARTITION_SIZE;
//     size_t partition_end = partition_start + DATA_PARTITION_SIZE;

//     uint64_t entry = 0;
//     read_cxl(entry_offset, sizeof(uint64_t), &entry, sk_hash);

//     uint64_t prev_entry = 0;
//     while (entry != 0) {
//         shkey_t sk_data = acquire(entry, 328, 3);
//         if (compare_cxl(entry, 64, key, sk_data) == 0) {
//             write_cxl(entry + 64, 256, value, sk_data);
//             return true;
//         }
//         prev_entry = entry;
//         read_cxl(entry + 320, sizeof(uint64_t), &entry, sk_data);
//     }

//     if (*next_free_offset + 328 > partition_end) {
//         fprintf(stderr, "Partition full\n");
//         return false;
//     }

//     shkey_t sk_new = acquire(*next_free_offset, 328, 3);
//     write_cxl(*next_free_offset, 64, key, sk_new);
//     write_cxl(*next_free_offset + 64, 256, value, sk_new);
//     uint64_t zero = 0;
//     write_cxl(*next_free_offset + 320, sizeof(uint64_t), &zero, sk_new);

//     if (prev_entry == 0) {
//         
//         write_cxl(entry_offset, sizeof(uint64_t), next_free_offset, sk_hash);
//     } else {
//        
//         shkey_t sk_prev = acquire(prev_entry, 328, 3);
//         write_cxl(prev_entry + 320, sizeof(uint64_t), next_free_offset, sk_prev);
//     }

//     *next_free_offset += 328;
//     return true;
// }

bool gshm_upr::del(uint8_t* key) {
    //timing_stats.del_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
    //shkey_t key_hash_table = acquire(entry_offset, sizeof(uint64_t), 3);

    uint64_t entry = 0;
    //read_cxl(entry_offset, sizeof(uint64_t), &entry, key_hash_table);
    read_cxl(entry_offset, sizeof(uint64_t), &entry);

    uint64_t prev_entry_offset = 0;
    uint64_t prev_entry_next_ptr_offset = entry_offset;

    while (entry != 0) {
       
        if (compare_cxl(entry, 64, key) == 0) {
            uint64_t next_entry = 0;
            read_cxl(entry + 64 + value_size, sizeof(uint64_t), &next_entry);
           
            write_cxl(prev_entry_next_ptr_offset, sizeof(uint64_t), &next_entry);
           
            return true;
        }
        prev_entry_offset = entry;
        prev_entry_next_ptr_offset = entry + 64 + value_size;
        read_cxl(entry + 64 + value_size, sizeof(uint64_t), &entry);
    }

    //timing_stats.del_stop_stamp();
    return false;
}

int gshm_upr::read_cxl(size_t offset, size_t size, void *buf) {
    //timing_stats.reset();
    timing_stats.start_stamp();

   

    // memcpy(buf, this->get_data_vaddr_by_offset(offset), size);
    char *src = static_cast<char *>(this->get_data_vaddr_by_offset(offset));
    char *dest = static_cast<char *>(buf);
    for (size_t i = 0; i < size; ++i) {
        dest[i] = src[i];
    }

    timing_stats.data_access_stop_stamp();
    //timing_stats.print_stats();
    //timing_stats.print_stats_proportion();
    return 0;
}

// int gshm_upr::write_cxl(size_t offset, size_t size, const void *buf, shkey_t shkey) {
//     //timing_stats.reset();
//     timing_stats.start_stamp();

//     size_t hash_index = hash_function(shkey);
//     timing_stats.hash_stop_stamp();

//     size_t original_index = hash_index;
//     while (protection_table[hash_index].valid && protection_table[hash_index].shkey != shkey) {
//         hash_index = (hash_index + 1) % HASH_TABLE_SIZE;
//         if (hash_index == original_index) {
//             fprintf(stderr, "write1 Invalid or missing shkey\n");
//             return -1;
//         }
//     }
//     timing_stats.collision_stop_stamp();

//     HashEntry &Hentry = protection_table[hash_index];
//     if (!Hentry.valid || Hentry.shkey != shkey) {
//         fprintf(stderr, "write2 Invalid or missing shkey\n");
//         return -1;
//     }

//     if (Hentry.Pentry.size == 0) {
//         fprintf(stderr, "Invalid memory region for shkey\n");
//         return -1;
//     }

//     if (offset < Hentry.Pentry.offset || offset + size > Hentry.Pentry.offset + Hentry.Pentry.size) {
//         //fprintf(stderr, "Access out of bounds\n");
//         return -1;
//     }

//     if ((Hentry.Pentry.permission & 2) == 0) { // Check write permission
//         fprintf(stderr, "Write access denied\n");
//         return -1;
//     }
//     timing_stats.permission_check_stop_stamp();
//     //memcpy(this->get_data_vaddr_by_offset(offset), buf, size);
//     char *dest = static_cast<char *>(this->get_data_vaddr_by_offset(offset));
//     const char *src = static_cast<const char *>(buf);
//     for (size_t i = 0; i < size; ++i) {
//         dest[i] = src[i];
//     }

//     timing_stats.data_access_stop_stamp();
//     //timing_stats.print_stats();
//     //timing_stats.print_stats_proportion();
//     return 0;
// }

int gshm_upr::write_cxl(size_t offset, size_t size, const void *buf) {
    //printf("[write_cxl] offset = %lu, size = %lu, shkey = 0x%lx\n", offset, size, shkey);
    assert(buf != nullptr);

    timing_stats.start_stamp();

    

    // Check get_data_vaddr_by_offset
    void *dest_void = this->get_data_vaddr_by_offset(offset);
    if (dest_void == nullptr) {
        fprintf(stderr, "[write_cxl] ERROR: get_data_vaddr_by_offset returned nullptr!\n");
        return -1;
    }

    char *dest = static_cast<char *>(dest_void);
    const char *src = static_cast<const char *>(buf);

    // Debug memory addresses
    //printf("[write_cxl] dest = %p, src = %p\n", (void*)dest, (const void*)src);

    for (size_t i = 0; i < size; ++i) {
        dest[i] = src[i];  
    }

    timing_stats.data_access_stop_stamp();
    return 0;
}


int gshm_upr::compare_cxl(size_t offset, size_t size, const void *buf) {
    //timing_stats.reset();
    timing_stats.start_stamp();

    

    // Compare memory region with buffer
    char *src = static_cast<char *>(this->get_data_vaddr_by_offset(offset));
    const char *cmp = static_cast<const char *>(buf);
    int result = safe_memcmp(src, cmp, size);

    timing_stats.data_access_stop_stamp();
    //timing_stats.print_stats();
    //timing_stats.print_stats_proportion();
    return result;
}







