#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <cinttypes>
#include <time.h>
#include "stats.h"
#include <atomic>
#include <iostream>
#include <array>

#define ZU(x)  x##ULL
#define CXL_DAX_DEV "/dev/dax0.0"
//#define CXL_DAX_DEV "/tmp/dax_dev"

#define KEY_SPACE (ZU(1) << 20) // 1G keys
#define HASH_TABLE_SIZE  (ZU(1)<<25) // 1M entries
#define KV_HASH_TABLE_START           (ZU(1)<<26)
#define KV_HASH_TABLE_SIZE            (ZU(1)<<20)
#define DATA_SEG_START                (ZU(1)<<27)
#define DATA_PARTITION_SIZE           (ZU(1)<<30)

typedef uint16_t shkey_t;

// Use a struct for shkey_t: 64-bit token_id + 64-bit hmac
// struct shkey_t {
//     uint64_t token_id;
//     uint64_t hmac;
//     bool operator==(const shkey_t& other) const {
//         return token_id == other.token_id && hmac == other.hmac;
//     }
// };

struct ProtectionEntry {
    size_t offset;
    size_t size;
    int permission;
};

struct HashEntry {
    shkey_t shkey;
    ProtectionEntry Pentry;
    //bool valid;
    std::atomic<bool> valid;
};

class gshm {
    private:
        void *data_addr;
        HashEntry *protection_table;
        TimingStats timing_stats;
        uint64_t host_id;
        size_t *next_free_offset;
        int dev_fd;
        int is_first;
        //int use_batch;
        //size_t kv_size = 328;
        //size_t batch_kvs;
        shkey_t batch_key;
        size_t available_slots;//number of kv pairs in a batch
        shkey_t global_key = 0;
        //int use_mac = 0;
        static const uint32_t SHKEY_SECRET_KEY;

    public:
        gshm();
        ~gshm();

        int value_size = 256;
        int entry_size = 64 + 8 + value_size; // 64 + 256 + 8
        int use_batch = 0;
        size_t kv_size = entry_size; // 64 + 8 + value_size
        size_t batch_kvs;
        int use_mac = 0;

        void *get_data_vaddr_by_offset(size_t offset);
        uint64_t get_host_id() { return host_id; }

        size_t find_available_slot(shkey_t shkey);
        int insert_protection_entry(shkey_t shkey, size_t offset, size_t size, int permission);
        shkey_t generate_shkey();

        void timereset() {
            timing_stats.reset();
        };
        void trans_read_start_stamp() {
            timing_stats.YCSB_read_start_stamp();
        };
        void trans_update_start_stamp() {
            timing_stats.YCSB_update_start_stamp();
        };
        void trans_insert_start_stamp() {
            timing_stats.YCSB_insert_start_stamp();
        };

        void put_start_stamp() {
            timing_stats.put_start_stamp();
        };
        void insert_p_start_stamp() {
            timing_stats.insert_p_start_stamp();
        };
        void update_p_start_stamp() {
            timing_stats.update_pstart_stamp();
        };

        void trans_start_stamp() {
            timing_stats.trans_start_stamp();
        };
        
        void get_start_stamp() {
            timing_stats.get_start_stamp();
        };
        void del_start_stamp() {
            timing_stats.del_start_stamp();
        };


        void trans_read_stop_stamp() {
            timing_stats.YCSB_read_stop_stamp();
        };
        void trans_update_stop_stamp() {
            timing_stats.YCSB_update_stop_stamp();
        };
        void trans_insert_stop_stamp() {
            timing_stats.YCSB_insert_stop_stamp();
        };

        void put_stop_stamp() {
            timing_stats.put_stop_stamp();
        };
        void insert_p_stop_stamp() {
            timing_stats.insert_p_stop_stamp();
        };
        void update_p_stop_stamp() {
            timing_stats.update_p_stop_stamp();
        };
        void trans_stop_stamp() {
            timing_stats.trans_stop_stamp();
        };

        void get_stop_stamp() {
            timing_stats.get_stop_stamp();
        };
        void del_stop_stamp() {
            timing_stats.del_stop_stamp();
        };

        void print_put() {
            timing_stats.print_put_proportion();
        };
        void print_insert() {
            timing_stats.print_insert_proportion();
        };
        void print_update() {
            timing_stats.print_update_proportion();
        };
        void print_get() {
            timing_stats.print_get_proportion();
        };

        void print_del() {
            timing_stats.print_del_proportion();
        };
        
        void print_trans() {
            timing_stats.print_trans_proportion();
        };

        bool get(uint8_t* key, uint8_t* value, shkey_t shkey);
        bool put(uint8_t* key, uint8_t* value);
        bool del(uint8_t* key, shkey_t shkey);

        void load_trace_and_insert(const std::string& trace_file, gshm * store);

        bool update(uint8_t* key, uint8_t* value, shkey_t shkey);
        
        shkey_t insert(uint8_t* key, uint8_t* value);

        int open_cxl_device();
        shkey_t acquire(size_t offset, size_t size, uint8_t permission);
        int read_cxl(size_t offset, size_t size, void *buf, shkey_t shkey);
        int write_cxl(size_t offset, size_t size, const void *buf, shkey_t shkey);
        int compare_cxl(size_t offset, size_t size, const void *buf, shkey_t shkey);
        int revoke_cxl(shkey_t shkey);
        int close_cxl_device();


        bool get_origin(uint8_t* key, uint8_t* value);
        bool put_origin(uint8_t* key, uint8_t* value);
        bool del_origin(uint8_t* key);

        shkey_t acquire_or(size_t offset, size_t size, uint8_t permission);
        int read_cxl_or(size_t offset, size_t size, void *buf, shkey_t shkey);
        int write_cxl_or(size_t offset, size_t size, const void *buf, shkey_t shkey);
        int compare_cxl_or(size_t offset, size_t size, const void *buf, shkey_t shkey);


        void clear_protection_table();


};



class gshm_upr {
    private:
        void *data_addr;
    
        TimingStats timing_stats;
        uint64_t host_id;
        size_t *next_free_offset;
        int dev_fd;
        int is_first;
        //int use_batch;
        //size_t kv_size = 328;
        //size_t batch_kvs;
 
        size_t available_slots;//number of kv pairs in a batch


    public:
        gshm_upr();
        ~gshm_upr();

        int value_size = 256;
        int entry_size = 64 + 8 + value_size; // 64 + 256 + 8
        int use_batch = 0;
        size_t kv_size = entry_size; // 64 + 8 + value_size
        size_t batch_kvs;

        void *get_data_vaddr_by_offset(size_t offset);
        uint64_t get_host_id() { return host_id; }

        size_t find_available_slot(shkey_t shkey);
        int insert_protection_entry(shkey_t shkey, size_t offset, size_t size, int permission);
        shkey_t generate_shkey();

        void timereset() {
            timing_stats.reset();
        };
        void trans_read_start_stamp() {
            timing_stats.YCSB_read_start_stamp();
        };
        void trans_update_start_stamp() {
            timing_stats.YCSB_update_start_stamp();
        };
        void trans_insert_start_stamp() {
            timing_stats.YCSB_insert_start_stamp();
        };

        void put_start_stamp() {
            timing_stats.put_start_stamp();
        };
        void insert_p_start_stamp() {
            timing_stats.insert_p_start_stamp();
        };
        void update_p_start_stamp() {
            timing_stats.update_pstart_stamp();
        };

        void trans_start_stamp() {
            timing_stats.trans_start_stamp();
        };
        
        void get_start_stamp() {
            timing_stats.get_start_stamp();
        };
        void del_start_stamp() {
            timing_stats.del_start_stamp();
        };


        void trans_read_stop_stamp() {
            timing_stats.YCSB_read_stop_stamp();
        };
        void trans_update_stop_stamp() {
            timing_stats.YCSB_update_stop_stamp();
        };
        void trans_insert_stop_stamp() {
            timing_stats.YCSB_insert_stop_stamp();
        };

        void put_stop_stamp() {
            timing_stats.put_stop_stamp();
        };
        void insert_p_stop_stamp() {
            timing_stats.insert_p_stop_stamp();
        };
        void update_p_stop_stamp() {
            timing_stats.update_p_stop_stamp();
        };
        void trans_stop_stamp() {
            timing_stats.trans_stop_stamp();
        };

        void get_stop_stamp() {
            timing_stats.get_stop_stamp();
        };
        void del_stop_stamp() {
            timing_stats.del_stop_stamp();
        };

        void print_put() {
            timing_stats.print_put_proportion();
        };
        void print_insert() {
            timing_stats.print_insert_proportion();
        };
        void print_update() {
            timing_stats.print_update_proportion();
        };
        void print_get() {
            timing_stats.print_get_proportion();
        };

        void print_del() {
            timing_stats.print_del_proportion();
        };
        
        void print_trans() {
            timing_stats.print_trans_proportion();
        };

        bool get(uint8_t* key, uint8_t* value);
        bool put(uint8_t* key, uint8_t* value);
        bool del(uint8_t* key);

        void load_trace_and_insert(const std::string& trace_file, gshm_upr * store);

        bool update(uint8_t* key, uint8_t* value);
        
        bool insert(uint8_t* key, uint8_t* value);

        int open_cxl_device();
       
        int read_cxl(size_t offset, size_t size, void *buf);
        int write_cxl(size_t offset, size_t size, const void *buf);
        int compare_cxl(size_t offset, size_t size, const void *buf);
        int close_cxl_device();





};
