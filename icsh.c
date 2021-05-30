#include <stdio.h> 
#include <string.h> 
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#define TOK_DELIM " \t\r\n"
#define RED "\033[0;31m"
#define RESET "\e[0m"

/*------------*
 * INITIALIZE *
 *------------*/
 
void add_to_history(char **);
char *read_line();
char **split_line(char *);
int execute(char *,int);
char **get_last_command(int);
void scripted_loop(char *);
void loop();
void signal_handler(int);
char *trim_spaces(char *);

int saved_stdout;
int saved_stdin;

int previous_exit_code = 0;

struct job {
	char command[256];
	int pid;
	int job_id;
	int status; // 0: Stopped, 1: BG, 2: FG
};

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

char **get_last_command(int n) {
	// Search through history file to get last command and execute it
	// n = 0 means script mode
	
	FILE *history = fopen(".icsh_history", "r");
	char line[1024]={0,};
	
	while( fgets(line, 1024, history) !=NULL ) {}
	
	if (n) {
		printf("%s", line);
	}
	fclose(history);
	
	return split_line(line);
	
}

/*-----------------*
 * COMMAND PARSING *
 *-----------------*/
 
char *trim_spaces(char *str) {
	// Turns out i need to trim whitespace for it to read filename correctly
	// Make copy of input str to avoid seg fault

	char *temp = (char *) malloc(strlen(str)+1);
	strcpy(temp,str);
	
	// Copied from https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
	
	char *end;

	// Trim leading space
	while(isspace((unsigned char)*temp)) temp++;

	if(*temp == 0)  // All spaces?
	return temp;

	// Trim trailing space
	end = temp + strlen(temp) - 1;
	while(end > temp && isspace((unsigned char)*end)) end--;

	// Write new null terminator character
	end[1] = '\0';

	return temp;
}


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
 * SIGNAL HANDLING *
 *-----------------*/

void signal_handler(int signum) {
	printf("\n");
}

/*-----------------*
 * I/O REDIRECTION *
 *-----------------*/
 
char* input_redirection(char *filename) {
	// Returns string because I have to read the file for commands
	// No idea how else to do this i have no idea how to use C
	
	int in = open(filename, O_RDONLY);
	
	if (in <= 0) {
		fprintf(stderr, "Couldn't open file\n");
		exit(errno);
	}
	
	dup2(in,0);
	close(in);
}

void output_redirection(char *filename) {
	int out = open(filename, O_TRUNC | O_CREAT | O_WRONLY, 0666);
	
	if (out <= 0) {
		fprintf(stderr, "Couldn't open file\n");
		exit(errno);
	}
	
	dup2(out,1);
	close(out);
}

char* redirection(char *line) {
	// Function checks if command has redirection symbol (">" or "<"
	// If command has redirection symbol then trim it from the line
	// e.g. "ls -l > some_file" becomes "ls -l somefile"
	
	char* c;
	
	// Check if command line wants '>' or '<' redirection or no redirect
	if (strchr(line, '>')) {
		c = ">";
	}
	else if (strchr(line, '<')) {
		c = "<";
	}
	else {
		c = NULL;
	}
	
	if (c) {
		char *token;
		char *temp = (char *) malloc(strlen(line)+1);
		
		strcpy(temp,line); // copy line because trying to access line directly would probably cause seg fault
		
		token = strtok(temp, c); // take part of string before '>' or '<'
		char *command = trim_spaces(token);
		token = strtok(NULL, c); // take part of string after '>' or '<'
		char *arg = trim_spaces(token);
		
		if (strchr(c, '<')) {
			input_redirection(arg);
		}
		else if (strchr(c, '>')) {
			output_redirection(arg);
		}
	
		free(temp);
		return command;
	}
	return line;
}

/*----------------------------------------*
 * FOREGROUND/BACKGROUND PROCESS HANDLING *
 *----------------------------------------*/

char* background_execute(char *line) {
	// If last character of command is & then it's a background execute command
	char *token;
	char *temp = (char *) malloc(strlen(line)+1);
	
	strcpy(temp,line); 
	
	token = strtok(temp, "&"); // take part of string before '&' which is the command
	char *command = trim_spaces(token);
	return command;
}

/*-----------------*
 * SHELL EXECUTION *
 *-----------------*/

int execute(char * line, int script_mode) {

	// Execute command 
	char * * args;	
	pid_t cpid;
	int status;
	int is_bgp;
	
	if (line[strlen(line)-1] == '&') {
		args = split_line(background_execute(line));
		is_bgp = 1;
	}
	else {
		args = split_line(line);
		is_bgp = 0;
	}

	if (strcmp(args[0], "!!") == 0) {	
		// If command is !! then repeat last command
		args = get_last_command(1);
	}
	if (strcmp(args[0], "exit") == 0) {
		if (args[1]) {
			printf("Goodbye :( \n");
			exit(atoi(args[1]));
		}
		else {
			printf("Exit code not specified \n");
			printf("Re-enter command with parameters in the form: exit <num> \n");
			return 1;
		}
	}
	
	if ( (strcmp(args[0],"echo") == 0) && (strcmp(args[1],"$?") == 0) ) {
		printf("%d \n",previous_exit_code);
		previous_exit_code = 0;
		return 1;
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
	
		struct job current_job;
		waitpid(cpid, & status, 0);
		previous_exit_code = WEXITSTATUS(status);
	}
	
	return 1;
}

void loop() {

	// Function for the main shell loop

	char * line;
	char * command;
	char * args;
	int running = 1;

	do {
		signal(SIGINT,signal_handler);
		signal(SIGTSTP,signal_handler);
		printf("icsh $: "); // Print command prompt symbol
		//fgets(line,1024,stdin);
		line = read_line();
		command = redirection(line);
		args = trim_spaces(command);
		if (strcmp(args, "\0") != 0) {
			// Only execute if command is not empty
			add_to_history(split_line(line));
			running = execute(args,1);
		}
		dup2(saved_stdout, 1);
		dup2(saved_stdin, 0);
	} while (running);

}

void scripted_loop(char *arg) {

	FILE *script = fopen(arg, "r");
	
	if (script) {
		char *  args;
		char line[1024]={0,};
		int is_bgp;
		char * command;
		while(fgets(line, 1024, script) !=NULL ) {
			// Read script line by line and do the command loop
			// Kinda messy and redundant code but for now just want it to work
			command = redirection(line);
			args = trim_spaces(command);
			if (strcmp(args, "\0") != 0) {
				// Only execute if command is not empty
				add_to_history(split_line(line)); 
				execute(args,2);
			}
			free(args);
		}
		fclose(script);
	}
	else {
		printf("Invalid file. Please try again :) \n");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	saved_stdout = dup(1);
	saved_stdin = dup(0);
	printf("Starting IC shell... \n");
	if (argv[1]) {
		scripted_loop(argv[1]);
	}
	loop();
	return 0;
}
