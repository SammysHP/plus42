/*****************************************************************************
 * Plus42 -- an enhanced HP-42S calculator simulator
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
#include "core_commands2.h"
#include "core_commands7.h"
#include "core_display.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_parser.h"
#include "shell.h"
#include "shell_spool.h"

static bool active = false;
static int menu_whence;

static vartype_list *eqns;
static int4 num_eqns;
static int selected_row = -1; // -1: top of list; num_eqns: bottom of list
static int edit_pos; // -1: in list; >= 0: in editor
static int display_pos;

#define DIALOG_NONE 0
#define DIALOG_SAVE_CONFIRM 1
#define DIALOG_DELETE_CONFIRM 2
#define DIALOG_RCL 3
#define DIALOG_STO 4
#define DIALOG_STO_OVERWRITE_X 5
#define DIALOG_STO_OVERWRITE_PRGM 6
#define DIALOG_STO_OVERWRITE_ALPHA 7
static int dialog = DIALOG_NONE;

static int edit_menu; // MENU_NONE = the navigation menu
static int prev_edit_menu = MENU_NONE;
static int catalog_row;
static bool menu_sticky;
static bool new_eq;
static char *edit_buf = NULL;
static int4 edit_len, edit_capacity;
static bool cursor_on;
static int current_error = ERR_NONE;

static int timeout_action = 0;
static int timeout_edit_pos;
static int rep_key = -1;

static short catalog[] = {
    CMD_ABS,       CMD_ACOS,   CMD_ACOSH,    CMD_AND,     CMD_ASIN,    CMD_ASINH,
    CMD_ATAN,      CMD_ATANH,  CMD_BASEADD,  CMD_BASESUB, CMD_BASEMUL, CMD_BASEDIV,
    CMD_BASECHS,   CMD_COMB,   CMD_COMPLEX,  CMD_COS,     CMD_COSH,    CMD_CPX_T,
    CMD_CROSS,     CMD_DET,    CMD_DIM,      CMD_DIM_T,   CMD_DOT,     CMD_E_POW_X,
    CMD_E_POW_X_1, CMD_FNRM,   CMD_FP,       CMD_GAMMA,   CMD_HMSADD,  CMD_HMSSUB,
    CMD_INVRT,     CMD_IP,     CMD_LN,       CMD_LN_1_X,  CMD_LOG,     CMD_MAT_T,
    CMD_MOD,       CMD_FACT,   CMD_NEWMAT,   CMD_NOT,     CMD_OR,      CMD_PERM,
    CMD_RAN,       CMD_REAL_T, CMD_RND,      CMD_RNRM,    CMD_ROTXY,   CMD_RSUM,
    CMD_SEED,      CMD_SIGN,   CMD_SIN,      CMD_SINH,    CMD_SQRT,    CMD_STR_T,
    CMD_TAN,       CMD_TANH,   CMD_TRANS,    CMD_UVEC,    CMD_XOR,     CMD_SQUARE,
    CMD_Y_POW_X,   CMD_INV,    CMD_10_POW_X, CMD_TO_DEC,  CMD_TO_DEG,  CMD_TO_HMS,
    CMD_TO_HR,     CMD_TO_OCT, CMD_TO_POL,   CMD_TO_RAD,  CMD_TO_REC,  CMD_DATE,
    CMD_DATE_PLUS, CMD_DDAYS,  CMD_DOW,      CMD_TIME,    CMD_APPEND,  CMD_C_TO_N,
    CMD_EXTEND,    CMD_LENGTH, CMD_LIST_T,   CMD_NEWLIST, CMD_NEWSTR,  CMD_N_TO_C,
    CMD_N_TO_S,    CMD_POS,    CMD_REV,      CMD_SUBSTR,  CMD_S_TO_N,  CMD_FMA
};

static int catalog_rows = 15;


static void restart_cursor();

bool unpersist_eqn(int4 ver) {
    if (!read_bool(&active)) return false;
    if (!read_int(&menu_whence)) return false;
    bool have_eqns;
    if (!read_bool(&have_eqns)) return false;
    if (have_eqns)
        eqns = (vartype_list *) recall_var("EQNS", 4);
    else
        eqns = NULL;
    if (eqns != NULL)
        num_eqns = eqns->size;
    else
        num_eqns = 0;
    if (!read_int(&selected_row)) return false;
    if (!read_int(&edit_pos)) return false;
    if (!read_int(&display_pos)) return false;

    if (!read_int(&dialog)) return false;

    if (!read_int(&edit_menu)) return false;
    if (!read_int(&prev_edit_menu)) return false;
    if (!read_int(&catalog_row)) return false;
    if (!read_bool(&menu_sticky)) return false;
    if (!read_bool(&new_eq)) return false;
    if (!read_int4(&edit_len)) return false;
    edit_capacity = edit_len;
    if (edit_buf != NULL)
        free(edit_buf);
    edit_buf = (char *) malloc(edit_len);
    if (edit_buf == NULL)
        return false;
    if (fread(edit_buf, 1, edit_len, gfile) != edit_len) goto fail;
    if (!read_bool(&cursor_on)) goto fail;
    if (!read_int(&current_error)) return false;

    if (active && edit_pos != -1 && dialog == DIALOG_NONE)
        restart_cursor();
    return true;
    
    fail:
    free(edit_buf);
    edit_buf = NULL;
    return false;
}

bool persist_eqn() {
    if (!write_bool(active)) return false;
    if (!write_int(menu_whence)) return false;
    if (!write_bool(eqns != NULL)) return false;
    if (!write_int(selected_row)) return false;
    if (!write_int(edit_pos)) return false;
    if (!write_int(display_pos)) return false;
    if (!write_int(dialog)) return false;
    if (!write_int(edit_menu)) return false;
    if (!write_int(prev_edit_menu)) return false;
    if (!write_int(catalog_row)) return false;
    if (!write_bool(menu_sticky)) return false;
    if (!write_bool(new_eq)) return false;
    if (!write_int(edit_len)) return false;
    if (fwrite(edit_buf, 1, edit_len, gfile) != edit_len) return false;
    if (!write_bool(cursor_on)) return false;
    if (!write_int(current_error)) return false;
    return true;
}

void clear_eqn() {
    free_vartype((vartype *) eqns);
    eqns = NULL;
}

#if 0
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
#endif

static void show_error(int err) {
    current_error = err;
    eqn_draw();
}
    
static void restart_cursor() {
    timeout_action = 2;
    cursor_on = true;
    shell_request_timeout3(500);
}

static int t_rep_key;
static int t_rep_count;

static bool insert_text(const char *text, int len) {
    if (len == 1) {
        t_rep_count++;
        if (t_rep_count == 1)
            t_rep_key = 1024 + *text;
    }

    if (edit_len + len > edit_capacity) {
        int newcap = edit_len + len + 32;
        char *newbuf = (char *) realloc(edit_buf, newcap);
        if (newbuf == NULL) {
            show_error(ERR_INSUFFICIENT_MEMORY);
            return false;
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
    restart_cursor();
    eqn_draw();
    return true;
}

struct eqn_name_entry {
    int2 cmd;
    int2 len;
    const char *name;
};

/* Most built-ins are represented in equations using the same name
 * as in the RPN environment, with an opening parenthesis tacked on.
 * These functions deviate from that pattern:
 */
static eqn_name_entry eqn_name[] = {
    { CMD_Y_POW_X,   1, "^"        },
    { CMD_ADD,       1, "+"        },
    { CMD_SUB,       1, "-"        },
    { CMD_MUL,       1, "\001"     },
    { CMD_DIV,       1, "\000"     },
    { CMD_SIGMAADD,  2, "\005("    },
    { CMD_STO,       2, "L("       },
    { CMD_RCL,       2, "G("       },
    { CMD_INV,       4, "INV("     },
    { CMD_SQUARE,    3, "SQ("      },
    { CMD_E_POW_X,   4, "EXP("     },
    { CMD_10_POW_X,  5, "ALOG("    },
    { CMD_E_POW_X_1, 6, "EXPM1("   },
    { CMD_LN_1_X,    5, "LNP1("    },
    { CMD_AND,       5, "BAND("    },
    { CMD_OR,        4, "BOR("     },
    { CMD_XOR,       5, "BXOR("    },
    { CMD_NOT,       5, "BNOT("    },
    { CMD_BASEADD,   5, "BADD("    },
    { CMD_BASESUB,   5, "BSUB("    },
    { CMD_BASEMUL,   5, "BMUL("    },
    { CMD_BASEDIV,   5, "BDIV("    },
    { CMD_BASECHS,   5, "BNEG("    },
    { CMD_DATE_PLUS, 5, "DATE("    },
    { CMD_HMSADD,    7, "HMSADD("  },
    { CMD_HMSSUB,    7, "HMSSUB("  },
    { CMD_FACT,      5, "FACT("    },
    { CMD_TO_DEG,    4, "DEG("     },
    { CMD_TO_RAD,    4, "RAD("     },
    { CMD_TO_HR,     4, "HRS("     },
    { CMD_TO_HMS,    4, "HMS("     },
    { CMD_TO_DEC,    4, "DEC("     },
    { CMD_TO_OCT,    4, "OCT("     },
    { CMD_TO_REC,    4, "REC("     },
    { CMD_TO_POL,    4, "POL("     },
    { CMD_RAN,       4, "RAN#"     },
    { CMD_DATE,      5, "CDATE"    },
    { CMD_TIME,      5, "CTIME"    },
    { CMD_NEWSTR,    6, "NEWSTR"   },
    { CMD_NEWLIST,   7, "NEWLIST"  },
    { CMD_NULL,      0, NULL       }
};

/* Inserts a function, given by its command id, into the equation.
 * Only functions from our restricted catalog and our list of
 * special cases (the 'catalog' and 'eqn_name' arrays, above)
 * are allowed.
 */
static bool insert_function(int cmd) {
    if (cmd == CMD_NULL) {
        squeak();
        return false;
    }
    for (int i = 0; eqn_name[i].cmd != CMD_NULL; i++) {
        if (cmd == eqn_name[i].cmd)
            return insert_text(eqn_name[i].name, eqn_name[i].len);
    }
    for (int i = 0; i < catalog_rows * 6; i++) {
        if (cmd == catalog[i]) {
            const command_spec *cs = cmd_array + cmd;
            return insert_text(cs->name, cs->name_length)
                && insert_text("(", 1);
        }
    }
    squeak();
    return false;
}

static void save() {
    if (eqns != NULL) {
        if (!disentangle((vartype *) eqns)) {
            show_error(ERR_INSUFFICIENT_MEMORY);
            return;
        }
    }
    int errpos;
    vartype *v = new_equation(edit_buf, edit_len, flags.f.eqn_compat, &errpos);
    if (v == NULL)
        v = new_string(edit_buf, edit_len);
    if (v == NULL) {
        show_error(ERR_INSUFFICIENT_MEMORY);
        return;
    }
    if (new_eq) {
        if (num_eqns == 0) {
            eqns = (vartype_list *) new_list(1);
            if (eqns == NULL) {
                show_error(ERR_INSUFFICIENT_MEMORY);
                return;
            }
            eqns->array->data[0] = v;
            int err = store_var("EQNS", 4, (vartype *) eqns);
            if (err != ERR_NONE) {
                free_vartype((vartype *) eqns);
                eqns = NULL;
                show_error(err);
                return;
            }
            selected_row = 0;
            num_eqns = 1;
        } else {
            vartype **new_data = (vartype **) realloc(eqns->array->data, (num_eqns + 1) * sizeof(vartype *));
            if (new_data == NULL) {
                show_error(ERR_INSUFFICIENT_MEMORY);
                return;
            }
            eqns->array->data = new_data;
            eqns->size++;
            num_eqns++;
            selected_row++;
            if (selected_row == num_eqns)
                selected_row--;
            int n = num_eqns - selected_row - 1;
            if (n > 0)
                memmove(eqns->array->data + selected_row + 1,
                        eqns->array->data + selected_row,
                        n);
            eqns->array->data[selected_row] = v;
        }
    } else {
        free_vartype(eqns->array->data[selected_row]);
        eqns->array->data[selected_row] = v;
    }
    free(edit_buf);
    edit_pos = -1;
    eqn_draw();
}

static int print_eq_row;
static bool print_eq_do_all;

static int print_eq_worker(bool interrupted) {
    if (interrupted) {
        shell_annunciators(-1, -1, 0, -1, -1, -1);
        return ERR_STOP;
    }

    if (print_eq_do_all || edit_pos == -1) {
        vartype *v = eqns->array->data[print_eq_row];
        if (v->type == TYPE_STRING) {
            vartype_string *s = (vartype_string *) v;
            print_lines(s->txt(), s->length, 1);
        } else if (v->type == TYPE_EQUATION) {
            vartype_equation *eq = (vartype_equation *) v;
            equation_data *eqd = prgms[eq->data.index()].eq_data;
            print_lines(eqd->text, eqd->length, 1);
        } else {
            print_lines("<Invalid>", 9, 1);
        }
    } else {
        print_lines(edit_buf, edit_len, 1);
    }

    if (print_eq_do_all) {
        if (print_eq_row == num_eqns - 1)
            goto done;
        print_text(NULL, 0, true);
        print_eq_row++;
        return ERR_INTERRUPTIBLE;
    } else {
        done:
        shell_annunciators(-1, -1, 0, -1, -1, -1);
        return ERR_NONE;
    }
}

static void print_eq(bool all) {
    print_eq_row = all ? 0 : selected_row;
    print_eq_do_all = all;
    mode_interruptible = print_eq_worker;
    mode_stoppable = false;
    shell_annunciators(-1, -1, 1, -1, -1, -1);
}

static void update_menu(int menuid) {
    edit_menu = menuid;
    int multirow = edit_menu == MENU_CATALOG || edit_menu != MENU_NONE && menus[edit_menu].next != MENU_NONE;
    shell_annunciators(multirow, -1, -1, -1, -1, -1);
}

static void goto_prev_menu() {
    if (!menu_sticky) {
        update_menu(prev_edit_menu);
        prev_edit_menu = MENU_NONE;
    }
}

int eqn_start(int whence) {
    active = true;
    menu_whence = whence;
    set_shift(false);
    
    vartype *v = recall_var("EQNS", 4);
    if (v == NULL) {
        eqns = NULL;
        num_eqns = 0;
    } else if (v->type == TYPE_REALMATRIX) {
        vartype_realmatrix *rm = (vartype_realmatrix *) v;
        int4 n = rm->rows * rm->columns;
        vartype_list *list = (vartype_list *) new_list(n);
        if (list == NULL) {
            active = false;
            return ERR_INSUFFICIENT_MEMORY;
        }
        for (int4 i = 0; i < n; i++) {
            vartype *s;
            if (rm->array->is_string[i]) {
                char *text;
                int len;
                get_matrix_string(rm, i, &text, &len);
                s = new_string(text, len);
            } else {
                char buf[50];
                int len = real2buf(buf, rm->array->data[i]);
                s = new_string(buf, len);
            }
            if (s == NULL) {
                active = false;
                free_vartype((vartype *) list);
                return ERR_INSUFFICIENT_MEMORY;
            }
            list->array->data[i] = s;
        }
        store_var("EQNS", 4, (vartype *) list);
        goto do_list;
    } else if (v->type != TYPE_LIST) {
        active = false;
        return ERR_INVALID_TYPE;
    } else {
        do_list:
        eqns = (vartype_list *) v;
        num_eqns = eqns->size;
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

bool eqn_active() {
    return active;
}

bool eqn_editing() {
    return active && edit_pos != -1;
}

char *eqn_copy() {
    textbuf tb;
    tb.buf = NULL;
    tb.size = 0;
    tb.capacity = 0;
    tb.fail = false;
    char buf[50];
    if (edit_pos != -1) {
        for (int4 i = 0; i < edit_len; i += 10) {
            int4 seg_len = edit_len - i;
            if (seg_len > 10)
                seg_len = 10;
            int4 bufptr = hp2ascii(buf, edit_buf + i, seg_len, false);
            tb_write(&tb, buf, bufptr);
        }
    } else {
        for (int4 i = 0; i < num_eqns; i++) {
            const char *text;
            int len;
            vartype *v = eqns->array->data[i];
            if (v->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) v;
                text = s->txt();
                len = s->length;
            } else if (v->type == TYPE_EQUATION) {
                vartype_equation *eq = (vartype_equation *) v;
                equation_data *eqd = prgms[eq->data.index()].eq_data;
                text = eqd->text;
                len = eqd->length;
                tb_write(&tb, buf, len);
            } else {
                text = "<Invalid>";
                len = 9;
            }
            for (int4 j = 0; j < len; j += 10) {
                int4 seg_len = len - j;
                if (seg_len > 10)
                    seg_len = 10;
                int4 bufptr = hp2ascii(buf, text + j, seg_len, false);
                tb_write(&tb, buf, bufptr);
            }
            tb_write(&tb, "\r\n", 2);
        }
    }
    tb_write_null(&tb);
    if (tb.fail) {
        show_error(ERR_INSUFFICIENT_MEMORY);
        free(tb.buf);
        return NULL;
    } else
        return tb.buf;
}

void eqn_paste(const char *buf) {
    if (edit_pos == -1) {
        if (num_eqns == 0 && !ensure_var_space(1)) {
            show_error(ERR_INSUFFICIENT_MEMORY);
            return;
        }
        int4 s = 0;
        while (buf[s] != 0) {
            int4 p = s;
            while (buf[s] != 0 && buf[s] != '\r' && buf[s] != '\n')
                s++;
            if (s == p) {
                if (buf[s] == 0)
                    break;
                else {
                    s++;
                    continue;
                }
            }
            int4 t = s - p;
            char *hpbuf = (char *) malloc(t + 4);
            if (hpbuf == NULL) {
                show_error(ERR_INSUFFICIENT_MEMORY);
                return;
            }
            int len = ascii2hp(hpbuf, t, buf + p, t);
            int errpos;
            vartype *v = new_equation(hpbuf, len, flags.f.eqn_compat, &errpos);
            if (v == NULL)
                v = new_string(hpbuf, len);
            if (v == NULL)
                goto nomem;
            if (num_eqns == 0) {
                eqns = (vartype_list *) new_list(1);
                if (eqns == NULL) {
                    nomem:
                    show_error(ERR_INSUFFICIENT_MEMORY);
                    free(v);
                    free(hpbuf);
                    return;
                }
            } else {
                vartype **new_data = (vartype **) realloc(eqns->array->data, (num_eqns + 1) * sizeof(vartype *));
                if (new_data == NULL)
                    goto nomem;
                eqns->array->data = new_data;
                eqns->size++;
            }
            int n = selected_row + 1;
            if (n > num_eqns)
                n = num_eqns;
            memmove(eqns->array->data + n + 1, eqns->array->data + n, (num_eqns - n) * sizeof(vartype *));
            eqns->array->data[n] = v;
            free(hpbuf);
            if (num_eqns == 0)
                store_var("EQNS", 4, (vartype *) eqns);
            selected_row = n;
            num_eqns++;
            if (buf[s] != 0)
                s++;
        }
        eqn_draw();
    } else {
        int4 p = 0;
        while (buf[p] != 0 && buf[p] != '\r' && buf[p] != '\n')
            p++;
        char *hpbuf = (char *) malloc(p + 4);
        if (hpbuf == NULL) {
            show_error(ERR_INSUFFICIENT_MEMORY);
            return;
        }
        int len = ascii2hp(hpbuf, p, buf, p);
        insert_text(hpbuf, len);
        free(hpbuf);
    }
}

static void draw_print1_menu() {
    draw_key(0, 0, 0, "EQ", 2);
    draw_key(1, 0, 0, "LISTE", 5);
    draw_key(2, 0, 0, "VARS", 4);
    draw_key(3, 0, 0, "LISTV", 5);
    draw_key(4, 0, 0, "PRST", 4);
    draw_key(5, 0, 0, "ADV", 3);
}

static void draw_print2_menu() {
    if (flags.f.printer_exists) {
        draw_key(0, 0, 0, "PON\037", 4);
        draw_key(1, 0, 0, "POFF", 4);
    } else {
        draw_key(0, 0, 0, "PON", 3);
        draw_key(1, 0, 0, "POFF\037", 5);
    }
    if (!flags.f.trace_print && !flags.f.normal_print)
        draw_key(2, 0, 0, "MAN\037", 4);
    else
        draw_key(2, 0, 0, "MAN", 3);
    if (!flags.f.trace_print && flags.f.normal_print)
        draw_key(3, 0, 0, "NOR\037", 4);
    else
        draw_key(3, 0, 0, "NORM", 4);
    if (flags.f.trace_print && !flags.f.normal_print)
        draw_key(4, 0, 0, "TRAC\037", 5);
    else
        draw_key(4, 0, 0, "TRACE", 5);
    if (flags.f.trace_print && flags.f.normal_print)
        draw_key(5, 0, 0, "STRA\037", 5);
    else
        draw_key(5, 0, 0, "STRAC", 5);
}

bool eqn_draw() {
    if (!active)
        return false;
    clear_display();
    if (current_error != ERR_NONE) {
        draw_string(0, 0, errors[current_error].text, errors[current_error].length);
        if (current_error == ERR_INVALID_EQUATION)
            goto draw_eqn_menu;
        draw_key(1, 0, 0, "OK", 2);
    } else if (dialog == DIALOG_SAVE_CONFIRM) {
        draw_string(0, 0, "Save this equation?", 19);
        draw_key(0, 0, 0, "YES", 3);
        draw_key(2, 0, 0, "NO", 2);
        draw_key(4, 0, 0, "EDIT", 4);
    } else if (dialog == DIALOG_DELETE_CONFIRM) {
        draw_string(0, 0, "Delete the equation?", 20);
        draw_key(1, 0, 0, "YES", 3);
        draw_key(5, 0, 0, "NO", 2);
    } else if (dialog == DIALOG_RCL) {
        draw_string(0, 0, "Recall equation from:", 21);
        goto sto_rcl_keys;
    } else if (dialog == DIALOG_STO) {
        draw_string(0, 0, "Store equation to:", 18);
        sto_rcl_keys:
        draw_key(0, 0, 0, "X", 1);
        draw_key(1, 0, 0, "PRGM", 4);
        draw_key(2, 0, 0, "ALPHA", 5);
        draw_key(4, 0, 0, "CNCL", 4);
    } else if (dialog == DIALOG_STO_OVERWRITE_X
            || dialog == DIALOG_STO_OVERWRITE_PRGM
            || dialog == DIALOG_STO_OVERWRITE_ALPHA) {
        draw_string(0, 0, "Insert or overwrite?", 20);
        draw_key(0, 0, 0, "INSR", 4);
        draw_key(2, 0, 0, "OVER", 4);
        draw_key(4, 0, 0, "CNCL", 4);
    } else if (edit_pos == -1) {
        if (selected_row == -1) {
            draw_string(0, 0, "<Top of List>", 13);
        } else if (selected_row == num_eqns) {
            draw_string(0, 0, "<Bottom of List>", 16);
        } else {
            const char *text;
            int4 len;
            vartype *v = eqns->array->data[selected_row];
            if (v->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) v;
                text = s->txt();
                len = s->length;
            } else if (v->type == TYPE_EQUATION) {
                vartype_equation *eq = (vartype_equation *) v;
                equation_data *eqd = prgms[eq->data.index()].eq_data;
                text = eqd->text;
                len = eqd->length;
            } else {
                text = "<Invalid>";
                len = 9;
            }
            if (len <= 22) {
                draw_string(0, 0, text, len);
            } else {
                draw_string(0, 0, text, 21);
                draw_char(21, 0, 26);
            }
        }
        if (edit_menu == MENU_PRINT1) {
            draw_print1_menu();
        } else if (edit_menu == MENU_PRINT2) {
            draw_print2_menu();
        } else {
            draw_eqn_menu:
            draw_key(0, 0, 0, "CALC", 4);
            draw_key(1, 0, 0, "EDIT", 4);
            draw_key(2, 0, 0, "DELET", 5);
            draw_key(3, 0, 0, "NEW", 3);
            draw_key(4, 0, 0, "^", 1, true);
            draw_key(5, 0, 0, "\016", 1, true);
        }
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
            draw_block(cpos, 0);
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
        } else if (edit_menu == MENU_PRINT1) {
            draw_print1_menu();
        } else if (edit_menu == MENU_PRINT2) {
            draw_print2_menu();
        } else if (edit_menu >= MENU_CUSTOM1 && edit_menu <= MENU_CUSTOM3) {
            int row = edit_menu - MENU_CUSTOM1;
            for (int k = 0; k < 6; k++) {
                char label[7];
                int len;
                get_custom_key(row * 6 + k + 1, label, &len);
                draw_key(k, 0, 1, label, len);
            }
        } else if (edit_menu == MENU_CATALOG) {
            for (int k = 0; k < 6; k++) {
                int cmd = catalog[catalog_row * 6 + k];
                draw_key(k, 0, 1, cmd_array[cmd].name, cmd_array[cmd].name_length);
            }
        } else {
            const menu_item_spec *mi = menus[edit_menu].child;
            for (int i = 0; i < 6; i++) {
                int id = mi[i].menuid;
                if (id == MENU_NONE || (id & 0x3000) == 0) {
                    draw_key(i, 0, 0, mi[i].title, mi[i].title_length);
                } else {
                    id &= 0x0fff;
                    draw_key(i, 0, 1, cmd_array[id].name, cmd_array[id].name_length);
                }
            }
        }
    }
    flush_display();
    return true;
}

static int keydown_list(int key, bool shift, int *repeat);
static int keydown_edit(int key, bool shift, int *repeat);
static int keydown_print1(int key, bool shift, int *repeat);
static int keydown_print2(int key, bool shift, int *repeat);
static int keydown_error(int key, bool shift, int *repeat);
static int keydown_save_confirmation(int key, bool shift, int *repeat);
static int keydown_delete_confirmation(int key, bool shift, int *repeat);
static int keydown_rcl(int key, bool shift, int *repeat);
static int keydown_sto(int key, bool shift, int *repeat);
static int keydown_sto_overwrite(int key, bool shift, int *repeat);

/* eqn_keydown() return values:
 * 0: equation editor not active; caller should perform normal event processing
 * 1: equation editor active
 * 2: equation editor active; caller should NOT suppress key timeouts
 *    (this mode is for when PRMSLVi, PGMINTi, or PMEXEC are being performed,
 *     i.e., when the CALC menu key in the list view has been pressed)
 * 3: equation editor active but busy; request CPU
 */
int eqn_keydown(int key, int *repeat) {
    if (!active)
        return 0;
    
    bool shift = false;
    if (mode_interruptible == NULL) {
        if (key == 0)
            return 1;
        if (key == KEY_SHIFT) {
            set_shift(!mode_shift);
            return 1;
        }
        shift = mode_shift;
        set_shift(false);
    } else {
        // Used to make print functions EQ, LISTE, and LISTV interruptible
        if (key == KEY_SHIFT) {
            set_shift(!mode_shift);
        } else if (key != 0) {
            shift = mode_shift;
            set_shift(false);
        }
        if (key == KEY_EXIT) {
            mode_interruptible(true);
            mode_interruptible = NULL;
            return 1;
        } else {
            int err = mode_interruptible(false);
            if (err == ERR_INTERRUPTIBLE) {
                if (key != 0 && key != KEY_SHIFT)
                    squeak();
                return 3;
            }
            mode_interruptible = NULL;
            // Continue normal key event processing...
            if (key == 0 || key == KEY_SHIFT)
                return 1;
        }
    }
    
    if (current_error != ERR_NONE)
        return keydown_error(key, shift, repeat);
    else if (dialog == DIALOG_SAVE_CONFIRM)
        return keydown_save_confirmation(key, shift, repeat);
    else if (dialog == DIALOG_DELETE_CONFIRM)
        return keydown_delete_confirmation(key, shift, repeat);
    else if (dialog == DIALOG_RCL)
        return keydown_rcl(key, shift, repeat);
    else if (dialog == DIALOG_STO)
        return keydown_sto(key, shift, repeat);
    else if (dialog == DIALOG_STO_OVERWRITE_X
            || dialog == DIALOG_STO_OVERWRITE_PRGM
            || dialog == DIALOG_STO_OVERWRITE_ALPHA)
        return keydown_sto_overwrite(key, shift, repeat);
    else if (edit_menu == MENU_PRINT1)
        return keydown_print1(key, shift, repeat);
    else if (edit_menu == MENU_PRINT2)
        return keydown_print2(key, shift, repeat);
    else if (edit_pos == -1)
        return keydown_list(key, shift, repeat);
    else
        return keydown_edit(key, shift, repeat);
}

static int keydown_print1(int key, bool shift, int *repeat) {
    arg_struct arg;
    switch (key) {
        case KEY_SIGMA: {
            /* EQ */
            if (flags.f.printer_exists) {
                if (selected_row == -1 || selected_row == num_eqns) {
                    squeak();
                    return 1;
                } else {
                    print_eq(false);
                    goto exit_menu;
                }
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_INV: {
            /* LISTE */
            if (flags.f.printer_exists) {
                if (num_eqns == 0) {
                    squeak();
                    return 1;
                } else {
                    print_eq(true);
                    goto exit_menu;
                }
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_SQRT: {
            /* VARS */
            if (flags.f.printer_exists) {
                arg.type = ARGTYPE_STR;
                arg.length = 0;
                docmd_prmvar(&arg);
                goto exit_menu;
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_LOG: {
            /* LISTV */
            if (flags.f.printer_exists) {
                docmd_prusr(NULL);
                goto exit_menu;
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_LN: {
            /* PRSTK */
            if (flags.f.printer_exists) {
                docmd_prstk(&arg);
                goto exit_menu;
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_XEQ: {
            /* ADV */
            docmd_adv(NULL);
            return 1;
        }
        case KEY_UP:
        case KEY_DOWN: {
            edit_menu = MENU_PRINT2;
            eqn_draw();
            return 1;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
            } else {
                exit_menu:
                goto_prev_menu();
                if (edit_pos != -1)
                    restart_cursor();
                eqn_draw();
            }
            return mode_interruptible == NULL ? 1 : 3;
        }
        default: {
            squeak();
            return 1;
        }
    }
}

static int keydown_print2(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* PRON */
            flags.f.printer_exists = true;
            break;
        }
        case KEY_INV: {
            /* PROFF */
            flags.f.printer_exists = false;
            break;
        }
        case KEY_SQRT: {
            /* MAN */
            flags.f.trace_print = false;
            flags.f.normal_print = false;
            break;
        }
        case KEY_LOG: {
            /* NORM */
            flags.f.trace_print = false;
            flags.f.normal_print = true;
            break;
        }
        case KEY_LN: {
            /* TRACE */
            flags.f.trace_print = true;
            flags.f.normal_print = false;
            break;
        }
        case KEY_XEQ: {
            /* STRACE */
            flags.f.trace_print = true;
            flags.f.normal_print = true;
            break;
        }
        case KEY_UP:
        case KEY_DOWN: {
            edit_menu = MENU_PRINT1;
            break;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                return 1;
            } else {
                goto_prev_menu();
                if (edit_pos != -1)
                    restart_cursor();
                eqn_draw();
                return 1;
            }
        }
        default: {
            squeak();
            return 1;
        }
    }

    eqn_draw();
    return 1;
}

static int keydown_error(int key, bool shift, int *repeat) {
    if (shift && key == KEY_EXIT) {
        docmd_off(NULL);
    } else {
        show_error(ERR_NONE);
        restart_cursor();
    }
    return 1;
}

static int keydown_save_confirmation(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* Yes */
            if (edit_len == 0) {
                squeak();
                break;
            }
            dialog = DIALOG_NONE;
            save();
            break;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                break;
            }
            /* Fall through */
        }
        case KEY_SQRT: {
            /* No */
            free(edit_buf);
            edit_pos = -1;
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
        case KEY_LN: {
            /* Cancel */
            dialog = DIALOG_NONE;
            restart_cursor();
            eqn_draw();
            break;
        }
        default: {
            squeak();
            break;
        }
    }
    return 1;
}

static int keydown_delete_confirmation(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_INV: {
            /* Yes */
            if (num_eqns == 1) {
                purge_var("EQNS", 4);
                eqns = NULL;
                num_eqns = 0;
            } else {
                if (!disentangle((vartype *) eqns)) {
                    show_error(ERR_INSUFFICIENT_MEMORY);
                    return 0;
                }
                free_vartype(eqns->array->data[selected_row]);
                memmove(eqns->array->data + selected_row,
                        eqns->array->data + selected_row + 1,
                        (num_eqns - selected_row - 1) * sizeof(vartype *));
                num_eqns--;
                eqns->size--;
                vartype **new_data = (vartype **) realloc(eqns->array->data, num_eqns * sizeof(vartype *));
                if (new_data != NULL)
                    eqns->array->data = new_data;
            }
            goto finish;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                break;
            }
            /* Fall through */
        }
        case KEY_XEQ: {
            /* No */
            finish:
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
        default: {
            squeak();
            break;
        }
    }
    return 1;
}

static int keydown_rcl(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* X */
            if (sp == -1 || stack[sp]->type != TYPE_STRING) {
                nope:
                squeak();
            } else {
                vartype_string *s = (vartype_string *) stack[sp];
                if (s->length == 0)
                    goto nope;
                edit_buf = (char *) malloc(s->length);
                if (edit_buf == NULL) {
                    show_error(ERR_INSUFFICIENT_MEMORY);
                } else {
                    memcpy(edit_buf, s->txt(), s->length);
                    edit_len = s->length;
                    goto store;
                }
            }
            break;
        }
        case KEY_INV: {
            /* PRGM */
            int4 oldpc = pc;
            int cmd;
            arg_struct arg;
            get_next_command(&pc, &cmd, &arg, 0, NULL);
            pc = oldpc;
            if (cmd != CMD_XSTR || arg.length == 0) {
                squeak();
            } else {
                edit_buf = (char *) malloc(arg.length);
                if (edit_buf == NULL) {
                    show_error(ERR_INSUFFICIENT_MEMORY);
                } else {
                    memcpy(edit_buf, arg.val.xstr, arg.length);
                    edit_len = arg.length;
                    goto store;
                }
            }
            break;
        }
        case KEY_SQRT: {
            /* ALPHA */
            if (reg_alpha_length == 0) {
                squeak();
            } else {
                edit_buf = (char *) malloc(reg_alpha_length);
                if (edit_buf == NULL) {
                    show_error(ERR_INSUFFICIENT_MEMORY);
                } else {
                    memcpy(edit_buf, reg_alpha, reg_alpha_length);
                    edit_len = reg_alpha_length;
                    store:
                    edit_pos = 0;
                    new_eq = true;
                    save();
                    if (edit_pos == 0) {
                        free(edit_buf);
                        edit_pos = -1;
                    } else {
                        dialog = DIALOG_NONE;
                        eqn_draw();
                    }
                }
            }
            break;
        }
        case KEY_LN:
        case KEY_EXIT: {
            /* Cancel */
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
    }
    return 1;
}

static bool get_equation() {
    const char *text;
    int len;
    vartype *v = eqns->array->data[selected_row];
    if (v->type == TYPE_STRING) {
        vartype_string *s = (vartype_string *) v;
        text = s->txt();
        len = s->length;
    } else if (v->type == TYPE_EQUATION) {
        vartype_equation *eq = (vartype_equation *) v;
        equation_data *eqd = prgms[eq->data.index()].eq_data;
        text = eqd->text;
        len = eqd->length;
    } else {
        text = "<Invalid>";
        len = 9;
    }
    edit_buf = (char *) malloc(len);
    if (edit_buf == NULL)
        return false;
    edit_len = edit_capacity = len;
    memcpy(edit_buf, text, len);
    return true;
}

static int keydown_sto(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* X */
            dialog = DIALOG_STO_OVERWRITE_X;
            if (sp != -1 && stack[sp]->type == TYPE_STRING)
                goto done;
            else
                return keydown_sto_overwrite(KEY_SIGMA, false, NULL);
        }
        case KEY_INV: {
            /* PRGM */
            dialog = DIALOG_STO_OVERWRITE_PRGM;
            int4 oldpc = pc;
            int cmd;
            arg_struct arg;
            get_next_command(&pc, &cmd, &arg, 0, NULL);
            pc = oldpc;
            if (cmd == CMD_XSTR)
                goto done;
            else
                return keydown_sto_overwrite(KEY_SIGMA, false, NULL);
        }
        case KEY_SQRT: {
            /* ALPHA */
            dialog = DIALOG_STO_OVERWRITE_ALPHA;
            if (reg_alpha_length > 0)
                goto done;
            else
                return keydown_sto_overwrite(KEY_SIGMA, false, NULL);
        }
        case KEY_LN:
        case KEY_EXIT: {
            /* Cancel */
            dialog = DIALOG_NONE;
            done:
            eqn_draw();
            break;
        }
    }
    return 1;
}

static int keydown_sto_overwrite(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: /* Insert */
        case KEY_SQRT: /* Overwrite */ {
            if (!get_equation()) {
                show_error(ERR_INSUFFICIENT_MEMORY);
                return 1;
            }
            switch (dialog) {
                case DIALOG_STO_OVERWRITE_X: {
                    vartype *s = new_string(edit_buf, edit_len);
                    if (s == NULL) {
                        nomem:
                        free(edit_buf);
                        show_error(ERR_INSUFFICIENT_MEMORY);
                        return 1;
                    }
                    bool sld = flags.f.stack_lift_disable;
                    flags.f.stack_lift_disable = 0;
                    if (key == KEY_SIGMA) {
                        int err = recall_result_silently(s);
                        if (err != ERR_NONE) {
                            flags.f.stack_lift_disable = sld;
                            free_vartype(s);
                            goto nomem;
                        }
                    } else {
                        free_vartype(stack[sp]);
                        stack[sp] = s;
                    }
                    break;
                }
                case DIALOG_STO_OVERWRITE_PRGM: {
                    arg_struct arg;
                    arg.type = ARGTYPE_XSTR;
                    arg.length = edit_len > 65535 ? 65535 : edit_len;
                    arg.val.xstr = edit_buf;
                    if (key == KEY_SIGMA) {
                        store_command_after(&pc, CMD_XSTR, &arg, NULL);
                    } else {
                        delete_command(pc);
                        store_command(pc, CMD_XSTR, &arg, NULL);
                    }
                    break;
                }
                case DIALOG_STO_OVERWRITE_ALPHA: {
                    char *ptr = edit_buf;
                    int len = edit_len;
                    if (len > 44) {
                        len = 44;
                        ptr += edit_len - 44;
                    }
                    if (key == KEY_SIGMA) {
                        if (reg_alpha_length + len > 44) {
                            int excess = reg_alpha_length + len - 44;
                            memmove(reg_alpha, reg_alpha + excess, reg_alpha_length - excess);
                            reg_alpha_length -= excess;
                        }
                    } else {
                        reg_alpha_length = 0;
                    }
                    memcpy(reg_alpha + reg_alpha_length, ptr, len);
                    reg_alpha_length += len;
                    break;
                }
            }
            free(edit_buf);
            goto done;
        }
        case KEY_LN:
        case KEY_EXIT: {
            /* Cancel */
            done:
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
    }
    return 1;
}

static bool is_function_menu(int menu) {
    return menu == MENU_TOP_FCN
            || menu == MENU_CONVERT1
            || menu == MENU_CONVERT2
            || menu == MENU_PROB
            || menu == MENU_CUSTOM1
            || menu == MENU_CUSTOM2
            || menu == MENU_CUSTOM3
            || menu == MENU_CATALOG;
}

static void select_function_menu(int menu) {
    if (!is_function_menu(edit_menu))
        prev_edit_menu = edit_menu;
    menu_sticky = menu == edit_menu || edit_menu != MENU_NONE && menus[edit_menu].next == menu;
    if (!menu_sticky) {
        update_menu(menu);
        eqn_draw();
    }
}

static void start_edit(int pos) {
    if (!get_equation()) {
        show_error(ERR_INSUFFICIENT_MEMORY);
    } else {
        new_eq = false;
        edit_pos = pos;
        display_pos = 0;
        if (pos > 12) {
            display_pos = pos - 12;
            int slop = edit_len - display_pos - 22;
            if (slop < 0)
                display_pos -= slop;
        }
        update_menu(MENU_NONE);
        restart_cursor();
        eqn_draw();
    }
}

static int keydown_list(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_UP: {
            if (shift) {
                selected_row = -1;
                eqn_draw();
            } else if (selected_row >= 0) {
                selected_row--;
                rep_key = key;
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
                rep_key = key;
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
            vartype *v = eqns->array->data[selected_row];
            if (v->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) v;
                int errpos;
                vartype *eq = new_equation(s->txt(), s->length, flags.f.eqn_compat, &errpos);
                if (eq == NULL) {
                    if (errpos == -1) {
                        show_error(ERR_INSUFFICIENT_MEMORY);
                    } else {
                        squeak();
                        show_error(ERR_INVALID_EQUATION);
                        current_error = ERR_NONE;
                        timeout_action = 3;
                        timeout_edit_pos = errpos;
                        shell_request_timeout3(1000);
                    }
                    return 1;
                }
                free_vartype(eqns->array->data[selected_row]);
                eqns->array->data[selected_row] = eq;
                v = eq;
            } else if (v->type != TYPE_EQUATION) {
                show_error(ERR_INVALID_TYPE);
                return 1;
            }

            pending_command_arg.type = ARGTYPE_EQN;
            pending_command_arg.val.num = ((vartype_equation *) v)->data.index();
            if (menu_whence == CATSECT_PGM_SOLVE)
                pending_command = CMD_PGMSLVi;
            else if (menu_whence == CATSECT_PGM_INTEG)
                pending_command = CMD_PGMINTi;
            else
                /* PGMMENU */
                pending_command = CMD_PMEXEC;
            /* Note that we don't do active = false here, since at this point
             * it is still possible that the command will go to NULL, and in
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
            start_edit(0);
            return 1;
        }
        case KEY_SQRT: {
            /* DELET */
            if (selected_row == -1 || selected_row == num_eqns) {
                squeak();
            } else {
                dialog = DIALOG_DELETE_CONFIRM;
                eqn_draw();
            }
            return 1;
        }
        case KEY_LOG: {
            /* NEW */
            edit_buf = NULL;
            edit_len = edit_capacity = 0;
            new_eq = true;
            edit_pos = 0;
            display_pos = 0;
            update_menu(MENU_ALPHA1);
            restart_cursor();
            eqn_draw();
            return 1;
        }
        case KEY_LN:
        case KEY_XEQ: {
            /* MOVE up, MOVE down */
            if (selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            /* First, show a glimpse of the current contents of the target row,
             * then, perform the actual swap (unless the target row is one of
             * the end-of-list markers), and finally, schedule a redraw after
             * 0.5 to make the screen reflect the state of affairs with the
             * completed swap.
             */
            int dir;
            if (key == KEY_LN) {
                /* up */
                dir = -1;
                goto move;
            } else {
                /* down */
                dir = 1;
                move:
                selected_row += dir;
                eqn_draw();
                if (selected_row == -1 || selected_row == num_eqns) {
                    selected_row -= dir;
                } else {
                    vartype *v = eqns->array->data[selected_row];
                    eqns->array->data[selected_row] = eqns->array->data[selected_row - dir];
                    eqns->array->data[selected_row - dir] = v;
                }
            }
            timeout_action = 1;
            shell_request_timeout3(500);
            return 1;
        }
        case KEY_STO: {
            if (selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            dialog = DIALOG_STO;
            eqn_draw();
            return 1;
        }
        case KEY_RCL: {
            dialog = DIALOG_RCL;
            eqn_draw();
            return 1;
        }
        case KEY_SUB: {
            if (shift)
                select_function_menu(MENU_PRINT1);
            else
                squeak();
            return 1;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                return 1;
            }
            active = false;
            set_menu(MENULEVEL_APP, MENU_CATALOG);
            set_cat_section(menu_whence);
            redisplay();
            return 1;
        }
        default: {
            squeak();
            return 1;
        }
    }
}

static int keydown_edit_2(int key, bool shift, int *repeat) {
    if (key >= 1024 && key < 2048) {
        char c = key - 1024;
        insert_text(&c, 1);
        return 1;
    }

    if (key >= 2048) {
        int cmd = key - 2048;
        insert_function(cmd);
        return 1;
    }

    if (key >= KEY_SIGMA && key <= KEY_XEQ) {
        /* Menu keys */
        if (edit_menu == MENU_NONE) {
            /* Navigation menu */
            switch (key) {
                case KEY_SIGMA: {
                    /* DEL */
                    if (edit_len > 0 && edit_pos < edit_len) {
                        memmove(edit_buf + edit_pos, edit_buf + edit_pos + 1, edit_len - edit_pos - 1);
                        edit_len--;
                        rep_key = KEY_SIGMA;
                        *repeat = 2;
                        restart_cursor();
                        eqn_draw();
                    } else
                        squeak();
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
                            rep_key = KEY_SQRT;
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
                            rep_key = KEY_LOG;
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
                    prev_edit_menu = MENU_NONE;
                    eqn_draw();
                    return 1;
                }
            }
        } else if (edit_menu == MENU_ALPHA1 || edit_menu == MENU_ALPHA2) {
            /* ALPHA menu */
            update_menu(menus[edit_menu].child[key - 1].menuid);
            eqn_draw();
            return 1;
        } else if (edit_menu >= MENU_ALPHA_ABCDE1 && edit_menu <= MENU_ALPHA_MISC2) {
            /* ALPHA sub-menus */
            char c = menus[edit_menu].child[key - 1].title[0];
            if (shift && c >= 'A' && c <= 'Z')
                c += 32;
            update_menu(menus[edit_menu].parent);
            insert_text(&c, 1);
            return 1;
        } else if (edit_menu >= MENU_CUSTOM1 && edit_menu <= MENU_CUSTOM3) {
            int row = edit_menu - MENU_CUSTOM1;
            char label[7];
            int len;
            get_custom_key(row * 6 + key, label, &len);
            if (len == 0) {
                squeak();
            } else {
                /* Builtins go through the usual mapping; everything else
                 * is inserted literally.
                 */
                int cmd = find_builtin(label, len);
                if (cmd != CMD_NONE) {
                    if (insert_function(cmd)) {
                        goto_prev_menu();
                        eqn_draw();
                    }
                } else {
                    goto_prev_menu();
                    insert_text(label, len);
                    insert_text("(", 1);
                    eqn_draw();
                }
            }
        } else if (edit_menu == MENU_CATALOG) {
            /* Subset of the regular FCN catalog plus Free42 extensions */
            int cmd = catalog[catalog_row * 6 + key - 1];
            if (cmd == CMD_NULL) {
                squeak();
            } else {
                if (insert_function(cmd)) {
                    goto_prev_menu();
                    eqn_draw();
                }
            }
            return 1;
        } else {
            /* Various function menus */
            int cmd;
            if (shift && edit_menu == MENU_TOP_FCN) {
                switch (key) {
                    case KEY_SIGMA: cmd = CMD_SIGMASUB; break;
                    case KEY_INV: cmd = CMD_Y_POW_X; break;
                    case KEY_SQRT: cmd = CMD_SQUARE; break;
                    case KEY_LOG: cmd = CMD_10_POW_X; break;
                    case KEY_LN: cmd = CMD_E_POW_X; break;
                    case KEY_XEQ: cmd = CMD_GTO; break;
                }
            } else {
                cmd = menus[edit_menu].child[key - 1].menuid;
                if (cmd == MENU_NONE || (cmd & 0xf000) == 0)
                    cmd = CMD_NULL;
                else
                    cmd = cmd & 0x0fff;
            }
            if (insert_function(cmd)) {
                goto_prev_menu();
                eqn_draw();
            }
            return 1;
        }
    } else {
        /* Rest of keyboard */
        switch (key) {
            case KEY_STO: {
                if (shift)
                    squeak();
                else
                    insert_function(CMD_STO);
                break;
            }
            case KEY_RCL: {
                if (shift)
                    insert_text("%", 1);
                else
                    insert_function(CMD_RCL);
                break;
            }
            case KEY_RDN: {
                if (shift)
                    insert_text("PI", 2);
                else
                    squeak();
                break;
            }
            case KEY_SIN: {
                if (shift)
                    insert_function(CMD_ASIN);
                else
                    insert_function(CMD_SIN);
                break;
            }
            case KEY_COS: {
                if (shift)
                    insert_function(CMD_ACOS);
                else
                    insert_function(CMD_COS);
                break;
            }
            case KEY_TAN: {
                if (shift)
                    insert_function(CMD_ATAN);
                else
                    insert_function(CMD_TAN);
                break;
            }
            case KEY_ENTER: {
                if (shift) {
                    update_menu(MENU_ALPHA1);
                    eqn_draw();
                } else if (edit_len == 0) {
                    squeak();
                } else {
                    save();
                }
                break;
            }
            case KEY_SWAP: {
                if (shift)
                    insert_text("[", 1);
                else
                    insert_text("(", 1);
                break;
            }
            case KEY_CHS: {
                if (shift)
                    insert_text("]", 1);
                else
                    insert_text(")", 1);
                break;
            }
            case KEY_E: {
                if (shift)
                    squeak();
                else
                    insert_text("\030", 1);
                break;
            }
            case KEY_BSP: {
                if (shift) {
                    edit_len = 0;
                    edit_pos = 0;
                    display_pos = 0;
                    eqn_draw();
                } else if (edit_len > 0 && edit_pos > 0) {
                    edit_pos--;
                    memmove(edit_buf + edit_pos, edit_buf + edit_pos + 1, edit_len - edit_pos - 1);
                    edit_len--;
                    if (display_pos > 0)
                        display_pos--;
                    rep_key = KEY_BSP;
                    *repeat = 2;
                    restart_cursor();
                    eqn_draw();
                } else
                    squeak();
                return 1;
            }
            case KEY_0: {
                if (shift)
                    select_function_menu(MENU_TOP_FCN);
                else
                    insert_text("0", 1);
                break;
            }
            case KEY_1: {
                if (shift)
                    squeak();
                else
                    insert_text("1", 1);
                break;
            }
            case KEY_2: {
                if (shift)
                    select_function_menu(MENU_CUSTOM1);
                else
                    insert_text("2", 1);
                break;
            }
            case KEY_3: {
                if (shift)
                    squeak();
                else
                    insert_text("3", 1);
                break;
            }
            case KEY_4: {
                if (shift)
                    squeak();
                else
                    insert_text("4", 1);
                break;
            }
            case KEY_5: {
                if (shift)
                    select_function_menu(MENU_CONVERT1);
                else
                    insert_text("5", 1);
                break;
            }
            case KEY_6: {
                if (shift)
                    squeak();
                else
                    insert_text("6", 1);
                break;
            }
            case KEY_7: {
                if (shift)
                    squeak();
                else
                    insert_text("7", 1);
                break;
            }
            case KEY_8: {
                if (shift)
                    squeak();
                else
                    insert_text("8", 1);
                break;
            }
            case KEY_9: {
                if (shift)
                    squeak();
                else
                    insert_text("9", 1);
                break;
            }
            case KEY_DOT: {
                if (shift)
                    squeak();
                else
                    insert_text(".", 1);
                break;
            }
            case KEY_RUN: {
                if (shift)
                    squeak();
                else
                    insert_text("=", 1);
                break;
            }
            case KEY_DIV: {
                if (shift)
                    insert_text(":", 1);
                else
                    insert_function(CMD_DIV);
                break;
            }
            case KEY_MUL: {
                if (shift)
                    select_function_menu(MENU_PROB);
                else
                    insert_function(CMD_MUL);
                break;
            }
            case KEY_SUB: {
                if (shift) {
                    select_function_menu(MENU_PRINT1);
                    cursor_on = false;
                    eqn_draw();
                } else {
                    insert_function(CMD_SUB);
                }
                break;
            }
            case KEY_ADD: {
                if (shift) {
                    catalog_row = 0;
                    select_function_menu(MENU_CATALOG);
                } else
                    insert_function(CMD_ADD);
                break;
            }
            case KEY_UP:
            case KEY_DOWN: {
                if (edit_menu == MENU_CATALOG) {
                    if (key == KEY_UP) {
                        catalog_row--;
                        if (catalog_row == -1)
                            catalog_row = catalog_rows - 1;
                    } else {
                        catalog_row++;
                        if (catalog_row == catalog_rows)
                            catalog_row = 0;
                    }
                    *repeat = 1;
                    eqn_draw();
                } else if (edit_menu != MENU_NONE && menus[edit_menu].next != MENU_NONE) {
                    /* No need to handle Up and Down separately, since none of the
                     * menus we're using here have more than two rows.
                     */
                    update_menu(menus[edit_menu].next);
                    *repeat = 1;
                    eqn_draw();
                } else
                    squeak();
                break;
            }
            case KEY_EXIT: {
                if (shift) {
                    docmd_off(NULL);
                    break;
                }
                if (edit_menu == MENU_NONE) {
                    if (!new_eq) {
                        vartype *v = eqns->array->data[selected_row];
                        const char *orig_text;
                        int orig_len;
                        if (v->type == TYPE_STRING) {
                            vartype_string *s = (vartype_string *) v;
                            orig_text = s->txt();
                            orig_len = s->length;
                        } else if (v->type == TYPE_EQUATION) {
                            vartype_equation *eq = (vartype_equation *) v;
                            equation_data *eqd = prgms[eq->data.index()].eq_data;
                            orig_text = eqd->text;
                            orig_len = eqd->length;
                        } else {
                            orig_text = "<Invalid>";
                            orig_len = 9;
                        }
                        if (string_equals(edit_buf, edit_len, orig_text, orig_len)) {
                            edit_pos = -1;
                            free(edit_buf);
                            eqn_draw();
                            break;
                        }
                    }
                    dialog = DIALOG_SAVE_CONFIRM;
                } else if (is_function_menu(edit_menu)) {
                    menu_sticky = false;
                    goto_prev_menu();
                } else {
                    update_menu(menus[edit_menu].parent);
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

static int keydown_edit(int key, bool shift, int *repeat) {
    t_rep_count = 0;
    int ret = keydown_edit_2(key, shift, repeat);
    if (core_settings.auto_repeat && t_rep_count == 1) {
        *repeat = 2;
        rep_key = t_rep_key;
    } else if (*repeat != 0) {
        rep_key = key;
    }
    return ret;
}

int eqn_repeat() {
    if (!active)
        return -1;
    // Like core_repeat(): 0 means stop repeating; 1 means slow repeat,
    // for Up/Down; 2 means fast repeat, for text entry; 3 means extra
    // slow repeat, for the equation editor's list view.
    if (rep_key == -1)
        return 0;
    if (edit_pos == -1) {
        if (rep_key == KEY_UP) {
            if (selected_row >= 0) {
                selected_row--;
                eqn_draw();
                return 3;
            } else {
                rep_key = -1;
            }
        } else if (rep_key == KEY_DOWN) {
            if (selected_row < num_eqns) {
                selected_row++;
                eqn_draw();
                return 3;
            } else {
                rep_key = -1;
            }
        }
    } else {
        int repeat = 0;
        keydown_edit(rep_key, false, &repeat);
        if (repeat == 0)
            rep_key = -1;
        else
            return rep_key == KEY_UP || rep_key == KEY_DOWN ? 1 : 2;
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
        eqn_draw();
    } else if (action == 2) {
        /* Cursor blinking */
        if (edit_pos == -1 || current_error != ERR_NONE || dialog != DIALOG_NONE
                || edit_menu == MENU_PRINT1 || edit_menu == MENU_PRINT2)
            return true;
        cursor_on = !cursor_on;
        if (cursor_on) {
            draw_block(edit_pos - display_pos, 0);
        } else {
            char c = edit_pos == edit_len ? ' ' : edit_buf[edit_pos];
            draw_char(edit_pos - display_pos, 0, c);
        }
        flush_display();
        timeout_action = 2;
        shell_request_timeout3(500);
    } else if (action == 3) {
        /* Start editing after parse error message has timed out */
        start_edit(timeout_edit_pos);
    }
    return true;
}
