#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
  char *buf;
  int rows;
  int cols;
  size_t size;
} ASCIIRender;

typedef struct {
  unsigned char *img;
  int width;
  int height;
  int channels;
} Image;

typedef struct {
  char buf[256];
  int len;
} UserInput;

int readImg(char *filename, Image *img);
ASCIIRender generateASCIIRender(char *out, size_t out_size, int cols, int rows);
char getCharFromLightness(float lightness);
ASCIIRender convertToASCII(Image *img, int block_width, int samples);
void updateInput(UserInput *cur, int in);

char ASCII[10] = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};
int N_ASCII = sizeof(ASCII) / sizeof(ASCII[0]);
int BLOCK_ASPECT_RATIO = 2;
int SAMPLES = 1;

int readImg(char *filename, Image *img) {
  img->img = stbi_load(filename, &img->width, &img->height, &img->channels, 3);
  if (img->img == NULL) {
    printf("Could not load image\n");
    return 1;
  }
  return 0;
}

char getCharFromLightness(float lightness) {
  int idx = floor(lightness * (N_ASCII - 1));
  return ASCII[idx];
}

ASCIIRender generateASCIIRender(char *out, size_t out_size, int cols,
                                int rows) {
  assert(out != NULL);
  assert(rows > 0);
  assert(cols > 0);
  assert(out_size == rows * cols + rows);
  ASCIIRender render = {
      .buf = out, .rows = rows, .cols = cols, .size = out_size};
  return render;
}

ASCIIRender convertToASCII(Image *img, int block_width, int samples) {
  unsigned char r, g, b;
  float rel_lum;
  float avg_rel_lum;
  int block_height =
      block_width *
      BLOCK_ASPECT_RATIO; // monospaced term chars are 2:1 height/width
  int cols = img->width / block_width;
  int rows = img->height / block_height;

  size_t out_size = cols * rows + rows;
  char *out = malloc(out_size);

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      for (int ch = 0; ch < img->channels; ch++) {
        avg_rel_lum = 0.0;

        for (int s = 1; s <= samples; s++) {
          int x = col * block_width + block_width / s;
          int y = row * block_height + block_height / s;

          if (ch == 0) {
            r = img->img[(y * img->width + x) * 3 + ch];
          } else if (ch == 1) {
            g = img->img[(y * img->width + x) * 3 + ch];
          } else {
            b = img->img[(y * img->width + x) * 3 + ch];
            rel_lum = ((r * 0.2126) + (g * 0.7152) + (b * 0.0722)) / 255;
            avg_rel_lum += rel_lum;
          }
        }
      }
      avg_rel_lum /= samples;
      out[row * (cols + 1) + col] = getCharFromLightness(avg_rel_lum);

      if (col == cols - 1) {
        out[row * (cols + 1) + cols] = '\n';
      }
    }
  }

  ASCIIRender render = generateASCIIRender(out, out_size, cols, rows);
  return render;
}

void updateInput(UserInput *cur, int in) {
  if (in == KEY_BACKSPACE || in == 127 || in == 8) {
    if (cur->len == 0)
      return;
    --cur->len;
    cur->buf[cur->len] = '\0';
    return;
  }

  if (!isprint(in) || cur->len >= sizeof(cur->buf) - 1)
    return;

  cur->buf[cur->len] = in;
  ++cur->len;
  cur->buf[cur->len] = '\0';
  return;
}

int main() {
  srand(time(0));
  set_escdelay(25);
  initscr();
  curs_set(0);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  refresh();

  // term size
  int sx, sy;
  getmaxyx(stdscr, sy, sx);

  // init windows
  int input_w, input_h, input_x, input_y, ascii_w, ascii_h, ascii_x, ascii_y,
      margin;
  margin = 1; // 1 char
  input_w = sx - (margin * 2);
  input_h = 3;
  input_x = margin;
  input_y = margin;

  ascii_w = sx - (margin * 2);
  ascii_h = sy * .8 - (margin * 2);
  ascii_x = margin;
  ascii_y = input_y + input_h;

  WINDOW *input_win = newwin(input_h, input_w, input_y, input_x);
  wborder(input_win, 0, 0, 0, 0, 0, 0, 0, 0);
  mvwprintw(input_win, 1, 1, "pos: %dx%d size: %dwx%dh", input_x, input_y,
            input_w, input_h);

  WINDOW *ascii_win = newwin(ascii_h, ascii_w, ascii_y, ascii_x);
  wborder(ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);
  mvwprintw(ascii_win, 1, 1, "pos: %dx%d size: %dwx%dh", ascii_x, ascii_y,
            ascii_w, ascii_h);

  // init state
  UserInput input = {"", 0};

  // read image
  Image img;
  readImg("circle.png", &img);

  // get render that fits within ascii_win
  ASCIIRender render = {.buf = NULL, 0};
  int interior_h = ascii_h - 2;
  int interior_w = ascii_w - 2;
  int denom_h = BLOCK_ASPECT_RATIO * interior_h;
  // ceil idiom: ceil(x / y) == (x + y - 1) / y
  int bw_w = (img.width + interior_w - 1) / interior_w;
  int bw_h = (img.height + denom_h - 1) / denom_h;
  int bw = bw_h > bw_w ? bw_h : bw_w;
  if (bw == 0)
    bw = 1;
  render = convertToASCII(&img, bw, SAMPLES);

  // center render if necessary
  int offset_h = 0;
  int offset_w = 0;
  if (render.cols < interior_w)
    offset_w = (interior_w - render.cols) / 2;
  if (render.rows < interior_h)
    offset_h = (interior_h - render.rows) / 2;

  // loop until user presses q
  int ch = getch();
  do {
    getmaxyx(stdscr, sy, sx);
    wrefresh(input_win);
    for (int i = 0; i < render.rows; i++) {
      mvwaddnstr(ascii_win, offset_h + i, offset_w,
                 &render.buf[i * (render.cols + 1)], render.cols);
    }
    mvwprintw(ascii_win, 1, 1, "pos: %dx%d size: %dwx%dh", ascii_x, ascii_y,
              ascii_w, ascii_h);
    wrefresh(ascii_win);

    if (ch == 27)
      break;
    updateInput(&input, ch);
    mvwprintw(input_win, 1, 1, "%s", input.buf);
    wrefresh(input_win);
    ch = getch();
  } while (true);

  free(render.buf);
  stbi_image_free(img.img);
  endwin();
}
