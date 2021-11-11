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

class StringOutputStream : public OutputStream {
    private:

    std::stringstream ss;

    public:

    StringOutputStream() {}
    void write(std::string text) {
        ss << text;
    }
    std::string get() {
        return ss.str();
    }
};

//////////////////////////
/////  OutputStream  /////
//////////////////////////

void OutputStream::write(phloat d) {
    char buf[50];
    sprintf(buf, "%.9g", to_double(d));
    write(buf);
}
void OutputStream::newline() {
    write("\n");
}

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
        int saved_prgm = current_prgm;
        current_prgm = prgm->eq_data->prgm_index;
        prgm->text = NULL;
        prgm->size = 0;
        prgm->capacity = 0;
        // Temporarily turn off PRGM mode. This is because
        // store_command() usually refuses to insert commands
        // in programs above prgms_count, in order to prevent
        // users from editing generated code.
        char saved_prgm_mode = flags.f.prgm_mode;
        flags.f.prgm_mode = false;
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
                    || line->cmd == CMD_SVAR_T) {
                arg.type = ARGTYPE_STR;
                arg.length = line->s->size();
                if (arg.length > 7)
                    arg.length = 7;
                memcpy(arg.val.text, line->s->c_str(), arg.length);
            } else if (line->cmd == CMD_NUMBER) {
                arg.type = ARGTYPE_DOUBLE;
                arg.val_d = line->d;
            } else if (line->cmd == CMD_LBL) {
                continue;
            } else if (line->cmd == CMD_GTOL
                    || line->cmd == CMD_XEQL
                    || line->cmd == CMD_FUNC
                    || line->cmd == CMD_RDNN
                    || line->cmd == CMD_DROPN) {
                arg.type = ARGTYPE_NUM;
                arg.val.num = line->n;
            } else if (line->cmd == CMD_RCL_ADD
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
    }
};

/////////////////
/////  Abs  /////
/////////////////

class Abs : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Abs(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Abs() {
        delete ev;
    }

    Evaluator *clone() {
        return new Abs(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("ABS(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ABS);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name) == 0 ? 0 : -1;
    }
};

//////////////////
/////  Acos  /////
//////////////////

class Acos : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Acos(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Acos() {
        delete ev;
    }

    Evaluator *clone() {
        return new Acos(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("ACOS(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ACOS);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

//////////////////
/////  Alog  /////
////////////./////

class Alog : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Alog(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Alog() {
        delete ev;
    }

    Evaluator *clone() {
        return new Alog(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("ALOG(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_10_POW_X);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

/////////////////
/////  And  /////
/////////////////

class And : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    And(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~And() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new And(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(" AND ");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_AND);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

//////////////////
/////  Asin  /////
//////////////////

class Asin : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Asin(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Asin() {
        delete ev;
    }

    Evaluator *clone() {
        return new Asin(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("ASIN(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ASIN);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

//////////////////
/////  Atan  /////
//////////////////

class Atan : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Atan(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Atan() {
        delete ev;
    }

    Evaluator *clone() {
        return new Atan(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("ATAN(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ATAN);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
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

    Evaluator *clone() {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone());
        return new Call(tpos, name, evs2);
    }

    void printAlg(OutputStream *os) {
        os->write(name);
        os->write("(");
        for (int i = 0; i < evs->size(); i++) {
            if (i != 0)
                os->write(":");
            (*evs)[i]->printAlg(os);
        }
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        // TODO
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // TODO
    }

    int howMany(const std::string *name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

///////////////////////
/////  CompareEQ  /////
///////////////////////

class CompareEQ : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareEQ(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~CompareEQ() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new CompareEQ(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("=");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_EQ);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

///////////////////////
/////  CompareNE  /////
///////////////////////

class CompareNE : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareNE(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~CompareNE() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new CompareNE(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("<>");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_NE);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

///////////////////////
/////  CompareLT  /////
///////////////////////

class CompareLT : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareLT(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~CompareLT() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new CompareLT(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("<");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_LT);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

///////////////////////
/////  CompareLE  /////
///////////////////////

class CompareLE : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareLE(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~CompareLE() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new CompareLE(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("<=");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_LE);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

///////////////////////
/////  CompareGT  /////
///////////////////////

class CompareGT : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareGT(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~CompareGT() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new CompareGT(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(">");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_GT);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

///////////////////////
/////  CompareGE  /////
///////////////////////

class CompareGE : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareGE(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~CompareGE() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new CompareGE(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(">=");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_GE);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

/////////////////
/////  Cos  /////
/////////////////

class Cos : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Cos(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Cos() {
        delete ev;
    }

    Evaluator *clone() {
        return new Cos(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("COS(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_COS);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

////////////////////////
/////  Difference  /////
////////////////////////

class Difference : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Difference(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Difference() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Difference(tpos, left->clone(), right->clone());
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("-");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SUB);
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
        return a + b;
    }
};

/////////////////
/////  Ell  /////
/////////////////

class Ell : public Evaluator {

    private:

    std::string name;
    Evaluator *ev;
    bool compatMode;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Ell(int pos, std::string name, Evaluator *ev, bool compatMode) : Evaluator(pos), name(name), ev(ev), compatMode(compatMode) {}

    ~Ell() {
        delete ev;
    }

    Evaluator *clone() {
        return new Ell(tpos, name, ev->clone(), compatMode);
    }

    void printAlg(OutputStream *os) {
        os->write("L(");
        os->write(name);
        os->write(":");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(compatMode ? CMD_GSTO : CMD_STO, name);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

//////////////////////
/////  Equation  /////
//////////////////////

class Equation : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Equation(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Equation() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Equation(tpos, left->clone(), right->clone());
    }

    bool isEquation() {
        return true;
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("=");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SUB);
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
        return a + b;
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

    void printAlg(OutputStream *os) {
        os->write("S(");
        os->write(name);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_NUMBER, (phloat) 0);
        ctx->addLine(CMD_SVAR_T, name);
        ctx->addLine(CMD_SIGN);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string *name) {
        // TODO: Not sure what to do here
        return 0;
    }
};

/////////////////
/////  Exp  /////
/////////////////

class Exp : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Exp(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Exp() {
        delete ev;
    }

    Evaluator *clone() {
        return new Exp(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("EXP(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_E_POW_X);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
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

    void printAlg(OutputStream *os) {
        os->write("G(");
        os->write(name);
        os->write(")");
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

//////////////////////
/////  Identity  /////
//////////////////////

class Identity : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Identity(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Identity() {
        delete ev;
    }

    Evaluator *clone() {
        return new Identity(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
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

    Evaluator *clone() {
        return new If(tpos, condition->clone(), trueEv->clone(), falseEv->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("IF(");
        condition->printAlg(os);
        os->write(":");
        trueEv->printAlg(os);
        os->write(":");
        falseEv->printAlg(os);
        os->write(")");
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
/////  Inv  /////
/////////////////

class Inv : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Inv(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Inv() {
        delete ev;
    }

    Evaluator *clone() {
        return new Inv(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("INV(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_INV);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

//////////////////
/////  Item  /////
//////////////////

class Item : public Evaluator {

    private:

    std::string name;
    Evaluator *ev;

    public:

    Item(int pos, std::string name, Evaluator *ev) : Evaluator(pos), name(name), ev(ev) {}

    ~Item() {
        delete ev;
    }

    Evaluator *clone() {
        return new Item(tpos, name, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("ITEM(");
        os->write(name);
        os->write(":");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCL, name);
        ev->generateCode(ctx);
        ctx->addLine(CMD_MATITEM);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *nam) {
        if (*nam == name || ev->howMany(nam) != 0)
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

    friend int isolate(vartype *eqn, const char *name, int length);

    Literal(int pos, phloat value) : Evaluator(pos), value(value) {}

    Evaluator *clone() {
        return new Literal(tpos, value);
    }

    void printAlg(OutputStream *os) {
        os->write(to_double(value));
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

class Ln : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Ln(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Ln() {
        delete ev;
    }

    Evaluator *clone() {
        return new Ln(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("LN(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_LN);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

/////////////////
/////  Log  /////
/////////////////

class Log : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Log(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Log() {
        delete ev;
    }

    Evaluator *clone() {
        return new Log(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("LOG(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_LOG);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
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

    Evaluator *clone() {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone());
        return new Max(tpos, evs2);
    }

    void printAlg(OutputStream *os) {
        os->write("MAX(");
        for (int i = 0; i < evs->size(); i++) {
            if (i != 0)
                os->write(":");
            (*evs)[i]->printAlg(os);
        }
        os->write(")");
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

    Evaluator *clone() {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone());
        return new Min(tpos, evs2);
    }

    void printAlg(OutputStream *os) {
        os->write("MIN(");
        for (int i = 0; i < evs->size(); i++) {
            if (i != 0)
                os->write(":");
            (*evs)[i]->printAlg(os);
        }
        os->write(")");
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

/////////////////////
/////  NameTag  /////
/////////////////////

class NameTag : public Evaluator {
    
    private:
    
    std::string name;
    Evaluator *ev;
    
    public:
    
    NameTag(int pos, std::string name, Evaluator *ev) : Evaluator(pos), name(name), ev(ev) {}
    
    ~NameTag() {
        delete ev;
    }

    Evaluator *clone() {
        return new NameTag(tpos, name, ev->clone());
    }
    
    std::string eqnName() {
        return name;
    }

    Evaluator *removeName() {
        Evaluator *ret = ev;
        delete this;
        return ret;
    }

    void printAlg(OutputStream *os) {
        ev->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

//////////////////////
/////  Negative  /////
//////////////////////

class Negative : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Negative(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Negative() {
        delete ev;
    }

    Evaluator *clone() {
        return new Negative(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("-");
        ev->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_CHS);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

/////////////////
/////  Not  /////
/////////////////

class Not : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Not(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Not() {
        delete ev;
    }

    Evaluator *clone() {
        return new Not(tpos, ev->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        os->write(" NOT ");
        ev->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_GEN_NOT);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name) == 0 ? 0 : -1;
    }
};

////////////////
/////  Or  /////
////////////////

class Or : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Or(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Or() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Or(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(" OR ");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_OR);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

//////////////////////
/////  Positive  /////
//////////////////////

class Positive : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Positive(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Positive() {
        delete ev;
    }

    Evaluator *clone() {
        return new Positive(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("+");
        ev->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

///////////////////
/////  Power  /////
///////////////////

class Power : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Power(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Power() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Power(tpos, left->clone(), right->clone());
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("^");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_Y_POW_X);
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
        return a + b;
    }
};

/////////////////////
/////  Product  /////
/////////////////////

class Product : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Product(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Product() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Product(tpos, left->clone(), right->clone());
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("*");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_MUL);
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
        return a + b;
    }
};

//////////////////////
/////  Quotient  /////
//////////////////////

class Quotient : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Quotient(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Quotient() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Quotient(tpos, left->clone(), right->clone());
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("/");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_DIV);
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
        return a + b;
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

    Evaluator *clone() {
        return new Sigma(tpos, name, from->clone(), to->clone(), step->clone(), ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("Sigma(");
        os->write(name);
        os->write(":");
        from->printAlg(os);
        os->write(":");
        to->printAlg(os);
        os->write(":");
        step->printAlg(os);
        os->write(":");
        ev->printAlg(os);
        os->write(")");
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
        locals->pop_back();
        ev->collectVariables(vars, locals);
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

class Sin : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Sin(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Sin() {
        delete ev;
    }

    Evaluator *clone() {
        return new Sin(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("SIN(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SIN);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

////////////////
/////  Sq  /////
////////////////

class Sq : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Sq(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Sq() {
        delete ev;
    }

    Evaluator *clone() {
        return new Sq(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("SQ(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SQUARE);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

//////////////////
/////  Sqrt  /////
//////////////////

class Sqrt : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Sqrt(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Sqrt() {
        delete ev;
    }

    Evaluator *clone() {
        return new Sqrt(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("SQRT(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SQRT);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
    }
};

/////////////////
/////  Sum  /////
/////////////////

class Sum : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Sum(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Sum() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Sum(tpos, left->clone(), right->clone());
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("+");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_ADD);
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
        return a + b;
    }
};

/////////////////
/////  Tan  /////
/////////////////

class Tan : public Evaluator {

    private:

    Evaluator *ev;

    public:

    friend int isolate(vartype *eqn, const char *name, int length);

    Tan(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Tan() {
        delete ev;
    }

    Evaluator *clone() {
        return new Tan(tpos, ev->clone());
    }

    void printAlg(OutputStream *os) {
        os->write("TAN(");
        ev->printAlg(os);
        os->write(")");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TAN);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        return ev->howMany(name);
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

    void printAlg(OutputStream *os) {
        os->write(nam);
    }

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

/////////////////
/////  Xor  /////
/////////////////

class Xor : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Xor(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Xor() {
        delete left;
        delete right;
    }

    Evaluator *clone() {
        return new Xor(tpos, left->clone(), right->clone());
    }

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(" XOR ");
        right->printAlg(os);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_XOR);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string *name) {
        if (left->howMany(name) == 0 && right->howMany(name) == 0)
            return 0;
        else
            return -1;
    }
};

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
    int tpos;
    
    // Look for equation name
    Lexer *lex = new Lexer(expr, compatMode);
    if (!lex->nextToken(&t, &tpos))
        goto no_name;
    if (!lex->isIdentifier(t))
        goto no_name;
    if (!lex->nextToken(&t2, &tpos))
        goto no_name;
    if (t2 != ":")
        goto no_name;
    eqnName = t;
    goto name_done;
    no_name:
    lex->reset();
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
            ev = new NameTag(0, eqnName, ev);
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
            return new Positive(tpos, ev);
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

std::vector<Evaluator *> *Parser::parseExprList(int nargs, int mode) {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    pushback(t, tpos);
    std::vector<Evaluator *> *evs = new std::vector<Evaluator *>;
    if (t == ")") {
        if (nargs == 0 || nargs == -1) {
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
            if (t == ")" && (nargs == -1 || nargs == evs->size()))
                return evs;
            else
                goto fail;
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
            return new Positive(tpos, ev);
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
        return new Identity(tpos, ev);
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
                    || t == "LOG" || t == "EXP" || t == "SQRT"
                    || t == "SQ" || t == "INV" || t == "ABS") {
                nargs = 1;
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
            } else if (t == "L" || t == "ITEM") {
                nargs = 2;
                mode = EXPR_LIST_NAME;
            } else if (t == "\5") {
                nargs = 5;
                mode = EXPR_LIST_NAME;
            } else {
                nargs = -1;
                mode = EXPR_LIST_EXPR;
            }
            std::vector<Evaluator *> *evs = parseExprList(nargs, mode);
            if (evs == NULL)
                return NULL;
            if (!nextToken(&t2, &t2pos) || t2 != ")") {
                fail:
                for (int i = 0; i < evs->size(); i++)
                    delete (*evs)[i];
                delete evs;
                return NULL;
            }
            if (t == "SIN" || t == "COS" || t == "TAN"
                    || t == "ASIN" || t == "ACOS" || t == "ATAN"
                    || t == "LN" || t == "LOG" || t == "EXP"
                    || t == "ALOG" || t == "SQRT" 
                    || t == "SQ" || t == "INV" || t == "ABS") {
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
                else if (t == "LN")
                    return new Ln(tpos, ev);
                else if (t == "LOG")
                    return new Log(tpos, ev);
                else if (t == "EXP")
                    return new Exp(tpos, ev);
                else if (t == "ALOG")
                    return new Alog(tpos, ev);
                else if (t == "SQ")
                    return new Sq(tpos, ev);
                else if (t == "SQRT")
                    return new Sqrt(tpos, ev);
                else if (t == "INV")
                    return new Inv(tpos, ev);
                else // t == "ABS"
                    return new Abs(tpos, ev);
            } else if (t == "MAX" || t == "MIN") {
                if (t == "MAX")
                    return new Max(tpos, evs);
                else // t == "MIN"
                    return new Min(tpos, evs);
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
                Evaluator *name = (*evs)[0];
                Evaluator *ev = (*evs)[1];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Ell(tpos, n, ev, lex->compatMode);
            } else if (t == "ITEM") {
                Evaluator *name = (*evs)[0];
                Evaluator *ev = (*evs)[1];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Item(tpos, n, ev);
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
            Evaluator *ev = parseExpr(CTX_VALUE);
            if (ev == NULL)
                return NULL;
            if (!nextToken(&t2, &t2pos) || t2 != "]") {
                delete ev;
                return NULL;
            }
            return new Item(tpos, t, ev);
        } else {
            pushback(t2, t2pos);
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
    Evaluator *ev = ((vartype_equation *) eqn)->data->ev;
    std::vector<std::string> vars;
    std::vector<std::string> locals;
    ev->collectVariables(&vars, &locals);
    *rows = (vars.size() + 5) / 6;
    if (*rows == 0)
        return;
    if (*row >= *rows)
        *row = *rows - 1;
    for (int i = 0; i < 6; i++) {
        int r = 6 * *row + i;
        if (r < vars.size()) {
            std::string t = vars[r];
            int len = t.length();
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
    Evaluator *ev = eq->data->ev;
    std::string n(name, length);
    if (ev->howMany(&n) != 1)
        return -1;
    ev = ev->clone()->removeName();
    Evaluator *lhs, *rhs;
    {
        Equation *e = dynamic_cast<Equation *>(ev);
        if (e != NULL) {
            if (e->left->howMany(&n) == 1) {
                lhs = e->left;
                rhs = e->right;
            } else {
                lhs = e->right;
                rhs = e->left;
            }
            e->left = NULL;
            e->right = NULL;
            delete e;
        } else {
            lhs = ev;
            rhs = new Literal(0, 0);
        }
    }

    while (!lhs->is(&n)) {
        {
            Acos *e = dynamic_cast<Acos *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Cos(0, rhs);
                continue;
            }
        }
        {
            Alog *e = dynamic_cast<Alog *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Log(0, rhs);
                continue;
            }
        }
        {
            Asin *e = dynamic_cast<Asin *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Sin(0, rhs);
                continue;
            }
        }
        {
            Atan *e = dynamic_cast<Atan *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Tan(0, rhs);
                continue;
            }
        }
        {
            Cos *e = dynamic_cast<Cos *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Acos(0, rhs);
                continue;
            }
        }
        {
            Difference *e = dynamic_cast<Difference *>(lhs);
            if (e != NULL) {
                if (e->left->howMany(&n) == 1) {
                    lhs = e->left;
                    rhs = new Sum(0, rhs, e->right);
                } else {
                    lhs = e->right;
                    rhs = new Difference(0, e->left, rhs);
                }
                continue;
            }
        }
        {
            Ell *e = dynamic_cast<Ell *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Ell(0, e->name, rhs, e->compatMode);
                continue;
            }
        }
        {
            Exp *e = dynamic_cast<Exp *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Ln(0, rhs);
                continue;
            }
        }
        {
            Identity *e = dynamic_cast<Identity *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Identity(0, rhs);
                continue;
            }
        }
        {
            Inv *e = dynamic_cast<Inv *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Inv(0, rhs);
                continue;
            }
        }
        {
            Ln *e = dynamic_cast<Ln *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Exp(0, rhs);
                continue;
            }
        }
        {
            Log *e = dynamic_cast<Log *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Alog(0, rhs);
                continue;
            }
        }
        {
            Negative *e = dynamic_cast<Negative *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Negative(0, rhs);
                continue;
            }
        }
        {
            Positive *e = dynamic_cast<Positive *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Positive(0, rhs);
                continue;
            }
        }
        {
            Power *e = dynamic_cast<Power *>(lhs);
            if (e != NULL) {
                if (e->left->howMany(&n) == 1) {
                    lhs = e->left;
                    rhs = new Power(0, rhs, new Inv(0, e->right));
                } else {
                    lhs = e->right;
                    rhs = new Quotient(0, new Ln(0, rhs), new Ln(0, e->left));
                }
                continue;
            }
        }
        {
            Product *e = dynamic_cast<Product *>(lhs);
            if (e != NULL) {
                if (e->left->howMany(&n) == 1) {
                    lhs = e->left;
                    rhs = new Quotient(0, rhs, e->right);
                } else {
                    lhs = e->right;
                    rhs = new Quotient(0, rhs, e->left);
                }
                continue;
            }
        }
        {
            Quotient *e = dynamic_cast<Quotient *>(lhs);
            if (e != NULL) {
                if (e->left->howMany(&n) == 1) {
                    lhs = e->left;
                    rhs = new Product(0, rhs, e->right);
                } else {
                    lhs = e->right;
                    rhs = new Quotient(0, e->left, rhs);
                }
                continue;
            }
        }
        {
            Sin *e = dynamic_cast<Sin *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Asin(0, rhs);
                continue;
            }
        }
        {
            Sq *e = dynamic_cast<Sq *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Sqrt(0, rhs);
                continue;
            }
        }
        {
            Sqrt *e = dynamic_cast<Sqrt *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Sq(0, rhs);
                continue;
            }
        }
        {
            Sum *e = dynamic_cast<Sum *>(lhs);
            if (e != NULL) {
                if (e->left->howMany(&n) == 1) {
                    lhs = e->left;
                    rhs = new Difference(0, rhs, e->right);
                } else {
                    lhs = e->right;
                    rhs = new Difference(0, rhs, e->left);
                }
                continue;
            }
        }
        {
            Tan *e = dynamic_cast<Tan *>(lhs);
            if (e != NULL) {
                lhs = e->ev;
                rhs = new Atan(0, rhs);
                continue;
            }
        }
        // Shouldn't get here
        delete lhs;
        delete rhs;
        return -1;
    }

    int errpos;
    // Dummy equation, just to reserve a program slot
    vartype_equation *neq = (vartype_equation *) new_equation("A", 1, eq->data->compatMode, &errpos);
    // That's why I'm using free() here, not free_vartype()...
    equation_data *eqd = neq->data;
    free(neq);
    GeneratorContext ctx;
    rhs->generateCode(&ctx);
    ctx.store(prgms + eqd->prgm_index);
    return eqd->prgm_index;
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
