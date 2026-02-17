#include "tui.hpp"

#include <iostream>
#include <termios.h>
#include <unistd.h>

namespace hyprsync {

namespace {
    constexpr int KEY_UP = 1000;
    constexpr int KEY_DOWN = 1001;
    constexpr int KEY_ENTER = 10;
    constexpr int KEY_SPACE = 32;
    constexpr int KEY_ESCAPE = 27;
    constexpr int KEY_Q = 'q';

    struct termios original_termios;
}

Tui::Tui() : raw_mode_enabled_(false) {
}

Tui::~Tui() {
    if (raw_mode_enabled_) {
        disable_raw_mode();
    }
}

void Tui::enable_raw_mode() {
    if (raw_mode_enabled_) return;

    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios raw = original_termios;

    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_enabled_ = true;
}

void Tui::disable_raw_mode() {
    if (!raw_mode_enabled_) return;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    raw_mode_enabled_ = false;
}

int Tui::read_key() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        return -1;
    }

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

    return c;
}

void Tui::clear_line() {
    std::cout << "\r\033[K";
}

void Tui::move_cursor_up(int lines) {
    if (lines > 0) {
        std::cout << "\033[" << lines << "A";
    }
}

void Tui::hide_cursor() {
    std::cout << "\033[?25l";
}

void Tui::show_cursor() {
    std::cout << "\033[?25h";
}

std::string Tui::prompt(const std::string& question, const std::string& default_val) {
    std::cout << "  " << question;
    if (!default_val.empty()) {
        std::cout << " [" << default_val << "]";
    }
    std::cout << ": ";
    std::cout.flush();

    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        return default_val;
    }
    return input;
}

bool Tui::confirm(const std::string& question, bool default_val) {
    std::string hint = default_val ? "[Y/n]" : "[y/N]";
    std::cout << "  " << question << " " << hint << ": ";
    std::cout.flush();

    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        return default_val;
    }

    char c = std::tolower(input[0]);
    return c == 'y';
}

std::vector<size_t> Tui::checkbox(const std::string& label,
                                   const std::vector<std::string>& items,
                                   const std::vector<bool>& defaults) {
    if (items.empty()) {
        return {};
    }

    std::vector<bool> selected = defaults;
    if (selected.size() < items.size()) {
        selected.resize(items.size(), false);
    }

    size_t cursor = 0;

    enable_raw_mode();
    hide_cursor();

    auto render = [&]() {
        for (size_t i = 0; i < items.size(); ++i) {
            clear_line();
            std::cout << "  ";
            if (i == cursor) {
                std::cout << "> ";
            } else {
                std::cout << "  ";
            }
            std::cout << "[" << (selected[i] ? "x" : " ") << "] ";
            std::cout << items[i] << "\n";
        }
        std::cout << "\n  (arrows: move, space: toggle, enter: confirm)\n";
    };

    std::cout << "\n  " << label << "\n\n";
    render();

    bool done = false;
    while (!done) {
        int key = read_key();

        switch (key) {
            case KEY_UP:
                if (cursor > 0) cursor--;
                break;
            case KEY_DOWN:
                if (cursor < items.size() - 1) cursor++;
                break;
            case KEY_SPACE:
                selected[cursor] = !selected[cursor];
                break;
            case KEY_ENTER:
                done = true;
                break;
            case KEY_Q:
            case KEY_ESCAPE:
                done = true;
                break;
        }

        if (!done) {
            move_cursor_up(items.size() + 2);
            render();
        }
    }

    show_cursor();
    disable_raw_mode();

    std::vector<size_t> result;
    for (size_t i = 0; i < selected.size(); ++i) {
        if (selected[i]) {
            result.push_back(i);
        }
    }

    return result;
}

int Tui::select(const std::string& label,
                const std::vector<std::string>& options,
                int default_index) {
    if (options.empty()) {
        return -1;
    }

    size_t cursor = static_cast<size_t>(default_index);
    if (cursor >= options.size()) {
        cursor = 0;
    }

    enable_raw_mode();
    hide_cursor();

    auto render = [&]() {
        for (size_t i = 0; i < options.size(); ++i) {
            clear_line();
            std::cout << "  ";
            if (i == cursor) {
                std::cout << "> " << options[i] << " <";
            } else {
                std::cout << "  " << options[i];
            }
            std::cout << "\n";
        }
        std::cout << "\n  (arrows: move, enter: select)\n";
    };

    std::cout << "\n  " << label << "\n\n";
    render();

    bool done = false;
    while (!done) {
        int key = read_key();

        switch (key) {
            case KEY_UP:
                if (cursor > 0) cursor--;
                break;
            case KEY_DOWN:
                if (cursor < options.size() - 1) cursor++;
                break;
            case KEY_ENTER:
                done = true;
                break;
            case KEY_Q:
            case KEY_ESCAPE:
                show_cursor();
                disable_raw_mode();
                return default_index;
        }

        if (!done) {
            move_cursor_up(options.size() + 2);
            render();
        }
    }

    show_cursor();
    disable_raw_mode();

    return static_cast<int>(cursor);
}

void Tui::print_header(const std::string& text) {
    std::cout << "\n ── " << text << " ";
    size_t padding = 60 - text.length() - 5;
    for (size_t i = 0; i < padding; ++i) {
        std::cout << "─";
    }
    std::cout << "\n\n";
}

void Tui::print_step(int step, const std::string& text) {
    std::cout << " Step " << step << ": " << text << "\n";
}

void Tui::print_success(const std::string& text) {
    std::cout << "  [OK] " << text << "\n";
}

void Tui::print_error(const std::string& text) {
    std::cout << "  [ERROR] " << text << "\n";
}

void Tui::print_info(const std::string& text) {
    std::cout << "  " << text << "\n";
}

void Tui::print_line() {
    std::cout << " ";
    for (int i = 0; i < 60; ++i) {
        std::cout << "─";
    }
    std::cout << "\n";
}

void Tui::print_blank() {
    std::cout << "\n";
}

}
