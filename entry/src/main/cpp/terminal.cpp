// for standalone build to test on Linux:
// g++ terminal.cpp -I/usr/include/freetype2 -DSTANDALONE -lfreetype -lGLESv2 -lglfw -o terminal

#include "terminal.h"
#include <GLES3/gl32.h>
#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <deque>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <pty.h>
#include <set>
#include <stdint.h>
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <array>
#include <pthread.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef STANDALONE
// https://stackoverflow.com/questions/5343190/how-do-i-replace-all-instances-of-a-string-with-another-string
std::string ReplaceString(std::string subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

void CustomPrintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::string new_fmt = fmt;
    new_fmt = ReplaceString(new_fmt, "%{public}", "%");
    new_fmt += "\n";
    vfprintf(stderr, new_fmt.c_str(), args);
}
#define OH_LOG_ERROR(tag, fmt, ...) CustomPrintf(fmt, __VA_ARGS__)
#define OH_LOG_INFO(tag, fmt, ...) CustomPrintf(fmt, __VA_ARGS__)
#define OH_LOG_WARN(tag, fmt, ...) CustomPrintf(fmt, __VA_ARGS__)
#else
#include "hilog/log.h"
#undef LOG_TAG
#define LOG_TAG "testTag"
#endif

// docs for escape codes:
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
// https://vt100.net/docs/vt220-rm/chapter4.html
// https://espterm.github.io/docs/VT100%20escape%20codes.html
// https://ecma-international.org/wp-content/uploads/ECMA-48_5th_edition_june_1991.pdf
// https://xtermjs.org/docs/api/vtfeatures/
// terminology:
// C: a single character
// Ps: a single and optional numeric parameter
// Pm: list of Ps, separated by ;
// Pt: text parameter of printable characters

enum term_colors {
    brblack,
    black,
    brgreen,
    bryellow,
    brblue,
    brcyan,
    white,
    brwhite,
    yellow,
    brred,
    red,
    magenta,
    brmagenta,
    blue,
    cyan,
    green,
    max_term_color
};

// solarized light
static std::array<int, 3> predefined_colors[max_term_color] = {
    [brblack] = { 0, 43, 54 },
    [black] = { 7, 54, 66 },
    [brgreen] = { 88, 110, 117 },
    [bryellow] = { 101, 123, 131 },
    [brblue] = { 131, 148, 150 },
    [brcyan] = { 147, 161, 161 },
    [white] = { 238, 232, 213 },
    [brwhite] = { 253, 246, 227 },
    [yellow] = { 181, 137, 0 },
    [brred] = { 203, 75, 22 },
    [red] = { 220, 50, 47 },
    [magenta] = { 221, 54, 130 },
    [brmagenta] = { 108, 113, 196 },
    [blue] = { 38, 139, 210 },
    [cyan] = { 42, 161, 152 },
    [green] = { 13, 153, 0 },
};

term_style::term_style() {
    fg_red = predefined_colors[black][0] / 255.0;
    fg_green = predefined_colors[black][1] / 255.0;
    fg_blue = predefined_colors[black][2] / 255.0;
    bg_red = predefined_colors[white][0] / 255.0;
    bg_green = predefined_colors[white][1] / 255.0;
    bg_blue = predefined_colors[white][2] / 255.0;
}

uint32_t color_map_256[] = {
    0x000000,
    0x800000,
    0x008000,
    0x808000,
    0x000080,
    0x800080,
    0x008080,
    0xc0c0c0,
    0x808080,
    0xff0000,
    0x00ff00, 
    0xffff00, 
    0x0000ff,
    0xff00ff,
    0x00ffff,
    0xffffff,
    0x000000,
    0x00005f,
    0x000087,
    0x0000af,
    0x0000d7,
    0x0000ff,
    0x005f00,
    0x005f5f,
    0x005f87,
    0x005faf,
    0x005fd7,
    0x005fff,
    0x008700,
    0x00875f,
    0x008787,
    0x0087af,
    0x0087d7,
    0x0087ff,
    0x00af00,
    0x00af5f,
    0x00af87,
    0x00afaf,
    0x00afd7,
    0x00afff,
    0x00d700,
    0x00d75f,
    0x00d787,
    0x00d7af,
    0x00d7d7,
    0x00d7ff,
    0x00ff00,
    0x00ff5f,
    0x00ff87,
    0x00ffaf,
    0x00ffd7,
    0x00ffff,
    0x5f0000,
    0x5f005f,
    0x5f0087,
    0x5f00af,
    0x5f00d7,
    0x5f00ff,
    0x5f5f00,
    0x5f5f5f,
    0x5f5f87,
    0x5f5faf,
    0x5f5fd7,
    0x5f5fff,
    0x5f8700,
    0x5f875f,
    0x5f8787,
    0x5f87af,
    0x5f87d7,
    0x5f87ff,
    0x5faf00,
    0x5faf5f,
    0x5faf87,
    0x5fafaf,
    0x5fafd7,
    0x5fafff,
    0x5fd700,
    0x5fd75f,
    0x5fd787,
    0x5fd7af,
    0x5fd7d7,
    0x5fd7ff,
    0x5fff00,
    0x5fff5f,
    0x5fff87,
    0x5fffaf,
    0x5fffd7,
    0x5fffff,
    0x870000,
    0x87005f,
    0x870087,
    0x8700af,
    0x8700d7,
    0x8700ff,
    0x875f00,
    0x875f5f,
    0x875f87,
    0x875faf,
    0x875fd7,
    0x875fff,
    0x878700, 
    0x87875f, 
    0x878787, 
    0x8787af, 
    0x8787d7, 
    0x8787ff,
    0x87af00, 
    0x87af5f, 
    0x87af87, 
    0x87afaf, 
    0x87afd7, 
    0x87afff,
    0x87d700, 
    0x87d75f, 
    0x87d787, 
    0x87d7af, 
    0x87d7d7, 
    0x87d7ff,
    0x87ff00, 
    0x87ff5f, 
    0x87ff87, 
    0x87ffaf, 
    0x87ffd7, 
    0x87ffff,
    0xaf0000, 
    0xaf005f, 
    0xaf0087, 
    0xaf00af, 
    0xaf00d7, 
    0xaf00ff,
    0xaf5f00, 
    0xaf5f5f, 
    0xaf5f87, 
    0xaf5faf, 
    0xaf5fd7, 
    0xaf5fff,
    0xaf8700, 
    0xaf875f, 
    0xaf8787, 
    0xaf87af, 
    0xaf87d7, 
    0xaf87ff,
    0xafaf00, 
    0xafaf5f, 
    0xafaf87, 
    0xafafaf, 
    0xafafd7, 
    0xafafff,
    0xafd700, 
    0xafd75f, 
    0xafd787, 
    0xafd7af, 
    0xafd7d7, 
    0xafd7ff,
    0xafff00, 
    0xafff5f, 
    0xafff87, 
    0xafffaf, 
    0xafffd7, 
    0xafffff,
    0xd70000, 
    0xd7005f, 
    0xd70087, 
    0xd700af, 
    0xd700d7, 
    0xd700ff,
    0xd75f00, 
    0xd75f5f, 
    0xd75f87, 
    0xd75faf, 
    0xd75fd7, 
    0xd75fff,
    0xd78700, 
    0xd7875f, 
    0xd78787, 
    0xd787af, 
    0xd787d7, 
    0xd787ff,
    0xd7af00, 
    0xd7af5f, 
    0xd7af87, 
    0xd7afaf, 
    0xd7afd7, 
    0xd7afff,
    0xd7d700, 
    0xd7d75f, 
    0xd7d787, 
    0xd7d7af, 
    0xd7d7d7, 
    0xd7d7ff,
    0xd7ff00, 
    0xd7ff5f, 
    0xd7ff87, 
    0xd7ffaf, 
    0xd7ffd7, 
    0xd7ffff,
    0xff0000, 
    0xff005f, 
    0xff0087, 
    0xff00af, 
    0xff00d7, 
    0xff00ff,
    0xff5f00, 
    0xff5f5f, 
    0xff5f87, 
    0xff5faf, 
    0xff5fd7, 
    0xff5fff,
    0xff8700, 
    0xff875f, 
    0xff8787, 
    0xff87af, 
    0xff87d7, 
    0xff87ff,
    0xffaf00, 
    0xffaf5f, 
    0xffaf87, 
    0xffafaf, 
    0xffafd7, 
    0xffafff,
    0xffd700, 
    0xffd75f, 
    0xffd787, 
    0xffd7af, 
    0xffd7d7, 
    0xffd7ff,
    0xffff00, 
    0xffff5f, 
    0xffff87, 
    0xffffaf, 
    0xffffd7, 
    0xffffff,
    0x080808, 
    0x121212, 
    0x1c1c1c, 
    0x262626, 
    0x303030, 
    0x3a3a3a,
    0x444444, 
    0x4e4e4e, 
    0x585858, 
    0x606060, 
    0x666666, 
    0x767676,
    0x808080, 
    0x8a8a8a, 
    0x949494, 
    0x9e9e9e, 
    0xa8a8a8, 
    0xb2b2b2,
    0xbcbcbc, 
    0xc6c6c6, 
    0xd0d0d0, 
    0xdadada, 
    0xe4e4e4, 
    0xeeeeee
};

static std::vector<std::string> SplitString(const std::string &str, const std::string &delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    result.push_back(str.substr(start));
    return result;
}

// viewport width/height
static int width = 0;
static int height = 0;
static GLint surface_location = -1;
static GLint render_pass_location = -1;
#ifdef STANDALONE
static int font_height = 24;
static int font_width = 12;
static int max_font_width = 24;
static int baseline_height = 5;
#else
static int font_height = 48;
static int font_width = 24;
static int max_font_width = 48;
static int baseline_height = 10;
#endif

// scroll offset in y axis
static float scroll_offset = 0;

const int MAX_HISTORY_LINES = 5000;

constexpr uint32_t TrueColorFrom(uint8_t index) {
    return color_map_256[index];
}

void terminal_context::ResizeTo(int new_term_row, int new_term_col) {
    int old_term_col = term_col;
    term_row = new_term_row;
    term_col = new_term_col;

    // update scroll margin
    scroll_top = 0;
    scroll_bottom = term_row - 1;

    terminal.resize(term_row);
    for (int i = 0; i < term_row; i++) {
        terminal[i].resize(term_col);
    }

    if (row > term_row - 1) {
        row = term_row - 1;
    }

    if (col > term_col - 1) {
        col = term_col - 1;
    }

    tab_stops.resize(term_col);
    for (int i = old_term_col;i < term_col;i += tab_size) {
        tab_stops[i] = true;
    }

    struct winsize ws = {};
    ws.ws_col = term_col;
    ws.ws_row = term_row;
    ioctl(fd, TIOCSWINSZ, &ws);
}

void terminal_context::DropFirstRowIfOverflow() {
    if (row == scroll_bottom + 1) {
        // drop first row in scrolling margin
        assert(scroll_top < scroll_bottom);
        history.push_back(terminal[scroll_top]);
        terminal.erase(terminal.begin() + scroll_top);
        terminal.insert(terminal.begin() + scroll_bottom, std::vector<term_char>());
        terminal[scroll_bottom].resize(term_col);
        row--;

        while (history.size() > MAX_HISTORY_LINES) {
            history.pop_front();
        }
    } else if (row >= term_row) {
        row = term_row - 1;
    }
}

void terminal_context::InsertUtf8(uint32_t codepoint) {
    assert(row >= 0 && row < term_row);
    assert(col >= 0 && col <= term_col);
    if (col == term_col) {
        // special handling if cursor is on the right edge
        if (autowrap) {
            // wrap to next line
            col = 0;
            row++;
            DropFirstRowIfOverflow();

            terminal[row][col].ch = codepoint;
            terminal[row][col].style = current_style;
            col++;
        } else {
            // override last column
            terminal[row][term_col - 1].ch = codepoint;
            terminal[row][term_col - 1].style = current_style;
        }
    } else {
        terminal[row][col].ch = codepoint;
        terminal[row][col].style = current_style;
        col++;
    }
}

// clamp cursor to valid range
void terminal_context::ClampCursor() {
    // clamp col
    if (col < 0) {
        col = 0;
    } else if (col > term_col - 1) {
        col = term_col - 1;
    }

    // clamp row
    if (origin_mode) {
        // limit cursor to scroll region
        if (row < scroll_top) {
            row = scroll_top;
        } else if (row > scroll_bottom) {
            row = scroll_bottom;
        }
    } else {
        // limit cursor to terminal
        if (row < 0) {
            row = 0;
        } else if (row > term_row - 1) {
            row = term_row - 1;
        }
    }
}

// set absolute cursor location
void terminal_context::SetCursor(int new_row, int new_col) {
    if (origin_mode) {
        // origin mode, home position is the scrolling top
        row = new_row + scroll_top;
        col = new_col;
    } else {
        row = new_row;
        col = new_col;
    }
    ClampCursor();
}

// move cursor in relative position
void terminal_context::MoveCursor(int row_diff, int col_diff) {
    ClampCursor();
    SetCursor(row + row_diff, col + col_diff);
}

// write data to pty until fully sent
void terminal_context::WriteFull(uint8_t *data, size_t length) {
    if (fd == -1) {
        return;
    }

    // pretty print
    std::string hex;
    for (int i = 0; i < length; i++) {
        if (data[i] >= 127 || data[i] < 32) {
            char temp[8];
            snprintf(temp, sizeof(temp), "\\x%02x", data[i]);
            hex += temp;
        } else {
            hex += (char)data[i];
        }
    }
    OH_LOG_INFO(LOG_APP, "Send: %{public}s", hex.c_str());

    int written = 0;
    while (written < length) {
        int size = write(fd, (uint8_t *)data + written, length - written);
        assert(size >= 0);
        written += size;
    }
}

// CAUTION: clobbers temp
#define read_int_or_default(def)                                                                                       \
    (temp = 0, (escape_buffer != "" ? sscanf(escape_buffer.c_str(), "%d", &temp) : temp = (def)), temp)

// handle CSI escape sequences
void terminal_context::HandleCSI(uint8_t current) {
    int temp = 0;
    if (current >= 0x40 && current <= 0x7E) {
        // final byte in [0x40, 0x7E]
        if (current == 'A') {
            // CSI Ps A, CUU, move cursor up # lines
            int line = read_int_or_default(1);
            if (row >= scroll_top) {
                // do not move past scrolling margin
                MoveCursor(-std::min(line, row - scroll_top), 0);
            } else {
                // we are out of scrolling region, move nevertheless
                MoveCursor(-line, 0);
            }
        } else if (current == 'B') {
            // CSI Ps B, CUD, move cursor down # lines
            int line = read_int_or_default(1);
            if (row <= scroll_bottom) {
                // do not move past scrolling margin
                MoveCursor(std::min(line, scroll_bottom - row), 0);
            } else {
                // we are out of scrolling region, move nevertheless
                MoveCursor(line,  0);
            }
        } else if (current == 'C') {
            // CSI Ps C, CUF, move cursor right # columns
            col += std::max(read_int_or_default(1), 1);
            ClampCursor();
        } else if (current == 'D') {
            // CSI Ps D, CUB, move cursor left # columns
            col -= std::max(read_int_or_default(1), 1);
            ClampCursor();
        } else if (current == 'E') {
            // CSI Ps E, CNL, move cursor to the beginning of next line, down # lines
            row += read_int_or_default(1);
            col = 0;
            ClampCursor();
        } else if (current == 'F') {
            // CSI Ps F, CPL, move cursor to the beginning of previous line, up # lines
            row -= read_int_or_default(1);
            col = 0;
            ClampCursor();
        } else if (current == 'G') {
            // CSI Ps G, CHA, move cursor to column #
            col = read_int_or_default(1);
            // convert from 1-based to 0-based
            col--;
            ClampCursor();
        } else if (current == 'H') {
            std::vector<std::string> parts = SplitString(escape_buffer, ";");
            if (parts.size() == 2) {
                // CSI Ps ; PS H, CUP, move cursor to x, y
                sscanf(parts[0].c_str(), "%d", &row);
                sscanf(parts[1].c_str(), "%d", &col);
                // convert from 1-based to 0-based
                row--;
                col--;
                ClampCursor();
            } else if (parts.size() == 1 && escape_buffer != "") {
                // CSI Ps H, CUP, move cursor to x, y, col is 0
                sscanf(parts[0].c_str(), "%d", &row);
                col = 0;
                // convert from 1-based to 0-based
                row--;
                ClampCursor();
            } else if (escape_buffer == "") {
                // CSI H, HOME, move cursor upper left corner
                row = col = 0;
            } else {
                goto unknown;
            }
        } else if (current == 'J') {
            // CSI Ps J, ED, erase in display
            if (escape_buffer == "" || escape_buffer == "0") {
                // CSI J, CSI 0 J
                // erase below
                for (int i = col; i < term_col; i++) {
                    terminal[row][i] = term_char();
                }
                for (int i = row + 1; i < term_row; i++) {
                    std::fill(terminal[i].begin(), terminal[i].end(), term_char());
                }
            } else if (escape_buffer == "1") {
                // CSI 1 J
                // erase above
                for (int i = 0; i < row; i++) {
                    std::fill(terminal[i].begin(), terminal[i].end(), term_char());
                }
                for (int i = 0; i <= col; i++) {
                    terminal[row][i] = term_char();
                }
            } else if (escape_buffer == "2") {
                // CSI 2 J
                // erase all
                for (int i = 0; i < term_row; i++) {
                    std::fill(terminal[i].begin(), terminal[i].end(), term_char());
                }
            } else {
                goto unknown;
            }
        } else if (current == 'K') {
            // CSI Ps K, EL, erase in line
            if (escape_buffer == "" || escape_buffer == "0") {
                // CSI K, CSI 0 K
                // erase to right
                for (int i = col; i < term_col; i++) {
                    terminal[row][i] = term_char();
                }
            } else if (escape_buffer == "1") {
                // CSI 1 K
                // erase to left
                for (int i = 0; i <= col && i < term_col; i++) {
                    terminal[row][i] = term_char();
                }
            } else if (escape_buffer == "2") {
                // CSI 2 K
                // erase whole line
                for (int i = 0; i < term_col; i++) {
                    terminal[row][i] = term_char();
                }
            } else {
                goto unknown;
            }
        } else if (current == 'L') {
            // CSI Ps L, Insert Ps blank lines at active row
            int line = read_int_or_default(1);
            if (row < scroll_top || row > scroll_bottom) {
                // outside the scroll margins, do nothing
            } else {
                // insert lines from current row, add new rows from scroll bottom
                for (int i = scroll_bottom;i >= row;i --) {
                    if (i - line >= row) {
                        terminal[i] = terminal[i - line];
                    } else {
                        std::fill(terminal[i].begin(), terminal[i].end(), term_char());
                    }
                }
                // set to first column
                col = 0;
            }
        } else if (current == 'M') {
            // CSI Ps M, Delete Ps lines at active row
            int line = read_int_or_default(1);
            if (row < scroll_top || row > scroll_bottom) {
                // outside the scroll margins, do nothing
            } else {
                // delete lines from current row, add new rows from scroll bottom
                for (int i = row;i <= scroll_bottom;i ++) {
                    if (i + line <= scroll_bottom) {
                        terminal[i] = terminal[i + line];
                    } else {
                        std::fill(terminal[i].begin(), terminal[i].end(), term_char());
                    }
                }
                // set to first column
                col = 0;
            }
        } else if (current == 'P') {
            // CSI Ps P, DCH, delete # characters, move right to left
            int del = read_int_or_default(1);
            for (int i = col; i < term_col; i++) {
                if (i + del < term_col) {
                    terminal[row][i] = terminal[row][i + del];
                } else {
                    terminal[row][i] = term_char();
                }
            }
        } else if (current == 'S') {
            // CSI Ps S, SU, Scroll up Ps lines
            int line = read_int_or_default(1);
            for (int i = scroll_top; i <= scroll_bottom; i++) {
                if (i + line <= scroll_bottom) {
                    terminal[i] = terminal[i + line];
                } else {
                    std::fill(terminal[i].begin(), terminal[i].end(), term_char());
                }
            }
        } else if (current == 'X') {
            // CSI Ps X, ECH, erase # characters, do not move others
            int del = read_int_or_default(1);
            for (int i = col; i < col + del && i < term_col; i++) {
                terminal[row][i] = term_char();
            }
        } else if (current == 'c' && (escape_buffer == "" || escape_buffer == "0")) {
            // CSI Ps c, Send Device Attributes, Primary DA
            // mimic xterm
            // send CSI ? 1 ; 2 c: I am VT100 with Advance Video Option
            uint8_t send_buffer[] = {0x1b, '[', '?', '1', ';', '2', 'c'};
            WriteFull(send_buffer, sizeof(send_buffer));
        } else if (current == 'c' && (escape_buffer == ">" || escape_buffer == ">0")) {
            // CSI > Ps c, Send Device Attributes, Secondary DA
            // mimic xterm
            // send CSI > 0 ; 2 7 6 ; 0 c: I am VT100
            uint8_t send_buffer[] = {0x1b, '[', '>', '0', ';', '2', '7', '6', ';', '0', 'c'};
            WriteFull(send_buffer, sizeof(send_buffer));
        } else if (current == 'd' && escape_buffer != "") {
            // CSI Ps d, VPA, move cursor to row #
            sscanf(escape_buffer.c_str(), "%d", &row);
            // convert from 1-based to 0-based
            row--;
            ClampCursor();
        } else if (current == 'f') {
            std::vector<std::string> parts = SplitString(escape_buffer, ";");
            if (parts.size() == 2) {
                // CSI Ps ; PS f, CUP, move cursor to x, y
                sscanf(parts[0].c_str(), "%d", &row);
                sscanf(parts[1].c_str(), "%d", &col);
                // convert from 1-based to 0-based
                row--;
                col--;
                ClampCursor();
            } else {
                goto unknown;
            }
        } else if (current == 'g') {
            int mode = read_int_or_default(0);
            if (mode == 0) {
                // CSI g, CSI 0 g, clear tab stop at the current position
                tab_stops[col] = false;
            } else if (mode == 3) {
                // CSI 3 g, clear all tab stops
                std::fill(tab_stops.begin(), tab_stops.end(), false);
            } else {
                goto unknown;
            }
        } else if (current == 'h' && escape_buffer.size() > 0 && escape_buffer[0] != '?') {
            // CSI Pm h, Set Mode, SM
            std::vector<std::string> parts = SplitString(escape_buffer, ";");
            for (auto part : parts) {
                if (part == "4") {
                    // CSI 4 h, Insert Mode (IRM)
                    insert_mode = true;
                } else {
                    OH_LOG_WARN(LOG_APP, "Unknown CSI Pm h: %{public}s %{public}c",
                                escape_buffer.c_str(), current);
                }
            }
        } else if (current == 'h' && escape_buffer.size() > 0 && escape_buffer[0] == '?') {
            // CSI ? Pm h, DEC Private Mode Set (DECSET)
            std::vector<std::string> parts = SplitString(escape_buffer.substr(1), ";");
            for (auto part : parts) {
                if (part == "1") {
                    // CSI ? 1 h, Application Cursor Keys (DECCKM)
                    // TODO
                } else if (part == "3") {
                    // CSI ? 3 h, Enable 132 Column mode, DECCOLM
                    ResizeTo(term_row, 132);
                    ResizeWidth(132 * font_width);
                } else if (part == "4") {
                    // CSI ? 4 h, Smooth (Slow) Scroll (DECSCLM)
                    // TODO
                } else if (part == "5") {
                    // CSI ? 5 h, Reverse Video (DECSCNM)
                    reverse_video = true;
                } else if (part == "6") {
                    // CSI ? 6 h, Origin Mode (DECOM)
                    origin_mode = true;
                } else if (part == "7") {
                    // CSI ? 7 h, Set autowrap
                    autowrap = true;
                } else if (part == "12") {
                    // CSI ? 12 h, Start blinking cursor
                    // TODO
                } else if (part == "25") {
                    // CSI ? 25 h, DECTCEM, make cursor visible
                    show_cursor = true;
                } else if (part == "40") {
                    // CSI ? 40 h, Allow 80 -> 132 mode, xterm
                    // TODO
                } else if (part == "1000") {
                    // CSI ? 1000 h, Send Mouse X & Y on button press and release
                    // TODO
                } else if (part == "1002") {
                    // CSI ? 1002 h, Use Cell Motion Mouse Tracking
                    // TODO
                } else if (part == "1006") {
                    // CSI ? 1006 h, Enable SGR Mouse Mode
                    // TODO
                } else if (part == "2004") {
                    // CSI ? 2004 h, set bracketed paste mode
                    // TODO
                } else {
                    OH_LOG_WARN(LOG_APP, "Unknown CSI ? Pm h: %{public}s %{public}c",
                                escape_buffer.c_str(), current);
                }
            }
        } else if (current == 'l' && escape_buffer.size() > 0 && escape_buffer[0] != '?') {
            // CSI Pm l, Reset Mode, RM
            std::vector<std::string> parts = SplitString(escape_buffer, ";");
            for (auto part : parts) {
                if (part == "4") {
                    // CSI 4 l, Replace Mode (IRM)
                    insert_mode = false;
                } else {
                    OH_LOG_WARN(LOG_APP, "Unknown CSI Pm h: %{public}s %{public}c",
                                escape_buffer.c_str(), current);
                }
            }
        } else if (current == 'l' && escape_buffer.size() > 0 && escape_buffer[0] == '?') {
            // CSI ? Pm l, DEC Private Mode Reset (DECRST)
            std::vector<std::string> parts = SplitString(escape_buffer.substr(1), ";");
            for (auto part : parts) {
                if (part == "1") {
                    // CSI ? 1 l, Normal Cursor Keys (DECCKM)
                    // TODO
                } else if (part == "3") {
                    // CSI ? 3 l, 80 Column Mode (DECCOLM)
                    ResizeTo(term_row, 80);
                    ResizeWidth(80 * font_width);
                } else if (part == "4") {
                    // CSI ? 4 l, Jump (Fast) Scroll (DECSCLM)
                    // TODO
                } else if (part == "5") {
                    // CSI ? 5 l, Normal Video (DECSCNM)
                    reverse_video = false;
                } else if (part == "6") {
                    // CSI ? 6 l, Normal Cursor Mode (DECOM)
                    origin_mode = false;
                } else if (part == "7") {
                    // CSI ? 7 l, Reset autowrap
                    autowrap = false;
                } else if (part == "8") {
                    // CSI ? 8 l, No Auto-Repeat Keys (DECARM)
                    // TODO
                } else if (part == "12") {
                    // CSI ? 12 l, Stop blinking cursor
                    // TODO
                } else if (part == "25") {
                    // CSI ? 25 l, Hide cursor (DECTCEM)
                    show_cursor = true;
                } else if (part == "45") {
                    // CSI ? 40 l, Disable Graphic Print Color Syntax (DECGPCS)
                    // TODO
                } else if (part == "2004") {
                    // CSI ? 2004 l, reset bracketed paste mode
                    // TODO
                } else {
                    OH_LOG_WARN(LOG_APP, "Unknown CSI ? Pm l: %{public}s %{public}c",
                                escape_buffer.c_str(), current);
                }
            }
        } else if (current == 'm' && (escape_buffer.size() == 0 || escape_buffer[0] != '>')) {
            // CSI Pm m, Character Attributes (SGR)

            // set color
            std::vector<std::string> parts = SplitString(escape_buffer, ";");
            for (size_t i = 0; i < parts.size(); i++) {
                int param = 0;
                std::string part = parts[i];
                sscanf(part.c_str(), "%d", &param);
                if (param == 0) {
                    // reset all attributes to their defaults
                    current_style = term_style();
                } else if (param == 1) {
                    // set bold, CSI 1 m
                    current_style.weight = font_weight::bold;
                } else if (param == 2) {
                    // set faint, CSI 2 m
                    // TODO
                } else if (param == 4) {
                    // set underline, CSI 4 m
                    // TODO
                } else if (param == 5 || param == 6) {
                    // set slowly blink, CSI 5 m
                    // set rapidly blink, CSI 6 m
                    current_style.blink = true;
                } else if (param == 7) {
                    // inverse, flip foreground and background color, CSI 7 m
                    std::swap(current_style.fg_red, current_style.bg_red);
                    std::swap(current_style.fg_green, current_style.bg_green);
                    std::swap(current_style.fg_blue, current_style.bg_blue);
                } else if (param == 9) {
                    // set strikethrough, CSI 9 m
                    // TODO
                } else if (param == 10) {
                    // reset to primary font, CSI 10 m
                    current_style = term_style();
                } else if (param == 21) {
                    // set doubly underlined, CSI 21 m
                    // TODO
                } else if (param == 22) {
                    // set not bold faint, CSI 22 m
                    current_style.weight = font_weight::regular;
                } else if (param == 24) {
                    // set not underlined, CSI 24 m
                    // TODO
                } else if (param == 25) {
                    // set steady (not blinking), CSI 25 m
                    current_style.blink = false;
                } else if (param == 27) {
                    // set positive (not inverse), CSI 27 m
                    std::swap(current_style.fg_red, current_style.bg_red);
                    std::swap(current_style.fg_green, current_style.bg_green);
                    std::swap(current_style.fg_blue, current_style.bg_blue);
                } else if (param == 30) {
                    // black foreground
                    current_style.fg_red = predefined_colors[black][0] / 255.0;
                    current_style.fg_green = predefined_colors[black][1] / 255.0;
                    current_style.fg_blue = predefined_colors[black][2] / 255.0;
                } else if (param == 31) {
                    // red foreground
                    current_style.fg_red = predefined_colors[red][0] / 255.0;
                    current_style.fg_green = predefined_colors[red][1] / 255.0;
                    current_style.fg_blue = predefined_colors[red][2] / 255.0;
                } else if (param == 32) {
                    // green foreground
                    current_style.fg_red = predefined_colors[green][0] / 255.0;
                    current_style.fg_green = predefined_colors[green][1] / 255.0;
                    current_style.fg_blue = predefined_colors[green][2] / 255.0;
                } else if (param == 33) {
                    // yellow foreground
                    current_style.fg_red = predefined_colors[yellow][0] / 255.0;
                    current_style.fg_green = predefined_colors[yellow][1] / 255.0;
                    current_style.fg_blue = predefined_colors[yellow][2] / 255.0;
                } else if (param == 34) {
                    // blue foreground
                    current_style.fg_red = predefined_colors[blue][0] / 255.0;
                    current_style.fg_green = predefined_colors[blue][1] / 255.0;
                    current_style.fg_blue = predefined_colors[blue][2] / 255.0;
                } else if (param == 35) {
                    // magenta foreground
                    current_style.fg_red = predefined_colors[magenta][0] / 255.0;
                    current_style.fg_green = predefined_colors[magenta][1] / 255.0;
                    current_style.fg_blue = predefined_colors[magenta][2] / 255.0;
                } else if (param == 36) {
                    // cyan foreground
                    current_style.fg_red = predefined_colors[cyan][0] / 255.0;
                    current_style.fg_green = predefined_colors[cyan][1] / 255.0;
                    current_style.fg_blue = predefined_colors[cyan][2] / 255.0;
                } else if (param == 37) {
                    // white foreground
                    current_style.fg_red = predefined_colors[white][0] / 255.0;
                    current_style.fg_green = predefined_colors[white][1] / 255.0;
                    current_style.fg_blue = predefined_colors[white][2] / 255.0;
                } else if (param == 38 || param == 48) {
                    // foreground color: extended color, CSI 38 : ... m
                    // background color: extended color, CSI 48 : ... m
                    if (i + 1 < parts.size()) {
                        int color_type = std::stoi(parts[++i]);
                        if (color_type == 5 && i + 1 < parts.size()) { // 256-color mode
                            // specified color index
                            int color_index = std::stoi(parts[++i]);
                            uint32_t color = TrueColorFrom(color_index);
                            if (param == 38) {
                                current_style.fg_red = ((color >> 16) & 0xff) / 255.0;
                                current_style.fg_green = ((color >> 8) & 0xff)  / 255.0;
                                current_style.fg_blue = ((color >> 0) & 0xff)  / 255.0;
                            } else {
                                current_style.bg_red = ((color >> 16) & 0xff) / 255.0;
                                current_style.bg_green = ((color >> 8) & 0xff) / 255.0;
                                current_style.bg_blue = ((color >> 0) & 0xff) / 255.0;
                            }
                        } else if (color_type == 2 && i + 3 < parts.size()) { // RGB mode
                            // specified rgb
                            int r = std::stoi(parts[++i]);
                            int g = std::stoi(parts[++i]);
                            int b = std::stoi(parts[++i]);
                            if (param == 38) {
                                current_style.fg_red = r / 255.0;
                                current_style.fg_green = g / 255.0;
                                current_style.fg_blue = b / 255.0;
                            } else {
                                current_style.bg_red = r / 255.0;
                                current_style.bg_green = g / 255.0;
                                current_style.bg_blue = b / 255.0;
                            }
                        }
                    }
                } else if (param == 39) {
                    // default foreground
                    current_style.fg_red = predefined_colors[black][0] / 255.0;
                    current_style.fg_green = predefined_colors[black][1] / 255.0;
                    current_style.fg_blue = predefined_colors[black][2] / 255.0;
                } else if (param == 40) {
                    // black background
                    current_style.bg_red = predefined_colors[black][0] / 255.0;
                    current_style.bg_green = predefined_colors[black][1] / 255.0;
                    current_style.bg_blue = predefined_colors[black][2] / 255.0;
                } else if (param == 41) {
                    // red background
                    current_style.bg_red = predefined_colors[red][0] / 255.0;
                    current_style.bg_green = predefined_colors[red][1] / 255.0;
                    current_style.bg_blue = predefined_colors[red][2] / 255.0;
                } else if (param == 42) {
                    // green background
                    current_style.bg_red = predefined_colors[green][0] / 255.0;
                    current_style.bg_green = predefined_colors[green][1] / 255.0;
                    current_style.bg_blue = predefined_colors[green][2] / 255.0;
                } else if (param == 43) {
                    // yellow background
                    current_style.bg_red = predefined_colors[yellow][0] / 255.0;
                    current_style.bg_green = predefined_colors[yellow][1] / 255.0;
                    current_style.bg_blue = predefined_colors[yellow][2] / 255.0;
                } else if (param == 44) {
                    // blue background
                    current_style.bg_red = predefined_colors[blue][0] / 255.0;
                    current_style.bg_green = predefined_colors[blue][1] / 255.0;
                    current_style.bg_blue = predefined_colors[blue][2] / 255.0;
                } else if (param == 45) {
                    // magenta background
                    current_style.bg_red = predefined_colors[magenta][0] / 255.0;
                    current_style.bg_green = predefined_colors[magenta][1] / 255.0;
                    current_style.bg_blue = predefined_colors[magenta][2] / 255.0;
                } else if (param == 46) {
                    // cyan background
                    current_style.bg_red = predefined_colors[cyan][0] / 255.0;
                    current_style.bg_green = predefined_colors[cyan][1] / 255.0;
                    current_style.bg_blue = predefined_colors[cyan][2] / 255.0;
                } else if (param == 47) {
                    // white background
                    current_style.bg_red = predefined_colors[white][0] / 255.0;
                    current_style.bg_green = predefined_colors[white][1] / 255.0;
                    current_style.bg_blue = predefined_colors[white][2] / 255.0;
                } else if (param == 49) {
                    // default background
                    current_style.bg_red = predefined_colors[white][0] / 255.0;
                    current_style.bg_green = predefined_colors[white][1] / 255.0;
                    current_style.bg_blue = predefined_colors[white][2] / 255.0;
                } else if (param == 90) {
                    // bright black foreground
                    current_style.fg_red = predefined_colors[brblack][0] / 255.0;
                    current_style.fg_green = predefined_colors[brblack][1] / 255.0;
                    current_style.fg_blue = predefined_colors[brblack][2] / 255.0;
                } else if (param == 91) {
                    // red foreground
                    current_style.fg_red = predefined_colors[brred][0] / 255.0;
                    current_style.fg_green = predefined_colors[brred][1] / 255.0;
                    current_style.fg_blue = predefined_colors[brred][2] / 255.0;
                } else if (param == 92) {
                    // green foreground
                    current_style.fg_red = predefined_colors[brgreen][0] / 255.0;
                    current_style.fg_green = predefined_colors[brgreen][1] / 255.0;
                    current_style.fg_blue = predefined_colors[brgreen][2] / 255.0;
                } else if (param == 93) {
                    // yellow foreground
                    current_style.fg_red = predefined_colors[bryellow][0] / 255.0;
                    current_style.fg_green = predefined_colors[bryellow][1] / 255.0;
                    current_style.fg_blue = predefined_colors[bryellow][2] / 255.0;
                } else if (param == 94) {
                    // blue foreground
                    current_style.fg_red = predefined_colors[brblue][0] / 255.0;
                    current_style.fg_green = predefined_colors[brblue][1] / 255.0;
                    current_style.fg_blue = predefined_colors[brblue][2] / 255.0;
                } else if (param == 95) {
                    // magenta foreground
                    current_style.fg_red = predefined_colors[brmagenta][0] / 255.0;
                    current_style.fg_green = predefined_colors[brmagenta][1] / 255.0;
                    current_style.fg_blue = predefined_colors[brmagenta][2] / 255.0;
                } else if (param == 96) {
                    // cyan foreground
                    current_style.fg_red = predefined_colors[brcyan][0] / 255.0;
                    current_style.fg_green = predefined_colors[brcyan][1] / 255.0;
                    current_style.fg_blue = predefined_colors[brcyan][2] / 255.0;
                } else if (param == 97) {
                    // white foreground
                    current_style.fg_red = predefined_colors[brwhite][0] / 255.0;
                    current_style.fg_green = predefined_colors[brwhite][1] / 255.0;
                    current_style.fg_blue = predefined_colors[brwhite][2] / 255.0;
                } else if (param == 100) {
                    // black background
                    current_style.bg_red = predefined_colors[brblack][0] / 255.0;
                    current_style.bg_green = predefined_colors[brblack][1] / 255.0;
                    current_style.bg_blue = predefined_colors[brblack][2] / 255.0;
                } else if (param == 101) {
                    // red background
                    current_style.bg_red = predefined_colors[brred][0] / 255.0;
                    current_style.bg_green = predefined_colors[brred][1] / 255.0;
                    current_style.bg_blue = predefined_colors[brred][2] / 255.0;
                } else if (param == 102) {
                    // green background
                    current_style.bg_red = predefined_colors[brgreen][0] / 255.0;
                    current_style.bg_green = predefined_colors[brgreen][1] / 255.0;
                    current_style.bg_blue = predefined_colors[brgreen][2] / 255.0;
                } else if (param == 103) {
                    // yellow background
                    current_style.bg_red = predefined_colors[bryellow][0] / 255.0;
                    current_style.bg_green = predefined_colors[bryellow][1] / 255.0;
                    current_style.bg_blue = predefined_colors[bryellow][2] / 255.0;
                } else if (param == 104) {
                    // blue background
                    current_style.bg_red = predefined_colors[brblue][0] / 255.0;
                    current_style.bg_green = predefined_colors[brblue][1] / 255.0;
                    current_style.bg_blue = predefined_colors[brblue][2] / 255.0;
                } else if (param == 105) {
                    // magenta background
                    current_style.bg_red = predefined_colors[brmagenta][0] / 255.0;
                    current_style.bg_green = predefined_colors[brmagenta][1] / 255.0;
                    current_style.bg_blue = predefined_colors[brmagenta][2] / 255.0;
                } else if (param == 106) {
                    // cyan background
                    current_style.bg_red = predefined_colors[brcyan][0] / 255.0;
                    current_style.bg_green = predefined_colors[brcyan][1] / 255.0;
                    current_style.bg_blue = predefined_colors[brcyan][2] / 255.0;
                } else if (param == 107) {
                    // white background
                    current_style.bg_red = predefined_colors[brwhite][0] / 255.0;
                    current_style.bg_green = predefined_colors[brwhite][1] / 255.0;
                    current_style.bg_blue = predefined_colors[brwhite][2] / 255.0;
                } else {
                    OH_LOG_WARN(LOG_APP, "Unknown CSI Pm m: %{public}s from %{public}s %{public}c",
                                part.c_str(), escape_buffer.c_str(), current);
                }
            }
        } else if (current == 'm' && escape_buffer.size() > 0 && escape_buffer[0] == '>') {
            // CSI > Pp m, XTMODKEYS, set/reset key modifier options
            // TODO
        } else if (current == 'n' && escape_buffer == "5") {
            // CSI 5 n - Device Status Report
            // send "OK" - ESC [ 0 n
            uint8_t ok_response[] = {0x1B, '[', '0', 'n'};
            WriteFull(ok_response, sizeof(ok_response));
        } else if (current == 'n' && (escape_buffer == "6")) {
            // CSI Ps n, DSR, Device Status Report
            // Ps = 6: Report Cursor Position (CPR)
            // send ESC [ row ; col R
            char send_buffer[128] = {};
            snprintf(send_buffer, sizeof(send_buffer), "\x1b[%d;%dR", row + 1, col + 1);
            int len = strlen(send_buffer);
            WriteFull((uint8_t *)send_buffer, len);
        } else if (current == 'r') {
            // CSI Ps ; Ps r, Set Scrolling Region [top;bottom]
            std::vector<std::string> parts = SplitString(escape_buffer, ";");
            int new_top = 1;
            int new_bottom = term_row;
            if (parts.size() == 2) {
                // CSI Ps ; Ps r
                sscanf(parts[0].c_str(), "%d", &new_top);
                sscanf(parts[1].c_str(), "%d", &new_bottom);
                // convert to 1-based
                new_top --;
                new_bottom --;
            } else if (escape_buffer == "") {
                // full size of window
                // CSI r
                new_top = 0;
                new_bottom = term_row - 1;
            } else if (parts.size() == 1) {
                // CSI Ps r
                sscanf(parts[0].c_str(), "%d", &new_top);
                // convert to 1-based
                new_top --;
                new_bottom = term_row - 1;
            } else {
                goto unknown;
            }

            if (new_bottom > new_top) {
                scroll_top = new_top;
                scroll_bottom = new_bottom;

                // move cursor to new home position
                row = scroll_top;
                col = 0;
            }
        } else if (current == '@' &&
                ((escape_buffer.size() > 0 && escape_buffer[escape_buffer.size() - 1] >= '0' &&
                    escape_buffer[escape_buffer.size() - 1] <= '9') ||
                    escape_buffer == "")) {
            // CSI Ps @, ICH, Insert Ps (Blank) Character(s)
            int count = read_int_or_default(1);
            for (int i = term_col - 1; i >= col; i--) {
                if (i - col < count) {
                    terminal[row][i].ch = ' ';
                } else {
                    terminal[row][i] = terminal[row][i - count];
                }
            }
        } else {
unknown:
            // unknown
            OH_LOG_WARN(LOG_APP, "Unknown escape sequence in CSI: %{public}s %{public}c",
                        escape_buffer.c_str(), current);
        }
        escape_state = state_idle;
    } else if (current >= 0x20 && current <= 0x3F) {
        // parameter bytes in [0x30, 0x3F],
        // or intermediate bytes in [0x20, 0x2F]
        escape_buffer += current;
    } else {
        // invalid byte
        // unknown
        OH_LOG_WARN(LOG_APP, "Unknown escape sequence in CSI: %{public}s %{public}c",
                    escape_buffer.c_str(), current);
        escape_state = state_idle;
    }
}

void *terminal_context::TerminalWorker(void * data) {
    terminal_context *ctx = (terminal_context *)data;
    ctx->Worker();
    return NULL;
}

void terminal_context::Parse(uint8_t input) {
    if (escape_state == state_esc) {
        if (input == '[' && escape_buffer == "") {
            // ESC [ = CSI
            escape_state = state_csi;
        } else if (input == ']' && escape_buffer == "") {
            // ESC ] = OSC
            escape_state = state_osc;
        } else if (input == '=' && escape_buffer == "") {
            // ESC =, enter alternate keypad mode
            // TODO
            escape_state = state_idle;
        } else if (input == '>' && escape_buffer == "") {
            // ESC >, exit alternate keypad mode
            // TODO
            escape_state = state_idle;
        } else if (input == 'A' && escape_buffer == "") {
            // ESC A, cursor up
            row --;
            ClampCursor();
            escape_state = state_idle;
        } else if (input == 'B' && escape_buffer == "") {
            // ESC B, cursor down
            row ++;
            ClampCursor();
            escape_state = state_idle;
        } else if (input == 'C' && escape_buffer == "") {
            // ESC C, cursor right
            col ++;
            ClampCursor();
            escape_state = state_idle;
        } else if (input == 'D' && escape_buffer == "") {
            // ESC D, IND, cursor down and scroll
            row += 1;
            DropFirstRowIfOverflow();
            escape_state = state_idle;
        } else if (input == 'E' && escape_buffer == "") {
            // ESC E, goto to the beginning of next row
            row ++;
            col = 0;
            ClampCursor();
            escape_state = state_idle;
        } else if (input == 'H' && escape_buffer == "") {
            // ESC H, place tab stop at the current position
            tab_stops[col] = true;
            escape_state = state_idle;
        } else if (input == 'M' && escape_buffer == "") {
            // ESC M, move cursor one line up, scrolls down if at the top margin
            if (row == scroll_top) {
                // shift rows down
                for (int i = scroll_bottom;i > scroll_top;i--) {
                    terminal[i] = terminal[i-1];
                }
                std::fill(terminal[scroll_top].begin(), terminal[scroll_top].end(), term_char());
            } else {
                row --;
                ClampCursor();
            }
            escape_state = state_idle;
        } else if (input == 'P' && escape_buffer == "") {
            // ESC P = DCS
            escape_state = state_dcs;
        } else if (input == '8' && escape_buffer == "#") {
            // ESC # 8, DECALN fill viewport with a test pattern (E)
            for (int i = 0;i < term_row;i++) {
                for (int j = 0;j < term_col;j++) {
                    terminal[i][j] = term_char();
                    terminal[i][j].ch = 'E';
                }
            }
            escape_state = state_idle;
        } else if (input == '7' && escape_buffer == "") {
            // ESC 7, save cursor
            save_row = row;
            save_col = col;
            save_style = current_style;
            escape_state = state_idle;
        } else if (input == '8' && escape_buffer == "") {
            // ESC 8, restore cursor
            row = save_row;
            col = save_col;
            ClampCursor();
            current_style = save_style;
            escape_state = state_idle;
        } else if (input == '#' || input == '(' || input == ')') {
            // non terminal
            escape_buffer += input;
        } else {
            // unknown
            OH_LOG_WARN(LOG_APP, "Unknown escape sequence after ESC: %{public}s %{public}c",
                        escape_buffer.c_str(), input);
            escape_state = state_idle;
        }
    } else if (escape_state == state_csi) {
        HandleCSI(input);
    } else if (escape_state == state_osc) {
        if (input == '\x07') {
            // OSC Ps ; Pt BEL
            std::vector<std::string> parts = SplitString(escape_buffer, ";");
            if (parts.size() == 3 && parts[0] == "52" && parts[1] == "c" && parts[2] != "?") {
                // OSC 52 ; c ; BASE64 BEL
                // copy to clipboard
                std::string base64 = parts[2];
                OH_LOG_INFO(LOG_APP, "Copy to pasteboard in native: %{public}s",
                            base64.c_str());
                Copy(base64);
            } else if (parts.size() == 3 && parts[0] == "52" && parts[1] == "c" && parts[2] == "?") {
                // OSC 52 ; c ; ? BEL
                // paste from clipboard
                RequestPaste();
                OH_LOG_INFO(LOG_APP, "Request Paste from pasteboard: %{public}s", escape_buffer.c_str());
            }
            escape_state = state_idle;
        } else if (input == '\\' && escape_buffer.size() > 0 && escape_buffer[escape_buffer.size() - 1] == '\x1b') {
            // ST is ESC \
            // OSC Ps ; Pt ST
            std::vector<std::string> parts = SplitString(escape_buffer.substr(0, escape_buffer.size() - 1), ";");
            if (parts.size() == 2 && parts[0] == "10" && parts[1] == "?") {
                // OSC 10 ; ? ST
                // report foreground color: black
                // send OSI 10 ; r g b : 0 / 0 / 0 ST
                uint8_t send_buffer[] = {0x1b, ']', '1', '0', ';', 'r', 'g', 'b', ':', '0', '/', '0', '/', '0', '\x1b', '\\'};
                WriteFull(send_buffer, sizeof(send_buffer));
            } else if (parts.size() == 2 && parts[0] == "11" && parts[1] == "?") {
                // OSC 11 ; ? ST
                // report background color: white
                // send OSI 11 ; r g b : f / f / f ST
                uint8_t send_buffer[] = {0x1b, ']', '1', '0', ';', 'r', 'g', 'b', ':', 'f', '/', 'f', '/', 'f', '\x1b', '\\'};
                WriteFull(send_buffer, sizeof(send_buffer));
            }
            escape_state = state_idle;
        } else if ((input >= ' ' && input < 127) || input == '\x1b') {
            // printable character
            escape_buffer += input;
        } else {
            // unknown
            OH_LOG_WARN(LOG_APP, "Unknown escape sequence in OSC: %{public}s %{public}c",
                        escape_buffer.c_str(), input);
            escape_state = state_idle;
        }
    } else if (escape_state == state_dcs) {
        if (input == '\\' && escape_buffer.size() > 0 && escape_buffer[escape_buffer.size() - 1] == '\x1b') {
            // ST
            escape_state = state_idle;
        } else if (input >= ' ' && input < 127 && input == '\x1b') {
            // printable character
            escape_buffer += input;
        } else {
            // unknown
            OH_LOG_WARN(LOG_APP, "Unknown escape sequence in DCS: %{public}s %{public}c",
                        escape_buffer.c_str(), input);
            escape_state = state_idle;
        }
    } else if (escape_state == state_idle) {
        // escape state is idle
        if (utf8_state == state_initial) {
            if (input >= ' ' && input <= 0x7f) {
                // printable
                if (insert_mode) {
                    // move characters rightward
                    for (int i = term_col - 1;i > col;i--) {
                        terminal[row][i] = terminal[row][i - 1];
                    }
                }
                InsertUtf8(input);
            } else if (input >= 0xc2 && input <= 0xdf) {
                // 2-byte utf8
                utf8_state = state_2byte_2;
                current_utf8 = (uint32_t)(input & 0x1f) << 6;
            } else if (input == 0xe0) {
                // 3-byte utf8 starting with e0
                utf8_state = state_3byte_2_e0;
                current_utf8 = (uint32_t)(input & 0x0f) << 12;
            } else if (input >= 0xe1 && input <= 0xef) {
                // 3-byte utf8 starting with non-e0
                utf8_state = state_3byte_2_non_e0;
                current_utf8 = (uint32_t)(input & 0x0f) << 12;
            } else if (input == 0xf0) {
                // 4-byte utf8 starting with f0
                utf8_state = state_4byte_2_f0;
                current_utf8 = (uint32_t)(input & 0x07) << 18;
            } else if (input >= 0xf1 && input <= 0xf3) {
                // 4-byte utf8 starting with f1 to f3
                utf8_state = state_4byte_2_f1_f3;
                current_utf8 = (uint32_t)(input & 0x07) << 18;
            } else if (input == 0xf4) {
                // 4-byte utf8 starting with f4
                utf8_state = state_4byte_2_f4;
                current_utf8 = (uint32_t)(input & 0x07) << 18;
            } else if (input == '\r') {
                col = 0;
            } else if (input == '\n') {
                // CUD1=\n, cursor down by 1
                row += 1;
                DropFirstRowIfOverflow();
            } else if (input == '\b') {
                // CUB1=^H, cursor backward by 1
                if (col > 0) {
                    col -= 1;
                }
            } else if (input == '\t') {
                // goto next tab stop
                col ++;
                while (col < term_col && !tab_stops[col]) {
                    col ++;
                }
                ClampCursor();
            } else if (input == 0x1b) {
                escape_buffer = "";
                escape_state = state_esc;
            }
        } else if (utf8_state == state_2byte_2) {
            // expecting the second byte of 2-byte utf-8
            if (input >= 0x80 && input <= 0xbf) {
                current_utf8 |= (input & 0x3f);
                InsertUtf8(current_utf8);
            }
            utf8_state = state_initial;
        } else if (utf8_state == state_3byte_2_e0) {
            // expecting the second byte of 3-byte utf-8 starting with 0xe0
            if (input >= 0xa0 && input <= 0xbf) {
                current_utf8 |= (uint32_t)(input & 0x3f) << 6;
                utf8_state = state_3byte_3;
            } else {
                utf8_state = state_initial;
            }
        } else if (utf8_state == state_3byte_2_non_e0) {
            // expecting the second byte of 3-byte utf-8 starting with non-0xe0
            if (input >= 0x80 && input <= 0xbf) {
                current_utf8 |= (uint32_t)(input & 0x3f) << 6;
                utf8_state = state_3byte_3;
            } else {
                utf8_state = state_initial;
            }
        } else if (utf8_state == state_3byte_3) {
            // expecting the third byte of 3-byte utf-8 starting with 0xe0
            if (input >= 0x80 && input <= 0xbf) {
                current_utf8 |= (input & 0x3f);
                InsertUtf8(current_utf8);
            }
            utf8_state = state_initial;
        } else if (utf8_state == state_4byte_2_f0) {
            // expecting the second byte of 4-byte utf-8 starting with 0xf0
            if (input >= 0x90 && input <= 0xbf) {
                current_utf8 |= (uint32_t)(input & 0x3f) << 12;
                utf8_state = state_4byte_3;
            } else {
                utf8_state = state_initial;
            }
        } else if (utf8_state == state_4byte_2_f1_f3) {
            // expecting the second byte of 4-byte utf-8 starting with 0xf0 to 0xf3
            if (input >= 0x80 && input <= 0xbf) {
                current_utf8 |= (uint32_t)(input & 0x3f) << 12;
                utf8_state = state_4byte_3;
            } else {
                utf8_state = state_initial;
            }
        } else if (utf8_state == state_4byte_2_f4) {
            // expecting the second byte of 4-byte utf-8 starting with 0xf4
            if (input >= 0x80 && input <= 0x8f) {
                current_utf8 |= (uint32_t)(input & 0x3f) << 12;
                utf8_state = state_4byte_3;
            } else {
                utf8_state = state_initial;
            }
        } else if (utf8_state == state_4byte_3) {
            // expecting the third byte of 4-byte utf-8
            if (input >= 0x80 && input <= 0xbf) {
                current_utf8 |= (uint32_t)(input & 0x3f) << 6;
                utf8_state = state_4byte_4;
            } else {
                utf8_state = state_initial;
            }
        } else if (utf8_state == state_4byte_4) {
            // expecting the third byte of 4-byte utf-8
            if (input >= 0x80 && input <= 0xbf) {
                current_utf8 |= (input & 0x3f);
                InsertUtf8(current_utf8);
            }
            utf8_state = state_initial;
        } else {
            assert(false && "unreachable utf8 state");
        }
    } else {
        assert(false && "unreachable escape state");
    }
}

void terminal_context::Worker() {
    pthread_setname_np(pthread_self(), "terminal worker");

    int temp = 0;
    // poll from fd, and render
    struct timeval tv;
    while (1) {
        struct pollfd fds[1];
        fds[0].fd = fd;
        fds[0].events = POLLIN;
        int res = poll(fds, 1, 100);

        uint8_t buffer[1024];
        if (res > 0) {
            ssize_t r = read(fd, buffer, sizeof(buffer) - 1);
            if (r > 0) {
                // pretty print
                std::string hex;
                for (int i = 0; i < r; i++) {
                    if (buffer[i] >= 127 || buffer[i] < 32) {
                        char temp[8];
                        snprintf(temp, sizeof(temp), "\\x%02x", buffer[i]);
                        hex += temp;
                    } else {
                        hex += (char)buffer[i];
                    }
                }
                OH_LOG_INFO(LOG_APP, "Got: %{public}s", hex.c_str());

                // parse output
                pthread_mutex_lock(&lock);
                for (int i = 0; i < r; i++) {
                    Parse(buffer[i]);
                }
                pthread_mutex_unlock(&lock);
            } else if (r < 0 && errno == EIO) {
                // handle child exit
                OH_LOG_INFO(LOG_APP, "Program exited: %{public}ld %{public}d", r, errno);
                // relaunch
                pthread_mutex_lock(&lock);
                close(fd);
                fd = -1;

                // print message in a separate line
                if (col > 0) {
                    row += 1;
                    DropFirstRowIfOverflow();
                    col = 0;
                }

                std::string message = "[program exited, restarting]";
                for (char ch : message) {
                    InsertUtf8(ch);
                }

                row += 1;
                DropFirstRowIfOverflow();
                col = 0;

                Fork();
                pthread_mutex_unlock(&lock);
                break;
            }
        }

        // check if anything to paste
        std::string paste = GetPaste();
        if (paste.size() > 0) {
            // send OSC 52 ; c ; BASE64 ST
            OH_LOG_INFO(LOG_APP, "Paste from pasteboard: %{public}s",
                        paste.c_str());
            std::string resp = "\x1b]52;c;" + paste + "\x1b\\";
            WriteFull((uint8_t *)resp.c_str(), resp.size());
        }
    }
    return;
}

// fork & create pty
// assume lock is held
void terminal_context::Fork() {
    struct winsize ws = {};
    ws.ws_col = term_col;
    ws.ws_row = term_row;

    int pid = forkpty(&fd, nullptr, nullptr, &ws);
    if (!pid) {
#ifdef STANDALONE
        execl("/bin/bash", "/bin/bash", nullptr);
#else
        // override HOME to /storage/Users/currentUser since it is writable
        const char *home = "/storage/Users/currentUser";
        setenv("HOME", home, 1);
        setenv("PWD", home, 1);
        // set LD_LIRBARY_PATH for shared libraries
        setenv("LD_LIBRARY_PATH", "/data/app/base.org/base_1.0/lib", 1);
        // override TMPDIR for tmux
        setenv("TMUX_TMPDIR", "/data/storage/el2/base/cache", 1);
        chdir(home);
        execl("/data/app/bin/bash", "/data/app/bin/bash", nullptr);
#endif
    }

    // set as non blocking
    int res = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    assert(res == 0);

    // start terminal worker in another thread
    pthread_t terminal_thread;
    pthread_create(&terminal_thread, NULL, TerminalWorker, this);
}

static terminal_context term;

// https://learnopengl.com/In-Practice/Text-Rendering
struct ivec2 {
    int x;
    int y;

    ivec2(int x, int y) {
        this->x = x;
        this->y = y;
    }
    ivec2() { this->x = this->y = 0; }
};

// glyph info
struct character {
    // location within the large texture
    float left;
    float right;
    float top;
    float bottom;
    // x, y offset from origin for bearing etc.
    int xoff;
    int yoff;
    // glyph size
    int width;
    int height;
};

// record info for each character
// map from (codepoint, font weight) to character
static std::map<std::pair<uint32_t, enum font_weight>, struct character> characters;
// code points to load from the font
static std::set<uint32_t> codepoints_to_load;
// do we need to reload font due to missing glyphs?
static bool need_reload_font = false;
// id of texture for glyphs
static GLuint texture_id;

static void ResizeTo(int new_term_row, int new_term_col, bool update_viewport = true) {
    // update viewport
    if (update_viewport) {
        width = new_term_col * font_width;
        height = new_term_row * font_height;
    }

    term.ResizeTo(new_term_row, new_term_col);
}

void Start() {
    pthread_mutex_lock(&term.lock);
    if (term.fd != -1) {
        return;
    }

    // setup terminal, default to 80x24
    term.ResizeTo(24, 80);

    term.Fork();

    pthread_mutex_unlock(&term.lock);
}

void SendData(uint8_t *data, size_t length) {
    if (term.fd == -1) {
        return;
    }

    // reset scroll offset to bottom
    scroll_offset = 0.0;

    term.WriteFull(data, length);
}

// load font
// texture contains all glyphs of all weights:
// fixed width of max_font_width, variable height based on face->glyph->bitmap.rows
// glyph goes in vertical, possibly not filling the whole row space:
//    0.0       1.0
// 0.0 +------+--+
//     | 0x00 |  |
// 0.5 +------+--+
//     | 0x01    |
// 1.0 +------+--+
static void LoadFont() {
    need_reload_font = false;

    FT_Library ft;
    FT_Error err = FT_Init_FreeType(&ft);
    assert(err == 0);

    std::vector<std::pair<const char *, font_weight>> fonts = {
#ifdef STANDALONE
        {"../../../../../fonts/ttf/Inconsolata-Regular.ttf", font_weight::regular},
        {"../../../../../fonts/ttf/Inconsolata-Bold.ttf", font_weight::bold},
#else
        {"/data/storage/el2/base/haps/entry/files/Inconsolata-Regular.ttf", font_weight::regular},
        {"/data/storage/el2/base/haps/entry/files/Inconsolata-Bold.ttf", font_weight::bold},
#endif
    };

    // save glyph for all characters of all weights
    // only one channel
    std::vector<uint8_t> bitmap;
    int row_stride = max_font_width;
    int bitmap_height = 0;

    for (auto pair : fonts) {
        const char *font = pair.first;
        font_weight weight = pair.second;

        FT_Face face;
        err = FT_New_Face(ft, font, 0, &face);
        assert(err == 0);
        FT_Set_Pixel_Sizes(face, 0, font_height);
        // Note: in 26.6 fractional pixel format
        OH_LOG_INFO(LOG_APP,
                    "Ascender: %{public}d Descender: %{public}d Height: %{public}d XMin: %{public}ld XMax: %{public}ld "
                    "YMin: %{public}ld YMax: %{public}ld XScale: %{public}ld YScale: %{public}ld",
                    face->ascender, face->descender, face->height, face->bbox.xMin, face->bbox.xMax, face->bbox.yMin,
                    face->bbox.yMax, face->size->metrics.x_scale, face->size->metrics.y_scale);

        for (uint32_t c : codepoints_to_load) {
            // load character glyph
            assert(FT_Load_Char(face, c, FT_LOAD_RENDER) == 0);

            OH_LOG_INFO(LOG_APP,
                        "Weight: %{public}d Char: %{public}d(0x%{public}x) Glyph: %{public}d %{public}d Left: "
                        "%{public}d "
                        "Top: %{public}d "
                        "Advance: %{public}ld",
                        weight, c, c, face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap_left,
                        face->glyph->bitmap_top, face->glyph->advance.x);

            // copy to bitmap
            int old_bitmap_height = bitmap_height;
            int new_bitmap_height = bitmap_height + face->glyph->bitmap.rows;
            bitmap.resize(row_stride * new_bitmap_height);
            bitmap_height = new_bitmap_height;

            assert(face->glyph->bitmap.width <= row_stride);
            for (int i = 0; i < face->glyph->bitmap.rows; i++) {
                for (int j = 0; j < face->glyph->bitmap.width; j++) {
                    // compute offset in the large texture
                    int off = old_bitmap_height * row_stride;
                    bitmap[i * row_stride + j + off] = face->glyph->bitmap.buffer[i * face->glyph->bitmap.width + j];
                }
            }

            // compute location within the texture
            // first pass: store pixels
            character character = {
                .left = 0,
                .right = (float)face->glyph->bitmap.width - 1,
                .top = (float)old_bitmap_height,
                .bottom = (float)new_bitmap_height - 1,
                .xoff = face->glyph->bitmap_left,
                .yoff = (int)(baseline_height + face->glyph->bitmap_top - face->glyph->bitmap.rows),
                .width = (int)face->glyph->bitmap.width,
                .height = (int)face->glyph->bitmap.rows,
            };
            characters[{c, weight}] = character;
        }


        FT_Done_Face(face);
    }

    FT_Done_FreeType(ft);

    // now bitmap contains all glyphs
    // second pass: convert pixels to uv coordinates
    for (auto &pair : characters) {
        // https://stackoverflow.com/questions/35454432/finding-image-pixel-coordinates-integers-from-uv-values-floats-of-obj-file
        pair.second.left /= row_stride - 1;
        pair.second.right /= row_stride - 1;
        pair.second.top = (pair.second.top + 0.5) / bitmap_height;
        pair.second.bottom = (pair.second.bottom + 0.5) / bitmap_height;
    }

    // disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // generate texture
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, row_stride, bitmap_height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());

    // set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static GLuint program_id;
static GLuint vertex_array;
// vec4 vertex
static GLuint vertex_buffer;
// vec3 textColor
static GLuint text_color_buffer;
// vec3 backGroundColor
static GLuint background_color_buffer;

static void Draw() {
    // blink every 0.5s
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t current_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // clear buffer with background color
    glClearColor(predefined_colors[white][0] / 255.0, predefined_colors[white][1] / 255.0, predefined_colors[white][2] / 255.0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // update surface size
    pthread_mutex_lock(&term.lock);
    int aligned_width = width / font_width * font_width;
    int aligned_height = height / font_height * font_height;
    glUniform2f(surface_location, aligned_width, aligned_height);
    glViewport(0, height - aligned_height, aligned_width, aligned_height);

    // set texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // bind our vertex array
    glBindVertexArray(vertex_array);

    int max_lines = height / font_height;
    // vec4 vertex
    static std::vector<GLfloat> vertex_pass0_data;
    static std::vector<GLfloat> vertex_pass1_data;
    // vec3 textColor
    static std::vector<GLfloat> text_color_data;
    // vec3 backgroundColor
    static std::vector<GLfloat> background_color_data;

    vertex_pass0_data.clear();
    vertex_pass0_data.reserve(term.term_row * term.term_col * 24);
    vertex_pass1_data.clear();
    vertex_pass1_data.reserve(term.term_row * term.term_col * 24);
    text_color_data.clear();
    text_color_data.reserve(term.term_row * term.term_col * 18);
    background_color_data.clear();
    background_color_data.reserve(term.term_row * term.term_col * 18);

    // ensure at least one line shown, for very large scroll_offset
    int scroll_rows = scroll_offset / font_height;
    if ((int)term.history.size() + max_lines - 1 - scroll_rows < 0) {
        scroll_offset = ((int)term.history.size() + max_lines - 1) * font_height;
        scroll_rows = scroll_offset / font_height;
    }

    for (int i = 0; i < max_lines; i++) {
        // (aligned_height - font_height) is terminal[0] when scroll_offset is zero
        float x = 0.0;
        float y = aligned_height - (i + 1) * font_height;
        int i_row = i - scroll_rows;
        std::vector<term_char> ch;
        if (i_row >= 0 && i_row < term.term_row) {
            ch = term.terminal[i_row];
        } else if (i_row < 0 && (int)term.history.size() + i_row >= 0) {
            ch = term.history[term.history.size() + i_row];
        } else {
            continue;
        }

        int cur_col = 0;
        for (auto c : ch) {
            uint32_t codepoint = c.ch;
            auto key = std::pair<uint32_t, enum font_weight>(c.ch, c.style.weight);
            auto it = characters.find(key);
            if (it == characters.end()) {
                // reload font to locate it
                OH_LOG_WARN(LOG_APP, "Missing character: %{public}d of weight %{public}d", c.ch, c.style.weight);
                need_reload_font = true;
                codepoints_to_load.insert(c.ch);

                // we don't have the character, fallback to space
                it = characters.find(std::pair<uint32_t, enum font_weight>(' ', c.style.weight));
                assert(it != characters.end());
            }

            character ch = it->second;
            float xpos = x;
            float ypos = y;
            float w = font_width;
            float h = font_height;

            // 1-2
            // | |
            // 3-4
            // (xpos    , ypos + h): 1
            // (xpos + w, ypos + h): 2
            // (xpos    , ypos    ): 3
            // (xpos + w, ypos    ): 4

            // pass 0: draw background
            GLfloat g_vertex_pass0_data[24] = {// first triangle: 1->3->4
                                               xpos, ypos + h, 0.0, 0.0, xpos, ypos, 0.0, 0.0, xpos + w, ypos, 0.0, 0.0,
                                               // second triangle: 1->4->2
                                               xpos, ypos + h, 0.0, 0.0, xpos + w, ypos, 0.0, 0.0, xpos + w, ypos + h,
                                               0.0, 0.0};
            vertex_pass0_data.insert(vertex_pass0_data.end(), &g_vertex_pass0_data[0], &g_vertex_pass0_data[24]);

            // pass 1: draw text
            xpos = x + ch.xoff;
            ypos = y + ch.yoff;
            w = ch.width;
            h = ch.height;
            GLfloat g_vertex_pass1_data[24] = {// first triangle: 1->3->4
                                               xpos, ypos + h, ch.left, ch.top, xpos, ypos, ch.left, ch.bottom,
                                               xpos + w, ypos, ch.right, ch.bottom,
                                               // second triangle: 1->4->2
                                               xpos, ypos + h, ch.left, ch.top, xpos + w, ypos, ch.right, ch.bottom,
                                               xpos + w, ypos + h, ch.right, ch.top};
            vertex_pass1_data.insert(vertex_pass1_data.end(), &g_vertex_pass1_data[0], &g_vertex_pass1_data[24]);

            GLfloat g_text_color_buffer_data[18];
            GLfloat g_background_color_buffer_data[18];

            if (i_row == term.row && cur_col == term.col && term.show_cursor) {
                // cursor
                for (int i = 0; i < 6; i++) {
                    g_text_color_buffer_data[i * 3 + 0] = 1.0 - c.style.fg_red;
                    g_text_color_buffer_data[i * 3 + 1] = 1.0 - c.style.fg_green;
                    g_text_color_buffer_data[i * 3 + 2] = 1.0 - c.style.fg_blue;
                    g_background_color_buffer_data[i * 3 + 0] = 1.0 - c.style.bg_red;
                    g_background_color_buffer_data[i * 3 + 1] = 1.0 - c.style.bg_green;
                    g_background_color_buffer_data[i * 3 + 2] = 1.0 - c.style.bg_blue;
                }
            } else {
                for (int i = 0; i < 6; i++) {
                    g_text_color_buffer_data[i * 3 + 0] = c.style.fg_red;
                    g_text_color_buffer_data[i * 3 + 1] = c.style.fg_green;
                    g_text_color_buffer_data[i * 3 + 2] = c.style.fg_blue;
                    g_background_color_buffer_data[i * 3 + 0] = c.style.bg_red;
                    g_background_color_buffer_data[i * 3 + 1] = c.style.bg_green;
                    g_background_color_buffer_data[i * 3 + 2] = c.style.bg_blue;
                }
            }

            if (term.reverse_video) {
                // invert all colors
                for (int i = 0; i < 18; i++) {
                    g_text_color_buffer_data[i] = 1.0 - g_text_color_buffer_data[i];
                    g_background_color_buffer_data[i] = 1.0 - g_background_color_buffer_data[i];
                }
            }

            if (c.style.blink && current_msec % 1000 > 500) {
                // for every 1s, in 0.5s, text color equals to back ground color
                for (int i = 0; i < 18; i++) {
                    g_text_color_buffer_data[i] = g_background_color_buffer_data[i];
                }
            }

            text_color_data.insert(text_color_data.end(), &g_text_color_buffer_data[0], &g_text_color_buffer_data[18]);
            background_color_data.insert(background_color_data.end(), &g_background_color_buffer_data[0],
                                         &g_background_color_buffer_data[18]);

            x += font_width;
            cur_col++;
        }
    }
    pthread_mutex_unlock(&term.lock);

    // draw in two pass
    glBindBuffer(GL_ARRAY_BUFFER, text_color_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * text_color_data.size(), text_color_data.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, background_color_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * background_color_data.size(), background_color_data.data(),
                 GL_STREAM_DRAW);

    // first pass
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vertex_pass0_data.size(), vertex_pass0_data.data(), GL_STREAM_DRAW);
    glUniform1i(render_pass_location, 0);
    glDrawArrays(GL_TRIANGLES, 0, vertex_pass0_data.size() / 4);

    // second pass
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vertex_pass1_data.size(), vertex_pass1_data.data(), GL_STREAM_DRAW);
    glUniform1i(render_pass_location, 1);
    glDrawArrays(GL_TRIANGLES, 0, vertex_pass1_data.size() / 4);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFlush();
    glFinish();
    AfterDraw();
}


static void *RenderWorker(void *) {
    pthread_setname_np(pthread_self(), "render worker");

    BeforeDraw();

    // build vertex and fragment shader
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    char const *vertex_source = "#version 320 es\n"
                                "\n"
                                "in vec4 vertex;\n"
                                "in vec3 textColor;\n"
                                "in vec3 backgroundColor;\n"
                                "out vec2 texCoords;\n"
                                "out vec3 fragTextColor;\n"
                                "out vec3 fragBackgroundColor;\n"
                                "uniform vec2 surface;\n"
                                "void main() {\n"
                                "  gl_Position.x = vertex.x / surface.x * 2.0f - 1.0f;\n"
                                "  gl_Position.y = vertex.y / surface.y * 2.0f - 1.0f;\n"
                                "  gl_Position.z = 0.0;\n"
                                "  gl_Position.w = 1.0;\n"
                                "  texCoords = vertex.zw;\n"
                                "  fragTextColor = textColor;\n"
                                "  fragBackgroundColor = backgroundColor;\n"
                                "}";
    glShaderSource(vertex_shader_id, 1, &vertex_source, NULL);
    glCompileShader(vertex_shader_id);

    int info_log_length;
    glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> vertex_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(vertex_shader_id, info_log_length, NULL, &vertex_shader_error_message[0]);
        OH_LOG_ERROR(LOG_APP, "Failed to build vertex shader: %{public}s", &vertex_shader_error_message[0]);
    }

    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    char const *fragment_source = "#version 320 es\n"
                                  "\n"
                                  "precision lowp float;\n"
                                  "in vec2 texCoords;\n"
                                  "in vec3 fragTextColor;\n"
                                  "in vec3 fragBackgroundColor;\n"
                                  "out vec4 color;\n"
                                  "uniform sampler2D text;\n"
                                  "uniform int renderPass;\n"
                                  "void main() {\n"
                                  "  if (renderPass == 0) {\n"
                                  "    color = vec4(fragBackgroundColor, 1.0);\n"
                                  "  } else {\n"
                                  "    float alpha = texture(text, texCoords).r;\n"
                                  "    color = vec4(fragTextColor, 1.0) * alpha;\n"
                                  "  }\n"
                                  "}";
    // blending is done by opengl (GL_ONE + GL_ONE_MINUS_SRC_ALPHA):
    // final = src * 1 + dest * (1 - src.a)
    // first pass: src = (fragBackgroundColor, 1.0), dest = (1.0, 1.0, 1.0, 1.0), final = (fragBackgroundColor, 1.0)
    // second pass: src = (fragTextColor * alpha, alpha), dest = (fragBackgroundColor, 1.0), final = (fragTextColor *
    // alpha + fragBackgroundColor * (1 - alpha), 1.0)
    glShaderSource(fragment_shader_id, 1, &fragment_source, NULL);
    glCompileShader(fragment_shader_id);

    glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> fragment_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL, &fragment_shader_error_message[0]);
        OH_LOG_ERROR(LOG_APP, "Failed to build fragment shader: %{public}s", &fragment_shader_error_message[0]);
    }

    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);

    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> link_program_error_message(info_log_length + 1);
        glGetProgramInfoLog(program_id, info_log_length, NULL, &link_program_error_message[0]);
        OH_LOG_ERROR(LOG_APP, "Failed to link program: %{public}s", &link_program_error_message[0]);
    }

    surface_location = glGetUniformLocation(program_id, "surface");
    assert(surface_location != -1);

    render_pass_location = glGetUniformLocation(program_id, "renderPass");
    assert(render_pass_location != -1);

    glUseProgram(program_id);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // load font from ttf for the initial characters
    glGenTextures(1, &texture_id);
    // load common characters initially
    for (uint32_t i = 0; i < 128; i++) {
        codepoints_to_load.insert(i);
    }
    LoadFont();

    // create buffers for drawing
    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);

    // vec4 vertex
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    GLint vertex_location = glGetAttribLocation(program_id, "vertex");
    assert(vertex_location != -1);
    glEnableVertexAttribArray(vertex_location);
    glVertexAttribPointer(vertex_location,   // attribute 0
                          4,                 // size
                          GL_FLOAT,          // type
                          GL_FALSE,          // normalized?
                          4 * sizeof(float), // stride
                          (void *)0          // array buffer offset
    );

    // vec3 textColor
    glGenBuffers(1, &text_color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, text_color_buffer);
    GLint text_color_location = glGetAttribLocation(program_id, "textColor");
    assert(text_color_location != -1);
    glEnableVertexAttribArray(text_color_location);
    glVertexAttribPointer(text_color_location, // attribute 0
                          3,                   // size
                          GL_FLOAT,            // type
                          GL_FALSE,            // normalized?
                          3 * sizeof(float),   // stride
                          (void *)0            // array buffer offset
    );

    // vec3 backgroundColor
    glGenBuffers(1, &background_color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, background_color_buffer);
    GLint background_color_location = glGetAttribLocation(program_id, "backgroundColor");
    assert(background_color_location != -1);
    glEnableVertexAttribArray(background_color_location);
    glVertexAttribPointer(background_color_location, // attribute 0
                          3,                         // size
                          GL_FLOAT,                  // type
                          GL_FALSE,                  // normalized?
                          3 * sizeof(float),         // stride
                          (void *)0                  // array buffer offset
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t last_redraw_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    uint64_t last_fps_msec = last_redraw_msec;
    Draw();
    int fps = 0;
    std::vector<uint64_t> time;
    while (1) {
        gettimeofday(&tv, nullptr);
        uint64_t now_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;

        // even if we call faster than system settings (60Hz/120Hz), it does not get faster
        // 120 Hz, 8ms
        uint64_t deadline = last_redraw_msec + 8;
        if (now_msec < deadline) {
            usleep((deadline - now_msec) * 1000);
        }

        // redraw
        gettimeofday(&tv, nullptr);
        now_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        last_redraw_msec = now_msec;
        Draw();

        gettimeofday(&tv, nullptr);
        uint64_t msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        time.push_back(msec - now_msec);

        fps++;

        // report fps
        if (now_msec - last_fps_msec > 1000) {
            last_fps_msec = now_msec;
            uint64_t sum = 0;
            for (auto t : time) {
                sum += t;
            }
            OH_LOG_INFO(LOG_APP, "FPS: %{public}d, %{public}ld ms per draw", fps, sum / fps);
            fps = 0;
            time.clear();
        }

        if (need_reload_font) {
            LoadFont();
        }
    }
}


// on resize
void Resize(int new_width, int new_height) {
    pthread_mutex_lock(&term.lock);
    width = new_width;
    height = new_height;

    ResizeTo(height / font_height, width / font_width, false);
    pthread_mutex_unlock(&term.lock);
}

// handle scrolling
void ScrollBy(double offset) {
    pthread_mutex_lock(&term.lock);
    // natural scrolling
    scroll_offset -= offset;
    if (scroll_offset < 0) {
        scroll_offset = 0.0;
    }
    pthread_mutex_unlock(&term.lock);
}

// start render thread
void StartRender() {
    pthread_t render_thread;
    pthread_create(&render_thread, NULL, RenderWorker, NULL);
}

#ifdef STANDALONE
#include <GLFW/glfw3.h>
static GLFWwindow *window;

void BeforeDraw() {
    glfwMakeContextCurrent(window);
}

void AfterDraw() {
    glfwSwapBuffers(window);
}

void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                  int mode) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_ENTER) {
            uint8_t data[] = {0x0d};
            SendData(data, 1);
        } else if (key == GLFW_KEY_BACKSPACE) {
            uint8_t data[] = {0x7f};
            SendData(data, 1);
        } else if (key == GLFW_KEY_UP) {
            uint8_t data[] = {0x1b, 0x5b, 0x41};
            SendData(data, 3);
        } else if (key == GLFW_KEY_DOWN) {
            uint8_t data[] = {0x1b, 0x5b, 0x42};
            SendData(data, 3);
        } else if (key == GLFW_KEY_RIGHT) {
            uint8_t data[] = {0x1b, 0x5b, 0x43};
            SendData(data, 3);
        } else if (key == GLFW_KEY_LEFT) {
            uint8_t data[] = {0x1b, 0x5b, 0x44};
            SendData(data, 3);
        } else if (key == GLFW_KEY_TAB) {
            uint8_t data[] = {0x09};
            SendData(data, 1);
        }
    }
}

void CharCallback(GLFWwindow* window, uint32_t codepoint) {
    // TODO: encode utf8 properly
    SendData((uint8_t *)&codepoint, 1);
}


void Copy(std::string base64) {
    // TODO
}

void RequestPaste() {
    // TODO
}

std::string GetPaste() {
    // TODO
    return "";
}

#ifdef TESTING

void ResizeWidth(int new_width) {
    // Do nothing
}

#else
void ResizeWidth(int new_width) {
    int current_width, current_height;
    glfwGetWindowSize(window, &current_width, &current_height);
    glfwSetWindowSize(window, new_width, current_height);
}

int main() {
    // Init GLFW
    glfwInit();

    // Set all the required options for GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Create a GLFWwindow object that we can use for GLFW's functions
    int window_width = 80 * font_width;
    int window_height = 30 * font_height;
    window =
        glfwCreateWindow(window_width, window_height, "Terminal", nullptr, nullptr);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCharCallback(window, CharCallback);

    Start();
    StartRender();
    Resize(window_width, window_height);
    while (!glfwWindowShouldClose(window)) {
        // Check if any events have been activated (key pressed, mouse moved etc.)
        // and call corresponding response functions
        glfwPollEvents();
    }
}
#endif
#endif
