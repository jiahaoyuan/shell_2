#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "./jobs.h"

#define MAX_ARGV 10
#define MAX_CMD 1024

void signal_reset();
void handle_new_background_process(pid_t pid);
void handle_foreground_process(int status, pid_t pid);
void monitor_foreground_process(pid_t curr_pid);
void handle_resumed_background_process(pid_t pid);

static int jid;
static job_list_t *job_list;
static char buf[MAX_CMD];

int valid_command() { return 1; }

int tokenize_cmd(char *cmd, int max, char *argv[]) {
    if (!valid_command()) {
        return -1;
    }
    int num_args = 0;
    if (cmd == NULL) return 0;

    argv[0] = strtok(cmd, " \t\n");
    while ((num_args < max) && (argv[num_args] != NULL)) {
        num_args += 1;
        argv[num_args] = strtok(NULL, " \t\n");
    }
    return num_args;
}

void redirect_trunc(char filename[]) {
    close(1);
    int file_descriptor;
    if ((file_descriptor = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
                                S_IRUSR | S_IWUSR)) == -1) {
        perror("open file failed");
        exit(1);
    }
}

void redirect_append(char filename[]) {
    close(1);
    int file_descriptor;
    if ((file_descriptor = open(filename, O_WRONLY | O_CREAT | O_APPEND,
                                S_IRUSR | S_IWUSR)) == -1) {
        perror("open file failed");
        exit(1);
    }
}

void redirect_input(char filename[]) {  // TODO redirect_input is not working
    close(0);
    int file_descriptor;
    if ((file_descriptor = open(filename, O_RDONLY, S_IRUSR | S_IWUSR)) == -1) {
        exit(1);
    }
}

void helper_move(char *argv[], int num_argv) {
    while (argv[num_argv + 2]) {
        argv[num_argv] = argv[num_argv + 2];
        num_argv += 1;
    }
    argv[num_argv] = NULL;
}

void execute_command(char *argv[]) {
    // TODO: OVERRIDE and APPEND is not working
    char filename[80];
    int num_argv = 0;
    // check redirect symbol
    while (num_argv < MAX_ARGV && argv[num_argv]) {
        if (!strcmp(argv[num_argv], ">")) {
            strcpy(filename, argv[num_argv + 1]);
            redirect_trunc(filename);
            helper_move(argv, num_argv);
            num_argv -= 1;
        } else if (!strcmp(argv[num_argv], ">>")) {
            strcpy(filename, argv[num_argv + 1]);
            redirect_append(filename);
            helper_move(argv, num_argv);
            num_argv -= 1;
        } else if (!strcmp(argv[num_argv], "<")) {
            strcpy(filename, argv[num_argv + 1]);
            redirect_input(filename);
            helper_move(argv, num_argv);
            num_argv -= 1;
        }
        num_argv += 1;
    }

    char *first_argv;
    first_argv = argv[0];
    char *ptr = strrchr(first_argv, '/');
    ptr += 1;
    argv[0] = ptr;
    execv(first_argv, argv);
}

void ready_command(char *cmd) {
    char *argv[MAX_ARGV];

    // TOKENIZE COMMANDS
    if (cmd == NULL) {
        return;
    }

    int num_argv = tokenize_cmd(cmd, MAX_ARGV, argv);
    if (num_argv == -1 || num_argv == 0){
      return;
    }
    // if (!argv[0]){
    //   return;
    // }

    if (!strcmp(argv[0], "cd")) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: syntax error");
            return;
        }
        chdir(argv[1]);
    } else if (!strcmp(argv[0], "ln")) {  // TODO link is not working
        link(argv[1], argv[2]);
    } else if (!strcmp(argv[0], "rm")) {
        unlink(argv[1]);
    } else if (!strcmp(argv[0], "exit")) {
        exit(0);
    } else if (!strcmp(argv[0], "jobs")) {
      // TODO: list all the jobs
        jobs(job_list);
    } else if (!strcmp(argv[0], "fg")) {
      // TODO: fg, resume the foreground job, must be two argv
      // resume by sending signal SIGCONT and bring it foreground
        char *temp = argv[1] + 1;
        int curr_jid = atoi(temp);
        int curr_pid;
        if ((curr_pid = get_job_pid(job_list, curr_jid)) == -1){
          perror("Failed to get job pid");
          return;
        }
        kill(curr_pid, SIGCONT);
        monitor_foreground_process(curr_pid);
    } else if (!strcmp(argv[0], "bg")) {
      // TODO: bg, reumse the background job, must be two argv
      // resume by sending signal SIGCONT
        char *temp = argv[1] + 1;
        int curr_jid = atoi(temp);
        int curr_pid;
        if ((curr_pid = get_job_pid(job_list, curr_jid)) == -1){
          perror("Failed to get job pid");
          return;
        }
        kill(curr_pid, SIGCONT);
        handle_resumed_background_process(curr_pid);
    } else {
        // TODO: RUN in background
        int background = 0;
        if (!(strcmp(argv[num_argv - 1], "&"))){
          background = 1;
          argv[num_argv - 1] = NULL;
        }
        // FORK and EXECUTE
        pid_t pid;
        if ((pid = fork()) == 0) {
            // signal handler back to default
            signal_reset();
            execute_command(argv);
            exit(0);
        }

        if (!background) {
          setpgid(pid, pid);
          monitor_foreground_process(pid);
        } else {
          // register backgound process
          setpgid(pid, pid);
          jid += 1;
          handle_new_background_process(pid);
        }
    }
}

void monitor_foreground_process(pid_t curr_pgid){ // pgid == pid now since setpgid(pid, pid)
  int status;
  remove_job_pid(job_list, curr_pgid);
  tcsetpgrp(0, curr_pgid);
  waitpid(curr_pgid, &status, WUNTRACED);
  tcsetpgrp(0, getpgid(getpid()));
  handle_foreground_process(status, curr_pgid);
}

void handle_foreground_process(int wstatus, pid_t curr_pid){
  if (WIFEXITED(wstatus)){ // terminated normally
    // int exit_status = 1;
    // if (WIFEXITED(wstatus)){
    //     exit_status = 0;
    // }
    // printf("[%d] (%d) terminated with exit status <%d>\n", curr_jid, curr_pid, exit_status);
  }
  if (WIFSIGNALED(wstatus)){ // terminated by a signal CTRL-C
    jid += 1;
    int term_sig = WTERMSIG(wstatus);
    printf("[%d] (%d) terminated by signal %d\n", jid, curr_pid, term_sig);
  }
  if (WIFSTOPPED(wstatus)) { // stopped by a signal
    jid += 1;
    int stop_sig = WSTOPSIG(wstatus);
    printf("[%d] (%d) suspended by signal %d\n", jid, curr_pid, stop_sig);
    process_state_t state = STOPPED;
    //add suspended foreground process to job list
    if (add_job(job_list, jid, curr_pid, state, buf) != 0){
      perror("Failed to add suspended foreground process to list");
    }
  }
}

void handle_resumed_background_process(pid_t pid){
  printf("[%d] (%d) resumed\n",jid, pid);
  process_state_t state;
  state = RUNNING;
  if (update_job_pid(job_list, pid, state) != 0){
    perror("Failed to update the list");
  };
}

void handle_new_background_process(pid_t pid){
  printf("[%d] (%d)\n",jid, pid);
  process_state_t state;
  state = RUNNING;
  if (add_job(job_list, jid, pid, state, buf) != 0){
    perror("Failed to add to the list");
  }
}

void signal_reset(){
  signal(SIGINT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
}

void signal_ignore(){
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

// reap all background processes
// detect their state, update the list, and clean terminated process
void reap(){
  int wstatus;
  pid_t curr_pid;
  while ((curr_pid = get_next_pid(job_list)) != -1){
    if (waitpid(curr_pid, &wstatus, WNOHANG|WUNTRACED) > 0){
      if (WIFEXITED(wstatus)){ // terminated normally
        int curr_jid = get_job_jid(job_list, curr_pid);
        int exit_status;
        if (WIFEXITED(wstatus)){
          exit_status = 0;
        }
        remove_job_pid(job_list, curr_pid);
        printf("[%d] (%d) terminated with exit status %d\n", curr_jid, curr_pid, exit_status);
      }
      if (WIFSIGNALED(wstatus)){ // terminated by a signal
        int curr_jid = get_job_jid(job_list, curr_pid);
        int term_sig = WTERMSIG(wstatus);
        remove_job_pid(job_list, curr_pid);
        printf("[%d] (%d) terminated by signal %d\n", curr_jid, curr_pid, term_sig);
      }
      if (WIFSTOPPED(wstatus)) { // stopped by a signal, update its status
        int curr_jid = get_job_jid(job_list, curr_pid);
        int stop_sig = WSTOPSIG(wstatus);
        printf("[%d] (%d) suspended by signal %d\n", curr_jid, curr_pid, stop_sig);
        process_state_t state;
        state = STOPPED;
        update_job_jid(job_list, curr_jid, state);
      }
    }
  }
}

int main() {
    // signal handlers are installed
    signal_ignore();
    //pid_t pid = getpid();
    //pid_t pgid = getpgid(pid);
    //printf("The Shell's PID is %d, PGID is %d\n", pid, pgid);

    // init jobs list
    jid = 0;
    job_list = init_job_list();

    while (1) {
        reap();
        buf[0] = '\0';
// PROMPT
#ifdef PROMPT
        if (printf("33sh> ") < 0) {
            printf("Error: prompt is not correct");
        }
        fflush(stdout);
#endif

        // READ INPUT
        ssize_t count = read(STDIN_FILENO, buf, MAX_CMD);
        buf[count] = '\0';

        // PARSE INPUT
        if (count == 0) {
          exit(0);
        }
        if (count != 1) {
            ready_command(buf);
        }
    }
}
