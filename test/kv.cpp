#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <thread>
#include <unistd.h>
#include <sys/time.h>
#include <chrono>
#include <numeric> 
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <mutex>


#include "fred.h"
#include "generator.hpp"
#include "../include/gshmp.h"


int value_size_args = 256; // Default value size
int entry_size_args = 64 + 8 + value_size_args; // 64 + 256 + 8
int use_batch_args = 0;
size_t batch_kvs_args = 5; // Default batch size
int use_mac_args = 0; // Default MAC usage


int niterations = 10;
int nthreads = 1;
size_t length;
int shm_id;
int read_ratio = 9;
pthread_barrier_t barrier;

std::mutex log_file_mutex;
// HL::Timer t;



void record_shkey_mapping(const std::string& log_file, const uint8_t* key, shkey_t shkey) {

    std::lock_guard<std::mutex> lock(log_file_mutex); 
    std::ofstream ofs(log_file, std::ios::app);
    if (!ofs) {
        std::cerr << "Failed to open log file: " << log_file << std::endl;
        return;
    }

    std::string key_str(reinterpret_cast<const char*>(key), strnlen(reinterpret_cast<const char*>(key), 64));

    ofs << key_str << " " << std::hex << shkey << "\n";
    ofs.close();
}


shkey_t lookup_shkey_from_file(const std::string& log_file, const std::string& target_key_str) {
    std::ifstream ifs(log_file);
    if (!ifs) {
        std::cerr << "Failed to open log file: " << log_file << std::endl;
        return 0;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string key_str;
        std::string shkey_hex;

        if (!(iss >> key_str >> shkey_hex)) continue;

        if (key_str == target_key_str) {
            return std::stoull(shkey_hex, nullptr, 16); // 十六进制转为整数
        }
    }

    std::cerr << "Key not found in log.\n";
    return 0;
}


extern "C" void * worker (void * arg)
{
    RandomGen r(niterations*2);
    gshm *shm = new gshm();
    pthread_barrier_wait(&barrier);

    uint64_t id = shm->get_host_id();


    shm->value_size = value_size_args;
    shm->entry_size = entry_size_args;
    shm->use_batch = use_batch_args;
    shm->batch_kvs = batch_kvs_args;
    shm->use_mac = use_mac_args;
    
    int i = niterations;
    int len = 1 + read_ratio;
    uint8_t *key = new uint8_t[64];
    uint8_t *value_put = new uint8_t[value_size_args];
    uint8_t *value_get = new uint8_t[value_size_args];
    memset(key, 0, 64);
    memset(value_put, 0, value_size_args);
    memset(value_get, 0, value_size_args);

   

    int load_cycle = 0;

    shm->timereset();
    //std::string trace_file = "./workloada.spec_load";
    std::string trace_file = "../workload/workloada.spec_load";
    //std::string trace_file = "./test_trace_simple.txt";
    //std::string trace_file = "./test_trace_load.txt";
    std::ifstream infile(trace_file);
    
    //std::ifstream infile("test_trace.txt");

    if (!infile.is_open()) {
        std::cerr << "Failed to open trace file: " << trace_file << std::endl;
        return NULL;
    }

    std::string line;
    int inserted = 0;

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string op, table, key_str;

        if (!(iss >> op >> table >> key_str)) {
            std::cerr << "Skipping malformed line: " << line << std::endl;
            continue;
        }

        if (op != "INSERT") continue;

        uint8_t key[64] = {0};
        uint8_t value[value_size_args];

       
        std::strncpy(reinterpret_cast<char*>(key), key_str.c_str(), 64 - 1);

       
        std::memset(value, 'v', value_size_args);
        shkey_t inserted_key;

        shm->insert_p_start_stamp();
        
        inserted_key = shm->insert(key, value);
        inserted++;
        shm->insert_p_stop_stamp();

        record_shkey_mapping("key-token.log", key, inserted_key);



        load_cycle++;
        if (load_cycle == 500000) { 
            shm->clear_protection_table();
            load_cycle = 0;
        }
    }
    shm->print_insert();
    std::cout << "Total inserted keys: " << inserted << std::endl;

    infile.close();

    shm->clear_protection_table(); // 


    int trans_cycle = 0;
    shm->timereset();

    //std::string trans_trace_file = "./workloada.spec_trans";
    //std::string trans_trace_file = "./test_trans_trace.txt";
    //std::string trans_trace_file = "./test_trans_trace_simple.txt";
    std::string trans_trace_file = "../workload/workloada.spec_trans";
    std::ifstream transinfile(trans_trace_file);

    if (!transinfile.is_open()) {
        std::cerr << "Failed to open trace file: " << trans_trace_file << std::endl;
        return NULL;
    }

    int read_count = 0, update_count = 0, fail_count = 0, trans_insert_count = 0;

    while (std::getline(transinfile, line)) {
        std::istringstream iss(line);
        std::string op, table, key_str;

        if (!(iss >> op >> table >> key_str)) {
            std::cerr << "Skipping malformed line: " << line << std::endl;
            continue;
        }

        uint8_t key[64] = {0};
        uint8_t value[value_size_args];

        std::memset(key, 0, 64);
        std::strncpy(reinterpret_cast<char*>(key), key_str.c_str(), 64 - 1);

        shkey_t lookup_key;

        if (op != "INSERT") lookup_key = lookup_shkey_from_file("key-token.log", key_str);
        

        shm->trans_start_stamp();
        if (op == "READ") {
            std::memset(value, 0, value_size_args); //
          
            shm->trans_read_start_stamp();
            bool ok = shm->get(key, value, lookup_key);
            shm->trans_read_stop_stamp();
            
            read_count++;
        } else if (op == "UPDATE") {
          
            std::memset(value, 'U', value_size_args);  // 
          
            shm->trans_update_start_stamp();
            bool ok = shm->update(key, value, lookup_key);
            shm->trans_update_stop_stamp();
          
            update_count++;
        } else if (op == "INSERT") {
            std::memset(value, 'v', value_size_args);  // 填充为 v 表示 insert
            shkey_t inserted_key;

            shm->trans_insert_start_stamp();
           
            inserted_key = shm->insert(key, value);
            trans_insert_count++;
            shm->trans_insert_stop_stamp();

            record_shkey_mapping("key-token.log", key, inserted_key);
            
        
        } else {
            std::cerr << "[Unknown op] " << op << std::endl;
        }
        shm->trans_stop_stamp();

        trans_cycle++;
        if(trans_cycle == 500000){
            shm->clear_protection_table();
            trans_cycle = 0;
        }
        //printf("Processed %d operations: READ=%d, UPDATE=%d, trans_INSERT=%d, FAIL=%d\n", trans_cycle, read_count, update_count, trans_insert_count, fail_count);
    }

    shm->print_trans();

  


    std::cout << "Trace finished. READ=" << read_count
              << ", UPDATE=" << update_count
              << ", FAIL=" << fail_count 
              << ", TRANS_INSERT=" << trans_insert_count
              << std::endl;




    

    delete[] key;
    delete[] value_put;
    delete[] value_get;

    delete shm;


    return NULL;
}


extern "C" void * worker2 (void * arg)
{
    RandomGen r(niterations*2);
    gshm *shm = new gshm();
    pthread_barrier_wait(&barrier);

    shm->value_size = value_size_args;
    shm->entry_size = entry_size_args;
    shm->use_batch = use_batch_args;
    shm->batch_kvs = batch_kvs_args;
    shm->use_mac = use_mac_args;

    uint64_t id = shm->get_host_id();
    int i = niterations;
    int len = 1 + read_ratio;
    uint8_t *key = new uint8_t[64];
    uint8_t *value_put = new uint8_t[256];
    uint8_t *value_get = new uint8_t[256];
    memset(key, 0, 64);
    memset(value_put, 0, 256);
    memset(value_get, 0, 256);
 

    
    i = niterations;
 
    shm->timereset();
    while (i > 0) {
        reinterpret_cast<uint64_t*>(key)[0] = (id << 56) + i % 256;
        value_put[0] = i % 256;
        shm->insert_p_start_stamp();
        shkey_t shkey = shm->insert(key, value_put);
        shm->insert_p_stop_stamp();
        record_shkey_mapping("key-token.log", key, shkey);
        i--;
    }
    shm->print_insert();


    //shm->clear_protection_table(); 

    i = niterations;
 
    shm->timereset();
    while (i > 0) {
        reinterpret_cast<uint64_t*>(key)[0] = (id << 56) + i % 256;
        value_put[0] = i % 256;
        shkey_t shkey = lookup_shkey_from_file("key-token.log", std::string(reinterpret_cast<char*>(key)));
        shm->update_p_start_stamp();
        shm->update(key, value_put, shkey);
        shm->update_p_stop_stamp();
        i--;
    }
    shm->print_update();
  

   // shm->clear_protection_table(); 

    i = niterations;


    shm->timereset();
    while (i > 0) {
        reinterpret_cast<uint64_t*>(key)[0] = (id << 56) + i % 256;
        value_get[0] = i % 256;
        shkey_t shkey = lookup_shkey_from_file("key-token.log", std::string(reinterpret_cast<char*>(key)));
        shm->get_start_stamp();
        shm->get(key, value_get, shkey);
        shm->get_stop_stamp();

        i--;
    }
    shm->print_get();


    //shm->clear_protection_table(); 

    i = niterations;


    shm->timereset();
    while (i > 0) {
        reinterpret_cast<uint64_t*>(key)[0] = (id << 56) + i % 256;
        shkey_t shkey = lookup_shkey_from_file("key-token.log", std::string(reinterpret_cast<char*>(key)));
        shm->del_start_stamp();
        bool success = shm->del(key, shkey);
        shm->del_stop_stamp();
 
        i--;
    }
    shm->print_del();



    

    delete[] key;
    delete[] value_put;
    delete[] value_get;

    delete shm;


    return NULL;
}


extern "C" void * worker3 (void * arg)
{
    RandomGen r(niterations*2);
    gshm *shm = new gshm();
    pthread_barrier_wait(&barrier);

    uint64_t id = shm->get_host_id();
    int i = niterations;
    int len = 1 + read_ratio;
    uint8_t *key = new uint8_t[64];
    uint8_t *value_put = new uint8_t[256];
    uint8_t *value_get = new uint8_t[256];
    memset(key, 0, 64);
    memset(value_put, 0, 256);
    memset(value_get, 0, 256);
    i = 10;
    shm->timereset();
    shm->get_start_stamp();
    while(i--){
        shm->write_cxl(0, 64, key, 0);
    }
    shm->get_stop_stamp();
    shm->print_get();
    

    shm->timereset();
    shm->insert_p_start_stamp();
    i = 10;
    while(i--){
        shm->read_cxl(0, 64, key, 0);
    }
    shm->insert_p_stop_stamp();
    shm->print_insert();


    shm->timereset();
    shm->update_p_start_stamp();

    i = 10;
    while(i--){
        shm->write_cxl(0, 256, value_put, 0);
    }

    shm->update_p_stop_stamp();
    shm->print_update();
    
    shm->timereset();
    shm->del_start_stamp();
    i = 10;
    while(i--){
        shm->read_cxl(0, 256, value_get, 0);
    }
    shm->del_stop_stamp();
    shm->print_del();
 

    delete[] key;
    delete[] value_put;
    delete[] value_get;

    delete shm;


    return NULL;
}



int main(int argc, char* argv[])
{
    HL::Fred * threads;

    if (argc >= 2) {
        nthreads = atoi(argv[1]);
    }

    if (argc >= 3) {
        //read_ratio = atoi(argv[2]);
        value_size_args = atoi(argv[2]);
        entry_size_args = 64 + 8 + value_size_args; // 64 + 256 + 8
    }

    if (argc >= 4) {
        use_batch_args = atoi(argv[3]);
    }

    if (argc >= 5) {
        batch_kvs_args = atoi(argv[4]);
    }

    if (argc >= 6) {
        use_mac_args = atoi(argv[5]);
    }



    const int TEST_ROUNDS = 1;

 


    for (size_t k = 0; k < TEST_ROUNDS; k++)
    {
        pthread_barrier_init(&barrier,NULL,nthreads);
        threads = new HL::Fred[nthreads];

        int i;
        int *threadArg = (int*)malloc(nthreads*sizeof(int));


        for (i = 0; i < nthreads; i++) {
            threadArg[i] = i;
            threads[i].create (worker, &threadArg[i]);
        }

        for (i = 0; i < nthreads; i++) {
            threads[i].join();
        }

        delete[] threads;

    }



    return 0;
}

