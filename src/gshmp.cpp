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
#include "../include/mhmpk.h"

int total_insection_time = 0;
int insection_count = 0;

size_t hash_function(shkey_t shkey) {
    //return shkey % HASH_TABLE_SIZE;
    return (shkey * 11400714819323198485ull) >> (64 - 20);
}

const uint32_t gshm::SHKEY_SECRET_KEY = 0xA5B35791;

static uint32_t simple_hash32(uint32_t data) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 4; ++i) {
        hash ^= (data >> (i * 8)) & 0xFF;
        hash *= 16777619u;
    }
    return hash;
}

int safe_memcmp(const void* a, const void* b, size_t len) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < len; ++i) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

gshm::gshm() {
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
gshm::~gshm() {
    
    close_cxl_device();
}

size_t gshm::find_available_slot(shkey_t shkey) {
    size_t hash_index = hash_function(shkey);
    size_t original_index = hash_index;

    while (protection_table[hash_index].valid && protection_table[hash_index].shkey != shkey) {
        hash_index = (hash_index + 1) % HASH_TABLE_SIZE;
        if (hash_index == original_index) {
            fprintf(stderr, "Hash table is full\n");
            exit(-1);
        }
    }
    return hash_index;
}

int gshm::insert_protection_entry(shkey_t shkey, size_t offset, size_t size, int permission) {
    size_t hash_index = hash_function(shkey);
    size_t original_index = hash_index;
    int fail_count = 0;
    do {
        bool expected = false;
       
        if (protection_table[hash_index].valid.compare_exchange_weak(expected, true)) {
            
            
            protection_table[hash_index].shkey = shkey;
            protection_table[hash_index].Pentry = {offset, size, permission};

            return 0;
        } else if (protection_table[hash_index].shkey == shkey) {
        
            //protection_table[hash_index].Pentry = {offset, size, permission};
           
            return -2; 
        }


      
        hash_index = (hash_index + 1) % HASH_TABLE_SIZE;
        if (hash_index == original_index) {
            fprintf(stderr, "Hash table is full\n");
            return -1;
        }

        // if (++fail_count > 10) {
        //     std::this_thread::sleep_for(std::chrono::microseconds(1 << std::min(fail_count, 10)));
        // }

    } while (true);
  


}


shkey_t gshm::generate_shkey() {
    if (use_mac) {
        uint32_t token_id = static_cast<uint32_t>(rand());
        uint32_t hmac = simple_hash32(token_id ^ SHKEY_SECRET_KEY);
        uint64_t shkey = (static_cast<uint64_t>(token_id) << 32) | hmac;
        return static_cast<shkey_t>(shkey);
    }
    else    
        return static_cast<shkey_t>((uint64_t)rand() % KEY_SPACE);
}

void *gshm::get_data_vaddr_by_offset(size_t offset) {
    return (data_addr + offset);
}

int gshm::open_cxl_device() {
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

    protection_table = static_cast<HashEntry *>(cxl_mem_addr);
    memset(protection_table, 0, HASH_TABLE_SIZE * sizeof(HashEntry));

    fprintf(stdout, "CXL DEV is open\n");
    return 0;
}

int gshm::close_cxl_device() {
    size_t length = (ZU(1) << 32); 

    if (protection_table != nullptr) {
        void* cxl_base_addr = static_cast<void*>(protection_table); 
        if (munmap(cxl_base_addr, length) != 0) {
            perror("munmap failed");
            return -1;
        }
        protection_table = nullptr;
        data_addr = nullptr;
    }

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


bool gshm::get(uint8_t* key, uint8_t* value, shkey_t shkey) {
    //timing_stats.get_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    // uint64_t* entry = ((uint64_t*)get_data_vaddr_by_offset(KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE)) + hash_offset;
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
    shkey_t key_hash_table = acquire(entry_offset, sizeof(uint64_t), 1);
    uint64_t entry = 0;
    //read_cxl(entry_offset, sizeof(uint64_t), &entry, key_hash_table);
    read_cxl(entry_offset, sizeof(uint64_t), &entry, global_key);

    while (entry != 0) {
        shkey_t key_data = acquire(entry, entry_size, 1);
        if (compare_cxl(entry, 64, key, global_key) == 0) { 
            read_cxl(entry + 64, value_size, value, shkey); 
            return true;
        }
        read_cxl(entry + 64 + value_size, sizeof(uint64_t), &entry, global_key); // Move to the next entry (64 + 256 bytes)
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


bool gshm::update(uint8_t* key, uint8_t* value, shkey_t shkey) {
    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
    shkey_t key_hash_table = acquire(entry_offset, sizeof(uint64_t), 3);

    uint64_t entry = 0;
    //read_cxl(entry_offset, sizeof(uint64_t), &entry, key_hash_table);
    read_cxl(entry_offset, sizeof(uint64_t), &entry, global_key);

    while (entry != 0) {
        shkey_t key_data = acquire(entry, entry_size, 3);
        if (compare_cxl(entry, 64, key, global_key) == 0) {
            write_cxl(entry + 64, value_size, value, shkey);
            return true;
        }
        read_cxl(entry + 64 + value_size, sizeof(uint64_t), &entry, global_key); // Move to the next entry (64 + 256 bytes)
    }

    return false;
}


shkey_t gshm::insert(uint8_t* key, uint8_t* value) {
    //timing_stats.insert_p_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
    shkey_t key_hash_table = acquire(entry_offset, sizeof(uint64_t), 3);
    uint64_t zero = 0;

    // Calculate partition range
    size_t partition_start = DATA_SEG_START + host_id * DATA_PARTITION_SIZE;
    size_t partition_end = partition_start + DATA_PARTITION_SIZE;
    if (use_batch) {
        if (!available_slots) {
            available_slots = batch_kvs - 1;
            size_t start_offset = *next_free_offset;
            *next_free_offset += kv_size * batch_kvs;
            shkey_t key_new_entry = acquire(start_offset, kv_size * batch_kvs, 3);
            write_cxl(start_offset, 64, key, key_new_entry);
            write_cxl(start_offset + 64, value_size, value, key_new_entry);
            write_cxl(start_offset + 64 + value_size, sizeof(uint64_t), &zero, key_new_entry);
            write_cxl(entry_offset, sizeof(uint64_t), &start_offset, key_hash_table);
            batch_key = key_new_entry;
            return batch_key;
        }
        else {
            size_t start_offset = *next_free_offset + (batch_kvs - available_slots) * kv_size;
            available_slots--;
            write_cxl(start_offset, 64, key, batch_key);
            write_cxl(start_offset + 64, value_size, value, batch_key);
            write_cxl(start_offset + 64 + value_size, sizeof(uint64_t), &zero, batch_key);
            write_cxl(entry_offset, sizeof(uint64_t), &start_offset, key_hash_table);    
            return batch_key;        
        }
    }
    else {
        if (*next_free_offset + entry_size > partition_end) {
            fprintf(stderr, "Partition is full, cannot allocate new entry\n");
            //return false;
            exit(-1);
        }

        shkey_t key_new_entry = acquire(*next_free_offset, entry_size, 3);
        write_cxl(*next_free_offset, 64, key, key_new_entry);
        write_cxl(*next_free_offset + 64, value_size, value, key_new_entry);
        write_cxl(*next_free_offset + 64 + value_size, sizeof(uint64_t), &zero, key_new_entry);

        write_cxl(entry_offset, sizeof(uint64_t), next_free_offset, key_hash_table);
        *next_free_offset += entry_size;

        //timing_stats.insert_p_stop_stamp();

        return key_new_entry;
    }
}



void gshm::load_trace_and_insert(const std::string& trace_file, gshm * store) {
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


bool gshm::put(uint8_t* key, uint8_t* value) {
    //timing_stats.put_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    // uint64_t* entry = ((uint64_t*)get_data_vaddr_by_offset(KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE)) + hash_offset;
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
    shkey_t key_hash_table = acquire(entry_offset, sizeof(uint64_t), 3);
    // Calculate the start and end of the partition for this node using DATA_PARTITION_SIZE
    size_t partition_start = DATA_SEG_START + host_id * DATA_PARTITION_SIZE;
    size_t partition_end = partition_start + DATA_PARTITION_SIZE;

    uint64_t entry = 0; // entry itself is also an offset
    read_cxl(entry_offset, sizeof(uint64_t), &entry, key_hash_table);

    while (entry != 0) {
        shkey_t key_data = acquire(entry, 328, 3);
        if (compare_cxl(entry, 64, key, key_data) == 0) { 
            write_cxl(entry + 64, 256, value, key_data); 
            return true;
        }        
        read_cxl(entry + 320, sizeof(uint64_t), &entry, key_data);
    }

    // Ensure there is enough space in the partition for the new entry (328 bytes)
    if (*next_free_offset + 328 > partition_end) {
        fprintf(stderr, "Partition is full, cannot allocate new entry\n");
        return false;
    }

    // Allocate space for the new entry
    shkey_t key_new_entry = acquire(*next_free_offset, 328, 3);
    write_cxl(*next_free_offset, 64, key, key_new_entry); // Write the key
    write_cxl(*next_free_offset + 64, 256, value, key_new_entry); // Write the value
    uint64_t zero = 0;
    write_cxl(*next_free_offset + 320, sizeof(uint64_t), &zero, key_new_entry); // Set next pointer to 0
    // uint8_t* new_entry = static_cast<uint8_t*>(get_data_vaddr_by_offset(*next_free_offset));
    // memcpy(new_entry, key, 64);      // Copy 64-byte key
    // memcpy(new_entry + 64, value, 256); // Copy 256-byte value
    // *reinterpret_cast<uint64_t*>(new_entry + 320) = 0; // Set next pointer to 0

    // Update the hash table to point to the new entry
    write_cxl(entry_offset, sizeof(uint64_t), next_free_offset, key_hash_table); // Update the hash table with the new entry offset

    // Increment the next free offset
    *next_free_offset += 328; // Increment by the total size of the entry (328 bytes)
    //timing_stats.put_stop_stamp();
    return true;
}

bool gshm::del(uint8_t* key, shkey_t shkey) {
    //timing_stats.del_start_stamp();

    uint64_t partition_id = (reinterpret_cast<uint64_t*>(key)[0] >> 56);
    uint64_t hash_offset = reinterpret_cast<uint64_t*>(key)[0] & ((ZU(1) << 17) - 1);
    uint64_t entry_offset = KV_HASH_TABLE_START + partition_id * KV_HASH_TABLE_SIZE + hash_offset * sizeof(uint64_t);
    //shkey_t key_hash_table = acquire(entry_offset, sizeof(uint64_t), 3);

    uint64_t entry = 0;
    //read_cxl(entry_offset, sizeof(uint64_t), &entry, key_hash_table);
    read_cxl(entry_offset, sizeof(uint64_t), &entry, global_key);

    uint64_t prev_entry_offset = 0;
    uint64_t prev_entry_next_ptr_offset = entry_offset;

    while (entry != 0) {
        //shkey_t key_data = acquire(entry, 328, 3);
        if (compare_cxl(entry, 64, key, global_key) == 0) {
            uint64_t next_entry = 0;
            read_cxl(entry + 64 + value_size, sizeof(uint64_t), &next_entry, shkey);
            //write_cxl(prev_entry_next_ptr_offset, sizeof(uint64_t), &next_entry, key_hash_table);
            write_cxl(prev_entry_next_ptr_offset, sizeof(uint64_t), &next_entry, global_key);
            // Optionally clear the entry's memory here if desired.
            return true;
        }
        prev_entry_offset = entry;
        prev_entry_next_ptr_offset = entry + 64 + value_size;
        read_cxl(entry + 64 + value_size, sizeof(uint64_t), &entry, global_key);
    }

    //timing_stats.del_stop_stamp();
    return false;
}

int gshm::read_cxl(size_t offset, size_t size, void *buf, shkey_t shkey) {
    //timing_stats.reset();
    timing_stats.start_stamp();

    int pkey = 1;

    size_t hash_index = hash_function(shkey);
    timing_stats.hash_stop_stamp();
    
    size_t original_index = hash_index;
    while ((protection_table[hash_index].valid && protection_table[hash_index].shkey != shkey)&&shkey!=global_key) {
        hash_index = (hash_index + 1) % HASH_TABLE_SIZE;
        if (hash_index == original_index) {
            fprintf(stderr, "read Invalid or missing shkey\n");
            return -1;
        }
    }
    timing_stats.collision_stop_stamp();

    HashEntry &Hentry = protection_table[hash_index];
    if ((!Hentry.valid || Hentry.shkey != shkey)&& shkey!=global_key) {
        fprintf(stderr, "read Invalid or missing shkey\n");
        return -1;
    }

    if (Hentry.Pentry.size == 0 && shkey!=global_key) {
        fprintf(stderr, "Invalid memory region for shkey\n");
        return -1;
    }

    if ((offset < Hentry.Pentry.offset || offset + size > Hentry.Pentry.offset + Hentry.Pentry.size)&&shkey!=global_key) {
        fprintf(stderr, "Access out of bounds\n");
        return -1;
    }

    if (((Hentry.Pentry.permission & 1) == 0)&&shkey!=global_key) { // Check read permission
        fprintf(stderr, "Read access denied\n");
        return -1;
    }
    timing_stats.permission_check_stop_stamp();

    pkey_mprotect(this->get_data_vaddr_by_offset(offset), size, PROT_READ, pkey);

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

int gshm::write_cxl(size_t offset, size_t size, const void *buf, shkey_t shkey) {
    //printf("[write_cxl] offset = %lu, size = %lu, shkey = 0x%lx\n", offset, size, shkey);
    assert(buf != nullptr);

    timing_stats.start_stamp();

    int pkey = 1;

    size_t hash_index = hash_function(shkey);
    //printf("[write_cxl] hash_index = %lu\n", hash_index);
    timing_stats.hash_stop_stamp();

    size_t original_index = hash_index;
    size_t probe_count = 0;

    while (protection_table[hash_index].valid && protection_table[hash_index].shkey != shkey && shkey != global_key) {
        hash_index = (hash_index + 1) % HASH_TABLE_SIZE;
        probe_count++;
        if (probe_count > HASH_TABLE_SIZE) {
            //fprintf(stderr, "[write_cxl] ERROR: Hash table full or infinite loop\n");
            return -1;
        }
        if (hash_index == original_index) {
            //fprintf(stderr, "[write_cxl] ERROR: write1 Invalid or missing shkey\n");
            return -1;
        }
    }

    timing_stats.collision_stop_stamp();

    HashEntry &Hentry = protection_table[hash_index];

    if ((!Hentry.valid || Hentry.shkey != shkey)&& shkey != global_key) {
        //fprintf(stderr, "[write_cxl] ERROR: write2 Invalid or missing shkey (hash_index=%lu)\n", hash_index);
        return -1;
    }

    //printf("[write_cxl] Found protection entry: offset = %lu, size = %lu, perm = %d\n",
           //Hentry.Pentry.offset, Hentry.Pentry.size, Hentry.Pentry.permission);

    if (Hentry.Pentry.size == 0&& shkey != global_key) {
        fprintf(stderr, "[write_cxl] ERROR: Invalid memory region for shkey\n");
        return -1;
    }

    if ((offset < Hentry.Pentry.offset || offset + size > Hentry.Pentry.offset + Hentry.Pentry.size)&&shkey != global_key) {
        //fprintf(stderr, "[write_cxl] ERROR: Access out of bounds! offset = %lu, size = %lu, region_start = %lu, region_end = %lu\n", offset, size, Hentry.Pentry.offset, Hentry.Pentry.offset + Hentry.Pentry.size);
        return -1;
    }

    if ((Hentry.Pentry.permission & 2) == 0 && shkey != global_key) { // Check write permission
        fprintf(stderr, "[write_cxl] ERROR: Write access denied\n");
        return -1;
    }

    timing_stats.permission_check_stop_stamp();

    // Check get_data_vaddr_by_offset
    void *dest_void = this->get_data_vaddr_by_offset(offset);
    if (dest_void == nullptr) {
        fprintf(stderr, "[write_cxl] ERROR: get_data_vaddr_by_offset returned nullptr!\n");
        return -1;
    }

    pkey_mprotect(dest_void, size, PROT_WRITE, pkey);


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


int gshm::compare_cxl(size_t offset, size_t size, const void *buf, shkey_t shkey) {
    //timing_stats.reset();
    timing_stats.start_stamp();

    size_t hash_index = hash_function(shkey);
    timing_stats.hash_stop_stamp();

    size_t original_index = hash_index;
    while (protection_table[hash_index].valid && protection_table[hash_index].shkey != shkey && shkey != global_key) {
        hash_index = (hash_index + 1) % HASH_TABLE_SIZE;
        if (hash_index == original_index) {
            fprintf(stderr, "com Invalid or missing shkey\n");
            return -2;
        }
    }
    timing_stats.collision_stop_stamp();

    HashEntry &Hentry = protection_table[hash_index];
    if ((!Hentry.valid || Hentry.shkey != shkey)&& shkey != global_key) {
        fprintf(stderr, "com Invalid or missing shkey\n");
        return -2;
    }

    if (Hentry.Pentry.size == 0 && shkey != global_key) {
        fprintf(stderr, "Invalid memory region for shkey\n");
        return -2;
    }

    if ((offset < Hentry.Pentry.offset || offset + size > Hentry.Pentry.offset + Hentry.Pentry.size)&&shkey != global_key) {
        fprintf(stderr, "Access out of bounds\n");
        return -2;
    }

    if ((Hentry.Pentry.permission & 1) == 0&&shkey != global_key) {
        fprintf(stderr, "Read access denied\n");
        return -2;
    }
    timing_stats.permission_check_stop_stamp();

    // Compare memory region with buffer
    char *src = static_cast<char *>(this->get_data_vaddr_by_offset(offset));
    const char *cmp = static_cast<const char *>(buf);
    int result = safe_memcmp(src, cmp, size);

    timing_stats.data_access_stop_stamp();
    //timing_stats.print_stats();
    //timing_stats.print_stats_proportion();
    return result;
}


int gshm::revoke_cxl(shkey_t shkey) {
    size_t hash_index = hash_function(shkey);
    size_t original_index = hash_index;
    size_t probe_count = 0;

 
    while (protection_table[hash_index].valid && protection_table[hash_index].shkey != shkey && shkey != global_key) {
        hash_index = (hash_index + 1) % HASH_TABLE_SIZE;
        probe_count++;

        if (probe_count > HASH_TABLE_SIZE) {
            fprintf(stderr, "[revoke_cxl] ERROR: Hash table full or infinite loop\n");
            return -1;
        }
        if (hash_index == original_index) {
            fprintf(stderr, "[revoke_cxl] ERROR: Invalid or missing shkey\n");
            return -1;
        }
    }

    HashEntry &Hentry = protection_table[hash_index];


    if ((!Hentry.valid || Hentry.shkey != shkey) && shkey != global_key) {
        fprintf(stderr, "[revoke_cxl] ERROR: No matching entry found\n");
        return -1;
    }


    Hentry.valid = false;

    // optional
    // memset(&Hentry.Pentry, 0, sizeof(ProtectionEntry));
    // Hentry.shkey = 0;

    //fprintf(stdout, "[revoke_cxl] SUCCESS: Entry with shkey 0x%lx revoked\n", shkey);
    return 0;
}

shkey_t gshm::acquire(size_t offset, size_t size, uint8_t permission) {
    shkey_t shkey = generate_shkey();
    //timing_stats.reset();
    timing_stats.start_stamp();
    int success = insert_protection_entry(shkey, offset, size, permission);
    // while(success == -2) { 
    //     shkey = generate_shkey();
    //     success = insert_protection_entry(shkey, offset, size, permission);
    // }
    timing_stats.insert_stop_stamp();
    return shkey;
}


void gshm::clear_protection_table() {
    for (size_t i = 0; i < HASH_TABLE_SIZE; ++i) {
        protection_table[i].valid.store(false);  // 标记为无效
        protection_table[i].shkey = 0;
        protection_table[i].Pentry = {0, 0, 0};
    }
    //printf("[Init] Protection table cleared.\n");
}
