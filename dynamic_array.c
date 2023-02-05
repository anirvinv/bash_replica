#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Job {
    char* command;
    volatile int pid;
    volatile int status;
    volatile bool fg;
} Job;

void print_jobs(Job* list, int size) {
    for (int i = 0; i < size; i++) {
        printf("{%s %d %d %d}\n", list[i].command, list[i].pid, list[i].status,
               list[i].fg);
    }
}

void edit_job(Job* list, int occupied, int target_pid, int status, bool fg) {
    for (int i = 0; i < occupied; i++) {
        if (list[i].pid == target_pid) {
            list[i].fg = fg;
            list[i].status = status;
            return;
        }
    }
}

Job* add_job(Job j, Job* list, int* occupied, int* total_size) {
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
Job* del_job(int pid, Job* list, int* occupied, int total_size) {
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
            }
        }
        (*occupied)--;
        free(list);
        return new_list;
    } else {
        return list;
    }
}

int main() {
    int occupied = 0;
    int total_size = 1;
    Job* list = (Job*)calloc(1, sizeof(Job));

    size_t size = 500;
    char* buffer = malloc(size * sizeof(char));
    getline(&buffer, &size, stdin);

    list = add_job((Job){buffer, 1, 1, 0}, list, &occupied, &total_size);
    // printf("occupied: %d\n", occupied);
    // printf("total_size: %d\n", total_size);
    // list = add_job((Job){2, 1, 0}, list, &occupied, &total_size);
    // list = add_job((Job){34, 1, 0}, list, &occupied, &total_size);
    // list = add_job((Job){6, 1, 0}, list, &occupied, &total_size);
    // list = add_job((Job){5, 1, 0}, list, &occupied, &total_size);
    // print_jobs(list, occupied);
    // list = del_job(34, list, &occupied, total_size);
    // list = del_job(1, list, &occupied, total_size);
    // list = del_job(2, list, &occupied, total_size);
    // list = del_job(5, list, &occupied, total_size);
    // list = del_job(6, list, &occupied, total_size);
    // list = add_job((Job){5, 1, 0}, list, &occupied, &total_size);
    // list = add_job((Job){1234, 1, 0}, list, &occupied, &total_size);
    printf("After ops:\n");
    print_jobs(list, occupied);

    // list[0] = (Job){1, 1, 0};

    // list = realloc(list, 2 * total_size * sizeof(Job));
    // total_size *= 2;

    // list[1] = (Job){2, 2, 0};
    // print_jobs(total_size, list);

    // list = del_job(1, list, &total_size);
    // printf("After deletion:\n");
    // print_jobs(total_size, list);
    free(list);
    free(buffer);
}