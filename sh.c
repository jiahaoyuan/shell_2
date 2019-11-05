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
void handle_background_process(pid_t pid);
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
      // TODO: fg
    } else if (!strcmp(argv[0], "bg")) {
      // TODO: bg
    } else {
        // TODO: RUN in background
        int background = 0;
        if (!(strcmp(argv[num_argv - 1], "&"))){
          background = 1;
          argv[num_argv - 1] = NULL;
        }
        // FORK and EXECUTE
        pid_t pid;
        int status;

        if ((pid = fork()) == 0) {
            // signal handler back to default
            signal_reset();
            execute_command(argv);
            exit(0);
        }


        if (!background) {
          setpgid(pid, pid);
          tcsetpgrp(0, pid);
          waitpid(pid, &status, 0);
          tcsetpgrp(0, getpgid(getpid()));
          if (WIFEXITED(status)){
          } else {
          }
        } else {
          // register backgound process
          handle_background_process(pid);
        }
    }
}

void handle_background_process(pid_t pid){
  jid += 1;
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

int main() {
    // signal handlers are installed
    signal_ignore();
    pid_t pid = getpid();
    pid_t pgid = getpgid(pid);
    printf("The Shell's PID is %d, PGID is %d\n", pid, pgid);

    // init jobs list
    jid = 0;
    job_list = init_job_list();

    while (1) {
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
        if (count != 1) {
            ready_command(buf);
        }
    }
}
