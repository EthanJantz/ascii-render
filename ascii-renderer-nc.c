#include <math.h>
#include <ncurses.h>
#include <panel.h>
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

ASCIIRender generateASCIIRender(char *out, size_t out_size, int cols, int rows);
char getCharFromLightness(float lightness);
ASCIIRender convertToASCII(unsigned char *img, int width, int height,
                           int channels, int samples);

char ASCII[10] = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};
int N_ASCII = sizeof(ASCII) / sizeof(ASCII[0]);
int BLOCK_WIDTH = 8;
int BLOCK_HEIGHT = 16;
int SAMPLES = 8;

char getCharFromLightness(float lightness) {
  int idx = floor(lightness * (N_ASCII - 1));
  return ASCII[idx];
}

ASCIIRender generateASCIIRender(char *out, size_t out_size, int cols,
                                int rows) {
  // TODO: Validate buf non-null, rows/cols > 0, size matches dimensions
  ASCIIRender render = {
      .buf = out, .rows = rows, .cols = cols, .size = out_size};
  return render;
}

ASCIIRender convertToASCII(unsigned char *img, int width, int height,
                           int channels, int samples) {
  unsigned char r, g, b;
  float rel_lum;
  float avg_rel_lum;
  int cols = width / BLOCK_WIDTH;
  int rows = height / BLOCK_HEIGHT;

  size_t out_size = cols * rows + rows;
  char *out = malloc(out_size);

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      for (int ch = 0; ch < channels; ch++) {
        avg_rel_lum = 0.0;

        for (int s = 1; s <= samples; s++) {
          int x = col * BLOCK_WIDTH + BLOCK_WIDTH / s;
          int y = row * BLOCK_HEIGHT + BLOCK_HEIGHT / s;

          if (ch == 0) {
            r = img[(y * width + x) * 3 + ch];
          } else if (ch == 1) {
            g = img[(y * width + x) * 3 + ch];
          } else {
            b = img[(y * width + x) * 3 + ch];
            rel_lum = ((r * 0.2126) + (g * 0.7152) + (b * 0.0722)) / 255;
            avg_rel_lum += rel_lum;
          }
        }
      }
      avg_rel_lum /= samples;
      out[row * (cols + 1) + col] = getCharFromLightness(avg_rel_lum);
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

  // refresh, should display the window within the border window
  wrefresh(ascii_win);
  wrefresh(input_win);

  getch();

  // int width, height, channels;
  // unsigned char *img = stbi_load(argv[1], &width, &height, &channels, 3);
  // if (img == NULL) {
  //   printf("Could not load image\n");
  //   return 1;
  // }
  //
  // // printf("Loaded: %dx%d, %d channels\n", width, height, channels);
  //
  // ASCIIRender render = convertToASCII(img, width, height, channels, SAMPLES);
  //
  // FILE *fo = fopen(argv[2], "wb");
  // if (fo == NULL) {
  //   free(render.buf);
  //   return 1;
  // }
  //
  // size_t bytes_written = fwrite(render.buf, 1, render.size, fo);
  // printf("Wrote %zu bytes\n", bytes_written);
  // free(render.buf);
  // stbi_image_free(img);

  endwin();
}
