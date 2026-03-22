//ncalc - ncurses calculator & CAS
//by Jonathan Torres
//
//This program is free software: you can redistribute it and/or modify it under the terms of the 
//GNU General Public License as published by the Free Software Foundation, either version 3 of the 
//License, or (at your option) any later version.


#include <ncurses.h>
#include <ginac/ginac.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cln/exception.h>
#include <clocale>
#include <algorithm>

using namespace std;
using namespace GiNaC;

//Constants & Enums
enum AppMode {
    MODE_CALCULATOR,
    MODE_GRAPH
};

#define CTRL(c) ((c) & 0x1f)

const int KEY_CTRL_G = CTRL('g');
const int KEY_CTRL_P = CTRL('p');
const int KEY_CTRL_F = CTRL('f'); // Find
const int KEY_CTRL_W = CTRL('w'); // Where
const int KEY_CTRL_E = CTRL('e');
const int KEY_CTRL_N = CTRL('n'); // Imaginary unit
const int KEY_CTRL_D = CTRL('d'); // Derivative shortcut

//Global State
AppMode current_mode = MODE_CALCULATOR;
string current_input = "";
string last_result = "";
bool has_result = false;
ex memory_val = 0;
symtab table;

struct HistoryEntry {
    string expr;
    string res;
};
vector<HistoryEntry> history;
bool history_focus = false;
int scroll_offset = 0;

//Graphing State
double g_x_min = -10.0;
double g_x_max = 10.0;
double g_y_min = -10.0;
double g_y_max = 10.0;

//Forward Declarations
string format_result(const ex &e);
void calculate();
void draw_history(WINDOW* win);
void draw_graph(WINDOW* win);
void draw_input(WINDOW* win);
void apply_zoom(double factor);
string sanitize_expr(const string& in);

/**
 * Sanitizes the input:
 * 1. Brackets [] -> ()
 * 2. Implicit multiplication
 */
string sanitize_expr(const string& in) {
    string s = in;
    for (char &c : s) {
        if (c == '[') c = '(';
        if (c == ']') c = ')';
    }
    
    string res_impl = "";
    for (size_t i = 0; i < s.length(); ++i) {
        res_impl += s[i];
        if (i + 1 < s.length()) {
            char curr = s[i];
            char next = s[i+1];
            if ((isdigit(curr) && (isalpha(next) || next == '(')) ||
                (curr == ')' && (isdigit(next) || isalpha(next)))) {
                res_impl += '*';
            }
        }
    }
    return res_impl;
}

string format_result(const ex &e) {
    if (is_a<numeric>(e)) {
        numeric n = ex_to<numeric>(e);
        if (n.is_integer()) {
            ostringstream ss;
            ss << n;
            return ss.str();
        }
        if (n.is_real()) {
            double d = n.to_double();
            if (abs(d) >= 1e10 || (abs(d) > 0 && abs(d) < 1e-4)) {
                ostringstream ss;
                ss << scientific << setprecision(6) << d;
                return ss.str();
            }
            string s = to_string(d);
            if (s.find('.') != string::npos) {
                s.erase(s.find_last_not_of('0') + 1, string::npos);
                if (s.back() == '.') s.pop_back();
            }
            return s;
        }
    }
    ostringstream ss;
    ss << e;
    return ss.str();
}

/**
 * Solves equations or simplifies expressions.
 */
void calculate() {
    if (current_input.empty()) return;
    
    string input = current_input;
    try {
        parser reader(table);
        ex final_res;
        bool found_custom = false;

        // 1. Handle "where" syntax for derivatives or general substitution
        size_t where_pos = input.find(" where ");
        if (where_pos != string::npos) {
            string lhs_part = input.substr(0, where_pos);
            if (lhs_part.find("find ") == 0) lhs_part = lhs_part.substr(5);
            string rhs_part = input.substr(where_pos + 7);
            
            if (lhs_part.find("/d") != string::npos) {
                size_t slash = lhs_part.find("/d");
                size_t paren = lhs_part.find('(', slash);
                if (slash != string::npos && paren != string::npos) {
                    string var_name = lhs_part.substr(slash + 2, paren - (slash + 2));
                    var_name.erase(remove(var_name.begin(), var_name.end(), ' '), var_name.end());
                    
                    size_t last_paren = lhs_part.find_last_of(')');
                    string pts_str = lhs_part.substr(paren + 1, last_paren - paren - 1);
                    
                    size_t eq_sign = rhs_part.find('=');
                    if (eq_sign != string::npos) {
                        string expr_str = sanitize_expr(rhs_part.substr(eq_sign + 1));
                        ex expr = reader(expr_str);
                        
                        if (table.find(var_name) != table.end()) {
                            ex diff_ex = expr.diff(ex_to<symbol>(table[var_name]));
                            
                            lst subs_list;
                            stringstream ss(pts_str);
                            string pt;
                            vector<string> var_order = {"x", "y", "z", "a", "b"};
                            int v_idx = 0;
                            while (getline(ss, pt, ',') && v_idx < (int)var_order.size()) {
                                subs_list.append(ex_to<symbol>(table[var_order[v_idx]]) == reader(sanitize_expr(pt)).evalf());
                                v_idx++;
                            }
                            final_res = diff_ex.subs(subs_list).evalf();
                            last_result = format_result(final_res);
                            found_custom = true;
                        }
                    }
                }
            }
        }
        
        // 2. Handle "find var, eq"
        if (!found_custom && input.find("find ") == 0) {
            int level = 0;
            size_t comma_pos = string::npos;
            for (size_t i = 0; i < input.length(); ++i) {
                if (input[i] == '(') level++;
                else if (input[i] == ')') level--;
                else if (input[i] == ',' && level == 0) {
                    comma_pos = i;
                    break;
                }
            }

            if (comma_pos != string::npos) {
                string var_name = input.substr(5, comma_pos - 5);
                var_name.erase(remove(var_name.begin(), var_name.end(), ' '), var_name.end());
                string eq_str = input.substr(comma_pos + 1);
                
                string sanitized_eq = sanitize_expr(eq_str);
                size_t eq_sign = sanitized_eq.find('=');
                ex eq_ex;
                if (eq_sign != string::npos) {
                    eq_ex = reader(sanitized_eq.substr(0, eq_sign)) - reader(sanitized_eq.substr(eq_sign + 1));
                } else {
                    eq_ex = reader(sanitized_eq);
                }
                
                if (table.find(var_name) == table.end()) table[var_name] = symbol(var_name);
                symbol target_var = ex_to<symbol>(table[var_name]);
                
                try {
                    final_res = lsolve(eq_ex == 0, target_var);
                    if (final_res.nops() > 0 || !is_a<lst>(final_res)) {
                        last_result = var_name + " = " + format_result(final_res.evalf());
                        found_custom = true;
                    }
                } catch (...) {
                    try {
                        // Try Quadratic formula as fallback
                        ex eq = eq_ex.expand();
                        if (eq.degree(target_var) == 2) {
                            ex a = eq.coeff(target_var, 2);
                            ex b = eq.coeff(target_var, 1);
                            ex c = eq.coeff(target_var, 0);
                            ex disc = (b*b - 4*a*c).normal();
                            ex r1 = ((-b + sqrt(disc)) / (2*a)).normal();
                            ex r2 = ((-b - sqrt(disc)) / (2*a)).normal();
                            last_result = var_name + " = {" + format_result(r1) + ", " + format_result(r2) + "}";
                            found_custom = true;
                        }
                    } catch (...) {}
                }

                if (!found_custom) {
                    // Newton's method fallback for univariate
                    try {
                        symbol x = target_var;
                        ex f = eq_ex;
                        ex df = f.diff(x);
                        vector<double> guesses = {1.0, 0.1, 10.0, -1.0, 100.0, 0.0};
                        for (double guess : guesses) {
                            double curr = guess;
                            bool converged = false;
                            for (int i = 0; i < 100; ++i) {
                                try {
                                    ex val_ex = f.subs(x == curr).evalf();
                                    ex dval_ex = df.subs(x == curr).evalf();
                                    if (is_a<numeric>(val_ex) && is_a<numeric>(dval_ex)) {
                                        double val = ex_to<numeric>(val_ex).to_double();
                                        double dval = ex_to<numeric>(dval_ex).to_double();
                                        if (abs(val) < 1e-10) { converged = true; break; }
                                        if (abs(dval) < 1e-18) break;
                                        double next_val = curr - val / dval;
                                        if (isnan(next_val) || isinf(next_val) || abs(next_val - curr) < 1e-14) {
                                            if (abs(val) < 1e-8) converged = true;
                                            break;
                                        }
                                        curr = next_val;
                                    } else break;
                                } catch (...) { break; }
                            }
                            if (converged) {
                                if (abs(curr - round(curr)) < 1e-9) curr = round(curr);
                                last_result = var_name + " ~ " + format_result(numeric(curr).evalf());
                                found_custom = true;
                                break;
                            }
                        }
                    } catch (...) {}
                }

                if (!found_custom) {
                    last_result = format_result(eq_ex.normal()) + " = 0";
                    found_custom = true;
                }
            }
        }

        // 3. Handle "i(u,l), expr dx"
        if (!found_custom && input.find("i(") == 0) {
            size_t end_paren = input.find("),");
            if (end_paren != string::npos) {
                string range = input.substr(2, end_paren - 2);
                size_t comma = range.find(',');
                double upper = ex_to<numeric>(reader(range.substr(0, comma)).evalf()).to_double();
                double lower = ex_to<numeric>(reader(range.substr(comma + 1)).evalf()).to_double();
                
                string body = input.substr(end_paren + 2);
                size_t d_pos = body.find(" d");
                string expr_str = sanitize_expr(body.substr(0, d_pos));
                string var_name = body.substr(d_pos + 2);
                var_name.erase(remove(var_name.begin(), var_name.end(), ' '), var_name.end());
                
                ex expr = reader(expr_str);
                if (table.find(var_name) == table.end()) table[var_name] = symbol(var_name);
                symbol x_sym = ex_to<symbol>(table[var_name]);
                
                // Numeric integration using Simpson's Rule
                double sum = 0;
                int n = 1000; 
                double step = (upper - lower) / n;
                try {
                    for (int j = 0; j <= n; ++j) {
                        double wx = lower + j * step;
                        double vy = ex_to<numeric>(expr.subs(x_sym == wx).evalf()).to_double();
                        if (j == 0 || j == n) sum += vy;
                        else if (j % 2 == 1) sum += 4 * vy;
                        else sum += 2 * vy;
                    }
                    double area = (step / 3.0) * sum;
                    last_result = "A = " + format_result(numeric(area));
                    found_custom = true;
                } catch (...) {
                    last_result = "Integration failed";
                }
            }
        }

        if (!found_custom) {
            string raw = sanitize_expr(input);
            size_t eq_pos = raw.find('=');
            if (eq_pos != string::npos) {
                string lhs_s = raw.substr(0, eq_pos);
                string rhs_s = raw.substr(eq_pos + 1);
                if (lhs_s == "y" || lhs_s == "f(x)") {
                    final_res = reader(rhs_s).evalf();
                    last_result = format_result(final_res);
                } else {
                    ex eq = reader(lhs_s) - reader(rhs_s);
                    last_result = format_result(eq.normal()) + " = 0";
                }
            } else {
                final_res = reader(raw).normal().evalf();
                last_result = format_result(final_res);
            }
        }

        has_result = true;
        history.push_back({current_input, last_result});
        current_input = last_result;
        
        int rows, cols; getmaxyx(stdscr, rows, cols);
        int max_entries = (rows - 5 - 2) / 2;
        scroll_offset = max(0, (int)history.size() - max_entries);

    } catch (exception &e) {
        last_result = "Error: " + string(e.what());
        has_result = false;
    } catch (...) {
        last_result = "Unknown Error";
        has_result = false;
    }
}

void draw_graph(WINDOW* win) {
    werase(win); box(win, 0, 0);
    int h, w; getmaxyx(win, h, w);
    int plot_w = w - 2, plot_h = h - 2;
    if (plot_w <= 0 || plot_h <= 0) return;

    mvwprintw(win, 0, 2, " Graph Mode: %s ", current_input.c_str());
    mvwprintw(win, h - 1, 2, " X: [%.1f, %.1f] Y: [%.1f, %.1f] ", g_x_min, g_x_max, g_y_min, g_y_max);

    int grid_w = plot_w * 2, grid_h = plot_h * 4;
    vector<vector<unsigned char>> grid(plot_h, vector<unsigned char>(plot_w, 0));

    auto set_dot = [&](int r, int c) {
        if (r >= 0 && r < grid_h && c >= 0 && c < grid_w) {
            grid[r/4][c/2] |= (unsigned char[]){0x01, 0x02, 0x04, 0x40, 0x08, 0x10, 0x20, 0x80}[(r%4) + (c%2)*4];
        }
    };

    // Axes
    if (g_y_min <= 0 && g_y_max >= 0) {
        int r = (int)((g_y_max) / (g_y_max - g_y_min) * grid_h);
        for (int c = 0; c < grid_w; ++c) set_dot(r, c);
    }
    if (g_x_min <= 0 && g_x_max >= 0) {
        int c = (int)((-g_x_min) / (g_x_max - g_x_min) * grid_w);
        for (int r = 0; r < grid_h; ++r) set_dot(r, c);
    }

    try {
        parser reader(table);
        string raw = sanitize_expr(current_input);
        size_t eq_pos = raw.find('=');
        ex func = reader(eq_pos != string::npos ? raw.substr(eq_pos + 1) : raw);
        symbol x_sym = ex_to<symbol>(table["x"]);

        double dx = (g_x_max - g_x_min) / grid_w;
        double dy = (g_y_max - g_y_min) / grid_h;

        for (int col = 0; col < grid_w; ++col) {
            double wx = g_x_min + col * dx;
            ex vy = func.subs(x_sym == wx).evalf();
            if (is_a<numeric>(vy) && ex_to<numeric>(vy).is_real()) {
                int row = (int)((g_y_max - ex_to<numeric>(vy).to_double()) / dy);
                set_dot(row, col);
            }
        }
    } catch (...) {}

    for (int r = 0; r < plot_h; ++r) {
        wmove(win, r + 1, 1);
        for (int c = 0; c < plot_w; ++c) {
            unsigned char v = grid[r][c];
            if (v == 0) waddch(win, ' ');
            else {
                char u[4] = {(char)0xE2, (char)(0xA0 + (v >> 6)), (char)(0x80 + (v & 0x3F)), 0};
                waddstr(win, u);
            }
        }
    }
    wrefresh(win);
}

void draw_history(WINDOW* win) {
    if (current_mode == MODE_GRAPH) { draw_graph(win); return; }
    werase(win); if (history_focus) wattron(win, A_REVERSE); box(win, 0, 0);
    mvwprintw(win, 0, 2, " History "); if (history_focus) wattroff(win, A_REVERSE);
    int h, w; getmaxyx(win, h, w);
    int max_e = (h - 2) / 2;
    for (int i = 0; i < max_e && (scroll_offset + i) < (int)history.size(); ++i) {
        int idx = scroll_offset + i;
        mvwprintw(win, 1 + i * 2, 2, "Exp: %s", history[idx].expr.c_str());
        mvwprintw(win, 2 + i * 2, 2, "Res: %s", history[idx].res.c_str());
    }
    wrefresh(win);
}

void draw_input(WINDOW* win) {
    werase(win); if (!history_focus) wattron(win, A_REVERSE); box(win, 0, 0);
    mvwprintw(win, 0, 2, current_mode == MODE_GRAPH ? " Graph Mode " : " Input ");
    if (!history_focus) wattroff(win, A_REVERSE);
    mvwprintw(win, 1, 2, "Exp: %s", current_input.c_str());
    if (current_mode == MODE_CALCULATOR) {
        mvwprintw(win, 2, 2, "Res: %s", last_result.c_str());
        mvwprintw(win, 3, 2, "[q] Quit | [Esc] Clear | [Ctrl+f] Find | [Ctrl+w] Where");
    } else {
        mvwprintw(win, 3, 2, "Zoom: [+/-] or [j/k] | [q] Quit | [Ctrl+g] Calc");
    }
    wrefresh(win);
}

void apply_zoom(double factor) {
    double xm = (g_x_min + g_x_max)/2, ym = (g_y_min + g_y_max)/2;
    double xh = (g_x_max - g_x_min)*factor/2, yh = (g_y_max - g_y_min)*factor/2;
    g_x_min = xm - xh; g_x_max = xm + xh; g_y_min = ym - yh; g_y_max = ym + yh;
}

void handle_input(int ch) {
    if (ch == KEY_CTRL_G) { current_mode = (current_mode == MODE_CALCULATOR ? MODE_GRAPH : MODE_CALCULATOR); return; }
    if (ch == '\t') { history_focus = !history_focus; return; }
    if (ch == KEY_CTRL_P) { current_input += "Pi"; return; }
    if (ch == KEY_CTRL_E) { current_input += "exp(1)"; return; }
    if (ch == KEY_CTRL_N) { current_input += "I"; return; }
    if (ch == KEY_CTRL_F) { current_input += "find "; return; }
    if (ch == KEY_CTRL_W) { current_input += " where "; return; }
    if (ch == KEY_CTRL_D) { current_input += "diff("; return; }

    if (current_mode == MODE_GRAPH) {
        if (ch == '+' || ch == '=' || ch == 'k' || ch == KEY_UP) { apply_zoom(0.8); return; }
        if (ch == '-' || ch == '_' || ch == 'j' || ch == KEY_DOWN) { apply_zoom(1.25); return; }
    }

    if (history_focus && current_mode == MODE_CALCULATOR) {
        int rows, cols; getmaxyx(stdscr, rows, cols);
        int max_e = (rows - 5 - 2) / 2;
        if (ch == KEY_UP && scroll_offset > 0) scroll_offset--;
        else if (ch == KEY_DOWN && scroll_offset < max(0, (int)history.size() - max_e)) scroll_offset++;
        return;
    }
    
    if (ch == '\n' || ch == KEY_ENTER) { if (current_mode == MODE_CALCULATOR) calculate(); return; }
    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) { if (!current_input.empty()) current_input.pop_back(); return; }
    if (ch == 27) { //Alt sequences
        nodelay(stdscr, TRUE); int n = getch(); nodelay(stdscr, FALSE);
        if (n == -1) { current_input = ""; last_result = ""; has_result = false; }
        else if (n == 's') current_input += "asin(";
        else if (n == 'c') current_input += "acos(";
        else if (n == 't') current_input += "atan(";
        else if (n == 'f') current_input += "f(";
        else if (n == 'u') current_input += "upper";
        else if (n == 'l') current_input += "lower";
        else if (n == 'e') current_input += "expr";
        else if (n == 'v') current_input += "dvar";
        return;
    }

    if (current_mode == MODE_CALCULATOR) {
        if (ch == 's') { current_input += "sin("; return; }
        if (ch == 'c') { current_input += "cos("; return; }
        if (ch == 't') { current_input += "tan("; return; }
        if (ch == 'i') { current_input += "i("; return; }
        if (ch == 'N') { if (!current_input.empty()) current_input = "-(" + current_input + ")"; return; }
        if (ch == 'S') { current_input += "sqrt("; return; }
        if (ch == 'Q') { current_input += "cbrt("; return; }
        if (ch == 'm') { 
            if (has_result) { try { parser r(table); memory_val = r(last_result).evalf(); } catch(...) {} }
            return; 
        }
        if (ch == 'n') { memory_val = 0; return; }
        if (ch == 'r') { current_input += format_result(memory_val); return; }
        if (ch == '%') { current_input = "(" + current_input + ")/100"; calculate(); return; }
    }

    if (ch >= ' ' && ch <= '~') current_input += (char)ch;
}

int main() {
    setlocale(LC_ALL, "");
    table["x"] = symbol("x"); table["y"] = symbol("y"); table["z"] = symbol("z");
    table["a"] = symbol("a"); table["b"] = symbol("b");
    table["upper"] = symbol("upper"); table["lower"] = symbol("lower");
    table["expr"] = symbol("expr"); table["dvar"] = symbol("dvar");
    table["Pi"] = Pi; table["I"] = I;
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    #if defined(ESCDELAY)
    ESCDELAY = 25;
    #endif
    int rows, cols; getmaxyx(stdscr, rows, cols);
    int input_h = 5;
    WINDOW *h_win = newwin(rows - input_h, cols, 0, 0), *i_win = newwin(input_h, cols, rows - input_h, 0);
    keypad(i_win, TRUE);
    while (true) {
        draw_history(h_win); draw_input(i_win);
        int ch = wgetch(i_win);
        if (ch == 'q') break;
        if (ch == KEY_RESIZE) {
            getmaxyx(stdscr, rows, cols);
            wresize(h_win, rows - input_h, cols); wresize(i_win, input_h, cols);
            mvwin(i_win, rows - input_h, 0); clear();
        } else handle_input(ch);
    }
    delwin(h_win); delwin(i_win); endwin();
    return 0;
}
