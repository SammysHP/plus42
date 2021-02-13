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
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "core_commands1.h"
#include "core_commands2.h"
#include "core_commands7.h"
#include "core_display.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_variables.h"
#include "shell.h"

/////////////////////////////////////////////////////////////////
///// Accelerometer, Location Services, and Compass support /////
/////////////////////////////////////////////////////////////////

#if defined(ANDROID) || defined(IPHONE)

int docmd_accel(arg_struct *arg) {
    double x, y, z;
    int err = shell_get_acceleration(&x, &y, &z);
    if (err == 0)
        return ERR_NONEXISTENT;
    if (flags.f.big_stack)
        if (!ensure_stack_capacity(flags.f.stack_lift_disable ? 2 : 3))
            return ERR_INSUFFICIENT_MEMORY;
    vartype *new_x = new_real(x);
    vartype *new_y = new_real(y);
    vartype *new_z = new_real(z);
    if (new_x == NULL || new_y == NULL || new_z == NULL) {
        free_vartype(new_x);
        free_vartype(new_y);
        free_vartype(new_z);
        return ERR_INSUFFICIENT_MEMORY;
    }
    if (flags.f.big_stack) {
        if (flags.f.stack_lift_disable) {
            free_vartype(stack[sp]);
            sp += 2;
        } else {
            sp += 3;
        }
    } else {
        free_vartype(stack[REG_T]);
        free_vartype(stack[REG_Z]);
        if (flags.f.stack_lift_disable) {
            free_vartype(stack[REG_X]);
            stack[REG_T] = stack[REG_Y];
        } else {
            free_vartype(stack[REG_Y]);
            stack[REG_T] = stack[REG_X];
        }
    }
    stack[sp - 2] = new_z;
    stack[sp - 1] = new_y;
    stack[sp] = new_x;
    print_trace();
    return ERR_NONE;
}

int docmd_locat(arg_struct *arg) {
    double lat, lon, lat_lon_acc, elev, elev_acc;
    int err = shell_get_location(&lat, &lon, &lat_lon_acc, &elev, &elev_acc);
    if (err == 0)
        return ERR_NONEXISTENT;
    if (flags.f.big_stack)
        if (!ensure_stack_capacity(flags.f.stack_lift_disable ? 3 : 4))
            return ERR_INSUFFICIENT_MEMORY;
    vartype *new_x = new_real(lat);
    vartype *new_y = new_real(lon);
    vartype *new_z = new_real(elev);
    vartype *new_t = new_realmatrix(1, 2);
    if (new_x == NULL || new_y == NULL || new_z == NULL || new_t == NULL) {
        free_vartype(new_x);
        free_vartype(new_y);
        free_vartype(new_z);
        free_vartype(new_t);
        return ERR_INSUFFICIENT_MEMORY;
    }
    vartype_realmatrix *rm = (vartype_realmatrix *) new_t;
    rm->array->data[0] = lat_lon_acc;
    rm->array->data[1] = elev_acc;
    if (flags.f.big_stack) {
        if (flags.f.stack_lift_disable) {
            free_vartype(stack[sp]);
            sp += 3;
        } else {
            sp += 4;
        }
    } else {
        for (int i = 0; i < 4; i++)
            free_vartype(stack[i]);
    }
    stack[sp - 3] = new_t;
    stack[sp - 2] = new_z;
    stack[sp - 1] = new_y;
    stack[sp] = new_x;
    print_trace();
    return ERR_NONE;
}

int docmd_heading(arg_struct *arg) {
    double mag_heading, true_heading, acc, x, y, z;
    int err = shell_get_heading(&mag_heading, &true_heading, &acc, &x, &y, &z);
    if (err == 0)
        return ERR_NONEXISTENT;
    if (flags.f.big_stack)
        if (!ensure_stack_capacity(flags.f.stack_lift_disable ? 3 : 4))
            return ERR_INSUFFICIENT_MEMORY;
    vartype *new_x = new_real(mag_heading);
    vartype *new_y = new_real(true_heading);
    vartype *new_z = new_real(acc);
    vartype *new_t = new_realmatrix(1, 3);
    if (new_x == NULL || new_y == NULL || new_z == NULL || new_t == NULL) {
        free_vartype(new_x);
        free_vartype(new_y);
        free_vartype(new_z);
        free_vartype(new_t);
        return ERR_INSUFFICIENT_MEMORY;
    }
    vartype_realmatrix *rm = (vartype_realmatrix *) new_t;
    rm->array->data[0] = x;
    rm->array->data[1] = y;
    rm->array->data[2] = z;
    if (flags.f.big_stack) {
        if (flags.f.stack_lift_disable) {
            free_vartype(stack[sp]);
            sp += 3;
        } else {
            sp += 4;
        }
    } else {
        for (int i = 0; i < 4; i++)
            free_vartype(stack[i]);
    }
    stack[sp - 3] = new_t;
    stack[sp - 2] = new_z;
    stack[sp - 1] = new_y;
    stack[sp] = new_x;
    print_trace();
    return ERR_NONE;
}

#else

int docmd_accel(arg_struct *arg) {
    return ERR_NONEXISTENT;
}

int docmd_locat(arg_struct *arg) {
    return ERR_NONEXISTENT;
}

int docmd_heading(arg_struct *arg) {
    return ERR_NONEXISTENT;
}

#endif

/////////////////////////////////////////////////
///// HP-41 Time Module & CX Time emulation /////
/////////////////////////////////////////////////

static int date2comps(phloat x, int4 *yy, int4 *mm, int4 *dd) {
    int4 y, m, d;
#ifdef BCD_MATH
    if (flags.f.ymd) {
        y = to_int4(floor(x));
        m = to_int4(floor((x - y) * 100));
        d = to_int4(x * 10000) % 100;
    } else {
        m = to_int4(floor(x));
        d = to_int4(floor((x - m) * 100));
        y = to_int4(x * 1000000) % 10000;
    }
#else
    if (flags.f.ymd) {
        y = to_int4(floor(x));
        int4 r = (int4) floor((x - y) * 100000000 + 0.5);
        r /= 10000;
        m = r / 100;
        d = r % 100;
    } else {
        m = to_int4(floor(x));
        int4 r = (int4) floor((x - m) * 100000000 + 0.5);
        r /= 100;
        d = r / 10000;
        y = r % 10000;
    }
#endif

    if (!flags.f.ymd && flags.f.dmy) {
        int4 t = m;
        m = d;
        d = t;
    }

    if (y < 1582 || y > 4320 || m < 1 || m > 12 || d < 1 || d > 31)
        return ERR_INVALID_DATA;
    if ((m == 4 || m == 6 || m == 9 || m == 11) && d == 31)
        return ERR_INVALID_DATA;
    if (m == 2 && d > ((y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28))
        return ERR_INVALID_DATA;
    if (y == 1582 && (m < 10 || m == 10 && d < 15)
            || y == 4320 && (m > 9 || m == 9 && d > 10))
        return ERR_INVALID_DATA;

    *yy = y;
    *mm = m;
    *dd = d;
    return ERR_NONE;
}

static phloat comps2date(int4 y, int4 m, int4 d) {
    if (flags.f.ymd)
        return phloat(y * 10000 + m * 100 + d) / 10000;
    if (flags.f.dmy) {
        int4 t = m;
        m = d;
        d = t;
    }
    return phloat(m * 1000000 + d * 10000 + y) / 1000000;
}

/* Gregorian Date <-> Day Number conversion functions
 * Algorithm due to Henry F. Fliegel and Thomas C. Van Flandern,
 * Communications of the ACM, Vol. 11, No. 10 (October, 1968).
 */
static int greg2jd(int4 y, int4 m, int4 d, int4 *jd) {
    *jd = ( 1461 * ( y + 4800 + ( m - 14 ) / 12 ) ) / 4 +
          ( 367 * ( m - 2 - 12 * ( ( m - 14 ) / 12 ) ) ) / 12 -
          ( 3 * ( ( y + 4900 + ( m - 14 ) / 12 ) / 100 ) ) / 4 +
          d - 32075;
    return ERR_NONE;
}

static int jd2greg(int4 jd, int4 *y, int4 *m, int4 *d) {
    if (jd < 2299161 || jd > 3299160)
        return ERR_OUT_OF_RANGE;
    int4 l = jd + 68569;
    int4 n = ( 4 * l ) / 146097;
    l = l - ( 146097 * n + 3 ) / 4;
    int4 i = ( 4000 * ( l + 1 ) ) / 1461001;
    l = l - ( 1461 * i ) / 4 + 31;
    int4 j = ( 80 * l ) / 2447;
    *d = l - ( 2447 * j ) / 80;
    l = j / 11;
    *m = j + 2 - ( 12 * l );
    *y = 100 * ( n - 49 ) + i + l;
    return ERR_NONE;
}


int docmd_adate(arg_struct *arg) {
    phloat x = ((vartype_real *) stack[sp])->x;
    if (x < 0)
        x = -x;

    int digits;
    if (flags.f.fix_or_all && flags.f.eng_or_all)
        digits = 11;
    else {
        digits = 0;
        if (flags.f.digits_bit3)
            digits += 8;
        if (flags.f.digits_bit2)
            digits += 4;
        if (flags.f.digits_bit1)
            digits += 2;
        if (flags.f.digits_bit0)
            digits += 1;
    }

    char buf[10];
    int bufptr = 0;

    if (flags.f.ymd) {
        if (x >= 10000)
            return ERR_INVALID_DATA;

        int4 y = to_int4(floor(x));
#ifdef BCD_MATH
        int4 m = to_int4(floor((x - y) * 100));
        int4 d = to_int4(x * 10000) % 100;
#else
        int4 r = (int4) floor((x - y) * 100000000 + 0.5);
        r /= 10000;
        int4 m = r / 100;
        int4 d = r % 100;
#endif
        bufptr += int2string(y, buf + bufptr, 10 - bufptr);
        if (digits > 0) {
            char2buf(buf, 10, &bufptr, '-');
            if (m < 10)
                char2buf(buf, 10, &bufptr, '0');
            bufptr += int2string(m, buf + bufptr, 10 - bufptr);
            if (digits > 2) {
                char2buf(buf, 10, &bufptr, '-');
                if (d < 10)
                    char2buf(buf, 10, &bufptr, '0');
                bufptr += int2string(d, buf + bufptr, 10 - bufptr);
            }
        }
    } else {
        if (x >= 100)
            return ERR_INVALID_DATA;

        int4 m = to_int4(floor(x));
#ifdef BCD_MATH
        int4 d = to_int4(floor((x - m) * 100));
        int4 y = to_int4(x * 1000000) % 10000;
#else
        int4 r = (int4) floor((x - m) * 100000000 + 0.5);
        r /= 100;
        int4 d = r / 10000;
        int4 y = r % 10000;
#endif
        int c = y / 100;
        y %= 100;

        if (m < 10)
            char2buf(buf, 10, &bufptr, '0');
        bufptr += int2string(m, buf + bufptr, 10 - bufptr);
        if (digits > 0) {
            char2buf(buf, 10, &bufptr, flags.f.dmy ? '.' : '/');
            if (d < 10)
                char2buf(buf, 10, &bufptr, '0');
            bufptr += int2string(d, buf + bufptr, 10 - bufptr);
            if (digits > 2) {
                char2buf(buf, 10, &bufptr, flags.f.dmy ? '.' : '/');
                if (digits > 4) {
                    if (c < 10)
                        char2buf(buf, 10, &bufptr, '0');
                    bufptr += int2string(c, buf + bufptr, 10 - bufptr);
                }
                if (y < 10)
                    char2buf(buf, 10, &bufptr, '0');
                bufptr += int2string(y, buf + bufptr, 10 - bufptr);
            }
        }
    }

    append_alpha_string(buf, bufptr, 0);
    return ERR_NONE;
}

int docmd_atime(arg_struct *arg) {
    phloat x = ((vartype_real *) stack[sp])->x;
    bool neg = x < 0;
    if (neg)
        x = -x;
    if (x >= 100)
        return ERR_INVALID_DATA;
    int h = to_int(floor(x));
    if (h == 0)
        neg = false;
    int4 ms = to_int4(floor((x - floor(x)) * 1000000));
    int m = (int) (ms / 10000);
    int s = (int) (ms / 100 % 100);
    int cs = (int) (ms % 100);
    bool am = false;
    bool pm = false;

    if (mode_time_clk24) {
        if (neg && h >= 1 && h <= 11)
            h += 12;
    } else if (h < 24) {
        if (!neg && h < 12)
            am = true;
        else
            pm = true;
        if (h == 0)
            h = 12;
        else if (h > 12)
            h -= 12;
    }

    int digits;
    if (flags.f.fix_or_all && flags.f.eng_or_all)
        digits = 11;
    else {
        digits = 0;
        if (flags.f.digits_bit3)
            digits += 8;
        if (flags.f.digits_bit2)
            digits += 4;
        if (flags.f.digits_bit1)
            digits += 2;
        if (flags.f.digits_bit0)
            digits += 1;
    }

    char buf[14];
    int bufptr = 0;
    if (h < 10)
        char2buf(buf, 14, &bufptr, mode_time_clk24 ? '0' : ' ');
    bufptr += int2string(h, buf + bufptr, 14 - bufptr);
    if (digits > 0) {
        char2buf(buf, 14, &bufptr, ':');
        if (m < 10)
            char2buf(buf, 14, &bufptr, '0');
        bufptr += int2string(m, buf + bufptr, 14 - bufptr);
        if (digits > 2) {
            char2buf(buf, 14, &bufptr, ':');
            if (s < 10)
                char2buf(buf, 14, &bufptr, '0');
            bufptr += int2string(s, buf + bufptr, 14 - bufptr);
            if (digits > 4) {
                char2buf(buf, 14, &bufptr, '.');
                if (cs < 10)
                    char2buf(buf, 14, &bufptr, '0');
                bufptr += int2string(cs, buf + bufptr, 14 - bufptr);
            }
        }
    }
    if (am)
        string2buf(buf, 14, &bufptr, " AM", 3);
    else if (pm)
        string2buf(buf, 14, &bufptr, " PM", 3);
    append_alpha_string(buf, bufptr, 0);

    return ERR_NONE;
}

int docmd_atime24(arg_struct *arg) {
    bool saved_clk24 = mode_time_clk24;
    mode_time_clk24 = true;
    int res = docmd_atime(arg);
    mode_time_clk24 = saved_clk24;
    return res;
}

int docmd_clk12(arg_struct *arg) {
    mode_time_clk24 = false;
    return ERR_NONE;
}

int docmd_clk24(arg_struct *arg) {
    mode_time_clk24 = true;
    return ERR_NONE;
}

static char weekdaynames[] = "SUNMONTUEWEDTHUFRISAT";

int docmd_date(arg_struct *arg) {
    uint4 date;
    int weekday;
    shell_get_time_date(NULL, &date, &weekday);
    int y = date / 10000;
    int m = date / 100 % 100;
    int d = date % 100;
    if (flags.f.ymd)
        date = y * 10000L + m * 100 + d;
    else if (flags.f.dmy)
        date = y + m * 10000L + d * 1000000;
    else
        date = y + m * 1000000 + d * 10000L;
    vartype *new_x = new_real((int4) date);
    if (new_x == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    ((vartype_real *) new_x)->x /= flags.f.ymd ? 10000 : 1000000;
    if (!program_running()) {
        /* Note: I'm not completely faithful to the HP-41 here. It formats the
         * date as "14.03.2010 SUN" in DMY mode, and as "03/14/2010:SU" in MDY
         * mode. I mimic the former, but the latter I changed to
         * "03/14/2010 SUN"; the MDY display format used on the HP-41 is the
         * way it is because that was all they could fit in its 12-character
         * display. (Note that the periods in the DMY format and the colon in
         * the MDY format don't take up a character position on the HP-41.)
         */
        char buf[22];
        int bufptr = 0;
        if (flags.f.ymd) {
            bufptr += int2string(y, buf + bufptr, 22 - bufptr);
            char2buf(buf, 22, &bufptr, '-');
            if (m < 10)
                char2buf(buf, 22, &bufptr, '0');
            bufptr += int2string(m, buf + bufptr, 22 - bufptr);
            char2buf(buf, 22, &bufptr, '-');
            if (d < 10)
                char2buf(buf, 22, &bufptr, '0');
            bufptr += int2string(d, buf + bufptr, 22 - bufptr);
        } else {
            int n = flags.f.dmy ? d : m;
            if (n < 10)
                char2buf(buf, 22, &bufptr, '0');
            bufptr += int2string(n, buf + bufptr, 22 - bufptr);
            char2buf(buf, 22, &bufptr, flags.f.dmy ? '.' : '/');
            n = flags.f.dmy ? m : d;
            if (n < 10)
                char2buf(buf, 22, &bufptr, '0');
            bufptr += int2string(n, buf + bufptr, 22 - bufptr);
            char2buf(buf, 22, &bufptr, flags.f.dmy ? '.' : '/');
            bufptr += int2string(y, buf + bufptr, 22 - bufptr);
        }
        char2buf(buf, 22, &bufptr, ' ');
        string2buf(buf, 22, &bufptr, weekdaynames + weekday * 3, 3);
        clear_row(0);
        draw_string(0, 0, buf, bufptr);
        flush_display();
        flags.f.message = 1;
        flags.f.two_line_message = 0;
        if (flags.f.trace_print && flags.f.printer_exists)
            print_text(buf, bufptr, 1);
    }
    return recall_result(new_x);
}

int docmd_date_plus(arg_struct *arg) {
    phloat date = ((vartype_real *) stack[sp - 1])->x;
    if (date < 0 || date > (flags.f.ymd ? 10000 : 100))
        return ERR_INVALID_DATA;
    phloat days = ((vartype_real *) stack[sp])->x;
    if (days < -1000000 || days > 1000000)
        return ERR_OUT_OF_RANGE;

    int4 y, m, d, jd;
    int err = date2comps(date, &y, &m, &d);
    if (err != ERR_NONE)
        return err;
    err = greg2jd(y, m, d, &jd);
    if (err != ERR_NONE)
        return err;
    jd += to_int4(floor(days));
    err = jd2greg(jd, &y, &m, &d);
    if (err != ERR_NONE)
        return err;
    date = comps2date(y, m, d);

    vartype *new_x = new_real(date);
    if (new_x == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    binary_result(new_x);
    return ERR_NONE;
}

int docmd_ddays(arg_struct *arg) {
    phloat date1 = ((vartype_real *) stack[sp - 1])->x;
    if (date1 < 0 || date1 > (flags.f.ymd ? 10000 : 100))
        return ERR_INVALID_DATA;
    phloat date2 = ((vartype_real *) stack[sp])->x;
    if (date2 < 0 || date2 > (flags.f.ymd ? 10000 : 100))
        return ERR_INVALID_DATA;
    int4 y, m, d, jd1, jd2;
    int err = date2comps(date1, &y, &m, &d);
    if (err != ERR_NONE)
        return err;
    err = greg2jd(y, m, d, &jd1);
    if (err != ERR_NONE)
        return err;
    err = date2comps(date2, &y, &m, &d);
    if (err != ERR_NONE)
        return err;
    err = greg2jd(y, m, d, &jd2);
    if (err != ERR_NONE)
        return err;

    vartype *new_x = new_real(jd2 - jd1);
    if (new_x == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    binary_result(new_x);
    return ERR_NONE;
}

int docmd_dmy(arg_struct *arg) {
    flags.f.dmy = true;
    flags.f.ymd = false;
    return ERR_NONE;
}

int docmd_dow(arg_struct *arg) {
    phloat x = ((vartype_real *) stack[sp])->x;
    if (x < 0 || x > (flags.f.ymd ? 10000 : 100))
        return ERR_INVALID_DATA;

    int4 y, m, d, jd;
    int err = date2comps(x, &y, &m, &d);
    if (err != ERR_NONE)
        return err;
    err = greg2jd(y, m, d, &jd);
    if (err != ERR_NONE)
        return err;
    jd = (jd + 1) % 7;

    vartype *new_x = new_real(jd);
    if (new_x == NULL)
        return ERR_INSUFFICIENT_MEMORY;

    if (!program_running()) {
        clear_row(0);
        draw_string(0, 0, weekdaynames + jd * 3, 3);
        flush_display();
        flags.f.message = 1;
        flags.f.two_line_message = 0;
        if (flags.f.trace_print && flags.f.printer_exists)
            print_text(weekdaynames + jd * 3, 3, 1);
    }

    unary_result(new_x);
    return ERR_NONE;
}

int docmd_mdy(arg_struct *arg) {
    flags.f.dmy = false;
    flags.f.ymd = false;
    return ERR_NONE;
}

int docmd_time(arg_struct *arg) {
    uint4 time;
    shell_get_time_date(&time, NULL, NULL);
    vartype *new_x = new_real((int4) time);
    if (new_x == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    ((vartype_real *) new_x)->x /= 1000000;
    if (!program_running()) {
        int h = time / 1000000;
        bool am;
        if (!mode_time_clk24) {
            am = h < 12;
            h = h % 12;
            if (h == 0)
                h = 12;
        }
        int m = time / 10000 % 100;
        int s = time / 100 % 100;
        char buf[22];
        int bufptr = 0;
        if (h < 10)
            char2buf(buf, 22, &bufptr, ' ');
        bufptr += int2string(h, buf + bufptr, 22 - bufptr);
        char2buf(buf, 22, &bufptr, ':');
        if (m < 10)
            char2buf(buf, 22, &bufptr, '0');
        bufptr += int2string(m, buf + bufptr, 22 - bufptr);
        char2buf(buf, 22, &bufptr, ':');
        if (s < 10)
            char2buf(buf, 22, &bufptr, '0');
        bufptr += int2string(s, buf + bufptr, 22 - bufptr);
        if (!mode_time_clk24) {
            char2buf(buf, 22, &bufptr, ' ');
            char2buf(buf, 22, &bufptr, am ? 'A' : 'P');
            char2buf(buf, 22, &bufptr, 'M');
        }
        clear_row(0);
        draw_string(0, 0, buf, bufptr);
        flush_display();
        flags.f.message = 1;
        flags.f.two_line_message = 0;
        if (flags.f.trace_print && flags.f.printer_exists)
            print_text(buf, bufptr, 1);
    }
    return recall_result(new_x);
}

// The YMD function is not an original Time Module function, and in Free42,
// it is grouped with the "Programming" extension, but logically, of course,
// it belongs here. Also, most of the YMD implementation consists of
// modifications to Time Module functions, so in that sense, most of it is
// here anyway.

int docmd_ymd(arg_struct *arg) {
    flags.f.dmy = false;
    flags.f.ymd = true;
    return ERR_NONE;
}

int docmd_getkey1(arg_struct *arg) {
    mode_getkey = true;
    mode_getkey1 = true;
    mode_disable_stack_lift = flags.f.stack_lift_disable;
    return ERR_NONE;
}

////////////////////////////////////////////////////////////////
///// Intel Decimal Floating-Point Math Library: self-test /////
////////////////////////////////////////////////////////////////

#ifdef FREE42_FPTEST

static int tests_lineno;
extern const char *readtest_lines[];

extern "C" {
    int readtest_main(int argc, char *argv[]);

    int tests_eof() {
        return readtest_lines[tests_lineno] == NULL;
    }

    void tests_readline(char *buf, int bufsize) {
        const char *line = readtest_lines[tests_lineno++];
        strncpy(buf, line, bufsize - 1);
        buf[bufsize - 1] = 0;
    }

    int testlogprintf(const char *fmt, ...) {
        int c;
        va_list ap;
        char text[1024];
        va_start(ap, fmt);
        c = vsprintf(text, fmt, ap);
        shell_log(text);
        va_end(ap);
        return c;
    }
}

int docmd_fptest(arg_struct *arg) {
    tests_lineno = 0;
    char *argv[] = { (char *) "readtest", NULL };
    int result = readtest_main(1, argv);
    vartype *v = new_real(result);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

#else

int docmd_fptest(arg_struct *arg) {
    return ERR_NONEXISTENT;
}

#endif

/////////////////////////////////
///// Programming Extension /////
/////////////////////////////////

int docmd_lsto(arg_struct *arg) {
    int err;
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    /* Only allow matrices to be stored in "REGS" */
    if (string_equals(arg->val.text, arg->length, "REGS", 4)
            && stack[sp]->type != TYPE_REALMATRIX
            && stack[sp]->type != TYPE_COMPLEXMATRIX)
        return ERR_RESTRICTED_OPERATION;
    /* When EDITN is active, don't allow the matrix being
     * edited to be overwritten. */
    if (matedit_mode == 3 && string_equals(arg->val.text,
                arg->length, matedit_name, matedit_length))
        return ERR_RESTRICTED_OPERATION;
    vartype *newval = dup_vartype(stack[sp]);
    if (newval == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return store_var(arg->val.text, arg->length, newval, true);
}

int docmd_lasto(arg_struct *arg) {
    int temp_alpha_length = reg_alpha_length;
    if (reg_alpha_length > 6)
        reg_alpha_length = 6;
    int err = docmd_lxasto(arg);
    reg_alpha_length = temp_alpha_length;
    return err;
}

int docmd_lxasto(arg_struct *arg) {
    /* This relates to LSTO the same way ASTO relates to STO. */
    vartype *s = new_string(reg_alpha, reg_alpha_length);
    if (s == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    int err;
    if (arg->type == ARGTYPE_IND_STK && arg->val.stk == 'X') {
        // Special case for LASTO IND ST X
        err = resolve_ind_arg(arg);
        if (err != ERR_NONE) {
            free_vartype(s);
            return err;
        }
    }
    vartype *saved_x;
    if (sp == -1) {
        saved_x = NULL;
        sp = 0;
    } else {
        saved_x = stack[sp];
    }
    stack[sp] = s;
    err = docmd_lsto(arg);
    free_vartype(s);
    if (saved_x == NULL)
        sp = -1;
    else
        stack[sp] = saved_x;
    return err;
}

int docmd_wsize(arg_struct *arg) {
    phloat x = ((vartype_real *) stack[sp])->x;
#ifdef BCD_MATH
    if (x >= 65 || x < 1)
#else
    if (x >= 53 || x < 1)
#endif
        return ERR_INVALID_DATA;
    mode_wsize = to_int(x);
    print_trace();
    return ERR_NONE;
}

int docmd_wsize_t(arg_struct *arg) {
    vartype *new_x = new_real(effective_wsize());
    if (new_x == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(new_x);
}

int docmd_bsigned(arg_struct *arg) {
    flags.f.base_signed = !flags.f.base_signed;
    return ERR_NONE;
}

int docmd_bwrap(arg_struct *arg) {
    flags.f.base_wrap = !flags.f.base_wrap;
    return ERR_NONE;
}

int docmd_breset(arg_struct *arg) {
    mode_wsize = 36;
    flags.f.base_signed = 1;
    flags.f.base_wrap = 0;
    return ERR_NONE;
}

////////////////////////////////////////////////////////
///// The NOP that's been missing since the HP-41C /////
////////////////////////////////////////////////////////

int docmd_nop(arg_struct *arg) {
    return ERR_NONE;
}

//////////////////////////////
///// Fused Multiply-Add /////
//////////////////////////////

int docmd_fma(arg_struct *arg) {
    phloat x = ((vartype_real *) stack[sp])->x;
    phloat y = ((vartype_real *) stack[sp - 1])->x;
    phloat z = ((vartype_real *) stack[sp - 2])->x;
    phloat r = fma(z, y, x);
    int inf = p_isinf(r);
    if (inf != 0) {
        if (flags.f.range_error_ignore)
            r = inf == 1 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    vartype *res = new_real(r);
    if (res == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return ternary_result(res);
}

int docmd_func(arg_struct *arg) {
    return push_func_state(arg->val.num);
}

int docmd_rtnyes(arg_struct *arg) {
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    int err = pop_func_state(false);
    if (err != ERR_NONE)
        return err;
    return rtn(ERR_YES);
}

int docmd_rtnno(arg_struct *arg) {
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    int err = pop_func_state(false);
    if (err != ERR_NONE)
        return err;
    return rtn(ERR_NO);
}

int docmd_rtnerr(arg_struct *arg) {
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    int err;
    int4 e;
    err = arg_to_num(arg, &e);
    if (err != ERR_NONE)
        return err;
    if (e >= ERR_SIZE_ERROR)
        return ERR_INVALID_DATA;
    err = pop_func_state(true);
    if (err != ERR_NONE)
        return err;
    err = to_int(e);
    if (err != ERR_NONE && flags.f.error_ignore) {
        flags.f.error_ignore = 0;
        err = ERR_NONE;
    }
    if (err != ERR_NONE)
        return rtn_with_error(err);
    else
        return rtn(ERR_NONE);
}

int docmd_strace(arg_struct *arg) {
    flags.f.trace_print = 1;
    flags.f.normal_print = 1;
    return ERR_NONE;
}

/////////////////////
///// Big Stack /////
/////////////////////

int docmd_4stk(arg_struct *arg) {
    if (!flags.f.big_stack)
        return core_settings.allow_big_stack ? ERR_NONE : ERR_BIG_STACK_DISABLED;
    // Should be safe to assume the stack always has capacity >= 4
    if (sp < 3) {
        int off = 3 - sp;
        memmove(stack + off, stack, (sp + 1) * sizeof(vartype *));
        for (int i = 0; i < off; i++) {
            stack[i] = new_real(0);
            if (stack[i] == NULL) {
                for (int j = 0; j < i; j++)
                    free_vartype(stack[j]);
                memmove(stack, stack + off, (sp + 1) * sizeof(vartype *));
                return ERR_INSUFFICIENT_MEMORY;
            }
        }
    } else if (sp > 3) {
        int off = sp - 3;
        for (int i = 0; i < off; i++)
            free_vartype(stack[i]);
        memmove(stack, stack + off, 4 * sizeof(vartype *));
    }
    sp = 3;
    flags.f.big_stack = 0;
    if (arg != NULL)
        shrink_stack();
    return ERR_NONE;
}

int docmd_l4stk(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    return push_stack_state(false);
}

int docmd_nstk(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    flags.f.big_stack = 1;
    return ERR_NONE;
}

int docmd_lnstk(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    return push_stack_state(true);
}

int docmd_depth(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    vartype *v = new_real(sp + 1);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

int docmd_drop(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    if (sp == -1)
        return ERR_NONE;
    free_vartype(stack[sp]);
    if (flags.f.big_stack) {
        sp--;
    } else {
        memmove(stack + 1, stack, 3 * sizeof(vartype *));
        stack[REG_T] = new_real(0);
    }
    print_trace();
    return ERR_NONE;
}

int docmd_dropn(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    int4 n;
    int err = arg_to_num(arg, &n);
    if (err != ERR_NONE)
        return err;
    if (n > sp + 1)
        return ERR_SIZE_ERROR;
    for (int i = sp - n + 1; i <= sp; i++)
        free_vartype(stack[i]);
    if (flags.f.big_stack) {
        sp -= n;
    } else {
        memmove(stack + n, stack, (n - 4) * sizeof(vartype *));
        for (int i = 0; i < n; i++)
            stack[i] = new_real(0);
    }
    print_trace();
    return ERR_NONE;
}

int docmd_dup(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    vartype *v = dup_vartype(stack[sp]);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    char prev_stack_lift = flags.f.stack_lift_disable;
    flags.f.stack_lift_disable = 0;
    if (recall_result_silently(v) != ERR_NONE) {
        flags.f.stack_lift_disable = prev_stack_lift;
        return ERR_INSUFFICIENT_MEMORY;
    }
    print_stack_trace();
    return ERR_NONE;
}

int docmd_dupn(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    int4 n;
    int err = arg_to_num(arg, &n);
    if (err != ERR_NONE)
        return err;
    if (flags.f.big_stack) {
        if (n > sp + 1)
            return ERR_SIZE_ERROR;
        if (!ensure_stack_capacity(n))
            return ERR_INSUFFICIENT_MEMORY;
        for (int i = 1; i <= n; i++) {
            stack[sp + i] = dup_vartype(stack[sp + i - n]);
            if (stack[sp + i] == NULL) {
                while (--i >= 1)
                    free_vartype(stack[sp + i]);
                return ERR_INSUFFICIENT_MEMORY;
            }
        }
        sp += n;
    } else {
        switch (n) {
            case 0:
                break;
            case 1:
                return docmd_dup(NULL);
            case 2:
                vartype *v0, *v1;
                v0 = dup_vartype(stack[REG_X]);
                if (v0 == NULL)
                    return ERR_INSUFFICIENT_MEMORY;
                v1 = dup_vartype(stack[REG_Y]);
                if (v1 == NULL) {
                    free_vartype(v0);
                    return ERR_INSUFFICIENT_MEMORY;
                }
                free_vartype(stack[REG_Z]);
                free_vartype(stack[REG_T]);
                stack[REG_Z] = v1;
                stack[REG_T] = v0;
                break;
            default:
                return ERR_SIZE_ERROR;
        }
    }
    print_stack_trace();
    return ERR_NONE;
}

int docmd_pick(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    int4 n;
    int err = arg_to_num(arg, &n);
    if (err != ERR_NONE)
        return err;
    if (n == 0)
        return ERR_NONEXISTENT;
    n--;
    if (n > sp)
        return ERR_SIZE_ERROR;
    vartype *v = dup_vartype(stack[sp - n]);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

int docmd_unpick(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    int4 n;
    int err = arg_to_num(arg, &n);
    if (err != ERR_NONE)
        return err;
    if (n == 0)
        return ERR_NONEXISTENT;
    n--;
    if (n > (flags.f.big_stack ? sp - 1 : sp))
        return ERR_SIZE_ERROR;
    // Note: UNPICK consumes X, i.e. drops it from the stack. This is unlike
    // any other STO-like function in Free42, but it is needed in order to make
    // PICK and UNPICK work as a pair like they do in the RPL calculators.
    vartype *v = stack[sp];
    if (flags.f.big_stack) {
        sp--;
    } else {
        vartype *t = dup_vartype(stack[REG_T]);
        if (t == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        memmove(stack + 1, stack, 3 * sizeof(vartype *));
        stack[REG_T] = t;
    }
    free_vartype(stack[sp - n]);
    stack[sp - n] = v;
    print_stack_trace();
    return ERR_NONE;
}

int docmd_rdnn(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    int4 n;
    int err = arg_to_num(arg, &n);
    if (err != ERR_NONE)
        return err;
    if (n < 2)
        goto done;
    if (n > sp + 1)
        return ERR_SIZE_ERROR;
    vartype *v;
    v = stack[sp];
    memmove(stack + sp - n + 2, stack + sp - n + 1, (n - 1) * sizeof(vartype *));
    stack[sp - n + 1] = v;
    done:
    print_trace();
    return ERR_NONE;
}

int docmd_rupn(arg_struct *arg) {
    if (!core_settings.allow_big_stack)
        return ERR_BIG_STACK_DISABLED;
    int4 n;
    int err = arg_to_num(arg, &n);
    if (err != ERR_NONE)
        return err;
    if (n < 2)
        goto done;
    if (n > sp + 1)
        return ERR_SIZE_ERROR;
    vartype *v;
    v = stack[sp - n + 1];
    memmove(stack + sp - n + 1, stack + sp - n + 2, (n - 1) * sizeof(vartype *));
    stack[sp] = v;
    done:
    print_trace();
    return ERR_NONE;
}

////////////////////////
///// MVAR Catalog /////
////////////////////////

int docmd_mvarcat(arg_struct *arg) {
    int n = 0;
    vartype *res;
    for (int i = 0; i < labels_count; i++)
        if (label_has_mvar(i))
            n++;
    if (n == 0) {
        res = new_real(0);
        if (res == NULL)
            return ERR_INSUFFICIENT_MEMORY;
    } else {
        res = new_realmatrix(n, 1);
        if (res == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        vartype_realmatrix *rm = (vartype_realmatrix *) res;
        for (int i = 0; i < labels_count; i++)
            if (label_has_mvar(i)) {
                n--;
                rm->array->is_string[n] = 1;
                char *d = (char *) &rm->array->data[n];
                d[0] = labels[i].length;
                memcpy(d + 1, labels[i].name, labels[i].length);
            }
    }
    return recall_result(res);
}

///////////////////////////////////
///// String & List Functions /////
///////////////////////////////////

static int concat(bool extend) {
    if (stack[sp - 1]->type == TYPE_STRING) {
        char *text;
        int len;
        char buf[44];
        int templen;
        if (stack[sp]->type == TYPE_STRING) {
            vartype_string *s = (vartype_string *) stack[sp];
            text = s->txt();
            len = s->length;
        } else {
            memcpy(buf, reg_alpha, reg_alpha_length);
            templen = reg_alpha_length;
            reg_alpha_length = 0;
            arg_struct arg;
            arg.type = ARGTYPE_STK;
            arg.val.stk = 'X';
            docmd_arcl(&arg);
            text = reg_alpha;
            len = reg_alpha_length;
        }
        vartype_string *s = (vartype_string *) stack[sp - 1];
        vartype *v = new_string(NULL, s->length + len);
        if (v != NULL) {
            vartype_string *s2 = (vartype_string *) v;
            memcpy(s2->txt(), s->txt(), s->length);
            memcpy(s2->txt() + s->length, text, len);
        }
        if (text == reg_alpha) {
            memcpy(reg_alpha, buf, templen);
            reg_alpha_length = templen;
        }
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        binary_result(v);
        return ERR_NONE;
    } else if (stack[sp - 1]->type == TYPE_LIST) {
        vartype *v = dup_vartype(stack[sp]);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        vartype_list *list = (vartype_list *) stack[sp - 1];
        if (!disentangle((vartype *) list)) {
            nomem:
            free_vartype(v);
            return ERR_INSUFFICIENT_MEMORY;
        }
        if (extend && v->type == TYPE_LIST) {
            if (!disentangle(v))
                goto nomem;
            vartype_list *list2 = (vartype_list *) v;
            if (list2->size > 0) {
                vartype **new_data = (vartype **) realloc(list->array->data, (list->size + list2->size) * sizeof(vartype *));
                if (new_data == NULL)
                    goto nomem;
                list->array->data = new_data;
                memcpy(list->array->data + list->size, list2->array->data, list2->size * sizeof(vartype *));
                memset(list2->array->data, 0, list2->size * sizeof(vartype *));
                list->size += list2->size;
            }
            free_vartype(v);
            stack[sp - 1] = NULL;
            binary_result((vartype *) list);
            return ERR_NONE;
        }
        vartype **new_data = (vartype **) realloc(list->array->data, (list->size + 1) * sizeof(vartype *));
        if (new_data == NULL)
            goto nomem;
        list->array->data = new_data;
        list->array->data[list->size++] = v;
        stack[sp - 1] = NULL;
        binary_result((vartype *) list);
        return ERR_NONE;
    } else {
        return ERR_INVALID_TYPE;
    }
}

int docmd_append(arg_struct *arg) {
    // APPEND: adds the object in X to the string or list in Y and returns the
    // combined string or list. If Y is a string, the contents of X will be converted
    // to a string in the same way as ARCL. If Y is a list, X will be added to it
    // unchanged. If X is a list, it will be added to Y as one element.
    return concat(false);
}

int docmd_extend(arg_struct *arg) {
    // EXTEND: adds the object in X to the string or list in Y and returns the
    // combined string or list. If Y is a string, the contents of X will be converted
    // to a string in the same way as ARCL. If Y is a list, X will be added to it
    // unchanged. If X is a list, it will be added to Y element by element.
    return concat(true);
}

int docmd_substr(arg_struct *arg) {
    // SUBSTR: from the string or list in Z, gets the substring/sublist starting at
    // index Y and ending at index X. If X and/or Y are negative, they are counts from
    // the end, rather than the beginning. The very end of the string or list can be
    // specified by leaving off the 'end' parameter, i.e. by having the string or list
    // in Y and the starting index in X.
    if (sp + 1 < 2)
        return ERR_TOO_FEW_ARGUMENTS;
    vartype *s, *b, *e;
    if (stack[sp - 1]->type == TYPE_STRING || stack[sp - 1]->type == TYPE_LIST) {
        s = stack[sp - 1];
        b = stack[sp];
        e = NULL;
    } else {
        if (sp + 1 < 3)
            return ERR_TOO_FEW_ARGUMENTS;
        s = stack[sp - 2];
        b = stack[sp - 1];
        e = stack[sp];
        if (s->type != TYPE_STRING && s->type != TYPE_LIST)
            return ERR_INVALID_TYPE;
    }
    if (b->type != TYPE_REAL || e != NULL && e->type != TYPE_REAL)
        return ERR_INVALID_TYPE;
    phloat bp = ((vartype_real *) b)->x;
    if (bp <= -2147483648.0 || bp >= 2147483648.0)
        return ERR_INVALID_DATA;
    int4 begin = to_int4(bp);
    phloat ep;
    int4 end;
    if (e != NULL) {
        ep = ((vartype_real *) e)->x;
        if (bp <= -2147483648.0 || bp >= 2147483648.0)
            return ERR_INVALID_DATA;
        end = to_int4(ep);
    }
    int4 len = s->type == TYPE_STRING ? ((vartype_string *) s)->length
                : ((vartype_list *) s)->size;
    if (begin < 0)
        begin += len;
    if (e == NULL)
        end = len;
    else if (end < 0)
        end += len;
    if (begin < 0 || begin > end || end > len)
        return ERR_INVALID_DATA;
    int4 newlen = end - begin;
    vartype *v;
    if (newlen == len) {
        v = s;
        stack[sp - (e == NULL ? 1 : 2)] = NULL;
    } if (s->type == TYPE_STRING) {
        vartype_string *str = (vartype_string *) s;
        char *text = str->txt();
        v = new_string(text + begin, newlen);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
    } else {
        vartype_list *list = (vartype_list *) s;
        vartype_list *r = (vartype_list *) new_list(newlen);
        if (r == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        for (int i = 0; i < newlen; i++) {
            r->array->data[i] = dup_vartype(list->array->data[begin + i]);
            if (r->array->data[i] == NULL) {
                free_vartype((vartype *) r);
                return ERR_INSUFFICIENT_MEMORY;
            }
        }
        v = (vartype *) r;
    }
    if (e == NULL) {
        binary_result(v);
        return ERR_NONE;
    } else {
        return ternary_result(v);
    }
}

int docmd_length(arg_struct *arg) {
    // LENGTH: returns the length of the string or list in X.
    int4 len;
    if (stack[sp]->type == TYPE_STRING)
        len = ((vartype_string *) stack[sp])->length;
    else
        len = ((vartype_list *) stack[sp])->size;
    vartype *v = new_real(len);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(v);
    return ERR_NONE;
}

int docmd_head(arg_struct *arg) {
    // HEAD <param>: removes and returns the first character or element from the
    // string or list named by <param>. If the string or list is empty, skip the next
    // instruction.
    int err;
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    if (!ensure_stack_capacity(1))
        return ERR_INSUFFICIENT_MEMORY;
    vartype *s, *v;
    switch (arg->type) {
        case ARGTYPE_NUM: {
            vartype *regs = recall_var("REGS", 4);
            if (regs == NULL)
                return ERR_SIZE_ERROR;
            if (regs->type != TYPE_REALMATRIX)
                return ERR_INVALID_TYPE;
            vartype_realmatrix *rm = (vartype_realmatrix *) regs;
            int4 sz = rm->rows * rm->columns;
            int4 n = arg->val.num;
            if (n >= sz)
                return ERR_SIZE_ERROR;
            if (rm->array->is_string[n] == 0)
                return ERR_INVALID_TYPE;
            char *text;
            int len;
            get_matrix_string(rm, n, &text, &len);
            if (len == 0)
                return ERR_NO;
            if (!disentangle(regs))
                return ERR_INSUFFICIENT_MEMORY;
            get_matrix_string(rm, n, &text, &len);
            v = new_string(text, 1);
            if (v == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            if (!put_matrix_string(rm, n, text + 1, len - 1)) {
                free_vartype(v);
                return ERR_INSUFFICIENT_MEMORY;
            }
            err = recall_result(v);
            return err == ERR_NONE ? ERR_YES : err;
        }
        case ARGTYPE_STK: {
            int idx;
            switch (arg->val.stk) {
                case 'X': idx = 0; break;
                case 'Y': idx = 1; break;
                case 'Z': idx = 2; break;
                case 'T': idx = 3; break;
                case 'L': idx = -1; break;
            }
            if (idx == -1) {
                s = lastx;
            } else {
                if (idx > sp)
                    return ERR_NONEXISTENT;
                s = stack[sp - idx];
            }
            doit:
            if (s->type == TYPE_STRING) {
                vartype_string *str = (vartype_string *) s;
                if (str->length == 0)
                    return ERR_NO;
                v = new_string(str->txt(), 1);
                if (v == NULL)
                    return ERR_INSUFFICIENT_MEMORY;
                str->trim1();
                err = recall_result(v);
                return err == ERR_NONE ? ERR_YES : err;
            } else if (s->type == TYPE_LIST) {
                vartype_list *list = (vartype_list *) s;
                if (list->size == 0)
                    return ERR_NO;
                if (!disentangle(s))
                    return ERR_INSUFFICIENT_MEMORY;
                v = list->array->data[0];
                memmove(list->array->data, list->array->data + 1, --list->size * sizeof(vartype *));
                err = recall_result(v);
                return err == ERR_NONE ? ERR_YES : err;
            } else {
                return ERR_INVALID_TYPE;
            }
        }
        case ARGTYPE_STR: {
            s = recall_var(arg->val.text, arg->length);
            if (s == NULL)
                return ERR_NONEXISTENT;
            goto doit;
        }
        default:
            return ERR_INTERNAL_ERROR;
    }
}

int docmd_rev(arg_struct *arg) {
    // REV: reverse the string or list in X
    vartype *v;
    if (stack[sp]->type == TYPE_STRING) {
        vartype_string *src = (vartype_string *) stack[sp];
        int4 len = src->length;
        v = new_string(NULL, len);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        vartype_string *dst = (vartype_string *) v;
        char *s = src->txt();
        char *d = dst->txt() + len - 1;
        while (len-- > 0)
            *d-- = *s++;
    } else {
        vartype_list *src = (vartype_list *) stack[sp];
        int4 len = src->size;
        v = new_list(len);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        vartype_list *dst = (vartype_list *) v;
        vartype **s = src->array->data;
        vartype **d = dst->array->data + len - 1;
        while (len-- > 0) {
            vartype *t = dup_vartype(*s++);
            if (t == NULL) {
                free_vartype(v);
                return ERR_INSUFFICIENT_MEMORY;
            }
            *d-- = t;
        }
    }
    unary_result(v);
    return ERR_NONE;
}

int docmd_pos(arg_struct *arg) {
    // POS: finds the first occurrence of the string or list X in Y. Or with three
    // parameters: find the first occurrence of string or list X in Z, starting the
    // search from position Y.
    int pos, startpos;
    int list_sp;
    bool ternary;
    if (stack[sp - 1]->type == TYPE_REAL) {
        phloat start = ((vartype_real *) stack[sp - 1])->x;
        if (start < -2147483648.0 || start > 2147483648.0) {
            startpos = -2;
        } else {
            startpos = to_int(start);
            if (startpos < 0)
                startpos = -startpos;
        }
        list_sp = sp - 2;
        ternary = true;
    } else {
        startpos = 0;
        list_sp = sp - 1;
        ternary = false;
    }
    if (stack[list_sp]->type == TYPE_STRING) {
        if (stack[sp]->type != TYPE_STRING && stack[sp]->type != TYPE_REAL)
            return ERR_INVALID_TYPE;
        if (startpos == -2)
            return ERR_INVALID_DATA;
        vartype_string *s = (vartype_string *) stack[list_sp];
        pos = string_pos(s->txt(), s->length, stack[sp], startpos);
        if (pos == -2)
            return ERR_INVALID_DATA;
    } else if (stack[list_sp]->type == TYPE_LIST) {
        if (startpos == -2)
            return ERR_INVALID_DATA;
        vartype_list *list = (vartype_list *) stack[list_sp];
        pos = -1;
        for (int4 i = startpos; i < list->size; i++) {
            if (vartype_equals(list->array->data[i], stack[sp])) {
                pos = i;
                break;
            }
        }
    } else {
        return ERR_INVALID_TYPE;
    }
    vartype *v = new_real(pos);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    if (ternary) {
        return ternary_result(v);
    } else {
        binary_result(v);
        return ERR_NONE;
    }
}

int docmd_s_to_n(arg_struct *arg) {
    // S->N: convert string to number, like ANUM
    phloat res;
    vartype_string *s = (vartype_string *) stack[sp];
    if (!anum(s->txt(), s->length, &res))
        return ERR_INVALID_DATA;
    vartype *v = new_real(res);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(v);
    return ERR_NONE;
}

int docmd_n_to_s(arg_struct *arg) {
    // N->S: convert number to string, like ARCL
    vartype *v;
    if (stack[sp]->type == TYPE_STRING) {
        v = dup_vartype(stack[sp]);
    } else {
        char buf[100];
        int bufptr = vartype2string(stack[sp], buf, 100);
        v = new_string(buf, bufptr);
    }
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(v);
    return ERR_NONE;
}

int docmd_c_to_n(arg_struct *arg) {
    // C->N: convert character to number, like ATOX
    vartype_string *s = (vartype_string *) stack[sp];
    int n;
    if (s->length == 0)
        n = 0;
    else
        n = (unsigned char) s->txt()[0];
    vartype *v = new_real(n);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(v);
    return ERR_NONE;
}

int docmd_n_to_c(arg_struct *arg) {
    // N->C: convert number to character, like XTOA
    phloat n = ((vartype_real *) stack[sp])->x;
    if (n < 0)
        n = -n;
    if (n >= 256)
        return ERR_INVALID_DATA;
    vartype_string *s = (vartype_string *) new_string(NULL, 1);
    if (s == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    s->txt()[0] = to_int(n);
    unary_result((vartype *) s);
    return ERR_NONE;
}

int docmd_list_t(arg_struct *arg) {
    return stack[sp]->type == TYPE_LIST ? ERR_YES : ERR_NO;
}

int docmd_newlist(arg_struct *arg) {
    vartype *v = new_list(0);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

int docmd_newstr(arg_struct *arg) {
    vartype *v = new_string("", 0);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}
