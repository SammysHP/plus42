#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "core_parser.h"
#include "core_tables.h"
#include "core_variables.h"

////////////////////////////////
/////  class declarations  /////
////////////////////////////////

class FileOutputStream : public OutputStream {
    private:

    FILE *file;
    bool autoFlush;

    public:

    FileOutputStream(FILE *file, bool autoFlush = false) : file(file), autoFlush(autoFlush) {}
    ~FileOutputStream();
    void write(std::string text);
};

//////////////////////////
/////  OutputStream  /////
//////////////////////////

void OutputStream::write(double d) {
    char buf[50];
    sprintf(buf, "%.9g", d);
    write(buf);
}
void OutputStream::newline() {
    write("\n");
}

//////////////////////////////
/////  FileOutputStream  /////
//////////////////////////////

FileOutputStream::~FileOutputStream() {
    fclose(file);
}

void FileOutputStream::write(std::string text) {
    const char *s = text.c_str();
    fwrite(s, 1, strlen(s), file);
    if (autoFlush)
        fflush(file);
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

    void store(prgm_struct *prgm) {
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
        current_prgm = prgm->eq->data->prgm_index;
        prgm->text = NULL;
        prgm->size = 0;
        prgm->capacity = 0;
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
                    || line->cmd == CMD_DROPN) {
                arg.type = ARGTYPE_NUM;
                arg.val.num = line->n;
            } else {
                arg.type = ARGTYPE_NONE;
            }
            store_command_after(&pc, line->cmd, &arg, NULL);
        }
        current_prgm = saved_prgm;
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

    void printAlg(OutputStream *os) {
        os->write("ABS(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" ABS");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ABS);
    }
};

//////////////////
/////  Acos  /////
//////////////////

class Acos : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Acos(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Acos() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("ACOS(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" ACOS");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ACOS);
    }
};

//////////////////
/////  Alog  /////
////////////./////

class Alog : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Alog(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Alog() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("ALOG(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" ALOG");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_10_POW_X);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(" AND ");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" AND");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_AND);
    }
};

//////////////////
/////  Asin  /////
//////////////////

class Asin : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Asin(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Asin() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("ASIN(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" ASIN");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ASIN);
    }
};

//////////////////
/////  Atan  /////
//////////////////

class Atan : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Atan(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Atan() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("ATAN(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" ATAN");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_ATAN);
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

#if 0
    double eval() {
        // TODO: Not yet implemented. This is where it get interesting...
        return 0;
    }
#endif

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

    void printRpn(OutputStream *os) {
        for (int i = 0; i < evs->size(); i++) {
            (*evs)[i]->printRpn(os);
            os->write(" ");
        }
        os->write(name);
    }
    
    void generateCode(GeneratorContext *ctx) {
        // TODO
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("=");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" =");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_EQ);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("<>");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" <>");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_NE);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("<");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" <");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_LT);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("<=");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" <=");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_LE);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(">");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" >");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_GT);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(">=");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" >=");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_GE);
    }
};

/////////////////
/////  Cos  /////
/////////////////

class Cos : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Cos(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Cos() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("COS(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" ");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_COS);
    }
};

////////////////////////
/////  Difference  /////
////////////////////////

class Difference : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Difference(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Difference() {
        delete left;
        delete right;
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("-");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" -");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_SUB);
    }
};

/////////////////
/////  Ell  /////
/////////////////

class Ell : public Evaluator {

    private:

    std::string name;
    Evaluator *ev;

    public:

    Ell(int pos, std::string name, Evaluator *ev) : Evaluator(pos), name(name), ev(ev) {}

    ~Ell() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("L(");
        os->write(name);
        os->write(":");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" ");
        os->write(name);
        os->write(" L");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_STO, name);
    }
};

//////////////////////
/////  Equation  /////
//////////////////////

class Equation : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Equation(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Equation() {
        delete left;
        delete right;
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("=");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" =");
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

    void printAlg(OutputStream *os) {
        os->write("S(");
        os->write(name);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        os->write(name);
        os->write(" S");
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_NUMBER, (phloat) 0);
        ctx->addLine(CMD_SVAR_T, name);
        ctx->addLine(CMD_SIGN);
    }
};

/////////////////
/////  Exp  /////
/////////////////

class Exp : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Exp(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Exp() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("EXP(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" EXP");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_E_POW_X);
    }
};

/////////////////
/////  Gee  /////
/////////////////

class Gee : public Evaluator {

    private:

    std::string name;

    public:

    Gee(int pos, std::string name) : Evaluator(pos), name(name) {}

    void printAlg(OutputStream *os) {
        os->write("G(");
        os->write(name);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        os->write(name);
        os->write(" G");
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCL, name);
    }
};

//////////////////////
/////  Identity  /////
//////////////////////

class Identity : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Identity(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Identity() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
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

    void printAlg(OutputStream *os) {
        os->write("IF(");
        condition->printAlg(os);
        os->write(":");
        trueEv->printAlg(os);
        os->write(":");
        falseEv->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        condition->printRpn(os);
        os->write(" ");
        trueEv->printRpn(os);
        os->write(" ");
        falseEv->printRpn(os);
        os->write(" IF");
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

    void printAlg(OutputStream *os) {
        os->write("ITEM(");
        os->write(name);
        os->write(":");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        os->write(name);
        os->write(" ");
        ev->printRpn(os);
        os->write(" ITEM");
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCL, name);
        ev->generateCode(ctx);
        ctx->addLine(CMD_MATITEM);
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

    void printAlg(OutputStream *os) {
        os->write(value);
    }

    void printRpn(OutputStream *os) {
        os->write(value);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_NUMBER, value);
    }
};

////////////////
/////  Ln  /////
////////////////

class Ln : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Ln(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Ln() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("LN(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" LN");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_LN);
    }
};

/////////////////
/////  Log  /////
/////////////////

class Log : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Log(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Log() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("LOG(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" LOG");
    }

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

    void printAlg(OutputStream *os) {
        os->write("MAX(");
        for (int i = 0; i < evs->size(); i++) {
            if (i != 0)
                os->write(":");
            (*evs)[i]->printAlg(os);
        }
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        for (int i = 0; i < evs->size(); i++) {
            (*evs)[i]->printRpn(os);
            os->write(" ");
        }
        os->write((double) evs->size());
        os->write(" MAX");
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

    void printAlg(OutputStream *os) {
        os->write("MIN(");
        for (int i = 0; i < evs->size(); i++) {
            if (i != 0)
                os->write(":");
            (*evs)[i]->printAlg(os);
        }
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        for (int i = 0; i < evs->size(); i++) {
            (*evs)[i]->printRpn(os);
            os->write(" ");
        }
        os->write((double) evs->size());
        os->write(" MIN");
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
};

//////////////////////
/////  Negative  /////
//////////////////////

class Negative : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Negative(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Negative() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("-");
        ev->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" +/-");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_CHS);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        os->write(" NOT ");
        ev->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" NOT");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_GEN_NOT);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(" OR ");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" OR");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_OR);
    }
};

//////////////////////
/////  Positive  /////
//////////////////////

class Positive : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Positive(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Positive() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("+");
        ev->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" NOP");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
    }
};

///////////////////
/////  Power  /////
///////////////////

class Power : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Power(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Power() {
        delete left;
        delete right;
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("^");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" ^");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_Y_POW_X);
    }
};

/////////////////////
/////  Product  /////
/////////////////////

class Product : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Product(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Product() {
        delete left;
        delete right;
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("*");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" *");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_MUL);
    }
};

//////////////////////
/////  Quotient  /////
//////////////////////

class Quotient : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Quotient(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Quotient() {
        delete left;
        delete right;
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("/");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" /");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_DIV);
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

#if 0
    double eval() {
        double f = from->eval();
        double t = to->eval();
        double s = step->eval();
        double sum = 0;
        do {
            vartype *v = new_real(f);
            store_var(name.c_str(), name.length(), v, true);
            sum += ev->eval();
            // TODO: What if step is not positive?
            f += s;
        } while (f <= t);
        return sum;
    }
#endif

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

    void printRpn(OutputStream *os) {
        os->write(name);
        os->write(" ");
        from->printRpn(os);
        os->write(" ");
        to->printRpn(os);
        os->write(" ");
        step->printRpn(os);
        os->write(" ");
        ev->printRpn(os);
        os->write(" Sigma");
    }

    void generateCode(GeneratorContext *ctx) {
        // TODO: This is where it gets tricky; need subroutine so the loop variable can be a local.
    }
};

/////////////////
/////  Sin  /////
/////////////////

class Sin : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Sin(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Sin() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("SIN(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" SIN");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SIN);
    }
};

//////////////////
/////  Sqrt  /////
//////////////////

class Sqrt : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Sqrt(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Sqrt() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("SQRT(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" SQRT");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_SQRT);
    }
};

/////////////////
/////  Sum  /////
/////////////////

class Sum : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Sum(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}

    ~Sum() {
        delete left;
        delete right;
    }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write("+");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" +");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_ADD);
    }
};

/////////////////
/////  Tan  /////
/////////////////

class Tan : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Tan(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Tan() {
        delete ev;
    }

    void printAlg(OutputStream *os) {
        os->write("TAN(");
        ev->printAlg(os);
        os->write(")");
    }

    void printRpn(OutputStream *os) {
        ev->printRpn(os);
        os->write(" TAN");
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(CMD_TAN);
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
    
    std::string name() { return nam; }

    void printAlg(OutputStream *os) {
        os->write(nam);
    }

    void printRpn(OutputStream *os) {
        os->write(nam);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(CMD_RCL, nam);
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

    bool isBool() { return true; }

    void printAlg(OutputStream *os) {
        left->printAlg(os);
        os->write(" XOR ");
        right->printAlg(os);
    }

    void printRpn(OutputStream *os) {
        left->printRpn(os);
        os->write(" ");
        right->printRpn(os);
        os->write(" XOR");
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(CMD_GEN_XOR);
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

    int lpos() {
        return prevpos;
    }
    
    bool isIdentifierStartChar(char c) {
        return c != '+' && c != '-' && c != '\1' && c != '\0'
                && c != '^' && c != '(' && c != ')' && c != '<'
                && c != '>' && c != '=' && c != ':'
                && (compatMode
                        || c != '*' && c != '/' && c != '[' && c != ']' && c != '!');
    }
    
    bool isIdentifierContinuationChar(char c) {
        return c >= '0' && c <= '9' || c == '.' || c == ','
                || isIdentifierStartChar(c);
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
    Parser pz(expr, compatMode);
    Evaluator *ev = pz.parseExpr(CTX_TOP);
    if (ev == NULL) {
        fail:
        *errpos = pz.lex->lpos();
        return NULL;
    }
    std::string t;
    int tpos;
    if (!pz.nextToken(&t, &tpos)) {
        delete ev;
        goto fail;
    }
    if (t == "") {
        // Text consumed completely; this is the good scenario
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

Parser::Parser(std::string expr, bool compatMode) : text(expr), pbpos(-1) {
    lex = new Lexer(expr, compatMode);
}

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

bool Parser::isIdentifier(const std::string &s) {
    if (s.length() == 0)
        return false;
    if (!lex->isIdentifierStartChar(s[0]))
        return false;
    for (int i = 1; i < s.length(); i++)
        if (!lex->isIdentifierContinuationChar(s[i]))
            return false;
    return true;
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
            if (!isIdentifier(t))
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
    } else if (isIdentifier(t)) {
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
                    || t == "ABS") {
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
                    || t == "ALOG" || t == "SQRT" || t == "ABS") {
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
                else if (t == "SQRT")
                    return new Sqrt(tpos, ev);
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
                return new Gee(tpos, n);
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
                return new Ell(tpos, n, ev);
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
