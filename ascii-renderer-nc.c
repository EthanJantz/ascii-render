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

typedef struct {
  int term_w, term_h;
  WINDOW *input_win, *ascii_win;

  Image img;
  ASCIIRender cur_render;
  UserInput user_input;

  char error[256];
  bool should_rerender;
  bool should_resize;
  bool should_quit;
} AppState;

// TUI mgmt
void setup_ncurses();
AppState init_state();
void update(AppState *state, int event);
void render(AppState *state);
void teardown(AppState *state);
void reinit_windows(AppState *state);

// Helpers
int read_img(char *filename, Image *img);
ASCIIRender generate_ascii_render(char *out, size_t out_size, int cols,
                                  int rows);
char get_char_from_lightness(float lightness);
ASCIIRender convert_to_ascii(Image *img, int block_width, int samples);
void update_input(UserInput *cur, int in);

// Constants
char ASCII[10] = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};
int N_ASCII = sizeof(ASCII) / sizeof(ASCII[0]);
int BLOCK_ASPECT_RATIO = 2;
int SAMPLES = 4;
int CHANNELS = 3;

void setup_ncurses() {
  srand(time(0));
  set_escdelay(25);
  initscr();
  curs_set(0);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  refresh();
}

AppState init_state() {
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
  ascii_h = sy * .9 - (margin * 2);
  ascii_x = margin;
  ascii_y = input_y + input_h;

  WINDOW *input_win = newwin(input_h, input_w, input_y, input_x);
  wborder(input_win, 0, 0, 0, 0, 0, 0, 0, 0);

  WINDOW *ascii_win = newwin(ascii_h, ascii_w, ascii_y, ascii_x);
  wborder(ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);

  ASCIIRender r = {0};
  UserInput in = {0};

  AppState state = {.term_w = sx,
                    .term_h = sy,
                    .input_win = input_win,
                    .ascii_win = ascii_win,
                    .cur_render = r,
                    .user_input = in,
                    .should_rerender = true,
                    .should_resize = false,
                    .should_quit = false};

  return state;
}

void update(AppState *state, int event) {
  switch (event) {
  case 27:
    state->should_quit = 1;
    return;
  case '\n':
  case '\r':
    state->should_rerender = 1;
    if (state->should_rerender && state->user_input.len > 0) {
      int err = read_img(state->user_input.buf, &state->img);
      if (err) {
        werase(state->ascii_win);
        wborder(state->ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);
        mvwprintw(state->ascii_win, 2, 1, "Could not load image\n");
        return;
      }

      int ascii_w, ascii_h;
      getmaxyx(state->ascii_win, ascii_h, ascii_w);
      int interior_h = ascii_h - 2;
      int interior_w = ascii_w - 2;
      int denom_h = BLOCK_ASPECT_RATIO * interior_h;
      // ceil idiom: ceil(x / y) == (x + y - 1) / y
      int bw_w = (state->img.width + interior_w - 1) / interior_w;
      int bw_h = (state->img.height + denom_h - 1) / denom_h;
      int bw = bw_h > bw_w ? bw_h : bw_w;
      if (bw == 0)
        bw = 1;

      if (state->cur_render.buf != 0)
        free(state->cur_render.buf);
      state->cur_render = convert_to_ascii(&state->img, bw, SAMPLES);
    }
    return;
  case KEY_RESIZE:
    state->should_resize = true;
    state->should_rerender = true;
    break;
  default:
    update_input(&state->user_input, event);
    state->should_rerender = 1;
    break;
  }

  return;
}

void render(AppState *state) {
  if (!state->should_rerender)
    return;

  if (state->should_resize) {
    reinit_windows(state);
    state->should_resize = false;
  }

  int ascii_w, ascii_h;
  getmaxyx(state->ascii_win, ascii_h, ascii_w);
  int interior_h = ascii_h - 2;
  int interior_w = ascii_w - 2;

  // center render if necessary
  int offset_h = 1;
  int offset_w = 1;
  if (state->cur_render.cols < interior_w) {
    offset_w = (interior_w - state->cur_render.cols) / 2;
    offset_w = offset_w == 0 ? 1 : offset_w;
  }

  if (state->cur_render.rows < interior_h) {
    offset_h = (interior_h - state->cur_render.rows) / 2;
    offset_h = offset_h == 0 ? 1 : offset_h;
  }

  mvwprintw(state->input_win, 1, 1, "%s", state->user_input.buf);
  mvwprintw(state->input_win, 1, 1, "%-*s", state->term_w - 4,
            state->user_input.buf);
  wrefresh(state->input_win);

  werase(state->ascii_win);
  wborder(state->ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);
  for (int i = 0; i < state->cur_render.rows; i++) {
    mvwaddnstr(state->ascii_win, offset_h + i, offset_w,
               &state->cur_render.buf[i * (state->cur_render.cols + 1)],
               state->cur_render.cols);
  }
  wrefresh(state->ascii_win);

  mvwprintw(state->input_win, 1, 1, "%s", state->user_input.buf);
  wrefresh(state->input_win);
  state->should_rerender = 0;
}

void reinit_windows(AppState *state) {
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
  ascii_h = sy * .9 - (margin * 2);
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

  state->term_w = sx;
  state->term_h = sy;
  state->ascii_win = ascii_win;
  state->input_win = input_win;
}

int read_img(char *filename, Image *img) {
  img->img =
      stbi_load(filename, &img->width, &img->height, &img->channels, CHANNELS);
  if (img->img == NULL) {
    return 1;
  }
  return 0;
}

void teardown(AppState *state) {
  free(state->cur_render.buf);
  stbi_image_free(state->img.img);
  endwin();
}

char get_char_from_lightness(float lightness) {
  int idx = floor(lightness * (N_ASCII - 1));
  return ASCII[idx];
}

ASCIIRender generate_ascii_render(char *out, size_t out_size, int cols,
                                  int rows) {
  ASCIIRender render = {
      .buf = out, .rows = rows, .cols = cols, .size = out_size};
  return render;
}

ASCIIRender convert_to_ascii(Image *img, int block_width, int samples) {
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
      for (int ch = 0; ch < CHANNELS; ch++) {
        avg_rel_lum = 0.0;

        for (int s = 1; s <= samples; s++) {
          int x = col * block_width + (s * block_width) / (s + 1);
          int y = row * block_height + (s * block_height) / (s + 1);

          if (ch == 0) {
            r = img->img[(y * img->width + x) * 3 + ch];
          } else if (ch == 1) {
            g = img->img[(y * img->width + x) * 3 + ch];
          } else {
            b = img->img[(y * img->width + x) * 3 + ch];
          }
          rel_lum = ((r * 0.2126) + (g * 0.7152) + (b * 0.0722)) / 255;
          avg_rel_lum += rel_lum;
        }
      }
      avg_rel_lum /= samples;
      out[row * (cols + 1) + col] = get_char_from_lightness(avg_rel_lum);

      if (col == cols - 1) {
        out[row * (cols + 1) + cols] = '\n';
      }
    }
  }

  ASCIIRender render = generate_ascii_render(out, out_size, cols, rows);
  return render;
}

void update_input(UserInput *cur, int in) {
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
  setup_ncurses();
  AppState state = init_state();
  while (!state.should_quit) {
    render(&state);
    int key = getch();
    update(&state, key);
  }
  teardown(&state);
}
