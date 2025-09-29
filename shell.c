#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include "utils.h"

#define CONSOLE_BUFFER_SIZE 1024
#define FILE_BUFFER_SIZE 8192
#define NEWLINE '\n'

const char* HISTORY_PATH = ".421history";

char* prompt = "$";

int appendHistory(char* entry);
int processCommand(char* command);
char* readCommand();
void writePrompt();

/***************************************
 *	Console Functions
 ***************************************/

/**
 * Initializes user prompt.
 */
void initPrompt(){
	// Assemble user prompt
	char* userName = getUserName();
	char* hostName = getHostName();
	int userNameLen = strlen(userName);
	int hostNameLen = strlen(hostName);
	int len = 0;
	prompt = malloc(userNameLen + hostNameLen + 5);
	strcpy(prompt, userName);
	len += userNameLen;
	strcpy(prompt + len, "@");
	len++;
	strcpy(prompt + len, hostName);
	len += hostNameLen;
	strcpy(prompt + len, " $ ");

	// Deallocate values
	free(userName);
	free(hostName);
}

/**
 * Reads next line of input from console and blocks on input.
 * Caller is responsible for deallocating result.
 * @return input line or NULL if nothing entered.
 */
char* readInput(){
	char* result = NULL;
	size_t resultSize = 0;
	size_t bytesRead = 0;
	size_t bufferSize = CONSOLE_BUFFER_SIZE;

	do {
		// Allocate/reallocate memory
		result = realloc(result, resultSize + bufferSize);
		if (result == NULL) {
			fprintf(stderr, "readInput: ERROR: realloc failed!\n");
			return result;
		}

		// Read next chunk of input
		if (fgets(result + bytesRead, bufferSize, stdin) == NULL) {
			// Break on read error or EOF
			break;
		}
		bytesRead += strlen(result + bytesRead);

		// Check for end of line
		if (bytesRead > 0 && result[bytesRead - 1] == NEWLINE) {
			break;
		}

		// Accumulate result size (excluding null terminator)
		resultSize += bufferSize - 1;
	} while (1);

	return result;
}

/**
 * Reads next command from console and blocks on input.
 * Caller is responsible for deallocating result.
 * @return command or NULL if no new command entered.
 */
char* readCommand() {
	// Read line of input from console
	char* line = readInput();

	// Write command to history
	appendHistory(line);

	// Process escape sequences
	char* command = unescape(line, stderr);
	if (line != NULL){
		free(line);
	}

	// Trim whitespace from command
	trim(command);

	// Return command
	return command;
}

/**
 * Writes user prompt to console.
 */
void writePrompt(){
	printf(prompt);
	fflush(stdout);
}

/***************************************
 *	History Functions
 ***************************************/

/**
 * Appends history entry to end of history file.
 * @param entry	history entry.
 * @return number of history entries written.
 */
int appendHistory(char* entry) {
	int result = 0;
	if (entry != NULL){
		// Open file in append mode
		FILE* fp = fopen(HISTORY_PATH, "a");

		// If file open failed
		if (fp == NULL) {
			// Print error message and return 0
			fprintf(stderr, "writeHistory: ERROR: Failed to open command history file '%s'!\n", HISTORY_PATH);
			return result;
		}

		// Write history entry to file
		fputs(entry, fp);
		//fputc(NEWLINE, fp);

		result++;

		// Close file
		fclose(fp);
	}

	// Return number of entries written
	return result;
}

/**
 * Clears all entries from history file.
 * @return 1 if succeeded, 0 if failed.
 */
int clearHistory() {
	int result = 1;

	// Open file in write mode
	FILE* fp = fopen(HISTORY_PATH, "w");

	// If file open failed
	if (fp == NULL) {
		// Write error message and return failure
		fprintf(stderr, "clearHistory: ERROR: Failed to clear command history file '%s'!\n", HISTORY_PATH);
		return 0;
	}

	// Close file
	fclose(fp);

	return result;
}

/**
 * Deallocated memory for history entry list.
 * @param entries	array of history entries.
 * @param numEntries	number of history entries in array.
 */
void deallocateHistory(char** entries, int numEntries){
	if (entries != NULL){
		for (int i = 0; i < numEntries; i++){
			if (entries[i] != NULL){
				free(entries[i]);
			}
		}
	}
}

/**
 * Prints history entries to the standard output.
 * @param history	array of history entries.
 * @param numEntries	number of history entries to print.
 */
void printHistory(char** history, int numEntries){
	if (history != NULL){
		for (int i = 0; i < numEntries; i++){
			printf("%s\n", history[i]);
		}
	}
}

/**
 * Reads given number of history entries from end of command history file
 * and returns number of entries actually read in second argument.
 * Caller is responsible for deallocating result.
 * @param numEntries	number of history entries to read.
 * @param numReturned	pointer to number of history entries returned.
 * @return array of history entries or NULL if error occurred.
 */
char** readHistory(int numEntries, int* numReturned) {
	char** result = malloc(sizeof(char*) * (numEntries + 1));
	int numAllocated = numEntries;
	int numRead = 0;
	if (numEntries > 0){
		FILE *fp;
		char buffer[FILE_BUFFER_SIZE];

		// Open file in read mode
		fp = fopen(HISTORY_PATH, "r");

		// If file open failed
		if (fp == NULL) {
			// Print error and return NULL
			fprintf(stderr, "readHistory: ERROR: Failed to open command history file '%s'!\n", HISTORY_PATH);
			return NULL;
		}

		// Read and print the content line by line
		while (fgets(buffer, sizeof(buffer), fp) != NULL) {
			if (numRead == numAllocated){
				numAllocated += numEntries;
				result = realloc(result, sizeof(char*) * numAllocated);
			}
			result[numRead] = malloc(strlen(buffer) + 1);
			strcpy(result[numRead], buffer);
			trimTrailing(result[numRead]);
			numRead++;
		}

		// Remove current (last) command from results
		numRead--;
		free(result[numRead]);
		result[numRead] = NULL;

		// Limit result if necessary
		if (numRead > numEntries){
			int adjust = numRead - numEntries;
			for (int i = 0; i < numRead; i++){
				if (i < numEntries){
					// Deallocate replaced entry
					free(result[i]);
					// Shift entry in results
					result[i] = result[i + adjust];
				}
				else {
					// Remove and deallocate entry from result
					result[i] = NULL;
				}
			}
			numRead = numEntries;
		}

		// Return number of results
		*numReturned = numRead;

		// Close the file
		fclose(fp);
	}

	// Return array of entries
	return result;
}

/**
 * Writes array of history entries to history file.
 * @param entries	array of history entries.
 * @param numEntries	number of history entries to write.
 * @return number of history entries written.
 */
int writeHistory(char** entries, int numEntries) {
	int result = 0;
	if (entries != NULL){
		// Open file in append mode
		FILE* fp = fopen(HISTORY_PATH, "a");

		// If file open failed
		if (fp == NULL) {
			// Print error message and return 0
			fprintf(stderr, "writeHistory: ERROR: Failed to  open command history file '%s'!\n", HISTORY_PATH);
			return result;
		}

		// For each history entry
		for (int i = 0; i < numEntries; i++){
			// Write history entry to file
			fputs(entries[i], fp);
			fputc(NEWLINE, fp);
			result++;
		}

		// Close file
		fclose(fp);
	}

	// Return number of entries written
	return result;
}

/***************************************
 *	Control Functions
 ***************************************/

/**
 * Executes command processing loop until user exits shell.
 */
void commandLoop(){
	char* command = "";

	// Loop until user enters "exit"
	int loop = 1;
	while (loop){
		// Display user prompt
		writePrompt();

		// Read command
		command = readCommand();

		if (command != NULL){
			// Process command
			if (processCommand(command) < 0){
				loop = 0;
			}

			// Deallocate command
			free(command);
		}
	}
}

/**
 * Deallocates resources.
 */
void deallocate(){
	// Deallocate prompt
	if (prompt != NULL){
		free(prompt);
	}
}

/**
 * Deallocates array of arguments parsed from user command.
 * @param args		array of arguments which are pointers to
 *			locations inside original command string.
 * @param numArgs	number of arguments.
 */
void deallocateArgs(char** args, int numArgs){
	if (args != NULL){
		if (numArgs > 0){
			// Deallocate command copy
			free(args[0]);
		}
		// Deallocate argument array
		free(args);
	}
}

/**
 * Deallocates any resources and exits shell.
 */
void exitShell(){
	// Deallocate resources
	deallocate();

	// Exit with success
	exit(EXIT_SUCCESS);
}

/**
 * Extracts program name/path from user command.
 * Caller is responsible for deallocating result.
 * @param command	user command.
 * @return program name or path or NULL if argument is NULL.
 */
char* getProgramName(char* command){
	char* result = NULL;

	if (command != NULL){
		int i = 0;
		while(command[i] != '\0' && !isspace((unsigned char)command[i])){
			i++;
		}
		result = malloc(i + 1);
		strncpy(result, command, i);
		result[i] = '\0';
	}

	return result;
}

/**
 * Extracts all tokens from user command and returns array of tokens
 * and number of tokens extracted in second argument.
 * Caller is responsible for deallocating result which contains pointers
 * to locations in command string. Other than array itself, only the
 * first pointer in the array needs to be deallocated.
 * @param command	user command.
 * @param numArgs	pointer to number of token extracted.
 * @return array of tokens.
 */
char** getArgs(char* command, int* numArgs){
	char** result = NULL;

	if (command != NULL){
		// Copy command string
		int len = strlen(command);
		char* commandCopy = malloc(len + 1);
		strcpy(commandCopy, command);

		// Extract tokens from command copy (modified by getTokens)
		*numArgs = 0;
		result = getTokens(commandCopy, numArgs);
	}

	return result;
}

/**
 * Processes procread command.
 * @param fileName	file name or relative path.
 * @return 0 on success, 1 on failure.
 */
int processProcread(char* fileName){
	int result = 0;
	FILE *fp;
	char buffer[1024];

	// Validate file name
	if (fileName == NULL || strlen(fileName) == 0){
		// Print error message and return
		fprintf(stderr, "ERROR: file name missing\n");
		return 1;
	}
	else if (fileName[0] == '/'){
		// Print error message and return
		fprintf(stderr, "ERROR: only relative file paths are supported\n");
		return 1;
	}

	// Format file path
	char filename[256];
	if (strncmp("proc/", fileName, 5) == 0){
		strcpy(filename, fileName);
	}
	else {
		sprintf(filename, "/proc/%s", fileName);
	}

	// Open the file
	fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("ERROR: Process file not found");
		return 1;
	}

	// Read process information using fgets
	// The format string specifies the expected data types and order
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		printf("%s", buffer);
	}

	// Close file
	fclose(fp);

	// Return success
	return result;
}

/**
 * Processes a system command by forking new process to
 * execute it.
 * @param command	user system command.
 * @return 0 on success and 1 on failure.
 */
int processSystemCommand(char* command){
	// Fork new process
	pid_t pid = fork();

	// If fork failed
	if (pid == -1) {
		// Print error message and return failure
		perror("processSystemCommand: fork failed");
		return 1;
	// If this is child process
	} else if (pid == 0) {
		printf("Child process: My PID is %d, My parent's PID is %d\n", getpid(), getppid());
		printf("Child process: executing '%s'\n", command);
		// Parse command
		int numArgs;
		char* program = getProgramName(command);
		char** args = getArgs(command, &numArgs);

		// Execute system command in new process, searching PATH if necessary
		if (execvp(program, args) < 0){
			// Print error message
			fprintf(stderr, "processSystemCommand: ERROR - command execution failed with '%s'\n", strerror(errno));

			// Deallocate program name and argument list
			free(program);
			deallocateArgs(args, numArgs);

			// Return failure
			return 1;
		}

		// Deallocate program name and argument list
		free(program);
		deallocateArgs(args, numArgs);
	// If this is parent process
	} else {
		// This code runs in the parent process
		printf("Parent process: My PID is %d, My child's PID is %d\n", getpid(), pid);
		printf("Parent process: waiting for child to complete...\n");
		wait(NULL);
		printf("Parent process: child completed\n");
	}

	// Return success
	return 0;
}

/**
 * Processes user command.
 * @param command	user command.
 * @return 0 on success and 1 on failure.
 */
int processCommand(char* command){
	int result = 0;

	// Built-in: exit
	if (!strcmp(command, "exit")){
		// Terminate loop
		result = -1;
	}
	else {
		// Parse first command token
		char* firstToken = getProgramName(command);

		// Built-in: procread
		if (!strcmp(firstToken, "procread")){
			// Validate command
			int numArgs;
			char** args = getArgs(command, &numArgs);
			if (numArgs < 2){
				// Print error message and return failure
				fprintf(stderr, "ERROR: file argument required\n");	
				result = 1;
			}
			else if (numArgs > 2){
				// Print error message and return failure
				fprintf(stderr, "ERROR: only 1 argument is permitted\n");	
				result = 1;
			}
			else {
				// Process procread command
				result = processProcread(args[1]);
			}

			// Deallocate argument list
			deallocateArgs(args, numArgs);
		}
		// Built-in: history
		else if (!strcmp(firstToken, "history")){
			// Read command entries from history file
			int numRead;
			char** history = readHistory(10, &numRead);
			if (history != NULL){
				// Print command entries
				printHistory(history, numRead);
				// Deallocate command entries
				deallocateHistory(history, numRead);
			}
			else {
				result = 1;
			}
		}
		// System commands
		else if (strlen(command) > 0){
			// Validate command
			if (command[0] == '"' || command[0] == '\''){
				// Print error message and return failure
				fprintf(stderr, "ERROR: invalid command\n");	
			}
			// Assume command is system command
			result = processSystemCommand(command);
		}

		// Deallocate first token
		free(firstToken);
	}

	// Return result
	return result;
}

/**
 * Main entry point.
 * param argc	number of command-line arguments.
 * param argv	list of command-line arguments.
 * returns program exit status: 0 on success, non-0 otherwise.
 */
int main(int argc, char *argv[]) {
	// Initialize user prompt
	initPrompt();

	// Initialize history file
	clearHistory();

	// Run command loop
	commandLoop();
	
	// Exit shell
	exitShell();
}
