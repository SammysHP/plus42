#include <map>
#include <string>
#include <vector>

#include "core_globals.h"

////////////////////////////////
/////  class declarations  /////
////////////////////////////////

class OutputStream {
    public:

    virtual ~OutputStream() {}

    virtual void write(std::string) = 0;
    virtual void write(double d);
    virtual void newline();
};

class GeneratorContext;

class Evaluator {

    protected:

    int tpos;

    public:

    Evaluator(int pos) : tpos(pos) {}
    virtual ~Evaluator() {}
    virtual bool isBool() { return false; }
    virtual bool isEquation() { return false; }
    virtual std::string name() { return ""; }
    virtual std::string eqnName() { return ""; }
    virtual Evaluator *removeName() { return this; }
    virtual bool is(const std::string *name) { return false; }
    virtual Evaluator *clone() = 0;

    int pos() { return tpos; }

    virtual void printAlg(OutputStream *os) = 0;
    virtual void generateCode(GeneratorContext *ctx) = 0;
    virtual void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) = 0;
    virtual int howMany(const std::string *name) = 0;
};

class Lexer;

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
