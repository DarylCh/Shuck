////////////////////////////////////////////////////////////////////////
// COMP1521 21t2 -- Assignment 2 -- shuck, A Simple Shell
// <https://www.cse.unsw.edu.au/~cs1521/21T2/assignments/ass2/index.html>
//
// This program replicates a simpler subset of Shell in that it can be 
// used like a terminal to execute a series of commands. It allows for
// the execution of several built in commands (such as changing directory,
// and printing the command history) and external commands native to Linux. 
// Input redirection has also been implemented as feature in this program, 
// however the piping feature is still in its developmental stage and 
// requires further refining to support the use of more than 2 pipes.
// 
// In short, this program uses a series of functions to break given arguments
// down and analyse which process would need to be performed on them. It treats
// standard processes and ones that require input redirection as different and 
// thus sends them down different function paths (namely process_spawn, io_spawn
// and process_pipes). Unfortunately, I ran out of time to finalise my subset 5
// implementation. As a result, the code is unfinished and only supports up to 
// 2 pipes. With more time, Im sure I could have solved this issue and 
// reorganised its structure to a better standard. 
//
// Written by DARYL CHANG (z5078401) on 06/08/21.
//
// 2021-07-12    v1.0    Team COMP1521 <cs1521@cse.unsw.edu.au>
// 2021-07-21    v1.1    Team COMP1521 <cs1521@cse.unsw.edu.au>
//     * Adjust qualifiers and attributes in provided code,
//       to make `dcc -Werror' happy.
//

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// [[ TODO: put any extra `#include's here ]]
#include <limits.h>
#include <spawn.h>
#include <ctype.h>
#include <glob.h>
// [[ TODO: put any `#define's here ]]

#define ISINPUT 1
#define ISOUTPUT_1 2
#define ISOUTPUT_2 3
#define ISPIPE_1 4 
#define ISPIPE_2 5
#define IS_IO 6
#define IS_IO_2 7
#define ERROR 8
//
// Interactive prompt:
//     The default prompt displayed in `interactive' mode --- when both
//     standard input and standard output are connected to a TTY device.
//
static const char *const INTERACTIVE_PROMPT = "shuck& ";

//
// Default path:
//     If no `$PATH' variable is set in Shuck's environment, we fall
//     back to these directories as the `$PATH'.
//
static const char *const DEFAULT_PATH = "/bin:/usr/bin";

//
// Default history shown:
//     The number of history items shown by default; overridden by the
//     first argument to the `history' builtin command.
//     Remove the `unused' marker once you have implemented history.
//
static const int DEFAULT_HISTORY_SHOWN __attribute__((unused)) = 10;

//
// Input line length:
//     The length of the longest line of input we can read.
//
static const size_t MAX_LINE_CHARS = 1024;

//
// Special characters:
//     Characters that `tokenize' will return as words by themselves.
//
static const char *const SPECIAL_CHARS = "!><|";

//
// Word separators:
//     Characters that `tokenize' will use to delimit words.
//
static const char *const WORD_SEPARATORS = " \t\r\n";

// [[ TODO: put any extra constants here ]]

// [[ TODO: put any type definitions (i.e., `typedef', `struct', etc.) here ]]

// Provided functions
static void execute_command(char **words, char **path, char **environment, 
                            char *history[11]);
static void do_exit(char **words);
static int is_executable(char *pathname);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);


// [[ TODO: put any extra function prototypes here ]]

// Subset 0 - Built-in Functions
static void change_dir(char **words, char *program, int io);
static void current_dir(char **words, char *program, int io); 

// Subset 1 - Spawn Functions
static void process_spawn(char **words, char **path, char **environment, 
                          char *program, bool *res);
static char *find_command(char *program, char **path);
static char *check_program(char *program, char *arguments[], 
                           char **words, char **path);

// Subset 2 - History functions
static void process_history(char *program, char **words, 
                            char **history, int io);
static void check_history(char *string, char *history[11]);
static void write_history(char *address, char *string, bool exists);
static void remove_history(char *history[11]);
static char *history_address(char *homeAdd);
static char **new_Args(char **matchArray, int size);
static char *command_string(char **words, int wordLen);
static void record_command(char *commString, char *history[11]);
static char **recall_history(char **words, char **history, int index);
static int determine_index(char **words, char **history);

// Subset 3 - Glob Functions 
static char **glob_check(char **words);

// Subset 4 pipe functions 
static int check_io(char **words);
static int verify_io(char **words, int inputSign, int outputSign, int wordsEnd);
static void io_spawn(char **words, char **path, char **environment, 
                     char *program, bool *res, int io, char *outputFile, 
                     char **temp);
static char **io_words(char **words, int io, int wordsEnd);
static void io_words_input(char **words, char **newWords, int newWordsEnd);
static void io_words_output(char **words, char **newWords, int newWordsEnd);
static void initialise_arguments(char **words, char *arguments[10], 
                                 char *program);
static void pipe_input(pid_t pid, char *program, char **environment,
                       char *arguments[10], int pipe_fd[2], char **temp, 
                       char *input);
static void pipe_output(pid_t pid, char *program, char **environment, 
                        char *arguments[10], 
                        int pipe_fd[2], char *output, int io);
static void pipe_both(pid_t pid, char *program, char **environment, 
                      char *arguments[10], int pipe_fd1[2], 
                      int pipe_fd2[2], char **temp, int io, char *input, 
                      char *output);
static void exit_status(pid_t pid, char *program);


// Subset 5 pipes functions
static int check_pipes(char **words);
static void process_pipes(char **words, char **path, char **environment, 
                          char *program,  bool *res, int io, 
                          char *outputFile, char **temp);


// helper functions
static int get_size(char **array);
static int command_len(char **words);

int main (void)
{
    // Ensure `stdout' is line-buffered for autotesting.
    setlinebuf(stdout);

    // Environment variables are pointed to by `environ', an array of
    // strings terminated by a NULL value -- something like:
    //     { "VAR1=value", "VAR2=value", NULL }
    extern char **environ;

    char *history[100] = {NULL};
    // Grab the `PATH' environment variable for our path.
    // If it isn't set, use the default path defined above.
    char *pathp;
    if ((pathp = getenv("PATH")) == NULL) {
        pathp = (char *) DEFAULT_PATH;
    }
    char **path = tokenize(pathp, ":", "");

    // Should this shell be interactive?
    bool interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    // Main loop: print prompt, read line, execute command
    while (1) {
        // If `stdout' is a terminal (i.e., we're an interactive shell),
        // print a prompt before reading a line of input.
        if (interactive) {
            fputs(INTERACTIVE_PROMPT, stdout);
            fflush(stdout);
        }

        char line[MAX_LINE_CHARS];
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL)
            break;

        // Tokenise and execute the input line.
        char **command_words =
            tokenize(line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
        execute_command(command_words, path, environ, history);
        free_tokens(command_words);
    }

    free_tokens(path);
    return 0;
}

//
// Execute a command, and wait until it finishes.
//
//  * `words': a NULL-terminated array of words from the input command line
//  * `path': a NULL-terminated array of directories to search in;
//  * `environment': a NULL-terminated array of environment variables.
//



static void execute_command(char **words, char **path, char **environment, 
                            char *history[11])
{
    assert(words != NULL);
    assert(path != NULL);
    assert(environment != NULL);

    char **temp = words;
    bool record = true;
    char *program = NULL;
    int io = check_io(words);
    int wordsEnd = get_size(words) - 1;
    char *outputFile = words[wordsEnd];
    int commLen = command_len(words);
    
    char *commString = command_string(words, commLen);
    
    if (strcmp(words[0], "<") == 0) {
        program = words[2];
    }
    else {
        program = words[0];
    }
    
    if (program == NULL) {
        // nothing to do
        return;
    }

    if (strcmp(program, "!") == 0) {
        int index = determine_index(words, history);
        if (index == -1) {
            return;
        }

        words = recall_history(words, history, index);
        program = words[0];
        commLen = command_len(words);
        commString = command_string(words, commLen);
        printf("%s", commString);
    }

    // check arguments to see if they require pattern matching
    if (words[1] != NULL) {
        if (strchr(words[1], '*') != NULL || strchr(words[1], '?') != NULL ||
            strchr(words[1], '[') != NULL || strchr(words[1], '~') != NULL) {
            words = glob_check(words);
        }
    }
    
    
    if (strcmp(program, "exit") == 0) {
        do_exit(words);
        // `do_exit' will only return if there was an error.
        return;
    }

    else if (strcmp(program, "cd") == 0) {
        change_dir(words, program, io);
    }

    else if (strcmp(program, "pwd") == 0) {
        current_dir(words, program, io);
    }

    else if (strcmp(program, "history") == 0) {
       process_history(program, words, history, io);
    }
    
    // attemps to find an external process to spawn    
    else {
        int pipes = check_pipes(words);
        if (pipes != 0) {
            process_pipes(words, path, environment, 
                          program, &record, io, outputFile, temp);
        }
        else if (io == 0) {
            process_spawn(words, path, environment, program, &record);
        }
        else {
            io_spawn(words, path, environment, program, 
                     &record, io, outputFile, temp);
        }
        
    }
    
    if (record == true) {
        record_command(commString, history);
    }
    words = temp;
}

// Subset 0 Functions

// This function changes the current working directory to a new one
// Returns an error message if there are any issues
static void change_dir(char **words, char *program, int io) {
    char *newPath = words[1];
    if (io != 0) {
        fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", program);
        return;
    }
    if (newPath == NULL) {
        char *home = getenv("HOME");
        chdir(home);
    }
    else if (chdir(newPath) != 0) {
        fprintf(stderr, "cd: %s: No such file or directory\n", newPath);
        return;
    }
}

// This function prints the current working directory
static void current_dir(char **words, char *program, int io) {
    char pathname[PATH_MAX];
    if (io != 0) {
        fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", program);
        return;
    }
    if (words[1] != NULL) {
        fprintf(stderr, "%s: too many arguments\n", program);
        return;
    }
    if (getcwd(pathname, sizeof pathname) == NULL) {
        perror("pwd");
        return;
    }
    printf("current directory is '%s'\n", pathname);
}

// Subset 1 Functions

// This function spawns a system call for an external program
// by passing the proper arguments to posix_spawn. Spawns for
// one process only without input/output redirection
static void process_spawn(char **words, char **path, 
                          char **environment, char *program, bool *res) {
    // creates a process ID
    char *arguments[50] = {NULL}; 
    if (!is_executable(program)) {
        program = check_program(program, arguments, words, path);
        if (program == NULL) {
            return;
        }
    }

    else {
        initialise_arguments(words, arguments, program);
    }
    
    pid_t pid;
    
    if (posix_spawn(&pid, program, NULL, NULL, arguments, environment) != 0) {
        perror("spawn");
        return;
    }

    exit_status(pid, program);
}


// This function initialises the arguments array with the
// parameters in the words array
static void initialise_arguments(char **words, char *arguments[10], 
                                 char *program) {
    for (int i = 0; words[i] != NULL; i++) {
        arguments[i] = words[i];
    }
}

// This function searches for the given command in the path 
// array and returns a pointer to the program. If is unable 
// to the find the program, it prints the relevent error 
// message
static char *check_program(char *program, char *arguments[], 
                           char **words, char **path) {
    if (strrchr(program, '/') == NULL) {
        char *command = find_command(program, path);
        if (command == NULL) {
            fprintf(stderr, "%s: command not found\n", program);
        }
        else {
            initialise_arguments(words, arguments, program);
            program = command;
            return program;
        }       
    }
    else {
        fprintf(stderr, "%s: command not found\n", program);
    }
    return NULL;
}

// This function that displays exit status of spawned process
// when given its process ID
static void exit_status(pid_t pid, char *program) {
    int exitStatus;
    if (waitpid(pid, &exitStatus, 0) == -1) {
        perror("waitpid");
        exit(1);
    }
    
    if (exitStatus == 256 || exitStatus == 512) {
        exitStatus = exitStatus / 256;
    }
    printf("%s exit status = %d\n", program, exitStatus);
}

// Subset 2 - History Functions

static void process_history(char *program, char **words, 
                            char **history, int io) {
    
    if (io != 0) {
        fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", program);
        return;
    }
    
    int histSize = get_size(history);
    int wordSize = get_size(words);
    int limit = 0;
    
    if (wordSize >= 2 && words[2] != NULL) {
        fprintf(stderr, "history: too many arguments\n");
        return;
    }
    
    if (words[1] == NULL) {
        limit = 10;
    }
    else {
        char c = *words[1];
        if (isdigit(c) != 0) {
            limit = atoi(words[1]);
        }
        else {
            fprintf(stderr, "history: nonnumber: numeric argument required\n");
            return;
        }
    }

    int start = histSize - limit;
    if (start < 0) {
        start = 0;
    }

    for (int i = 0; history[i] != NULL; i++) {
        if (i >= start) {
            printf("%d: %s", i, history[i]);
        }
    }

}

// This function gets the pathname for the .shuck_history
// file contained within the $HOME address. It returns a
// pointer to the character string
static char *history_address(char *homeAdd) {
    int addLen = (strlen(homeAdd) + 1);
    addLen += strlen("/.shuck_history");
    char *addCpy = malloc(addLen * sizeof(char));
    strcpy(addCpy, homeAdd);
    strcat(addCpy, "/.shuck_history");
    return addCpy;
}

// This function checks if the .shuck_history file exists
// for writing. It creates the file if it does not.
static void check_history(char *string, char *history[11]) {
    char *address = history_address(getenv("HOME"));

    if ((fopen(address, "r"))) {
        write_history(address, string, true);
    }

    else {
        write_history(address, string, false);     
    }
    free(address);
}

// This function writes the given string to the .shuck_history
// file
static void write_history(char *address, char *string, bool exists) {
    if (!exists) {
        fopen(address, "w");
    }
    FILE *fp = fopen(address, "a");
    fprintf(fp, "%s", string);
    fclose(fp);
}

// This function finds the relevent position in the history 
// array to add the new command line
static void record_command(char *commString, char *history[]) {
    int i = 0;
    check_history(commString, history);

    while(history[i] != NULL) {
        i++;
    }
    
    history[i] = commString;
}

// This function checks that the given index is valid, 
// or sets it to 10 if none is given, returns the 
// appropriate error message if there is a fault
static int determine_index(char **words, char **history) {
    int last = 0;
    int index = 0; 
    for (int i = 0; history[i + 1] != NULL; i++) {
        last++;
    }
    if (words[1] == NULL) {
        index = last;
    }
    else {
        char c = *words[1];
        if (isdigit(c) != 0) {
            index = atoi(words[1]);
        }
        else {
            fprintf(stderr, "!: nonnumber: numeric argument required\n");
            return -1;
        }
    }
    return index;
    
}

// This function recalls a command from the previous 
// commands depending on parameter given. For example,
// !2 should return the second entry if valid
static char **recall_history(char **words, char **history, int index) {
    for (int i = 0; history[i] != NULL; i++) {
        if (i == index) {
            words = tokenize(history[i], " ", "!><|\n");
            int j = 0;
            while (*words[j] != '\n') {
                j++;
            }
            words[j] = NULL;
            
            return words;
        }
    }    
    return NULL;
}

// This function converts the separate words of arguments 
// from the words array, and converts it into one string.
// This is used to append lines to .shuck_history
static char *command_string(char **words, int wordLen) {
    char *string = malloc(wordLen * sizeof(char));
    strcpy(string, words[0]);
    for (int i = 1; words[i] != NULL; i++) {
        strcat(string, " ");
        strcat(string, words[i]);
        
    }
    strcat(string, "\n");
    return string;
}

// Subset 3 - Glob Functions
static char **glob_check(char **words) {
    glob_t matches;
    int totalMatches = 0;
    char *matchArray[50] = {NULL};
    int matchInd = 0;

    // loop through each argument, generate matches for it.
    for (int i = 0; words[i] != NULL; i++) {
        char *pattern = words[i];
        char *newLine = strchr(pattern, '\n');
        if (newLine != NULL) {
            *newLine = '\0';
        }

        // generate matches for the given argument
        glob(pattern, GLOB_NOCHECK|GLOB_TILDE, NULL, &matches);
        int j = 0;

        totalMatches += matches.gl_pathc;
        while (j < matches.gl_pathc) {
            matchArray[matchInd] = matches.gl_pathv[j];
            matchInd++;
            j++;
        }
    }
    
    // recreate the matches if needed
    if (totalMatches != 0) {
        words = new_Args(matchArray, totalMatches);
        globfree(&matches);

        return words;
    }
    else {
        globfree(&matches);
        return NULL;
    }
    
}

// This function replaces the current array with a new one
// from the matches present in glob
static char **new_Args(char **matchArray, int size) {
    // returns null for empty matches
    if (size == 0 || matchArray[0] == NULL) {
        return NULL;
    }
    char **newWords = malloc((size + 1) * sizeof(*newWords));
    int i;

    for (i = 0; i < size; i++) {
        int wordSize = 1;
        wordSize += strlen(matchArray[i]);
        newWords[i] = malloc(wordSize * sizeof(char));
    }

    for (i = 0; i < size; i++) {
        strcpy(newWords[i], matchArray[i]);
    }
    
    // terminates the arguments with NULL
    newWords[i] = NULL;
    return newWords;
}

// Subset 4 - IO Spawn Functions
// This function manages the process spawn when it requires 
// input/output redirection
static void io_spawn(char **words, char **path, char **environment, 
                     char *program, bool *res, int io, char *outputFile, 
                     char **temp) {
    pid_t pid = 0;
    char *arguments[10] = {NULL};
    
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return;
    }

    int wordsEnd = 0;
    while (words[wordsEnd] != NULL) {
        wordsEnd++;
    }

    // modifies the words array so that it removes file names
    words = io_words(words, io, wordsEnd);
    if (!is_executable(program)) {
        program = check_program(program, arguments, words, path);
        if (program == NULL) {
            return;
        }
    }

    else {
        initialise_arguments(words, arguments, program);
    }

    if (io == ISINPUT) {
        outputFile = temp[1];
        pipe_input(pid, program, environment, arguments, 
                   pipe_fd, temp, outputFile);
    }

    else if (io == ISOUTPUT_1 || io == ISOUTPUT_2) {
        pipe_output(pid, program, environment, arguments, pipe_fd, 
                    outputFile, io);
    }

    else if (io == IS_IO || io == IS_IO_2) {
        int count = get_size(temp);
        char *inputFile = temp[1];
        outputFile = temp[count - 1];

        // create a second pipe 
        int pipe_fd2[2];
        if (pipe(pipe_fd2) == -1) {
            perror("pipe");
            return;
        }    
        pipe_both(pid, program, environment, arguments, pipe_fd, 
                  pipe_fd2, temp, io, inputFile, outputFile);
    }
    exit_status(pid, program);
   
}

// This function attemps to find an executable path for the given
// argument, returns NULL if it is unable to find anything
static char *find_command(char *program, char **path) {
    if (strrchr(program, '/') == NULL) {
        // fprintf(stderr, "--- UNIMPLEMENTED: searching for a program to run\n");
        int progLen = strlen(program);
        for (int i = 0; path[i] != NULL; i++) {
            char *comPath = path[i];
            int dirLen = strlen(comPath);
            int newLen = dirLen + progLen + 2;
            char *newCommand = malloc(newLen * sizeof(char));
    
            strcpy(newCommand, comPath);
            strcat(newCommand, "/");
            strcat(newCommand, program);
            
            if (is_executable(newCommand)) {
                return newCommand;
            }

            free(newCommand);

        }
    }

    return NULL;

}

// This function checks if the given arguments require input/output redirection
// If is so, it passes the arguments to verify_io to determine the exact type
// of redirection needed.
static int check_io(char **words) {
    int wordsEnd = get_size(words) - 1;
    int inputSign = 0;
    int outputSign = 0;
    for (int i = 0; words[i] != NULL; i++) {
        if (strcmp(words[i], "<") == 0) {
            inputSign++;
        }
        if (strcmp(words[i], ">") == 0) {
            outputSign++;
        }
    }
    
    if (inputSign == 0 && outputSign == 0) {
        return 0;
    }

    else {
        int ret = verify_io(words, inputSign, outputSign, wordsEnd);
        return ret;
    }
}

// This function checks the i/o arguments provided by word to determine if they
// are valid. It will return the necessary status code for io_spawn
static int verify_io(char **words, int inputSign, int outputSign, int wordsEnd) {
    if (inputSign >= 1 && outputSign >= 1 &&
        strcmp(words[0], "<") == 0 &&
        strcmp(words[wordsEnd - 1], ">") == 0) {
        if (inputSign == 1 && outputSign == 1) {
            return IS_IO;
        }
        else if (inputSign == 1 && outputSign == 2 &&
                 strcmp(words[wordsEnd - 2], ">") == 0) {    
            return IS_IO_2;
        }
        else {
            fprintf(stderr, "invalid input redirection\n");
            return ERROR;
        }
    }
    
    else if (outputSign > 0 && outputSign <= 2) {
        if (outputSign == 1 && strcmp(words[wordsEnd - 1], ">") == 0) {
            return ISOUTPUT_1;
        }
        else if (outputSign == 2 && strcmp(words[wordsEnd - 1], ">") == 0
                 && strcmp(words[wordsEnd - 2], ">") == 0) {
            return ISOUTPUT_2;
        }
        else {
            fprintf(stderr, "invalid input redirection\n");
            return ERROR;
        }
    }
    
    else if (inputSign == 1) {
        if (strcmp(words[0],"<") == 0 || strcmp(words[1],"<") == 0) {
            return ISINPUT;
        }
        else {
            fprintf(stderr, "invalid input redirection\n");
            return ERROR;
        }
    }
    
    fprintf(stderr, "invalid input redirection\n");
    return ERROR;

}

// This function manages the words array to remove all filenames that are 
// unnecessary as arguments in spanwed processes
static char **io_words(char **words, int io, int wordsEnd) {
    // find length of words
    int newWordsEnd;
    if (io == 0) {
        return 0;
    }
    if (io == ISOUTPUT_2) {
        newWordsEnd = wordsEnd - 3;
    }
    else {
        newWordsEnd = wordsEnd - 2;
    }
    
    int size = newWordsEnd + 2;
    char **newWords = malloc((size) * sizeof(*newWords));
    
    if (io == ISINPUT || io == IS_IO) {
        io_words_input(words, newWords, newWordsEnd);
    }

    else {
        io_words_output(words, newWords, newWordsEnd);
    }

    return newWords;
}

// If io_words determines that the input of a process is to be redirected
// this function changes the words array so that only the necessary arguments
// remain
static void io_words_input(char **words, char **newWords, int newWordsEnd) {
    int found = 0;
    int ind = 0;
    for (int i = 0; ind < newWordsEnd; i++) {
        if (words[i] == NULL) {
            break;
        }
        
        if (strcmp(words[i], ">") == 0) {
            break;
        }

        if (strcmp(words[i], "<") == 0) {
            found = 1;
            i += 2;
        }
        
        if (found == 1) {
            int wordSize = 1;
            wordSize += strlen(words[i]);
            newWords[ind] = malloc(wordSize * sizeof(char));
            strcpy(newWords[ind], words[i]);
            ind++;
        }
    }
    newWords[ind] = NULL;
    
}

// If io_words determines that the output of a process is to be redirected
// this function changes the words array so that only the necessary arguments
// remain
static void io_words_output(char **words, char **newWords, int newWordsEnd) {
    int i;
    for (i = 0; i <= newWordsEnd; i++) {
        if (strcmp(words[i],">") == 0) {
            break;
        }

        int wordSize = 1;
        wordSize += strlen(words[i]);
        newWords[i] = malloc(wordSize * sizeof(char));
        strcpy(newWords[i], words[i]);
    }
    newWords[i] = malloc(sizeof(char));
    newWords[i] = NULL;
}

// This function writes the output of the process to 
// the write section of a pipe. The pipe is then read
// and its contents transferred to a given file
static void pipe_output(pid_t pid, char *program, char **environment, 
                        char *arguments[10], int pipe_fd[2], 
                        char *output, int io) {
    posix_spawn_file_actions_t actions;
    
    if (posix_spawn_file_actions_init(&actions) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // closes unused read end of pipe
    if (posix_spawn_file_actions_addclose(&actions, pipe_fd[0]) != 0) {
        perror("posix_spawn_file_acitons_init");
        return;
    }

    if (posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], 1) != 0) {
        perror("posix_spawn_file_actions_addup2");
        return;
    }

    // executes the process
    if (posix_spawn(&pid, program, &actions, NULL, arguments, environment) != 0) {
        perror("spawn");
        exit(1);
    }

    // close the write end of the pipe
    close(pipe_fd[1]);
    if (output != NULL) {
        FILE *fp = fdopen(pipe_fd[0], "r");
        FILE *out = NULL;
        if (io == ISOUTPUT_1) {
            out = fopen(output, "w");
        }
        else if (io == ISOUTPUT_2) {
            out = fopen(output, "a");
        }
        
        int c;
        while ((c = fgetc(fp)) != EOF) {
            fprintf(out, "%c", c);
        }
        fclose(fp);
        fclose(out);
    }
    
    posix_spawn_file_actions_destroy(&actions);
}

// This function spawns a process and uses the input from 
// the read section of a pipe as the arguments for the process. 
// The data provided comes from an input file whos path is given
static void pipe_input(pid_t pid, char *program, char **environment, 
                       char *arguments[10], int pipe_fd[2], char **temp, 
                       char *input) {    
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // closes unused write end of pipe
    if (posix_spawn_file_actions_addclose(&actions, pipe_fd[1]) != 0) {
        perror("posix_spawn_file_acitons_init");
        return;
    }

    // replaces stdin of process with read of the pipe
    if (posix_spawn_file_actions_adddup2(&actions, pipe_fd[0], 0) != 0) {
        perror("posix_spawn_file_actions_addup2");
        return;
    }

    if (posix_spawn(&pid, program, &actions, NULL, arguments, environment) != 0) {
        perror("spawn");
        exit(1);
    }

    // close the read end of the pipe
    close(pipe_fd[0]);

    if (input != NULL) {
        FILE *fp = fdopen(pipe_fd[1], "w");
        if (fp == NULL) {
            perror("fdopen");
            return;
        }
        
        FILE *inp = fopen(input, "r");
        if (inp == NULL) {
            fprintf(stderr, "%s: file not found\n", input);
            // return;
        }

        if (inp != NULL) {
            int c;
            while ((c = fgetc(inp)) != EOF) {
                fprintf(fp, "%c", c);
            }
            fclose(inp);
        }
        fclose(fp);
    }
    
    
    posix_spawn_file_actions_destroy(&actions);

}

// This function provides true piping functionality for a process, which 
// which will take arguments from a pipe and output the results of the process
// into another pipe. If either the input or output files are given pathnames
// instead of 'NULL', it will attempt to read/write from the paths.
static void pipe_both(pid_t pid, char *program, char **environment, 
                      char *arguments[10], int pipe_fd1[2], int pipe_fd2[2], 
                      char **temp, int io, char *input, char *output) {
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // closes unused write end of pipe 1
    if (posix_spawn_file_actions_addclose(&actions, pipe_fd1[1]) != 0) {
        perror("posix_spawn_file_acitons_init");
        return;
    }

    // replaces stdin of process with read of the pipe
    if (posix_spawn_file_actions_adddup2(&actions, pipe_fd1[0], 0) != 0) {
        perror("posix_spawn_file_actions_addup2");
        return;
    }

    // closes unused read end of pipe2
    if (posix_spawn_file_actions_addclose(&actions, pipe_fd2[0]) != 0) {
        perror("posix_spawn_file_acitons_init");
        return;
    }

    // replaces stdout of process with write of the pipe2
    if (posix_spawn_file_actions_adddup2(&actions, pipe_fd2[1], 1) != 0) {
        perror("posix_spawn_file_actions_addup2");
        return;
    }

    if (posix_spawn(&pid, program, &actions, NULL, arguments, 
                    environment) != 0) {
        perror("spawn");
        exit(1);
    }
    
    // if an input path is provided, then it extacts data 
    // from the given input file
    if (input != NULL) {
        FILE *fp = fdopen(pipe_fd1[1], "w");
        FILE *in = fopen(input, "r");
        if (in == NULL) {
            fprintf(stderr, "input: file does not exist");
        }

        int c;
        while ((c = fgetc(in)) != EOF) {
            fprintf(fp, "%c", c);
        }
        fclose(in);
        fclose(fp);
    }

    close(pipe_fd1[0]);
    close(pipe_fd2[1]);
   
    // if an output path is provided, then it extacts data 
    // from the given input file
    if (output != NULL) {
        FILE *fp = fdopen(pipe_fd2[0], "r");
        FILE *out = NULL;
        if (io == IS_IO_2) {
            out = fopen(output, "a");
        }
        else {
            out = fopen(output, "w");
        }

        int c;
        while ((c = fgetc(fp)) != EOF) {
            fprintf(out, "%c", c);
        }
        
        fclose(fp);
        fclose(out);
    }
    
    posix_spawn_file_actions_destroy(&actions);

}


// Subset 5 - Pipes Functions
// This section is unfinished and thus unfortunately does not have the best
// structure/styling.

// This function checks how many pipes exist and returns it
static int check_pipes(char **words) {
    int counter = 0;
    for (int i = 0; words[i] != NULL; i++) {
        if (strcmp(words[i], "|") == 0) {
            counter++;
        }
    }
    return counter;
}

// This function extracts the arguments from the pipes and tries to execute
// their respective processes, redirecting their input/output as necessary
static void process_pipes(char **words, char **path, char **environment, 
                          char *program,  bool *res, int io, char *outputFile, 
                          char **temp) {
    // first create pipe
    int pipe_fd1[2];
    int pipe_fd2[2];
    
    if (pipe(pipe_fd1) == -1) {
        perror("pipe");
        return;
    }
    if (pipe(pipe_fd2) == -1) {
        perror("pipe");
        return;
    }
    
    char *argProg1[10] = {NULL};
    char *argProg2[10] = {NULL};

    int found = -1;
    int pipe = 1;
    int nPipes = 0;
    int wordSize = 0;
    bool first = true;
    
    for (wordSize = 0; temp[wordSize] != NULL; wordSize++) {
        if (strcmp(temp[wordSize], "|") == 0) {
            nPipes++;
        }
    }

    for (int i = 0; temp[i] != NULL; i++) {
        
        pid_t pid1 = 0;
        pid_t pid2 = 0; 
    
        // find the first pipe
        if (strcmp(temp[i], "|") == 0) {
            nPipes--;
            int j = found + 1;
            int k = 0;
            while (j != i && temp[j] != NULL && strcmp(temp[j], "|") != 0) {
                argProg1[k] = temp[j];
                k++;
                j++;
            }
        
            k = 0;
            j++;

            if (j >= wordSize) {
                argProg2[0] = NULL;
            }
            else {
                while (temp[j] != NULL && strcmp(temp[j], "|") != 0) {
                    argProg2[k] = temp[j]; 
                    k++;
                    j++;
                }
                found = j;
            }

            char *programA = NULL;
            char *programB = NULL;
            if (argProg1[0] != NULL) {
                programA = find_command(argProg1[0], path);
            }
            if (argProg2[0] != NULL) {
                programB = find_command(argProg2[0], path);
            }

            if (first == true) {    
                pipe_output(pid1, programA, environment, argProg1, 
                            pipe_fd1, NULL, io);
                first = false;
            }
           
            else {
                if (nPipes == 0) {
                    if (pipe == 2) {
                        pipe_input(pid1, programA, environment, 
                                   argProg1, pipe_fd2, temp, NULL);
                    }
                    else {
                        pipe_input(pid1, programA, environment, 
                                   argProg1, pipe_fd1, temp, NULL);
                    }
                    exit_status(pid1, programA);
                }
                
                else {
                    if (pipe == 2) {
                    pipe_both(pid1, programA, environment, 
                              argProg1, pipe_fd2, pipe_fd1, 
                              temp, io, NULL, NULL);
                }
                    else {
                        pipe_both(pid1, programA, environment, 
                                  argProg1, pipe_fd1, pipe_fd2, 
                                  temp, io, NULL, NULL);
                    }
                }
                             
            }
    
            
            if (programB == NULL) {
                return;
            }
            
            if (nPipes == 0) {
                if (pipe == 1) {   
                    pipe_input(pid2, programB, environment, argProg2, 
                               pipe_fd1, temp, NULL);
                }
            
                else {
                    pipe_input(pid2, programB, environment, argProg2, 
                               pipe_fd2, temp, NULL);
                }
                
            }

            else {
                if (pipe == 1) {   
                    pipe_both(pid2, programB, environment, argProg2, 
                              pipe_fd1, pipe_fd2, temp, io, NULL, NULL);
                }
            
                else {
                    pipe_both(pid2, programB, environment, argProg2, 
                              pipe_fd2, pipe_fd1, temp, io, NULL, NULL);
                }
            }

            exit_status(pid2, programB);
            
            
            for (int z = 0; z < 10; z++) {
                argProg1[z] = NULL;
                argProg2[z] = NULL;
            }
            
            if (pipe == 1) {
                pipe = 2;
            }
            else {
                pipe = 1;
            }
        }
    }
}
//
// Implement the `exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
//
static void do_exit(char **words) 
{
    assert(words != NULL);
    assert(strcmp(words[0], "exit") == 0);

    int exit_status = 0;

    if (words[1] != NULL && words[2] != NULL) {
        // { "exit", "word", "word", ... }
        fprintf(stderr, "exit: too many arguments\n");

    } else if (words[1] != NULL) {
        // { "exit", something, NULL }
        char *endptr;
        exit_status = (int) strtol(words[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "exit: %s: numeric argument required\n", words[1]);
        }
    }

    exit(exit_status);
}


//
// Check whether this process can execute a file.  This function will be
// useful while searching through the list of directories in the path to
// find an executable file.
//
static int is_executable(char *pathname)
{
    struct stat s;
    return
        // does the file exist?
        stat(pathname, &s) == 0 &&
        // is the file a regular file?
        S_ISREG(s.st_mode) &&
        // can we execute it?
        faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}


//
// Split a string 's' into pieces by any one of a set of separators.
//
// Returns an array of strings, with the last element being `NULL'.
// The array itself, and the strings, are allocated with `malloc(3)';
// the provided `free_token' function can deallocate this.
//
static char **tokenize(char *s, char *separators, char *special_chars)
{
    size_t n_tokens = 0;

    // Allocate space for tokens.  We don't know how many tokens there
    // are yet --- pessimistically assume that every single character
    // will turn into a token.  (We fix this later.)
    char **tokens = calloc((strlen(s) + 1), sizeof *tokens);
    assert(tokens != NULL);

    while (*s != '\0') {
        // We are pointing at zero or more of any of the separators.
        // Skip all leading instances of the separators.
        s += strspn(s, separators);

        // Trailing separators after the last token mean that, at this
        // point, we are looking at the end of the string, so:
        if (*s == '\0') {
            break;
        }

        // Now, `s' points at one or more characters we want to keep.
        // The number of non-separator characters is the token length.
        size_t length = strcspn(s, separators);
        size_t length_without_specials = strcspn(s, special_chars);
        if (length_without_specials == 0) {
            length_without_specials = 1;
        }
        if (length_without_specials < length) {
            length = length_without_specials;
        }

        // Allocate a copy of the token.
        char *token = strndup(s, length);
        assert(token != NULL);
        s += length;

        // Add this token.
        tokens[n_tokens] = token;
        n_tokens++;
    }

    // Add the final `NULL'.
    tokens[n_tokens] = NULL;

    // Finally, shrink our array back down to the correct size.
    tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);
    assert(tokens != NULL);

    return tokens;
}


//
// Free an array of strings as returned by `tokenize'.
//
static void free_tokens(char **tokens)
{
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

// helper functions
// This function gets the size of an array and returns it
static int get_size(char **array) {
    int size = 0;
    while (array[size] != NULL) {
        size++;
    }
    return size;
}

// This function is used to determine the number of words
// in the words in the array 
static int command_len(char **words) {
    int wordLen = 1;
    int i;
    for (i = 0; words[i] != NULL; i++) {
        wordLen += strlen(words[i]);
    }
    wordLen += i;
    return wordLen;
}



