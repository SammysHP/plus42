#include <map>
#include <string>
#include <vector>

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

class Function;

class Context {

    private:

    std::map<std::string, double> variables;
    std::map<std::string, Function *> functions;
    std::vector<std::map<std::string, double> *> parameters;

    public:

    ~Context();
    void setVariable(std::string name, double value);
    double getVariable(std::string name);
    void setFunction(std::string name, Function *function);
    Function *getFunction(std::string name);
    void push(std::vector<std::string> &names, std::vector<double> &values);
    void pop();
    void dump(OutputStream *os, bool alg);
};

class Evaluator {

    private:

    int tpos;

    public:

    Evaluator(int pos) : tpos(pos) {}
    virtual ~Evaluator() {}
    virtual bool isBool() { return false; }

    int pos() { return tpos; }

    virtual double eval(Context *c) = 0;
    virtual void printAlg(OutputStream *os) = 0;
    virtual void printRpn(OutputStream *os) = 0;
};

class Abs : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Abs(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Abs();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Acos : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Acos(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Acos();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class And : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    And(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~And();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Asin : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Asin(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Asin();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Atan : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Atan(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Atan();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Call : public Evaluator {

    private:

    std::string name;
    std::vector<Evaluator *> *evs;

    public:

    Call(int pos, std::string name, std::vector<Evaluator *> *evs) : Evaluator(pos), name(name), evs(evs) {}
    ~Call();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class CompareEQ : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareEQ(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~CompareEQ();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class CompareNE : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareNE(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~CompareNE();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class CompareLT : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareLT(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~CompareLT();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class CompareLE : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareLE(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~CompareLE();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class CompareGT : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareGT(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~CompareGT();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class CompareGE : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    CompareGE(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~CompareGE();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Cos : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Cos(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Cos();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Difference : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Difference(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Difference();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Equation : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Equation(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Equation();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Exp : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Exp(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Exp();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Function {

    private:

    std::vector<std::string> paramNames;
    Evaluator *evaluator;

    public:

    Function(std::vector<std::string> &paramNames, Evaluator *ev) : paramNames(paramNames), evaluator(ev) {}
    ~Function();
    double eval(std::vector<double> params, Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Identity : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Identity(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Identity();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class If : public Evaluator {

    private:

    Evaluator *condition, *trueEv, *falseEv;

    public:

    If(int pos, Evaluator *condition, Evaluator *trueEv, Evaluator *falseEv)
            : Evaluator(pos), condition(condition), trueEv(trueEv), falseEv(falseEv) {}
    ~If();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Literal : public Evaluator {

    private:

    double value;

    public:

    Literal(int pos, double value) : Evaluator(pos), value(value) {}
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Log : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Log(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Log();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Max : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Max(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}
    ~Max();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Min : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Min(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}
    ~Min();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Negative : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Negative(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Negative();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Not : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Not(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Not();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Or : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Or(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Or();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Positive : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Positive(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Positive();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Power : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Power(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Power();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Product : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Product(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Product();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Quotient : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Quotient(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Quotient();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Sin : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Sin(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Sin();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Sqrt : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Sqrt(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Sqrt();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Sum : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Sum(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Sum();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Tan : public Evaluator {

    private:

    Evaluator *ev;

    public:

    Tan(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}
    ~Tan();
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Variable : public Evaluator {

    private:

    std::string name;

    public:

    Variable(int pos, std::string name) : Evaluator(pos), name(name) {}
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
};

class Xor : public Evaluator {

    private:

    Evaluator *left, *right;

    public:

    Xor(int pos, Evaluator *left, Evaluator *right) : Evaluator(pos), left(left), right(right) {}
    ~Xor();
    bool isBool() { return true; }
    double eval(Context *c);
    void printAlg(OutputStream *os);
    void printRpn(OutputStream *os);
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

    private:

    Parser(std::string expr, bool compatMode);
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
