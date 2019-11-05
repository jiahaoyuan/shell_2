#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "./signals.h"

void prep_signal(){
  sigset_t old;
  sigset_t full;
  sigfillset(&full);

  // Ignore signals while installing handlers
  sigprocmask(SIG_SETMASK, &full, &old);

  // Install signal handlers
  if (install_handler(SIGINT, &sigint_handler))
    perror("Warning: could not install handler for SIGINT");

  if (install_handler(SIGTSTP, &sigtstp_handler))
    perror("Warning: could not install handler for SIGTSTP");

  if (install_handler(SIGQUIT, &sigquit_handler))
    perror("Warning: could not install handler for SIGQUIT");

  if (install_handler(SIGTTOU, &sigttou_handler))
    perror("Warning: could not install handler for SIGQUIT");

  // Restore signal mask to previous value
  sigprocmask(SIG_SETMASK, &old, NULL);
}

int install_handler(int sig, void (*handler)(int)){
  struct sigaction act;
  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(sig, &act, 0) == 0){
    return 0;
  } else {
    return 1;
  }
}

void sigint_handler(int sig){
  printf("\nsigint_handler caught sig %d\n", sig);
}

void sigtstp_handler(int sig){
  printf("\nsigtstp_handler caught sig %d\n", sig);
}

void sigquit_handler(int sig){
  printf("\nsigquit_handler caught sig %d\n", sig);
}

void sigttou_handler(int sig){
  printf("\nsigquit_handler caught sig %d\n", sig);
}
