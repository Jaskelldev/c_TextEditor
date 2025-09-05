/*** INCLUDES ****/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define C_EDITOR_VERSION "0.0.1"

/*** DATA ***/
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};
struct editorConfig E;
/*** BUFFER TO APPEND ***/
struct apbuf {
  char *buffer;
  int length;
};

#define APBUF_INIT {NULL, 0}

/*** FUNCTIONS PROTOTYPES ***/
void initEditor();
void error(const char *s);
void enableRawMode();
char editorReadKey();
int windowSize(int *cols, int *rows);
int cursorPosition(int *rows, int *cols);
void abAppend(struct apbuf *ab, const char *s, int length);
void editorProcessKeyPress();
void editorNewScreen();
void editorDrawRows(struct apbuf *ab);

/*** INIT ***/
int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorNewScreen();
    editorProcessKeyPress();
  }

  return 0;
}

void initEditor() {
  if (windowSize(&E.screencols, &E.screenrows) == -1) error("windowSize");
}

/*** TERMINAL ***/
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    error("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) error("tcgetattr");
  atexit(disableRawMode);
  
  struct termios raw = E.orig_termios;
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

char editorReadKey() {
  int numread;
  char c;
  while ((numread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (numread == -1 && errno != EAGAIN) error("read");
  }
  return c;
}

int windowSize(int *cols, int *rows) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return cursorPosition(rows, cols);
  }else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

int cursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

/*** FUNCTIONS TO APPEND AND FREE THE BUFFER ***/
void abAppend(struct apbuf *ab, const char *s, int length) {
  char *new = realloc(ab->buffer, ab->length + length);
  if (new == NULL) return;
  memcpy(&new[ab->length], s, length);
  ab->buffer = new;
  ab->length += length;
}

void abFree(struct apbuf *ab) {
  free(ab->buffer);
}

/*** INPUT ***/
void editorProcessKeyPress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** OUTPUT ***/
void editorNewScreen(){
  struct apbuf ab = APBUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  // abAppend(&ab, "\x1b[2J", 4);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.buffer, ab.length);
  abFree(&ab);
}

void editorDrawRows(struct apbuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
      "C Editor -- version %s", C_EDITOR_VERSION);
    if (welcomelen > E.screencols) welcomelen = E.screencols;
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }
    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}