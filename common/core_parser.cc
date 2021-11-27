#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <sstream>

#include "core_helpers.h"
#include "core_parser.h"
#include "core_tables.h"
#include "core_variables.h"

////////////////////////////////
/////  class declarations  /////
////////////////////////////////

//////////////////////////////
/////  GeneratorContext  /////
//////////////////////////////

struct Line {
    int cmd;
    arg_struct arg;
    char *buf;
    Line(int cmd) : cmd(cmd), buf(NULL) {
        arg.type = ARGTYPE_NONE;
    }
    Line(phloat d) : cmd(CMD_NUMBER), buf(NULL) {
        arg.type = ARGTYPE_DOUBLE;
        arg.val_d = d;
    }
    Line(int cmd, char s, bool ind) : cmd(cmd), buf(NULL) {
        arg.type = ind ? ARGTYPE_IND_STK : ARGTYPE_STK;
        arg.val.stk = s;
    }
    Line(int cmd, int n, bool ind) : cmd(cmd), buf(NULL) {
        arg.type = ind ? ARGTYPE_IND_NUM : ARGTYPE_NUM;
        arg.val.num = n;
    }
    Line(int cmd, std::string s, bool ind) : cmd(cmd), buf(NULL) {
        if (cmd == CMD_XSTR) {
            int len = s.length();
            if (len > 65535)
                len = 65535;
            buf = (char *) malloc(len);
            memcpy(buf, s.c_str(), len);
            arg.type = ARGTYPE_XSTR;
            arg.val.xstr = buf;
            arg.length = len;
        } else {
            int len = s.length();
            if (len > 7)
                len = 7;
            arg.type = ind ? ARGTYPE_IND_STR : ARGTYPE_STR;
            memcpy(arg.val.text, s.c_str(), len);
            arg.length = len;
        }
    }
    ~Line() {
        free(buf);
    }
};

class GeneratorContext {
    private:

    std::vector<Line *> *lines;
    std::vector<std::vector<Line *> *> stack;
    std::vector<std::vector<Line *> *> queue;
    int lbl;
    int assertTwoRealsLbl;

    public:

    GeneratorContext() {
        lines = new std::vector<Line *>;
        lbl = 0;
        assertTwoRealsLbl = -1;
        // FUNC 01: 0 inputs, 1 output
        addLine(CMD_FUNC, 1);
        addLine(CMD_LNSTK);
    }

    ~GeneratorContext() {
        for (int i = 0; i < lines->size(); i++)
            delete (*lines)[i];
        delete lines;
    }

    void addLine(int cmd) {
        lines->push_back(new Line(cmd));
    }

    void addLine(phloat d) {
        lines->push_back(new Line(d));
    }

    void addLine(int cmd, char s, bool ind = false) {
        lines->push_back(new Line(cmd, s, ind));
    }

    void addLine(int cmd, int n, bool ind = false) {
        lines->push_back(new Line(cmd, n, ind));
    }

    void addLine(int cmd, std::string s, bool ind = false) {
        lines->push_back(new Line(cmd, s, ind));
    }

    int nextLabel() {
        return ++lbl;
    }
    
    void pushSubroutine() {
        stack.push_back(lines);
        lines = new std::vector<Line *>;
    }
    
    void popSubroutine() {
        queue.push_back(lines);
        lines = stack.back();
        stack.pop_back();
    }

    void addAssertTwoReals() {
        if (assertTwoRealsLbl == -1) {
            assertTwoRealsLbl = nextLabel();
            int lbl1 = nextLabel();
            int lbl2 = nextLabel();
            pushSubroutine();
            addLine(CMD_LBL, assertTwoRealsLbl);
            addLine(CMD_REAL_T);
            addLine(CMD_GTOL, lbl1);
            addLine(CMD_RTNERR, 4);
            addLine(CMD_LBL, lbl1);
            addLine(CMD_SWAP);
            addLine(CMD_REAL_T);
            addLine(CMD_GTOL, lbl2);
            addLine(CMD_SWAP);
            addLine(CMD_RTNERR, 4);
            addLine(CMD_LBL, lbl2);
            addLine(CMD_SWAP);
            popSubroutine();
        }
        addLine(CMD_XEQL, assertTwoRealsLbl);
    }

    void store(prgm_struct *prgm) {
        prgm->lclbl_invalid = 0;
        // Tack all the subroutines onto the main code
        for (int i = 0; i < queue.size(); i++) {
            addLine(CMD_RTN);
            std::vector<Line *> *l = queue[i];
            lines->insert(lines->end(), l->begin(), l->end());
            delete l;
        }
        queue.clear();
        // First, resolve labels
        std::map<int, int> label2line;
        int lineno = 1;
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_LBL)
                label2line[line->arg.val.num] = lineno;
            else
                lineno++;
        }
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_GTOL || line->cmd == CMD_XEQL)
                line->arg.val.num = label2line[line->arg.val.num];
        }
        // Label resolution done
        pgm_index saved_prgm = current_prgm;
        current_prgm.set_eqn(prgm->eq_data->eqn_index);
        prgm->text = NULL;
        prgm->size = 0;
        prgm->capacity = 0;
        // Temporarily turn off PRGM mode. This is because
        // store_command() usually refuses to insert commands
        // in programs above prgms_count, in order to prevent
        // users from editing generated code.
        char saved_prgm_mode = flags.f.prgm_mode;
        flags.f.prgm_mode = false;
        bool prev_printer_exists = flags.f.printer_exists;
        flags.f.printer_exists = false;
        bool prev_loading_state = loading_state;
        loading_state = true;
        // First, the end. Doing this before anything else prevents the program count from being bumped.
        arg_struct arg;
        arg.type = ARGTYPE_NONE;
        store_command(0, CMD_END, &arg, NULL);
        // Then, the rest...
        int4 pc = -1;
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_LBL)
                continue;
            store_command_after(&pc, line->cmd, &line->arg, NULL);
        }
        current_prgm = saved_prgm;
        flags.f.prgm_mode = saved_prgm_mode;
        flags.f.printer_exists = prev_printer_exists;
        loading_state = prev_loading_state;
    }
};

//////////////////////////////////////////////
/////  Boilerplate Evaluator subclasses  /////
//////////////////////////////////////////////

class UnaryEvaluator : public Evaluator {

    protected:

    Evaluator *ev;
    bool invertible;

    public:

    UnaryEvaluator(int pos, Evaluator *ev, bool invertible) : Evaluator(pos), ev(ev), invertible(invertible) {}

    ~UnaryEvaluator() {
        delete ev;
    }
    
    void detach() {
        ev = NULL;
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        int n = ev->howMany(name);
        return n == 0 ? 0 : invertible ? n : -1;
    }
};

class BinaryEvaluator : public Evaluator {

    protected:

    Evaluator *left, *right;
    bool invertible;
    bool swapArgs;

    public:

    BinaryEvaluator(int pos, Evaluator *left, Evaluator *right, bool invertible)
        : Evaluator(pos), left(left), right(right), invertible(invertible), swapArgs(false) {}
    BinaryEvaluator(int pos, Evaluator *left, Evaluator *right, bool invertible, bool swapArgs)
        : Evaluator(pos), left(left), right(right), invertible(invertible), swapArgs(swapArgs) {}

    ~BinaryEvaluator() {
        delete left;
        delete right;
    }
    
    void detach() {
        left = NULL;
        right = NULL;
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        int a = left->howMany(name);
        if (a == -1)
            return -1;
        int b = right->howMany(name);
        if (b == -1)
            return -1;
        int c = a + b;
        return c == 0 ? 0 : invertible ? c : -1;
    }
};


//////////////////
/////  Acos  /////
//////////////////

class Acos : public UnaryEvaluator {

    public:

    Acos(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Acos(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ACOS);
    }
};

///////////////////
/////  Acosh  /////
///////////////////

class Acosh : public UnaryEvaluator {

    public:

    Acosh(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Acosh(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ACOSH);
    }
};

//////////////////
/////  Alog  /////
////////////./////

class Alog : public UnaryEvaluator {

    public:

    Alog(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Alog(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_10_POW_X);
    }
};

/////////////////
/////  And  /////
/////////////////

class And : public BinaryEvaluator {

    public:

    And(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new And(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_AND);
    }
};

///////////////////
/////  Angle  /////
///////////////////

class Angle : public BinaryEvaluator {

    public:

    Angle(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Angle(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(CMD_CPX_T);
            ctx->addLine(CMD_GTOL, lbl1);
            ctx->addLine((phloat) 0);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_TO_POL);
            ctx->addLine(CMD_DROP);
            ctx->addLine(CMD_GTOL, lbl2);
            ctx->addLine(CMD_LBL, lbl1);
            ctx->addLine(CMD_PCOMPLX);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_DROP);
            ctx->addLine(CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_TO_POL);
            ctx->addLine(CMD_DROP);
        }
    }
};

//////////////////
/////  Asin  /////
//////////////////

class Asin : public UnaryEvaluator {

    public:

    Asin(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Asin(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ASIN);
    }
};

///////////////////
/////  Asinh  /////
///////////////////

class Asinh : public UnaryEvaluator {

    public:

    Asinh(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Asinh(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ASINH);
    }
};

///////////////////
/////  Array  /////
///////////////////

class Array : public Evaluator {
    
    private:
    
    std::vector<std::vector<Evaluator *> > data;
    bool trans;
    
public:
    
    Array(int pos, std::vector<std::vector<Evaluator *> > data, bool trans) : Evaluator(pos), data(data), trans(trans) {}
    Array(int pos, std::vector<std::vector<Evaluator *> > data, bool trans, int dummy) : Evaluator(pos), trans(trans) {
        for (int i = 0; i < data.size(); i++) {
            std::vector<Evaluator *> row;
            for (int j = 0; j < data[i].size(); j++)
                row.push_back(data[i][j]->clone(NULL));
            this->data.push_back(row);
        }
    }

    ~Array() {
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                delete data[i][j];
    }
    
    Evaluator *clone(For *) {
        return new Array(tpos, data, trans, true);
    }
    
    void generateCode(GeneratorContext *ctx) {
        int rows = data.size();
        int cols = 0;
        for (int i = 0; i < data.size(); i++) {
            int c = data[i].size();
            if (cols < c)
                cols = c;
        }
        int lbl = ctx->nextLabel();
        ctx->addLine(CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(CMD_LBL, lbl);
        ctx->addLine((phloat) rows);
        ctx->addLine((phloat) cols);
        ctx->addLine(CMD_NEWMAT);
        ctx->addLine(CMD_LSTO, std::string("_TMPMAT"));
        ctx->addLine(CMD_DROP);
        ctx->addLine(CMD_INDEX, std::string("_TMPMAT"));
        for (int i = 0; i < rows; i++) {
            int c = data[i].size();
            for (int j = 0; j < c; j++) {
                data[i][j]->generateCode(ctx);
                ctx->addLine(CMD_STOEL);
                ctx->addLine(CMD_DROP);
                if (j < c - 1)
                    ctx->addLine(CMD_J_ADD);
            }
            int gap = cols - c + 1;
            if (i < rows - 1)
                if (gap > 2) {
                    ctx->addLine((phloat) (i + 2));
                    ctx->addLine((phloat) 1);
                    ctx->addLine(CMD_STOIJ);
                    ctx->addLine(CMD_DROPN, 2);
                } else {
                    while (gap-- > 0)
                        ctx->addLine(CMD_J_ADD);
                }
        }
        ctx->addLine(CMD_RCL, std::string("_TMPMAT"));
        if (trans)
            ctx->addLine(CMD_TRANS);
        ctx->popSubroutine();
    }
    
    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                data[i][j]->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                if (data[i][j]->howMany(name) != 0)
                    return -1;
        return 0;
    }
};

//////////////////
/////  Atan  /////
//////////////////

class Atan : public UnaryEvaluator {

    public:

    Atan(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Atan(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ATAN);
    }
};

///////////////////
/////  Atanh  /////
///////////////////

class Atanh : public UnaryEvaluator {

    public:

    Atanh(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Atanh(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ATANH);
    }
};

//////////////////
/////  Badd  /////
//////////////////

class Badd : public BinaryEvaluator {

    public:

    Badd(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Badd(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_BASEADD);
    }
};

//////////////////
/////  Band  /////
//////////////////

class Band : public BinaryEvaluator {

    public:

    Band(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Band(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_AND);
    }
};

//////////////////
/////  Bdiv  /////
//////////////////

class Bdiv : public BinaryEvaluator {

    public:

    Bdiv(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Bdiv(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Bdiv(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_BASEDIV);
    }
};

//////////////////
/////  Bmul  /////
//////////////////

class Bmul : public BinaryEvaluator {

    public:

    Bmul(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Bmul(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_BASEMUL);
    }
};

//////////////////
/////  Bneg  /////
//////////////////

class Bneg : public UnaryEvaluator {

    public:

    Bneg(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Bneg(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_BASECHS);
    }
};

//////////////////
/////  Bnot  /////
//////////////////

class Bnot : public UnaryEvaluator {

    public:

    Bnot(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Bnot(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_NOT);
    }
};

/////////////////
/////  Bor  /////
/////////////////

class Bor : public BinaryEvaluator {

    public:

    Bor(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Bor(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_OR);
    }
};

///////////////////
/////  Break  /////
///////////////////

class Break : public Evaluator {

    private:

    For *f;

    public:

    Break(int pos, For *f) : Evaluator(pos), f(f) {}
    
    Evaluator *clone(For *f) {
        return new Break(tpos, f);
    }

    void generateCode(GeneratorContext *ctx);

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

//////////////////
/////  Bsub  /////
//////////////////

class Bsub : public BinaryEvaluator {

    public:

    Bsub(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Bsub(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Bsub(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_BASESUB);
    }
};

//////////////////
/////  Bxor  /////
//////////////////

class Bxor : public BinaryEvaluator {

    public:

    Bxor(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Bxor(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_XOR);
    }
};

//////////////////
/////  Call  /////
//////////////////

class Call : public Evaluator {

    private:

    std::string name;
    std::vector<Evaluator *> *evs;

    public:

    Call(int pos, std::string name, std::vector<Evaluator *> *evs) : Evaluator(pos), name(name), evs(evs) {}

    ~Call() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Call(tpos, name, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        // Wrapping the equation call in another subroutine,
        // so ->PAR can create locals for the parameters without
        // stepping on any alread-existing locals with the
        // same name.
        int lbl = ctx->nextLabel();
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->generateCode(ctx);
        ctx->addLine(CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(CMD_LBL, lbl);
        ctx->addLine(CMD_XSTR, name);
        ctx->addLine(CMD_GETEQN);
        ctx->addLine(CMD_TO_PAR);
        ctx->addLine(CMD_EVALN, 'L');
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

//////////////////
/////  Comb  /////
//////////////////

class Comb : public BinaryEvaluator {

    public:

    Comb(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Comb(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_COMB);
    }
};

///////////////////////
/////  CompareEQ  /////
///////////////////////

class CompareEQ : public BinaryEvaluator {

    public:

    CompareEQ(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareEQ(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_EQ);
    }
};

///////////////////////
/////  CompareNE  /////
///////////////////////

class CompareNE : public BinaryEvaluator {

    public:

    CompareNE(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareNE(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_NE);
    }
};

///////////////////////
/////  CompareLT  /////
///////////////////////

class CompareLT : public BinaryEvaluator {

    public:

    CompareLT(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareLT(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_LT);
    }
};

///////////////////////
/////  CompareLE  /////
///////////////////////

class CompareLE : public BinaryEvaluator {

    public:

    CompareLE(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareLE(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_LE);
    }
};

///////////////////////
/////  CompareGT  /////
///////////////////////

class CompareGT : public BinaryEvaluator {

    public:

    CompareGT(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareGT(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_GT);
    }
};

///////////////////////
/////  CompareGE  /////
///////////////////////

class CompareGE : public BinaryEvaluator {

    public:

    CompareGE(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareGE(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_GE);
    }
};

//////////////////////
/////  Continue  /////
//////////////////////

class Continue : public Evaluator {

    private:

    For *f;

    public:

    Continue(int pos, For *f) : Evaluator(pos), f(f) {}
    
    Evaluator *clone(For *f) {
        return new Continue(tpos, f);
    }

    void generateCode(GeneratorContext *ctx);

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

/////////////////
/////  Cos  /////
/////////////////

class Cos : public UnaryEvaluator {

    public:

    Cos(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Cos(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_COS);
    }
};

//////////////////
/////  Cosh  /////
//////////////////

class Cosh : public UnaryEvaluator {

    public:

    Cosh(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Cosh(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_COSH);
    }
};

///////////////////
/////  Cross  /////
///////////////////

class Cross : public BinaryEvaluator {

    public:

    Cross(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Cross(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_CROSS);
    }
};

//////////////////
/////  Date  /////
//////////////////

class Date : public BinaryEvaluator {

    public:

    Date(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Date(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Date(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_DATE_PLUS);
    }
};

///////////////////
/////  Ddays  /////
///////////////////

class Ddays : public Evaluator {

    private:

    Evaluator *date1;
    Evaluator *date2;
    Evaluator *cal;

    public:

    Ddays(int pos, Evaluator *date1, Evaluator *date2, Evaluator *cal) : Evaluator(pos), date1(date1), date2(date2), cal(cal) {}

    Evaluator *clone(For *f) {
        return new Ddays(tpos, date1->clone(f), date2->clone(f), cal->clone(f));
    }

    ~Ddays() {
        delete date1;
        delete date2;
        delete cal;
    }

    void generateCode(GeneratorContext *ctx) {
        date1->generateCode(ctx);
        date2->generateCode(ctx);
        cal->generateCode(ctx);
        ctx->addLine(CMD_DDAYSC);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        date1->collectVariables(vars, locals);
        date2->collectVariables(vars, locals);
        cal->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (date1->howMany(name) != 0
                || date2->howMany(name) != 0
                || cal->howMany(name) != 0)
            return -1;
        else
            return 0;
    }
};

/////////////////
/////  Dec  /////
/////////////////

class Dec : public UnaryEvaluator {

    public:

    Dec(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Dec(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TO_DEC);
    }
};

/////////////////
/////  Deg  /////
/////////////////

class Deg : public UnaryEvaluator {

    public:

    Deg(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Deg(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TO_DEG);
    }
};

////////////////////////
/////  Difference  /////
////////////////////////

class Difference : public BinaryEvaluator {

    public:

    Difference(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Difference(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Difference(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_SUB);
    }
};

/////////////////
/////  Dot  /////
/////////////////

class Dot : public BinaryEvaluator {

    public:

    Dot(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Dot(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_DOT);
    }
};

/////////////////
/////  Ell  /////
/////////////////

class Ell : public Evaluator {

    private:

    std::string name;
    Evaluator *left, *right;
    bool compatMode;

    public:

    Ell(int pos, std::string name, Evaluator *right, bool compatMode) : Evaluator(pos), name(name), left(NULL), right(right), compatMode(compatMode) {}
    Ell(int pos, Evaluator *left, Evaluator *right, bool compatMode) : Evaluator(pos), name(""), left(left), right(right), compatMode(compatMode) {}

    ~Ell() {
        delete left;
        delete right;
    }

    Evaluator *clone(For *f) {
        if (name != "")
            return new Ell(tpos, name, right->clone(f), compatMode);
        else
            return new Ell(tpos, left->clone(f), right->clone(f), compatMode);
    }

    void generateCode(GeneratorContext *ctx) {
        if (name != "") {
            right->generateCode(ctx);
            ctx->addLine(compatMode ? CMD_GSTO : CMD_STO, name);
        } else {
            left->generateCode(ctx);
            right->generateCode(ctx);
            left->generateAssignmentCode(ctx);
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        if (left != NULL)
            left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *nam) {
        if (left != NULL && left->howMany(nam) != 0)
            return -1;
        return right->howMany(nam) == 0 ? 0 : -1;
    }
};

//////////////////////
/////  Equation  /////
//////////////////////

class Equation : public BinaryEvaluator {

    public:

    Equation(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Equation(tpos, left->clone(f), right->clone(f));
    }

    bool isEquation() {
        return true;
    }

    void getSides(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
        if (left->howMany(name) == 1) {
            *lhs = left;
            *rhs = right;
        } else {
            *lhs = right;
            *rhs = left;
        }
        left = NULL;
        right = NULL;
        delete this;
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SUB);
    }
};

/////////////////
/////  Ess  /////
/////////////////

class Ess : public Evaluator {

    private:

    std::string name;

    public:

    Ess(int pos, std::string name) : Evaluator(pos), name(name) {}

    Evaluator *clone(For *f) {
        return new Ess(tpos, name);
    }

    bool isBool() { return true; }
    
    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_XSTR, name);
        ctx->addLine(CMD_SVAR);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *nam) {
        return *nam == name ? -1 : 0;
    }
};

/////////////////
/////  Exp  /////
/////////////////

class Exp : public UnaryEvaluator {

    public:

    Exp(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Exp(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_E_POW_X);
    }
};

///////////////////
/////  Expm1  /////
///////////////////

class Expm1 : public UnaryEvaluator {

    public:

    Expm1(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Expm1(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_E_POW_X_1);
    }
};

/////////////////
/////  For  /////
/////////////////

class For : public Evaluator {

    private:

    Evaluator *init, *cond, *next;
    std::vector<Evaluator *> *evs;
    int breakLbl, continueLbl;

    public:

    For(int pos) : Evaluator(pos), init(NULL), cond(NULL), next(NULL), evs(NULL) {}

    ~For() {
        delete init;
        delete cond;
        delete next;
        if (evs != NULL) {
            for (int i = 0; i < evs->size(); i++)
                delete (*evs)[i];
            delete evs;
        }
    }

    Evaluator *clone(For *) {
        For *f = new For(tpos);
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        f->finish_init(init->clone(f), cond->clone(f), next->clone(f), evs2);
        return f;
    }

    void finish_init(Evaluator *init, Evaluator *cond, Evaluator *next, std::vector<Evaluator *> *evs) {
        this->init = init;
        this->cond = cond;
        this->next = next;
        this->evs = evs;
    }

    int getBreak() {
        return breakLbl;
    }

    int getContinue() {
        return continueLbl;
    }

    void generateCode(GeneratorContext *ctx) {
        breakLbl = ctx->nextLabel();
        continueLbl = ctx->nextLabel();
        int top = ctx->nextLabel();
        int test = ctx->nextLabel();
        init->generateCode(ctx);
        ctx->addLine(CMD_GTOL, test);
        ctx->addLine(CMD_LBL, top);
        for (int i = 0; i < evs->size(); i++) {
            (*evs)[i]->generateCode(ctx);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_DROP);
        }
        ctx->addLine(CMD_LBL, continueLbl);
        next->generateCode(ctx);
        ctx->addLine(CMD_DROP);
        ctx->addLine(CMD_LBL, test);
        cond->generateCode(ctx);
        ctx->addLine(CMD_IF_T);
        ctx->addLine(CMD_GTOL, top);
        ctx->addLine(CMD_LBL, breakLbl);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        init->collectVariables(vars, locals);
        cond->collectVariables(vars, locals);
        next->collectVariables(vars, locals);
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (init->howMany(name) != 0
                || cond->howMany(name) != 0
                || next->howMany(name) != 0)
            return -1;
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Gee  /////
/////////////////

class Gee : public Evaluator {

    private:

    std::string name;
    bool compatMode;

    public:

    Gee(int pos, std::string name, bool compatMode) : Evaluator(pos), name(name), compatMode(compatMode) {}

    Evaluator *clone(For *f) {
        return new Gee(tpos, name, compatMode);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(compatMode ? CMD_GRCL : CMD_RCL, name);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

/////////////////
/////  Hms  /////
/////////////////

class Hms : public UnaryEvaluator {

    public:

    Hms(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Hms(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TO_HMS);
    }
};

////////////////////
/////  Hmsadd  /////
////////////////////

class Hmsadd : public BinaryEvaluator {

    public:

    Hmsadd(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Hmsadd(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_HMSADD);
    }
};

////////////////////
/////  Hmssub  /////
////////////////////

class Hmssub : public BinaryEvaluator {

    public:

    Hmssub(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Hmssub(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Hmssub(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_HMSSUB);
    }
};

/////////////////
/////  Hrs  /////
/////////////////

class Hrs : public UnaryEvaluator {

    public:

    Hrs(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Hrs(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TO_HR);
    }
};

//////////////////
/////  Idiv  /////
//////////////////

class Idiv : public BinaryEvaluator {

    public:

    Idiv(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Idiv(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_DIV);
        ctx->addLine(CMD_IP);
    }
};

////////////////
/////  If  /////
////////////////

class If : public Evaluator {

    private:

    Evaluator *condition, *trueEv, *falseEv;

    public:

    If(int pos, Evaluator *condition, Evaluator *trueEv, Evaluator *falseEv)
            : Evaluator(pos), condition(condition), trueEv(trueEv), falseEv(falseEv) {}

    ~If() {
        delete condition;
        delete trueEv;
        delete falseEv;
    }

    Evaluator *clone(For *f) {
        return new If(tpos, condition->clone(f), trueEv->clone(f), falseEv->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        condition->generateCode(ctx);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(CMD_IF_T);
        ctx->addLine(CMD_GTOL, lbl1);
        falseEv->generateCode(ctx);
        ctx->addLine(CMD_GTOL, lbl2);
        ctx->addLine(CMD_LBL, lbl1);
        trueEv->generateCode(ctx);
        ctx->addLine(CMD_LBL, lbl2);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        condition->collectVariables(vars, locals);
        trueEv->collectVariables(vars, locals);
        falseEv->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (condition->howMany(name) != 0
                || trueEv->howMany(name) != 0
                || falseEv->howMany(name) != 0)
            return -1;
        else
            return 0;
    }
};

/////////////////
/////  Int  /////
/////////////////

class Int : public UnaryEvaluator {

    public:

    Int(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone(For *f) {
        return new Int(tpos, ev->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_IP);
        ctx->addLine(CMD_X_EQ_NN, 'L');
        int lbl = ctx->nextLabel();
        ctx->addLine(CMD_GTOL, lbl);
        ctx->addLine(CMD_0_LT_NN, 'L');
        ctx->addLine(CMD_GTOL, lbl);
        ctx->addLine((phloat) 1);
        ctx->addLine(CMD_SUB);
        ctx->addLine(CMD_LBL, lbl);
    }
};

///////////////////
/////  Integ  /////
///////////////////

class Integ : public Evaluator {

    private:

    Evaluator *expr;
    std::string integ_var;
    Evaluator *llim;
    Evaluator *ulim;

    public:

    Integ(int tpos, Evaluator *expr, std::string integ_var, Evaluator *llim, Evaluator *ulim)
        : Evaluator(tpos), expr(expr), integ_var(integ_var), llim(llim), ulim(ulim) {}

    ~Integ() {
        delete expr;
        delete llim;
        delete ulim;
    }

    Evaluator *clone(For *f) {
        return new Integ(tpos, expr->clone(f), integ_var, llim->clone(f), ulim->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_XSTR, expr->getText());
        ctx->addLine(CMD_PARSE);
        ctx->addLine(CMD_PGMINT, 'X', true);
        llim->generateCode(ctx);
        ctx->addLine(CMD_LSTO, std::string("LLIM"));
        ulim->generateCode(ctx);
        ctx->addLine(CMD_LSTO, std::string("ULIM"));
        ctx->addLine(CMD_DROPN, 3);
        ctx->addLine(CMD_INTEG, integ_var);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_DROP);
    }
    
    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        locals->push_back(integ_var);
        expr->collectVariables(vars, locals);
        llim->collectVariables(vars, locals);
        ulim->collectVariables(vars, locals);
        addIfNew("ACC", vars, locals);
        locals->pop_back();
    }

    int howMany(const std::string *nam) {
        if (*nam != integ_var) {
            if (llim->howMany(nam) != 0
                    || ulim->howMany(nam) != 0)
                return -1;
        }
        return 0;
    }
};

/////////////////
/////  Inv  /////
/////////////////

class Inv : public UnaryEvaluator {

    public:

    Inv(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Inv(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_INV);
    }
};

//////////////////
/////  Item  /////
//////////////////

class Item : public Evaluator {

    private:

    std::string name;
    Evaluator *ev1, *ev2;
    bool lvalue;

    public:

    Item(int pos, std::string name, Evaluator *ev1, Evaluator *ev2) : Evaluator(pos), name(name), ev1(ev1), ev2(ev2), lvalue(false) {}

    ~Item() {
        delete ev1;
        delete ev2;
    }

    Evaluator *clone(For *f) {
        Evaluator *ret = new Item(tpos, name, ev1->clone(f), ev2 == NULL ? NULL : ev2->clone(f));
        if (lvalue)
            ret->makeLvalue();
        return ret;
    }
    
    bool makeLvalue() {
        lvalue = true;
        return true;
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_XSTR, name);
        ev1->generateCode(ctx);
        if (ev2 != NULL)
            ev2->generateCode(ctx);
        if (!lvalue)
            ctx->addLine(CMD_GETITEM);
    }
    
    void generateAssignmentCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_PUTITEM);
    }
    
    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev1->collectVariables(vars, locals);
        if (ev2 != NULL)
            ev2->collectVariables(vars, locals);
    }

    int howMany(const std::string *nam) {
        if (*nam == name || ev1->howMany(nam) != 0 || ev2 != NULL && ev2->howMany(nam) != 0)
            return -1;
        else
            return 0;
    }
};

/////////////////////
/////  Literal  /////
/////////////////////

class Literal : public Evaluator {

    private:

    phloat value;

    public:

    Literal(int pos, phloat value) : Evaluator(pos), value(value) {}

    Evaluator *clone(For *f) {
        return new Literal(tpos, value);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(value);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

////////////////
/////  Ln  /////
////////////////

class Ln : public UnaryEvaluator {

    public:

    Ln(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Ln(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_LN);
    }
};

//////////////////
/////  Ln1p  /////
//////////////////

class Ln1p : public UnaryEvaluator {

    public:

    Ln1p(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Ln1p(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_LN_1_X);
    }
};

/////////////////
/////  Log  /////
/////////////////

class Log : public UnaryEvaluator {

    public:

    Log(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Log(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_LOG);
    }
};

/////////////////
/////  Max  /////
/////////////////

class Max : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Max(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}

    ~Max() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Max(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        if (evs->size() == 0) {
            ctx->addLine(NEG_HUGE_PHLOAT);
        } else {
            (*evs)[0]->generateCode(ctx);
            for (int i = 1; i < evs->size(); i++) {
                (*evs)[i]->generateCode(ctx);
                ctx->addLine(CMD_X_GT_Y);
                ctx->addLine(CMD_SWAP);
                ctx->addLine(CMD_DROP);
            }
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Min  /////
/////////////////

class Min : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Min(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}

    ~Min() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Min(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        if (evs->size() == 0) {
            ctx->addLine(POS_HUGE_PHLOAT);
        } else {
            (*evs)[0]->generateCode(ctx);
            for (int i = 1; i < evs->size(); i++) {
                (*evs)[i]->generateCode(ctx);
                ctx->addLine(CMD_X_LT_Y);
                ctx->addLine(CMD_SWAP);
                ctx->addLine(CMD_DROP);
            }
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Mod  /////
/////////////////

class Mod : public BinaryEvaluator {

    public:

    Mod(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Mod(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_MOD);
    }
};

/////////////////////
/////  NameTag  /////
/////////////////////

class NameTag : public UnaryEvaluator {
    
    private:
    
    std::string name;
    std::vector<std::string> *params;
    
    public:
    
    NameTag(int pos, std::string name, std::vector<std::string> *params, Evaluator *ev)
            : UnaryEvaluator(pos, ev, false), name(name), params(params) {}
    
    ~NameTag() {
        delete params;
    }

    Evaluator *clone(For *f) {
        return new NameTag(tpos, name, new std::vector<std::string>(*params), ev->clone(f));
    }
    
    std::string eqnName() {
        return name;
    }

    std::vector<std::string> *eqnParamNames() {
        return params;
    }

    Evaluator *removeName() {
        Evaluator *ret = ev;
        ev = NULL;
        delete this;
        return ret;
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
    }
};

//////////////////////
/////  Negative  /////
//////////////////////

class Negative : public UnaryEvaluator {

    public:

    Negative(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Negative(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_CHS);
    }
};

////////////////////
/////  Newmat  /////
////////////////////

class Newmat : public BinaryEvaluator {

    public:

    Newmat(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Newmat(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_NEWMAT);
    }
};

/////////////////
/////  Oct  /////
/////////////////

class Oct : public UnaryEvaluator {

    public:

    Oct(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Oct(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TO_OCT);
    }
};

////////////////
/////  Or  /////
////////////////

class Or : public BinaryEvaluator {

    public:

    Or(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Or(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_OR);
    }
};

/////////////////////
/////  Pcomplx  /////
/////////////////////

class Pcomplx : public BinaryEvaluator {

    public:

    Pcomplx(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Pcomplx(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addAssertTwoReals();
        ctx->addLine(CMD_PCOMPLX);
    }
};

//////////////////
/////  Perm  /////
//////////////////

class Perm : public BinaryEvaluator {

    public:

    Perm(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Perm(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_PERM);
    }
};

///////////////////
/////  Power  /////
///////////////////

class Power : public BinaryEvaluator {

    public:

    Power(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Power(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Power(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_Y_POW_X);
    }
};

/////////////////////
/////  Product  /////
/////////////////////

class Product : public BinaryEvaluator {

    public:

    Product(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Product(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_MUL);
    }
};

//////////////////////
/////  Quotient  /////
//////////////////////

class Quotient : public BinaryEvaluator {

    public:

    Quotient(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Quotient(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Quotient(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_DIV);
    }
};

/////////////////
/////  Rad  /////
/////////////////

class Rad : public UnaryEvaluator {

    public:

    Rad(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Rad(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TO_RAD);
    }
};

////////////////////
/////  Radius  /////
////////////////////

class Radius : public BinaryEvaluator {

    public:

    Radius(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Radius(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(CMD_CPX_T);
            ctx->addLine(CMD_GTOL, lbl1);
            ctx->addLine((phloat) 0);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_TO_POL);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_DROP);
            ctx->addLine(CMD_GTOL, lbl2);
            ctx->addLine(CMD_LBL, lbl1);
            ctx->addLine(CMD_PCOMPLX);
            ctx->addLine(CMD_DROP);
            ctx->addLine(CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_TO_POL);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_DROP);
        }
    }
};

/////////////////////
/////  Rcomplx  /////
/////////////////////

class Rcomplx : public BinaryEvaluator {

    public:

    Rcomplx(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Rcomplx(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addAssertTwoReals();
        ctx->addLine(CMD_RCOMPLX);
    }
};

////////////////////////////
/////  RecallFunction  /////
////////////////////////////

class RecallFunction : public Evaluator {

    private:

    int cmd;

    public:

    RecallFunction(int pos, int cmd) : Evaluator(pos), cmd(cmd) {}

    Evaluator *clone(For *) {
        return new RecallFunction(tpos, cmd);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(cmd);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

////////////////////////////////////
/////  RecallOneOfTwoFunction  /////
////////////////////////////////////

class RecallOneOfTwoFunction : public Evaluator {

    private:

    int cmd;
    bool pick_x;

    public:

    RecallOneOfTwoFunction(int pos, int cmd, bool pick_x) : Evaluator(pos), cmd(cmd), pick_x(pick_x) {}

    Evaluator *clone(For *) {
        return new RecallOneOfTwoFunction(tpos, cmd, pick_x);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(cmd);
        if (pick_x)
            ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_DROP);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

//////////////////////
/////  Register  /////
//////////////////////

class Register : public Evaluator {
    
    private:
    
    int index; // X=1, Y=2, Z=3, T=4
    Evaluator *ev;
    
    public:
    
    Register(int pos, int index) : Evaluator(pos), index(index), ev(NULL) {}
    Register(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    
    ~Register() {
        delete ev;
    }

    Evaluator *clone(For *f) {
        if (ev == NULL)
            return new Register(tpos, index);
        else
            return new Register(tpos, ev);
    }
    
    void generateCode(GeneratorContext *ctx) {
        if (ev == NULL)
            ctx->addLine((phloat) index);
        else
            ev->generateCode(ctx);
        // TODO: Range check?
        ctx->addLine(CMD_FDEPTH);
        ctx->addLine(CMD_ADD);
        ctx->addLine(CMD_PICK, 'X', true);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_DROP);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        if (ev != NULL)
            ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (ev == NULL)
            return 0;
        else
            return ev->howMany(name) == 0 ? 0 : -1;
    }
};

/////////////////
/////  Rnd  /////
/////////////////

class Rnd : public BinaryEvaluator {

    private:

    bool trunc;

    public:

    Rnd(int pos, Evaluator *left, Evaluator *right, bool trunc) : BinaryEvaluator(pos, left, right, false), trunc(trunc) {}

    Evaluator *clone(For *f) {
        return new Rnd(tpos, left->clone(f), right->clone(f), trunc);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCLFLAG);
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_X_LT_0);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(CMD_GTOL, lbl1);
        ctx->addLine(CMD_FIX, 'X', true);
        ctx->addLine(CMD_GTOL, lbl2);
        ctx->addLine(CMD_LBL, lbl1);
        ctx->addLine((phloat) -1);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_SUB);
        ctx->addLine(CMD_SCI, 'X', true);
        ctx->addLine(CMD_LBL, lbl2);
        ctx->addLine(CMD_DROP);
        ctx->addLine(trunc ? CMD_TRUNC : CMD_RND);
        ctx->addLine(CMD_SWAP);
        ctx->addLine((phloat) 36.41);
        ctx->addLine(CMD_STOFLAG);
        ctx->addLine(CMD_DROPN, 2);
    }
};

/////////////////
/////  Seq  /////
/////////////////

class Seq : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Seq(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}

    ~Seq() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Seq(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        for (int i = 0; i < evs->size() - 1; i++) {
            (*evs)[i]->generateCode(ctx);
            ctx->addLine(CMD_DROP);
        }
        evs->back()->generateCode(ctx);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Sgn  /////
/////////////////

class Sgn : public UnaryEvaluator {

    public:

    Sgn(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone(For *f) {
        return new Sgn(tpos, ev->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_REAL_T);
        ctx->addLine(CMD_X_NE_0);
        ctx->addLine(CMD_SIGN);
    }
};

///////////////////
/////  Sigma  /////
///////////////////

class Sigma : public Evaluator {

    private:

    std::string name;
    Evaluator *from;
    Evaluator *to;
    Evaluator *step;
    Evaluator *ev;
    
    public:

    Sigma(int pos, std::string name, Evaluator *from, Evaluator *to, Evaluator *step, Evaluator *ev)
        : Evaluator(pos), name(name), from(from), to(to), step(step), ev(ev) {}

    ~Sigma() {
        delete from;
        delete to;
        delete step;
        delete ev;
    }

    Evaluator *clone(For *f) {
        return new Sigma(tpos, name, from->clone(NULL), to->clone(NULL), step->clone(NULL), ev->clone(NULL));
    }

    void generateCode(GeneratorContext *ctx) {
        to->generateCode(ctx);
        step->generateCode(ctx);
        from->generateCode(ctx);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        int lbl3 = ctx->nextLabel();
        ctx->addLine(CMD_XEQL, lbl1);
        ctx->pushSubroutine();
        ctx->addLine(CMD_LBL, lbl1);
        ctx->addLine(CMD_LSTO, name);
        ctx->addLine(CMD_DROP);
        ctx->addLine((phloat) 0);
        ctx->addLine(CMD_LBL, lbl2);
        ev->generateCode(ctx);
        ctx->addLine(CMD_ADD);
        ctx->addLine(CMD_RCL, name);
        ctx->addLine(CMD_RCL_ADD, 'Z');
        ctx->addLine(CMD_STO, name);
        ctx->addLine(CMD_X_GT_NN, 'T');
        ctx->addLine(CMD_GTOL, lbl3);
        ctx->addLine(CMD_DROP);
        ctx->addLine(CMD_GTOL, lbl2);
        ctx->addLine(CMD_LBL, lbl3);
        ctx->addLine(CMD_RDNN, 4);
        ctx->addLine(CMD_RDNN, 4);
        ctx->addLine(CMD_DROPN, 3);
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        locals->push_back(name);
        from->collectVariables(vars, locals);
        to->collectVariables(vars, locals);
        step->collectVariables(vars, locals);
        ev->collectVariables(vars, locals);
        locals->pop_back();
    }

    int howMany(const std::string *nam) {
        if (*nam != name) {
            if (ev->howMany(nam) != 0)
                return -1;
        }
        if (from->howMany(nam) != 0
                || to->howMany(nam) != 0
                || step->howMany(nam) != 0)
            return -1;
        return 0;
    }
};

/////////////////
/////  Sin  /////
/////////////////

class Sin : public UnaryEvaluator {

    public:

    Sin(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Sin(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SIN);
    }
};

//////////////////
/////  Sinh  /////
//////////////////

class Sinh : public UnaryEvaluator {

    public:

    Sinh(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Sinh(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SINH);
    }
};

//////////////////
/////  Size  /////
//////////////////

class Size : public UnaryEvaluator {

    private:

    char mode;

    public:

    Size(int pos, Evaluator *ev, char mode) : UnaryEvaluator(pos, ev, false), mode(mode) {}

    Evaluator *clone(For *f) {
        return new Size(tpos, ev->clone(f), mode);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        if (mode == 'S') {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(CMD_LIST_T);
            ctx->addLine(CMD_GTOL, lbl1);
            ctx->addLine(CMD_DIM_T);
            ctx->addLine(CMD_MUL);
            ctx->addLine(CMD_GTOL, lbl2);
            ctx->addLine(CMD_LBL, lbl1);
            ctx->addLine(CMD_LENGTH);
            ctx->addLine(CMD_LBL, lbl2);
        } else {
            ctx->addLine(CMD_DIM_T);
            if (mode == 'C')
                ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_DROP);
        }
    }
};

////////////////
/////  Sq  /////
////////////////

class Sq : public UnaryEvaluator {

    public:

    Sq(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Sq(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SQUARE);
    }
};

//////////////////
/////  Sqrt  /////
//////////////////

class Sqrt : public UnaryEvaluator {

    public:

    Sqrt(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Sqrt(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SQRT);
    }
};

/////////////////////
/////  StatSum  /////
/////////////////////

class StatSum : public Evaluator {

    private:

    int idx;

    public:

    StatSum(int pos, int idx) : Evaluator(pos), idx(idx) {}

    Evaluator *clone(For *) {
        return new StatSum(tpos, idx);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_SIGMAREG_T);
        ctx->addLine((phloat) idx);
        ctx->addLine(CMD_ADD);
        ctx->addLine(CMD_SF, 30);
        ctx->addLine(CMD_RCL, 'X', true);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

///////////////////////////
/////  Subexpression  /////
///////////////////////////

class Subexpression : public Evaluator {

    private:

    Evaluator *ev;
    std::string text;

    public:

    Subexpression(int pos, Evaluator *ev, std::string text) : Evaluator(pos), ev(ev), text(text) {}

    ~Subexpression() {
        delete ev;
    }

    Evaluator *clone(For *) {
        return new Subexpression(tpos, ev->clone(NULL), text);
    }

    std::string getText() {
        return text;
    }

    void generateCode(GeneratorContext *ctx) {
        // this is handled by Integ
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name) == 0 ? 0 : -1;
    }
};

/////////////////
/////  Sum  /////
/////////////////

class Sum : public BinaryEvaluator {

    public:

    Sum(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Sum(tpos, left->clone(f), right->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_ADD);
    }
};

/////////////////
/////  Tan  /////
/////////////////

class Tan : public UnaryEvaluator {

    public:

    Tan(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Tan(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TAN);
    }
};

//////////////////
/////  Tanh  /////
//////////////////

class Tanh : public UnaryEvaluator {

    public:

    Tanh(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Tanh(tpos, ev->clone(f));
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TANH);
    }
};

//////////////////////
/////  TypeTest  /////
//////////////////////

class TypeTest : public UnaryEvaluator {

    private:

    int cmd;

    public:

    TypeTest(int pos, Evaluator *ev, int cmd) : UnaryEvaluator(pos, ev, false), cmd(cmd) {}

    Evaluator *clone(For *f) {
        return new TypeTest(tpos, ev->clone(f), cmd);
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(cmd);
        ctx->addLine(CMD_GTOL, lbl1);
        ctx->addLine(CMD_DROP);
        ctx->addLine((phloat) 0);
        ctx->addLine(CMD_GTOL, lbl2);
        ctx->addLine(CMD_LBL, lbl1);
        ctx->addLine(CMD_DROP);
        ctx->addLine((phloat) 1);
        ctx->addLine(CMD_LBL, lbl2);
    }
};

///////////////////////////
/////  UnaryFunction  /////
///////////////////////////

class UnaryFunction : public UnaryEvaluator {

    private:

    int cmd;

    public:

    UnaryFunction(int pos, Evaluator *ev, int cmd) : UnaryEvaluator(pos, ev, false), cmd(cmd) {}

    Evaluator *clone(For *f) {
        return new UnaryFunction(tpos, ev->clone(f), cmd);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(cmd);
    }
};

//////////////////////
/////  Variable  /////
//////////////////////

class Variable : public Evaluator {

    private:

    std::string nam;

    public:

    Variable(int pos, std::string name) : Evaluator(pos), nam(name) {}

    Evaluator *clone(For *f) {
        return new Variable(tpos, nam);
    }
    
    std::string name() { return nam; }

    bool is(const std::string *name) { return *name == nam; }
    
    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCL, nam);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        addIfNew(nam, vars, locals);
    }

    int howMany(const std::string *name) {
        return nam == *name;
    }
};

////////////////////
/////  Xcoord  /////
////////////////////

class Xcoord : public BinaryEvaluator {

    public:

    Xcoord(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Xcoord(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(CMD_CPX_T);
            ctx->addLine(CMD_GTOL, lbl1);
            ctx->addLine(CMD_GTOL, lbl2);
            ctx->addLine(CMD_LBL, lbl1);
            ctx->addLine(CMD_RCOMPLX);
            ctx->addLine(CMD_DROP);
            ctx->addLine(CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_TO_REC);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_DROP);
        }
    }
};

/////////////////
/////  Xeq  /////
/////////////////

class Xeq : public Evaluator {

    private:

    std::string name;
    std::vector<Evaluator *> *evs;

    public:

    Xeq(int pos, std::string name, std::vector<Evaluator *> *evs) : Evaluator(pos), name(name), evs(evs) {}

    ~Xeq() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Xeq(tpos, name, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        // Wrapping the subroutine call in another subroutine,
        // so ->PAR can create locals for the parameters without
        // stepping on any already-existing locals with the
        // same name.
        int lbl = ctx->nextLabel();
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->generateCode(ctx);
        ctx->addLine(CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(CMD_LBL, lbl);
        ctx->addLine(CMD_XSTR, name);
        ctx->addLine(CMD_TO_PAR);
        ctx->addLine(CMD_XEQ, name);
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Xor  /////
/////////////////

class Xor : public BinaryEvaluator {

    public:

    Xor(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Xor(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_XOR);
    }
};

////////////////////
/////  Ycoord  /////
////////////////////

class Ycoord : public BinaryEvaluator {

    public:

    Ycoord(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Ycoord(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(CMD_CPX_T);
            ctx->addLine(CMD_GTOL, lbl1);
            ctx->addLine(CMD_DROP);
            ctx->addLine((phloat) 0);
            ctx->addLine(CMD_GTOL, lbl2);
            ctx->addLine(CMD_LBL, lbl1);
            ctx->addLine(CMD_RCOMPLX);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_DROP);
            ctx->addLine(CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(CMD_SWAP);
            ctx->addLine(CMD_TO_REC);
            ctx->addLine(CMD_DROP);
        }
    }
};

/* Methods that can't be defined in their class declarations
 * because they reference other Evaluator classes 
 *
 * NOTE: The invert() methods may emit nodes with swapArgs=true, but it doesn't
 * pay attention to swapArgs in the nodes it reads. The idea is that the
 * inverter logic will only be applied to parse trees created from strings, and
 * those will never have swapArgs=true; the trees created by the inverter may
 * contain swapArgs=true here or there, but they will never be fed back into
 * the inverter.
 * Should the need ever arise to apply the inverter to its own output, this
 * would have to be dealt with of course, but that doesn't seem likely.
 */

bool Acos::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Cos(0, *rhs);
    return true;
}

bool Acosh::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Cosh(0, *rhs);
    return true;
}

bool Alog::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Log(0, *rhs);
    return true;
}

bool Asin::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Sin(0, *rhs);
    return true;
}

bool Asinh::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Sinh(0, *rhs);
    return true;
}

bool Atan::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Tan(0, *rhs);
    return true;
}

bool Atanh::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Tanh(0, *rhs);
    return true;
}

bool Badd::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Bsub(0, right, *rhs, true);
    } else {
        *lhs = right;
        *rhs = new Bsub(0, left, *rhs, true);
    }
    return true;
}

bool Bdiv::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Bmul(0, right, *rhs);
    } else {
        *lhs = right;
        *rhs = new Bdiv(0, left, *rhs);
    }
    return true;
}

bool Bmul::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Bdiv(0, right, *rhs, true);
    } else {
        *lhs = right;
        *rhs = new Bdiv(0, left, *rhs, true);
    }
    return true;
}

bool Bneg::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Bneg(0, *rhs);
    return true;
}

bool Bnot::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Bnot(0, *rhs);
    return true;
}

bool Bsub::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Badd(0, right, *rhs);
    } else {
        *lhs = right;
        *rhs = new Bsub(0, left, *rhs);
    }
    return true;
}

bool Bxor::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Bxor(0, right, *rhs);
    } else {
        *lhs = right;
        *rhs = new Bxor(0, left, *rhs);
    }
    return true;
}

bool Cos::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Acos(0, *rhs);
    return true;
}

bool Cosh::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Acosh(0, *rhs);
    return true;
}

bool Date::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Date(0, new Negative(0, right), *rhs, true);
    } else {
        *lhs = right;
        *rhs = new Ddays(0, left, *rhs, new Literal(0, 1));
    }
    return true;
}

bool Dec::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Oct(0, *rhs);
    return true;
}

bool Deg::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Rad(0, *rhs);
    return true;
}

bool Difference::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Sum(0, right, *rhs);
    } else {
        *lhs = right;
        *rhs = new Difference(0, left, *rhs);
    }
    return true;
}

bool Exp::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Ln(0, *rhs);
    return true;
}

bool Expm1::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Ln1p(0, *rhs);
    return true;
}

bool Hms::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Hrs(0, *rhs);
    return true;
}

bool Hmsadd::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Hmssub(0, right, *rhs, true);
    } else {
        *lhs = right;
        *rhs = new Hmssub(0, left, *rhs, true);
    }
    return true;
}

bool Hmssub::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Hmsadd(0, right, *rhs);
    } else {
        *lhs = right;
        *rhs = new Hmssub(0, left, *rhs);
    }
    return true;
}

bool Hrs::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Hms(0, *rhs);
    return true;
}

bool Inv::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Inv(0, *rhs);
    return true;
}

bool Ln::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Exp(0, *rhs);
    return true;
}

bool Ln1p::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Expm1(0, *rhs);
    return true;
}

bool Log::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Alog(0, *rhs);
    return true;
}

bool Negative::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Negative(0, *rhs);
    return true;
}

bool Oct::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Dec(0, *rhs);
    return true;
}

bool Power::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Power(0, new Inv(0, right), *rhs, true);
    } else {
        *lhs = right;
        *rhs = new Quotient(0, new Ln(0, left), new Ln(0, *rhs), true);
    }
    return true;
}

bool Product::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Quotient(0, right, *rhs, true);
    } else {
        *lhs = right;
        *rhs = new Quotient(0, left, *rhs, true);
    }
    return true;
}

bool Quotient::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Product(0, right, *rhs);
    } else {
        *lhs = right;
        *rhs = new Quotient(0, left, *rhs);
    }
    return true;
}

bool Rad::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Deg(0, *rhs);
    return true;
}

bool Sin::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Asin(0, *rhs);
    return true;
}

bool Sinh::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Asinh(0, *rhs);
    return true;
}

bool Sq::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Sqrt(0, *rhs);
    return true;
}

bool Sqrt::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Sq(0, *rhs);
    return true;
}

bool Sum::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    if (left->howMany(name) == 1) {
        *lhs = left;
        *rhs = new Difference(0, right, *rhs, true);
    } else {
        *lhs = right;
        *rhs = new Difference(0, left, *rhs, true);
    }
    return true;
}

bool Tan::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Atan(0, *rhs);
    return true;
}

bool Tanh::invert(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = ev;
    *rhs = new Atanh(0, *rhs);
    return true;
}

void Break::generateCode(GeneratorContext *ctx) {
    if (f == NULL)
        ctx->addLine(CMD_XSTR, std::string("BREAK"));
    else
        ctx->addLine(CMD_GTOL, f->getBreak());
}

void Continue::generateCode(GeneratorContext *ctx) {
    if (f == NULL)
        ctx->addLine(CMD_XSTR, std::string("CONTINUE"));
    else
        ctx->addLine(CMD_GTOL, f->getContinue());
}

void Evaluator::getSides(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = this;
    *rhs = new Literal(0, 0);
}

void Evaluator::addIfNew(std::string name, std::vector<std::string> *vars, std::vector<std::string> *locals) {
    for (int i = 0; i < locals->size(); i++)
        if ((*locals)[i] == name)
            return;
    for (int i = 0; i < vars->size(); i++)
        if ((*vars)[i] == name)
            return;
    vars->push_back(name);
}

///////////////////
/////  Lexer  /////
///////////////////

class Lexer {

    private:

    std::string text;
    int pos, prevpos;

    public:

    bool compatMode;

    Lexer(std::string text, bool compatMode) {
        this->text = text;
        this->compatMode = compatMode;
        pos = 0;
        prevpos = 0;
    }
    
    void reset() {
        pos = 0;
        prevpos = 0;
    }

    int lpos() {
        return prevpos;
    }
    
    int cpos() {
        return pos;
    }
    
    std::string substring(int start, int end) {
        return text.substr(start, end - start);
    }

    bool isIdentifierStartChar(char c) {
        return !isspace(c) && c != '+' && c != '-' && c != '\1' && c != '\0'
                && c != '^' && c != '(' && c != ')' && c != '<'
                && c != '>' && c != '=' && c != ':'
                && c != '.' && c != ',' && (c < '0' || c > '9') && c != 24
                && (compatMode
                        || c != '*' && c != '/' && c != '[' && c != ']' && c != '!');
    }
    
    bool isIdentifierContinuationChar(char c) {
        return c >= '0' && c <= '9' || c == '.' || c == ','
                || isIdentifierStartChar(c);
    }
    
    bool isIdentifier(const std::string &s) {
        if (s.length() == 0)
            return false;
        if (!isIdentifierStartChar(s[0]))
            return false;
        for (int i = 1; i < s.length(); i++)
            if (!isIdentifierContinuationChar(s[i]))
                return false;
        return true;
    }

    bool nextToken(std::string *tok, int *tpos) {
        prevpos = pos;
        while (pos < text.length() && text[pos] == ' ')
            pos++;
        if (pos == text.length()) {
            *tok = "";
            *tpos = pos;
            return true;
        }
        int start = pos;
        *tpos = start;
        char c = text[pos++];
        // Identifiers
        if (isIdentifierStartChar(c)) {
            while (isIdentifierContinuationChar(text[pos]))
                pos++;
            *tok = text.substr(start, pos - start);
            return true;
        }
        // Compound symbols
        if (c == '<' || c == '>') {
            if (pos < text.length()) {
                char c2 = text[pos];
                if (c2 == '=' || c == '<' && c2 == '>') {
                    pos++;
                    *tok = text.substr(start, 2);
                    return true;
                }
            }
            *tok = text.substr(start, 1);
            return true;
        }
        if (!compatMode && c == '!') {
            if (pos < text.length() && text[pos] == '=') {
                pos++;
                *tok = std::string("<>");
                return true;
            }
        }
        // One-character symbols
        if (c == '+' || c == '-' || c == '(' || c == ')'
                || c == '^' || c == ':' || c == '='
                || !compatMode && (c == '*' || c == '/' || c == '[' || c == ']')) {
            *tok = text.substr(start, 1);
            return true;
        }
        switch (c) {
            case '\0': *tok = std::string("/"); return true;
            case '\1': *tok = std::string("*"); return true;
            case '\11': *tok = std::string("<="); return true;
            case '\13': *tok = std::string(">="); return true;
            case '\14': *tok = std::string("<>"); return true;
        }
        // What's left at this point is numbers or garbage.
        // Which one we're currently looking at depends on its
        // first character; if that's a digit or a decimal,
        // it's a number; anything else, it's garbage.
        bool multi_dot = false;
        if (c == '.' || c == ',' || c >= '0' && c <= '9') {
            int state = c == '.' || c == ',' ? 1 : 0;
            int d0 = c == '.' || c == ',' ? 0 : 1;
            int d1 = 0, d2 = 0;
            while (pos < text.length()) {
                c = text[pos];
                switch (state) {
                    case 0:
                        if (c == '.' || c == ',')
                            state = 1;
                        else if (c == 'E' || c == 'e' || c == 24)
                            state = 2;
                        else if (c >= '0' && c <= '9')
                            d0++;
                        else
                            goto done;
                        break;
                    case 1:
                        if (c == '.' || c == ',') {
                            multi_dot = true;
                            goto done;
                        }
                        else if (c == 'E' || c == 'e' || c == 24)
                            state = 2;
                        else if (c >= '0' && c <= '9')
                            d1++;
                        else
                            goto done;
                        break;
                    case 2:
                        if (c == '-' || c == '+')
                            state = 3;
                        else if (c >= '0' && c <= '9') {
                            d2++;
                            state = 3;
                        } else
                            goto done;
                        break;
                    case 3:
                        if (c >= '0' && c <= '9')
                            d2++;
                        else
                            goto done;
                        break;
                }
                pos++;
            }
            done:
            // Invalid number scenarios:
            if (d0 == 0 && d1 == 0 // A dot not followed by a digit.
                    || multi_dot // Multiple periods
                    || state == 2  // An 'E' not followed by a valid character.
                    || state == 3 && d2 == 0) { // An 'E' not followed by at least one digit
                *tok = "";
                return false;
            }
            *tok = text.substr(start, pos - start);
            return true;
        } else {
            // Garbage; return just the one character.
            // Parsing will fail at this point so no need to do anything clever.
            *tok = text.substr(start, 1);
            return true;
        }
    }
};

////////////////////
/////  Parser  /////
////////////////////

#define CTX_TOP 0
#define CTX_VALUE 1
#define CTX_BOOLEAN 2
#define CTX_ARRAY 3

/* static */ Evaluator *Parser::parse(std::string expr, bool compatMode, int *errpos) {
    std::string t, t2, eqnName;
    std::vector<std::string> *paramNames = new std::vector<std::string>;
    int tpos;
    
    // Look for equation name
    Lexer *lex = new Lexer(expr, compatMode);
    if (!lex->nextToken(&t, &tpos))
        goto no_name;
    if (!lex->isIdentifier(t))
        goto no_name;
    if (!lex->nextToken(&t2, &tpos))
        goto no_name;
    if (t2 != ":" && t2 != "(")
        goto no_name;
    if (t2 == "(") {
        while (true) {
            if (!lex->nextToken(&t2, &tpos))
                goto no_name;
            if (!lex->isIdentifier(t2))
                goto no_name;
            paramNames->push_back(t2);
            if (!lex->nextToken(&t2, &tpos))
                goto no_name;
            if (t2 == ":")
                continue;
            else if (t2 == ")") {
                if (!lex->nextToken(&t2, &tpos))
                    goto no_name;
                if (t2 == ":")
                    break;
                else
                    goto no_name;
            } else
                goto no_name;
        }
    }

    eqnName = t;
    goto name_done;
    no_name:
    lex->reset();
    delete paramNames;
    paramNames = NULL;
    name_done:
    
    Parser pz(lex);
    Evaluator *ev = pz.parseExpr(CTX_TOP);
    if (ev == NULL) {
        fail:
        *errpos = pz.lex->lpos();
        return NULL;
    }
    if (!pz.nextToken(&t, &tpos)) {
        delete ev;
        goto fail;
    }
    if (t == "") {
        // Text consumed completely; this is the good scenario
        if (eqnName != "")
            ev = new NameTag(0, eqnName, paramNames, ev);
        return ev;
    } else {
        // Trailing garbage
        delete ev;
        *errpos = tpos;
        return NULL;
    }
}

/* static */ void Parser::generateCode(Evaluator *ev, prgm_struct *prgm) {
    GeneratorContext ctx;
    ev->generateCode(&ctx);
    ctx.store(prgm);
}

Parser::Parser(Lexer *lex) : lex(lex), pbpos(-1) {}

Parser::~Parser() {
    delete lex;
}

Evaluator *Parser::parseExpr(int context) {
    int old_context = this->context;
    this->context = context;
    Evaluator *ret = parseExpr2();
    this->context = old_context;
    return ret;
}

Evaluator *Parser::parseExpr2() {
    Evaluator *ev = parseAnd();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "")
            return ev;
        if (t == "OR" || t == "XOR") {
            if (context != CTX_BOOLEAN || !ev->isBool())
                goto fail;
            Evaluator *ev2 = parseAnd();
            if (ev2 == NULL)
                goto fail;
            if (!ev2->isBool()) {
                delete ev2;
                goto fail;
            }
            if (t == "OR")
                ev = new Or(tpos, ev, ev2);
            else // t == "XOR"
                ev = new Xor(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

Evaluator *Parser::parseAnd() {
    Evaluator *ev = parseNot();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "")
            return ev;
        if (t == "AND") {
            if (context != CTX_BOOLEAN || !ev->isBool())
                goto fail;
            Evaluator *ev2 = parseNot();
            if (ev2 == NULL)
                goto fail;
            if (!ev2->isBool()) {
                delete ev2;
                goto fail;
            }
            ev = new And(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

Evaluator *Parser::parseNot() {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    if (t == "NOT") {
        Evaluator *ev = parseComparison();
        if (ev == NULL) {
            return NULL;
        } else if (context != CTX_BOOLEAN || !ev->isBool()) {
            delete ev;
            return NULL;
        } else {
            return new UnaryFunction(tpos, ev, CMD_GEN_NOT);
        }
    } else {
        pushback(t, tpos);
        return parseComparison();
    }
}

Evaluator *Parser::parseComparison() {
    Evaluator *ev = parseNumExpr();
    if (ev == NULL)
        return NULL;
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos)) {
        fail:
        delete ev;
        return NULL;
    }
    if (t == "")
        return ev;
    if (context == CTX_TOP && t == "=") {
        if (ev->isBool())
            goto fail;
        context = CTX_VALUE; // Only one '=' allowed
        Evaluator *ev2 = parseNumExpr();
        if (ev2 == NULL)
            goto fail;
        if (ev2->isBool()) {
            delete ev2;
            goto fail;
        }
        return new Equation(tpos, ev, ev2);
    } else if (t == "=" || t == "<>" || t == "<" || t == "<=" || t == ">" || t == ">=") {
        if (context != CTX_BOOLEAN || ev->isBool())
            goto fail;
        Evaluator *ev2 = parseNumExpr();
        if (ev2 == NULL)
            goto fail;
        if (ev2->isBool()) {
            delete ev2;
            goto fail;
        }
        if (t == "=")
            return new CompareEQ(tpos, ev, ev2);
        else if (t == "<>")
            return new CompareNE(tpos, ev, ev2);
        else if (t == "<")
            return new CompareLT(tpos, ev, ev2);
        else if (t == "<=")
            return new CompareLE(tpos, ev, ev2);
        else if (t == ">")
            return new CompareGT(tpos, ev, ev2);
        else // t == ">="
            return new CompareGE(tpos, ev, ev2);
    } else {
        pushback(t, tpos);
        return ev;
    }
}

Evaluator *Parser::parseNumExpr() {
    Evaluator *ev = parseTerm();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "")
            return ev;
        if (t == "+" || t == "-") {
            if (ev->isBool())
                goto fail;
            Evaluator *ev2 = parseTerm();
            if (ev2 == NULL)
                goto fail;
            if (ev2->isBool()) {
                delete ev2;
                return NULL;
            }
            if (t == "+")
                ev = new Sum(tpos, ev, ev2);
            else
                ev = new Difference(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

Evaluator *Parser::parseTerm() {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    if (t == "-" || t == "+") {
        Evaluator *ev = parseTerm();
        if (ev == NULL)
            return NULL;
        if (ev->isBool()) {
            delete ev;
            return NULL;
        }
        if (t == "+")
            return ev;
        else
            return new Negative(tpos, ev);
    } else {
        pushback(t, tpos);
        Evaluator *ev = parseFactor();
        if (ev == NULL)
            return NULL;
        while (true) {
            if (!nextToken(&t, &tpos))
                goto fail;
            if (t == "")
                return ev;
            if (t == "*" || t == "/") {
                if (ev->isBool()) {
                    fail:
                    delete ev;
                    return NULL;
                }
                Evaluator *ev2 = parseFactor();
                if (ev2 == NULL)
                    goto fail;
                if (ev2->isBool()) {
                    delete ev2;
                    goto fail;
                }
                if (t == "*")
                    ev = new Product(tpos, ev, ev2);
                else
                    ev = new Quotient(tpos, ev, ev2);
            } else {
                pushback(t, tpos);
                return ev;
            }
        }
    }
}

Evaluator *Parser::parseFactor() {
    Evaluator *ev = parseThing();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "^") {
            if (ev->isBool())
                goto fail;
            Evaluator *ev2 = parseThing();
            if (ev2 == NULL)
                goto fail;
            if (ev2->isBool()) {
                delete ev2;
                goto fail;
            }
            ev = new Power(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

#define EXPR_LIST_EXPR 0
#define EXPR_LIST_BOOLEAN 1
#define EXPR_LIST_NAME 2
#define EXPR_LIST_SUBEXPR 3
#define EXPR_LIST_LVALUE 4
#define EXPR_LIST_FOR 5

std::vector<Evaluator *> *Parser::parseExprList(int min_args, int max_args, int mode) {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    pushback(t, tpos);
    std::vector<Evaluator *> *evs = new std::vector<Evaluator *>;
    if (t == ")") {
        if (min_args == 0) {
            return evs;
        } else {
            fail:
            for (int i = 0; i < evs->size(); i++)
                delete (*evs)[i];
            delete evs;
            return NULL;
        }
    } else {
        pushback(t, tpos);
    }

    while (true) {
        Evaluator *ev;
        if (mode == EXPR_LIST_NAME) {
            if (!nextToken(&t, &tpos) || t == "")
                goto fail;
            if (!lex->isIdentifier(t))
                goto fail;
            ev = new Variable(tpos, t);
            mode = EXPR_LIST_EXPR;
        } else {
            bool wantBool = mode == EXPR_LIST_BOOLEAN;
            int startPos = pbpos != -1 ? pbpos : lex->cpos();
            ev = parseExpr(wantBool ? CTX_BOOLEAN : CTX_VALUE);
            if (ev == NULL)
                goto fail;
            if (wantBool != ev->isBool()) {
                delete ev;
                goto fail;
            }
            if (mode == EXPR_LIST_LVALUE &&
                    ev->name() == "" && !ev->makeLvalue()) {
                delete ev;
                goto fail;
            }
            if (mode == EXPR_LIST_SUBEXPR) {
                int endPos = pbpos != -1 ? pbpos : lex->cpos();
                std::string text = lex->substring(startPos, endPos);
                ev = new Subexpression(startPos, ev, text);
                mode = EXPR_LIST_NAME;
            } else if (mode == EXPR_LIST_FOR) {
                mode = EXPR_LIST_BOOLEAN;
            } else {
                mode = EXPR_LIST_EXPR;
            }
        }
        evs->push_back(ev);
        if (!nextToken(&t, &tpos))
            goto fail;
        if (t == ":") {
            if (evs->size() == max_args)
                goto fail;
        } else {
            pushback(t, tpos);
            if (t == ")" && evs->size() >= min_args)
                return evs;
            else
                goto fail;
        }
    }
}

static bool get_phloat(std::string tok, phloat *d) {
    char c = tok[0];
    if ((c < '0' || c > '9') && c != '.' && c != ',')
        return false;
    for (int i = 0; i < tok.length(); i++)
        if (tok[i] == 'E' || tok[i] == 'e')
            tok[i] = 24;
    return string2phloat(tok.c_str(), tok.length(), d) == 0;
}

Evaluator *Parser::parseThing() {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    if (t == "-" || t == "+") {
        Evaluator *ev = parseThing();
        if (ev == NULL)
            return NULL;
        if (ev->isBool()) {
            delete ev;
            return NULL;
        }
        if (t == "+")
            return ev;
        else
            return new Negative(tpos, ev);
    }
    phloat d;
    if (get_phloat(t, &d)) {
        return new Literal(tpos, d);
    } else if (t == "(") {
        Evaluator *ev = parseExpr(context == CTX_TOP ? CTX_VALUE : context);
        if (ev == NULL)
            return NULL;
        std::string t2;
        int t2pos;
        if (!nextToken(&t2, &t2pos) || t2 != ")") {
            delete ev;
            return NULL;
        }
        return ev;
    } else if (t == "[" && context != CTX_ARRAY) {
        // Array literal
        int apos = tpos;
        if (!nextToken(&t, &tpos))
            return NULL;
        bool one_d = t != "[";
        if (one_d)
            pushback(t, tpos);
        int width = 0;
        std::vector<std::vector<Evaluator *> > data;
        std::vector<Evaluator *> row;
        forStack.push_back(new For(-1));
        while (true) {
            if (!nextToken(&t, &tpos))
                goto array_fail;
            if (t == "]") {
                end_row:
                int w = row.size();
                if (w == 0)
                    goto array_fail;
                if (width < w)
                    width = w;
                data.push_back(row);
                row.clear();
                if (one_d)
                    goto array_success;
                if (!nextToken(&t, &tpos))
                    goto array_fail;
                if (t == "]")
                    goto array_success;
                if (t != ":")
                    goto array_fail;
                if (!nextToken(&t, &tpos))
                    goto array_fail;
                if (t != "[")
                    goto array_fail;
            } else {
                pushback(t, tpos);
                do_element:
                Evaluator *ev = parseExpr(CTX_ARRAY);
                if (ev == NULL)
                    goto array_fail;
                row.push_back(ev);
                if (!nextToken(&t, &tpos))
                    goto array_fail;
                if (t == "]")
                    goto end_row;
                if (t != ":")
                    goto array_fail;
                goto do_element;
            }
        }
        array_fail:
        forStack.pop_back();
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                delete data[i][j];
        return NULL;
        array_success:
        forStack.pop_back();
        return new Array(apos, data, one_d);
    } else if (lex->isIdentifier(t)) {
        std::string t2;
        int t2pos;
        if (!nextToken(&t2, &t2pos))
            return NULL;
        if (t2 == "(") {
            int min_args, max_args;
            int mode;
            if (t == "SIN" || t == "COS" || t == "TAN"
                    || t == "ASIN" || t == "ACOS" || t == "ATAN"
                    || t == "SINH" || t == "COSH" || t == "TANH"
                    || t == "ASINH" || t == "ACOSH" || t == "ATANH"
                    || t == "DEG" || t == "RAD"
                    || t == "LN" || t == "LNP1" || t == "LOG"
                    || t == "EXP" || t == "EXPM1" || t == "ALOG"
                    || t == "SQRT" || t == "SQ" || t == "INV"
                    || t == "ABS" || t == "FACT" || t == "GAMMA"
                    || t == "INT" || t == "IP" || t == "FP"
                    || t == "HMS" || t == "HRS" || t == "SIZES"
                    || t == "MROWS" || t == "MCOLS"
                    || t == "SGN" || t == "DEC" || t == "OCT"
                    || t == "BNOT" || t == "BNEG"
                    || t == "INVRT" || t == "DET" || t == "TRANS"
                    || t == "UVEC" || t == "FNRM" || t == "RNRM"
                    || t == "RSUM" || t == "REAL?" || t == "CPX?"
                    || t == "MAT?" || t == "LIST?"
                    || t == "FCSTX" || t == "FCSTY") {
                min_args = max_args = 1;
                mode = EXPR_LIST_EXPR;
            } else if (t == "COMB" || t == "PERM"
                    || t == "IDIV" || t == "MOD" || t == "RND"
                    || t == "TRN" || t == "DATE" || t == "BAND"
                    || t == "BOR" || t == "BXOR" || t == "BADD"
                    || t == "BSUB" || t == "BMUL" || t == "BDIV"
                    || t == "HMSADD" || t == "HMSSUB"
                    || t == "NEWMAT" || t == "DOT" || t == "CROSS"
                    || t == "RCOMPLX" || t == "PCOMPLX") {
                min_args = max_args = 2;
                mode = EXPR_LIST_EXPR;
            } else if (t == "ANGLE" || t == "RADIUS" || t == "XCOORD"
                    || t == "YCOORD") {
                min_args = 1;
                max_args = 2;
                mode = EXPR_LIST_EXPR;
            } else if (t == "DDAYS") {
                min_args = max_args = 3;
                mode = EXPR_LIST_EXPR;
            } else if (t == "MIN" || t == "MAX") {
                min_args = 0;
                max_args = INT_MAX;
                mode = EXPR_LIST_EXPR;
            } else if (t == "IF") {
                min_args = max_args = 3;
                mode = EXPR_LIST_BOOLEAN;
            } else if (t == "G" || t == "S") {
                min_args = max_args = 1;
                mode = EXPR_LIST_NAME;
            } else if (t == "L") {
                min_args = max_args = 2;
                mode = EXPR_LIST_LVALUE;
            } else if (t == "ITEM") {
                min_args = 2;
                max_args = 3;
                mode = EXPR_LIST_NAME;
            } else if (t == "FOR") {
                min_args = 4;
                max_args = INT_MAX;
                mode = EXPR_LIST_FOR;
                For *f = new For(tpos);
                forStack.push_back(f);
            } else if (t == "\5") {
                min_args = max_args = 5;
                mode = EXPR_LIST_NAME;
                For *f = new For(-1);
                forStack.push_back(f);
            } else if (t == "\3") {
                min_args = max_args = 4;
                mode = EXPR_LIST_SUBEXPR;
                For *f = new For(-1);
                forStack.push_back(f);
            } else if (t == "XEQ") {
                min_args = 1;
                max_args = INT_MAX;
                mode = EXPR_LIST_NAME;
            } else if (t == "SEQ") {
                min_args = 1;
                max_args = INT_MAX;
                mode = EXPR_LIST_EXPR;
            } else {
                // Call
                min_args = 0;
                max_args = INT_MAX;
                mode = EXPR_LIST_EXPR;
            }
            std::vector<Evaluator *> *evs = parseExprList(min_args, max_args, mode);
            if (t == "\5" || t == "\3")
                forStack.pop_back();
            For *f = NULL;
            if (t == "FOR") {
                f = forStack.back();
                forStack.pop_back();
            }
            if (evs == NULL) {
                delete f;
                return NULL;
            }
            if (!nextToken(&t2, &t2pos) || t2 != ")") {
                for (int i = 0; i < evs->size(); i++)
                    delete (*evs)[i];
                delete evs;
                delete f;
                return NULL;
            }
            if (t == "SIN" || t == "COS" || t == "TAN"
                    || t == "ASIN" || t == "ACOS" || t == "ATAN"
                    || t == "SINH" || t == "COSH" || t == "TANH"
                    || t == "ASINH" || t == "ACOSH" || t == "ATANH"
                    || t == "DEG" || t == "RAD"
                    || t == "LN" || t == "LNP1" || t == "LOG"
                    || t == "EXP" || t == "EXPM1" || t == "ALOG"
                    || t == "SQRT" || t == "SQ" || t == "INV"
                    || t == "ABS" || t == "FACT" || t == "GAMMA"
                    || t == "INT" || t == "IP" || t == "FP"
                    || t == "HMS" || t == "HRS" || t == "SIZES"
                    || t == "MROWS" || t == "MCOLS"
                    || t == "SGN" || t == "DEC" || t == "OCT"
                    || t == "BNOT" || t == "BNEG"
                    || t == "INVRT" || t == "DET" || t == "TRANS"
                    || t == "UVEC" || t == "FNRM" || t == "RNRM"
                    || t == "RSUM" || t == "REAL?" || t == "CPX?"
                    || t == "MAT?" || t == "LIST?"
                    || t == "FCSTX" || t == "FCSTY") {
                Evaluator *ev = (*evs)[0];
                delete evs;
                if (t == "SIN")
                    return new Sin(tpos, ev);
                else if (t == "COS")
                    return new Cos(tpos, ev);
                else if (t == "TAN")
                    return new Tan(tpos, ev);
                else if (t == "ASIN")
                    return new Asin(tpos, ev);
                else if (t == "ACOS")
                    return new Acos(tpos, ev);
                else if (t == "ATAN")
                    return new Atan(tpos, ev);
                else if (t == "SINH")
                    return new Sinh(tpos, ev);
                else if (t == "COSH")
                    return new Cosh(tpos, ev);
                else if (t == "TANH")
                    return new Tanh(tpos, ev);
                else if (t == "ASINH")
                    return new Asinh(tpos, ev);
                else if (t == "ACOSH")
                    return new Acosh(tpos, ev);
                else if (t == "ATANH")
                    return new Atanh(tpos, ev);
                else if (t == "DEG")
                    return new Deg(tpos, ev);
                else if (t == "RAD")
                    return new Rad(tpos, ev);
                else if (t == "LN")
                    return new Ln(tpos, ev);
                else if (t == "LNP1")
                    return new Ln1p(tpos, ev);
                else if (t == "LOG")
                    return new Log(tpos, ev);
                else if (t == "EXP")
                    return new Exp(tpos, ev);
                else if (t == "EXPM1")
                    return new Expm1(tpos, ev);
                else if (t == "ALOG")
                    return new Alog(tpos, ev);
                else if (t == "SQ")
                    return new Sq(tpos, ev);
                else if (t == "SQRT")
                    return new Sqrt(tpos, ev);
                else if (t == "INV")
                    return new Inv(tpos, ev);
                else if (t == "ABS")
                    return new UnaryFunction(tpos, ev, CMD_ABS);
                else if (t == "FACT")
                    return new UnaryFunction(tpos, ev, CMD_FACT);
                else if (t == "GAMMA")
                    return new UnaryFunction(tpos, ev, CMD_GAMMA);
                else if (t == "INT")
                    return new Int(tpos, ev);
                else if (t == "IP")
                    return new UnaryFunction(tpos, ev, CMD_IP);
                else if (t == "FP")
                    return new UnaryFunction(tpos, ev, CMD_FP);
                else if (t == "HMS")
                    return new Hms(tpos, ev);
                else if (t == "HRS")
                    return new Hrs(tpos, ev);
                else if (t == "SIZES")
                    return new Size(tpos, ev, 'S');
                else if (t == "MROWS")
                    return new Size(tpos, ev, 'R');
                else if (t == "MCOLS")
                    return new Size(tpos, ev, 'C');
                else if (t == "SGN")
                    return new Sgn(tpos, ev);
                else if (t == "DEC")
                    return new Dec(tpos, ev);
                else if (t == "OCT")
                    return new Oct(tpos, ev);
                else if (t == "BNOT")
                    return new Bnot(tpos, ev);
                else if (t == "BNEG")
                    return new Bneg(tpos, ev);
                else if (t == "INVRT")
                    return new UnaryFunction(tpos, ev, CMD_INVRT);
                else if (t == "DET")
                    return new UnaryFunction(tpos, ev, CMD_DET);
                else if (t == "TRANS")
                    return new UnaryFunction(tpos, ev, CMD_TRANS);
                else if (t == "UVEC")
                    return new UnaryFunction(tpos, ev, CMD_UVEC);
                else if (t == "FNRM")
                    return new UnaryFunction(tpos, ev, CMD_FNRM);
                else if (t == "RNRM")
                    return new UnaryFunction(tpos, ev, CMD_RNRM);
                else if (t == "RSUM")
                    return new UnaryFunction(tpos, ev, CMD_RSUM);
                else if (t == "REAL?")
                    return new TypeTest(tpos, ev, CMD_REAL_T);
                else if (t == "CPX?")
                    return new TypeTest(tpos, ev, CMD_CPX_T);
                else if (t == "MAT?")
                    return new TypeTest(tpos, ev, CMD_MAT_T);
                else if (t == "LIST?")
                    return new TypeTest(tpos, ev, CMD_LIST_T);
                else if (t == "FCSTX")
                    return new UnaryFunction(tpos, ev, CMD_FCSTX);
                else if (t == "FCSTY")
                    return new UnaryFunction(tpos, ev, CMD_FCSTY);
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "COMB" || t == "PERM"
                    || t == "IDIV" || t == "MOD" || t == "RND"
                    || t == "TRN" || t == "DATE" || t == "BAND"
                    || t == "BOR" || t == "BXOR" || t == "BADD"
                    || t == "BSUB" || t == "BMUL" || t == "BDIV"
                    || t == "HMSADD" || t == "HMSSUB"
                    || t == "NEWMAT" || t == "DOT" || t == "CROSS"
                    || t == "RCOMPLX" || t == "PCOMPLX") {
                Evaluator *left = (*evs)[0];
                Evaluator *right = (*evs)[1];
                delete evs;
                if (t == "COMB")
                    return new Comb(tpos, left, right);
                else if (t == "PERM")
                    return new Perm(tpos, left, right);
                else if (t == "IDIV")
                    return new Idiv(tpos, left, right);
                else if (t == "MOD")
                    return new Mod(tpos, left, right);
                else if (t == "RND")
                    return new Rnd(tpos, left, right, false);
                else if (t == "TRN")
                    return new Rnd(tpos, left, right, true);
                else if (t == "DATE")
                    return new Date(tpos, left, right);
                else if (t == "BAND")
                    return new Band(tpos, left, right);
                else if (t == "BOR")
                    return new Bor(tpos, left, right);
                else if (t == "BXOR")
                    return new Bxor(tpos, left, right);
                else if (t == "BADD")
                    return new Badd(tpos, left, right);
                else if (t == "BSUB")
                    return new Bsub(tpos, left, right);
                else if (t == "BMUL")
                    return new Bmul(tpos, left, right);
                else if (t == "BDIV")
                    return new Bdiv(tpos, left, right);
                else if (t == "HMSADD")
                    return new Hmsadd(tpos, left, right);
                else if (t == "HMSSUB")
                    return new Hmssub(tpos, left, right);
                else if (t == "NEWMAT")
                    return new Newmat(tpos, left, right);
                else if (t == "DOT")
                    return new Dot(tpos, left, right);
                else if (t == "CROSS")
                    return new Cross(tpos, left, right);
                else if (t == "RCOMPLX")
                    return new Rcomplx(tpos, left, right);
                else if (t == "PCOMPLX")
                    return new Pcomplx(tpos, left, right);
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "ANGLE" || t == "RADIUS" || t == "XCOORD"
                    || t == "YCOORD") {
                Evaluator *left = (*evs)[0];
                Evaluator *right = evs->size() > 1 ? (*evs)[1] : NULL;
                delete evs;
                if (t == "ANGLE")
                    return new Angle(tpos, left, right);
                else if (t == "RADIUS")
                    return new Radius(tpos, left, right);
                else if (t == "XCOORD")
                    return new Xcoord(tpos, left, right);
                else if (t == "YCOORD")
                    return new Ycoord(tpos, left, right);
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "DDAYS") {
                Evaluator *date1 = (*evs)[0];
                Evaluator *date2 = (*evs)[1];
                Evaluator *cal = (*evs)[2];
                delete evs;
                return new Ddays(tpos, date1, date2, cal);
            } else if (t == "MAX" || t == "MIN") {
                if (t == "MAX")
                    return new Max(tpos, evs);
                else // t == "MIN"
                    return new Min(tpos, evs);
            } else if (t == "XEQ") {
                Evaluator *name = (*evs)[0];
                std::string n = name->name();
                evs->erase(evs->begin());
                return new Xeq(tpos, n, evs);
            } else if (t == "SEQ") {
                return new Seq(tpos, evs);
            } else if (t == "IF") {
                Evaluator *condition = (*evs)[0];
                Evaluator *trueEv = (*evs)[1];
                Evaluator *falseEv = (*evs)[2];
                delete evs;
                return new If(tpos, condition, trueEv, falseEv);
            } else if (t == "G") {
                Evaluator *name = (*evs)[0];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Gee(tpos, n, lex->compatMode);
            } else if (t == "S") {
                Evaluator *name = (*evs)[0];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Ess(tpos, n);
            } else if (t == "L") {
                Evaluator *left = (*evs)[0];
                Evaluator *right = (*evs)[1];
                delete evs;
                std::string n = left->name();
                if (n != "") {
                    delete left;
                    return new Ell(tpos, n, right, lex->compatMode);
                } else {
                    return new Ell(tpos, left, right, lex->compatMode);
                }
            } else if (t == "ITEM") {
                Evaluator *name = (*evs)[0];
                Evaluator *ev1 = (*evs)[1];
                Evaluator *ev2 = evs->size() == 3 ? (*evs)[2] : NULL;
                delete evs;
                std::string n = name->name();
                delete name;
                return new Item(tpos, n, ev1, ev2);
            } else if (t == "FOR") {
                Evaluator *init = (*evs)[0];
                evs->erase(evs->begin());
                Evaluator *cond = (*evs)[0];
                evs->erase(evs->begin());
                Evaluator *next = (*evs)[0];
                evs->erase(evs->begin());
                f->finish_init(init, cond, next, evs);
                return f;
            } else if (t == "\5") {
                Evaluator *name = (*evs)[0];
                Evaluator *from = (*evs)[1];
                Evaluator *to = (*evs)[2];
                Evaluator *step = (*evs)[3];
                Evaluator *ev = (*evs)[4];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Sigma(tpos, n, from, to, step, ev);
            } else if (t == "\3") {
                Evaluator *expr = (*evs)[0];
                Evaluator *name = (*evs)[1];
                std::string integ_var = name->name();
                delete name;
                Evaluator *llim = (*evs)[2];
                Evaluator *ulim = (*evs)[3];
                delete evs;
                return new Integ(tpos, expr, integ_var, llim, ulim);
            } else
                return new Call(tpos, t, evs);
        } else if (!lex->compatMode && t2 == "[") {
            Evaluator *ev1 = parseExpr(CTX_VALUE);
            Evaluator *ev2 = NULL;
            if (ev1 == NULL)
                return NULL;
            if (!nextToken(&t2, &t2pos)) {
                item_fail:
                delete ev1;
                return NULL;
            }
            if (t2 == ":") {
                if (t == "STACK")
                    goto item_fail;
                ev2 = parseExpr(CTX_VALUE);
                if (ev2 == NULL)
                    goto item_fail;
                if (!nextToken(&t2, &t2pos)) {
                    item2_fail:
                    delete ev1;
                    delete ev2;
                    return NULL;
                }
            }
            if (t2 != "]")
                goto item2_fail;
            if (t == "STACK")
                return new Register(tpos, ev1);
            else
                return new Item(tpos, t, ev1, ev2);
        } else {
            pushback(t2, t2pos);
            if (t == "PI" || lex->compatMode && t == "\7")
                return new RecallFunction(tpos, CMD_PI);
            else if (t == "RAN#")
                return new RecallFunction(tpos, CMD_RAN);
            else if (t == "CDATE")
                return new RecallFunction(tpos, CMD_DATE);
            else if (t == "CTIME")
                return new RecallFunction(tpos, CMD_TIME);
            else if (t == "NEWLIST")
                return new RecallFunction(tpos, CMD_NEWLIST);
            else if (t == "REGX")
                return new Register(tpos, 1);
            else if (t == "REGY")
                return new Register(tpos, 2);
            else if (t == "REGZ")
                return new Register(tpos, 3);
            else if (t == "REGT")
                return new Register(tpos, 4);
            else if (t == "\5X")
                return new StatSum(tpos, 0);
            else if (t == "\5X2")
                return new StatSum(tpos, 1);
            else if (t == "\5Y")
                return new StatSum(tpos, 2);
            else if (t == "\5Y2")
                return new StatSum(tpos, 3);
            else if (t == "\5XY")
                return new StatSum(tpos, 4);
            else if (t == "\5N")
                return new StatSum(tpos, 5);
            else if (t == "\5LNX")
                return new StatSum(tpos, 6);
            else if (t == "\5LNX2")
                return new StatSum(tpos, 7);
            else if (t == "\5LNY")
                return new StatSum(tpos, 8);
            else if (t == "\5LNY2")
                return new StatSum(tpos, 9);
            else if (t == "\5LNXLNY")
                return new StatSum(tpos, 10);
            else if (t == "\5XLNY")
                return new StatSum(tpos, 11);
            else if (t == "\5YLNX")
                return new StatSum(tpos, 12);
            else if (t == "WMEAN")
                return new RecallFunction(tpos, CMD_WMEAN);
            else if (t == "CORR")
                return new RecallFunction(tpos, CMD_CORR);
            else if (t == "SLOPE")
                return new RecallFunction(tpos, CMD_SLOPE);
            else if (t == "YINT")
                return new RecallFunction(tpos, CMD_YINT);
            else if (t == "MEANX")
                return new RecallOneOfTwoFunction(tpos, CMD_MEAN, true);
            else if (t == "MEANY")
                return new RecallOneOfTwoFunction(tpos, CMD_MEAN, false);
            else if (t == "SDEVX")
                return new RecallOneOfTwoFunction(tpos, CMD_SDEV, true);
            else if (t == "SDEVY")
                return new RecallOneOfTwoFunction(tpos, CMD_SDEV, false);
            else if (t == "BREAK" || t == "CONTINUE") {
                if (forStack.size() == 0)
                    return NULL;
                For *f = forStack.back();
                if (f->pos() == -1)
                    return NULL;
                else if (t == "BREAK")
                    return new Break(tpos, f);
                else
                    return new Continue(tpos, f);
            } else
                return new Variable(tpos, t);
        }
    } else {
        return NULL;
    }
}

bool Parser::nextToken(std::string *tok, int *tpos) {
    if (pbpos != -1) {
        *tok = pb;
        *tpos = pbpos;
        pbpos = -1;
        return true;
    } else
        return lex->nextToken(tok, tpos);
}

void Parser::pushback(std::string o, int p) {
    pb = o;
    pbpos = p;
}

void get_varmenu_row_for_eqn(vartype *eqn, int *rows, int *row, char ktext[6][7], int klen[6]) {
    Evaluator *ev = prgms[((vartype_equation *) eqn)->data.index()].eq_data->ev;
    std::vector<std::string> vars;
    std::vector<std::string> locals;
    ev->collectVariables(&vars, &locals);
    *rows = ((int) vars.size() + 5) / 6;
    if (*rows == 0)
        return;
    if (*row >= *rows)
        *row = *rows - 1;
    for (int i = 0; i < 6; i++) {
        int r = 6 * *row + i;
        if (r < vars.size()) {
            std::string t = vars[r];
            int len = (int) t.length();
            if (len > 7)
                len = 7;
            memcpy(ktext[i], t.c_str(), len);
            klen[i] = len;
        } else
            klen[i] = 0;
    }
}

int isolate(vartype *eqn, const char *name, int length) {
    if (eqn == NULL || eqn->type != TYPE_EQUATION)
        return -1;
    vartype_equation *eq = (vartype_equation *) eqn;
    equation_data *eqd = prgms[eq->data.index()].eq_data;
    Evaluator *ev = eqd->ev;
    std::string n(name, length);
    if (ev->howMany(&n) != 1)
        return -1;
    ev = ev->clone(NULL)->removeName();
    Evaluator *lhs, *rhs;
    ev->getSides(&n, &lhs, &rhs);

    while (!lhs->is(&n)) {
        Evaluator *left = lhs;
        if (left->invert(&n, &lhs, &rhs)) {
            left->detach();
            delete left;
        } else {
            // Shouldn't happen
            delete lhs;
            delete rhs;
            return -1;
        }
    }
    delete lhs;

    int4 neq = new_eqn_idx(-1);
    if (neq == -1) {
        delete rhs;
        return -1;
    }
    equation_data *neqd = new equation_data;
    if (neqd == NULL) {
        delete rhs;
        return -1;
    }
    prgms[neq + prgms_count].eq_data = neqd;
    neqd->compatMode = eqd->compatMode;
    neqd->eqn_index = neq;
    GeneratorContext ctx;
    rhs->generateCode(&ctx);
    delete rhs;
    // We have to manually bump the refcount, because otherwise, in ctx.store(),
    // it would end up getting increased to 1 and then decreased to 0, and the
    // object would be deleted. Of course I could also return a pgm_index object,
    // that would be cleaner...
    neqd->refcount++;
    ctx.store(prgms + neq + prgms_count);
    neqd->refcount--;
    return neq;
}

bool has_parameters(equation_data *eqdata) {
    std::vector<std::string> names, locals;
    eqdata->ev->collectVariables(&names, &locals);
    return names.size() > 0;
}

std::vector<std::string> get_parameters(equation_data *eqdata) {
    std::vector<std::string> names, locals;
    eqdata->ev->collectVariables(&names, &locals);
    return names;
}

std::vector<std::string> get_mvars(const char *name, int namelen) {
    std::vector<std::string> names;
    if (namelen > 7)
        // Too long
        return names;
    arg_struct arg;
    arg.type = ARGTYPE_STR;
    int len;
    string_copy(arg.val.text, &len, name, namelen);
    arg.length = len;
    pgm_index prgm;
    int4 pc;
    if (!find_global_label(&arg, &prgm, &pc))
        return names;
    pgm_index saved_prgm = current_prgm;
    current_prgm = prgm;
    int cmd;
    pc += get_command_length(current_prgm, pc);
    while (get_next_command(&pc, &cmd, &arg, 0, NULL), cmd == CMD_MVAR)
        names.push_back(std::string(arg.val.text, arg.length));
    current_prgm = saved_prgm;
    return names;
}
