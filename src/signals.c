#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "include/signals.h"
#include "include/tasks.h"

// Para escribir el STOP_MESSAGE en el pipe
extern int pipe_fds[2];


void
signals_handler(int signal) {
    fprintf(stderr, "========== Signals Handler ==========\n");
    fprintf(stderr, "Signal Recibida: %s\n", strsignal(signal));
    write(pipe_fds[1], STOP_MESSAGE, sizeof(STOP_MESSAGE));
    fprintf(stderr, "========== End Signal Handler ==========\n");
}


int
signals_initialize() {
	/*
	 * http://www.gnu.org/s/libc/manual/html_node/index.html#toc_Signal-Handling
	 *
	 * SIGTERM: Handler principal para la terminacion en modo DAEMON
	 * SIGINT: Handler para la terminacion en modo CONSOLA
	 */

	struct sigaction action;

	// SA_RESTART asi sale de select() y se ejecuta el cleanup
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESTART; // SA_RESTART == 0

	// Manejador de CTRL+C
	if(sigaction(SIGINT, &action, NULL) == -1)
	{
		perror("signals_initialize():");
		return -1;
	}

	// Manejador de kill
	action.sa_handler = &signals_handler;
	if(sigaction(SIGTERM, &action, NULL) == -1)
	{
		perror("signals_initialize():");
		return -1;
	}

	// Ignoramos SIGHUP y SYSQUIT
	action.sa_handler = SIG_IGN;
	if(sigaction(SIGHUP, &action, NULL) == -1)
	{
		perror("signals_initialize():");
		return -1;
	}

	if(sigaction(SIGQUIT, &action, NULL) == -1)
	{
		perror("signals_initialize():");
		return -1;
	}

	return 0;
}
