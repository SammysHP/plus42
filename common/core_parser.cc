#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <sstream>

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
    std::string *s;
    int n;
    phloat d;
};

class GeneratorContext {
    private:

    std::vector<Line *> *lines;
    std::vector<std::vector<Line *> *> stack;
    std::vector<std::vector<Line *> *> queue;
    int lbl;

    public:

    GeneratorContext() {
        lines = new std::vector<Line *>;
        lbl = 0;
        // FUNC 11: 1 input, 1 output; the input being the equation itself
        addLine(CMD_FUNC, 11);
        addLine(CMD_LNSTK);
    }

    ~GeneratorContext() {
        for (int i = 0; i < lines->size(); i++)
            delete (*lines)[i];
        delete lines;
    }

    void addLine(int cmd) {
        Line *line = new Line;
        line->cmd = cmd;
        lines->push_back(line);
    }

    void addLine(int cmd, const std::string &arg) {
        Line *line = new Line;
        line->cmd = cmd;
        line->s = new std::string(arg);
        lines->push_back(line);
    }

    void addLine(int cmd, int lbl) {
        Line *line = new Line;
        line->cmd = cmd;
        line->n = lbl;
        lines->push_back(line);
    }

    void addLine(int cmd, phloat d) {
        Line *line = new Line;
        line->cmd = cmd;
        line->d = d;
        lines->push_back(line);
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
                label2line[line->n] = lineno;
            else
                lineno++;
        }
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_GTOL || line->cmd == CMD_XEQL)
                line->n = label2line[line->n];
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
        // First, the end. Doing this before anything else prevents the program count from being bumped.
        arg_struct arg;
        arg.type = ARGTYPE_NONE;
        store_command(0, CMD_END, &arg, NULL);
        // Then, the rest...
        int4 pc = -1;
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_STO
                    || line->cmd == CMD_RCL
                    || line->cmd == CMD_LSTO
                    || line->cmd == CMD_STO_ADD
                    || line->cmd == CMD_GSTO
                    || line->cmd == CMD_GRCL
                    || line->cmd == CMD_XEQ) {
                arg.type = ARGTYPE_STR;
                arg.length = line->s->size();
                if (arg.length > 7)
                    arg.length = 7;
                memcpy(arg.val.text, line->s->c_str(), arg.length);
            } else if (line->cmd == CMD_XSTR) {
                arg.type = ARGTYPE_XSTR;
                int len = (int) line->s->length();
                if (len > 65535)
                    len = 65535;
                arg.length = len;
                arg.val.xstr = line->s->c_str();
            } else if (line->cmd == CMD_NUMBER) {
                arg.type = ARGTYPE_DOUBLE;
                arg.val_d = line->d;
            } else if (line->cmd == CMD_LBL) {
                continue;
            } else if (line->cmd == CMD_FIX
                    || line->cmd == CMD_SCI
                    || line->cmd == CMD_PICK) {
                arg.type = ARGTYPE_IND_STK;
                arg.val.stk = 'X';
            } else if (line->cmd == CMD_GTOL
                    || line->cmd == CMD_XEQL
                    || line->cmd == CMD_FUNC
                    || line->cmd == CMD_RDNN
                    || line->cmd == CMD_DROPN) {
                arg.type = ARGTYPE_NUM;
                arg.val.num = line->n;
            } else if (line->cmd == CMD_RCL_ADD
                    || line->cmd == CMD_0_LT_NN
                    || line->cmd == CMD_X_EQ_NN
                    || line->cmd == CMD_X_GT_NN) {
                arg.type = ARGTYPE_STK;
                arg.val.stk = (char) line->n;
            } else {
                arg.type = ARGTYPE_NONE;
            }
            store_command_after(&pc, line->cmd, &arg, NULL);
        }
        current_prgm = saved_prgm;
        flags.f.prgm_mode = saved_prgm_mode;
        flags.f.printer_exists = prev_printer_exists;
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


/////////////////
/////  Abs  /////
/////////////////

class Abs : public UnaryEvaluator {

    public:

    Abs(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Abs(tpos, ev->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ABS);
    }
};

//////////////////
/////  Acos  /////
//////////////////

class Acos : public UnaryEvaluator {

    public:

    Acos(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone() {
        return new Acos(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Acosh(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Alog(tpos, ev->clone());
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

    Evaluator *clone() {
        return new And(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Angle(tpos, left->clone(), right->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_TO_POL);
        ctx->addLine(CMD_DROP);
    }
};

//////////////////
/////  Asin  /////
//////////////////

class Asin : public UnaryEvaluator {

    public:

    Asin(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone() {
        return new Asin(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Asinh(tpos, ev->clone());
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ASINH);
    }
};

//////////////////
/////  Atan  /////
//////////////////

class Atan : public UnaryEvaluator {

    public:

    Atan(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone() {
        return new Atan(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Atanh(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Badd(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Band(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Bdiv(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Bmul(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Bneg(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Bnot(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Bor(tpos, left->clone(), right->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_OR);
    }
};

//////////////////
/////  Bsub  /////
//////////////////

class Bsub : public BinaryEvaluator {

    public:

    Bsub(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Bsub(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone() {
        return new Bsub(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Bxor(tpos, left->clone(), right->clone());
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

    void detach() {
        delete evs;
    }

    Evaluator *clone() {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone());
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
        ctx->addLine(CMD_LASTX);
        ctx->addLine(CMD_EVAL);
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

///////////////////
/////  Cdate  /////
///////////////////

class Cdate : public Evaluator {

    public:

    Cdate(int pos) : Evaluator(pos) {}

    Evaluator *clone() {
        return new Cdate(tpos);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_DATE);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

//////////////////
/////  Comb  /////
//////////////////

class Comb : public BinaryEvaluator {

    public:

    Comb(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone() {
        return new Comb(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new CompareEQ(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new CompareNE(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new CompareLT(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new CompareLE(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new CompareGT(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new CompareGE(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_GE);
    }
};

/////////////////
/////  Cos  /////
/////////////////

class Cos : public UnaryEvaluator {

    public:

    Cos(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone() {
        return new Cos(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Cosh(tpos, ev->clone());
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_COSH);
    }
};

///////////////////
/////  Ctime  /////
///////////////////

class Ctime : public Evaluator {

    public:

    Ctime(int pos) : Evaluator(pos) {}

    Evaluator *clone() {
        return new Ctime(tpos);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_TIME);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

//////////////////
/////  Date  /////
//////////////////

class Date : public BinaryEvaluator {

    public:

    Date(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Date(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone() {
        return new Date(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Ddays(tpos, date1->clone(), date2->clone(), cal->clone());
    }

    ~Ddays() {
        delete date1;
        delete date2;
        delete cal;
    }

    void detach() {
        date1 = NULL;
        date2 = NULL;
        cal = NULL;
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

    Evaluator *clone() {
        return new Dec(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Deg(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Difference(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        if (name != "")
            return new Ell(tpos, name, right->clone(), compatMode);
        else
            return new Ell(tpos, left->clone(), right->clone(), compatMode);
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

    Evaluator *clone() {
        return new Equation(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
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

    Evaluator *clone() {
        return new Exp(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Expm1(tpos, ev->clone());
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_E_POW_X_1);
    }
};

//////////////////
/////  Fact  /////
//////////////////

class Fact : public UnaryEvaluator {

    public:

    Fact(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Fact(tpos, ev->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_FACT);
    }
};

////////////////
/////  Fp  /////
////////////////

class Fp : public UnaryEvaluator {

    public:

    Fp(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Fp(tpos, ev->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_FP);
    }
};

///////////////////
/////  Gamma  /////
///////////////////

class Gamma : public UnaryEvaluator {

    public:

    Gamma(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Gamma(tpos, ev->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_GAMMA);
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

    Evaluator *clone() {
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

    Evaluator *clone() {
        return new Hms(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Hmsadd(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Hmssub(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Hrs(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Idiv(tpos, left->clone(), right->clone());
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

    void detach() {
        condition = NULL;
        trueEv = NULL;
        falseEv = NULL;
    }

    Evaluator *clone() {
        return new If(tpos, condition->clone(), trueEv->clone(), falseEv->clone());
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

    Evaluator *clone() {
        return new Int(tpos, ev->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_IP);
        ctx->addLine(CMD_X_EQ_NN, 'L');
        int lbl = ctx->nextLabel();
        ctx->addLine(CMD_GTOL, lbl);
        ctx->addLine(CMD_0_LT_NN, 'L');
        ctx->addLine(CMD_GTOL, lbl);
        ctx->addLine(CMD_NUMBER, (phloat) 1);
        ctx->addLine(CMD_SUB);
        ctx->addLine(CMD_LBL, lbl);
    }
};

////////////////
/////  Ip  /////
////////////////

class Ip : public UnaryEvaluator {

    public:

    Ip(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Ip(tpos, ev->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_IP);
    }
};

/////////////////
/////  Inv  /////
/////////////////

class Inv : public UnaryEvaluator {

    public:

    Inv(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone() {
        return new Inv(tpos, ev->clone());
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

    Evaluator *ev1, *ev2;
    std::string name;
    bool lvalue;

    public:

    Item(int pos, std::string name, Evaluator *ev1, Evaluator *ev2) : Evaluator(pos), name(name), ev1(ev1), ev2(ev2), lvalue(false) {}

    Evaluator *clone() {
        Evaluator *ret = new Item(tpos, name, ev1->clone(), ev2 == NULL ? NULL : ev2->clone());
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

    Evaluator *clone() {
        return new Literal(tpos, value);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_NUMBER, value);
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

    Evaluator *clone() {
        return new Ln(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Ln1p(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Log(tpos, ev->clone());
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
    
    void detach() {
        delete evs;
    }

    Evaluator *clone() {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone());
        return new Max(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        if (evs->size() == 0) {
            ctx->addLine(CMD_NUMBER, (phloat) -1);
            ctx->addLine(CMD_NUMBER, (phloat) 0);
            ctx->addLine(CMD_DIV);
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

    void detach() {
        delete evs;
    }

    Evaluator *clone() {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone());
        return new Min(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        if (evs->size() == 0) {
            ctx->addLine(CMD_NUMBER, (phloat) 1);
            ctx->addLine(CMD_NUMBER, (phloat) 0);
            ctx->addLine(CMD_DIV);
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

    Evaluator *clone() {
        return new Mod(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new NameTag(tpos, name, new std::vector<std::string>(*params), ev->clone());
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

    Evaluator *clone() {
        return new Negative(tpos, ev->clone());
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_CHS);
    }
};

////////////////////
/////  Newstr  /////
////////////////////

class Newstr : public Evaluator {

    public:

    Newstr(int pos) : Evaluator(pos) {}

    Evaluator *clone() {
        return new Newstr(tpos);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_NEWSTR);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

/////////////////////
/////  Newlist  /////
/////////////////////

class Newlist : public Evaluator {

    public:

    Newlist(int pos) : Evaluator(pos) {}

    Evaluator *clone() {
        return new Newlist(tpos);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_NEWLIST);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

/////////////////
/////  Not  /////
/////////////////

class Not : public UnaryEvaluator {

    public:

    Not(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Not(tpos, ev->clone());
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_GEN_NOT);
    }
};

/////////////////
/////  Oct  /////
/////////////////

class Oct : public UnaryEvaluator {

    public:

    Oct(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone() {
        return new Oct(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Or(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_OR);
    }
};

//////////////////
/////  Perm  /////
//////////////////

class Perm : public BinaryEvaluator {

    public:

    Perm(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone() {
        return new Perm(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_PERM);
    }
};

////////////////
/////  Pi  /////
////////////////

class Pi : public Evaluator {

    public:

    Pi(int pos) : Evaluator(pos) {}

    Evaluator *clone() {
        return new Pi(tpos);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_PI);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        return 0;
    }
};

///////////////////
/////  Power  /////
///////////////////

class Power : public BinaryEvaluator {

    public:

    Power(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Power(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone() {
        return new Power(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Product(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Quotient(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Rad(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Radius(tpos, left->clone(), right->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_TO_POL);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_DROP);
    }
};

////////////////////
/////  Random  /////
////////////////////

class Random : public Evaluator {

    public:

    Random(int pos) : Evaluator(pos) {}

    Evaluator *clone() {
        return new Random(tpos);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RAN);
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
    
    Evaluator *clone() {
        if (ev == NULL)
            return new Register(tpos, index);
        else
            return new Register(tpos, ev);
    }
    
    void generateCode(GeneratorContext *ctx) {
        if (ev == NULL)
            ctx->addLine(CMD_NUMBER, (phloat) index);
        else
            ev->generateCode(ctx);
        // TODO: Range check?
        ctx->addLine(CMD_FDEPTH);
        ctx->addLine(CMD_ADD);
        ctx->addLine(CMD_PICK);
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

    Evaluator *clone() {
        return new Rnd(tpos, left->clone(), right->clone(), trunc);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCLFLAG);
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_X_LT_0);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(CMD_GTOL, lbl1);
        ctx->addLine(CMD_FIX);
        ctx->addLine(CMD_GTOL, lbl2);
        ctx->addLine(CMD_LBL, lbl1);
        ctx->addLine(CMD_NUMBER, (phloat) -1);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_SUB);
        ctx->addLine(CMD_SCI);
        ctx->addLine(CMD_LBL, lbl2);
        ctx->addLine(CMD_DROP);
        ctx->addLine(trunc ? CMD_TRUNC : CMD_RND);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_NUMBER, (phloat) 36.41);
        ctx->addLine(CMD_STOFLAG);
        ctx->addLine(CMD_DROPN, 2);
    }
};

/////////////////
/////  Sgn  /////
/////////////////

class Sgn : public UnaryEvaluator {

    public:

    Sgn(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Sgn(tpos, ev->clone());
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

    void detach() {
        from = NULL;
        to = NULL;
        step = NULL;
        ev = NULL;
    }

    Evaluator *clone() {
        return new Sigma(tpos, name, from->clone(), to->clone(), step->clone(), ev->clone());
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
        ctx->addLine(CMD_NUMBER, (phloat) 0);
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

    Evaluator *clone() {
        return new Sin(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Sinh(tpos, ev->clone());
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SINH);
    }
};

///////////////////
/////  Sizes  /////
///////////////////

class Sizes : public UnaryEvaluator {

    public:

    Sizes(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone() {
        return new Sizes(tpos, ev->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
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
    }
};

////////////////
/////  Sq  /////
////////////////

class Sq : public UnaryEvaluator {

    public:

    Sq(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone() {
        return new Sq(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Sqrt(tpos, ev->clone());
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SQRT);
    }
};

/////////////////
/////  Sum  /////
/////////////////

class Sum : public BinaryEvaluator {

    public:

    Sum(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone() {
        return new Sum(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Tan(tpos, ev->clone());
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

    Evaluator *clone() {
        return new Tanh(tpos, ev->clone());
    }

    bool invert(const std::string *name, Evaluator **lhs, Evaluator **rhs);

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TANH);
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

    Evaluator *clone() {
        return new Variable(tpos, nam);
    }
    
    std::string name() { return nam; }

    bool is(const std::string *name) { return *name == nam; }
    
    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCL, nam);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < locals->size(); i++)
            if ((*locals)[i] == nam)
                return;
        for (int i = 0; i < vars->size(); i++)
            if ((*vars)[i] == nam)
                return;
        vars->push_back(nam);
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

    Evaluator *clone() {
        return new Xcoord(tpos, left->clone(), right->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_TO_REC);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_DROP);
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

    void detach() {
        delete evs;
    }

    Evaluator *clone() {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone());
        return new Xeq(tpos, name, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        // Wrapping the subroutine call in another subroutine,
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

    Evaluator *clone() {
        return new Xor(tpos, left->clone(), right->clone());
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

    Evaluator *clone() {
        return new Ycoord(tpos, left->clone(), right->clone());
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SWAP);
        ctx->addLine(CMD_TO_REC);
        ctx->addLine(CMD_DROP);
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

void Evaluator::getSides(const std::string *name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = this;
    *rhs = new Literal(0, 0);
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
    
    bool isIdentifierStartChar(char c) {
        return !isspace(c) && c != '+' && c != '-' && c != '\1' && c != '\0'
                && c != '^' && c != '(' && c != ')' && c != '<'
                && c != '>' && c != '=' && c != ':'
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
            return new Not(tpos, ev);
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
#define EXPR_LIST_LVALUE 3

std::vector<Evaluator *> *Parser::parseExprList(int nargs, int mode) {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    pushback(t, tpos);
    std::vector<Evaluator *> *evs = new std::vector<Evaluator *>;
    if (t == ")") {
        if (nargs == 0 || nargs == -1 && mode == EXPR_LIST_EXPR) {
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
        } else {
            bool wantBool = mode == EXPR_LIST_BOOLEAN;
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
        }
        mode = EXPR_LIST_EXPR;
        evs->push_back(ev);
        if (!nextToken(&t, &tpos))
            goto fail;
        if (t == ":") {
            if (evs->size() == nargs)
                goto fail;
        } else {
            pushback(t, tpos);
            bool optional_two = nargs == -2 && evs->size() == 2;
            if (t == ")" && (nargs == -1 || nargs == evs->size() || optional_two))
                return evs;
            else
                goto fail;
            if (optional_two)
                nargs = 3;
        }
    }
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
    double d;
    if (sscanf(t.c_str(), "%lf", &d) == 1) {
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
    } else if (lex->isIdentifier(t)) {
        std::string t2;
        int t2pos;
        if (!nextToken(&t2, &t2pos))
            return NULL;
        if (t2 == "(") {
            int nargs;
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
                    || t == "SGN" || t == "DEC" || t == "OCT"
                    || t == "BNOT" || t == "BNEG") {
                nargs = 1;
                mode = EXPR_LIST_EXPR;
            } else if (t == "ANGLE" || t == "RADIUS" || t == "XCOORD"
                    || t == "YCOORD" || t == "COMB" || t == "PERM"
                    || t == "IDIV" || t == "MOD" || t == "RND"
                    || t == "TRN" || t == "DATE" || t == "BAND"
                    || t == "BOR" || t == "BXOR" || t == "BADD"
                    || t == "BSUB" || t == "BMUL" || t == "BDIV"
                    || t == "HMSADD" || t == "HMSSUB") {
                nargs = 2;
                mode = EXPR_LIST_EXPR;
            } else if (t == "DDAYS") {
                nargs = 3;
                mode = EXPR_LIST_EXPR;
            } else if (t == "MIN" || t == "MAX") {
                nargs = -1;
                mode = EXPR_LIST_EXPR;
            } else if (t == "IF") {
                nargs = 3;
                mode = EXPR_LIST_BOOLEAN;
            } else if (t == "G" || t == "S") {
                nargs = 1;
                mode = EXPR_LIST_NAME;
            } else if (t == "L") {
                nargs = 2;
                mode = EXPR_LIST_LVALUE;
            } else if (t == "ITEM") {
                nargs = -2;
                mode = EXPR_LIST_NAME;
            } else if (t == "\5") {
                nargs = 5;
                mode = EXPR_LIST_NAME;
            } else if (t == "XEQ") {
                nargs = -1;
                mode = EXPR_LIST_NAME;
            } else {
                // Call
                nargs = -1;
                mode = EXPR_LIST_EXPR;
            }
            std::vector<Evaluator *> *evs = parseExprList(nargs, mode);
            if (evs == NULL)
                return NULL;
            if (!nextToken(&t2, &t2pos) || t2 != ")") {
                for (int i = 0; i < evs->size(); i++)
                    delete (*evs)[i];
                delete evs;
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
                    || t == "SGN" || t == "DEC" || t == "OCT"
                    || t == "BNOT" || t == "BNEG") {
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
                    return new Abs(tpos, ev);
                else if (t == "FACT")
                    return new Fact(tpos, ev);
                else if (t == "GAMMA")
                    return new Gamma(tpos, ev);
                else if (t == "INT")
                    return new Int(tpos, ev);
                else if (t == "IP")
                    return new Ip(tpos, ev);
                else if (t == "FP")
                    return new Fp(tpos, ev);
                else if (t == "HMS")
                    return new Hms(tpos, ev);
                else if (t == "HRS")
                    return new Hrs(tpos, ev);
                else if (t == "SIZES")
                    return new Sizes(tpos, ev);
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
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "ANGLE" || t == "RADIUS" || t == "XCOORD"
                    || t == "YCOORD" || t == "COMB" || t == "PERM"
                    || t == "IDIV" || t == "MOD" || t == "RND"
                    || t == "TRN" || t == "DATE" || t == "BAND"
                    || t == "BOR" || t == "BXOR" || t == "BADD"
                    || t == "BSUB" || t == "BMUL" || t == "BDIV"
                    || t == "HMSADD" || t == "HMSSUB") {
                Evaluator *left = (*evs)[0];
                Evaluator *right = (*evs)[1];
                delete evs;
                if (t == "ANGLE")
                    return new Angle(tpos, left, right);
                else if (t == "RADIUS")
                    return new Radius(tpos, left, right);
                else if (t == "XCOORD")
                    return new Xcoord(tpos, left, right);
                else if (t == "YCOORD")
                    return new Ycoord(tpos, left, right);
                else if (t == "COMB")
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
                return new Pi(tpos);
            else if (t == "RAN#")
                return new Random(tpos);
            else if (t == "CDATE")
                return new Cdate(tpos);
            else if (t == "CTIME")
                return new Ctime(tpos);
            else if (t == "NEWSTR")
                return new Newstr(tpos);
            else if (t == "NEWLIST")
                return new Newlist(tpos);
            else if (t == "REGX")
                return new Register(tpos, 1);
            else if (t == "REGY")
                return new Register(tpos, 2);
            else if (t == "REGZ")
                return new Register(tpos, 3);
            else if (t == "REGT")
                return new Register(tpos, 4);
            else
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
    ev = ev->clone()->removeName();
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
