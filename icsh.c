#include <stdio.h> 
#include <string.h> 
#include <stdlib.h>

#define TOK_DELIM " \t\r\n"
#define RED "\033[0;31m"
#define RESET "\e[0m"

void add_to_history(char **);
char *read_line();
char **split_line(char *);
int exit_shell(char **);
int execute(char **);

/*--------------------*
 * HISTORY MANAGEMENT *
 *--------------------*/

void add_to_history(char **args) {
	// Adds user input to history file

	FILE *history = fopen(".icsh_history", "a+");
	int i = 0;
	
	while (args[i] != NULL) {
		if (i > 0) {
			// Add spaces between command args
			fputs(" ", history);
		}
		// Add command arg to history file
		fputs(args[i], history);
		i++;
	}
	fputs("\n", history); // Add new line to history file after each command
	fclose(history);
}

/*-------------------*
 * BUILT IN COMMANDS *
 *-------------------*/

int exit_shell(char **args) {
	// Exit command
	printf("Goodbye! :( \n");
	return 0;
}

/*-----------------*
 * COMMAND PARSING *
 *-----------------*/

char * read_line() {

	// Function to read user commands that are entered into the shell
	
	int buffsize = 1024;
	int position = 0;
	char * pointer = malloc(sizeof(char) * buffsize); // Char pointer
	int c;
	
	if (!pointer) {
		// Check if allocation was done properly
		fprintf(stderr, "%sAllocation error%s\n", RED, RESET);
		exit(EXIT_FAILURE);
	}
	
	while (1) {
		// Loop through entire command and get every char
		c = getchar();
		if (c == EOF || c == '\n') {
			// If EOF or \n then end
			pointer[position] = '\0';
			return pointer;
		} else {
			// Else store the char in pointer
			pointer[position] = c;
		}
		position++;
		
		if (position >= buffsize) {
			// If size of pointer is bigger than the initial size then realloc
			buffsize += 1024;
			pointer = realloc(pointer, buffsize);
			
			if (!pointer) {
				// Check if allocation was done properly
				fprintf(stderr, "%sAllocation error%s\n", RED, RESET);
				exit(EXIT_FAILURE);
			}
		}
	}
}

char * * split_line(char * line) {

	// Function for splitting the line using strtok() to get command and args
	
	int buffsize = 1024, position = 0;
	char * * tokens = malloc(buffsize * sizeof(char *));
	char * token;
	
	if (!tokens) {
		// Allocation check
		fprintf(stderr, "%sAllocation error%s\n", RED, RESET);
		exit(EXIT_FAILURE);
	}
	token = strtok(line, TOK_DELIM);
	while (token != NULL) {
		tokens[position] = token;
		position++;
		
		if (position >= buffsize) {
			buffsize += 1024;
			tokens = realloc(tokens, buffsize * sizeof(char * ));
			
			if (!tokens) {
				// Allocation check
				fprintf(stderr, "%sAllocation error%s\n", RED, RESET);
				exit(EXIT_FAILURE);
			}
		}
		
		token = strtok(NULL, TOK_DELIM);
	}
	
	tokens[position] = NULL;
	
	return tokens;
	
}

/*-----------------*
 * SHELL EXECUTION *
 *-----------------*/

int execute(char * * args) {

	// Execute command using execvp and fork
	
	pid_t cpid;
	int status;

	if (strcmp(args, "\0") == 0) {
		// If user gives empty command just return so it takes them back to prompt
		return 1;
	}
	
	add_to_history(args);
	
	if (strcmp(args[0], "exit") == 0) {
		return exit_shell(args);
	}
	
	cpid = fork();
	
	if (cpid == 0) {
		if (execvp(args[0],args) < 0)
			printf("icsh: bad command \n");
		exit(EXIT_FAILURE);
	}
	
	else if (cpid < 0)
		printf(RED "Error forking" RESET "\n");
	else {
		waitpid(cpid, & status, WUNTRACED);
	}
	
	return 1;
}

void loop() {

	// Function for the main shell loop

	char * line;
	char * * args;
	int running = 1;

	do {
		printf("icsh $: "); // Print command prompt symbol
		line = read_line();
		args = split_line(line);
		running = execute(args);
		free(line);
		free(args);
	} while (running);

}

int main() {
	printf("Starting IC shell... \n");
	loop();
	return 0;
}
