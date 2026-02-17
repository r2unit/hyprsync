#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hyprsync {

class Tui {
public:
    Tui();
    ~Tui();

    std::string prompt(const std::string& question,
                       const std::string& default_val = "");

    bool confirm(const std::string& question, bool default_val = true);

    std::vector<size_t> checkbox(const std::string& label,
                                  const std::vector<std::string>& items,
                                  const std::vector<bool>& defaults);

    int select(const std::string& label,
               const std::vector<std::string>& options,
               int default_index = 0);

    void print_header(const std::string& text);
    void print_step(int step, const std::string& text);
    void print_success(const std::string& text);
    void print_error(const std::string& text);
    void print_info(const std::string& text);
    void print_line();
    void print_blank();

private:
    bool raw_mode_enabled_;

    void enable_raw_mode();
    void disable_raw_mode();
    int read_key();
    void clear_line();
    void move_cursor_up(int lines);
    void hide_cursor();
    void show_cursor();
};

}
