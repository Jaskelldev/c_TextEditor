/*** INCLUDES ****/
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** DATA ***/
struct termios orig_termios;

/*** TERMINAL FUNCTIONS PROTOTYPES ***/
void error(const char *s);
void enableRawMode();

/*** INIT ***/
int main() {
  enableRawMode();

  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) error("read");
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    }else {
      printf("%d ('%c')\r\n",c,c);
    }
    if (c == 'q') {
      break;
    }
  }

  return 0;
}

/*** TERMINAL ***/
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    error("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) error("tcgetattr");
  atexit(disableRawMode);
  
  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag &= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)error("tcsetattr");
}

void error(const char *s) {
  perror(s);
  exit(1);
}