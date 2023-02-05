#ifndef LINKED_LIST_HEADER
#define LINKED_LIST_HEADER

#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Job {
    char* command;
    volatile int pid;
    volatile int status;
    volatile int job_id;
    volatile bool fg;
} Job;

Job* add_job(Job j, Job* list, volatile int* occupied,
             volatile int* total_size) {
    if (*occupied < *total_size) {
        list[*occupied] = j;
    } else {
        *total_size *= 2;
        list = realloc(list, *total_size * sizeof(Job));
        list[*occupied] = j;
    }
    (*occupied)++;
    return list;
}
Job* del_job(int pid, Job* list, volatile int* occupied, int total_size) {
    bool found = false;
    for (int i = 0; i < *occupied; i++) {
        if (list[i].pid == pid) {
            found = true;
            break;
        }
    }
    if (found) {
        Job* new_list = calloc(total_size, sizeof(Job));
        int ni = 0;
        for (int i = 0; i < *occupied; i++) {
            if (list[i].pid != pid) {
                new_list[ni++] = list[i];
            } else {
                free(list[i].command);
            }
        }
        (*occupied)--;
        free(list);
        return new_list;
    } else {
        return list;
    }
}
void edit_job_status(Job* list, int occupied, int target_pid, int status) {
    for (int i = 0; i < occupied; i++) {
        if (list[i].pid == target_pid) {
            list[i].status = status;
            return;
        }
    }
}
Job* find_job_by_id(Job* list, int occupied, int job_id) {
    for (int i = 0; i < occupied; i++) {
        if (list[i].job_id == job_id) return &list[i];
    }
    return NULL;
}
void free_job_list(Job* list, int occupied) {
    for (int i = 0; i < occupied; i++) {
        free(list[i].command);
    }
    free(list);
}

#endif