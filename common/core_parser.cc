#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "core_parser.h"

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

/////////////////////
/////  Context  /////
/////////////////////

Context::~Context() {
    for (std::map<std::string, Function *>::iterator it = functions.begin(); it != functions.end(); it++)
        delete it->second;
    for (int i = 0; i < parameters.size(); i++)
        delete parameters[i];
}

void Context::setVariable(std::string name, double value) {
    variables[name] = value;
}

double Context::getVariable(std::string name) {
    for (int i = parameters.size() - 1; i >= 0; i--) {
        std::map<std::string, double> *m = parameters[i];
        std::map<std::string, double>::iterator t = m->find(name);
        if (t != m->end())
            return (*m)[name];
    }
    std::map<std::string, double>::iterator t = variables.find(name);
    if (t != variables.end())
        return t->second;
    else
        return 0;
}

void Context::setFunction(std::string name, Function *function) {
    std::map<std::string, Function *>::iterator it = functions.find(name);
    if (it != functions.end())
        delete it->second;
    functions[name] = function;
}

Function *Context::getFunction(std::string name) {
    return functions[name];
}

void Context::push(std::vector<std::string> &names, std::vector<double> &values) {
    std::map<std::string, double> *h = new std::map<std::string, double>;
    for (int i = 0; i < names.size(); i++)
        (*h)[names[i]] = values[i];
    parameters.push_back(h);
}

void Context::pop() {
    int n = parameters.size() - 1;
    delete parameters[n];
    parameters.erase(parameters.begin() + n);
}

void Context::dump(OutputStream *os, bool alg) {
    for (std::map<std::string, double>::iterator it = variables.begin(); it != variables.end(); it++) {
        os->write(it->first);
        os->write("=");
        os->write(it->second);
        os->newline();
    }
    for (std::map<std::string, Function *>::iterator it = functions.begin(); it != functions.end(); it++) {
        os->write(it->first);
        os->write(alg ? "=" : ":");
        if (alg)
            it->second->printAlg(os);
        else
            it->second->printRpn(os);
        os->newline();
    }
}

/////////////////
/////  Abs  /////
/////////////////

Abs::~Abs() {
    delete ev;
}

double Abs::eval(Context *c) {
    return fabs(ev->eval(c));
}

void Abs::printAlg(OutputStream *os) {
    os->write("ABS(");
    ev->printAlg(os);
    os->write(")");
}

void Abs::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" ABS");
}

//////////////////
/////  Acos  /////
//////////////////

Acos::~Acos() {
    delete ev;
}

double Acos::eval(Context *c) {
    return acos(ev->eval(c));
}

void Acos::printAlg(OutputStream *os) {
    os->write("ACOS(");
    ev->printAlg(os);
    os->write(")");
}

void Acos::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" ACOS");
}

/////////////////
/////  And  /////
/////////////////

And::~And() {
    delete left;
    delete right;
}

double And::eval(Context *c) {
    return left->eval(c) != 0 && right->eval(c) != 0;
}

void And::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write(" AND ");
    right->printAlg(os);
}

void And::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" AND");
}

//////////////////
/////  Asin  /////
//////////////////

Asin::~Asin() {
    delete ev;
}

double Asin::eval(Context *c) {
    return asin(ev->eval(c));
}

void Asin::printAlg(OutputStream *os) {
    os->write("ASIN(");
    ev->printAlg(os);
    os->write(")");
}

void Asin::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" ASIN");
}

//////////////////
/////  Atan  /////
//////////////////

Atan::~Atan() {
    delete ev;
}

double Atan::eval(Context *c) {
    return atan(ev->eval(c));
}

void Atan::printAlg(OutputStream *os) {
    os->write("ATAN(");
    ev->printAlg(os);
    os->write(")");
}

void Atan::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" ATAN");
}

//////////////////
/////  Call  /////
//////////////////

Call::~Call() {
    for (int i = 0; i < evs->size(); i++)
        delete (*evs)[i];
    delete evs;
}

double Call::eval(Context *c) {
    int n = evs->size();
    std::vector<double> values(n);
    for (int i = 0; i < n; i++)
        values[i] = (*evs)[i]->eval(c);
    Function *f = c->getFunction(name);
    double res = f->eval(values, c);
    return res;
}

void Call::printAlg(OutputStream *os) {
    os->write(name);
    os->write("(");
    for (int i = 0; i < evs->size(); i++) {
        if (i != 0)
            os->write(",");
        (*evs)[i]->printAlg(os);
    }
    os->write(")");
}

void Call::printRpn(OutputStream *os) {
    for (int i = 0; i < evs->size(); i++) {
        (*evs)[i]->printRpn(os);
        os->write(" ");
    }
    os->write(name);
}

///////////////////////
/////  CompareEQ  /////
///////////////////////

CompareEQ::~CompareEQ() {
    delete left;
    delete right;
}

double CompareEQ::eval(Context *c) {
    return left->eval(c) == right->eval(c);
}

void CompareEQ::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("=");
    right->printAlg(os);
}

void CompareEQ::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" =");
}

////////////////////////
/////  CompareNE  /////
////////////////////////

CompareNE::~CompareNE() {
    delete left;
    delete right;
}

double CompareNE::eval(Context *c) {
    return left->eval(c) != right->eval(c);
}

void CompareNE::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("<>");
    right->printAlg(os);
}

void CompareNE::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" <>");
}

////////////////////////
/////  CompareLT  /////
////////////////////////

CompareLT::~CompareLT() {
    delete left;
    delete right;
}

double CompareLT::eval(Context *c) {
    return left->eval(c) < right->eval(c);
}

void CompareLT::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("<");
    right->printAlg(os);
}

void CompareLT::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" <");
}

////////////////////////
/////  CompareLE  /////
////////////////////////

CompareLE::~CompareLE() {
    delete left;
    delete right;
}

double CompareLE::eval(Context *c) {
    return left->eval(c) <= right->eval(c);
}

void CompareLE::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("<=");
    right->printAlg(os);
}

void CompareLE::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" <=");
}

////////////////////////
/////  CompareGT  /////
////////////////////////

CompareGT::~CompareGT() {
    delete left;
    delete right;
}

double CompareGT::eval(Context *c) {
    return left->eval(c) > right->eval(c);
}

void CompareGT::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write(">");
    right->printAlg(os);
}

void CompareGT::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" >");
}

////////////////////////
/////  CompareGE  /////
////////////////////////

CompareGE::~CompareGE() {
    delete left;
    delete right;
}

double CompareGE::eval(Context *c) {
    return left->eval(c) >= right->eval(c);
}

void CompareGE::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write(">=");
    right->printAlg(os);
}

void CompareGE::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" >=");
}

/////////////////
/////  Cos  /////
/////////////////

Cos::~Cos() {
    delete ev;
}

double Cos::eval(Context *c) {
    return cos(ev->eval(c));
}

void Cos::printAlg(OutputStream *os) {
    os->write("COS(");
    ev->printAlg(os);
    os->write(")");
}

void Cos::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" ");
}

////////////////////////
/////  Difference  /////
////////////////////////

Difference::~Difference() {
    delete left;
    delete right;
}

double Difference::eval(Context *c) {
    return left->eval(c) - right->eval(c);
}

void Difference::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("-");
    right->printAlg(os);
}

void Difference::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" -");
}

//////////////////////
/////  Equation  /////
//////////////////////

Equation::~Equation() {
    delete left;
    delete right;
}

double Equation::eval(Context *c) {
    return left->eval(c) - right->eval(c);
}

void Equation::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("=");
    right->printAlg(os);
}

void Equation::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" =");
}

/////////////////
/////  Exp  /////
/////////////////

Exp::~Exp() {
    delete ev;
}

double Exp::eval(Context *c) {
    return exp(ev->eval(c));
}

void Exp::printAlg(OutputStream *os) {
    os->write("EXP(");
    ev->printAlg(os);
    os->write(")");
}

void Exp::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" EXP");
}

//////////////////////
/////  Function  /////
//////////////////////

Function::~Function() {
    delete evaluator;
}

double Function::eval(std::vector<double> params, Context *c) {
    c->push(paramNames, params);
    double ret = evaluator->eval(c);
    c->pop();
    return ret;
}

void Function::printAlg(OutputStream *os) {
    os->write("(");
    for (int i = 0; i < paramNames.size(); i++) {
        if (i != 0)
            os->write(",");
        os->write(paramNames[i]);
    }
    os->write(")=>");
    evaluator->printAlg(os);
}

void Function::printRpn(OutputStream *os) {
    for (int i = 0; i < paramNames.size(); i++) {
        os->write(paramNames[i]);
        os->write(" ");
    }
    evaluator->printRpn(os);
}

//////////////////////
/////  Identity  /////
//////////////////////

Identity::~Identity() {
    delete ev;
}

double Identity::eval(Context *c) {
    return ev->eval(c);
}

void Identity::printAlg(OutputStream *os) {
    os->write("(");
    ev->printAlg(os);
    os->write(")");
}

void Identity::printRpn(OutputStream *os) {
    ev->printRpn(os);
}

/////////////////////
/////  Literal  /////
/////////////////////

double Literal::eval(Context *c) {
    return value;
}

void Literal::printAlg(OutputStream *os) {
    os->write(value);
}

void Literal::printRpn(OutputStream *os) {
    os->write(value);
}

/////////////////
/////  Log  /////
/////////////////

Log::~Log() {
    delete ev;
}

double Log::eval(Context *c) {
    return log(ev->eval(c));
}

void Log::printAlg(OutputStream *os) {
    os->write("LOG(");
    ev->printAlg(os);
    os->write(")");
}

void Log::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" LOG");
}

/////////////////
/////  Max  /////
/////////////////

Max::~Max() {
    for (int i = 0; i < evs->size(); i++)
        delete (*evs)[i];
    delete evs;
}

double Max::eval(Context *c) {
    double res = -DBL_MAX;
    for (int i = 0; i < evs->size(); i++) {
        double x = (*evs)[i]->eval(c);
        if (x > res)
            res = x;
    }
    return res;
}

void Max::printAlg(OutputStream *os) {
    os->write("MAX(");
    for (int i = 0; i < evs->size(); i++) {
        if (i != 0)
            os->write(",");
        (*evs)[i]->printAlg(os);
    }
    os->write(")");
}

void Max::printRpn(OutputStream *os) {
    for (int i = 0; i < evs->size(); i++) {
        (*evs)[i]->printRpn(os);
        os->write(" ");
    }
    os->write((double) evs->size());
    os->write(" MAX");
}

/////////////////
/////  Min  /////
/////////////////

Min::~Min() {
    for (int i = 0; i < evs->size(); i++)
        delete (*evs)[i];
    delete evs;
}

double Min::eval(Context *c) {
    double res = DBL_MAX;
    for (int i = 0; i < evs->size(); i++) {
        double x = (*evs)[i]->eval(c);
        if (x < res)
            res = x;
    }
    return res;
}

void Min::printAlg(OutputStream *os) {
    os->write("MIN(");
    for (int i = 0; i < evs->size(); i++) {
        if (i != 0)
            os->write(",");
        (*evs)[i]->printAlg(os);
    }
    os->write(")");
}

void Min::printRpn(OutputStream *os) {
    for (int i = 0; i < evs->size(); i++) {
        (*evs)[i]->printRpn(os);
        os->write(" ");
    }
    os->write((double) evs->size());
    os->write(" MIN");
}

/////////////////
/////  Not  /////
/////////////////

Not::~Not() {
    delete ev;
}

double Not::eval(Context *c) {
    return ev->eval(c) == 0;
}

void Not::printAlg(OutputStream *os) {
    os->write(" NOT ");
    ev->printAlg(os);
}

void Not::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" NOT");
}

////////////////
/////  Or  /////
////////////////

Or::~Or() {
    delete left;
    delete right;
}

double Or::eval(Context *c) {
    return left->eval(c) != 0 || right->eval(c) != 0;
}

void Or::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write(" OR ");
    right->printAlg(os);
}

void Or::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" OR");
}

//////////////////////
/////  Negative  /////
//////////////////////

Negative::~Negative() {
    delete ev;
}

double Negative::eval(Context *c) {
    return -ev->eval(c);
}

void Negative::printAlg(OutputStream *os) {
    os->write("-");
    ev->printAlg(os);
}

void Negative::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" +/-");
}

//////////////////////
/////  Positive  /////
//////////////////////

Positive::~Positive() {
    delete ev;
}

double Positive::eval(Context *c) {
    return ev->eval(c);
}

void Positive::printAlg(OutputStream *os) {
    os->write("+");
    ev->printAlg(os);
}

void Positive::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" NOP");
}

///////////////////
/////  Power  /////
///////////////////

Power::~Power() {
    delete left;
    delete right;
}

double Power::eval(Context *c) {
    return pow(left->eval(c), right->eval(c));
}

void Power::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("^");
    right->printAlg(os);
}

void Power::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" ^");
}

/////////////////////
/////  Product  /////
/////////////////////

Product::~Product() {
    delete left;
    delete right;
}

double Product::eval(Context *c) {
    return left->eval(c) * right->eval(c);
}

void Product::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("*");
    right->printAlg(os);
}

void Product::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" *");
}

//////////////////////
/////  Quotient  /////
//////////////////////

Quotient::~Quotient() {
    delete left;
    delete right;
}

double Quotient::eval(Context *c) {
    return left->eval(c) / right->eval(c);
}

void Quotient::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("/");
    right->printAlg(os);
}

void Quotient::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" /");
}

/////////////////
/////  Sin  /////
/////////////////

Sin::~Sin() {
    delete ev;
}

double Sin::eval(Context *c) {
    return sin(ev->eval(c));
}

void Sin::printAlg(OutputStream *os) {
    os->write("SIN(");
    ev->printAlg(os);
    os->write(")");
}

void Sin::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" SIN");
}

//////////////////
/////  Sqrt  /////
//////////////////

Sqrt::~Sqrt() {
    delete ev;
}

double Sqrt::eval(Context *c) {
    return sqrt(ev->eval(c));
}

void Sqrt::printAlg(OutputStream *os) {
    os->write("SQRT(");
    ev->printAlg(os);
    os->write(")");
}

void Sqrt::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" SQRT");
}

/////////////////
/////  Sum  /////
/////////////////

Sum::~Sum() {
    delete left;
    delete right;
}

double Sum::eval(Context *c) {
    return left->eval(c) + right->eval(c);
}

void Sum::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write("+");
    right->printAlg(os);
}

void Sum::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" +");
}

/////////////////
/////  Tan  /////
/////////////////

Tan::~Tan() {
    delete ev;
}

double Tan::eval(Context *c) {
    return tan(ev->eval(c));
}

void Tan::printAlg(OutputStream *os) {
    os->write("TAN(");
    ev->printAlg(os);
    os->write(")");
}

void Tan::printRpn(OutputStream *os) {
    ev->printRpn(os);
    os->write(" TAN");
}

//////////////////////
/////  Variable  /////
//////////////////////

double Variable::eval(Context *c) {
    return c->getVariable(name);
}

void Variable::printAlg(OutputStream *os) {
    os->write(name);
}

void Variable::printRpn(OutputStream *os) {
    os->write(name);
}

/////////////////
/////  Xor  /////
/////////////////

Xor::~Xor() {
    delete left;
    delete right;
}

double Xor::eval(Context *c) {
    return (left->eval(c) != 0) != (right->eval(c) != 0);
}

void Xor::printAlg(OutputStream *os) {
    left->printAlg(os);
    os->write(" XOR ");
    right->printAlg(os);
}

void Xor::printRpn(OutputStream *os) {
    left->printRpn(os);
    os->write(" ");
    right->printRpn(os);
    os->write(" XOR");
}

///////////////////
/////  Lexer  /////
///////////////////

class Lexer {

    private:

    std::string text;
    int pos;

    public:

    Lexer(std::string text) {
        this->text = text;
        pos = 0;
    }

    int lpos() {
        return pos;
    }

    bool nextToken(std::string *tok, int *tpos) {
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
        // Compound symbols
        if (c == '<' || c == '>') {
            if (pos < text.length()) {
                char c2 = text[pos + 1];
                if (c2 == '=' || c == '<' && c2 == '>') {
                    pos++;
                    *tok = text.substr(start, 2);
                    return true;
                }
            }
            *tok = text.substr(start, 1);
            return true;
        }
        // One-character symbols
        if (c == '+' || c == '-' || c == '*' || c == '/'
                || c == '(' || c == ')' || c == '[' || c == ']'
                || c == '^' || c == ':' || c == '=') {
                // Note: 42S mul/div/NE/LE/GE symbols!
            *tok = text.substr(start, 1);
            return true;
        }
        // What's left at this point is numbers and names.
        // Which one we're currently looking at depends on its
        // first character; if that's a digit or a decimal,
        // it's a number; anything else, it's a name.
        bool multi_dot = false;
        if (c == '.' || c >= '0' && c <= '9') {
            int state = c == '.' ? 1 : 0;
            int d0 = c == '.' ? 0 : 1;
            int d1 = 0, d2 = 0;
            while (pos < text.length()) {
                c = text[pos];
                switch (state) {
                    case 0:
                        if (c == '.')
                            state = 1;
                        else if (c == 'E' || c == 'e')
                            // Note: 42S exponent symbol!
                            state = 2;
                        else if (c >= '0' && c <= '9')
                            d0++;
                        else
                            goto done;
                        break;
                    case 1:
                        if (c == '.') {
                            multi_dot = true;
                            goto done;
                        }
                        else if (c == 'E' || c == 'e')
                            // Note: 42S exponent symbol!
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
            if (d0 == 0 && d1 == 0 // A '.' not followed by a digit.
                    || multi_dot // Multiple periods
                    || state == 2  // An 'E' not followed by a valid character.
                    || state == 3 && d2 == 0) { // An 'E' not followed by at least one digit
                *tok = "";
                return false;
            }
            *tok = text.substr(start, pos - start);
            return true;
        } else {
            while (pos < text.length()) {
                char c = text[pos];
                if (c == '+' || c == '-' || c == '*' || c == '/'
                        || c == '(' || c == ')' || c == '[' || c == ']'
                        || c == '^' || c == ':' || c == '='
                        // Note: 42S mul/div/NE/LE/GE symbols!
                        || c == '<' || c == '>' || c == ' ')
                    break;
                pos++;
            }
            *tok = text.substr(start, pos - start);
            return true;
        }
    }
};

////////////////////
/////  Parser  /////
////////////////////

/* static */ Evaluator *Parser::parse(std::string expr, int *errpos) {
    Parser pz(expr);
    Evaluator *ev = pz.parseEquation();
    if (ev == NULL)
        *errpos = pz.lex->lpos();
    return ev;
}

Parser::Parser(std::string expr) : text(expr), pbpos(-1) {
    lex = new Lexer(expr);
}

Parser::~Parser() {
    delete lex;
}

Evaluator *Parser::parseEquation() {
    Evaluator *ev = parseExpr();
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
    if (t != "=")
        goto fail;
    Evaluator *ev2 = parseExpr();
    if (ev2 == NULL)
        goto fail;
    return new Equation(tpos, ev, ev2);
}

Evaluator *Parser::parseExpr() {
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
            Evaluator *ev2 = parseAnd();
            if (ev2 == NULL)
                goto fail;
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
            Evaluator *ev2 = parseNot();
            if (ev2 == NULL)
                goto fail;
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
        if (ev == NULL)
            return NULL;
        else
            // Check ev->isBool(); if wrong, set error position
            // appropriately, delete ev, and return NULL.
            // Implement similar type checks throughout the parser,
            // in all places where Evaluators are instantiated.
            // Don't defer the checks to the instantiation itself,
            // but perform them as early as possible. Check their
            // behavior against the 17B.
            return new Not(tpos, ev);
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
    if (t == "=" || t == "<>" || t == "<" || t == "<=" || t == ">" || t == ">=") {
        Evaluator *ev2 = parseNumExpr();
        if (ev2 == NULL)
            goto fail;
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
            delete ev;
            return NULL;
        }
        if (t == "")
            return ev;
        if (t == "+" || t == "-") {
            Evaluator *ev2 = parseTerm();
            if (ev2 == NULL) {
                delete ev;
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
            if (!nextToken(&t, &tpos)) {
                delete ev;
                return NULL;
            }
            if (t == "")
                return ev;
            if (t == "*" || t == "/") {
                Evaluator *ev2 = parseFactor();
                if (ev2 == NULL) {
                    delete ev;
                    return NULL;
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
            Evaluator *ev2 = parseThing();
            if (ev2 == NULL)
                goto fail;
            ev = new Power(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
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
        if (t == "+")
            return new Positive(tpos, ev);
        else
            return new Negative(tpos, ev);
    }
    double d;
    if (sscanf(t.c_str(), "%lf", &d) == 1) {
        return new Literal(tpos, d);
    } else if (!isOperator(t)) {
        std::string t2;
        int t2pos;
        if (!nextToken(&t2, &t2pos))
            return NULL;
        if (t2 == "(") {
            std::vector<Evaluator *> *evs = parseExprList();
            if (evs == NULL)
                return NULL;
            if (!nextToken(&t2, &t2pos) || t2 != ")") {
                fail:
                for (int i = 0; i < evs->size(); i++)
                    delete (*evs)[i];
                delete evs;
                return NULL;
            }
            /* TODO: Parsing an arbitrarily long argument list when you
             * know you need exactly one seems a bit stupid, and will lead
             * to unhelpful error messages.
             */
            if (t == "SIN" || t == "COS" || t == "TAN"
                    || t == "ASIN" || t == "ACOS" || t == "ATAN"
                    || t == "LOG" || t == "EXP" || t == "SQRT"
                    || t == "ABS") {
                if (evs->size() != 1)
                    goto fail;
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
                else if (t == "LOG")
                    return new Log(tpos, ev);
                else if (t == "EXP")
                    return new Exp(tpos, ev);
                else if (t == "SQRT")
                    return new Sqrt(tpos, ev);
                else // t == "ABS"
                    return new Abs(tpos, ev);
            } else if (t == "MAX")
                return new Max(tpos, evs);
            else if (t == "MIN")
                return new Min(tpos, evs);
            else
                return new Call(tpos, t, evs);
        } else {
            pushback(t2, t2pos);
            return new Variable(tpos, t);
        }
    } else if (t == "(") {
        Evaluator *ev = parseExpr();
        if (ev == NULL)
            return NULL;
        std::string t2;
        int t2pos;
        if (!nextToken(&t2, &t2pos) || t2 != ")") {
            delete ev;
            return NULL;
        }
        return new Identity(tpos, ev);
    } else
        return NULL;
}

std::vector<Evaluator *> *Parser::parseExprList() {
    std::vector<Evaluator *> *evs = new std::vector<Evaluator *>;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            for (int i = 0; i < evs->size(); i++)
                delete (*evs)[i];
            delete evs;
            return NULL;
        }
        pushback(t, tpos);
        if (t == ")")
            return evs;
        Evaluator *ev = parseExpr();
        if (ev == NULL)
            goto fail;
        evs->push_back(ev);
        if (!nextToken(&t, &tpos))
            goto fail;
        if (t != ",") {
            pushback(t, tpos);
            return evs;
        }
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

/* static */ bool Parser::isOperator(const std::string &s) {
    return s.find_first_of("+-*/^(),") != std::string::npos;
}

#if 0

int main(int argc, char *argv[]) {
    Context c;
    char line[1024];
    char *eqpos;
    OutputStream *out = new FileOutputStream(stdout, true);

    while (true) {
        printf("> ");
        fflush(stdout);
        if (fgets(line, 1024, stdin) == NULL)
            break;
        //strcpy(line, "SIN(1.57)");
        int linelen = strlen(line);
        while (linelen > 0 && isspace(line[0]))
            memmove(line, line + 1, linelen--);
        while (linelen > 0 && isspace(line[linelen - 1]))
            line[--linelen] = 0;
        if (linelen == 0)
            break;
        if (strcmp(line, "exit") == 0) {
            break;
        } else if (strcmp(line, "dump") == 0 || strcmp(line, "dumpalg") == 0) {
            c.dump(out, true);
        } else if (strcmp(line, "dumprpn") == 0) {
            c.dump(out, false);
        } else if ((eqpos = strchr(line, '=')) != NULL) {
            // Assignment
            std::string left(line, eqpos - line);
            std::string right(eqpos + 1);
            int p1 = left.find('(');
            std::string name = left.substr(0, p1);
            if (p1 != std::string::npos) {
                // Function definition
                std::vector<std::string> paramNames;
                while (++p1 < left.length()) {
                    int p2 = left.find_first_of(",)", p1);
                    if (p2 == std::string::npos) {
                        paramNames.push_back(left.substr(p1));
                        break;
                    }
                    paramNames.push_back(left.substr(p1, p2 - p1));
                    p1 = p2;
                }
                int errpos;
                Evaluator *ev = Parser::parse(right, &errpos);
                if (ev == NULL) {
                    fprintf(stderr, "Error at %d\n", errpos);
                    continue;
                }
                Function *f = new Function(paramNames, ev);
                c.setFunction(name, f);
            } else {
                // Variable assignment
                int errpos;
                Evaluator *ev = Parser::parse(right, &errpos);
                if (ev == NULL) {
                    fprintf(stderr, "Error at %d\n", errpos);
                    continue;
                }
                c.setVariable(left, ev->eval(&c));
                delete ev;
            }
        } else {
            // Immediate evaluation
            int errpos;
            Evaluator *ev = Parser::parse(line, &errpos);
            if (ev == NULL) {
                fprintf(stderr, "Error at %d\n", errpos);
                continue;
            }
            out->write(ev->eval(&c));
            out->newline();
            delete ev;
        }
    }
    delete out;
    return 0;
}

#endif
