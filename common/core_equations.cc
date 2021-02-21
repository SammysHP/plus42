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

static bool active = false;
static int menu_whence;

static vartype_realmatrix *eqns;
static int4 num_eqns;
static int selected_row = -1; // -1: top of list; num_eqns: bottom of list
static int edit_pos; // -1: in list

bool unpersist_eqn() {
    return true;
}

bool persist_eqn() {
    return true;
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
    flush_display();
    return true;
}

int eqn_keydown(int key) {
    if (!active)
        return 0;
    
    if (edit_pos == -1) {
        if (key == KEY_UP) {
            if (selected_row >= 0) {
                selected_row--;
                redisplay();
            }
        } else if (key == KEY_DOWN) {
            if (selected_row < num_eqns) {
                selected_row++;
                redisplay();
            }
        }
    }

#if 0
    char name[10];
    int len = 0;
    if (key == KEY_SIN) {
        memcpy(name, "PLOT", 4);
        len = 4;
    } else if (key == KEY_COS) {
        memcpy(name, "SUN", 3);
        len = 3;
    } else if (key == KEY_TAN) {
        memcpy(name, "FOO", 3);
        len = 3;
    }
    if (len != 0) {
        if (menu_whence == CATSECT_PGM_SOLVE)
            pending_command = flags.f.prgm_mode ? CMD_PGMSLV
                                                : CMD_PGMSLVi;
        else if (menu_whence == CATSECT_PGM_INTEG)
            pending_command = flags.f.prgm_mode ? CMD_PGMINT
                                                : CMD_PGMINTi;
        else
            /* PGMMENU */
            pending_command = CMD_PMEXEC;
        pending_command_arg.type = ARGTYPE_STR;
        pending_command_arg.length = len;
        memcpy(pending_command_arg.val.text, name, len);
        goto done;
    }
#endif

    if (key == KEY_EXIT) {
        done:
        active = false;
        redisplay();
        return 2;
    } else {
        return 1;
    }

    // Return 1 when eqn continues (no need to request the CPU because we'll continue to be keyboard-driven);
    // Return 2 when eqn is done (so core_keydown should request the CPU)
}
