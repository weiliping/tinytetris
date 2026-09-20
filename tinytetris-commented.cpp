#include <ctime>
#include <curses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// block layout is: {w-1,h-1}{x0,y0}{x1,y1}{x2,y2}{x3,y3} (two bits each)
int x = 431424, y = 598356, r = 427089, px = 247872, py = 799248, pr,
    c = 348480, p = 615696, tick, board[30][10],  // 【改】高度30
    block[7][4] = {{x, y, x, y},
                   {r, p, r, p},
                   {c, c, c, c},
                   {599636, 431376, 598336, 432192},
                   {411985, 610832, 415808, 595540},
                   {px, py, px, py},
                   {614928, 399424, 615744, 428369}},
    score = 0;

#define ESC_KEY 27
#define CELL_CHAR_WIDTH 3  // 1格=3字符(1.5倍放大)

int pause_game(int start_y, int start_x);

// extract a 2-bit number from a block entry
int NUM(int x, int y) { return 3 & block[p][x] >> y; }

// create a new piece, don't remove old one (it has landed and should stick)
void new_piece() {
  y = py = 0;
  p = rand() % 7;
  r = pr = rand() % 4;
  x = px = rand() % (10 - NUM(r, 16));
}

void draw_border(int start_y, int start_x) {
  int h = 30;           //【改】游戏区高度30
  int w_chars = 10 * CELL_CHAR_WIDTH;

  // 左上角
  mvaddch(start_y - 1, start_x - 1, ACS_ULCORNER);
  // 右上角
  mvaddch(start_y - 1, start_x + w_chars, ACS_URCORNER);
  // 左下角
  mvaddch(start_y + h, start_x - 1, ACS_LLCORNER);
  // 右下角
  mvaddch(start_y + h, start_x + w_chars, ACS_LRCORNER);

  // 上边框
  for (int bx = start_x; bx < start_x + w_chars; bx++) {
    mvaddch(start_y - 1, bx, ACS_HLINE);
  }
  // 下边框
  for (int bx = start_x; bx < start_x + w_chars; bx++) {
    mvaddch(start_y + h, bx, ACS_HLINE);
  }
  // 左边框
  for (int by = start_y; by < start_y + h; by++) {
    mvaddch(by, start_x - 1, ACS_VLINE);
  }
  // 右边框
  for (int by = start_y; by < start_y + h; by++) {
    mvaddch(by, start_x + w_chars, ACS_VLINE);
  }
}

// draw the board and score
void frame(int start_y, int start_x) {
  draw_border(start_y, start_x);
  for (int i = 0; i < 30; i++) {  //【改】循环30行
    move(start_y + i, start_x);
    for (int j = 0; j < 10; j++) {
      board[i][j] && attron(262176 | board[i][j] << 8);
      printw("   ");
      attroff(262176 | board[i][j] << 8);
    }
  }
  move(start_y + 31, start_x); //【改】score下移到30行棋盘下方
  printw("Score: %d", score);
  refresh();
}

// set the value of the board for a particular (x,y,r) piece
void set_piece(int x, int y, int r, int v) {
  for (int i = 0; i < 8; i += 2) {
    board[NUM(r, i * 2) + y][NUM(r, (i * 2) + 2) + x] = v;
  }
}

// move a piece from old (p*) coords to new
void update_piece() {
  set_piece(px, py, pr, 0);
  set_piece(px = x, py = y, pr = r, p + 1);
}

// remove line(s) from the board if they're full
void remove_line() {
  for (int row = y; row <= y + NUM(r, 18); row++) {
    c = 1;
    for (int i = 0; i < 10; i++) {
      c *= board[row][i];
    }
    if (!c) {
      continue;
    }
    for (int i = row - 1; i > 0; i--) {
      memcpy(&board[i + 1][0], &board[i][0], 40);
    }
    memset(&board[0][0], 0, 10);
    score++;
  }
}

// check if placing p at (x,y,r) will be a collision
int check_hit(int x, int y, int r) {
  if (y + NUM(r, 18) > 29) {  //【改】上限29
    return 1;
  }
  set_piece(px, py, pr, 0);
  c = 0;
  for (int i = 0; i < 8; i += 2) {
    board[y + NUM(r, i * 2)][x + NUM(r, (i * 2) + 2)] && c++;
  }
  set_piece(px, py, pr, p + 1);
  return c;
}

// slowly tick the piece y position down so the piece falls
int do_tick() {
  if (++tick > 30) {
    tick = 0;
    if (check_hit(x, y + 1, r)) {
      if (!y) {
        return 0;
      }
      remove_line();
      new_piece();
    } else {
      y++;
      update_piece();
    }
  }
  return 1;
}

// main game loop with wasd input checking
void runloop(int start_y, int start_x) {
  while (do_tick()) {
    usleep(10000);
    c = getch();

    if (c == ERR) {
      update_piece();
      frame(start_y, start_x);
      continue;
    }

    if (c == ESC_KEY) {
      usleep(25000);
      if (getch() == ERR) {
        if (!pause_game(start_y, start_x)) {
          return;
        }
      } else {
        while (getch() != ERR) {
          ;
        }
      }
      update_piece();
      frame(start_y, start_x);
      continue;
    }

    if (c == 'a' && x > 0 && !check_hit(x - 1, y, r)) {
      x--;
    }
    if (c == 'd' && x + NUM(r, 16) < 9 && !check_hit(x + 1, y, r)) {
      x++;
    }
    if (c == 's') {
      while (!check_hit(x, y + 1, r)) {
        y++;
        update_piece();
      }
      remove_line();
      new_piece();
    }
    if (c == 'j') {
      ++r %= 4;
      while (x + NUM(r, 16) > 9) {
        x--;
      }
      if (check_hit(x, y, r)) {
        x = px;
        r = pr;
      }
    }
    if (c == 'q') {
      return;
    }
    update_piece();
    frame(start_y, start_x);
  }
}

int pause_game(int start_y, int start_x) {
  move(start_y + 2, start_x);
  attron(A_BOLD);
  printw("  PAUSED   ");
  attroff(A_BOLD);
  move(start_y + 3, start_x);
  printw("[ESC/SPACE] resume  [Q] quit");
  refresh();

  for (;;) {
    usleep(10000);
    int k = getch();
    if (k == ' ' || k == ESC_KEY) {
      return 1;
    }
    if (k == 'q' || k == 'Q') {
      return 0;
    }
  }
}

int main() {
  srand(time(0));
  initscr();
  start_color();
  keypad(stdscr, TRUE);
  for (int i = 1; i < 8; i++) {
    init_pair(i, i, 0);
  }
  new_piece();

  noecho();
  timeout(0);
  curs_set(0);

  int game_h = 30; //【改】游戏高度30
  int game_w_chars = 10 * CELL_CHAR_WIDTH;
  int start_y = (LINES - game_h) / 2;
  int start_x = (COLS - game_w_chars) / 2;

  runloop(start_y, start_x);
  endwin();
}