#include <assert.h>
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

int readImg(char *filename, Image *img);
ASCIIRender generateASCIIRender(char *out, size_t out_size, int cols, int rows);
char getCharFromLightness(float lightness);
ASCIIRender convertToASCII(Image *img, int samples);

char ASCII[10] = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};
int N_ASCII = sizeof(ASCII) / sizeof(ASCII[0]);
int BLOCK_WIDTH = 8;
int BLOCK_HEIGHT = 16;
int SAMPLES = 8;

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

ASCIIRender convertToASCII(Image *img, int samples) {
  unsigned char r, g, b;
  float rel_lum;
  float avg_rel_lum;
  int cols = img->width / BLOCK_WIDTH;
  int rows = img->height / BLOCK_HEIGHT;

  size_t out_size = cols * rows + rows;
  char *out = malloc(out_size);

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      for (int ch = 0; ch < img->channels; ch++) {
        avg_rel_lum = 0.0;

        for (int s = 1; s <= samples; s++) {
          int x = col * BLOCK_WIDTH + BLOCK_WIDTH / s;
          int y = row * BLOCK_HEIGHT + BLOCK_HEIGHT / s;

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

  // generate ascii render from image
  Image img;
  readImg("circle.png", &img);
  ASCIIRender render = convertToASCII(&img, SAMPLES);

  // loop until user presses q
  char ch;
  do {
    getmaxyx(stdscr, sy, sx);
    wrefresh(input_win);
    for (int i = 0; i < render.rows; i++) {
      mvwaddnstr(ascii_win, i + 1, 1, &render.buf[i * (render.cols + 1)],
                 render.cols);
    }
    wrefresh(ascii_win);

    ch = getch();
  } while (ch != 'q');

  free(render.buf);
  stbi_image_free(img.img);
  endwin();
}
