// Manmeet Singh (msingh11)
// CS111 - ASG 1 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

typedef struct Arg_info
{
  int* pipe_indices;
  int* scolon_indices;
  int pipe_num;
  int scolon_num;
  int last_index;
  int redir_index;
  char* default_dir;
} Arg_info;


extern char ** get_args();
void exit_error (char* msg);
void check_args(char** args, Arg_info* cmd);
int exec_command (char** args);
int change_dir (char* path, char* default_dir);
int simple_redir (char** args, int index);
int fork_and_run(char** cmd, int output_to, int input_from, bool include_stderr);
void handle_scolons(char** args, Arg_info* cmd);
int do_pipes(char** args, Arg_info* cmd);


int
main()
{
    char **     args;

	  Arg_info cmd = { .pipe_indices = (int*)malloc(sizeof(int)), .scolon_indices = (int*)malloc(sizeof(int)),
			   .pipe_num=0, .scolon_num=0, .last_index = -1, .redir_index=-1, .default_dir = malloc(sizeof(char)* 256)};

         cmd.default_dir = getcwd(cmd.default_dir, 256);
	  
    while (1) {
	
	    printf ("Command ('exit' to quit): ");
	    args = get_args();
	
      if (args[0] == NULL) {
	    printf ("No arguments on line!\n");
	    } else  if ( !strcmp (args[0], "exit")) {
	    printf ("Exiting...\n");
	    exit(0);
      }

      check_args(args, &cmd);
	

  // reset struct values
    cmd.pipe_indices = (int*)malloc(sizeof(int));
    cmd.scolon_indices = (int*)malloc(sizeof(int));
    cmd.scolon_num = 0;
    cmd.pipe_num = 0; 
    cmd.last_index = -1;
    cmd.redir_index=-1;
  }
    return EXIT_SUCCESS;
}

void check_args(char** args, Arg_info* cmd){
  int single_command = 1;
  int i;
  for (i = 0; args[i] != NULL; i++) {
    if (args[i+1] == NULL) cmd->last_index = i;
    if (!strcmp(args[i], "cd")) {
            change_dir(args[i+1], cmd->default_dir);
      single_command = 0;
            break;
    }

    if ( !strcmp(args[i], "<") || !strcmp(args[i], ">") || !strcmp(args[i], ">>") ||
      !strcmp(args[i], ">&") || !strcmp(args[i], ">>&" ) ){
      single_command = 0;
      cmd->redir_index = i;
    }

    if (!strcmp(args[i], "|") ||
                !strcmp(args[i], "|&")){
      cmd->pipe_indices[cmd->pipe_num] = i;
      cmd->pipe_num++;
      cmd->pipe_indices = (int*) realloc (cmd->pipe_indices, (cmd->pipe_num+1) *  sizeof(int));
            single_command = 0;
    }

    if (!strcmp(args[i], ";")){
      cmd->scolon_indices[cmd->scolon_num] = i;
      cmd->scolon_num++;
      cmd->scolon_indices = (int*) realloc (cmd->scolon_indices, (cmd->scolon_num+1) *  sizeof(int));
            single_command = 0;
    }

  } 

  if (single_command) exec_command(args);
  if (cmd->scolon_num > 0) {cmd->redir_index = -1; handle_scolons(args, cmd);}
  else if (cmd->pipe_num > 0) {printf("PIPE(s) Detected\n"); do_pipes(args, cmd); }
  // semi-colon and pipe handling before simple redir takes place. 
  else if (cmd->redir_index > 0 && cmd->last_index <= cmd->redir_index + 2) {simple_redir(args, cmd->redir_index);  cmd->redir_index = -1;}

}

int create_proc(int in, int out, char** cmd){
  pid_t pid;
printf("Create pipe Proc command: %s --- in: %i --- out: %i --\n", cmd[0], in, out); 
  if ((pid = fork ()) == 0){
      if (in != 0){
          dup2 (in, 0);
          close (in);
        }

      if (out != 1) {
          dup2 (out, 1);
          close (out);
        }
    execvp (cmd[0], cmd);
    }
  return pid;
}

int do_pipes(char** args, Arg_info* cmd){
  int num_pipes = cmd-> pipe_num;
  cmd-> pipe_num = -1;
 // int last_index = cmd->last_index;
  
  int i;
  int j=0;
  int in = 0;
  int fd[2];

  char *comm[64];
  char** cmd_counter = comm;
  bool incl_stderr = false;


  int num_cmds = num_pipes+1;
  for(i=0; i<num_cmds; i++){
    for(;j<cmd->pipe_indices[i]; j++){
      if (!strcmp(args[j], "|&")) incl_stderr = true;
      if (incl_stderr && !strcmp(args[j], "|")) exit_error("Combining of '|' and '|&' not supported");
         *cmd_counter++ = args[j];
    }
    *cmd_counter++ = NULL;
    j++;
    //printf("Command %s %s %s\n", comm[0], comm[1], comm[2] );
    pipe(fd);
    if (comm[0] != NULL) {create_proc(in, fd[1], comm);}
    close(fd[1]);
    in = fd[0];
    printf("OUTPUT FILE DESCRIPTOR: %i\n", in);
    memset(comm, 0, sizeof (comm));
    cmd_counter=comm;
  }
  // if (in != 0) dup2(in, 0);       COPYING THE ORIGINAL FILE DESCRIPTOR OVER STDIN RESULTS IN SEGFAULT

  for(;args[j] != NULL; j++){
    *cmd_counter++ = args[j];
    }
    *cmd_counter++ = NULL;
    // Take input from fd_in for one last time, run the command and output to stdout.
    memset(comm, 0, sizeof (comm));

  return 0;
}

void handle_scolons(char** args, Arg_info* cmd){
int num_scolons = cmd->scolon_num;
cmd->scolon_num = 0; // prevent running of this function on re-parsing
cmd->pipe_num = 0;
int last_index = cmd->last_index;

int i;
int j = 0;
char *comm[64];
char** cmd_counter = comm;

  for (i=0; i<num_scolons; i++){
    for (;j < cmd->scolon_indices[i]; j++){
      *cmd_counter++ = args[j];
    }
    *cmd_counter++ = NULL;

    check_args(comm, cmd);
    j++;
    memset(comm, 0, sizeof (comm));
    cmd_counter=comm;
  }

  for(;args[j] != NULL; j++){
    *cmd_counter++ = args[j];
    }
    *cmd_counter++ = NULL;
    check_args(comm, cmd);
    memset(comm, 0, sizeof (comm));
}

int fork_and_run(char** cmd, int output_to, int input_from, bool include_stderr){
  int fd;
  switch (fork()) {
    case 0: 
      if (include_stderr) dup2(output_to, STDERR_FILENO);
       fd = dup2(output_to, input_from); // replaces stdin file desc with File's so that all output now goes into file
       execvp(cmd[0], cmd);
        perror(cmd[0]);
      exit(1);

     default: 
       wait(NULL);
    break;

    case -1: 
      perror("fork");
  }
  close(output_to);
  return fd;
}


int simple_redir(char** args, int index){
  char *comm[64];
  char** cmd_counter = comm;

  int i;
  for(i=0; i< index; i++){
    *cmd_counter++ = args[i];
  }
  *cmd_counter++ = NULL;

  int File;
  int fd = -1;

  //fflush(stdout); 

  if (!strcmp(args[index], "<")){
    if ((File = open(args[index+1], O_RDONLY)) < 0) 
             exit_error("Could not open redirect file");
    fd = fork_and_run(comm, File, STDIN_FILENO, false);
  }
  if (!strcmp(args[index], ">")){
   if ((File = open(args[index+1], O_CREAT|O_TRUNC|O_WRONLY)) < 0) 
             exit_error("Could not open redirect file");
    fd = fork_and_run(comm, File, STDOUT_FILENO, false);
  }
  if (!strcmp(args[index], ">>")){
   if ((File = open(args[index+1], O_CREAT|O_APPEND|O_WRONLY)) < 0)
              exit_error("Could not open redirect file");
    fd = fork_and_run(comm, File, STDOUT_FILENO, false);
  }
 
 if (!strcmp(args[index], ">&")){
    if ((File = open(args[index+1], O_CREAT|O_TRUNC|O_WRONLY)) < 0)
              exit_error("Could not open redirect file");
    fd = fork_and_run(comm, File, STDOUT_FILENO, true);
  }
  if (!strcmp(args[index], ">>&")){
    if ((File = open(args[index+1], O_CREAT|O_APPEND|O_WRONLY)) < 0)
      exit_error("Could not open redirect file");
    fd = fork_and_run(comm, File, STDOUT_FILENO, true);
  }
  memset(comm, 0, sizeof comm);
  return fd;
}


int exec_command(char** args){
  pid_t pid = fork();
  
  if(pid == -1) {
    perror("fork");
    exit(1);
  }
  
  if (pid == 0){
    if (execvp(args[0], args) < 0){
      perror(args[0]);
      exit(EXIT_FAILURE); // exit the sub-process
      }
  }else { wait(NULL); }
  
  return 0;
}


int change_dir(char* path, char* default_dir){
   if (path == NULL){
	      if (chdir(default_dir) < 0)
	        exit_error ("EXIT ERR: could not cd to default directory\n");
	    }
	  else{
      	    if (chdir(path) < 0){
	      fprintf(stderr, "ERR: failed to locate such directory\n");
	      return -1;
	    }
	  }
   return 0;
}


void exit_error(char* msg){
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}
