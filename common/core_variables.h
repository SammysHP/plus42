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

#ifndef CORE_VARIABLES_H
#define CORE_VARIABLES_H 1

#include "core_phloat.h"

/***********************/
/* Variable data types */
/***********************/

class Evaluator;

class equation_data {
    public:
    int refcount;
    equation_data() : refcount(0), text(NULL), ev(NULL) {}
    ~equation_data();
    int4 length;
    char *text;
    Evaluator *ev;
    bool compatMode;
    int eqn_index;
};

class pgm_index {
    private:
    // The unified program index combines these possibilities:
    // Special values: -1 = none, -2 = solve, -3 = integ
    // Program indices: even nonnegative, index = unified >> 1
    // Equation indices: odd nonnegative, index = (unified >> 1) + prgms_count
    int4 uni;
    public:
    void inc_refcount();
    void dec_refcount();
    pgm_index() {
        uni = -1;
    }
    pgm_index(const pgm_index &that) {
        uni = that.uni;
        inc_refcount();
    }
    ~pgm_index() {
        dec_refcount();
    }
    void init_copy(const pgm_index &that) {
        uni = that.uni;
        inc_refcount();
    }
    void init_eqn_from_state(int4 eqn) {
        uni = (eqn << 1) | 1;
    }
    void init_unified_from_state(int4 unified) {
        this->uni = unified;
    }
    void init_eqn(int4 eqn) {
        uni = (eqn << 1) | 1;
        inc_refcount();
    }
    void init_eqn(int4 eqn, equation_data *data);
    void clear() {
        dec_refcount();
        uni = -1;
    }
    void set_special(int4 special) {
        dec_refcount();
        this->uni = special;
    }
    void set_prgm(int4 prgm) {
        dec_refcount();
        uni = prgm << 1;
    }
    void set_eqn(int4 eqn) {
        dec_refcount();
        uni = (eqn << 1) | 1;
        inc_refcount();
    }
    void set_unified(int4 unified) {
        dec_refcount();
        this->uni = unified;
        inc_refcount();
    }
    bool is_eqn() {
        return uni > 0 && (uni & 1) != 0;
    }
    bool is_prgm() {
        return uni >= 0 && (uni & 1) == 0;
    }
    bool is_special() {
        return uni < 0;
    }
    int special() {
        return uni;
    }
    int4 eqn() {
        return uni >> 1;
    }
    int4 prgm() {
        return uni >> 1;
    }
    int4 index();
    int4 unified() {
        return uni;
    }
    pgm_index &operator=(const pgm_index &that) {
        dec_refcount();
        uni = that.uni;
        inc_refcount();
        return *this;
    }
    bool operator==(const pgm_index &that) {
        return uni == that.uni;
    }
    bool operator!=(const pgm_index &that) {
        return uni != that.uni;
    }
};

#define TYPE_NULL 0
#define TYPE_REAL 1
#define TYPE_COMPLEX 2
#define TYPE_REALMATRIX 3
#define TYPE_COMPLEXMATRIX 4
#define TYPE_STRING 5
#define TYPE_LIST 6
#define TYPE_EQUATION 7

struct vartype {
    int type;
};


struct vartype_real {
    int type;
    phloat x;
};


struct vartype_complex {
    int type;
    phloat re, im;
};


struct realmatrix_data {
    int refcount;
    phloat *data;
    char *is_string;
};

struct vartype_realmatrix {
    int type;
    int4 rows;
    int4 columns;
    realmatrix_data *array;
};


struct complexmatrix_data {
    int refcount;
    phloat *data;
};

struct vartype_complexmatrix {
    int type;
    int4 rows;
    int4 columns;
    complexmatrix_data *array;
};


/* Maximum short string length in a stand-alone variable */
#define SSLENV ((int) (sizeof(char *) < 8 ? 8 : sizeof(char *)))
/* Maximum short string length in a matrix element */
#define SSLENM ((int) sizeof(phloat) - 1)

struct vartype_string {
    int type;
    int4 length;
    /* When length <= SSLENV, use buf; otherwise, use ptr */
    union {
        char buf[SSLENV];
        char *ptr;
    } t;
    char *txt() {
        return length > SSLENV ? t.ptr : t.buf;
    }
    const char *txt() const {
        return length > SSLENV ? t.ptr : t.buf;
    }
    void trim1();
};


struct list_data {
    int refcount;
    vartype **data;
};

struct vartype_list {
    int type;
    int4 size;
    list_data *array;
};


struct vartype_equation {
    int type;
    pgm_index data;
};


vartype *new_real(phloat value);
vartype *new_complex(phloat re, phloat im);
vartype *new_string(const char *s, int slen);
vartype *new_realmatrix(int4 rows, int4 columns);
vartype *new_complexmatrix(int4 rows, int4 columns);
vartype *new_list(int4 size);
vartype *new_equation(const char *text, int4 length, bool compat_mode, int *errpos, int eqn_index = -1);
void free_vartype(vartype *v);
void clean_vartype_pools();
void free_long_strings(char *is_string, phloat *data, int4 n);
void get_matrix_string(vartype_realmatrix *rm, int4 i, char **text, int4 *length);
void get_matrix_string(const vartype_realmatrix *rm, int4 i, const char **text, int4 *length);
bool put_matrix_string(vartype_realmatrix *rm, int4 i, const char *text, int4 length);
vartype *dup_vartype(const vartype *v);
int disentangle(vartype *v);
int lookup_var(const char *name, int namelength);
vartype *recall_var(const char *name, int namelength);
vartype *recall_global_var(const char *name, int namelength);
equation_data *find_equation_data(const char *name, int namelength);
int store_params();
bool ensure_var_space(int n);
int store_var(const char *name, int namelength, vartype *value, bool local = false, bool global = false);
void purge_var(const char *name, int namelength);
bool vars_exist(int section);
bool contains_strings(const vartype_realmatrix *rm);
int matrix_copy(vartype *dst, const vartype *src);
vartype *recall_private_var(const char *name, int namelength);
vartype *recall_and_purge_private_var(const char *name, int namelength);
int store_private_var(const char *name, int namelength, vartype *value);

#endif
