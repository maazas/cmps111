// Author: Adam Henry, adlhenry@ucsc.edu

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **getLine();

int exit_status = EXIT_SUCCESS;
char *execname = NULL;

char *infile = NULL;
char *outfile = NULL;
char **argvs[10];
int *pipes[9];
int cmdi = 0;

// Print a warning to stderr
void errprintf (char *string) {
	fflush(NULL);
	fprintf(stderr, "%s: ", execname);
	fprintf(stderr, "%s\n", string);
	fflush(NULL);
	exit_status = EXIT_FAILURE;
}

// Print the shell prompt
void print_prompt () {
	printf("$> ");
}

// Parse the command line
void read_command () {
	outfile = NULL;
	infile = NULL;
	cmdi = 0;
	char **args = getLine();
	argvs[cmdi] = args;
	for (int i = 0; args[i] != NULL; i++) {
		switch (args[i][0]) {
			case '>':
				args[i] = NULL;
				outfile = args[++i];
				break;
			case '<':
				args[i] = NULL;
				infile = args[++i];
				break;
			case '|':
				args[i] = NULL;
				argvs[++cmdi] = &args[++i];
				break;
		}
	}
}

// Check for exit command
bool is_exit (char **args) {
	if (strcmp(args[0], "exit") == 0) {
		if (args[1] != NULL) {
			errprintf("exit: no options supported");
			return true;
		}
		exit(exit_status);
	}
	return false;
}

// Redirect stdin and stdout
void redirect () {
	if (infile != NULL) {
		close(STDIN_FILENO);
		int fd = open(infile, O_RDONLY);
		if (fd == -1) perror(infile);
	}
	if (outfile != NULL) {
		close(STDOUT_FILENO);
		int fd = creat(outfile, 00644);
		if (fd == -1) perror(outfile);
	}
}

// Execute a command with possible file redirection
void exec_c (char *command, char **parameters) {
	int status;
	pid_t pid = fork();
	if (pid == -1) {
		perror(execname);
	} else if (pid != 0) {
		int ret = wait(&status);
		if (ret == -1) perror(execname);
	} else {
		redirect();
		int ret = execvp(command, parameters);
		if (ret == -1) {
			perror(command);
			exit(exit_status);
		}
	}
}

// Connect a pipe to stdin (0) or stdout (1)
void connect_pipe (int *pipefd, int mode) {
	close(pipefd[!mode]);
	close(mode);
	dup(pipefd[mode]);
}

// Open all necessary pipes
void open_pipes () {
	for (int i = 0; i < cmdi; i++) {
		pipes[i] = malloc(2*sizeof(int));
		if (pipe(pipes[i]) == -1) {
			perror(execname);
		}
	}
}

// Close all pipes except pipe1 and pipe2
void close_pipes (int pipe1, int pipe2) {
	for (int i = 0; i < cmdi; i++) {
		if (i != pipe1 && i != pipe2) {
			close(pipes[i][0]);
			close(pipes[i][1]);
			free(pipes[i]);
		}
	}
}

// Execute a piped command
void exec_pipe (char *command, char **parameters, int i) {
	pid_t pid = fork();
	if (pid == -1) {
		perror(execname);
	} else if (pid == 0) {
		if (i == 0) {
			close_pipes(0, 0);
			connect_pipe(pipes[0], 1);
		} else if (i == cmdi) {
			close_pipes(i-1,i-1);
			connect_pipe(pipes[i-1], 0);
		} else {
			close_pipes(i-1,i);
			connect_pipe(pipes[i-1], 0);
			connect_pipe(pipes[i], 1);
		}
		if (execvp(command, parameters) == -1) {
			perror(command);
			exit(exit_status);
		}
	}
}

// Execute the command line
void execute_command () {
	if (cmdi == 0) {
		exec_c(argvs[0][0], argvs[0]);
	} else {
		open_pipes();
		for (int i = 0; i <= cmdi; i++) {
			exec_pipe(argvs[i][0], argvs[i], i);
		}
		close_pipes(-1, -1);
		while (true) {
			if (wait(NULL) == -1) break;
		}
	}
}

// Main loop
int main (int argc, char *argv[]) {
	execname = basename(argv[0]);
	if (argc > 1) {
		errprintf("usage: no options supported");
		return exit_status;
	}
	while (true) {
		print_prompt();		
		read_command();		
		if (is_exit(argvs[0])) continue;
		execute_command();
	}
	return exit_status;
}
