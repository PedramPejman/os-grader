#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "macros.h"

char log_file[MAX_FILENAME_LEN];
char err_file[MAX_FILENAME_LEN];
char out_file[MAX_FILENAME_LEN];
char diff_file[MAX_FILENAME_LEN];

/************************ Judge Routines *****************************/

/*
** Creates sandbox user/module_num
** reroutes the stdout and stderr of the process to log_file and err_file
*/
void init_sandbox(char *user, char *module_num, char *filename) {
  // Create SANDBOX directory if it doesn't exist and chdir into it
  if (access(SANDBOX, F_OK) == -1) system("mkdir "SANDBOX);
  chdir(SANDBOX);

  // Create user directory if does not exist already
  char command1[MAX_COMMAND_LEN];
  sprintf(command1, "mkdir %s", user);
  if (access(user, F_OK) == -1) system(command1);

  // Remove module directory
  char command2[MAX_COMMAND_LEN];
  sprintf(command2, "rm -rf %s/%s", user, module_num);
  system(command2);

  // Create module directory
  char command3[MAX_COMMAND_LEN];
  sprintf(command3, "mkdir %s/%s", user, module_num);
  system(command3);

  // Log stdout
  sprintf(log_file, "%s/%s/%s_%s_%s", user, module_num, user, module_num,
          LOGFILE_SUFFIX);
  freopen(log_file, "w", stdout);

  // Log stderr
  sprintf(err_file, "%s/%s/%s_%s_%s", user, module_num, user, module_num,
          ERRORFILE_SUFFIX);
  freopen(err_file, "w", stderr);

  // Copy submitted sourcefile
  char command4[MAX_COMMAND_LEN];
  sprintf(command4, "cp ../%s/%s %s/%s/%s", TEMP, filename, user, 
      module_num, MAINFILE_SUFFIX);
  system(command4);

  if (DEBUG) {
    printf("%sRedirecting stdout output%s\n", FILLER, FILLER);
    printf("$(%s)\n$(%s)\n$(%s)\n$(%s)", 
        command1, command2, command3, command4);
  }
}

/*
** Compiles C file (MAINFILE_SUFFIX) for user for module module_num
** Binary will be named user/module_num/bin
** Returns 0 if compiled with no errors otherwise positive number
*/
int compile_source(char *user, char *module_num) {
  if (DEBUG)
    printf("\n%sCompiling module %s [%s] for %s%s\n", FILLER, module_num,
           MAINFILE_SUFFIX, user, FILLER);

  // Compile source
  char command[MAX_COMMAND_LEN];
  memset(command, 0, MAX_COMMAND_LEN);
  sprintf(command, "gcc -w %s/%s/%s -o %s/%s/%s", user, module_num,
      MAINFILE_SUFFIX, user, module_num, BIN);
  if (DEBUG) printf("$(%s)\n", command);
  system(command);

  if (DEBUG)
    printf("%sFinished compiling module %s [%s] for %s%s\n\n", FILLER,
           module_num, MAINFILE_SUFFIX, user, FILLER);

  // Read error log file stats
  struct stat log_stat;
  stat(err_file, &log_stat);

  return log_stat.st_size;
}

/*
** Runs the program submissions/<user>/<module_num>/bin with input modules/<module_num>/input_file
*/
int run_program(char *user, char *module_num, char *input_file) {
  if (DEBUG)
    printf("\n%sRunning %s/%s/%s%s\n", FILLER, user, module_num, BIN, FILLER);

  // Redirect output to OUT
  sprintf(out_file, "%s/%s/%s_%s_%s_%s", user, module_num, user, module_num,
          OUTFILE_PREFIX, input_file);
  freopen(out_file, "w", stdout);

  // Run the binary
  char command[MAX_COMMAND_LEN];
  memset(&command, 0, MAX_COMMAND_LEN);
  if (input_file) {
    sprintf(&command, "./%s/%s/%s < ../%s/%s/%s", user, module_num, BIN, MODULES,
            module_num, input_file);
  } else {
    sprintf(&command, "./%s/%s/%s", user, module_num, BIN);
  }
  system(command);

  // Redirect output to logfile
  freopen(log_file, "a", stdout);

  if (DEBUG) printf("$(%s)\n", command);
  if (DEBUG)
    printf("%sFinished running %s/%s/%s%s\n\n", FILLER, user, module_num, BIN,
           FILLER);

  // Read error log file stats
  struct stat log_stat;
  stat(err_file, &log_stat);
  return log_stat.st_size;
}

/*
** Compares the following 2 files:
** submissions/<user>/<module_num>/<user>_<module_num>_out_<input_file>
** master/<module_num>/out_<input_file>
*/
int judge(char *user, char *module_num, char *input_file) {
  if (DEBUG)
    printf("\n%sJudging module %s with input %s for %s%s\n", FILLER,
           module_num, input_file, user, FILLER);

  // Set up file to send diff to
  sprintf(&diff_file, "%s/%s/%s_%s_%s", user, module_num, user, module_num,
          DIFF_SUFFIX);

  // Set up master file path
  char master_file[MAX_FILENAME_LEN];
  sprintf(&master_file, "../%s/%s/%s_%s", MODULES, module_num, OUTFILE_PREFIX,
          input_file);

  // Execute the diff and write to the diff file
  char command[MAX_COMMAND_LEN];
  sprintf(&command, "diff %s %s > %s", out_file, master_file, diff_file);
  if (DEBUG) printf("$(%s)\n", command);
  int return_code = system(command);

  if (DEBUG)
    printf("%sFinished judging module %s with input %s for %s%s\n\n",
           FILLER, module_num, input_file, user, FILLER);

  // Read error log file stats
  struct stat log_stat;
  stat(diff_file, &log_stat);

  return log_stat.st_size + return_code;
}

void clean_and_exit(int code) {
  if (DEBUG) printf("%sFinishing stdout redirect%s\n\n", FILLER, FILLER);

  // Reroute stdout and stderr to TTY
  fflush(stdout);
  fflush(stderr);
  freopen(TTY, "a", stderr);
  freopen(TTY, "a", stdout);

  // Exit with the supplied exit code
  exit(code);
}

/******************************* Helpers ***********************************/

/*
** Writes acknowledgement (<message
*size><delimiter><judge_id><delimiter><ack_code><delimiter>)
** onto the pipe.
*/
void send_ack(int pipe_fd, const char *ack_str, char *judge_id) {
  // Compute size of the second and third bits + delimiters
  int size = strlen(ack_str) + strlen(judge_id) + 3 /* delimiters */;

  // Add the length of the size itself
  int size_len = 1;
  if (size > 8) size_len = 2;
  size += size_len;

  // Allocate space for the message
  char *buf = (char *)malloc(size);

  // Concat the message length, judge_id and acknowledgement string
  sprintf(buf, "%d", size);
  strcpy(buf + size_len + 1, judge_id);
  strcpy(buf + size_len + strlen(judge_id) + 2, ack_str);

  // Change null terminators to delimiters
  buf[size_len] = *DELIM;
  buf[size_len + 1 + strlen(judge_id)] = *DELIM;
  buf[size - 1] = *DELIM;

  // Write the message to the pipe
  write(pipe_fd, buf, strlen(buf));

  // Deallocate message
  free(buf);
}

/******************************* Main ***********************************/

/*
** Captures and logs arguments, creates sandbox, compiles the source code and
** runs the code. It also sends ack messages on the shared pipe after 
** compilation, each run, each diff and the end of the process.
*/
int main(int argc, char **argv) {
  // Check arguments
  if (argc < 5) {
    printf(
        "USAGE: %s <judge id> <source file> <username> <module number> <pipe fd> [<input files>...]\n",
        argv[0]);
    exit(EXIT_FAILURE);
  }

  // Capture arguments
  int k = 1;
  char *judge_id = argv[k++];
  char *source_file = argv[k++];
  char *user = argv[k++];
  char *module_num = argv[k++];
  int pipe_fd = atoi(argv[k++]);
  int num_input_files = argc - k;
  char **input_files = &argv[k];

  // Create sandbox
  init_sandbox(user, module_num, source_file);

  // Log arguments
  int i;
  if (DEBUG) {
    printf("%sArgs: ", FILLER);
    for (i = 0; i < argc; i++) printf("<%s>", argv[i]);
    printf("%s\n", FILLER);
  }

  // Compile submitted source code
  int comp_result = compile_source(user, module_num);

  // Write compilation result code to pipe, exit if errored
  if (comp_result) {
    if (DEBUG) printf("%sCompilation failed - Exiting%s\n\n", FILLER, FILLER);
    send_ack(pipe_fd, CMP_ERR, judge_id);
    clean_and_exit(EXIT_FAILURE);
  } else {
    send_ack(pipe_fd, CMP_AOK, judge_id);
  }

  for (i = 0; i < num_input_files; i++) {
    // Run and exit if errored
    if (run_program(user, module_num, input_files[i])) {
      if (DEBUG) printf("%sRun #%d failed - Exiting%s\n\n", FILLER, i, FILLER);
      send_ack(pipe_fd, RUN_ERR, judge_id);
      clean_and_exit(EXIT_FAILURE);
    }

    send_ack(pipe_fd, RUN_AOK, judge_id);

    // Judge the output against master
    if (judge(user, module_num, input_files[i])) {
      if (DEBUG)
        printf("%sFailed test case #%d - Exiting%s\n\n", FILLER, i, FILLER);
      send_ack(pipe_fd, CHK_ERR, judge_id);
      clean_and_exit(EXIT_FAILURE);
    }

    send_ack(pipe_fd, CHK_AOK, judge_id);
  }

  // Send judge over ack
  send_ack(pipe_fd, JDG_AOK, judge_id);

  // Restore stdout and stderr and exit
  clean_and_exit(EXIT_SUCCESS);
}

