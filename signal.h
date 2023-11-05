#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int signal_interrupt = 0;
int running = 1;

static void handle_ignore(int signum) {
    printf("\nsignal interrupt.\nPress CTRL + C to exit.\n");
    signal_interrupt = 1;
}

static void handle_done(int signum) {
    // printf("\nExiting.\n");
    running = 0;
}

static int exit_signal(const int signum) {
    struct sigaction  act;

    memset(&act, 0, sizeof act);
    sigemptyset(&act.sa_mask);
    act.sa_handler = handle_done;
    act.sa_flags = 0;
    if (sigaction(signum, &act, NULL) == -1)
        return errno;
    return 0;
}

static int ignore_signal(int signum) {
    struct sigaction  act;

    memset(&act, 0, sizeof act);
    sigemptyset(&act.sa_mask);
    act.sa_handler = handle_ignore;
    act.sa_flags = 0;
    if (sigaction(signum, &act, NULL) == -1)
        return errno;
    return 0;
}

int is_interrupted() {
	return signal_interrupt;
}

int is_running() {
	return running;
}

void reset_interrupts() {
	signal_interrupt = 0;
}