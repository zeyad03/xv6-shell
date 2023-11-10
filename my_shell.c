
#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define true 1

void read_input(char *input, int size) {
  // Write prompet, erase previoulsy sotred data
  // and read input.
  write(1, ">>> ", 4);
  memset(input, 0, size);
  read(0, input, size);
}

void set_args(char *input, char *args[], char *largs[], char *files[]) {
  // Word start indicator and pointers to get and store into.
  int ws = 1;
  char **arg, *b;
  arg = &args[0];
  b = input;

  // Store arguments to array.
  while (*b != '\n') {
    if (*b != ' ') {
      if (*b == '<') {
        b += 2;
        arg = &files[0];
      } else if (*b == '>') {
        b +=2;
        arg = &files[1];
      } else if (*b == '|') {
        b += 2;
        arg = &largs[0];
      }
      if (ws) {
        *arg = b;
        ws = 0;
      }
    } else {
      if (!ws) {
        *b = '\0';
        arg++;
        ws = 1;
      }
    }
    b++;
  }
  // Remove \n at the end.
  *b = '\0';
}

void cd(char *input, int space) {
  // Remove \n then call parent to process.
  input[strlen(input)-1] = 0;
  if (chdir(input+space+2) < 0)
    fprintf(2, "cannot cd %s\n", input+space+2);
}

void write_file(char *args[], int fd) {
  // Writing into output file.
  for (int i = 1; i < sizeof(*args); i++) {
    if (args[i] != 0)
      fprintf(fd, "%s ", args[i]);
  }
  fprintf(fd, "\n");
}

void redirect(char *args[], char *files[], int in, int out) {
  int f;
  if (in && out) {
    f = fork();
    if (f < 0) {
      write(2, "fork failed", 11);
      exit(1);
    } else if (f == 0) {
      int fd;
      fd = dup(0);
      close(0);
      close(1);
      open(files[0], O_RDONLY);
      open(files[1], O_WRONLY | O_CREATE);
      write_file(args, fd);
      close(fd);
      exec(args[0], args);
    }
    return;
  } else if (in) {
    f = fork();
    if (f < 0) {
      write(2, "fork failed", 11);
      exit(1);
    } else if (f == 0) {
      close(0);
      open(files[0], O_RDONLY);
      exec(args[0], args);
    } else {
      wait(0);
    }
  } else if (out) {
    f = fork();
    if (f < 0) {
      write(2, "fork failed", 11);
      exit(0);
    } else if (fork() == 0) {
      int fd;
      close(1);
      fd = open(files[1], O_WRONLY | O_CREATE);
      write_file(args, fd);
      close(fd);
      exit(0);
    } else {
      wait(0);
    }
  }
}

void execute(char *input, char *args[], char *files[], int space, int index) {
  // Execute thrown arguments.
  int f = fork();
  if (f < 0) {
    write(2, "fork failed", 11);
    exit(1);
  } else if (f == 0) {
    // Check for cd command.
    if (input[index] == 'c' && input[index+1] == 'd') {
      cd(input, space);
    }

    // Add file names to the end of args.
    if (files[0] != 0) {
      args[sizeof(*args)+1] = files[0];
    }
    if (files[1] != 0) {
      args[sizeof(*args)+1] = files[1];
    }

    // Execute entered command.
    exec(args[0], args);
  } else {
    wait(0);
  }
}

void piping(char *args[], char *largs[], char *files[], int pipes, int in, int out) {
  int p[2];
  pipe(p);
  // Execute command on the left.
  if(fork() == 0){
    close(1);
    dup(p[1]);
    close(p[0]);
    close(p[1]);
    if (out) {
      close(0);
      open(files[0], O_RDONLY);
    }
    exec(args[0], args);
  }
  // Execute command on the right.
  if(fork() == 0){
    int fd;
    close(0);
    fd = dup(p[0]);
    close(p[0]);
    close(p[1]);
    if (out) {
      close(1);
      open(files[1], O_WRONLY | O_CREATE);
      write_file(args, fd);
    }
    exec(largs[0], largs);
  }
  // Close pipe.
  close(p[0]);
  close(p[1]);
  // Wait for both childern to finish.
  wait(0);
  wait(0);
}

void main(void) {
  // Buffer to store input and array of arguments.
  char buf[512], *argv[MAXARG], *largv[MAXARG], *farg[2];

  // Run program nonstop.
  while(true) {
    // Redirecting in/out.
    int in = 0, out = 0, pipe = 0;

    // Erasing old data.
    memset(argv, 0, MAXARG);
    memset(largv, 0, MAXARG);
    memset(farg, 0, 2);

    // Get input from the user.
    read_input(buf, sizeof(buf));

    // Get index where first char exists.
    int s = 0, idx = 0;
    for (int i = 0; i < strlen(buf); i++) {
      if (buf[i] != ' ') {
        idx = i;
        break;
      }
    }
    // Count spaces after cd.
    for (int i = 2; i < strlen(buf); i++) {
      if (buf[i] == ' ') {
        s++;
      } else 
        break;
    }
    // Increase buffer size if exists spaces behind cd.
    if (idx > 0)
      s += idx + (3 - idx);

    // Specify process type.
    for (int i = 0; i < strlen(buf); i++) {
      if (buf[i] == '>') {
        out = 1;
      }
      if (buf[i] == '<' || (buf[i] == 'c' && buf[i+1] == 'a')) {
        in = 1;
      }
      if (buf[i] == '|') {
        pipe++;
      }
    }

    // Through input to get arguments.
    set_args(buf, argv, largv, farg);
    
    // Execute command.
    if (pipe > 0) {
      piping(argv, largv, farg, pipe, in, out);
    } else if (in || out) {
      redirect(argv, farg, in, out);
    } else
      execute(buf, argv, farg, s, idx);
  }

  exit(0);
}
