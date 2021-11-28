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
#include <stdint.h>
#include <string>

#include "core_globals.h"
#include "core_commands2.h"
#include "core_commands4.h"
#include "core_commands7.h"
#include "core_display.h"
#include "core_equations.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_math1.h"
#include "core_parser.h"
#include "core_tables.h"
#include "core_variables.h"
#include "shell.h"
#include "shell_spool.h"

#ifndef BCD_MATH
// We need these locally for BID128->double conversion
#include "bid_conf.h"
#include "bid_functions.h"
#endif

// File used for reading and writing the state file, and for importing and
// exporting programs. Since only one of these operations can be active at one
// time, having one FILE pointer for all of them is sufficient.
FILE *gfile = NULL;

const error_spec errors[] = {
    { /* NONE */                   NULL,                       0 },
    { /* ALPHA_DATA_IS_INVALID */  "Alpha Data Is Invalid",   21 },
    { /* OUT_OF_RANGE */           "Out of Range",            12 },
    { /* DIVIDE_BY_0 */            "Divide by 0",             11 },
    { /* INVALID_TYPE */           "Invalid Type",            12 },
    { /* INVALID_DATA */           "Invalid Data",            12 },
    { /* NONEXISTENT */            "Nonexistent",             11 },
    { /* DIMENSION_ERROR */        "Dimension Error",         15 },
    { /* TOO_FEW_ARGUMENTS */      "Too Few Arguments",       17 },
    { /* SIZE_ERROR */             "Size Error",              10 },
    { /* STACK_DEPTH_ERROR */      "Stack Depth Error",       17 },
    { /* RESTRICTED_OPERATION */   "Restricted Operation",    20 },
    { /* YES */                    "Yes",                      3 },
    { /* NO */                     "No",                       2 },
    { /* STOP */                   NULL,                       0 },
    { /* LABEL_NOT_FOUND */        "Label Not Found",         15 },
    { /* NO_REAL_VARIABLES */      "No Real Variables",       17 },
    { /* NO_COMPLEX_VARIABLES */   "No Complex Variables",    20 },
    { /* NO_MATRIX_VARIABLES */    "No Matrix Variables",     19 },
    { /* NO_EQUATION_VARIABLES */  "No Equation Variables",   21 },
    { /* NO_MENU_VARIABLES */      "No Menu Variables",       17 },
    { /* STAT_MATH_ERROR */        "Stat Math Error",         15 },
    { /* INVALID_FORECAST_MODEL */ "Invalid Forecast Model",  22 },
    { /* SINGULAR_MATRIX */        "Singular Matrix",         15 },
    { /* SOLVE_SOLVE */            "Solve(Solve)",            12 },
    { /* INTEG_INTEG */            "Integ(Integ)",            12 },
    { /* RUN */                    NULL,                       0 },
    { /* INTERRUPTED */            "Interrupted",             11 },
    { /* PRINTING_IS_DISABLED */   "Printing Is Disabled",    20 },
    { /* INTERRUPTIBLE */          NULL,                       0 },
    { /* NO_VARIABLES */           "No Variables",            12 },
    { /* INSUFFICIENT_MEMORY */    "Insufficient Memory",     19 },
    { /* NOT_YET_IMPLEMENTED */    "Not Yet Implemented",     19 },
    { /* INTERNAL_ERROR */         "Internal Error",          14 },
    { /* SUSPICIOUS_OFF */         "Suspicious OFF",          14 },
    { /* RTN_STACK_FULL */         "RTN Stack Full",          14 },
    { /* NUMBER_TOO_LARGE */       "Number Too Large",        16 },
    { /* NUMBER_TOO_SMALL */       "Number Too Small",        16 },
    { /* INVALID_CONTEXT */        "Invalid Context",         15 },
    { /* NAME_TOO_LONG */          "Name Too Long",           13 },
    { /* PARSE_ERROR */            "Parse Error",             11 },
    { /* INVALID_EQUATION */       "Invalid Equation",        16 }
};


const menu_spec menus[] = {
    { /* MENU_ALPHA1 */ MENU_NONE, MENU_ALPHA2, MENU_ALPHA2,
                      { { MENU_ALPHA_ABCDE1, 5, "ABCDE" },
                        { MENU_ALPHA_FGHI,   4, "FGHI"  },
                        { MENU_ALPHA_JKLM,   4, "JKLM"  },
                        { MENU_ALPHA_NOPQ1,  4, "NOPQ"  },
                        { MENU_ALPHA_RSTUV1, 5, "RSTUV" },
                        { MENU_ALPHA_WXYZ,   4, "WXYZ"  } } },
    { /* MENU_ALPHA2 */ MENU_NONE, MENU_ALPHA1, MENU_ALPHA1,
                      { { MENU_ALPHA_PAREN, 5, "( [ {"     },
                        { MENU_ALPHA_ARROW, 3, "\020^\016" },
                        { MENU_ALPHA_COMP,  5, "< = >"     },
                        { MENU_ALPHA_MATH,  4, "MATH"      },
                        { MENU_ALPHA_PUNC1, 4, "PUNC"      },
                        { MENU_ALPHA_MISC1, 4, "MISC"      } } },
    { /* MENU_ALPHA_ABCDE1 */ MENU_ALPHA1, MENU_ALPHA_ABCDE2, MENU_ALPHA_ABCDE2,
                      { { MENU_NONE, 1, "A" },
                        { MENU_NONE, 1, "B" },
                        { MENU_NONE, 1, "C" },
                        { MENU_NONE, 1, "D" },
                        { MENU_NONE, 1, "E" },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_ABCDE2 */ MENU_ALPHA1, MENU_ALPHA_ABCDE1, MENU_ALPHA_ABCDE1,
                      { { MENU_NONE, 1, "\026" },
                        { MENU_NONE, 1, "\024" },
                        { MENU_NONE, 1, "\031" },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    } } },
    { /* MENU_ALPHA_FGHI */ MENU_ALPHA1, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "F" },
                        { MENU_NONE, 1, "G" },
                        { MENU_NONE, 1, "H" },
                        { MENU_NONE, 1, "I" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_JKLM */ MENU_ALPHA1, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "J" },
                        { MENU_NONE, 1, "K" },
                        { MENU_NONE, 1, "L" },
                        { MENU_NONE, 1, "M" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_NOPQ1 */ MENU_ALPHA1, MENU_ALPHA_NOPQ2, MENU_ALPHA_NOPQ2,
                      { { MENU_NONE, 1, "N" },
                        { MENU_NONE, 1, "O" },
                        { MENU_NONE, 1, "P" },
                        { MENU_NONE, 1, "Q" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_NOPQ2 */ MENU_ALPHA1, MENU_ALPHA_NOPQ1, MENU_ALPHA_NOPQ1,
                      { { MENU_NONE, 1, "\025" },
                        { MENU_NONE, 1, "\034" },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    } } },
    { /* MENU_ALPHA_RSTUV1 */ MENU_ALPHA1, MENU_ALPHA_RSTUV2, MENU_ALPHA_RSTUV2,
                      { { MENU_NONE, 1, "R" },
                        { MENU_NONE, 1, "S" },
                        { MENU_NONE, 1, "T" },
                        { MENU_NONE, 1, "U" },
                        { MENU_NONE, 1, "V" },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_RSTUV2 */ MENU_ALPHA1, MENU_ALPHA_RSTUV1, MENU_ALPHA_RSTUV1,
                      { { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, "\035" },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    } } },
    { /* MENU_ALPHA_WXYZ */ MENU_ALPHA1, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "W" },
                        { MENU_NONE, 1, "X" },
                        { MENU_NONE, 1, "Y" },
                        { MENU_NONE, 1, "Z" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_PAREN */ MENU_ALPHA2, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "(" },
                        { MENU_NONE, 1, ")" },
                        { MENU_NONE, 1, "[" },
                        { MENU_NONE, 1, "]" },
                        { MENU_NONE, 1, "{" },
                        { MENU_NONE, 1, "}" } } },
    { /* MENU_ALPHA_ARROW */ MENU_ALPHA2, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "\020" },
                        { MENU_NONE, 1, "^"    },
                        { MENU_NONE, 1, "\016" },
                        { MENU_NONE, 1, "\017" },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    } } },
    { /* MENU_ALPHA_COMP */ MENU_ALPHA2, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "="    },
                        { MENU_NONE, 1, "\014" },
                        { MENU_NONE, 1, "<"    },
                        { MENU_NONE, 1, ">"    },
                        { MENU_NONE, 1, "\011" },
                        { MENU_NONE, 1, "\013" } } },
    { /* MENU_ALPHA_MATH */ MENU_ALPHA2, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "\005" },
                        { MENU_NONE, 1, "\003" },
                        { MENU_NONE, 1, "\002" },
                        { MENU_NONE, 1, "\027" },
                        { MENU_NONE, 1, "\023" },
                        { MENU_NONE, 1, "\021" } } },
    { /* MENU_ALPHA_PUNC1 */ MENU_ALPHA2, MENU_ALPHA_PUNC2, MENU_ALPHA_PUNC2,
                      { { MENU_NONE, 1, ","  },
                        { MENU_NONE, 1, ";"  },
                        { MENU_NONE, 1, ":"  },
                        { MENU_NONE, 1, "!"  },
                        { MENU_NONE, 1, "?"  },
                        { MENU_NONE, 1, "\"" } } },
    { /* MENU_ALPHA_PUNC2 */ MENU_ALPHA2, MENU_ALPHA_PUNC1, MENU_ALPHA_PUNC1,
                      { { MENU_NONE, 1, "\032" },
                        { MENU_NONE, 1, "_"    },
                        { MENU_NONE, 1, "`"    },
                        { MENU_NONE, 1, "'"    },
                        { MENU_NONE, 1, "\010" },
                        { MENU_NONE, 1, "\012" } } },
    { /* MENU_ALPHA_MISC1 */ MENU_ALPHA2, MENU_ALPHA_MISC2, MENU_ALPHA_MISC2,
                      { { MENU_NONE, 1, "$"    },
                        { MENU_NONE, 1, "*"    },
                        { MENU_NONE, 1, "#"    },
                        { MENU_NONE, 1, "/"    },
                        { MENU_NONE, 1, "\037" },
                        { MENU_NONE, 1, " "    } } },
    { /* MENU_ALPHA_MISC2 */ MENU_ALPHA2, MENU_ALPHA_MISC1, MENU_ALPHA_MISC1,
                      { { MENU_NONE, 1, "\022" },
                        { MENU_NONE, 1, "&"    },
                        { MENU_NONE, 1, "@"    },
                        { MENU_NONE, 1, "\\"   },
                        { MENU_NONE, 1, "~"    },
                        { MENU_NONE, 1, "|"    } } },
    { /* MENU_ST */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 4, "ST L" },
                        { MENU_NONE, 4, "ST X" },
                        { MENU_NONE, 4, "ST Y" },
                        { MENU_NONE, 4, "ST Z" },
                        { MENU_NONE, 4, "ST T" },
                        { MENU_NONE, 0, ""     } } },
    { /* MENU_IND_ST */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 3, "IND"  },
                        { MENU_NONE, 4, "ST L" },
                        { MENU_NONE, 4, "ST X" },
                        { MENU_NONE, 4, "ST Y" },
                        { MENU_NONE, 4, "ST Z" },
                        { MENU_NONE, 4, "ST T" } } },
    { /* MENU_IND */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 3, "IND"  },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" } } },
    { /* MENU_MODES1 */ MENU_NONE, MENU_MODES2, MENU_MODES5,
                      { { 0x2000 + CMD_DEG,   0, "" },
                        { 0x2000 + CMD_RAD,   0, "" },
                        { 0x2000 + CMD_GRAD,  0, "" },
                        { 0x1000 + CMD_NULL,  0, "" },
                        { 0x2000 + CMD_RECT,  0, "" },
                        { 0x2000 + CMD_POLAR, 0, "" } } },
    { /* MENU_MODES2 */ MENU_NONE, MENU_MODES3, MENU_MODES1,
                      { { 0x1000 + CMD_SIZE,    0, "" },
                        { 0x2000 + CMD_QUIET,   0, "" },
                        { 0x2000 + CMD_CPXRES,  0, "" },
                        { 0x2000 + CMD_REALRES, 0, "" },
                        { 0x2000 + CMD_KEYASN,  0, "" },
                        { 0x2000 + CMD_LCLBL,   0, "" } } },
    { /* MENU_MODES3 */ MENU_NONE, MENU_MODES4, MENU_MODES2,
                      { { 0x1000 + CMD_WSIZE,   0, "" },
                        { 0x1000 + CMD_WSIZE_T, 0, "" },
                        { 0x2000 + CMD_BSIGNED, 0, "" },
                        { 0x2000 + CMD_BWRAP,   0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x1000 + CMD_BRESET,  0, "" } } },
    { /* MENU_MODES4 */ MENU_NONE, MENU_MODES5, MENU_MODES3,
                      { { 0x2000 + CMD_MDY,     0, "" },
                        { 0x2000 + CMD_DMY,     0, "" },
                        { 0x2000 + CMD_YMD,     0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x2000 + CMD_CLK12,   0, "" },
                        { 0x2000 + CMD_CLK24,   0, "" } } },
    { /* MENU_MODES5 */ MENU_NONE, MENU_MODES1, MENU_MODES4,
                      { { 0x2000 + CMD_4STK,    0, "" },
                        { 0x2000 + CMD_NSTK,    0, "" },
                        { 0x2000 + CMD_CAPS,    0, "" },
                        { 0x2000 + CMD_MIXED,   0, "" },
                        { 0x2000 + CMD_STD,     0, "" },
                        { 0x2000 + CMD_COMP,    0, "" } } },
    { /* MENU_DISP */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x2000 + CMD_FIX,      0, "" },
                        { 0x2000 + CMD_SCI,      0, "" },
                        { 0x2000 + CMD_ENG,      0, "" },
                        { 0x2000 + CMD_ALL,      0, "" },
                        { 0x2000 + CMD_RDXDOT,   0, "" },
                        { 0x2000 + CMD_RDXCOMMA, 0, "" } } },
    { /* MENU_CLEAR1 */ MENU_NONE, MENU_CLEAR2, MENU_CLEAR2,
                      { { 0x1000 + CMD_CLSIGMA, 0, "" },
                        { 0x1000 + CMD_CLP,     0, "" },
                        { 0x1000 + CMD_CLV,     0, "" },
                        { 0x1000 + CMD_CLST,    0, "" },
                        { 0x1000 + CMD_CLA,     0, "" },
                        { 0x1000 + CMD_CLX,     0, "" } } },
    { /* MENU_CLEAR2 */ MENU_NONE, MENU_CLEAR1, MENU_CLEAR1,
                      { { 0x1000 + CMD_CLRG,   0, "" },
                        { 0x1000 + CMD_DEL,    0, "" },
                        { 0x1000 + CMD_CLKEYS, 0, "" },
                        { 0x1000 + CMD_CLLCD,  0, "" },
                        { 0x1000 + CMD_CLMENU, 0, "" },
                        { 0x1000 + CMD_CLALLa, 0, "" } } },
    { /* MENU_CONVERT1 */ MENU_NONE, MENU_CONVERT2, MENU_CONVERT2,
                      { { 0x1000 + CMD_TO_DEG, 0, "" },
                        { 0x1000 + CMD_TO_RAD, 0, "" },
                        { 0x1000 + CMD_TO_HR,  0, "" },
                        { 0x1000 + CMD_TO_HMS, 0, "" },
                        { 0x1000 + CMD_TO_REC, 0, "" },
                        { 0x1000 + CMD_TO_POL, 0, "" } } },
    { /* MENU_CONVERT2 */ MENU_NONE, MENU_CONVERT1, MENU_CONVERT1,
                      { { 0x1000 + CMD_IP,   0, "" },
                        { 0x1000 + CMD_FP,   0, "" },
                        { 0x1000 + CMD_RND,  0, "" },
                        { 0x1000 + CMD_ABS,  0, "" },
                        { 0x1000 + CMD_SIGN, 0, "" },
                        { 0x1000 + CMD_MOD,  0, "" } } },
    { /* MENU_FLAGS */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_SF,    0, "" },
                        { 0x1000 + CMD_CF,    0, "" },
                        { 0x1000 + CMD_FS_T,  0, "" },
                        { 0x1000 + CMD_FC_T,  0, "" },
                        { 0x1000 + CMD_FSC_T, 0, "" },
                        { 0x1000 + CMD_FCC_T, 0, "" } } },
    { /* MENU_PROB */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_COMB,  0, "" },
                        { 0x1000 + CMD_PERM,  0, "" },
                        { 0x1000 + CMD_FACT,  0, "" },
                        { 0x1000 + CMD_GAMMA, 0, "" },
                        { 0x1000 + CMD_RAN,   0, "" },
                        { 0x1000 + CMD_SEED,  0, "" } } },
    { /* MENU_CUSTOM1 */ MENU_NONE, MENU_CUSTOM2, MENU_CUSTOM3,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_CUSTOM2 */ MENU_NONE, MENU_CUSTOM3, MENU_CUSTOM1,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_CUSTOM3 */ MENU_NONE, MENU_CUSTOM1, MENU_CUSTOM2,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_PGM_FCN1 */ MENU_NONE, MENU_PGM_FCN2, MENU_PGM_FCN4,
                      { { 0x1000 + CMD_LBL,   0, "" },
                        { 0x1000 + CMD_RTN,   0, "" },
                        { 0x1000 + CMD_INPUT, 0, "" },
                        { 0x1000 + CMD_VIEW,  0, "" },
                        { 0x1000 + CMD_AVIEW, 0, "" },
                        { 0x1000 + CMD_XEQ,   0, "" } } },
    { /* MENU_PGM_FCN2 */ MENU_NONE, MENU_PGM_FCN3, MENU_PGM_FCN1,
                      { { MENU_PGM_XCOMP0,     3, "X?0" },
                        { MENU_PGM_XCOMPY,     3, "X?Y" },
                        { 0x1000 + CMD_PROMPT, 0, ""    },
                        { 0x1000 + CMD_PSE,    0, ""    },
                        { 0x1000 + CMD_ISG,    0, ""    },
                        { 0x1000 + CMD_DSE,    0, ""    } } },
    { /* MENU_PGM_FCN3 */ MENU_NONE, MENU_PGM_FCN4, MENU_PGM_FCN2,
                      { { 0x1000 + CMD_AIP,    0, "" },
                        { 0x1000 + CMD_XTOA,   0, "" },
                        { 0x1000 + CMD_AGRAPH, 0, "" },
                        { 0x1000 + CMD_PIXEL,  0, "" },
                        { 0x1000 + CMD_BEEP,   0, "" },
                        { 0x1000 + CMD_TONE,   0, "" } } },
    { /* MENU_PGM_FCN4 */ MENU_NONE, MENU_PGM_FCN1, MENU_PGM_FCN3,
                      { { 0x1000 + CMD_MVAR,    0, "" },
                        { 0x1000 + CMD_VARMENU, 0, "" },
                        { 0x1000 + CMD_GETKEY,  0, "" },
                        { 0x1000 + CMD_MENU,    0, "" },
                        { 0x1000 + CMD_KEYG,    0, "" },
                        { 0x1000 + CMD_KEYX,    0, "" } } },
    { /* MENU_PGM_XCOMP0 */ MENU_PGM_FCN2, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_X_EQ_0, 0, "" },
                        { 0x1000 + CMD_X_NE_0, 0, "" },
                        { 0x1000 + CMD_X_LT_0, 0, "" },
                        { 0x1000 + CMD_X_GT_0, 0, "" },
                        { 0x1000 + CMD_X_LE_0, 0, "" },
                        { 0x1000 + CMD_X_GE_0, 0, "" } } },
    { /* MENU_PGM_XCOMPY */ MENU_PGM_FCN2, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_X_EQ_Y, 0, "" },
                        { 0x1000 + CMD_X_NE_Y, 0, "" },
                        { 0x1000 + CMD_X_LT_Y, 0, "" },
                        { 0x1000 + CMD_X_GT_Y, 0, "" },
                        { 0x1000 + CMD_X_LE_Y, 0, "" },
                        { 0x1000 + CMD_X_GE_Y, 0, "" } } },
    { /* MENU_PRINT1 */ MENU_NONE, MENU_PRINT2, MENU_PRINT3,
                      { { 0x1000 + CMD_PRSIGMA, 0, "" },
                        { 0x1000 + CMD_PRP,     0, "" },
                        { 0x1000 + CMD_PRV,     0, "" },
                        { 0x1000 + CMD_PRSTK,   0, "" },
                        { 0x1000 + CMD_PRA,     0, "" },
                        { 0x1000 + CMD_PRX,     0, "" } } },
    { /* MENU_PRINT2 */ MENU_NONE, MENU_PRINT3, MENU_PRINT1,
                      { { 0x1000 + CMD_PRUSR, 0, "" },
                        { 0x1000 + CMD_LIST,  0, "" },
                        { 0x1000 + CMD_ADV,   0, "" },
                        { 0x1000 + CMD_PRLCD, 0, "" },
                        { 0x1000 + CMD_NULL,  0, "" },
                        { 0x1000 + CMD_DELAY, 0, "" } } },
    { /* MENU_PRINT3 */ MENU_NONE, MENU_PRINT1, MENU_PRINT2,
                      { { 0x2000 + CMD_PON,    0, "" },
                        { 0x2000 + CMD_POFF,   0, "" },
                        { 0x2000 + CMD_MAN,    0, "" },
                        { 0x2000 + CMD_NORM,   0, "" },
                        { 0x2000 + CMD_TRACE,  0, "" },
                        { 0x2000 + CMD_STRACE, 0, "" } } },
    { /* MENU_TOP_FCN */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_SIGMAADD, 0, "" },
                        { 0x1000 + CMD_INV,      0, "" },
                        { 0x1000 + CMD_SQRT,     0, "" },
                        { 0x1000 + CMD_LOG,      0, "" },
                        { 0x1000 + CMD_LN,       0, "" },
                        { 0x1000 + CMD_XEQ,      0, "" } } },
    { /* MENU_CATALOG */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_BLANK */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_PROGRAMMABLE */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_VARMENU */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_STAT1 */ MENU_NONE, MENU_STAT2, MENU_STAT2,
                      { { 0x1000 + CMD_SIGMAADD, 0, ""     },
                        { 0x1000 + CMD_SUM,      0, ""     },
                        { 0x1000 + CMD_MEAN,     0, ""     },
                        { 0x1000 + CMD_WMEAN,    0, ""     },
                        { 0x1000 + CMD_SDEV,     0, ""     },
                        { MENU_STAT_CFIT,        4, "CFIT" } } },
    { /* MENU_STAT2 */ MENU_NONE, MENU_STAT1, MENU_STAT1,
                      { { 0x2000 + CMD_ALLSIGMA,   0, "" },
                        { 0x2000 + CMD_LINSIGMA,   0, "" },
                        { 0x1000 + CMD_NULL,       0, "" },
                        { 0x1000 + CMD_NULL,       0, "" },
                        { 0x1000 + CMD_SIGMAREG,   0, "" },
                        { 0x1000 + CMD_SIGMAREG_T, 0, "" } } },
    { /* MENU_STAT_CFIT */ MENU_STAT1, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_FCSTX, 0, ""     },
                        { 0x1000 + CMD_FCSTY, 0, ""     },
                        { 0x1000 + CMD_SLOPE, 0, ""     },
                        { 0x1000 + CMD_YINT,  0, ""     },
                        { 0x1000 + CMD_CORR,  0, ""     },
                        { MENU_STAT_MODL,     4, "MODL" } } },
    { /* MENU_STAT_MODL */ MENU_STAT_CFIT, MENU_NONE, MENU_NONE,
                      { { 0x2000 + CMD_LINF, 0, "" },
                        { 0x2000 + CMD_LOGF, 0, "" },
                        { 0x2000 + CMD_EXPF, 0, "" },
                        { 0x2000 + CMD_PWRF, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_BEST, 0, "" } } },
    { /* MENU_MATRIX1 */ MENU_NONE, MENU_MATRIX2, MENU_MATRIX3,
                      { { 0x1000 + CMD_NEWMAT, 0, "" },
                        { 0x1000 + CMD_INVRT,  0, "" },
                        { 0x1000 + CMD_DET,    0, "" },
                        { 0x1000 + CMD_TRANS,  0, "" },
                        { 0x1000 + CMD_SIMQ,   0, "" },
                        { 0x1000 + CMD_EDIT,   0, "" } } },
    { /* MENU_MATRIX2 */ MENU_NONE, MENU_MATRIX3, MENU_MATRIX1,
                      { { 0x1000 + CMD_DOT,   0, "" },
                        { 0x1000 + CMD_CROSS, 0, "" },
                        { 0x1000 + CMD_UVEC,  0, "" },
                        { 0x1000 + CMD_DIM,   0, "" },
                        { 0x1000 + CMD_INDEX, 0, "" },
                        { 0x1000 + CMD_EDITN, 0, "" } } },
    { /* MENU_MATRIX3 */ MENU_NONE, MENU_MATRIX1, MENU_MATRIX2,
                      { { 0x1000 + CMD_STOIJ, 0, "" },
                        { 0x1000 + CMD_RCLIJ, 0, "" },
                        { 0x1000 + CMD_STOEL, 0, "" },
                        { 0x1000 + CMD_RCLEL, 0, "" },
                        { 0x1000 + CMD_PUTM,  0, "" },
                        { 0x1000 + CMD_GETM,  0, "" } } },
    { /* MENU_MATRIX_SIMQ */ MENU_MATRIX1, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_MATA, 0, "" },
                        { 0x1000 + CMD_MATB, 0, "" },
                        { 0x1000 + CMD_MATX, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" } } },
    { /* MENU_MATRIX_EDIT1 */ MENU_NONE, MENU_MATRIX_EDIT2, MENU_MATRIX_EDIT2,
                      { { 0x1000 + CMD_LEFT,    0, "" },
                        { 0x1000 + CMD_OLD,     0, "" },
                        { 0x1000 + CMD_UP,      0, "" },
                        { 0x1000 + CMD_DOWN,    0, "" },
                        { 0x1000 + CMD_GOTOROW, 0, "" },
                        { 0x1000 + CMD_RIGHT,   0, "" } } },
    { /* MENU_MATRIX_EDIT2 */ MENU_NONE, MENU_MATRIX_EDIT1, MENU_MATRIX_EDIT1,
                      { { 0x1000 + CMD_INSR, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_DELR, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x2000 + CMD_WRAP, 0, "" },
                        { 0x2000 + CMD_GROW, 0, "" } } },
    { /* MENU_BASE */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_A_THRU_F, 0, ""      },
                        { 0x2000 + CMD_HEXM,     0, ""      },
                        { 0x2000 + CMD_DECM,     0, ""      },
                        { 0x2000 + CMD_OCTM,     0, ""      },
                        { 0x2000 + CMD_BINM,     0, ""      },
                        { MENU_BASE_LOGIC,       5, "LOGIC" } } },
    { /* MENU_BASE_A_THRU_F */ MENU_BASE, MENU_NONE, MENU_NONE,
                      { { 0, 1, "A" },
                        { 0, 1, "B" },
                        { 0, 1, "C" },
                        { 0, 1, "D" },
                        { 0, 1, "E" },
                        { 0, 1, "F" } } },
    { /* MENU_BASE_LOGIC */ MENU_BASE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_AND,   0, "" },
                        { 0x1000 + CMD_OR,    0, "" },
                        { 0x1000 + CMD_XOR,   0, "" },
                        { 0x1000 + CMD_NOT,   0, "" },
                        { 0x1000 + CMD_BIT_T, 0, "" },
                        { 0x1000 + CMD_ROTXY, 0, "" } } },
    { /* MENU_SOLVE */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_MVAR,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_PGMSLV, 0, "" },
                        { 0x1000 + CMD_SOLVE,  0, "" } } },
    { /* MENU_INTEG */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_MVAR,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_PGMINT, 0, "" },
                        { 0x1000 + CMD_INTEG,  0, "" } } },
    { /* MENU_INTEG_PARAMS */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0,                 4, "LLIM" },
                        { 0,                 4, "ULIM" },
                        { 0,                 3, "ACC"  },
                        { 0x1000 + CMD_NULL, 0, ""     },
                        { 0x1000 + CMD_NULL, 0, ""     },
                        { 0,                 1, "\003" } } }
};


/* By how much do the variables, programs, and labels
 * arrays grow when they are full
 */
#define VARS_INCREMENT 25
#define PRGMS_INCREMENT 10
#define LABELS_INCREMENT 10

/* Registers */
vartype **stack = NULL;
int sp = -1;
int stack_capacity = 0;
vartype *lastx = NULL;
int reg_alpha_length = 0;
char reg_alpha[44];

/* Flags */
flags_struct flags;
const char *virtual_flags =
    /* 00-49 */ "00000000000000000000000000010000000000000000111111"
    /* 50-99 */ "00010000000000010000000001000000000000000000000000";

/* Variables */
int vars_capacity = 0;
int vars_count = 0;
var_struct *vars = NULL;

/* Programs */
int prgms_capacity = 0;
int prgms_count = 0;
int prgms_and_eqns_count = 0;
prgm_struct *prgms = NULL;
int labels_capacity = 0;
int labels_count = 0;
label_struct *labels = NULL;

pgm_index current_prgm;
int4 pc;
int prgm_highlight_row = 0;

vartype *varmenu_eqn;
int varmenu_length;
char varmenu[7];
int varmenu_rows;
int varmenu_row;
int varmenu_labellength[6];
char varmenu_labeltext[6][7];
int varmenu_role;

bool mode_clall;
int (*mode_interruptible)(bool) = NULL;
bool mode_stoppable;
bool mode_command_entry;
bool mode_number_entry;
bool mode_alpha_entry;
bool mode_shift;
int mode_appmenu;
int mode_plainmenu;
bool mode_plainmenu_sticky;
int mode_transientmenu;
int mode_alphamenu;
int mode_commandmenu;
bool mode_running;
bool mode_getkey;
bool mode_getkey1;
bool mode_pause = false;
bool mode_disable_stack_lift; /* transient */
bool mode_varmenu;
bool mode_updown;
int4 mode_sigma_reg;
int mode_goose;
bool mode_time_clktd;
bool mode_time_clk24;
int mode_wsize;
bool mode_menu_caps;

phloat entered_number;
int entered_string_length;
char entered_string[15];

int pending_command;
arg_struct pending_command_arg;
int xeq_invisible;

/* Multi-keystroke commands -- edit state */
/* Relevant when mode_command_entry != 0 */
int incomplete_command;
bool incomplete_ind;
bool incomplete_alpha;
int incomplete_length;
int incomplete_maxdigits;
int incomplete_argtype;
int incomplete_num;
char incomplete_str[22];
int4 incomplete_saved_pc;
int4 incomplete_saved_highlight_row;

/* Command line handling temporaries */
char cmdline[100];
int cmdline_length;
int cmdline_row;

/* Matrix editor / matrix indexing */
int matedit_mode; /* 0=off, 1=index, 2=edit, 3=editn */
char matedit_name[7];
int matedit_length;
vartype *matedit_x;
int4 matedit_i;
int4 matedit_j;
int matedit_prev_appmenu;

/* INPUT */
char input_name[11];
int input_length;
arg_struct input_arg;

/* ERRMSG/ERRNO */
int lasterr = 0;
int lasterr_length;
char lasterr_text[22];

/* BASE application */
int baseapp = 0;

/* Random number generator */
int8 random_number_low, random_number_high;

/* NORM & TRACE mode: number waiting to be printed */
int deferred_print = 0;

/* Keystroke buffer - holds keystrokes received while
 * there is a program running.
 */
int keybuf_head = 0;
int keybuf_tail = 0;
int keybuf[16];

int remove_program_catalog = 0;

int state_file_number_format;

/* No user interaction: we keep track of whether or not the user
 * has pressed any keys since powering up, and we don't allow
 * programmatic OFF until they have. The reason is that we want
 * to prevent nastiness like
 *
 *   LBL "YIKES"  SF 11  OFF  GTO "YIKES"
 *
 * from locking the user out.
 */
bool no_keystrokes_yet;


/* Version number for the state file.
 * State file versions correspond to application releases as follows:
 * 
 * Version  0: 1.0    First release
 * Version  1: 1.0    Cursor left, cursor right, del key handling
 * Version  2: 1.0    Return to user code after interactive EVAL
 */
#define PLUS42_VERSION 2


/*******************/
/* Private globals */
/*******************/

struct rtn_stack_entry {
    int4 prgm;
    int4 pc;
    int4 get_prgm() {
        int4 p = prgm & 0x3fffffff;
        if ((p & 0x20000000) != 0)
            p |= 0xc0000000;
        return p;
    }
    void set_prgm(int4 prgm) {
        this->prgm = prgm & 0x3fffffff;
    }
    bool has_matrix() {
        return (prgm & 0x80000000) != 0;
    }
    void set_has_matrix(bool state) {
        if (state)
            prgm |= 0x80000000;
        else
            prgm &= 0x7fffffff;
    }
    bool has_func() {
        return (prgm & 0x40000000) != 0;
    }
    void set_has_func(bool state) {
        if (state)
            prgm |= 0x40000000;
        else
            prgm &= 0xbfffffff;
    }
};

struct rtn_stack_matrix_name_entry {
    unsigned char length;
    char name[7];
};

struct rtn_stack_matrix_ij_entry {
    int4 i, j;
};

/* Stack pointer vs. level:
 * The stack pointer is the pointer into the actual rtn_stack array, while
 * the stack level is the number of pending returns. The difference between
 * the two is due to the fact that a return may or may not have a saved
 * INDEX matrix name & position associated with it, and that saved matrix
 * state takes up 16 bytes, compared to 8 bytes for a return by itself.
 * I don't want to set aside 24 bytes for every return, and optimize by
 * storing 8 bytes for a plain return (rtn_stack_entry struct) and 24 bytes
 * for a return with INDEX matrix (rtn_stack_entry plus rtn_stack_matrix_name_entry
 * plus rtn_stack_matrix_ij_entry), but that means that the physical stack
 * pointer and the number of pending returns are no longer guaranteed to
 * be in sync, hence the need to track them separately.
 */
#define MAX_RTN_LEVEL 1024
static int rtn_sp = 0;
static int rtn_stack_capacity = 0;
static rtn_stack_entry *rtn_stack = NULL;
static int rtn_level = 0;
static bool rtn_level_0_has_matrix_entry;
static bool rtn_level_0_has_func_state;
static int4 rtn_after_last_rtn_prgm = -1;
static int4 rtn_after_last_rtn_pc = -1;
static int rtn_stop_level = -1;
static bool rtn_solve_active = false;
static bool rtn_integ_active = false;

#ifdef IPHONE
/* For iPhone, we disable OFF by default, to satisfy App Store
 * policy, but we allow users to enable it using a magic value
 * in the X register. This flag determines OFF behavior.
 */
bool off_enable_flag = false;
#endif

struct matrix_persister {
    int type;
    int4 rows;
    int4 columns;
};

static int shared_data_count;
static int shared_data_capacity;
static void **shared_data;


static bool shared_data_grow();
static int shared_data_search(void *data);
static void update_label_table(pgm_index prgm, int4 pc, int inserted);
static void invalidate_lclbls(pgm_index idx, bool force);
static int pc_line_convert(int4 loc, int loc_is_pc);

#ifdef BCD_MATH
#define bin_dec_mode_switch() ( state_file_number_format == NUMBER_FORMAT_BINARY )
#else
#define bin_dec_mode_switch() ( state_file_number_format != NUMBER_FORMAT_BINARY )
#endif


void vartype_string::trim1() {
    if (length > SSLENV + 1) {
        memmove(t.ptr, t.ptr + 1, --length);
    } else if (length == SSLENV + 1) {
        char temp[SSLENV];
        memcpy(temp, t.ptr + 1, --length);
        free(t.ptr);
        memcpy(t.buf, temp, length);
    } else if (length > 0) {
        memmove(t.buf, t.buf + 1, --length);
    }
}

static bool shared_data_grow() {
    if (shared_data_count < shared_data_capacity)
        return true;
    shared_data_capacity += 10;
    void **p = (void **) realloc(shared_data,
                                 shared_data_capacity * sizeof(void *));
    if (p == NULL)
        return false;
    shared_data = p;
    return true;
}

static int shared_data_search(void *data) {
    for (int i = 0; i < shared_data_count; i++)
        if (shared_data[i] == data)
            return i;
    return -1;
}

bool persist_vartype(vartype *v) {
    if (v == NULL)
        return write_char(TYPE_NULL);
    if (!write_char(v->type))
        return false;
    switch (v->type) {
        case TYPE_REAL: {
            vartype_real *r = (vartype_real *) v;
            return write_phloat(r->x);
        }
        case TYPE_COMPLEX: {
            vartype_complex *c = (vartype_complex *) v;
            return write_phloat(c->re) && write_phloat(c->im);
        }
        case TYPE_STRING: {
            vartype_string *s = (vartype_string *) v;
            return write_int4(s->length)
                && fwrite(s->txt(), 1, s->length, gfile) == s->length;
        }
        case TYPE_REALMATRIX: {
            vartype_realmatrix *rm = (vartype_realmatrix *) v;
            int4 rows = rm->rows;
            int4 columns = rm->columns;
            bool must_write = true;
            if (rm->array->refcount > 1) {
                int n = shared_data_search(rm->array);
                if (n == -1) {
                    // A negative row count signals a new shared matrix
                    rows = -rows;
                    if (!shared_data_grow())
                        return false;
                    shared_data[shared_data_count++] = rm->array;
                } else {
                    // A zero row count means this matrix shares its data
                    // with a previously written matrix
                    rows = 0;
                    columns = n;
                    must_write = false;
                }
            }
            write_int4(rows);
            write_int4(columns);
            if (must_write) {
                int size = rm->rows * rm->columns;
                if (fwrite(rm->array->is_string, 1, size, gfile) != size)
                    return false;
                for (int i = 0; i < size; i++) {
                    if (rm->array->is_string[i] == 0) {
                        if (!write_phloat(rm->array->data[i]))
                            return false;
                    } else {
                        char *text;
                        int4 len;
                        get_matrix_string(rm, i, &text, &len);
                        if (!write_int4(len))
                            return false;
                        if (fwrite(text, 1, len, gfile) != len)
                            return false;
                    }
                }
            }
            return true;
        }
        case TYPE_COMPLEXMATRIX: {
            vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
            int4 rows = cm->rows;
            int4 columns = cm->columns;
            bool must_write = true;
            if (cm->array->refcount > 1) {
                int n = shared_data_search(cm->array);
                if (n == -1) {
                    // A negative row count signals a new shared matrix
                    rows = -rows;
                    if (!shared_data_grow())
                        return false;
                    shared_data[shared_data_count++] = cm->array;
                } else {
                    // A zero row count means this matrix shares its data
                    // with a previously written matrix
                    rows = 0;
                    columns = n;
                    must_write = false;
                }
            }
            write_int4(rows);
            write_int4(columns);
            if (must_write) {
                int size = 2 * cm->rows * cm->columns;
                for (int i = 0; i < size; i++)
                    if (!write_phloat(cm->array->data[i]))
                        return false;
            }
            return true;
        }
        case TYPE_LIST: {
            vartype_list *list = (vartype_list *) v;
            int4 size = list->size;
            int data_index = -1;
            bool must_write = true;
            if (list->array->refcount > 1) {
                int n = shared_data_search(list->array);
                if (n == -1) {
                    // data_index == -2 indicates a new shared list
                    data_index = -2;
                    if (!shared_data_grow())
                        return false;
                    shared_data[shared_data_count++] = list->array;
                } else {
                    // data_index >= 0 refers to a previously shared list
                    data_index = n;
                    must_write = false;
                }
            }
            write_int4(size);
            write_int(data_index);
            if (must_write) {
                for (int4 i = 0; i < list->size; i++)
                    if (!persist_vartype(list->array->data[i]))
                        return false;
            }
            return true;
        }
        case TYPE_EQUATION: {
            vartype_equation *eq = (vartype_equation *) v;
            int4 eqn_index = eq->data.eqn();
            if (!write_int4(eqn_index))
                return false;
            return true;
        }
        default:
            /* Should not happen */
            return false;
    }
}


// Using global for 'ver' so we don't have to pass it around all the time

int4 ver;

bool unpersist_vartype(vartype **v) {
    char type;
    if (!read_char(&type))
        return false;
    switch (type) {
        case TYPE_NULL: {
            *v = NULL;
            return true;
        }
        case TYPE_REAL: {
            vartype_real *r = (vartype_real *) new_real(0);
            if (r == NULL)
                return false;
            if (!read_phloat(&r->x)) {
                free_vartype((vartype *) r);
                return false;
            }
            *v = (vartype *) r;
            return true;
        }
        case TYPE_COMPLEX: {
            vartype_complex *c = (vartype_complex *) new_complex(0, 0);
            if (c == NULL)
                return false;
            if (!read_phloat(&c->re) || !read_phloat(&c->im)) {
                free_vartype((vartype *) c);
                return false;
            }
            *v = (vartype *) c;
            return true;
        }
        case TYPE_STRING: {
            int4 len;
            if (!read_int4(&len))
                return false;
            vartype_string *s = (vartype_string *) new_string(NULL, len);
            if (s == NULL)
                return false;
            if (fread(s->txt(), 1, len, gfile) != len) {
                free_vartype((vartype *) s);
                return false;
            }
            *v = (vartype *) s;
            return true;
        }
        case TYPE_REALMATRIX: {
            int4 rows, columns;
            if (!read_int4(&rows) || !read_int4(&columns))
                return false;
            if (rows == 0) {
                // Shared matrix
                vartype *m = dup_vartype((vartype *) shared_data[columns]);
                if (m == NULL)
                    return false;
                else {
                    *v = m;
                    return true;
                }
            }
            bool shared = rows < 0;
            if (shared)
                rows = -rows;
            vartype_realmatrix *rm = (vartype_realmatrix *) new_realmatrix(rows, columns);
            if (rm == NULL)
                return false;
            int4 size = rows * columns;
            if (fread(rm->array->is_string, 1, size, gfile) != size) {
                free_vartype((vartype *) rm);
                return false;
            }
            bool success = true;
            int4 i;
            for (i = 0; i < size; i++) {
                success = false;
                if (rm->array->is_string[i] == 0) {
                    if (!read_phloat(&rm->array->data[i]))
                        break;
                } else {
                    rm->array->is_string[i] = 1;
                    // 4-byte length followed by n bytes of text
                    int4 len;
                    if (!read_int4(&len))
                        break;
                    if (len > SSLENM) {
                        int4 *p = (int4 *) malloc(len + 4);
                        if (p == NULL)
                            break;
                        if (fread(p + 1, 1, len, gfile) != len) {
                            free(p);
                            break;
                        }
                        *p = len;
                        *(int4 **) &rm->array->data[i] = p;
                        rm->array->is_string[i] = 2;
                    } else {
                        char *t = (char *) &rm->array->data[i];
                        *t = len;
                        if (fread(t + 1, 1, len, gfile) != len)
                            break;
                    }
                }
                success = true;
            }
            if (!success) {
                memset(rm->array->is_string + i, 0, size - i);
                free_vartype((vartype *) rm);
                return false;
            }
            if (shared) {
                if (!shared_data_grow()) {
                    free_vartype((vartype *) rm);
                    return false;
                }
                shared_data[shared_data_count++] = rm;
            }
            *v = (vartype *) rm;
            return true;
        }
        case TYPE_COMPLEXMATRIX: {
            int4 rows, columns;
            if (!read_int4(&rows) || !read_int4(&columns))
                return false;
            if (rows == 0) {
                // Shared matrix
                vartype *m = dup_vartype((vartype *) shared_data[columns]);
                if (m == NULL)
                    return false;
                else {
                    *v = m;
                    return true;
                }
            }
            bool shared = rows < 0;
            if (shared)
                rows = -rows;
            vartype_complexmatrix *cm = (vartype_complexmatrix *) new_complexmatrix(rows, columns);
            if (cm == NULL)
                return false;
            int4 size = 2 * rows * columns;
            for (int4 i = 0; i < size; i++) {
                if (!read_phloat(&cm->array->data[i])) {
                    free_vartype((vartype *) cm);
                    return false;
                }
            }
            if (shared) {
                if (!shared_data_grow()) {
                    free_vartype((vartype *) cm);
                    return false;
                }
                shared_data[shared_data_count++] = cm;
            }
            *v = (vartype *) cm;
            return true;
        }
        case TYPE_LIST: {
            int4 size;
            int data_index;
            if (!read_int4(&size) || !read_int(&data_index))
                return false;
            if (data_index >= 0) {
                // Shared list
                vartype *m = dup_vartype((vartype *) shared_data[data_index]);
                if (m == NULL)
                    return false;
                else {
                    *v = m;
                    return true;
                }
            }
            bool shared = data_index == -2;
            vartype_list *list = (vartype_list *) new_list(size);
            if (list == NULL)
                return false;
            if (shared) {
                if (!shared_data_grow()) {
                    free_vartype((vartype *) list);
                    return false;
                }
                shared_data[shared_data_count++] = list;
            }
            for (int4 i = 0; i < size; i++) {
                if (!unpersist_vartype(&list->array->data[i])) {
                    free_vartype((vartype *) list);
                    return false;
                }
            }
            *v = (vartype *) list;
            return true;
        }
        case TYPE_EQUATION: {
            int4 eqn_index;
            if (!read_int4(&eqn_index))
                return false;
            vartype_equation *eq = (vartype_equation *) malloc(sizeof(vartype_equation));
            if (eq == NULL)
                return false;
            eq->type = TYPE_EQUATION;
            eq->data.init_eqn_from_state(eqn_index);
            track_eqn(TRACK_VAR, eqn_index);
            *v = (vartype *) eq;
            return true;
        }
        default:
            return false;
    }
}

static bool persist_globals() {
    int i;
    shared_data_count = 0;
    shared_data_capacity = 0;
    shared_data = NULL;
    bool ret = false;
    pgm_index saved_prgm;

    if (!write_int(reg_alpha_length))
        goto done;
    if (fwrite(reg_alpha, 1, 44, gfile) != 44)
        goto done;
    if (!write_int4(mode_sigma_reg))
        goto done;
    if (!write_int(mode_goose))
        goto done;
    if (!write_bool(mode_time_clktd))
        goto done;
    if (!write_bool(mode_time_clk24))
        goto done;
    if (!write_int(mode_wsize))
        goto done;
    if (!write_bool(mode_menu_caps))
        goto done;
    if (fwrite(&flags, 1, sizeof(flags_struct), gfile) != sizeof(flags_struct))
        goto done;
    int total_prgms;
    total_prgms = prgms_count;
    for (i = prgms_count; i < prgms_and_eqns_count; i++)
        if (prgms[i].eq_data != NULL)
            total_prgms++;
    if (!write_int(total_prgms))
        goto done;
    for (i = 0; i < prgms_count; i++)
        core_export_programs(1, &i, NULL);
    for (i = prgms_count; i < prgms_and_eqns_count; i++) {
        equation_data *eqd = prgms[i].eq_data;
        if (eqd != NULL)
            core_export_programs(1, &i, NULL);
    }
    if (!write_int(prgms_count))
        goto done;
    for (i = prgms_and_eqns_count - 1; i >= prgms_count; i--) {
        equation_data *eqd = prgms[i].eq_data;
        if (eqd != NULL) {
            if (!write_int4(eqd->eqn_index))
                goto done;
            if (!write_int(eqd->refcount))
                goto done;
            if (!write_int4(eqd->length))
                goto done;
            if (fwrite(eqd->text, 1, eqd->length, gfile) != eqd->length)
                goto done;
            if (!write_bool(eqd->compatMode))
                goto done;
            if (eqd->map == NULL) {
                if (!write_int(0))
                    goto done;
            } else {
                int size = eqd->map->getSize();
                if (!write_int(size))
                    goto done;
                if (fwrite(eqd->map->getData(), 1, size, gfile) != size)
                    goto done;
            }
        }
    }
    if (!write_int(sp))
        goto done;
    for (int i = 0; i <= sp; i++)
        if (!persist_vartype(stack[i]))
            goto done;
    if (!persist_vartype(lastx))
        goto done;
    if (!write_int4(current_prgm.unified()))
        goto done;
    if (!write_int4(pc2line(pc)))
        goto done;
    if (!write_int(prgm_highlight_row))
        goto done;
    if (!write_int(vars_count))
        goto done;
    for (i = 0; i < vars_count; i++) {
        if (!write_char(vars[i].length)
            || fwrite(vars[i].name, 1, vars[i].length, gfile) != vars[i].length
            || !write_int2(vars[i].level)
            || !write_int2(vars[i].flags)
            || !persist_vartype(vars[i].value))
            goto done;
    }
    if (!persist_vartype(varmenu_eqn))
        goto done;
    if (!write_int(varmenu_length))
        goto done;
    if (fwrite(varmenu, 1, 7, gfile) != 7)
        goto done;
    if (!write_int(varmenu_rows))
        goto done;
    if (!write_int(varmenu_row))
        goto done;
    for (i = 0; i < 6; i++)
        if (!write_char(varmenu_labellength[i])
                || fwrite(varmenu_labeltext[i], 1, varmenu_labellength[i], gfile) != varmenu_labellength[i])
            goto done;
    if (!write_int(varmenu_role))
        goto done;
    if (!write_int(rtn_sp))
        goto done;
    if (!write_int(rtn_level))
        goto done;
    if (!write_bool(rtn_level_0_has_matrix_entry))
        goto done;
    if (!write_bool(rtn_level_0_has_func_state))
        goto done;
    if (!write_int4(rtn_after_last_rtn_prgm))
        goto done;
    if (!write_int4(rtn_after_last_rtn_pc))
        goto done;
    saved_prgm = current_prgm;
    for (i = rtn_sp - 1; i >= 0; i--) {
        bool matrix_entry_follows = i == 1 && rtn_level_0_has_matrix_entry;
        if (matrix_entry_follows) {
            i++;
        } else {
            matrix_entry_follows = rtn_stack[i].has_matrix();
            current_prgm.set_unified(rtn_stack[i].get_prgm());
            int4 line = rtn_stack[i].pc;
            if (!current_prgm.is_special())
                line = pc2line(line);
            if (!write_int4(rtn_stack[i].prgm)
                    || !write_int4(line))
                goto done;
        }
        if (matrix_entry_follows) {
            rtn_stack_matrix_name_entry *e1 = (rtn_stack_matrix_name_entry *) &rtn_stack[--i];
            rtn_stack_matrix_ij_entry *e2 = (rtn_stack_matrix_ij_entry *) &rtn_stack[--i];
            if (!write_char(e1->length)
                    || fwrite(e1->name, 1, e1->length, gfile) != e1->length
                    || !write_int4(e2->i)
                    || !write_int4(e2->j))
                goto done;
        }
    }
    current_prgm = saved_prgm;
    if (!write_bool(rtn_solve_active))
        goto done;
    if (!write_bool(rtn_integ_active))
        goto done;
    ret = true;

    done:
    free(shared_data);
    return ret;
}

bool loading_state = false;

static bool unpersist_globals() {
    int i;
    shared_data_count = 0;
    shared_data_capacity = 0;
    shared_data = NULL;
    pgm_index saved_prgm;
    bool ret = false;

    if (!read_int(&reg_alpha_length)) {
        reg_alpha_length = 0;
        goto done;
    }
    if (fread(reg_alpha, 1, 44, gfile) != 44) {
        reg_alpha_length = 0;
        goto done;
    }
    if (!read_int4(&mode_sigma_reg)) {
        mode_sigma_reg = 11;
        goto done;
    }
    if (!read_int(&mode_goose)) {
        mode_goose = -1;
        goto done;
    }
    if (!read_bool(&mode_time_clktd)) {
        mode_time_clktd = false;
        goto done;
    }
    if (!read_bool(&mode_time_clk24)) {
        mode_time_clk24 = false;
        goto done;
    }
    if (!read_int(&mode_wsize)) {
        mode_wsize = 36;
        goto done;
    }
    if (!read_bool(&mode_menu_caps)) {
        mode_menu_caps = false;
        goto done;
    }
    if (fread(&flags, 1, sizeof(flags_struct), gfile)
            != sizeof(flags_struct))
        goto done;

    prgms_capacity = 0;
    if (prgms != NULL) {
        free(prgms);
        prgms = NULL;
    }
    int total_prgms;
    if (!read_int(&total_prgms))
        goto done;
    core_import_programs(total_prgms, NULL);
    if (!read_int(&prgms_count))
        goto done;
    rebuild_label_table();
    prgms_and_eqns_count = prgms_count;
    while (--total_prgms >= prgms_count) {
        equation_data *eqd = new equation_data;
        if (eqd == NULL)
            goto done;
        if (!read_int4(&eqd->eqn_index)) {
            eqd_fail:
            delete eqd;
            goto done;
        }
        if (!read_int4(&eqd->refcount))
            goto eqd_fail;
        if (!read_int4(&eqd->length))
            goto eqd_fail;
        if (eqd->length > 0) {
            eqd->text = (char *) malloc(eqd->length);
            if (eqd->text == NULL)
                goto eqd_fail;
            if (fread(eqd->text, 1, eqd->length, gfile) != eqd->length)
                goto eqd_fail;
        }
        if (!read_bool(&eqd->compatMode))
            goto eqd_fail;
        if (new_eqn_idx(eqd->eqn_index) == -1)
            goto eqd_fail;
        int errpos;
        eqd->ev = Parser::parse(std::string(eqd->text, eqd->length), eqd->compatMode, &errpos);
        if (ver >= 2) {
            int mapsize;
            if (!read_int(&mapsize))
                goto eqd_fail;
            if (mapsize > 0) {
                char *d = (char *) malloc(mapsize);
                if (d == NULL)
                    goto eqd_fail;
                if (fread(d, 1, mapsize, gfile) != mapsize) {
                    free(d);
                    goto eqd_fail;
                }
                eqd->map = new CodeMap(d, mapsize);
            }
        }
        prgms[prgms_count + eqd->eqn_index] = prgms[total_prgms];
        prgms[total_prgms].eq_data = NULL;
        prgms[prgms_count + eqd->eqn_index].eq_data = eqd;
    }

    if (!read_int(&sp)) {
        sp = -1;
        goto done;
    }
    stack_capacity = sp + 1;
    if (stack_capacity < 4)
        stack_capacity = 4;
    stack = (vartype **) malloc(stack_capacity * sizeof(vartype *));
    if (stack == NULL) {
        stack_capacity = 0;
        sp = -1;
        goto done;
    }
    for (int i = 0; i <= sp; i++) {
        if (!unpersist_vartype(&stack[i]) || stack[i] == NULL) {
            for (int j = 0; j < i; j++)
                free_vartype(stack[j]);
            free(stack);
            stack = NULL;
            sp = -1;
            stack_capacity = 0;
            goto done;
        }
    }

    free_vartype(lastx);
    if (!unpersist_vartype(&lastx))
        goto done;

    int4 currprgm;
    if (!read_int4(&currprgm)) {
        current_prgm.set_special(-1);
        goto done;
    }
    current_prgm.init_unified_from_state(currprgm);
    track_unified(TRACK_IDX, currprgm);
    if (!read_int4(&pc)) {
        pc = -1;
        goto done;
    }
    if (!read_int(&prgm_highlight_row)) {
        prgm_highlight_row = 0;
        goto done;
    }
    
    vars_capacity = 0;
    if (vars != NULL) {
        free(vars);
        vars = NULL;
    }
    if (!read_int(&vars_count)) {
        vars_count = 0;
        goto done;
    }
    vars = (var_struct *) malloc(vars_count * sizeof(var_struct));
    if (vars == NULL) {
        vars_count = 0;
        goto done;
    }
    for (i = 0; i < vars_count; i++) {
        if (!read_char((char *) &vars[i].length))
            goto vars_fail;
        if (fread(vars[i].name, 1, vars[i].length, gfile) != vars[i].length)
            goto vars_fail;
        if (!read_int2(&vars[i].level))
            goto vars_fail;
        if (!read_int2(&vars[i].flags))
            goto vars_fail;
        if (!unpersist_vartype(&vars[i].value)) {
            vars_fail:
            for (int j = 0; j < i; j++)
                free_vartype(vars[j].value);
            free(vars);
            vars = NULL;
            vars_count = 0;
            goto done;
        }
    }
    vars_capacity = vars_count;

    pc = line2pc(pc);
    incomplete_saved_pc = line2pc(incomplete_saved_pc);

    if (!unpersist_vartype(&varmenu_eqn)) {
        varmenu_eqn = NULL;
        goto done;
    }
    if (!read_int(&varmenu_length))
        goto varmenu_fail;
    if (fread(varmenu, 1, 7, gfile) != 7)
        goto varmenu_fail;
    if (!read_int(&varmenu_rows))
        goto varmenu_fail;
    if (!read_int(&varmenu_row)) {
        varmenu_fail:
        free_vartype(varmenu_eqn);
        varmenu_eqn = NULL;
        varmenu_length = 0;
        goto done;
    }
    char c;
    for (i = 0; i < 6; i++) {
        if (!read_char(&c)
                || fread(varmenu_labeltext[i], 1, c, gfile) != c)
            goto done;
        varmenu_labellength[i] = c;
    }
    if (!read_int(&varmenu_role))
        goto done;
    if (!read_int(&rtn_sp))
        goto done;
    if (!read_int(&rtn_level))
        goto done;
    if (!read_bool(&rtn_level_0_has_matrix_entry))
        goto done;
    if (!read_bool(&rtn_level_0_has_func_state))
        goto done;
    if (ver >= 2) {
        if (!read_int4(&rtn_after_last_rtn_prgm))
            goto done;
        if (!read_int4(&rtn_after_last_rtn_pc))
            goto done;
    } else {
        rtn_after_last_rtn_prgm = -1;
        rtn_after_last_rtn_pc = -1;
    }
    rtn_stack_capacity = 16;
    while (rtn_sp > rtn_stack_capacity)
        rtn_stack_capacity <<= 1;
    rtn_stack = (rtn_stack_entry *) realloc(rtn_stack, rtn_stack_capacity * sizeof(rtn_stack_entry));
    saved_prgm = current_prgm;
    for (i = rtn_sp - 1; i >= 0; i--) {
        bool matrix_entry_follows = i == 1 && rtn_level_0_has_matrix_entry;
        if (matrix_entry_follows) {
            i++;
        } else {
            int4 prgm, line;
            if (!read_int4(&prgm) || !read_int4(&line))
                goto done;
            rtn_stack[i].prgm = prgm;
            matrix_entry_follows = rtn_stack[i].has_matrix();
            current_prgm.init_unified_from_state(rtn_stack[i].get_prgm());
            track_unified(TRACK_STK, rtn_stack[i].get_prgm());
            if (!current_prgm.is_special())
                line = line2pc(line);
            rtn_stack[i].pc = line;
        }
        if (matrix_entry_follows) {
            rtn_stack_matrix_name_entry *e1 = (rtn_stack_matrix_name_entry *) &rtn_stack[--i];
            rtn_stack_matrix_ij_entry *e2 = (rtn_stack_matrix_ij_entry *) &rtn_stack[--i];
            if (!read_char((char *) &e1->length)
                    || fread(e1->name, 1, e1->length, gfile) != e1->length
                    || !read_int4(&e2->i)
                    || !read_int4(&e2->j))
                goto done;
        }
    }
    current_prgm = saved_prgm;
    if (!read_bool(&rtn_solve_active))
        goto done;
    if (!read_bool(&rtn_integ_active))
        goto done;

    ret = true;

    done:
    free(shared_data);
    return ret;
}

static bool make_prgm_space(int n) {
    if (prgms_and_eqns_count + n <= prgms_capacity)
        return true;
    int new_prgms_capacity = prgms_capacity + n + 10;
    prgm_struct *new_prgms = (prgm_struct *) realloc(prgms, new_prgms_capacity * sizeof(prgm_struct));
    if (new_prgms == NULL)
        return false;
    for (int i = prgms_capacity; i < new_prgms_capacity; i++)
        new_prgms[i].eq_data = (equation_data *) (((uintptr_t) -1) / 3 * 2);
    prgms = new_prgms;
    prgms_capacity = new_prgms_capacity;
    return true;
}

int4 new_prgm_idx(int4 idx) {
    if (!make_prgm_space(1))
        return -1;
    if (idx == -1)
        idx = prgms_count;
    memmove(prgms + idx + 1, prgms + idx, (prgms_and_eqns_count - idx) * sizeof(prgm_struct));
    prgms_count++;
    prgms_and_eqns_count++;
    return idx;
}

int4 new_eqn_idx(int4 idx) {
    if (idx == -1) {
        for (int i = prgms_count; i < prgms_and_eqns_count; i++)
            if (prgms[i].eq_data == NULL)
                return i - prgms_count;
        idx = prgms_and_eqns_count;
    } else {
        idx += prgms_count;
    }
    if (idx >= prgms_and_eqns_count) {
        if (!make_prgm_space(idx + 1 - prgms_and_eqns_count))
            return -1;
        for (int i = prgms_and_eqns_count; i < idx; i++)
            prgms[i].eq_data = NULL;
        prgms_and_eqns_count = idx + 1;
    }
    prgms[idx].eq_data = NULL;
    return idx - prgms_count;
}

void clear_rtns_vars_and_prgms() {
    clear_all_rtns();
    current_prgm.clear();
    
    for (int i = 0; i < vars_count; i++)
        free_vartype(vars[i].value);
    free(vars);
    vars = NULL;
    vars_count = 0;
    vars_capacity = 0;
    
    // At this point, no more equations exist, hence,
    // prmgs_count == prgms_and_eqns_count
    // Also, because the stack is clear, there won't be any
    // pending returns into equation code.

    for (int i = 0; i < prgms_count; i++)
        if (prgms[i].text != NULL)
            free(prgms[i].text);
    free(prgms);
    prgms = NULL;
    prgms_count = 0;
    prgms_and_eqns_count = 0;
    prgms_capacity = 0;

    if (labels != NULL)
        free(labels);
    labels = NULL;
    labels_capacity = 0;
    labels_count = 0;
}

int clear_prgm(const arg_struct *arg) {
    pgm_index prgm;
    if (arg->type == ARGTYPE_LBLINDEX)
        prgm.set_prgm(labels[arg->val.num].prgm);
    else if (arg->type == ARGTYPE_STR) {
        if (arg->length == 0) {
            if (current_prgm.is_special())
                return ERR_INTERNAL_ERROR;
            if (current_prgm.is_eqn())
                return ERR_RESTRICTED_OPERATION;
            prgm = current_prgm;
        } else {
            int i;
            for (i = labels_count - 1; i >= 0; i--)
                if (string_equals(arg->val.text, arg->length,
                                 labels[i].name, labels[i].length))
                    goto found;
            return ERR_LABEL_NOT_FOUND;
            found:
            prgm.set_prgm(labels[i].prgm);
        }
    }
    return clear_prgm_by_index(prgm);
}

int clear_prgm_by_index(pgm_index prgm) {
    int i, j;
    if (!prgm.is_prgm())
        return ERR_LABEL_NOT_FOUND;
    clear_all_rtns();
    if (prgm == current_prgm)
        pc = -1;
    else if (current_prgm.is_prgm() && current_prgm.index() > prgm.index())
        current_prgm.set_prgm(current_prgm.prgm() - 1);
    free(prgms[prgm.index()].text);
    for (i = prgm.index(); i < prgms_and_eqns_count - 1; i++)
        prgms[i] = prgms[i + 1];
    prgms_count--;
    prgms_and_eqns_count--;
    i = j = 0;
    while (j < labels_count) {
        if (j > i)
            labels[i] = labels[j];
        j++;
        if (labels[i].prgm > prgm.index()) {
            labels[i].prgm--;
            i++;
        } else if (labels[i].prgm < prgm.index())
            i++;
    }
    labels_count = i;
    if (prgms_count == 0 || prgm.index() == prgms_count) {
        pgm_index saved_prgm = current_prgm;
        int saved_pc = pc;
        goto_dot_dot(false);
        current_prgm = saved_prgm;
        pc = saved_pc;
    }
    update_catalog();
    return ERR_NONE;
}

int clear_prgm_by_int_index(int prgm) {
    pgm_index idx;
    idx.set_prgm(prgm);
    return clear_prgm_by_index(idx);
}

void clear_prgm_lines(int4 count) {
    int4 frompc, deleted, i, j;
    if (pc == -1)
        pc = 0;
    frompc = pc;
    while (count > 0) {
        int command;
        arg_struct arg;
        get_next_command(&pc, &command, &arg, 0, NULL);
        if (command == CMD_END) {
            pc -= 2;
            break;
        }
        count--;
    }
    deleted = pc - frompc;

    int4 idx = current_prgm.index();
    for (i = pc; i < prgms[idx].size; i++)
        prgms[idx].text[i - deleted] = prgms[idx].text[i];
    prgms[idx].size -= deleted;
    pc = frompc;

    i = j = 0;
    while (j < labels_count) {
        if (j > i)
            labels[i] = labels[j];
        j++;
        if (labels[i].prgm == current_prgm.prgm()) {
            if (labels[i].pc < frompc)
                i++;
            else if (labels[i].pc >= frompc + deleted) {
                labels[i].pc -= deleted;
                i++;
            }
        } else
            i++;
    }
    labels_count = i;

    invalidate_lclbls(current_prgm, false);
    clear_all_rtns();
}

void goto_dot_dot(bool force_new) {
    clear_all_rtns();
    int command;
    arg_struct arg;
    if (prgms_count != 0 && !force_new) {
        /* Check if last program is empty */
        pc = 0;
        current_prgm.set_prgm(prgms_count - 1);
        get_next_command(&pc, &command, &arg, 0, NULL);
        if (command == CMD_END) {
            pc = -1;
            return;
        }
    }
    if (prgms_and_eqns_count == prgms_capacity) {
        prgm_struct *newprgms;
        int i;
        prgms_capacity += 10;
        newprgms = (prgm_struct *) malloc(prgms_capacity * sizeof(prgm_struct));
        // TODO - handle memory allocation failure
        for (i = prgms_capacity - 10; i < prgms_capacity; i++)
            newprgms[i].eq_data = (equation_data *) (((uintptr_t) -1) / 3 * 2);
        for (i = 0; i < prgms_count; i++)
            newprgms[i] = prgms[i];
        for (i = prgms_count; i < prgms_and_eqns_count; i++)
            newprgms[i + 1] = prgms[i];
        if (prgms != NULL)
            free(prgms);
        prgms = newprgms;
    } else {
        for (int i = prgms_and_eqns_count; i > prgms_count; i--)
            prgms[i] = prgms[i - 1];
    }
    int4 idx = prgms_count++;
    current_prgm.set_prgm(idx);
    prgms_and_eqns_count++;
    prgms[idx].capacity = 0;
    prgms[idx].size = 0;
    prgms[idx].lclbl_invalid = 1;
    prgms[idx].text = NULL;
    command = CMD_END;
    arg.type = ARGTYPE_NONE;
    store_command(0, command, &arg, NULL);
    pc = -1;
}

int mvar_prgms_exist() {
    int i;
    for (i = 0; i < labels_count; i++)
        if (label_has_mvar(i))
            return 1;
    return 0;
}

int label_has_mvar(int lblindex) {
    pgm_index saved_prgm;
    int4 pc;
    int command;
    arg_struct arg;
    if (labels[lblindex].length == 0)
        return 0;
    saved_prgm = current_prgm;
    current_prgm.set_prgm(labels[lblindex].prgm);
    pc = labels[lblindex].pc;
    pc += get_command_length(current_prgm, pc);
    get_next_command(&pc, &command, &arg, 0, NULL);
    current_prgm = saved_prgm;
    return command == CMD_MVAR;
}

int get_command_length(pgm_index idx, int4 pc) {
    prgm_struct *prgm = prgms + idx.index();
    int4 pc2 = pc;
    int command = prgm->text[pc2++];
    int argtype = prgm->text[pc2++];
    command |= (argtype & 112) << 4;
    bool have_orig_num = command == CMD_NUMBER && (argtype & 128) != 0;
    argtype &= 15;

    if ((command == CMD_GTO || command == CMD_XEQ)
            && (argtype == ARGTYPE_NUM || argtype == ARGTYPE_STK
                                       || argtype == ARGTYPE_LCLBL)
            || command == CMD_GTOL || command == CMD_XEQL)
        pc2 += 4;
    switch (argtype) {
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM: {
            while ((prgm->text[pc2++] & 128) == 0);
            break;
        }
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
        case ARGTYPE_LCLBL:
            pc2++;
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR: {
            pc2 += prgm->text[pc2] + 1;
            break;
        }
        case ARGTYPE_DOUBLE:
            pc2 += sizeof(phloat);
            break;
        case ARGTYPE_XSTR: {
            int xl = prgm->text[pc2++];
            xl += prgm->text[pc2++] << 8;
            pc2 += xl;
            break;
        }
    }
    if (have_orig_num)
        while (prgm->text[pc2++]);
    return pc2 - pc;
}

void get_next_command(int4 *pc, int *command, arg_struct *arg, int find_target, const char **num_str) {
    prgm_struct *prgm = prgms + current_prgm.index();
    int i;
    int4 target_pc;
    int4 orig_pc = *pc;

    *command = prgm->text[(*pc)++];
    arg->type = prgm->text[(*pc)++];
    *command |= (arg->type & 112) << 4;
    bool have_orig_num = *command == CMD_NUMBER && (arg->type & 128) != 0;
    arg->type &= 15;

    if ((*command == CMD_GTO || *command == CMD_XEQ)
            && (arg->type == ARGTYPE_NUM
                || arg->type == ARGTYPE_LCLBL
                || arg->type == ARGTYPE_STK)
            || *command == CMD_GTOL || *command == CMD_XEQL) {
        if (find_target) {
            target_pc = 0;
            for (i = 0; i < 4; i++)
                target_pc = (target_pc << 8) | prgm->text[(*pc)++];
            if (target_pc != -1) {
                arg->target = target_pc;
                find_target = 0;
            }
        } else
            (*pc) += 4;
    } else {
        find_target = 0;
        arg->target = -1;
    }

    switch (arg->type) {
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM: {
            int4 num = 0;
            unsigned char c;
            do {
                c = prgm->text[(*pc)++];
                num = (num << 7) | (c & 127);
            } while ((c & 128) == 0);
            if (arg->type == ARGTYPE_NEG_NUM) {
                arg->type = ARGTYPE_NUM;
                num = -num;
            }
            arg->val.num = num;
            break;
        }
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            arg->val.stk = prgm->text[(*pc)++];
            break;
        case ARGTYPE_LCLBL:
            arg->val.lclbl = prgm->text[(*pc)++];
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR: {
            arg->length = prgm->text[(*pc)++];
            for (i = 0; i < arg->length; i++)
                arg->val.text[i] = prgm->text[(*pc)++];
            break;
        }
        case ARGTYPE_DOUBLE: {
            unsigned char *b = (unsigned char *) &arg->val_d;
            for (int i = 0; i < (int) sizeof(phloat); i++)
                *b++ = prgm->text[(*pc)++];
            break;
        }
        case ARGTYPE_XSTR: {
            int xstr_len = prgm->text[(*pc)++];
            xstr_len += prgm->text[(*pc)++] << 8;
            arg->length = xstr_len;
            arg->val.xstr = (const char *) (prgm->text + *pc);
            (*pc) += xstr_len;
            break;
        }
    }

    if (*command == CMD_NUMBER) {
        if (have_orig_num) {
            char *p = (char *) &prgm->text[*pc];
            if (num_str != NULL)
                *num_str = p;
            /* Make sure the decimal stored in the program matches
             * the current setting of flag 28.
             */
            char wrong_dot = flags.f.decimal_point ? ',' : '.';
            char right_dot = flags.f.decimal_point ? '.' : ',';
            int numlen = 1;
            while (*p != 0) {
                if (*p == wrong_dot)
                    *p = right_dot;
                p++;
                numlen++;
            }
            *pc += numlen;
        } else {
            if (num_str != NULL)
                *num_str = NULL;
        }
        if (arg->type != ARGTYPE_DOUBLE) {
            /* argtype is ARGTYPE_NUM; convert to phloat */
            arg->val_d = arg->val.num;
            arg->type = ARGTYPE_DOUBLE;
        }
    }
    
    if (find_target) {
        if (*command == CMD_GTOL || *command == CMD_XEQL)
            target_pc = line2pc(arg->val.num);
        else
            target_pc = find_local_label(arg);
        arg->target = target_pc;
        for (i = 5; i >= 2; i--) {
            prgm->text[orig_pc + i] = target_pc;
            target_pc >>= 8;
        }
        prgm->lclbl_invalid = 0;
    }
}

void rebuild_label_table() {
    /* TODO -- this is *not* efficient; inserting and deleting ENDs and
     * global LBLs should not cause every single program to get rescanned!
     * But, I don't feel like dealing with that at the moment, so just
     * this ugly brute force approach for now.
     */
    int prgm_index;
    int4 pc;
    labels_count = 0;
    for (prgm_index = 0; prgm_index < prgms_count; prgm_index++) {
        prgm_struct *prgm = prgms + prgm_index;
        pc = 0;
        while (pc < prgm->size) {
            int command = prgm->text[pc];
            int argtype = prgm->text[pc + 1];
            command |= (argtype & 112) << 4;
            argtype &= 15;

            if (command == CMD_END
                        || (command == CMD_LBL && argtype == ARGTYPE_STR)) {
                label_struct *newlabel;
                if (labels_count == labels_capacity) {
                    label_struct *newlabels;
                    int i;
                    labels_capacity += 50;
                    newlabels = (label_struct *)
                                malloc(labels_capacity * sizeof(label_struct));
                    // TODO - handle memory allocation failure
                    for (i = 0; i < labels_count; i++)
                        newlabels[i] = labels[i];
                    if (labels != NULL)
                        free(labels);
                    labels = newlabels;
                }
                newlabel = labels + labels_count++;
                if (command == CMD_END)
                    newlabel->length = 0;
                else {
                    int i;
                    newlabel->length = prgm->text[pc + 2];
                    for (i = 0; i < newlabel->length; i++)
                        newlabel->name[i] = prgm->text[pc + 3 + i];
                }
                newlabel->prgm = prgm_index;
                newlabel->pc = pc;
            }
            pgm_index idx;
            idx.set_prgm(prgm_index);
            pc += get_command_length(idx, pc);
        }
    }
}

static void update_label_table(pgm_index prgm, int4 pc, int inserted) {
    int i;
    for (i = 0; i < labels_count; i++) {
        if (labels[i].prgm > prgm.index())
            return;
        if (labels[i].prgm == prgm.index() && labels[i].pc >= pc)
            labels[i].pc += inserted;
    }
}

static void invalidate_lclbls(pgm_index idx, bool force) {
    prgm_struct *prgm = prgms + idx.index();
    if (force || !prgm->lclbl_invalid) {
        int4 pc2 = 0;
        while (pc2 < prgm->size) {
            int command = prgm->text[pc2];
            int argtype = prgm->text[pc2 + 1];
            command |= (argtype & 112) << 4;
            argtype &= 15;
            if ((command == CMD_GTO || command == CMD_XEQ)
                    && (argtype == ARGTYPE_NUM || argtype == ARGTYPE_STK
                                               || argtype == ARGTYPE_LCLBL)
                    || command == CMD_GTOL || command == CMD_XEQL) {
                /* A dest_pc value of -1 signals 'unknown',
                 * -2 means 'nonexistent', and anything else is
                 * the pc where the destination label is found.
                 */
                int4 pos;
                for (pos = pc2 + 2; pos < pc2 + 6; pos++)
                    prgm->text[pos] = 255;
            }
            pc2 += get_command_length(idx, pc2);
        }
        prgm->lclbl_invalid = 1;
    }
}

void delete_command(int4 pc) {
    prgm_struct *prgm = prgms + current_prgm.index();
    int command = prgm->text[pc];
    int argtype = prgm->text[pc + 1];
    int length = get_command_length(current_prgm, pc);
    int4 pos;

    command |= (argtype & 112) << 4;
    argtype &= 15;

    if (command == CMD_END) {
        int4 newsize;
        prgm_struct *nextprgm;
        if (current_prgm.index() == prgms_count - 1)
            /* Don't allow deletion of last program's END. */
            return;
        nextprgm = prgm + 1;
        prgm->size -= 2;
        newsize = prgm->size + nextprgm->size;
        if (newsize > prgm->capacity) {
            int4 newcapacity = (newsize + 511) & ~511;
            unsigned char *newtext = (unsigned char *) malloc(newcapacity);
            // TODO - handle memory allocation failure
            for (pos = 0; pos < prgm->size; pos++)
                newtext[pos] = prgm->text[pos];
            free(prgm->text);
            prgm->text = newtext;
            prgm->capacity = newcapacity;
        }
        for (pos = 0; pos < nextprgm->size; pos++)
            prgm->text[prgm->size++] = nextprgm->text[pos];
        free(nextprgm->text);
        clear_all_rtns();
        for (pos = current_prgm.index() + 1; pos < prgms_and_eqns_count - 1; pos++)
            prgms[pos] = prgms[pos + 1];
        prgms_count--;
        prgms_and_eqns_count--;
        rebuild_label_table();
        invalidate_lclbls(current_prgm, true);
        draw_varmenu();
        return;
    }

    for (pos = pc; pos < prgm->size - length; pos++)
        prgm->text[pos] = prgm->text[pos + length];
    prgm->size -= length;
    if (command == CMD_LBL && argtype == ARGTYPE_STR)
        rebuild_label_table();
    else
        update_label_table(current_prgm, pc, -length);
    invalidate_lclbls(current_prgm, false);
    clear_all_rtns();
    draw_varmenu();
}

bool store_command(int4 pc, int command, arg_struct *arg, const char *num_str) {
    unsigned char buf[100];
    int bufptr = 0;
    int xstr_len;
    int i;
    int4 pos;
    prgm_struct *prgm = prgms + current_prgm.index();

    if (flags.f.prgm_mode && current_prgm.is_eqn()) {
        display_error(ERR_RESTRICTED_OPERATION, false);
        return false;
    }

    /* We should never be called with pc = -1, but just to be safe... */
    if (pc == -1)
        pc = 0;

    if (arg->type == ARGTYPE_NUM && arg->val.num < 0) {
        arg->type = ARGTYPE_NEG_NUM;
        arg->val.num = -arg->val.num;
    } else if (command == CMD_NUMBER) {
        /* Store the string representation of the number, unless it matches
         * the canonical representation, or unless the number is zero.
         */
        if (num_str != NULL) {
            if (arg->val_d == 0) {
                num_str = NULL;
            } else {
                const char *ap = phloat2program(arg->val_d);
                const char *bp = num_str;
                bool equal = true;
                while (1) {
                    char a = *ap++;
                    char b = *bp++;
                    if (a == 0) {
                        if (b != 0)
                            equal = false;
                        break;
                    } else if (b == 0) {
                        goto notequal;
                    }
                    if (a != b) {
                        if (a == 24) {
                            if (b != 'E' && b != 'e')
                                goto notequal;
                        } else if (a == '.' || a == ',') {
                            if (b != '.' && b != ',')
                                goto notequal;
                        } else {
                            notequal:
                            equal = false;
                            break;
                        }
                    }
                }
                if (equal)
                    num_str = NULL;
            }
        }
        /* arg.type is always ARGTYPE_DOUBLE for CMD_NUMBER, but for storage
         * efficiency, we handle integers specially and store them as
         * ARGTYPE_NUM or ARGTYPE_NEG_NUM instead.
         */
        int4 n = to_int4(arg->val_d);
        if (n == arg->val_d && n != (int4) 0x80000000) {
            if (n >= 0) {
                arg->val.num = n;
                arg->type = ARGTYPE_NUM;
            } else {
                arg->val.num = -n;
                arg->type = ARGTYPE_NEG_NUM;
            }
        }
    } else if (arg->type == ARGTYPE_LBLINDEX) {
        int li = arg->val.num;
        arg->length = labels[li].length;
        for (i = 0; i < arg->length; i++)
            arg->val.text[i] = labels[li].name[i];
        arg->type = ARGTYPE_STR;
    }

    buf[bufptr++] = command & 255;
    buf[bufptr++] = arg->type | ((command & 0x700) >> 4) | (command != CMD_NUMBER || num_str == NULL ? 0 : 128);

    /* If the program is nonempty, it must already contain an END,
     * since that's the very first thing that gets stored in any new
     * program. In this case, we need to split the program.
     */
    if (command == CMD_END && prgm->size > 0) {
        prgm_struct *new_prgm;
        if (prgms_and_eqns_count == prgms_capacity) {
            prgm_struct *new_prgms;
            int4 i;
            prgms_capacity += 10;
            new_prgms = (prgm_struct *)
                            malloc(prgms_capacity * sizeof(prgm_struct));
            // TODO - handle memory allocation failure
            for (i = prgms_capacity - 10; i < prgms_capacity; i++)
                new_prgms[i].eq_data = (equation_data *) (((uintptr_t) -1) / 3 * 2);
            int4 cp = current_prgm.index();
            for (i = 0; i <= cp; i++)
                new_prgms[i] = prgms[i];
            for (i = cp + 1; i < prgms_and_eqns_count; i++)
                new_prgms[i + 1] = prgms[i];
            free(prgms);
            prgms = new_prgms;
            prgm = prgms + cp;
        } else {
            for (i = prgms_and_eqns_count - 1; i > current_prgm.index(); i--)
                prgms[i + 1] = prgms[i];
        }
        prgms_count++;
        prgms_and_eqns_count++;
        new_prgm = prgm + 1;
        new_prgm->size = prgm->size - pc;
        new_prgm->capacity = (new_prgm->size + 511) & ~511;
        new_prgm->text = (unsigned char *) malloc(new_prgm->capacity);
        // TODO - handle memory allocation failure
        for (i = pc; i < prgm->size; i++)
            new_prgm->text[i - pc] = prgm->text[i];
        current_prgm.set_prgm(current_prgm.prgm() + 1);

        /* Truncate the previously 'current' program and append an END.
         * No need to check the size against the capacity and grow the
         * program; since it contained an END before, it still has the
         * capacity for one now;
         */
        prgm->size = pc;
        prgm->text[prgm->size++] = CMD_END;
        prgm->text[prgm->size++] = ARGTYPE_NONE;
        pgm_index before;
        before.set_prgm(current_prgm.prgm() - 1);
        if (flags.f.printer_exists && (flags.f.trace_print || flags.f.normal_print))
            print_program_line(before, pc);

        rebuild_label_table();
        invalidate_lclbls(current_prgm, true);
        invalidate_lclbls(before, true);
        clear_all_rtns();
        draw_varmenu();
        return true;
    }

    if ((command == CMD_GTO || command == CMD_XEQ)
            && (arg->type == ARGTYPE_NUM || arg->type == ARGTYPE_STK 
                                         || arg->type == ARGTYPE_LCLBL)
            || command == CMD_GTOL || command == CMD_XEQL)
        for (i = 0; i < 4; i++)
            buf[bufptr++] = 255;
    switch (arg->type) {
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM: {
            int4 num = arg->val.num;
            char tmpbuf[5];
            int tmplen = 0;
            while (num > 127) {
                tmpbuf[tmplen++] = num & 127;
                num >>= 7;
            }
            tmpbuf[tmplen++] = num;
            tmpbuf[0] |= 128;
            while (--tmplen >= 0)
                buf[bufptr++] = tmpbuf[tmplen];
            break;
        }
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            buf[bufptr++] = arg->val.stk;
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR: {
            buf[bufptr++] = (unsigned char) arg->length;
            for (i = 0; i < arg->length; i++)
                buf[bufptr++] = arg->val.text[i];
            break;
        }
        case ARGTYPE_LCLBL:
            buf[bufptr++] = arg->val.lclbl;
            break;
        case ARGTYPE_DOUBLE: {
            unsigned char *b = (unsigned char *) &arg->val_d;
            for (int i = 0; i < (int) sizeof(phloat); i++)
                buf[bufptr++] = *b++;
            break;
        }
        case ARGTYPE_XSTR: {
            xstr_len = arg->length;
            if (xstr_len > 65535)
                xstr_len = 65535;
            buf[bufptr++] = xstr_len;
            buf[bufptr++] = xstr_len >> 8;
            // Not storing the text in 'buf' because it may not fit;
            // we'll handle that separately when copying the buffer
            // into the program.
            bufptr += xstr_len;
            break;
        }
    }

    if (command == CMD_NUMBER && num_str != NULL) {
        const char *p = num_str;
        char c;
        const char wrong_dot = flags.f.decimal_point ? ',' : '.';
        const char right_dot = flags.f.decimal_point ? '.' : ',';
        while ((c = *p++) != 0) {
            if (c == wrong_dot)
                c = right_dot;
            else if (c == 'E' || c == 'e')
                c = 24;
            buf[bufptr++] = c;
        }
        buf[bufptr++] = 0;
    }

    if (bufptr + prgm->size > prgm->capacity) {
        unsigned char *newtext;
        prgm->capacity += bufptr + 512;
        newtext = (unsigned char *) malloc(prgm->capacity);
        // TODO - handle memory allocation failure
        for (pos = 0; pos < pc; pos++)
            newtext[pos] = prgm->text[pos];
        for (pos = pc; pos < prgm->size; pos++)
            newtext[pos + bufptr] = prgm->text[pos];
        if (prgm->text != NULL)
            free(prgm->text);
        prgm->text = newtext;
    } else {
        for (pos = prgm->size - 1; pos >= pc; pos--)
            prgm->text[pos + bufptr] = prgm->text[pos];
    }
    if (arg->type == ARGTYPE_XSTR) {
        int instr_len = bufptr - xstr_len;
        memcpy(prgm->text + pc, buf, instr_len);
        memcpy(prgm->text + pc + instr_len, arg->val.xstr, xstr_len);
    } else {
        memcpy(prgm->text + pc, buf, bufptr);
    }
    prgm->size += bufptr;
    if (command != CMD_END && flags.f.printer_exists && (flags.f.trace_print || flags.f.normal_print))
        print_program_line(current_prgm, pc);
    
    if (command == CMD_END ||
            (command == CMD_LBL && arg->type == ARGTYPE_STR))
        rebuild_label_table();
    else
        update_label_table(current_prgm, pc, bufptr);

    if (!loading_state) {
        invalidate_lclbls(current_prgm, false);
        clear_all_rtns();
        draw_varmenu();
    }
    return true;
}

void store_command_after(int4 *pc, int command, arg_struct *arg, const char *num_str) {
    int4 oldpc = *pc;
    if (*pc == -1)
        *pc = 0;
    else if (prgms[current_prgm.index()].text[*pc] != CMD_END)
        *pc += get_command_length(current_prgm, *pc);
    if (!store_command(*pc, command, arg, num_str))
        *pc = oldpc;
}

static bool ensure_prgm_space(int n) {
    prgm_struct *prgm = prgms + current_prgm.index();
    if (prgm->size + n <= prgm->capacity)
        return true;
    int4 newcapacity = prgm->size + n;
    unsigned char *newtext = (unsigned char *) realloc(prgm->text, newcapacity);
    if (newtext == NULL)
        return false;
    prgm->text = newtext;
    prgm->capacity = newcapacity;
    return true;
}

int x2line() {
    switch (stack[sp]->type) {
        case TYPE_REAL: {
            if (!ensure_prgm_space(2 + sizeof(phloat)))
                return ERR_INSUFFICIENT_MEMORY;
            vartype_real *r = (vartype_real *) stack[sp];
            arg_struct arg;
            arg.type = ARGTYPE_DOUBLE;
            arg.val_d = r->x;
            store_command_after(&pc, CMD_NUMBER, &arg, NULL);
            return ERR_NONE;
        }
        case TYPE_COMPLEX: {
            if (!ensure_prgm_space(6 + 2 * sizeof(phloat)))
                return ERR_INSUFFICIENT_MEMORY;
            vartype_complex *c = (vartype_complex *) stack[sp];
            arg_struct arg;
            arg.type = ARGTYPE_DOUBLE;
            arg.val_d = c->re;
            store_command_after(&pc, CMD_NUMBER, &arg, NULL);
            arg.type = ARGTYPE_DOUBLE;
            arg.val_d = c->im;
            store_command_after(&pc, CMD_NUMBER, &arg, NULL);
            arg.type = ARGTYPE_NONE;
            store_command_after(&pc, CMD_RCOMPLX, &arg, NULL);
            return ERR_NONE;
        }
        case TYPE_STRING: {
            vartype_string *s = (vartype_string *) stack[sp];
            int len = s->length;
            if (len > 65535)
                len = 65535;
            if (!ensure_prgm_space(4 + len))
                return ERR_INSUFFICIENT_MEMORY;
            arg_struct arg;
            arg.type = ARGTYPE_XSTR;
            arg.length = len;
            arg.val.xstr = s->txt();
            store_command_after(&pc, CMD_XSTR, &arg, NULL);
            return ERR_NONE;
        }
        default:
            return ERR_INTERNAL_ERROR;
    }
}

int a2line(bool append) {
    if (reg_alpha_length == 0) {
        squeak();
        return ERR_NONE;
    }
    if (!ensure_prgm_space(reg_alpha_length + ((reg_alpha_length - 2) / 14 + 1) * 3))
        return ERR_INSUFFICIENT_MEMORY;
    const char *p = reg_alpha;
    int len = reg_alpha_length;
    int maxlen = 15;

    arg_struct arg;
    if (append) {
        maxlen = 14;
    } else if (p[0] == 0x7f || (p[0] & 128) != 0) {
        arg.type = ARGTYPE_NONE;
        store_command_after(&pc, CMD_CLA, &arg, NULL);
        maxlen = 14;
    }

    while (len > 0) {
        int len2 = len;
        if (len2 > maxlen)
            len2 = maxlen;
        arg.type = ARGTYPE_STR;
        if (maxlen == 15) {
            arg.length = len2;
            memcpy(arg.val.text, p, len2);
        } else {
            arg.length = len2 + 1;
            arg.val.text[0] = 127;
            memcpy(arg.val.text + 1, p, len2);
        }
        store_command_after(&pc, CMD_STRING, &arg, NULL);
        p += len2;
        len -= len2;
        maxlen = 14;
    }
    return ERR_NONE;
}

static int pc_line_convert(int4 loc, int loc_is_pc) {
    int4 pc = 0;
    int4 line = 1;
    prgm_struct *prgm = prgms + current_prgm.index();

    while (1) {
        if (loc_is_pc) {
            if (pc >= loc)
                return line;
        } else {
            if (line >= loc)
                return pc;
        }
        if (prgm->text[pc] == CMD_END)
            return loc_is_pc ? line : pc;
        pc += get_command_length(current_prgm, pc);
        line++;
    }
}

int4 pc2line(int4 pc) {
    if (pc == -1)
        return 0;
    else
        return pc_line_convert(pc, 1);
}

int4 line2pc(int4 line) {
    if (line == 0)
        return -1;
    else
        return pc_line_convert(line, 0);
}

int4 find_local_label(const arg_struct *arg) {
    int4 orig_pc = pc;
    int4 search_pc;
    int wrapped = 0;
    prgm_struct *prgm = prgms + current_prgm.index();

    if (orig_pc == -1)
        orig_pc = 0;
    search_pc = orig_pc;

    while (!wrapped || search_pc < orig_pc) {
        int command, argtype;
        if (search_pc >= prgm->size - 2) {
            if (orig_pc == 0)
                break;
            search_pc = 0;
            wrapped = 1;
        }
        command = prgm->text[search_pc];
        argtype = prgm->text[search_pc + 1];
        command |= (argtype & 112) << 4;
        argtype &= 15;
        if (command == CMD_LBL && (argtype == arg->type
                                || argtype == ARGTYPE_STK)) {
            if (argtype == ARGTYPE_NUM) {
                int num = 0;
                unsigned char c;
                int pos = search_pc + 2;
                do {
                    c = prgm->text[pos++];
                    num = (num << 7) | (c & 127);
                } while ((c & 128) == 0);
                if (num == arg->val.num)
                    return search_pc;
            } else if (argtype == ARGTYPE_STK) {
                // Synthetic LBL ST T etc.
                // Allow GTO ST T and GTO 112
                char stk = prgm->text[search_pc + 2];
                if (arg->type == ARGTYPE_STK) {
                    if (stk = arg->val.stk)
                        return search_pc;
                } else if (arg->type == ARGTYPE_NUM) {
                    int num = 0;
                    switch (stk) {
                        case 'T': num = 112; break;
                        case 'Z': num = 113; break;
                        case 'Y': num = 114; break;
                        case 'X': num = 115; break;
                        case 'L': num = 116; break;
                    }
                    if (num == arg->val.num)
                        return search_pc;
                }
            } else {
                char lclbl = prgm->text[search_pc + 2];
                if (lclbl == arg->val.lclbl)
                    return search_pc;
            }
        }
        search_pc += get_command_length(current_prgm, search_pc);
    }

    return -2;
}

static int find_global_label_2(const arg_struct *arg, pgm_index *prgm, int4 *pc, int *idx) {
    int i;
    const char *name = arg->val.text;
    int namelen = arg->length;
    for (i = labels_count - 1; i >= 0; i--) {
        int j;
        char *labelname;
        if (labels[i].length != namelen)
            continue;
        labelname = labels[i].name;
        for (j = 0; j < namelen; j++)
            if (labelname[j] != name[j])
                goto nomatch;
        if (prgm != NULL)
            prgm->set_prgm(labels[i].prgm);
        if (pc != NULL)
            *pc = labels[i].pc;
        if (idx != NULL)
            *idx = i;
        return 1;
        nomatch:;
    }
    return 0;
}

int find_global_label(const arg_struct *arg, pgm_index *prgm, int4 *pc) {
    return find_global_label_2(arg, prgm, pc, NULL);
}

int find_global_label_index(const arg_struct *arg, int *idx) {
    return find_global_label_2(arg, NULL, NULL, idx);
}

int push_rtn_addr(pgm_index prgm, int4 pc) {
    if (rtn_level == MAX_RTN_LEVEL)
        return ERR_RTN_STACK_FULL;
    if (rtn_sp == rtn_stack_capacity) {
        int new_rtn_stack_capacity = rtn_stack_capacity + 16;
        rtn_stack_entry *new_rtn_stack = (rtn_stack_entry *) realloc(rtn_stack, new_rtn_stack_capacity * sizeof(rtn_stack_entry));
        if (new_rtn_stack == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        rtn_stack_capacity = new_rtn_stack_capacity;
        rtn_stack = new_rtn_stack;
    }
    rtn_stack[rtn_sp].set_prgm(prgm.unified());
    rtn_stack[rtn_sp].pc = pc;
    rtn_sp++;
    rtn_level++;
    if (prgm.unified() == -2)
        rtn_solve_active = true;
    else if (prgm.unified() == -3)
        rtn_integ_active = true;
    else
        prgm.inc_refcount();
    return ERR_NONE;
}

int push_indexed_matrix() {
    if (rtn_level == 0) {
        if (rtn_level_0_has_matrix_entry)
            return ERR_NONE;
        if (rtn_sp + 2 > rtn_stack_capacity) {
            int new_rtn_stack_capacity = rtn_stack_capacity + 16;
            rtn_stack_entry *new_rtn_stack = (rtn_stack_entry *) realloc(rtn_stack, new_rtn_stack_capacity * sizeof(rtn_stack_entry));
            if (new_rtn_stack == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            rtn_stack_capacity = new_rtn_stack_capacity;
            rtn_stack = new_rtn_stack;
        }
        rtn_level_0_has_matrix_entry = true;
        rtn_sp += 2;
        rtn_stack_matrix_name_entry e1;
        int dlen;
        string_copy(e1.name, &dlen, matedit_name, matedit_length);
        e1.length = dlen;
        memcpy(&rtn_stack[rtn_sp - 1], &e1, sizeof(e1));
        rtn_stack_matrix_ij_entry e2;
        e2.i = matedit_i;
        e2.j = matedit_j;
        memcpy(&rtn_stack[rtn_sp - 2], &e2, sizeof(e2));
    } else {
        if (rtn_stack[rtn_sp - 1].has_matrix())
            return ERR_NONE;
        if (rtn_sp + 2 > rtn_stack_capacity) {
            int new_rtn_stack_capacity = rtn_stack_capacity + 16;
            rtn_stack_entry *new_rtn_stack = (rtn_stack_entry *) realloc(rtn_stack, new_rtn_stack_capacity * sizeof(rtn_stack_entry));
            if (new_rtn_stack == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            rtn_stack_capacity = new_rtn_stack_capacity;
            rtn_stack = new_rtn_stack;
        }
        rtn_sp += 2;
        rtn_stack[rtn_sp - 1] = rtn_stack[rtn_sp - 3];
        rtn_stack[rtn_sp - 1].set_has_matrix(true);
        rtn_stack_matrix_name_entry e1;
        int dlen;
        string_copy(e1.name, &dlen, matedit_name, matedit_length);
        e1.length = dlen;
        memcpy(&rtn_stack[rtn_sp - 2], &e1, sizeof(e1));
        rtn_stack_matrix_ij_entry e2;
        e2.i = matedit_i;
        e2.j = matedit_j;
        memcpy(&rtn_stack[rtn_sp - 3], &e2, sizeof(e2));
    }
    matedit_mode = 0;
    return ERR_NONE;
}

int push_func_state(int n) {
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    vartype *fd = recall_private_var("FD", 2);
    if (fd != NULL)
        return ERR_INVALID_CONTEXT;
    vartype *st = recall_private_var("ST", 2);
    if (st != NULL)
        return ERR_INVALID_CONTEXT;
    if (!ensure_var_space(1))
        return ERR_INSUFFICIENT_MEMORY;
    int inputs = flags.f.big_stack ? n / 10 : 4;
    if (sp + 1 < inputs)
        return ERR_TOO_FEW_ARGUMENTS;
    int size = inputs + 4;
    // FD list layout:
    // 0: n, the 2-digit parameter to FUNC
    // 1: original stack depth, or -1 for 4-level stack
    // 2: flag 25, ERRNO, and ERRMSG
    // 3: lastx
    // 4: X / level 1
    // 5: Y / level 2
    // etc.
    // For 4-level stack, all 4 registers are saved;
    // for big stack, the input parameters are saved.
    fd = new_list(size);
    if (fd == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    vartype **fd_data = ((vartype_list *) fd)->array->data;
    fd_data[0] = new_real(n);
    fd_data[1] = new_real(flags.f.big_stack ? sp + 1 : -1);
    fd_data[2] = new_string(NULL, lasterr == -1 ? 2 + lasterr_length : 2);
    fd_data[3] = dup_vartype(lastx);
    for (int i = 0; i < inputs; i++)
        fd_data[i + 4] = dup_vartype(stack[sp - i]);
    for (int i = 0; i < size; i++)
        if (fd_data[i] == NULL) {
            free_vartype(fd);
            return ERR_INSUFFICIENT_MEMORY;
        }
    vartype_string *s = (vartype_string *) fd_data[2];
    s->txt()[0] = flags.f.error_ignore ? '1' : '0';
    s->txt()[1] = (char) lasterr;
    if (lasterr == -1)
        memcpy(s->txt() + 2, lasterr_text, lasterr_length);
    store_private_var("FD", 2, fd);
    flags.f.error_ignore = 0;
    lasterr = ERR_NONE;

    if (rtn_level == 0)
        rtn_level_0_has_func_state = true;
    else
        rtn_stack[rtn_sp - 1].set_has_func(true);
    return ERR_NONE;
}

int push_stack_state(bool big) {
    vartype *st = recall_private_var("ST", 2);
    if (st != NULL)
        return ERR_INVALID_CONTEXT;

    if (!ensure_var_space(1))
        return ERR_INSUFFICIENT_MEMORY;
    int save_levels;
    vartype *dups[4];
    int n_dups = 0;
    if (!big && flags.f.big_stack) {
        // Save levels 5 and up, unless FUNC has been called, in which case,
        // save levels <NumInputs> + 1 and up.
        vartype *fd = recall_private_var("FD", 2);
        if (fd != NULL) {
            vartype **fd_data = ((vartype_list *) fd)->array->data;
            int n = to_int(((vartype_real *) fd_data[0])->x);
            int in = n / 10;
            n_dups = 4 - in;
            for (int i = 0; i < n_dups; i++) {
                int p = sp - 3 + i;
                if (p < 0)
                    dups[i] = new_real(0);
                else
                    dups[i] = dup_vartype(stack[p]);
                if (dups[i] == NULL) {
                    n_dups = i;
                    goto nomem;
                }
            }
            save_levels = sp + 1 - in;
        } else {
            save_levels = sp - 3;
        }
        if (save_levels < 0)
            save_levels = 0;
    } else {
        save_levels = 0;
    }
    st = new_list(save_levels + 1);
    if (st == NULL) {
        nomem:
        for (int j = 0; j < n_dups; j++)
            free_vartype(dups[j]);
        return ERR_INSUFFICIENT_MEMORY;
    }

    vartype **st_data = ((vartype_list *) st)->array->data;
    st_data[0] = new_string(flags.f.big_stack ? "1" : "0", 1);
    if (st_data[0] == NULL) {
        free_vartype(st);
        goto nomem;
    }
    if (!big && flags.f.big_stack) {
        memcpy(st_data + 1, stack, save_levels * sizeof(vartype *));
        memmove(stack + n_dups, stack + sp - 3 + n_dups, (sp + 1 - save_levels) * sizeof(vartype *));
        memcpy(stack, dups, n_dups * sizeof(vartype *));
        sp -= save_levels - n_dups;
    }

    store_private_var("ST", 2, st);
    flags.f.big_stack = big;

    if (rtn_level == 0)
        rtn_level_0_has_func_state = true;
    else
        rtn_stack[rtn_sp - 1].set_has_func(true);
    return ERR_NONE;
}

int pop_func_state(bool error) {
    if (rtn_level == 0) {
        if (!rtn_level_0_has_func_state)
            return ERR_NONE;
    } else {
        if (!rtn_stack[rtn_sp - 1].has_func())
            return ERR_NONE;
    }

    vartype_list *st = (vartype_list *) recall_private_var("ST", 2);
    vartype_list *fd = (vartype_list *) recall_private_var("FD", 2);

    if (st == NULL && fd == NULL)
        // Pre-bigstack. Rather a hassle to deal with, so punt.
        // Note that this can only happen if a user upgrades from
        // 2.5.23 or 2.5.24 to >2.5.24 while execution is stopped
        // with old FUNC data on the stack. That seems like a
        // reasonable scenario to ignore.
        return ERR_INVALID_DATA;

    if (st != NULL) {
        vartype **st_data = st->array->data;
        char big = ((vartype_string *) st_data[0])->txt()[0] == '1';
        if (big == flags.f.big_stack)
            st = NULL;
    }

    if (st != NULL && fd == NULL) {
        vartype **st_data = st->array->data;
        char big = ((vartype_string *) st_data[0])->txt()[0] == '1';
        if (big) {
            int4 size = st->size - 1;
            if (size > 0) {
                if (!ensure_stack_capacity(size))
                    return ERR_INSUFFICIENT_MEMORY;
                memmove(stack + size, stack, 4 * sizeof(vartype *));
                memcpy(stack, st_data + 1, size * sizeof(vartype *));
                memset(st_data + 1, 0, size * sizeof(vartype *));
                sp += size;
            }
            flags.f.big_stack = 1;
        } else {
            int err = docmd_4stk(NULL);
            if (err != ERR_NONE)
                return err;
        }
        goto done;
    }

    if (st == NULL && fd != NULL) {
        vartype **fd_data = fd->array->data;
        int n = to_int(((vartype_real *) fd_data[0])->x);
        int old_depth = to_int(((vartype_real *) fd_data[1])->x);
        char big = old_depth >= 0;
        if (big != flags.f.big_stack)
            // Note that this must be the result of NSTK or 4STK;
            // the effects of LNSTK or L4STK would already have been
            // undone by the time we get here (if switching back
            // to the saved mode was a no-op), or we wouldn't even
            // be here in the first place (the case of st != NULL
            // and fd != NULL with a nontrivial L4STK/LNSTK
            // wrap-up, is handled separately at the end of this
            // function).
            return ERR_INVALID_CONTEXT;

        if (error)
            n = 0;
        int in = n / 10;
        int out = n % 10;

        if (!big) {
            int n_tdups = (in == 4 ? 3 : in) - out;
            if (n_tdups < 0)
                n_tdups = 0;
            vartype *tdups[3];
            for (int i = 0; i < n_tdups; i++) {
                tdups[i] = dup_vartype(fd_data[7]); // T
                if (tdups[i] == NULL) {
                    for (int j = 0; j < i; j++)
                        free_vartype(tdups[j]);
                    return ERR_INSUFFICIENT_MEMORY;
                }
            }
            for (int d = out; d < 4; d++) {
                int s = d + in - out;
                vartype *v;
                if (s < 4) {
                    v = fd_data[s + 4];
                    fd_data[s + 4] = NULL;
                } else if (d > out) {
                    v = tdups[--n_tdups];
                } else {
                    v = fd_data[7]; // T
                    fd_data[7] = NULL;
                }
                free_vartype(stack[sp - d]);
                stack[sp - d] = v;
            }
        } else if (error) {
            int other_depth = old_depth - fd->size + 4;
            while (sp >= other_depth)
                free_vartype(stack[sp--]);
            for (int i = fd->size - 1; i >= 4; i--) {
                stack[++sp] = fd_data[i];
                fd_data[i] = NULL;
            }
        } else {
            int depth = sp + 1;
            int excess = depth - (old_depth - in + out);
            if (excess > 0) {
                for (int i = 0; i < excess; i++)
                    free_vartype(stack[sp - out - i]);
                memmove(stack + depth - out - excess, stack + depth - out, out * sizeof(vartype *));
                sp -= excess;
            }
        }

        free_vartype(lastx);
        int li = in == 0 ? 3 : 4; // L : X
        lastx = fd_data[li];
        fd_data[li] = NULL;

        vartype_string *s = (vartype_string *) fd_data[2];
        flags.f.error_ignore = s->txt()[0] == '1';
        if (s->length > 1) {
            lasterr = (signed char) s->txt()[1];
            if (lasterr == -1) {
                lasterr_length = s->length - 2;
                memcpy(lasterr_text, s->txt() + 2, lasterr_length);
            }
        }

        goto done;
    }

    if (st != NULL && fd != NULL) {
        vartype **st_data = st->array->data;
        char st_big = ((vartype_string *) st_data[0])->txt()[0] == '1';
        vartype **fd_data = fd->array->data;
        int old_depth = to_int(((vartype_real *) fd_data[1])->x);
        char fd_big = old_depth >= 0;
        if (st_big != fd_big)
            // Apparently someone called 4STK/NSTK between FUNC and L4STK/LNSTK
            return ERR_INVALID_CONTEXT;

        int n = to_int(((vartype_real *) fd_data[0])->x);

        if (error)
            n = 0;
        int in = n / 10;
        int out = n % 10;

        if (!fd_big) {
            int n_tdups = (in == 4 ? 3 : in) - out;
            if (n_tdups < 0)
                n_tdups = 0;
            vartype *tdups[3];
            for (int i = 0; i < n_tdups; i++) {
                tdups[i] = dup_vartype(fd_data[7]); // T
                if (tdups[i] == NULL) {
                    for (int j = 0; j < i; j++)
                        free_vartype(tdups[j]);
                    return ERR_INSUFFICIENT_MEMORY;
                }
            }
            int err = docmd_4stk(NULL);
            if (err != ERR_NONE) {
                for (int i = 0; i < n_tdups; i++)
                    free_vartype(tdups[i]);
                return err;
            }
            for (int d = out; d < 4; d++) {
                int s = d + in - out;
                vartype *v;
                if (s < 4) {
                    v = fd_data[s + 4];
                    fd_data[s + 4] = NULL;
                } else if (d > out) {
                    v = tdups[--n_tdups];
                } else {
                    v = fd_data[7]; // T
                    fd_data[7] = NULL;
                }
                free_vartype(stack[sp - d]);
                stack[sp - d] = v;
            }
        } else if (error) {
            int fd_levels = fd->size - 4;
            int st_levels = st->size - 1;
            int old_depth = fd_levels + st_levels;
            if (stack_capacity < old_depth) {
                vartype **new_stack = (vartype **) realloc(stack, old_depth * sizeof(vartype *));
                if (new_stack == NULL)
                    return ERR_INSUFFICIENT_MEMORY;
                stack = new_stack;
                stack_capacity = old_depth;
            }
            for (int i = 0; i <= sp; i++)
                free_vartype(stack[i]);
            memcpy(stack, st->array->data + 1, st_levels * sizeof(vartype *));
            memset(st->array->data + 1, 0, st_levels * sizeof(vartype *));
            sp = st_levels - 1;
            for (int i = fd->size - 1; i >= 4; i++)
                stack[++sp] = fd_data[i];
            memset(fd->array->data + 4, 0, fd_levels * sizeof(vartype *));
        } else {
            int st_levels = st->size - 1;
            int needed_capacity = st_levels + out;
            if (stack_capacity < needed_capacity) {
                vartype **new_stack = (vartype **) realloc(stack, needed_capacity * sizeof(vartype *));
                if (new_stack == NULL)
                    return ERR_INSUFFICIENT_MEMORY;
                stack = new_stack;
                stack_capacity = needed_capacity;
            }
            for (int i = 0; i <= sp - out; i++)
                free_vartype(stack[i]);
            memmove(stack + st_levels, stack + sp + 1 - out, out * sizeof(vartype *));
            memcpy(stack, st->array->data + 1, st_levels * sizeof(vartype *));
            sp = st_levels + out - 1;
            memset(st->array->data + 1, 0, st_levels * sizeof(vartype *));
        }

        free_vartype(lastx);
        int li = in == 0 ? 3 : 4; // L : X
        lastx = fd_data[li];
        fd_data[li] = NULL;

        vartype_string *s = (vartype_string *) fd_data[2];
        flags.f.error_ignore = s->txt()[0] == '1';
        if (s->length > 1) {
            lasterr = (signed char) s->txt()[1];
            if (lasterr == -1) {
                lasterr_length = s->length - 2;
                memcpy(lasterr_text, s->txt() + 2, lasterr_length);
            }
        }

        flags.f.big_stack = fd_big;
    }

    done:
    if (rtn_level == 0)
        rtn_level_0_has_func_state = false;
    else
        rtn_stack[rtn_sp - 1].set_has_func(false);

    print_trace();
    return ERR_NONE;
}

int get_frame_depth(int *depth) {
    if (!flags.f.big_stack)
        return ERR_INVALID_CONTEXT;
    vartype *fd = recall_private_var("FD", 2);
    if (fd == NULL)
        return ERR_INVALID_CONTEXT;
    int d = to_int(((vartype_real *) ((vartype_list *) fd)->array->data[1])->x);
    if (d == -1)
        d = 4;
    d = sp + 1 - d;
    if (d < 0)
        return ERR_INVALID_CONTEXT;
    *depth = d;
    return ERR_NONE;
}

void step_out() {
    if (rtn_sp > 0)
        rtn_stop_level = rtn_level - 1;
}

void step_over() {
    if (rtn_sp >= 0)
        rtn_stop_level = rtn_level;
}

void return_here_after_last_rtn() {
    if (current_prgm.is_prgm()) {
        rtn_after_last_rtn_prgm = current_prgm.prgm();
        rtn_after_last_rtn_pc = pc;
    } else {
        rtn_after_last_rtn_prgm = -1;
        rtn_after_last_rtn_pc = -1;
    }
}

void unwind_after_eqn_error() {
    int4 saved_prgm = rtn_after_last_rtn_prgm;
    int4 saved_pc = rtn_after_last_rtn_pc;
    clear_all_rtns();
    if (saved_prgm != -1) {
        current_prgm.set_prgm(saved_prgm);
        pc = saved_pc;
    }
}

bool should_i_stop_at_this_level() {
    bool stop = rtn_stop_level >= rtn_level;
    if (stop)
        rtn_stop_level = -1;
    return stop;
}

static void remove_locals() {
    int last = -1;
    for (int i = vars_count - 1; i >= 0; i--) {
        if (vars[i].level == -1)
            continue;
        if (vars[i].level < rtn_level)
            break;
        if ((matedit_mode == 1 || matedit_mode == 3)
                && string_equals(vars[i].name, vars[i].length, matedit_name, matedit_length)) {
            if (matedit_mode == 3) {
                set_appmenu_exitcallback(0);
                set_menu(MENULEVEL_APP, MENU_NONE);
            }
            matedit_mode = 0;
        }
        if ((vars[i].flags & VAR_HIDING) != 0) {
            for (int j = i - 1; j >= 0; j--)
                if ((vars[j].flags & VAR_HIDDEN) != 0 && string_equals(vars[i].name, vars[i].length, vars[j].name, vars[j].length)) {
                    vars[j].flags &= ~VAR_HIDDEN;
                    break;
                }
        }
        free_vartype(vars[i].value);
        vars[i].length = 100;
        last = i;
    }
    if (last == -1)
        return;
    int from = last;
    int to = last;
    while (from < vars_count) {
        if (vars[from].length != 100)
            vars[to++] = vars[from];
        from++;
    }
    vars_count -= from - to;
    update_catalog();
}

int rtn(int err) {
    // NOTE: 'err' should be one of ERR_NONE, ERR_YES, or ERR_NO.
    // For any actual *error*, i.e. anything that should actually
    // stop program execution, use rtn_with_error() instead.
    pgm_index newprgm;
    int4 newpc;
    bool stop;
    pop_rtn_addr(&newprgm, &newpc, &stop);
    if (newprgm.unified() == -3) {
        return return_to_integ(stop);
    } else if (newprgm.unified() == -2) {
        return return_to_solve(0, stop);
    } else if (newprgm.unified() == -1) {
        if (pc >= prgms[current_prgm.index()].size)
            /* It's an END; go to line 0 */
            pc = -1;
        if (err != ERR_NONE)
            display_error(err, true);
        return ERR_STOP;
    } else {
        current_prgm = newprgm;
        pc = newpc;
        if (err == ERR_NO) {
            int command;
            arg_struct arg;
            get_next_command(&pc, &command, &arg, 0, NULL);
            if (command == CMD_END)
                pc = newpc;
        }
        return stop ? ERR_STOP : ERR_NONE;
    }
}

int rtn_with_error(int err) {
    bool stop;
    if (solve_active() && (err == ERR_OUT_OF_RANGE
                            || err == ERR_DIVIDE_BY_0
                            || err == ERR_INVALID_DATA
                            || err == ERR_STAT_MATH_ERROR)) {
        stop = unwind_stack_until_solve();
        return return_to_solve(1, stop);
    }
    pgm_index newprgm;
    int4 newpc;
    pop_rtn_addr(&newprgm, &newpc, &stop);
    if (!newprgm.is_special()) {
        // Stop on the calling XEQ, not the RTNERR
        current_prgm = newprgm;
        int line = pc2line(newpc);
        set_old_pc(line2pc(line - 1));
    }
    return err;
}

/* When IJ is pushed because INDEX or EDITN are applied to a matrix more local
 * than the indexed matrix, it is possible for the pushed matrix to be deleted
 * before IJ are popped. Ideally, this should be handled by making sure that
 * the matrix named by the pushed name is actually the same one as currently
 * found by that name, but that would require saving additional state to the
 * stack. So, we use this simple check instead, which may not always prevent
 * the wrong matrix from ending up indexed, but will prevent crashes due to
 * accessing nonexistent matrices or to accessing the indexed matrix out of
 * range.
 */
static void validate_matedit() {
    if (matedit_mode != 1 && matedit_mode != 3)
        return;
    int idx = lookup_var(matedit_name, matedit_length);
    if (idx == -1) {
        fail:
        matedit_mode = 0;
        return;
    }
    vartype *v = vars[idx].value;
    int4 rows, cols;
    if (v->type == TYPE_REALMATRIX) {
        vartype_realmatrix *rm = (vartype_realmatrix *) v;
        rows = rm->rows;
        cols = rm->columns;
    } else if (v->type == TYPE_COMPLEXMATRIX) {
        vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
        rows = cm->rows;
        cols = cm->columns;
    } else {
        goto fail;
    }
    if (matedit_i >= rows || matedit_j >= cols)
        goto fail;
}

void pop_rtn_addr(pgm_index *prgm, int4 *pc, bool *stop) {
    remove_locals();
    if (rtn_level == 0) {
        if (rtn_after_last_rtn_prgm != -1) {
            prgm->set_prgm(rtn_after_last_rtn_prgm);
            *pc = rtn_after_last_rtn_pc;
            rtn_after_last_rtn_prgm = -1;
            rtn_after_last_rtn_pc = -1;
            *stop = true;
        } else {
            prgm->clear();
            *pc = -1;
        }
        rtn_stop_level = -1;
        rtn_level_0_has_func_state = false;
        if (rtn_level_0_has_matrix_entry) {
            rtn_level_0_has_matrix_entry = false;
            restore_indexed_matrix:
            rtn_stack_matrix_name_entry e1;
            memcpy(&e1, &rtn_stack[--rtn_sp], sizeof(e1));
            string_copy(matedit_name, &matedit_length, e1.name, e1.length);
            rtn_stack_matrix_ij_entry e2;
            memcpy(&e2, &rtn_stack[--rtn_sp], sizeof(e2));
            matedit_i = e2.i;
            matedit_j = e2.j;
            matedit_mode = 1;
            validate_matedit();
        }
    } else {
        rtn_sp--;
        rtn_level--;
        prgm->set_unified(rtn_stack[rtn_sp].get_prgm());
        prgm->dec_refcount();
        *pc = rtn_stack[rtn_sp].pc;
        if (rtn_stop_level >= rtn_level) {
            *stop = true;
            rtn_stop_level = -1;
        } else
            *stop = false;
        if (prgm->special() == -2)
            rtn_solve_active = false;
        else if (prgm->special() == -3)
            rtn_integ_active = false;
        if (rtn_stack[rtn_sp].has_matrix())
            goto restore_indexed_matrix;
    }
}

void pop_indexed_matrix(const char *name, int namelen) {
    if (rtn_level == 0) {
        if (rtn_level_0_has_matrix_entry) {
            rtn_stack_matrix_name_entry e1;
            memcpy(&e1, &rtn_stack[rtn_sp - 1], sizeof(e1));
            if (string_equals(e1.name, e1.length, name, namelen)) {
                rtn_level_0_has_matrix_entry = false;
                string_copy(matedit_name, &matedit_length, e1.name, e1.length);
                rtn_stack_matrix_ij_entry e2;
                memcpy(&e2, &rtn_stack[rtn_sp - 2], sizeof(e2));
                matedit_i = e2.i;
                matedit_j = e2.j;
                matedit_mode = 1;
                rtn_sp -= 2;
            }
        }
    } else {
        if (rtn_stack[rtn_sp - 1].has_matrix()) {
            rtn_stack_matrix_name_entry e1;
            memcpy(&e1, &rtn_stack[rtn_sp - 2], sizeof(e1));
            if (string_equals(e1.name, e1.length, name, namelen)) {
                string_copy(matedit_name, &matedit_length, e1.name, e1.length);
                rtn_stack_matrix_ij_entry e2;
                memcpy(&e2, &rtn_stack[rtn_sp - 3], sizeof(e2));
                matedit_i = e2.i;
                matedit_j = e2.j;
                matedit_mode = 1;
                rtn_stack[rtn_sp - 3].set_has_matrix(false);
                rtn_stack[rtn_sp - 3].pc = rtn_stack[rtn_sp - 1].pc;
                rtn_sp -= 2;
            }
        }
    }
}

static void get_saved_stack_mode(int *m) {
    if (rtn_level == 0) {
        if (!rtn_level_0_has_func_state)
            return;
    } else {
        if (!rtn_stack[rtn_sp - 1].has_func())
            return;
    }
    vartype_list *st = (vartype_list *) recall_private_var("ST", 2);
    if (st == NULL)
        return;
    vartype **st_data = st->array->data;
    *m = ((vartype_string *) st_data[0])->txt()[0] == '1';
}

void clear_all_rtns() {
    pgm_index prgm;
    int4 dummy1;
    bool dummy2;
    int st_mode = -1;
    while (rtn_level > 0) {
        get_saved_stack_mode(&st_mode);
        pop_rtn_addr(&prgm, &dummy1, &dummy2);
    }
    get_saved_stack_mode(&st_mode);
    pop_rtn_addr(&prgm, &dummy1, &dummy2);
    if (st_mode == 0) {
        arg_struct dummy_arg;
        docmd_4stk(&dummy_arg);
    } else if (st_mode == 1) {
        docmd_nstk(NULL);
    }
    if (mode_plainmenu == MENU_PROGRAMMABLE)
        set_menu(MENULEVEL_PLAIN, MENU_NONE);
    if (varmenu_role == 3)
        varmenu_role = 0;
}

int get_rtn_level() {
    return rtn_level;
}

bool solve_active() {
    return rtn_solve_active;
}

bool integ_active() {
    return rtn_integ_active;
}

bool unwind_stack_until_solve() {
    pgm_index prgm;
    int4 pc;
    bool stop;
    int st_mode = -1;
    while (true) {
        get_saved_stack_mode(&st_mode);
        pop_rtn_addr(&prgm, &pc, &stop);
        if (prgm.unified() == -2)
            break;
    }
    if (st_mode == 0) {
        arg_struct dummy_arg;
        docmd_4stk(&dummy_arg);
    } else if (st_mode == 1) {
        docmd_nstk(NULL);
    }
    return stop;
}

bool read_bool(bool *b) {
    return read_char((char *) b);
}

bool write_bool(bool b) {
    return fputc((char) b, gfile) != EOF;
}

bool read_char(char *c) {
    int i = fgetc(gfile);
    *c = (char) i;
    return i != EOF;
}

bool write_char(char c) {
    return fputc(c, gfile) != EOF;
}

bool read_int(int *n) {
    int4 m;
    if (!read_int4(&m))
        return false;
    *n = (int) m;
    return true;
}

bool write_int(int n) {
    return write_int4(n);
}

bool read_int2(int2 *n) {
    #ifdef F42_BIG_ENDIAN
        char buf[2];
        if (fread(buf, 1, 2, gfile) != 2)
            return false;
        char *dst = (char *) n;
        for (int i = 0; i < 2; i++)
            dst[i] = buf[1 - i];
        return true;
    #else
        return fread(n, 1, 2, gfile) == 2;
    #endif
}

bool write_int2(int2 n) {
    #ifdef F42_BIG_ENDIAN
        char buf[2];
        char *src = (char *) &n;
        for (int i = 0; i < 2; i++)
            buf[i] = src[1 - i];
        return fwrite(buf, 1, 2, gfile) == 2;
    #else
        return fwrite(&n, 1, 2, gfile) == 2;
    #endif
}

bool read_int4(int4 *n) {
    #ifdef F42_BIG_ENDIAN
        char buf[4];
        if (fread(buf, 1, 4, gfile) != 4)
            return false;
        char *dst = (char *) n;
        for (int i = 0; i < 4; i++)
            dst[i] = buf[3 - i];
        return true;
    #else
        return fread(n, 1, 4, gfile) == 4;
    #endif
}

bool write_int4(int4 n) {
    #ifdef F42_BIG_ENDIAN
        char buf[4];
        char *src = (char *) &n;
        for (int i = 0; i < 4; i++)
            buf[i] = src[3 - i];
        return fwrite(buf, 1, 4, gfile) == 4;
    #else
        return fwrite(&n, 1, 4, gfile) == 4;
    #endif
}

bool read_int8(int8 *n) {
    #ifdef F42_BIG_ENDIAN
        char buf[8];
        if (fread(buf, 1, 8, gfile) != 8)
            return false;
        char *dst = (char *) n;
        for (int i = 0; i < 8; i++)
            dst[i] = buf[7 - i];
        return true;
    #else
        return fread(n, 1, 8, gfile) == 8;
    #endif
}

bool write_int8(int8 n) {
    #ifdef F42_BIG_ENDIAN
        char buf[8];
        char *src = (char *) &n;
        for (int i = 0; i < 8; i++)
            buf[i] = src[7 - i];
        return fwrite(buf, 1, 8, gfile) == 8;
    #else
        return fwrite(&n, 1, 8, gfile) == 8;
    #endif
}

bool read_phloat(phloat *d) {
    if (bin_dec_mode_switch()) {
        #ifdef F42_BIG_ENDIAN
            #ifdef BCD_MATH
                char buf[8];
                if (fread(buf, 1, 8, gfile) != 8)
                    return false;
                double dbl;
                char *dst = (char *) &dbl;
                for (int i = 0; i < 8; i++)
                    dst[i] = buf[7 - i];
                *d = dbl;
                return true;
            #else
                char buf[16], data[16];
                if (fread(buf, 1, 16, gfile) != 16)
                    return false;
                for (int i = 0; i < 16; i++)
                    data[i] = buf[15 - i];
                *d = decimal2double(data);
                return true;
            #endif
        #else
            #ifdef BCD_MATH
                double dbl;
                if (fread(&dbl, 1, 8, gfile) != 8)
                    return false;
                *d = dbl;
                return true;
            #else
                char data[16];
                if (fread(data, 1, 16, gfile) != 16)
                    return false;
                *d = decimal2double(data);
                return true;
            #endif
        #endif
    } else {
        #ifdef F42_BIG_ENDIAN
            #ifdef BCD_MATH
                char buf[16];
                if (fread(buf, 1, 16, gfile) != 16)
                    return false;
                    char *dst = (char *) d;
                for (int i = 0; i < 16; i++)
                    dst[i] = buf[15 - i];
                return true;
            #else
                char buf[8];
                if (fread(buf, 1, 8, gfile) != 8)
                    return false;
                char *dst = (char *) d;
                for (int i = 0; i < 8; i++)
                    dst[i] = buf[7 - i];
                return true;
            #endif
        #else
            if (fread(d, 1, sizeof(phloat), gfile) != sizeof(phloat))
                return false;
        #endif
        return true;
    }
}

bool write_phloat(phloat d) {
    #ifdef F42_BIG_ENDIAN
        #ifdef BCD_MATH
            char buf[16];
            char *src = (char *) &d;
            for (int i = 0; i < 16; i++)
                buf[i] = src[15 - i];
            return fwrite(buf, 1, 16, gfile) == 16;
        #else
            char buf[8];
            char *src = (char *) &d;
            for (int i = 0; i < 8; i++)
                buf[i] = src[7 - i];
            return fwrite(buf, 1, 8, gfile) == 8;
        #endif
    #else
        return fwrite(&d, 1, sizeof(phloat), gfile) == sizeof(phloat);
    #endif
}

struct fake_bcd {
    char data[16];
};

struct dec_arg_struct {
    unsigned char type;
    unsigned char length;
    int4 target;
    union {
        int4 num;
        char text[15];
        char stk;
        int cmd;
        char lclbl;
    } val;
    fake_bcd val_d;
};

struct bin_arg_struct {
    unsigned char type;
    unsigned char length;
    int4 target;
    union {
        int4 num;
        char text[15];
        char stk;
        int cmd;
        char lclbl;
    } val;
    double val_d;
};

bool read_arg(arg_struct *arg, bool old) {
    if (!read_char((char *) &arg->type))
        return false;
    char c;
    switch (arg->type) {
        case ARGTYPE_NONE:
            return true;
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM:
        case ARGTYPE_LBLINDEX:
            return read_int4(&arg->val.num);
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            return read_char(&arg->val.stk);
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR:
            // Serializing 'length' as a char for backward compatibility.
            // Values > 255 only happen for XSTR, and those are never
            // serialized.
            if (!read_char(&c))
                return false;
            arg->length = c & 255;
            return fread(arg->val.text, 1, arg->length, gfile) == arg->length;
        case ARGTYPE_LCLBL:
            return read_char(&arg->val.lclbl);
        case ARGTYPE_DOUBLE:
            return read_phloat(&arg->val_d);
        default:
            // Should never happen
            return false;
    }
}

bool write_arg(const arg_struct *arg) {
    int type = arg->type;
    if (type == ARGTYPE_XSTR || type == ARGTYPE_EQN)
        // This types are always used immediately, so no need to persist them;
        // also, persisting it would be difficult, since this variant uses
        // a pointer to the actual text, which is context-dependent and
        // would be impossible to restore.
        type = ARGTYPE_NONE;

    if (!write_char(type))
        return false;
    switch (type) {
        case ARGTYPE_NONE:
            return true;
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM:
        case ARGTYPE_LBLINDEX:
            return write_int4(arg->val.num);
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            return write_char(arg->val.stk);
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR:
            return write_char((char) arg->length)
                && fwrite(arg->val.text, 1, arg->length, gfile) == arg->length;
        case ARGTYPE_LCLBL:
            return write_char(arg->val.lclbl);
        case ARGTYPE_DOUBLE:
            return write_phloat(arg->val_d);
        default:
            // Should never happen
            return false;
    }
}

static bool load_state2(bool *clear, bool *too_new) {
    int4 magic;
    int4 version;
    *clear = false;
    *too_new = false;

    /* The shell has verified the initial magic and version numbers,
     * and loaded the shell state, before we got called.
     */

    if (!read_int4(&magic))
        return false;
    if (magic != PLUS42_MAGIC)
        return false;
    if (!read_int4(&ver)) {
        // A state file containing nothing after the magic number
        // is considered empty, and results in a hard reset. This
        // is *not* an error condition; such state files are used
        // when creating a new state in the States window.
        *clear = true;
        return false;
    }

    if (ver > PLUS42_VERSION) {
        *too_new = true;
        return false;
    }

    // Embedded version information. No need to read this; it's just
    // there for troubleshooting purposes. All we need to do here is
    // skip it.
    while (true) {
        char c;
        if (!read_char(&c))
            return false;
        if (c == 0)
            break;
    }
    
    bool state_is_decimal;
    if (!read_bool(&state_is_decimal)) return false;
    if (!state_is_decimal)
        state_file_number_format = NUMBER_FORMAT_BINARY;
    else
        state_file_number_format = NUMBER_FORMAT_BID128;

    bool bdummy;
    if (!read_bool(&bdummy)) return false;
    if (!read_bool(&bdummy)) return false;
    if (!read_bool(&bdummy)) return false;

    if (!read_bool(&mode_clall)) return false;
    if (!read_bool(&mode_command_entry)) return false;
    if (!read_bool(&mode_number_entry)) return false;
    if (!read_bool(&mode_alpha_entry)) return false;
    if (!read_bool(&mode_shift)) return false;
    if (!read_int(&mode_appmenu)) return false;
    if (!read_int(&mode_plainmenu)) return false;
    if (!read_bool(&mode_plainmenu_sticky)) return false;
    if (!read_int(&mode_transientmenu)) return false;
    if (!read_int(&mode_alphamenu)) return false;
    if (!read_int(&mode_commandmenu)) return false;
    if (!read_bool(&mode_running)) return false;
    if (!read_bool(&mode_varmenu)) return false;
    if (!read_bool(&mode_updown)) return false;

    if (!read_bool(&mode_getkey))
        return false;

    if (!read_phloat(&entered_number)) return false;
    if (!read_int(&entered_string_length)) return false;
    if (fread(entered_string, 1, 15, gfile) != 15) return false;

    if (!read_int(&pending_command)) return false;
    if (!read_arg(&pending_command_arg, false)) return false;
    if (!read_int(&xeq_invisible)) return false;

    if (!read_int(&incomplete_command)) return false;
    if (!read_bool(&incomplete_ind)) return false;
    if (!read_bool(&incomplete_alpha)) return false;
    if (!read_int(&incomplete_length)) return false;
    if (!read_int(&incomplete_maxdigits)) return false;
    if (!read_int(&incomplete_argtype)) return false;
    if (!read_int(&incomplete_num)) return false;
    if (fread(incomplete_str, 1, 22, gfile) != 22) return false;
    if (!read_int4(&incomplete_saved_pc)) return false;
    if (!read_int4(&incomplete_saved_highlight_row)) return false;

    if (fread(cmdline, 1, 100, gfile) != 100) return false;
    if (!read_int(&cmdline_length)) return false;
    if (!read_int(&cmdline_row)) return false;

    if (!read_int(&matedit_mode)) return false;
    if (fread(matedit_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&matedit_length)) return false;
    if (!unpersist_vartype(&matedit_x)) return false;
    if (!read_int4(&matedit_i)) return false;
    if (!read_int4(&matedit_j)) return false;
    if (!read_int(&matedit_prev_appmenu)) return false;

    if (fread(input_name, 1, 11, gfile) != 11) return false;
    if (!read_int(&input_length)) return false;
    if (!read_arg(&input_arg, false)) return false;

    if (!read_int(&lasterr)) return false;
    if (!read_int(&lasterr_length)) return false;
    if (fread(lasterr_text, 1, 22, gfile) != 22) return false;

    if (!read_int(&baseapp)) return false;

    if (!read_int8(&random_number_low)) return false;
    if (!read_int8(&random_number_high)) return false;

    if (!read_int(&deferred_print)) return false;

    if (!read_int(&keybuf_head)) return false;
    if (!read_int(&keybuf_tail)) return false;
    for (int i = 0; i < 16; i++)
        if (!read_int(&keybuf[i]))
            return false;

    if (!unpersist_display(ver))
        return false;
    if (!unpersist_globals())
        return false;
    if (!unpersist_eqn(ver))
        return false;

#ifdef BCD_MATH
    if (!unpersist_math(ver, state_file_number_format != NUMBER_FORMAT_BID128))
        return false;
#else
    if (!unpersist_math(ver, state_file_number_format != NUMBER_FORMAT_BINARY))
        return false;
#endif

    if (!read_int4(&magic)) return false;
    if (magic != PLUS42_MAGIC)
        return false;
    if (!read_int4(&version)) return false;
    if (version != ver)
        return false;

    return true;
}

std::vector<int> *var_refs;
std::vector<int> *stk_refs;
std::vector<int> *idx_refs;

void track_eqn(int which, int eqn_index) {
    std::vector<int> *v;
    switch (which) {
        case TRACK_VAR: v = var_refs; break;
        case TRACK_STK: v = stk_refs; break;
        case TRACK_IDX: v = idx_refs; break;
    }
    if (eqn_index >= v->size()) {
        var_refs->resize(eqn_index + 1);
        stk_refs->resize(eqn_index + 1);
        idx_refs->resize(eqn_index + 1);
    }
    (*v)[eqn_index]++;
}

void track_unified(int which, int uni) {
    if (uni > 0 && (uni & 1) != 0)
        track_eqn(which, uni >> 1);
}

bool load_state(int4 ver_p, bool *clear, bool *too_new) {
    loading_state = true;
    var_refs = new std::vector<int>;
    stk_refs = new std::vector<int>;
    idx_refs = new std::vector<int>;
    bool ret = load_state2(clear, too_new);
    loading_state = false;
    char fn[1024];
    sprintf(fn, "%s/plus42-memory-stats.txt", getenv("HOME"));
    FILE *f = fopen(fn, "r+");
    if (f != NULL) {
        fseek(f, 0, SEEK_END);
        fprintf(f, "=========================================\n");
        uint4 t, d;
        shell_get_time_date(&t, &d, NULL);
        fprintf(f, "equation usage report %04d-%02d-%02d %02d:%02d:%02d\n",
                d / 10000, d / 100 % 100, d % 100,
                t / 1000000, t / 10000 % 100, t / 100 % 100);
        int max = prgms_and_eqns_count - prgms_count;
        if (max < var_refs->size())
            max = var_refs->size();
        for (int i = 0; i < max; i++) {
            int idx = i + prgms_count;
            fprintf(f, "%3d: ", i);
            equation_data *eqd = NULL;
            int r;
            if (i >= var_refs->size()) {
                fprintf(f, "var: 0 stk: 0 idx: 0 ");
                r = 0;
            } else {
                fprintf(f, "var:%2d stk:%2d idx:%2d ", (*var_refs)[i], (*stk_refs)[i], (*idx_refs)[i]);
                r = (*var_refs)[i] + (*stk_refs)[i] + (*idx_refs)[i];
            }
            if (idx >= prgms_and_eqns_count)
                fprintf(f, "index out of range\n");
            else if (prgms[idx].eq_data == NULL)
                fprintf(f, "unused index\n");
            else {
                
                eqd = prgms[idx].eq_data;
                if (eqd->length == 0) {
                    fprintf(f, "[%d] (direct solver)\n", eqd->refcount);
                } else {
                    char *hpbuf = (char *) malloc(eqd->length * 5 + 1);
                    int4 hplen = hp2ascii(hpbuf, eqd->text, eqd->length, false);
                    hpbuf[hplen] = 0;
                    fprintf(f, "[%d] \"%s\"\n", eqd->refcount, hpbuf);
                    free(hpbuf);
                }
                if (eqd->eqn_index != i)
                    fprintf(f, "    mismatched equation index: %d\n", eqd->eqn_index);
            }
            if (eqd == NULL) {
                if (r != 0)
                    fprintf(f, "    nonzero refcount for nonexistent equation\n");
            } else {
                if (r != eqd->refcount)
                    if (r == 0) {
                        fprintf(f, "    mismatched refcount; deleting equation\n");
                        delete eqd;
                        prgms[idx].eq_data = NULL;
                    } else {
                        fprintf(f, "    mismatched refcount; correcting\n");
                        eqd->refcount = r;
                    }
            }
        }
        fclose(f);
    } else {
        // No output file present; perform cleanup silently
        for (int idx = prgms_count; idx < prgms_and_eqns_count; idx++) {
            equation_data *eqd = prgms[idx].eq_data;
            int i = idx - prgms_count;
            if (eqd != NULL) {
                int r = 0;
                if (i < var_refs->size())
                    r = (*var_refs)[i] + (*stk_refs)[i] + (*idx_refs)[i];
                if (r == 0) {
                    // delete orphaned equation
                    delete eqd;
                    prgms[idx].eq_data = NULL;
                } else {
                    // fix refcount
                    eqd->refcount = r;
                }
            }
        }
    }
    delete var_refs;
    delete stk_refs;
    delete idx_refs;
    var_refs = stk_refs = idx_refs = NULL;
    return ret;
}

void save_state() {
    if (!write_int4(PLUS42_MAGIC) || !write_int4(PLUS42_VERSION))
        return;

    // Write app version and platform, for troubleshooting purposes
    const char *platform = shell_platform();
    char c;
    do {
        c = *platform++;
        write_char(c);
    } while (c != 0);

    #ifdef BCD_MATH
        if (!write_bool(true)) return;
    #else
        if (!write_bool(false)) return;
    #endif
    if (!write_bool(core_settings.matrix_singularmatrix)) return;
    if (!write_bool(core_settings.matrix_outofrange)) return;
    if (!write_bool(core_settings.auto_repeat)) return;
    if (!write_bool(mode_clall)) return;
    if (!write_bool(mode_command_entry)) return;
    if (!write_bool(mode_number_entry)) return;
    if (!write_bool(mode_alpha_entry)) return;
    if (!write_bool(mode_shift)) return;
    if (!write_int(mode_appmenu)) return;
    if (!write_int(mode_plainmenu)) return;
    if (!write_bool(mode_plainmenu_sticky)) return;
    if (!write_int(mode_transientmenu)) return;
    if (!write_int(mode_alphamenu)) return;
    if (!write_int(mode_commandmenu)) return;
    if (!write_bool(mode_running)) return;
    if (!write_bool(mode_varmenu)) return;
    if (!write_bool(mode_updown)) return;
    if (!write_bool(mode_getkey)) return;

    if (!write_phloat(entered_number)) return;
    if (!write_int(entered_string_length)) return;
    if (fwrite(entered_string, 1, 15, gfile) != 15) return;

    if (!write_int(pending_command)) return;
    if (!write_arg(&pending_command_arg)) return;
    if (!write_int(xeq_invisible)) return;

    if (!write_int(incomplete_command)) return;
    if (!write_bool(incomplete_ind)) return;
    if (!write_bool(incomplete_alpha)) return;
    if (!write_int(incomplete_length)) return;
    if (!write_int(incomplete_maxdigits)) return;
    if (!write_int(incomplete_argtype)) return;
    if (!write_int(incomplete_num)) return;
    if (fwrite(incomplete_str, 1, 22, gfile) != 22) return;
    if (!write_int4(pc2line(incomplete_saved_pc))) return;
    if (!write_int4(incomplete_saved_highlight_row)) return;

    if (fwrite(cmdline, 1, 100, gfile) != 100) return;
    if (!write_int(cmdline_length)) return;
    if (!write_int(cmdline_row)) return;

    if (!write_int(matedit_mode)) return;
    if (fwrite(matedit_name, 1, 7, gfile) != 7) return;
    if (!write_int(matedit_length)) return;
    if (!persist_vartype(matedit_x)) return;
    if (!write_int4(matedit_i)) return;
    if (!write_int4(matedit_j)) return;
    if (!write_int(matedit_prev_appmenu)) return;

    if (fwrite(input_name, 1, 11, gfile) != 11) return;
    if (!write_int(input_length)) return;
    if (!write_arg(&input_arg)) return;

    if (!write_int(lasterr)) return;
    if (!write_int(lasterr_length)) return;
    if (fwrite(lasterr_text, 1, 22, gfile) != 22) return;

    if (!write_int(baseapp)) return;

    if (!write_int8(random_number_low)) return;
    if (!write_int8(random_number_high)) return;

    if (!write_int(deferred_print)) return;

    if (!write_int(keybuf_head)) return;
    if (!write_int(keybuf_tail)) return;
    for (int i = 0; i < 16; i++)
        if (!write_int(keybuf[i]))
            return;

    if (!persist_display())
        return;
    if (!persist_globals())
        return;
    if (!persist_eqn())
        return;
    if (!persist_math())
        return;

    if (!write_int4(PLUS42_MAGIC)) return;
    if (!write_int4(PLUS42_VERSION)) return;
}

// Reason:
// 0 = Memory Clear
// 1 = State File Corrupt
// 2 = State File Too New
void hard_reset(int reason) {
    vartype *regs;

    /* Clear stack */
    for (int i = 0; i <= sp; i++)
        free_vartype(stack[i]);
    free(stack);
    free_vartype(lastx);
    sp = 3;
    stack_capacity = 4;
    stack = (vartype **) malloc(stack_capacity * sizeof(vartype *));
    for (int i = 0; i <= sp; i++)
        stack[i] = new_real(0);
    lastx = new_real(0);

    /* Clear alpha */
    reg_alpha_length = 0;

    /* Clear RTN stack, variables, and programs */
    clear_rtns_vars_and_prgms();
    
    /* Reinitialize variables */
    regs = new_realmatrix(25, 1);
    store_var("REGS", 4, regs);

    /* Reinitialize RTN stack */
    if (rtn_stack != NULL)
        free(rtn_stack);
    rtn_stack_capacity = 16;
    rtn_stack = (rtn_stack_entry *) malloc(rtn_stack_capacity * sizeof(rtn_stack_entry));
    rtn_sp = 0;
    rtn_level = 0;
    rtn_stop_level = -1;
    rtn_solve_active = false;
    rtn_integ_active = false;

    /* Reinitialize programs */
    bool prev_loading_state = loading_state;
    loading_state = true;
    goto_dot_dot(false);
    loading_state = prev_loading_state;


    pending_command = CMD_NONE;

    matedit_mode = 0;
    input_length = 0;
    baseapp = 0;
    random_number_low = 0;
    random_number_high = 0;

    flags.f.f00 = flags.f.f01 = flags.f.f02 = flags.f.f03 = flags.f.f04 = 0;
    flags.f.f05 = flags.f.f06 = flags.f.f07 = flags.f.f08 = flags.f.f09 = 0;
    flags.f.f10 = 0;
    flags.f.auto_exec = 0;
    flags.f.double_wide_print = 0;
    flags.f.lowercase_print = 0;
    flags.f.f14 = 0;
    flags.f.trace_print = 0;
    flags.f.normal_print = 0;
    flags.f.f17 = flags.f.f18 = flags.f.f19 = flags.f.f20 = 0;
    flags.f.printer_enable = 0;
    flags.f.numeric_data_input = 0;
    flags.f.alpha_data_input = 0;
    flags.f.range_error_ignore = 0;
    flags.f.error_ignore = 0;
    flags.f.audio_enable = 1;
    /* flags.f.VIRTUAL_custom_menu = 0; */
    flags.f.decimal_point = shell_decimal_point() ? 1 : 0; // HP-42S sets this to 1 on hard reset
    flags.f.thousands_separators = 1;
    flags.f.stack_lift_disable = 0;
    int df = shell_date_format();
    flags.f.dmy = df == 1;
    flags.f.f32 = flags.f.f33 = 0;
    flags.f.agraph_control1 = 0;
    flags.f.agraph_control0 = 0;
    flags.f.digits_bit3 = 0;
    flags.f.digits_bit2 = 1;
    flags.f.digits_bit1 = 0;
    flags.f.digits_bit0 = 0;
    flags.f.fix_or_all = 1;
    flags.f.eng_or_all = 0;
    flags.f.grad = 0;
    flags.f.rad = 0;
    /* flags.f.VIRTUAL_continuous_on = 0; */
    /* flags.f.VIRTUAL_solving = 0; */
    /* flags.f.VIRTUAL_integrating = 0; */
    /* flags.f.VIRTUAL_variable_menu = 0; */
    /* flags.f.VIRTUAL_alpha_mode = 0; */
    /* flags.f.VIRTUAL_low_battery = 0; */
    flags.f.message = 1;
    flags.f.two_line_message = 0;
    flags.f.prgm_mode = 0;
    /* flags.f.VIRTUAL_input = 0; */
    flags.f.eqn_compat = 0;
    flags.f.printer_exists = 0;
    flags.f.lin_fit = 1;
    flags.f.log_fit = 0;
    flags.f.exp_fit = 0;
    flags.f.pwr_fit = 0;
    flags.f.all_sigma = 1;
    flags.f.log_fit_invalid = 0;
    flags.f.exp_fit_invalid = 0;
    flags.f.pwr_fit_invalid = 0;
    flags.f.shift_state = 0;
    /* flags.f.VIRTUAL_matrix_editor = 0; */
    flags.f.grow = 0;
    flags.f.ymd = df == 2;
    flags.f.base_bit0 = 0;
    flags.f.base_bit1 = 0;
    flags.f.base_bit2 = 0;
    flags.f.base_bit3 = 0;
    flags.f.local_label = 0;
    flags.f.polar = 0;
    flags.f.real_result_only = 0;
    /* flags.f.VIRTUAL_programmable_menu = 0; */
    flags.f.matrix_edge_wrap = 0;
    flags.f.matrix_end_wrap = 0;
    flags.f.base_signed = 1;
    flags.f.base_wrap = 0;
    flags.f.big_stack = 0;
    flags.f.f81 = flags.f.f82 = flags.f.f83 = flags.f.f84 = 0;
    flags.f.f85 = flags.f.f86 = flags.f.f87 = flags.f.f88 = flags.f.f89 = 0;
    flags.f.f90 = flags.f.f91 = flags.f.f92 = flags.f.f93 = flags.f.f94 = 0;
    flags.f.f95 = flags.f.f96 = flags.f.f97 = flags.f.f98 = flags.f.f99 = 0;

    mode_clall = false;
    mode_command_entry = false;
    mode_number_entry = false;
    mode_alpha_entry = false;
    mode_shift = false;
    mode_commandmenu = MENU_NONE;
    mode_alphamenu = MENU_NONE;
    mode_transientmenu = MENU_NONE;
    mode_plainmenu = MENU_NONE;
    mode_appmenu = MENU_NONE;
    mode_running = false;
    mode_getkey = false;
    mode_pause = false;
    mode_varmenu = false;
    prgm_highlight_row = 0;
    varmenu_eqn = NULL;
    varmenu_length = 0;
    mode_updown = false;
    mode_sigma_reg = 11;
    mode_goose = -1;
    mode_time_clktd = false;
    mode_time_clk24 = shell_clk24();
    mode_wsize = 36;
    mode_menu_caps = false;

    reset_math();
    eqn_end();

    clear_display();
    clear_custom_menu();
    clear_prgm_menu();
    switch (reason) {
        case 0:
            draw_string(0, 0, "Memory Clear", 12);
            break;
        case 1:
            draw_string(0, 0, "State File Corrupt", 18);
            break;
        case 2:
            draw_string(0, 0, "State File Too New", 18);
            break;
    }
    display_x(1);
    flush_display();
}

#ifdef IPHONE
bool off_enabled() {
    if (off_enable_flag)
        return true;
    if (sp == -1 || stack[sp]->type != TYPE_STRING)
        return false;
    vartype_string *str = (vartype_string *) stack[sp];
    off_enable_flag = string_equals(str->txt(), str->length, "YESOFF", 6);
    return off_enable_flag;
}
#endif
