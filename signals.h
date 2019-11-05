#include <sys/types.h>
#include <unistd.h>

void prep_signal();
int install_handler(int sig, void (*handler)(int));
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigquit_handler(int sig);
void sigttou_handler(int sig);
