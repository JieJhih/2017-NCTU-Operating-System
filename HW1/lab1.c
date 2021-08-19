#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>
char **seperateCommand(char* command);
int launchProcess(char **command);

int position = 0;

void child_handle(int signo) {
	pid_t child_id;
	int status;
	do {
		child_id = waitpid(-1, &status, WNOHANG);
	} while(child_id == 0);
	// printf("Parent knows child %d is completed\n", (int) child_id);
}

int main(int argc, char *argv[]) {
	struct sigaction act;
	act.sa_handler = child_handle;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &act, NULL);
	printf("process id: %d\n", (int) getpid());
        char *command = NULL;
		size_t len = 0;
		ssize_t read;
		int code;
		do {
			printf("> ");
			if((read = getline(&command, &len, stdin)) == -1) {
				return 0;
			}
			char **sep = seperateCommand(command);
			code = launchProcess(sep);	

		} while(code);
		

		free(command);
	
	return 0;
}

int launchProcess(char **command) {
	if(command[0] == NULL) {
		return 1;
	}
	if(strcmp(command[0], "exit") == 0) {
		return 0;
	}
	
	char *fileName, **secondCommand;
	int status,oldfd, pfd[2];
	// 1: &, 2: >, 3: |
	int op;
	
	if(position > 0 && strcmp(command[position-1],"&") == 0) {
		command[position-1] = '\0';
		op = 1;
	} else if (position >= 2 && strcmp(command[position-2], ">") == 0) {
		fileName = command[position-1];
		command[position-2] = '\0';
		op = 2;
	} else if(position >= 2 && strcmp(command[position-2], "|") == 0) {
		pipe(pfd);
		command[position-2] = '\0';
		secondCommand = command+(position-1);
		op = 3;
	}

	pid_t child = fork();
	pid_t child_id;

	if(child == 0) {
		if(op == 2) {
			oldfd = open(fileName,(O_RDWR|O_CREAT),0644);
			dup2(oldfd,1);
			close(oldfd);
		} else if(op == 3) {
			// do the pipe operation for first command
			close(pfd[0]);
			dup2(pfd[1], STDOUT_FILENO);
			close(pfd[1]);
		}
		
		if(execvp(command[0], command) == -1) {
			printf("command execute error");
		}
		exit(EXIT_FAILURE);
	} else if(child < 0) {
		perror("os");
	} else {
		// Parent Process
		if(op == 3) {
			pid_t child2 = fork();
			// in child process
			if(child2 == 0) {
				close(pfd[1]);
				dup2(pfd[0], STDIN_FILENO);
				close(pfd[0]);
				if(execvp(secondCommand[0], secondCommand)==-1) {
					printf("second command failed\n");
				}
				exit(EXIT_FAILURE);
			} else if(child2 > 0) {
				close(pfd[0]);
				close(pfd[1]);
				waitpid(child2, &status, 0);
				waitpid(child, &status, 0);
			}
		} else {
			if(op != 1) {
				wait(NULL);				
			}
		}
	}
	return 1;
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **seperateCommand(char* line) {
	position = 0;
	int bufsize = LSH_TOK_BUFSIZE;
	char **tokens = malloc(bufsize*sizeof(char*));
	char *token, **tokens_backup;
	if(!tokens) {
		printf("memory allocation error");
		exit(EXIT_FAILURE);
	}
	
	token = strtok(line, LSH_TOK_DELIM);
	while(token != NULL) {
		tokens[position] = token;
        position++;

		if(position >= bufsize) {
			bufsize += LSH_TOK_BUFSIZE;
			tokens_backup = tokens;
			tokens = realloc(tokens, bufsize*sizeof(char*));
			if(!tokens) {
				free(tokens_backup);
				printf("memory allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		token = strtok(NULL, LSH_TOK_DELIM);
	}
	tokens[position] = NULL;
	return tokens;
}
