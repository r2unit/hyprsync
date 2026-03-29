#ifndef HS_TUI_H
#define HS_TUI_H

#include "vec.h"
#include <stddef.h>

typedef struct {
    int raw_mode_enabled;
} hs_tui;

hs_tui *hs_tui_create(void);
void hs_tui_free(hs_tui *t);

char *hs_tui_prompt(hs_tui *t, const char *question, const char *default_val);
int hs_tui_confirm(hs_tui *t, const char *question, int default_val);
hs_sizevec hs_tui_checkbox(hs_tui *t, const char *label,
                            const hs_strvec *items, const int *defaults);
int hs_tui_select(hs_tui *t, const char *label,
                  const hs_strvec *options, int default_index);

void hs_tui_print_header(const char *text);
void hs_tui_print_step(int step, const char *text);
void hs_tui_print_success(const char *text);
void hs_tui_print_error(const char *text);
void hs_tui_print_info(const char *text);
void hs_tui_print_line(void);
void hs_tui_print_blank(void);

#endif
