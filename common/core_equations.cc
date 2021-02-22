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

#include <string.h>

#include "core_equations.h"
#include "core_display.h"
#include "core_helpers.h"
#include "shell.h"

static bool active = false;
static int menu_whence;

static vartype_realmatrix *eqns;
static int4 num_eqns;
static int selected_row = -1; // -1: top of list; num_eqns: bottom of list
static int edit_pos; // -1: in list

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

int eqn_start(int whence) {
    active = true;
    menu_whence = whence;
    
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
    } else {
        draw_string(0, 0, "Welcome to EQN", 14);
    }
    draw_key(0, 0, 0, "CALC", 4);
    draw_key(1, 0, 0, "EDIT", 4);
    draw_key(2, 0, 0, "DELET", 5);
    draw_key(3, 0, 0, "NEW", 3);
    draw_key(4, 0, 0, "^", 1, true);
    draw_key(5, 0, 0, "\016", 1, true);
    flush_display();
    return true;
}

int eqn_keydown(int key, int *repeat) {
    if (!active)
        return 0;
    
    if (edit_pos == -1) {
        switch (key) {
            case KEY_UP: {
                if (selected_row >= 0) {
                    selected_row--;
                    dir = -1;
                    *repeat = 1;
                } else
                    squeak();
                break;
            }
            case KEY_DOWN: {
                if (selected_row < num_eqns) {
                    selected_row++;
                    dir = 1;
                    *repeat = 1;
                } else
                    squeak();
                break;
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
                    pending_command = flags.f.prgm_mode ? CMD_PGMSLV
                                                        : CMD_PGMSLVi;
                else if (menu_whence == CATSECT_PGM_INTEG)
                    pending_command = flags.f.prgm_mode ? CMD_PGMINT
                                                        : CMD_PGMINTi;
                else
                    /* PGMMENU */
                    pending_command = CMD_PMEXEC;
                goto done;
            }
            case KEY_INV: {
                /* EDIT */
                squeak();
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
                if (key == KEY_LN) {
                    /* up */
                    if (selected_row < 1 || selected_row == num_eqns) {
                        squeak();
                        return 1;
                    }
                    r1 = selected_row;
                    r2 = selected_row - 1;
                    selected_row--;
                } else {
                    /* down */
                    if (selected_row > num_eqns - 2 || selected_row == -1) {
                        squeak();
                        return 1;
                    }
                    r1 = selected_row;
                    r2 = selected_row + 1;
                    selected_row++;
                }

                eqn_draw();
                shell_request_timeout3(500);
                return 1;
            }
            case KEY_EXIT: {
                /* handled further down */
                break;
            }
            default: {
                squeak();
                break;
            }
        }
    }


    if (key == KEY_EXIT) {
        done:
        active = false;
        // return 2 indicates the caller should request the CPU and retake control
        redisplay();
        return 2;
    } else {
        // return 1 indicates the caller should do nothing further
        redisplay();
        return 1;
    }
}

int eqn_repeat() {
    if (!active)
        return -1;
    // Like core_repeat(): 0 means stop repeating; 1 means slow repeat,
    // for Up/Down; 2 means fast repeat, for text entry
    if (dir == -1) {
        if (selected_row >= 0) {
            selected_row--;
            eqn_draw();
            return 1;
        } else {
            squeak();
            dir = 0;
        }
    } else if (dir == 1) {
        if (selected_row < num_eqns) {
            selected_row++;
            eqn_draw();
            return 1;
        } else {
            squeak();
            dir = 0;
        }
    }
    return 0;
}

bool eqn_timeout() {
    if (!active)
        return false;

    /* Finish delayed Move Up/Down operation */
    bool t1 = eqns->array->is_string[r1];
    eqns->array->is_string[r1] = eqns->array->is_string[r2];
    eqns->array->is_string[r2] = t1;
    phloat t2 = eqns->array->data[r1];
    eqns->array->data[r1] = eqns->array->data[r2];
    eqns->array->data[r2] = t2;
    eqn_draw();
    return true;
}
