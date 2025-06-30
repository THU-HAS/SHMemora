#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <x86intrin.h>

time_t getNsTimestamp();
time_t getCycleCount();

struct TimingStats {
    time_t timestmp = 0;
    time_t get_timestmp = 0;
    time_t put_timestmp = 0;
    time_t del_timestmp = 0;
    time_t insert_p_timestmp = 0;
    time_t update_p_timestmp = 0;
    time_t trans_timestmp = 0;
    time_t hash_time = 0;
    time_t collision_time = 0;
    time_t permission_check_time = 0;
    time_t data_access_time = 0;
    time_t insert_time = 0;
    time_t put_time = 0;
    time_t get_time = 0;
    time_t del_time = 0;
    time_t insert_p_time = 0;
    time_t update_p_time = 0;
    time_t trans_time = 0;

    time_t YCSB_read_time = 0;
    time_t YCSB_update_time = 0;
    time_t YCSB_insert_time = 0;

    time_t YCSB_read_timestmp = 0;
    time_t YCSB_update_timestmp = 0;
    time_t YCSB_insert_timestmp = 0;

    time_t YCSB_read_hashtimestmp = 0;
    time_t YCSB_read_collisiontimestmp = 0;
    time_t YCSB_read_permission_checktimestmp = 0;

    time_t YCSB_update_hashtimestmp = 0;
    time_t YCSB_update_collisiontimestmp = 0;
    time_t YCSB_update_permission_checktimestmp = 0;

    time_t YCSB_insert_hashtimestmp = 0;
    time_t YCSB_insert_collisiontimestmp = 0;
    time_t YCSB_insert_permission_checktimestmp = 0;

    time_t YCSB_read_hash_time = 0;
    time_t YCSB_read_collision_time = 0;
    time_t YCSB_read_permission_check_time = 0;


    time_t YCSB_update_hash_time = 0;
    time_t YCSB_update_collision_time = 0;
    time_t YCSB_update_permission_check_time = 0;


    time_t YCSB_insert_hash_time = 0;
    time_t YCSB_insert_collision_time = 0;
    time_t YCSB_insert_permission_check_time = 0;






    int mode = 0; // 0 for cycle count, 1 for ns timestamp

    time_t getCurrentTime() {
        if (mode == 1) {
            return getNsTimestamp();
        } else {
            return getCycleCount();
        }
    }

    void start_stamp() {
        timestmp = getCurrentTime();
    }

    void get_start_stamp() {
        get_timestmp = getCurrentTime();
    }
    void put_start_stamp() {
        put_timestmp = getCurrentTime();
    }

    void insert_p_start_stamp() {
        insert_p_timestmp = getCurrentTime();
    }
    void update_pstart_stamp() {
        update_p_timestmp = getCurrentTime();
    }

    void trans_start_stamp() {
        trans_timestmp = getCurrentTime();
    }

    void del_start_stamp() {
        del_timestmp = getCurrentTime();
    }


    void YCSB_read_start_stamp() {
        YCSB_read_timestmp = getCurrentTime();

        YCSB_read_hashtimestmp = hash_time;
        YCSB_read_collisiontimestmp = collision_time;
        YCSB_read_permission_checktimestmp = permission_check_time;
        // YCSB_read_permission_checktimestmp = getCurrentTime();
    }
    void YCSB_update_start_stamp() {
        YCSB_update_timestmp = getCurrentTime();
        YCSB_update_hashtimestmp = hash_time;
        YCSB_update_collisiontimestmp = collision_time;
        YCSB_update_permission_checktimestmp = permission_check_time;
  
    }

    void YCSB_insert_start_stamp() {
        YCSB_insert_timestmp = getCurrentTime();
        YCSB_insert_hashtimestmp = hash_time;
        YCSB_insert_collisiontimestmp = collision_time;
        YCSB_insert_permission_checktimestmp = permission_check_time;
  
    }

    void YCSB_read_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        YCSB_read_time += tmp_stamp - YCSB_read_timestmp;
        YCSB_read_timestmp = tmp_stamp;

        YCSB_read_hash_time += hash_time - YCSB_read_hashtimestmp;
        YCSB_read_collision_time += collision_time - YCSB_read_collisiontimestmp;
        YCSB_read_permission_check_time += permission_check_time - YCSB_read_permission_checktimestmp;
    }
    void YCSB_update_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        YCSB_update_time += tmp_stamp - YCSB_update_timestmp;
        YCSB_update_timestmp = tmp_stamp;

        YCSB_update_hash_time += hash_time - YCSB_update_hashtimestmp;
        YCSB_update_collision_time += collision_time - YCSB_update_collisiontimestmp;
        YCSB_update_permission_check_time += permission_check_time - YCSB_update_permission_checktimestmp;
    }

    void YCSB_insert_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        YCSB_insert_time += tmp_stamp - YCSB_insert_timestmp;
        YCSB_insert_timestmp = tmp_stamp;

        YCSB_insert_hash_time += hash_time - YCSB_insert_hashtimestmp;
        YCSB_insert_collision_time += collision_time - YCSB_insert_collisiontimestmp;
        YCSB_insert_permission_check_time += permission_check_time - YCSB_insert_permission_checktimestmp;
    }

    void hash_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        hash_time += tmp_stamp - timestmp;
        timestmp = tmp_stamp;
    }
    void collision_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        collision_time += tmp_stamp - timestmp;
        timestmp = tmp_stamp;
    }
    void permission_check_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        permission_check_time += tmp_stamp - timestmp;
        timestmp = tmp_stamp;
    }
    void data_access_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        data_access_time += tmp_stamp - timestmp;
        timestmp = tmp_stamp;
    }
    void insert_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        insert_time += tmp_stamp - timestmp;
        timestmp = tmp_stamp;
    }
    void put_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        put_time += tmp_stamp - put_timestmp;
        put_timestmp = tmp_stamp;
    }

    void insert_p_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        insert_p_time += tmp_stamp - insert_p_timestmp;
        insert_p_timestmp = tmp_stamp;
    }
    void update_p_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        update_p_time += tmp_stamp - update_p_timestmp;
        update_p_timestmp = tmp_stamp;
    }

    void get_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        get_time += tmp_stamp - get_timestmp;
        get_timestmp = tmp_stamp;
    }

    void trans_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        trans_time += tmp_stamp - trans_timestmp;
        trans_timestmp = tmp_stamp;
    }

    void del_stop_stamp() {
        time_t tmp_stamp = getCurrentTime();
        del_time += tmp_stamp - del_timestmp;
        del_timestmp = tmp_stamp;
    }
    void print_stats() {
        printf("Hash time: %ld\n", hash_time);
        printf("Collision time: %ld\n", collision_time);
        printf("Permission check time: %ld\n", permission_check_time);
        printf("Data access time: %ld\n", data_access_time);
    }
    void print_insert_stats() {
        printf("Insert time: %ld\n", insert_time);
    }
    void print_put_stats() {
        printf("Put time: %ld\n", put_time);
    }
    void print_get_stats() {
        printf("Get time: %ld\n", get_time);
    }
    void print_del_stats() {
        printf("Del time: %ld\n", del_time);
    }
    void print_stats_proportion() {
        time_t total_time = hash_time + collision_time + permission_check_time + data_access_time;
        printf("Hash time: %ld%%\n", (hash_time * 100) / total_time);
        printf("Collision time: %ld%%\n", (collision_time * 100) / total_time);
        printf("Permission check time: %ld%%\n", (permission_check_time * 100) / total_time);
        printf("Data access time: %ld%%\n", (data_access_time * 100) / total_time);
    }
    void print_put_proportion(){
        //print_put_stats();
        //print_insert_stats();
        //print_stats();
        printf("Put time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Put_proportion: %lf%%\n", put_time, hash_time, collision_time, permission_check_time, ((hash_time + collision_time + permission_check_time) * 100.) / (put_time));
    }
    void print_insert_proportion(){
        //print_insert_stats();
        //print_stats();
        printf("Insert_p_time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Insert_proportion: %lf%%\n", insert_p_time, hash_time, collision_time, permission_check_time, ((hash_time + collision_time + permission_check_time) * 100.) / (insert_p_time));
    }
    void print_update_proportion(){
        //print_insert_stats();
        //print_stats();
        printf("Update_p_time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Update_proportion: %lf%%\n", update_p_time, hash_time, collision_time, permission_check_time, ((hash_time + collision_time + permission_check_time) * 100.) / (update_p_time));
    }

    void print_get_proportion(){
        //print_get_stats();
        //print_insert_stats();
        //print_stats();
        printf("Get time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Get_proportion: %lf%%\n", get_time, hash_time, collision_time, permission_check_time, ((hash_time + collision_time + permission_check_time) * 100.) / (get_time));
    }

    void print_trans_proportion(){
        //print_get_stats();
        //print_insert_stats();
        //print_stats();
        printf("Trans time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Trans_proportion: %lf%%\n", trans_time, hash_time, collision_time, permission_check_time, ((hash_time + collision_time + permission_check_time) * 100.) / (trans_time));
        printf("Trans_Read time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Trans_Read_proportion: %lf%%\n", YCSB_read_time, YCSB_read_hash_time, YCSB_read_collision_time, YCSB_read_permission_check_time, ((YCSB_read_hash_time + YCSB_read_collision_time + YCSB_read_permission_check_time) * 100.) / (YCSB_read_time));
        printf("Trans_Update time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Trans_Update_proportion: %lf%%\n", YCSB_update_time, YCSB_update_hash_time, YCSB_update_collision_time, YCSB_update_permission_check_time, ((YCSB_update_hash_time + YCSB_update_collision_time + YCSB_update_permission_check_time) * 100.) / (YCSB_update_time));
        printf("Trans_Insert time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Trans_Insert_proportion: %lf%%\n", YCSB_insert_time, YCSB_insert_hash_time, YCSB_insert_collision_time, YCSB_insert_permission_check_time, ((YCSB_insert_hash_time + YCSB_insert_collision_time + YCSB_insert_permission_check_time) * 100.) / (YCSB_insert_time));
    }
    void print_del_proportion(){
        //print_del_stats();
        //print_insert_stats();
        //print_stats();
        printf("Del time: %ld, Hash time: %ld, Collision time: %ld, Permission check time: %ld, Del_proportion: %lf%%\n", del_time, hash_time, collision_time, permission_check_time, ((hash_time + collision_time + permission_check_time) * 100.) / (del_time ));
    }
    void reset() {
        hash_time = 0;
        collision_time = 0;
        permission_check_time = 0;
        data_access_time = 0;
        timestmp = 0;
        insert_time = 0;
        put_time = 0;
        get_time = 0;
        del_time = 0;
        get_timestmp = 0;
        put_timestmp = 0;
        del_timestmp = 0;
        insert_p_timestmp = 0;
        update_p_timestmp = 0;
        trans_timestmp = 0;
        insert_p_time = 0;
        update_p_time = 0;
        trans_time = 0;
        YCSB_read_time = 0;
        YCSB_update_time = 0;
        YCSB_read_timestmp = 0;
        YCSB_update_timestmp = 0;
        YCSB_read_hashtimestmp = 0;
        YCSB_read_collisiontimestmp = 0;
        YCSB_read_permission_checktimestmp = 0;
        YCSB_update_hashtimestmp = 0;
        YCSB_update_collisiontimestmp = 0;
        YCSB_update_permission_checktimestmp = 0;
        YCSB_read_hash_time = 0;
        YCSB_read_collision_time = 0;
        YCSB_read_permission_check_time = 0;
        YCSB_update_hash_time = 0;
        YCSB_update_collision_time = 0;
        YCSB_update_permission_check_time = 0;
        YCSB_insert_time = 0;
        YCSB_insert_timestmp = 0;
        YCSB_insert_hashtimestmp = 0;
        YCSB_insert_collisiontimestmp = 0;
        YCSB_insert_permission_checktimestmp = 0;
        YCSB_insert_hash_time = 0;
        YCSB_insert_collision_time = 0;
        YCSB_insert_permission_check_time = 0;



    }

};

