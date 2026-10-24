// Raises SIGUSR1 with a handler installed. Under the debugger the program first stops with
// SIGUSR1; on "continue" the debugger must deliver the signal so the handler still runs.
#include <signal.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t handled = 0;

static void on_usr1(int signo) {
    (void)signo;
    handled = 1;
}

int main(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_usr1;
    sigaction(SIGUSR1, &action, NULL);
    raise(SIGUSR1);
    printf("handler ran: %s\n", handled ? "yes" : "no");
    return handled ? 0 : 1;
}
