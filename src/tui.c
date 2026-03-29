#define _POSIX_C_SOURCE 200809L
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>

#define KEY_UP     1000
#define KEY_DOWN   1001
#define KEY_ENTER  10
#define KEY_SPACE  32
#define KEY_ESCAPE 27
#define KEY_Q      'q'

static struct termios original_termios;

static void enable_raw_mode(hs_tui *t) {
    if (t->raw_mode_enabled) return;

    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios raw = original_termios;

    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    t->raw_mode_enabled = 1;
}

static void disable_raw_mode(hs_tui *t) {
    if (!t->raw_mode_enabled) return;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    t->raw_mode_enabled = 0;
}

static int read_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
        return -1;

    if (c == KEY_ESCAPE) {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESCAPE;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESCAPE;

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
            }
        }
        return KEY_ESCAPE;
    }

    return (unsigned char)c;
}

static void clear_line(void) {
    printf("\r\033[K");
}

static void move_cursor_up(int lines) {
    if (lines > 0)
        printf("\033[%dA", lines);
}

static void hide_cursor(void) {
    printf("\033[?25l");
}

static void show_cursor(void) {
    printf("\033[?25h");
}

hs_tui *hs_tui_create(void) {
    hs_tui *t = malloc(sizeof(hs_tui));
    if (!t) return NULL;
    t->raw_mode_enabled = 0;
    return t;
}

void hs_tui_free(hs_tui *t) {
    if (!t) return;
    if (t->raw_mode_enabled)
        disable_raw_mode(t);
    free(t);
}

char *hs_tui_prompt(hs_tui *t, const char *question, const char *default_val) {
    (void)t;
    printf("  %s", question);
    if (default_val && default_val[0] != '\0')
        printf(" [%s]", default_val);
    printf(": ");
    fflush(stdout);

    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);

    if (n <= 0) {
        free(line);
        return default_val ? strdup(default_val) : strdup("");
    }

    if (n > 0 && line[n - 1] == '\n')
        line[n - 1] = '\0';

    if (line[0] == '\0') {
        free(line);
        return default_val ? strdup(default_val) : strdup("");
    }

    return line;
}

int hs_tui_confirm(hs_tui *t, const char *question, int default_val) {
    (void)t;
    const char *hint = default_val ? "[Y/n]" : "[y/N]";
    printf("  %s %s: ", question, hint);
    fflush(stdout);

    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);

    if (n <= 0 || line[0] == '\n') {
        free(line);
        return default_val;
    }

    if (n > 0 && line[n - 1] == '\n')
        line[n - 1] = '\0';

    if (line[0] == '\0') {
        free(line);
        return default_val;
    }

    int result = (tolower((unsigned char)line[0]) == 'y');
    free(line);
    return result;
}

hs_sizevec hs_tui_checkbox(hs_tui *t, const char *label,
                            const hs_strvec *items, const int *defaults) {
    hs_sizevec result;
    hs_vec_init(&result);

    if (items->len == 0)
        return result;

    int *selected = calloc(items->len, sizeof(int));
    if (!selected) return result;

    if (defaults) {
        for (size_t i = 0; i < items->len; i++)
            selected[i] = defaults[i];
    }

    size_t cursor = 0;

    enable_raw_mode(t);
    hide_cursor();

    printf("\n  %s\n\n", label);

    // <lorenzo> render checkbox lijst
    for (size_t i = 0; i < items->len; i++) {
        clear_line();
        printf("  %s[%c] %s\n",
               i == cursor ? "> " : "  ",
               selected[i] ? 'x' : ' ',
               items->data[i]);
    }
    printf("\n  (arrows: move, space: toggle, enter: confirm)\n");
    fflush(stdout);

    int done = 0;
    while (!done) {
        int key = read_key();

        switch (key) {
            case KEY_UP:
                if (cursor > 0) cursor--;
                break;
            case KEY_DOWN:
                if (cursor < items->len - 1) cursor++;
                break;
            case KEY_SPACE:
                selected[cursor] = !selected[cursor];
                break;
            case KEY_ENTER:
                done = 1;
                break;
            case KEY_Q:
            case KEY_ESCAPE:
                done = 1;
                break;
        }

        if (!done) {
            move_cursor_up((int)items->len + 2);
            for (size_t i = 0; i < items->len; i++) {
                clear_line();
                printf("  %s[%c] %s\n",
                       i == cursor ? "> " : "  ",
                       selected[i] ? 'x' : ' ',
                       items->data[i]);
            }
            printf("\n  (arrows: move, space: toggle, enter: confirm)\n");
            fflush(stdout);
        }
    }

    show_cursor();
    disable_raw_mode(t);

    for (size_t i = 0; i < items->len; i++) {
        if (selected[i])
            hs_vec_push(&result, i);
    }

    free(selected);
    return result;
}

int hs_tui_select(hs_tui *t, const char *label,
                  const hs_strvec *options, int default_index) {
    if (options->len == 0)
        return -1;

    size_t cursor = (size_t)default_index;
    if (cursor >= options->len)
        cursor = 0;

    enable_raw_mode(t);
    hide_cursor();

    printf("\n  %s\n\n", label);

    for (size_t i = 0; i < options->len; i++) {
        clear_line();
        if (i == cursor)
            printf("  > %s <\n", options->data[i]);
        else
            printf("    %s\n", options->data[i]);
    }
    printf("\n  (arrows: move, enter: select)\n");
    fflush(stdout);

    int done = 0;
    while (!done) {
        int key = read_key();

        switch (key) {
            case KEY_UP:
                if (cursor > 0) cursor--;
                break;
            case KEY_DOWN:
                if (cursor < options->len - 1) cursor++;
                break;
            case KEY_ENTER:
                done = 1;
                break;
            case KEY_Q:
            case KEY_ESCAPE:
                show_cursor();
                disable_raw_mode(t);
                return default_index;
        }

        if (!done) {
            move_cursor_up((int)options->len + 2);
            for (size_t i = 0; i < options->len; i++) {
                clear_line();
                if (i == cursor)
                    printf("  > %s <\n", options->data[i]);
                else
                    printf("    %s\n", options->data[i]);
            }
            printf("\n  (arrows: move, enter: select)\n");
            fflush(stdout);
        }
    }

    show_cursor();
    disable_raw_mode(t);

    return (int)cursor;
}

void hs_tui_print_header(const char *text) {
    size_t text_len = strlen(text);
    size_t padding = (text_len + 5 < 60) ? 60 - text_len - 5 : 0;

    printf("\n ── %s ", text);
    for (size_t i = 0; i < padding; i++)
        printf("─");
    printf("\n\n");
}

void hs_tui_print_step(int step, const char *text) {
    printf(" Step %d: %s\n", step, text);
}

void hs_tui_print_success(const char *text) {
    printf("  [OK] %s\n", text);
}

void hs_tui_print_error(const char *text) {
    printf("  [ERROR] %s\n", text);
}

void hs_tui_print_info(const char *text) {
    printf("  %s\n", text);
}

void hs_tui_print_line(void) {
    printf(" ");
    for (int i = 0; i < 60; i++)
        printf("─");
    printf("\n");
}

void hs_tui_print_blank(void) {
    printf("\n");
}
