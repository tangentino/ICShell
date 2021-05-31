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
#define CHAR_SIZE 256
#define PID_SIZE 32768
#define JOBS_SIZE 1000
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

int job_number = 0;

struct job {
	char command[CHAR_SIZE];
	int pid;
	int job_id;
	int status; // 0: foreground, 1: background, 2: stopped
};

struct job jobs_list[PID_SIZE]; // List of jobs. Index is PID
pid_t job_pid_list[JOBS_SIZE]; // List of PIDs of jobs. Index is job ID
pid_t ppid;

/*--------------------*
 * HISTORY MANAGEMENT *
 *--------------------*/

void add_to_history(char **args) {
	// Adds user input to history file

	FILE *history = fopen(".icsh_history", "a+");
	int i = 0;
	
	if (strcmp(args[0], "!!") == 0) {	
		// If command is !! then dont count
		return;
	}
	
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

void c_handler() {
	printf("\n");
}

void z_handler() {
	printf("\n");
}

void process_handler(int sig, siginfo_t *sip, void *null) {
	int status = 0;
	if (sip->si_pid == waitpid(sip->si_pid, &status, WNOHANG | WUNTRACED)) {
		if (WIFEXITED(status) || WTERMSIG(status)) {
			struct job* j = &jobs_list[sip->si_pid];
			if (WIFEXITED(status) && j != NULL && j->status == 1 && j->job_id) {
				// Job complete
				printf("[%d] Done             %s\n",j->job_id,j->command); 
				fflush(stdout);
				
				job_pid_list[j->job_id] = 0;
				struct job temp;
				temp.job_id = 0;
				temp.pid = 0;
				jobs_list[sip->si_pid] = temp;
			}
			else if (WIFSTOPPED(status) && j != NULL) {
				if (!(j->job_id)) {
					j->job_id = ++job_number;
					job_pid_list[j->job_id] = sip->si_pid;
				}
				j->status = 2;
				printf("\n[%d] Stopped             %s\n",j->job_id,j->command);
				fflush(stdout);   
			}
		}
	}
}

void signal_handling() {
	struct sigaction c; // ctrl+c
	struct sigaction z; // ctrl+z
	struct sigaction sa; // process
	
	c.sa_handler = c_handler;
	z.sa_handler = z_handler;
	sa.sa_sigaction = process_handler;
	
	c.sa_flags = 0;
	z.sa_flags = 0;
	sa.sa_flags = SA_SIGINFO;
	
	sigemptyset(&c.sa_mask);
	sigemptyset(&z.sa_mask);
	sigfillset(&sa.sa_mask);
	
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	
	sigaction(SIGINT, &c,NULL);
	sigaction(SIGTSTP, &z, NULL);
	sigaction(SIGCHLD, &sa, NULL);
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
 
char *parse_job_status(int job_status) {
	// Turns job status int into job status string
	if (job_status == 1) {
		return "Running";
	}
	else if (job_status == 2) {
		return "Stopped";
	}
	return "";
}

int parse_job_id(char *arg) {
	char* ans;
	if (arg == NULL || arg[0] != '%') {
		printf("Invalid argument\n");
		return 0;
	}
	ans = strtok(arg,"%");
	if (!ans) {
		printf("Job ID not found. Please enter job ID in the form %%id \n");
		return 0;
	}
	return atoi(ans);
}

void print_jobs() {
	// Prints out all current jobs
	for (int i = 1; i <= job_number; i++) {
		pid_t pid = job_pid_list[i];
		if (pid != 0) {
			struct job* j = &jobs_list[pid];
			if (j != NULL && j->status > 0) {
				printf("[%d] %s              %s\n",i,parse_job_status(j->status),j->command);
			}
		}
	}
}
char* background_execute(char *line) {
	// If last character of command is & then it's a background execute command
	char *token;
	char *temp = (char *) malloc(strlen(line)+1);
	
	strcpy(temp,line); 
	
	token = strtok(temp, "&"); // take part of string before '&' which is the command
	char *command = trim_spaces(token);
	return command;
}

void bg_command(char *arg) {
	// Put job in background
	int job_id = parse_job_id(arg);
	pid_t pid = job_pid_list[job_id]; // Get the pid from job id
	if (pid) {
		struct job* j = &jobs_list[pid]; // Get job from pid
		printf("[%d] %s &\n",j->job_id,j->command);
		j->status = 1; // Change job status to background
		kill(pid,SIGCONT); // Send SIGCONT so process resumes where it left off
	}
	else {
		printf("Unable to find foreground job with ID %d\n",job_id);
	}
}

void fg_command(char *arg) {
	int job_id = parse_job_id(arg);
	pid_t pid = job_pid_list[job_id];
	if (pid) {
		struct job* j = &jobs_list[pid];
		j->status = 0; // Change status to foreground
		printf("%s\n",j->command); // Print command to terminal
		kill(pid,SIGCONT);
		int status;
		setpgid(pid,pid);
		tcsetpgrp(0,pid);
		waitpid(pid,&status,0);
		waitpid(pid,&status,0);
		tcsetpgrp(0,ppid);
		previous_exit_code = WEXITSTATUS(status);
	}
	else {
		printf("Unable to find background job with ID %d\n",job_id);
	}
}
/*-----------------*
 * SHELL EXECUTION *
 *-----------------*/

int execute(char * line, int script_mode) {

	// Execute command 
	char * command;
	char *temp = (char *) malloc(strlen(line)+1);
	char * * args;	
	pid_t cpid;
	int status;
	int is_bgp;
	
	strcpy(temp,line);
	
	
	
	if (line[strlen(line)-1] == '&') {
		command = background_execute(line);
		is_bgp = 1;
	}
	else {
		command = line;
		is_bgp = 0;
	}
	
	args = split_line(command);
	
	if (strcmp(args[0], "!!") == 0) {	
		// If command is !! then repeat last command
		args = get_last_command(script_mode);
	}
	
	add_to_history(args);
	
	if (strcmp(args[0],"jobs") == 0) {
		// List all jobs command
		print_jobs();
		return 1;
	}
	
	if (strcmp(args[0],"bg") == 0) {	
		// bg % <job_id> command
		bg_command(args[1]);
		return 1;
	}
	
	if (strcmp(args[0],"fg") == 0) {
		// fg % <job_id> command
		fg_command(args[1]);
		return 1;
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
		strcpy(current_job.command,temp);
		current_job.pid = cpid;
		current_job.status = is_bgp;
		current_job.job_id = is_bgp ? ++job_number : 0;
		jobs_list[cpid] = current_job;
		setpgid(cpid,cpid);
		
		if (is_bgp) {
			job_pid_list[job_number] = cpid;
			printf("[%d] %d\n",job_number,cpid);
		}
		else {
			tcsetpgrp(0,cpid);
			waitpid(cpid, & status, 0);
			tcsetpgrp(0,ppid);
			previous_exit_code = WEXITSTATUS(status);
		}
	}
	free(temp);
	return 1;
}

void loop() {

	// Function for the main shell loop

	char * line;
	char * command;
	char * args;
	int running = 1;

	do {
		printf("icsh $: "); // Print command prompt symbol
		//fgets(line,1024,stdin);
		line = read_line();
		command = redirection(line);
		args = trim_spaces(command);
		if (strcmp(args, "\0") != 0) {
			// Only execute if command is not empty
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
				execute(args,0);
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
	ppid = getpid();
	signal_handling();
	saved_stdout = dup(1);
	saved_stdin = dup(0);
	printf("Starting IC shell... \n");
	if (argv[1]) {
		scripted_loop(argv[1]);
	}
	loop();
	return 0;
}
