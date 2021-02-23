/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2021  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "core_equations.h"
#include "core_display.h"
#include "core_helpers.h"
#include "core_main.h"
#include "shell.h"

static bool active = false;
static int menu_whence;

static vartype_realmatrix *eqns;
static int4 num_eqns;
static int selected_row = -1; // -1: top of list; num_eqns: bottom of list
static int edit_pos; // -1: in list; >= 0: in editor
static int display_pos;
static int edit_menu; // MENU_NONE = the navigation menu
static char *edit_buf;
static int4 edit_len, edit_capacity;
static bool cursor_on;

static int timeout_action = 0;
static int dir = 0;
static int r1, r2;

bool unpersist_eqn() {
    return true;
}

bool persist_eqn() {
    return true;
}

static bool is_name_char(char c) {
    /* The non-name characters are the same as on the 17B,
     * plus not-equal, less-equal, greater-equal, and
     * square brackets.
     */
    return c != '+' && c != '-' && c != 1 /* mul */
        && c != 0 /* div */     && c != '('
        && c != ')' && c != '<' && c != '>'
        && c != '^' && c != ':' && c != '=' && c != ' '
        && c != 12 /* NE */ && c != 9 /* LE */
        && c != 11 /* GE */ && c != '['
        && c != ']';
}

static void restart_cursor() {
    timeout_action = 2;
    cursor_on = true;
    shell_request_timeout3(500);
}

static void erase_cursor() {
    if (cursor_on) {
        cursor_on = false;
        char c = edit_pos == edit_len ? ' ' : edit_buf[edit_pos];
        draw_char(edit_pos - display_pos, 0, c);
    }
}

static void insert_text(const char *text, int len) {
    if (edit_len + len > edit_capacity) {
        int newcap = edit_capacity + 32;
        char *newbuf = (char *) realloc(edit_buf, newcap);
        if (newbuf == NULL) {
            squeak();
            return;
        }
        edit_buf = newbuf;
        edit_capacity = newcap;
    }
    memmove(edit_buf + edit_pos + len, edit_buf + edit_pos, edit_len - edit_pos);
    memcpy(edit_buf + edit_pos, text, len);
    edit_len += len;
    edit_pos += len;
    while (edit_pos - display_pos > 21)
        display_pos++;
    if (edit_pos == 21 && edit_pos < edit_len - 1)
        display_pos++;
}

static void update_menu(int menuid) {
    edit_menu = menuid;
    int multirow = edit_menu != MENU_NONE && menus[edit_menu].next != MENU_NONE;
    shell_annunciators(multirow, -1, -1, -1, -1, -1);
}

int eqn_start(int whence) {
    active = true;
    menu_whence = whence;
    set_shift(false);
    
    vartype *v = recall_var("EQNS", 4);
    if (v == NULL) {
        eqns = NULL;
        num_eqns = 0;
    } else if (v->type != TYPE_REALMATRIX) {
        active = false;
        return ERR_INVALID_TYPE;
    } else {
        eqns = (vartype_realmatrix *) v;
        num_eqns = eqns->rows * eqns->columns;
    }
    if (selected_row > num_eqns)
        selected_row = num_eqns;
    edit_pos = -1;
    eqn_draw();
    return ERR_NONE;
}

void eqn_end() {
    active = false;
}

bool eqn_draw() {
    if (!active)
        return false;
    clear_display();
    if (edit_pos == -1) {
        if (selected_row == -1) {
            draw_string(0, 0, "<Top of List>", 13);
        } else if (selected_row == num_eqns) {
            draw_string(0, 0, "<Bottom of List>", 16);
        } else {
            char buf[22];
            int4 len;
            if (eqns->array->is_string[selected_row]) {
                const char *text;
                get_matrix_string(eqns, selected_row, &text, &len);
                int bufptr = 0;
                string2buf(buf, 22, &bufptr, text, len);
            } else {
                len = easy_phloat2string(eqns->array->data[selected_row], buf, 22, 0);
            }
            draw_string(0, 0, buf, len);
        }
        draw_key(0, 0, 0, "CALC", 4);
        draw_key(1, 0, 0, "EDIT", 4);
        draw_key(2, 0, 0, "DELET", 5);
        draw_key(3, 0, 0, "NEW", 3);
        draw_key(4, 0, 0, "^", 1, true);
        draw_key(5, 0, 0, "\016", 1, true);
    } else {
        int len = edit_len - display_pos;
        if (len > 22) {
            draw_char(21, 0, 26);
            len = 21;
        }
        int off = 0;
        if (display_pos > 0) {
            draw_char(0, 0, 26);
            off = 1;
        }
        if (cursor_on) {
            int cpos = edit_pos - display_pos;
            if (cpos > off)
                draw_string(off, 0, edit_buf + display_pos + off, cpos - off);
            draw_char(cpos, 0, 255);
            if (cpos < len)
                draw_string(cpos + 1, 0, edit_buf + display_pos + cpos + 1, len - cpos - 1);
        } else {
            draw_string(off, 0, edit_buf + display_pos + off, len - off);
        }
        if (edit_menu == MENU_NONE) {
            draw_key(0, 0, 0, "DEL", 3);
            draw_key(1, 0, 0, "<\020", 2);
            draw_key(2, 0, 0, "\020", 1);
            draw_key(3, 0, 0, "\017", 1);
            draw_key(4, 0, 0, "\017>", 2);
            draw_key(5, 0, 0, "ALPHA", 5);
        } else {
            const menu_item_spec *mi = menus[edit_menu].child;
            for (int i = 0; i < 6; i++) {
                draw_key(i, 0, 0, mi[i].title, mi[i].title_length);
            }
        }
    }
    flush_display();
    return true;
}

static int keydown_list(int key, bool shift, int *repeat);
static int keydown_edit(int key, bool shift, int *repeat);

int eqn_keydown(int key, int *repeat) {
    if (!active)
        return 0;

    if (key == KEY_SHIFT) {
        set_shift(!mode_shift);
        return 1;
    }
    
    bool shift = mode_shift;
    set_shift(false);
    
    if (edit_pos == -1)
        return keydown_list(key, shift, repeat);
    else
        return keydown_edit(key, shift, repeat);
}

static int keydown_list(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_UP: {
            if (shift) {
                selected_row = -1;
                eqn_draw();
            } else if (selected_row >= 0) {
                selected_row--;
                dir = -1;
                *repeat = 3;
                eqn_draw();
            }
            return 1;
        }
        case KEY_DOWN: {
            if (shift) {
                selected_row = num_eqns;
                eqn_draw();
            } else if (selected_row < num_eqns) {
                selected_row++;
                dir = 1;
                *repeat = 3;
                eqn_draw();
            }
            return 1;
        }
        case KEY_SIGMA: {
            /* CALC */
            if (selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            const char *name;
            int4 len = 0;
            if (eqns->array->is_string[selected_row]) {
                get_matrix_string(eqns, selected_row, &name, &len);
                int4 i = 0;
                while (i < len && is_name_char(name[i]))
                    i++;
                if (i > 0 && i < len && name[i] == ':')
                    len = i;
            }
            if (len != 0 && len <= 7) {
                pending_command_arg.length = len;
                memcpy(pending_command_arg.val.text, name, len);
            } else {
                /* For equations with no name, or with a name longer
                    * than 7 characters, we generate a pseudo-name
                    * "eq{NNN}", but only if NNN <= 999. If the number
                    * is >= 1000, we punt.
                    */
                if (selected_row >= 1000) {
                    squeak();
                    return 1;
                }
                len = 0;
                string2buf(pending_command_arg.val.text, 7, &len, "eq{", 3);
                len += int2string(selected_row, pending_command_arg.val.text + len, 7 - len);
                string2buf(pending_command_arg.val.text, 7, &len, "}", 1);
            }
            pending_command_arg.type = ARGTYPE_STR;
            pending_command_arg.length = len;
            if (menu_whence == CATSECT_PGM_SOLVE)
                pending_command = CMD_PGMSLVi;
            else if (menu_whence == CATSECT_PGM_INTEG)
                pending_command = CMD_PGMINTi;
            else
                /* PGMMENU */
                pending_command = CMD_PMEXEC;
            /* Note that we don't do active = false here, since at this point
                * it is still possible that the command will go do NULL, and in
                * that case, we should stay here. Thus, setting active = false
                * is accomplished by PGMSLV, PGMINT, and PMEXEC.
                */
            redisplay();
            return 2;
        }
        case KEY_INV: {
            /* EDIT */
            if (selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            if (eqns->array->is_string[selected_row]) {
                const char *text;
                int4 len;
                get_matrix_string(eqns, selected_row, &text, &len);
                edit_buf = (char *) malloc(len); // TODO: Error handling
                edit_len = edit_capacity = len;
                memcpy(edit_buf, text, len);
            } else {
                char buf[50];
                // TODO: This isn't the right call; what you want
                // is something that enforces ALL. And what about thousands
                // separators? I'm thinking no.
                int4 len = easy_phloat2string(eqns->array->data[selected_row], buf, 50, 0);
                edit_buf = (char *) malloc(len); // TODO: Error handling
                edit_len = edit_capacity = len;
                memcpy(edit_buf, buf, len);
            }
            edit_pos = 0;
            display_pos = 0;
            update_menu(MENU_NONE);
            restart_cursor();
            eqn_draw();
            return 1;
        }
        case KEY_SQRT: {
            /* DELET */
            squeak();
            return 1;
        }
        case KEY_LOG: {
            /* NEW */
            squeak();
            return 1;
        }
        case KEY_LN:
        case KEY_XEQ: {
            /* MOVE up, MOVE down */
            if (selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            if (key == KEY_LN) {
                /* up */
                if (selected_row == 0) {
                    r1 = -1;
                } else {
                    r1 = selected_row;
                    r2 = selected_row - 1;
                }
                selected_row--;
            } else {
                /* down */
                if (selected_row == num_eqns - 1) {
                    r1 = -2;
                } else {
                    r1 = selected_row;
                    r2 = selected_row + 1;
                }
                selected_row++;
            }

            eqn_draw();
            timeout_action = 1;
            shell_request_timeout3(500);
            return 1;
        }
        case KEY_EXIT: {
            if (shift)
                /* Power off -- TODO */;

            active = false;
            redisplay();
            return 2;
        }
        default: {
            squeak();
            return 1;
        }
    }
}

static int keydown_edit(int key, bool shift, int *repeat) {
    if (key >= KEY_SIGMA && key <= KEY_XEQ) {
        /* Menu keys */
        if (edit_menu == MENU_NONE) {
            /* Navigation menu */
            switch (key) {
                case KEY_SIGMA: {
                    if (edit_len > 0 && edit_pos < edit_len) {
                        memmove(edit_buf + edit_pos, edit_buf + edit_pos + 1, edit_len - edit_pos - 1);
                        edit_len--;
                        if (display_pos + 21 > edit_len && display_pos > 0)
                            display_pos--;
                        dir = 2;
                        *repeat = 2;
                        restart_cursor();
                        eqn_draw();
                    }
                    return 1;
                }
                case KEY_INV: {
                    /* <<- */
                    if (shift)
                        goto left;
                    int dpos = edit_pos - display_pos;
                    int off = display_pos > 0 ? 1 : 0;
                    if (dpos > off) {
                        edit_pos = display_pos + off;
                    } else {
                        edit_pos -= 20;
                        if (edit_pos < 0)
                            edit_pos = 0;
                        display_pos = edit_pos - 1;
                        if (display_pos < 0)
                            display_pos = 0;
                    }
                    restart_cursor();
                    eqn_draw();
                    return 1;
                }
                case KEY_SQRT: {
                    /* <- */
                    left:
                    if (edit_pos > 0) {
                        if (shift) {
                            edit_pos = 0;
                        } else {
                            edit_pos--;
                            dir = -1;
                            *repeat = 2;
                        }
                        while (true) {
                            int dpos = edit_pos - display_pos;
                            if (dpos > 0 || display_pos == 0 && dpos == 0)
                                break;
                            display_pos--;
                        }
                        restart_cursor();
                        eqn_draw();
                    }
                    return 1;
                }
                case KEY_LOG: {
                    /* -> */
                    right:
                    if (edit_pos < edit_len) {
                        if (shift) {
                            edit_pos = edit_len;
                        } else {
                            edit_pos++;
                            dir = 1;
                            *repeat = 2;
                        }
                        while (true) {
                            int dpos = edit_pos - display_pos;
                            if (dpos < 21 || display_pos + 22 >= edit_len && dpos == 21)
                                break;
                            display_pos++;
                        }
                        restart_cursor();
                        eqn_draw();
                    }
                    return 1;
                }
                case KEY_LN: {
                    /* ->> */
                    if (shift)
                        goto right;
                    int dpos = edit_pos - display_pos;
                    if (edit_len - display_pos > 22) {
                        /* There's an ellipsis in the right margin */
                        if (dpos < 20) {
                            edit_pos = display_pos + 20;
                        } else {
                            edit_pos += 20;
                            display_pos += 20;
                            if (edit_pos > edit_len) {
                                edit_pos = edit_len;
                                display_pos = edit_pos - 21;
                            }
                        }
                    } else {
                        edit_pos = edit_len;
                        display_pos = edit_pos - 21;
                        if (display_pos < 0)
                            display_pos = 0;
                    }
                    restart_cursor();
                    eqn_draw();
                    return 1;
                }
                case KEY_XEQ: {
                    /* ALPHA */
                    update_menu(MENU_ALPHA1);
                    eqn_draw();
                    return 1;
                }
            }
        } else {
            /* ALPHA menu */
            if (edit_menu == MENU_ALPHA1 || edit_menu == MENU_ALPHA2) {
                update_menu(menus[edit_menu].child[key - 1].menuid);
                eqn_draw();
                return 1;
            } else {
                char c = menus[edit_menu].child[key - 1].title[0];
                if (shift && c >= 'A' && c <= 'Z')
                    c += 32;
                update_menu(menus[edit_menu].parent);
                insert_text(&c, 1);
                eqn_draw();
                return 1;
            }
        }
    } else {
        /* Rest of keyboard */
        switch (key) {
            case KEY_UP:
            case KEY_DOWN: {
                /* No need to handle Up and Down separately, since none of the
                 * menus we're using here have more than two rows.
                 */
                if (edit_menu != MENU_NONE && menus[edit_menu].next != MENU_NONE) {
                    update_menu(menus[edit_menu].next);
                    eqn_draw();
                } else
                    squeak();
                break;
            }
            case KEY_EXIT: {
                if (edit_menu == MENU_NONE) {
                    edit_pos = -1;
                    free(edit_buf);
                } else {
                    update_menu(menus[edit_menu].parent);
                    eqn_draw();
                }
                eqn_draw();
                break;
            }
            default: {
                squeak();
                break;
            }
        }
    }
    return 1;
}

int eqn_repeat() {
    if (!active)
        return -1;
    // Like core_repeat(): 0 means stop repeating; 1 means slow repeat,
    // for Up/Down; 2 means fast repeat, for text entry; 3 means extra
    // slow repeat, for the equation editor's list view.
    if (edit_pos == -1) {
        if (dir == -1) {
            if (selected_row >= 0) {
                selected_row--;
                eqn_draw();
                return 3;
            } else {
                dir = 0;
            }
        } else if (dir == 1) {
            if (selected_row < num_eqns) {
                selected_row++;
                eqn_draw();
                return 3;
            } else {
                dir = 0;
            }
        }
    } else {
        int repeat = 0;
        keydown_edit(dir == 2 ? KEY_SIGMA : dir == -1 ? KEY_SQRT : KEY_LOG, false, &repeat);
        if (repeat == 0)
            dir = 0;
        else
            return 2;
    }
    return 0;
}

bool eqn_timeout() {
    if (!active)
        return false;

    int action = timeout_action;
    timeout_action = 0;

    if (action == 1) {
        /* Finish delayed Move Up/Down operation */
        if (edit_pos != -1)
            return true;
        if (r1 == -1) {
            selected_row++;
        } else if (r1 == -2) {
            selected_row--;
        } else {
            bool t1 = eqns->array->is_string[r1];
            eqns->array->is_string[r1] = eqns->array->is_string[r2];
            eqns->array->is_string[r2] = t1;
            phloat t2 = eqns->array->data[r1];
            eqns->array->data[r1] = eqns->array->data[r2];
            eqns->array->data[r2] = t2;
        }
        eqn_draw();
    } else if (action == 2) {
        /* Cursor blinking */
        if (edit_pos == -1)
            return true;
        cursor_on = !cursor_on;
        char c = cursor_on ? 255 : edit_pos == edit_len ? ' ' : edit_buf[edit_pos];
        draw_char(edit_pos - display_pos, 0, c);
        flush_display();
        timeout_action = 2;
        shell_request_timeout3(500);
    }
    return true;
}
