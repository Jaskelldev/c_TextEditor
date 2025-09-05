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

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT = 1001,
  ARROW_UP = 1002,
  ARROW_DOWN = 1003,
  INSERT_KEY,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
  // ARROW_LEFT = 'h',
  // ARROW_RIGHT = 'l',
  // ARROW_UP = 'k',
  // ARROW_DOWN = 'j'
};

/*** DATA ***/
struct editorConfig {
  int cursorx, cursory;
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
int editorReadKey();
int windowSize(int *cols, int *rows);
int cursorPosition(int *rows, int *cols);
void abAppend(struct apbuf *ab, const char *s, int length);
void abFree(struct apbuf *ab);
void editorCursorMovement(int key);
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
  E.cursorx = 10;
  E.cursory = 10;

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

int editorReadKey() {
  int numread;
  char c;
  while ((numread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (numread == -1 && errno != EAGAIN) error("read");
  }
  if (c == '\x1b') {    
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '2': return INSERT_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      }else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    }else if (seq[0] == '0') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
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
void editorCursorMovement(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cursorx != 0) {
        E.cursorx--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cursorx != E.screencols - 1) {
        E.cursorx++;
      }
      break;
    case ARROW_DOWN:
      if (E.cursory != E.screenrows - 1) {
        E.cursory++;
      }
      break;
    case ARROW_UP:
      if (E.cursory != 0) {
        E.cursory--;
      }
      break;
  }
}

void editorProcessKeyPress() {
  int c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    
      case HOME_KEY:
      E.cursorx = 0;
      break;
    case END_KEY:
      E.cursorx = E.screencols - 1;
      break;
      
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--)
          editorCursorMovement(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_UP:
      editorCursorMovement(c);
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

  char buff[32];
  snprintf(buff, sizeof(buff), "\x1b[%d;%dH", E.cursory + 1, E.cursorx + 1);
  abAppend(&ab, buff, strlen(buff));

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