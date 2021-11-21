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

#ifndef CORE_PARSER_H
#define CORE_PARSER_H 1


#include <map>
#include <string>
#include <vector>

#include "core_globals.h"
#include "core_variables.h"

////////////////////////////////
/////  class declarations  /////
////////////////////////////////

class GeneratorContext;

class Evaluator {

    protected:

    int tpos;

    public:

    Evaluator(int pos) : tpos(pos) {}
    virtual ~Evaluator() {}
    virtual bool isBool() { return false; }
    virtual bool isEquation() { return false; }
    virtual bool makeLvalue() { return false; }
    virtual std::string name() { return ""; }
    virtual std::string eqnName() { return ""; }
    virtual std::vector<std::string> *eqnParamNames() { return NULL; }
    virtual Evaluator *removeName() { return this; }
    virtual void getSides(const std::string *name, Evaluator **lhs, Evaluator **rhs);
    virtual bool is(const std::string *name) { return false; }
    virtual Evaluator *clone() = 0;
    virtual void detach() {}

    int pos() { return tpos; }

    virtual bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) { return false; }
    virtual void generateCode(GeneratorContext *ctx) = 0;
    virtual void generateAssignmentCode(GeneratorContext *ctx) {} /* For lvalues */
    virtual void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) = 0;
    virtual int howMany(const std::string *name) = 0;
};

class Lexer;
struct prgm_struct;

class Parser {

    private:

    std::string text;
    Lexer *lex;
    std::string pb;
    int pbpos;
    int context;

    public:

    static Evaluator *parse(std::string expr, bool compatMode, int *errpos);
    static void generateCode(Evaluator *ev, prgm_struct *prgm);

    private:

    Parser(Lexer *lex);
    ~Parser();
    Evaluator *parseExpr(int context);
    Evaluator *parseExpr2();
    Evaluator *parseAnd();
    Evaluator *parseNot();
    Evaluator *parseComparison();
    Evaluator *parseNumExpr();
    Evaluator *parseTerm();
    Evaluator *parseFactor();
    Evaluator *parseThing();
    std::vector<Evaluator *> *parseExprList(int nargs, int mode);
    bool isIdentifier(const std::string &s);
    bool nextToken(std::string *tok, int *tpos);
    void pushback(std::string o, int p);
    static bool isOperator(const std::string &s);
};

void get_varmenu_row_for_eqn(vartype *eqn, int *rows, int *row, char ktext[6][7], int klen[6]);
int isolate(vartype *eqn, const char *name, int length);
bool has_parameters(equation_data *eqdata);
std::vector<std::string> get_parameters(equation_data *eqdata);

#endif
