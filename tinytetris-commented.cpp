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

// Global variables to track the upcoming piece
int next_p, next_r;

#define ESC_KEY 27
#define CELL_CHAR_WIDTH 3  // 1格=3字符(1.5倍放大)

int pause_game(int start_y, int start_x);

// extract a 2-bit number from a specified block type entry
int GET_BIT(int block_type, int rotation, int bit_shift) { 
  return 3 & block[block_type][rotation] >> bit_shift; 
}

// Keep legacy shorthand active without breaking name structures
#define NUM(rotation, bit_shift) GET_BIT(p, rotation, bit_shift)

// create a new piece, pulling from the previewed "next" item
void new_piece() {
  y = py = 0;
  
  // Current piece takes the values of the previous "next_piece"
  p = next_p;
  r = pr = next_r;
  x = px = rand() % (10 - NUM(r, 16));

  // Generate the next piece ahead of time for the preview window
  next_p = rand() % 7;
  next_r = rand() % 4;
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

// Render the preview UI window to the right side of the main grid
void draw_preview(int start_y, int start_x) {
  int preview_x = start_x + (10 * CELL_CHAR_WIDTH) + 4; // Shifted right past main border
  
  // Title text
  move(start_y, preview_x);
  printw("NEXT:");

  // Draw small 4x4 preview subgrid box border
  mvaddch(start_y + 1, preview_x - 1, ACS_ULCORNER);
  mvaddch(start_y + 1, preview_x + 12, ACS_URCORNER);
  mvaddch(start_y + 6, preview_x - 1, ACS_LLCORNER);
  mvaddch(start_y + 6, preview_x + 12, ACS_LRCORNER);
  for(int i = 0; i < 12; i++) {
    mvaddch(start_y + 1, preview_x + i, ACS_HLINE);
    mvaddch(start_y + 6, preview_x + i, ACS_HLINE);
  }
  for(int i = 0; i < 4; i++) {
    mvaddch(start_y + 2 + i, preview_x - 1, ACS_VLINE);
    mvaddch(start_y + 2 + i, preview_x + 12, ACS_VLINE);
  }

  // Clear inner preview box contents 
  for (int i = 0; i < 4; i++) {
    move(start_y + 2 + i, preview_x);
    printw("            ");
  }

  // Draw the actual next tetris block into the preview slot
  for (int i = 0; i < 8; i += 2) {
    int block_y = GET_BIT(next_p, next_r, i * 2);
    int block_x = GET_BIT(next_p, next_r, (i * 2) + 2);
    
    move(start_y + 2 + block_y, preview_x + (block_x * CELL_CHAR_WIDTH));
    attron(262176 | (next_p + 1) << 8);
    printw("   ");
    attroff(262176 | (next_p + 1) << 8);
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
  
  // Render our new preview panel side-by-side
  draw_preview(start_y, start_x);
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
  
  // Seed the initial preview queue before picking the very first block
  next_p = rand() % 7;
  next_r = rand() % 4;
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
