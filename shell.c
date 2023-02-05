#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dynamic_array.h"

pid_t shell_pid;
pid_t foreground_pid;
volatile int occupied = 0;
Job* job_list = NULL;
volatile int total_size = 1024;
volatile int static_job_id = 0;
volatile bool flag = 0;

int num_words_line(char* line, int length) {
    int num_words = 0;
    for (int i = 0; i < length;) {
        if (line[i] != ' ') {
            num_words++;
            while (i < length && line[i] != ' ') i++;
        } else {
            i++;
        }
    }
    return num_words;
}
char** line_to_program_args(char* line, int length) {
    int numStrings = num_words_line(line, length);
    if (numStrings == 0) {
        return NULL;
    }
    char** prog_args = malloc((numStrings + 1) * sizeof(char*));
    char delim = ' ';
    int prog_idx = 0;
    for (int i = 0; i < length;) {
        if (line[i] != delim) {
            int k = i;
            while (k < length && line[k] != delim) {
                k++;
            }
            int str_len = k - i;
            prog_args[prog_idx] = malloc((str_len + 1) * sizeof(char));
            for (int j = i; j < k; j++) {
                prog_args[prog_idx][j - i] = line[j];
            }
            prog_args[prog_idx][str_len] = '\0';

            prog_idx++;
            i = k;
        } else {
            i++;
        }
    }
    prog_args[numStrings] = NULL;
    return prog_args;
}

void free_prog_args(char** prog_args, int length) {
    for (int i = 0; i < length + 1; i++) {
        free(prog_args[i]);
    }
    free(prog_args);
}

pid_t Fork() {
    pid_t pid;
    if ((pid = fork()) < 0) {
        fprintf(stderr, "Error forking: %s\n", strerror(errno));
        exit(0);
    }
    return pid;
}

// access REPLACE THIS
void run_in_path(char* path, char** prog_args) {
    DIR* dir = opendir(path);
    struct dirent* entry;
    int name_length = strlen(prog_args[0]);
    char* buffer = malloc(sizeof(char) * (name_length + 20));
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, prog_args[0]) == 0) {
            sprintf(buffer, "%s/%s", path, prog_args[0]);
            execv(buffer, prog_args);
        }
    }
    free(buffer);
    closedir(dir);
}

void run_program(char* program_name, char** prog_args, bool bg) {
    // check if program_name is in /usr/bin or /bin in that order
    run_in_path("/usr/bin", prog_args);
    run_in_path("/bin", prog_args);
    execv(prog_args[0], prog_args);
    printf("%s: No such file or directory found\n", program_name);
    exit(0);
}

void sigchld_handler(int signal) {
    /*
     */

    // printf("Inside sigchld_handler: Calling process PID: %d\n", getpid());
    sigset_t mask_all, prev;
    sigfillset(&mask_all);
    // printf("In sigchld_handler\n");
    sigprocmask(SIG_BLOCK, &mask_all, &prev);

    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
        // we dont want the signal handler to hang here so we say
        // WNOHANG. We don't want it to hang because ctrl Z would suspend the
        // parent process if it hung
        if (pid == foreground_pid) {
            flag = 1;
        }
        // printf("new status: %d\n", status);
        // printf("signal: %d\n", signal);
        edit_job_status(job_list, occupied, pid, status);
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);
    // printf("Inside sigchld_handler: pid reaped? %d\n", status);
}

void clear_dead_jobs() {
    // job_list = del_job(job.pid, job_list, &occupied, total_size);
    sigset_t mask_all, prev;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev);
    for (int i = 0; i < occupied; i++) {
        Job job = job_list[i];
        if (job.status == -1) {
        } else if (WIFEXITED(job.status)) {
        } else if (WIFSIGNALED(job.status)) {
            printf("[%d] Terminated %s\n", job.job_id, job.command);
        }
    }
    int count = 1;
    while (count > 0) {
        count = 0;
        for (int i = 0; i < occupied; i++) {
            Job job = job_list[i];
            if (job.status != -1 &&
                (WIFEXITED(job.status) || WIFSIGNALED(job.status))) {
                job_list = del_job(job.pid, job_list, &occupied, total_size);
                count++;
                break;
            }
        }
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);
}

void dummy_handler() {}

void init_shell() {
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, dummy_handler);
    signal(SIGTSTP, dummy_handler);
    shell_pid = getpid();
    setpgid(shell_pid, shell_pid);
    tcsetpgrp(0, shell_pid);
}

void exit_shell() {
    sigset_t mask_all;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, NULL);
    for (int i = 0; i < occupied; i++) {
        if (WIFSTOPPED(job_list[i].status)) {
            kill(job_list[i].pid, SIGHUP);
            kill(job_list[i].pid, SIGCONT);
        }
    }

    free_job_list(job_list, occupied);
    exit(0);
}

char* get_string_line() {
    char* str = malloc(10);
    int str_size = 9;
    char c;
    printf("> ");
    int size = 1;
    while ((c = getchar()) != '\n') {
        while (str_size < size) {
            str = (char*)realloc(str, str_size * 2 + 1);
            str_size *= 2;
        }
        str[size - 1] = c;
        size++;
    }
    str[size - 1] = '\0';
    return str;
}

int get_job_id(char* arg) {
    int num = 0;
    int place = 1;
    int len = strlen(arg);
    for (int i = len - 1; i >= 1; i--) {
        num += place * (arg[i] - '0');
        place *= 10;
    }
    return num;
}
void print_jobs() {
    for (int i = 0; i < occupied; i++) {
        char* buffer = NULL;
        int status = job_list[i].status;
        if (status == -1 || WIFCONTINUED(status)) {
            buffer = "Running";
        } else if (WIFSTOPPED(status)) {
            buffer = "Stopped";
        } else if (WIFSIGNALED(status)) {
            buffer = "Signaled";
        } else if (WIFEXITED(status)) {
            buffer = "Terminated";
        }

        printf("[%d] %d %s %s\n", job_list[i].job_id, job_list[i].pid, buffer,
               job_list[i].command);
    }
}

void move_to_foreground(int job_id) {
    Job* job = find_job_by_id(job_list, occupied, job_id);
    pid_t child_pid = job->pid;

    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    sigset_t empty, mask_all, prev;
    sigfillset(&mask_all);
    sigemptyset(&empty);
    sigprocmask(SIG_BLOCK, &mask_all, &prev);

    // setpgid(child_pid, child_pid);
    if (WIFSTOPPED(job->status)) {
        // sends a continue signal, which makes the background process run and
        // then the waitpid reaps the child.
        kill(child_pid, SIGCONT);
    }
    tcsetpgrp(0, child_pid);

    foreground_pid = child_pid;
    flag = 0;
    // in a while loop cuz multiple background processes can return SIGCHLD, but
    // we only wait for the child with pid == foreground_pid
    while (!flag) {
        sigsuspend(&empty);
    }
    foreground_pid = getpid();
    tcsetpgrp(0, getpid());
    sigprocmask(SIG_SETMASK, &prev, NULL);
    signal(SIGINT, dummy_handler);
    signal(SIGTSTP, dummy_handler);
}

void exec(char* line, char** prog_args, int num_words, int str_len, bool bg) {
    pid_t child_pid = Fork();
    sigset_t mask_all, prev, empty;
    sigemptyset(&empty);
    sigfillset(&mask_all);
    sigprocmask(SIG_SETMASK, &mask_all, &prev);
    if (child_pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        setpgid(0, 0);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        run_program(prog_args[0], prog_args, bg);
    } else {
        char* command = malloc((str_len + 1) * sizeof(char));
        strcpy(command, line);
        job_list = add_job((Job){command, child_pid, -1, ++static_job_id,
                                 prog_args[num_words - 1][0] != '&'},
                           job_list, &occupied, &total_size);
        // printf("Parent: Job %d added\n", child_pid);

        if (!bg) {
            /*
            This call does hang for non stopped or non terminated.
            We want this because this is foreground.

            when ctrl Z is being called, child process is
            being reaped here I think.

            pid_t temp = waitpid(-1, &status, 0)
            -->this does NOT return when a child process is stopped
                aka ctrl+Z-ed

            because WUNTRACED returns for stopped children and
            everyone else, UNTRACED works
            */
            /*
                SIGCHLDs from foreground processes are handled here
               because this is where main goes?
            */
            // int status;
            // pid_t child_reaped = waitpid(-1, &status, WUNTRACED);
            // printf("Child reaped in main: %d\n", child_reaped);
            // sigset_t mask_all, old;
            // sigfillset(&mask_all);

            // sigprocmask(SIG_BLOCK, &mask_all, &old);
            // if (WIFSTOPPED(status)) {
            //     edit_job(job_list, occupied, child_reaped, status, 0);
            // } else if (WIFEXITED(status)) {
            //     job_list =
            //         del_job(child_reaped, job_list, &occupied, total_size);
            // } else if (WIFSIGNALED(status)) {
            //     job_list =
            //         del_job(child_reaped, job_list, &occupied, total_size);
            // }
            // sigprocmask(SIG_SETMASK, &old, NULL);
            signal(SIGTTIN, dummy_handler);
            signal(SIGTTOU, dummy_handler);
            setpgid(child_pid, child_pid);
            tcsetpgrp(0, child_pid);
            foreground_pid = child_pid;
            flag = false;
            while (!flag) {
                sigsuspend(&empty);
            }
            foreground_pid = 0;
            tcsetpgrp(0, shell_pid);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            sigprocmask(SIG_SETMASK, &prev, NULL);
        } else {
            printf("[%d] %d\n", static_job_id, child_pid);
            sigprocmask(SIG_SETMASK, &prev, NULL);
        }
    }
}

int main() {
    init_shell();

    job_list = (Job*)calloc(total_size, sizeof(Job));

    sigset_t mask_all_but_child, prev, empty;
    sigemptyset(&empty);

    sigfillset(&mask_all_but_child);          // filling in mask_all_but_child
    sigdelset(&mask_all_but_child, SIGCHLD);  // unblocking sigchld

    sigprocmask(SIG_SETMASK, &mask_all_but_child,
                &prev);  // blockign everything but the child

    while (1) {
        if (feof(stdin)) {
            // worry later
            exit_shell();
        }
        clear_dead_jobs();
        char* line = get_string_line();
        int str_len = strlen(line);
        if (str_len != 0) {
            int num_words = num_words_line(line, str_len);
            char** prog_args = line_to_program_args(line, str_len);
            bool bg = (prog_args[num_words - 1][0] == '&');
            if (bg) {
                free(prog_args[num_words - 1]);
                prog_args[num_words - 1] = NULL;
                num_words--;
            }
            // int status = 0;
            if (strcmp("exit", prog_args[0]) == 0) {
                exit_shell();
            } else if (strcmp("jobs", prog_args[0]) == 0) {
                print_jobs();
            } else if (strcmp("cd", prog_args[0]) == 0) {
                if (prog_args[1] == NULL) {
                    // set to home
                    char* HOME = getenv("HOME");
                    chdir(HOME);
                } else {
                    int old_errno = errno;
                    errno = chdir(prog_args[1]);
                    // printf("%s\n", prog_args[1]);
                    if (errno != 0) {
                        printf("Invalid path\n");
                    }
                    errno = old_errno;
                }
            } else if ((strcmp("fg", prog_args[0]) == 0) ||
                       strcmp("bg", prog_args[0]) == 0 ||
                       strcmp("kill", prog_args[0]) == 0) {
                if (num_words != 2 || prog_args[1][0] != '%') {
                    printf("Usage: %s <JobID>\n", prog_args[0]);
                } else {
                    int targ_job_id = get_job_id(prog_args[1]);
                    Job* targ_job =
                        find_job_by_id(job_list, occupied, targ_job_id);
                    if (targ_job == NULL) {
                        printf("No jobs with specified jobID\n");
                    } else {
                        if (strcmp("fg", prog_args[0]) == 0) {
                            // fg <JOBID>

                            // suspend ./shell program, give terminal control to
                            // job using tcsetpgrp(). Send SIGCONT to job when
                            // the process terminates, reset tcsetpgrp() to
                            // ./shell
                            move_to_foreground(targ_job_id);

                        } else if (strcmp("bg", prog_args[0]) == 0) {
                            // bg <JOBID>
                            // send SIGCONT to process
                            kill(targ_job->pid, SIGCONT);

                        } else if (strcmp("kill", prog_args[0]) == 0) {
                            sigset_t mask_all, previous, no_child;
                            sigfillset(&mask_all);
                            sigprocmask(SIG_BLOCK, &mask_all, &previous);

                            sigfillset(&no_child);
                            sigdelset(&no_child, SIGCHLD);
                            kill(targ_job->pid, SIGTERM);
                            kill(targ_job->pid, SIGCONT);

                            sigsuspend(&no_child);
                            clear_dead_jobs();

                            sigprocmask(SIG_SETMASK, &prev, NULL);
                        }
                    }
                }

            } else {
                char* buffer =
                    malloc(sizeof(char) * (strlen(prog_args[0]) + 20));
                sprintf(buffer, "%s/%s", "/usr/bin/", prog_args[0]);
                bool worked = false;
                if (access(buffer, X_OK) == 0) {
                    exec(line, prog_args, num_words, str_len, bg);
                    worked = 1;
                }
                sprintf(buffer, "%s/%s", "/bin/", prog_args[0]);
                if (!worked && access(buffer, X_OK) == 0) {
                    exec(line, prog_args, num_words, str_len, bg);
                    worked = 1;
                }
                if (!worked && access(prog_args[0], X_OK) == 0) {
                    exec(line, prog_args, num_words, str_len, bg);
                }
                free(buffer);
            }
            free_prog_args(prog_args, num_words);
        }
        free(line);
    }
    free_job_list(job_list, occupied);
}