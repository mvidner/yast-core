/*---------------------------------------------------------------------\
|                                                                      |
|                      __   __    ____ _____ ____                      |
|                      \ \ / /_ _/ ___|_   _|___ \                     |
|                       \ V / _` \___ \ | |   __) |                    |
|                        | | (_| |___) || |  / __/                     |
|                        |_|\__,_|____/ |_| |_____|                    |
|                                                                      |
|                               core system                            |
|                                                        (C) SuSE GmbH |
\----------------------------------------------------------------------/

   File:	YExpression.h

   Author:	Klaus Kaempf <kkaempf@suse.de>
   Maintainer:	Klaus Kaempf <kkaempf@suse.de>

   This file defines the various 'expressions' in YCode

/-*/
// -*- c++ -*-

#ifndef YExpression_h
#define YExpression_h

#include <iosfwd>
#include <string>
using std::string;

#include "ycp/YCode.h"
#include "y2/Y2Function.h"

//---------------------------------------------------------

DEFINE_DERIVED_POINTER(YEVariable, YCode);
DEFINE_DERIVED_POINTER(YEReference, YCode);
DEFINE_DERIVED_POINTER(YETerm, YCode);
DEFINE_DERIVED_POINTER(YECompare, YCode);
DEFINE_DERIVED_POINTER(YELocale, YCode);
DEFINE_DERIVED_POINTER(YEList, YCode);
DEFINE_DERIVED_POINTER(YEMap, YCode);
DEFINE_DERIVED_POINTER(YEPropagate, YCode);
DEFINE_DERIVED_POINTER(YEUnary, YCode);
DEFINE_DERIVED_POINTER(YEBinary, YCode);
DEFINE_DERIVED_POINTER(YETriple, YCode);
DEFINE_DERIVED_POINTER(YEIs, YCode);
DEFINE_DERIVED_POINTER(YEReturn, YCode);
DEFINE_DERIVED_POINTER(YEBracket, YCode);
DEFINE_DERIVED_POINTER(YEBuiltin, YCode);
DEFINE_DERIVED_POINTER(YEFunction, YCode);

//---------------------------------------------------------
// variable ref (-> Block + position)

class YEVariable : public YCode
{
    REP_BODY(YEVariable);
    SymbolEntryPtr m_entry;
public:
    YEVariable (SymbolEntryPtr entry);
    YEVariable (std::istream & str);
    ~YEVariable () {};
    const char *name () const;
    SymbolEntryPtr entry () const;
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return m_entry->type(); }
};

//---------------------------------------------------------
// reference (-> Block + position)

class YEReference : public YCode
{
    REP_BODY(YEReference);
    SymbolEntryPtr m_entry;
public:
    YEReference (SymbolEntryPtr entry);
    YEReference (std::istream & str);
    ~YEReference () {};
    const char *name () const;
    SymbolEntryPtr entry () const;
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const;
};

//---------------------------------------------------------
// Term (-> name, args)

class YETerm : public YCode
{
    REP_BODY(YETerm);
    const char *m_name;
    ycodelist_t *m_parameters;
    ycodelist_t *m_last;
public:
    YETerm (const char *name);
    YETerm (std::istream & str);
    ~YETerm ();
    // dummy is here just to make it similar to YEBuiltin and YEFunction
    constTypePtr attachParameter (YCodePtr code, constTypePtr dummy = Type::Unspec);
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return Type::Term; }
};


//---------------------------------------------------------
// Compare (-> arg1, arg2, type)

class YECompare : public YCode
{
    REP_BODY(YECompare);
public:
    enum cmp_op { C_NOT = 1, C_EQ = 2, C_LT = 4,	// base operations
		  C_NEQ = C_NOT|C_EQ,
		  C_LE =  C_EQ|C_LT,
		  C_GE =  C_NOT|C_LT,
		  C_GT =  C_NOT|C_EQ|C_LT
    };
    typedef cmp_op c_op;	    
private:
    YCodePtr m_left;
    c_op m_op;
    YCodePtr m_right;
public:
    YECompare (YCodePtr left, c_op op, YCodePtr right);
    YECompare (std::istream & str);
    ~YECompare ();
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return Type::Boolean; }
};


//---------------------------------------------------------
// locale expression (-> singular, plural, count)

class YELocale : public YCode
{
    REP_BODY(YELocale);
    const char *m_singular;
    const char *m_plural;
    YCodePtr m_count;
    YLocale::t_uniquedomains::const_iterator m_domain;
public:
    YELocale (const char *singular, const char *plural, YCodePtr count, const char *textdomain);
    YELocale (std::istream & str);
    ~YELocale ();
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return Type::Locale; }
};


//---------------------------------------------------------
// list expression (-> value, next list value)

class YEList : public YCode
{
    REP_BODY(YEList);
    ycodelist_t *m_first;
    ycodelist_t *m_last;
public:
    YEList (YCodePtr code);
    YEList (std::istream & str);
    ~YEList ();
    void attach (YCodePtr element);
//    YCodePtr code () const;
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return Type::List; }
    int count () const;
    YCodePtr value (int index) const;
};


//---------------------------------------------------------
// map expression (-> key, value, next key/value pair)

class YEMap : public YCode
{
    REP_BODY(YEMap);
    typedef struct mapval { YCodePtr key; YCodePtr value; struct mapval *next; } mapval_t;
    mapval_t *m_first;
    mapval_t *m_last;
public:
    YEMap (YCodePtr key, YCodePtr value);
    YEMap (std::istream & str);
    ~YEMap ();
    void attach (YCodePtr key, YCodePtr value);
//    YCodePtr key () const;
//    YCodePtr value () const;
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return Type::Map; }
};


//---------------------------------------------------------
// propagation expression (-> value, from type, to type)

class YEPropagate : public YCode
{
    REP_BODY(YEPropagate);
    constTypePtr m_from;
    constTypePtr m_to;
    YCodePtr m_value;
public:
    YEPropagate (YCodePtr value, constTypePtr from, constTypePtr to);
    YEPropagate (std::istream & str);
    ~YEPropagate ();
    string toString () const;
    bool canPropagate(const YCPValue& value, constTypePtr to_type) const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return m_to; }
};


//---------------------------------------------------------
// unary expression (-> declaration_t, arg)

class YEUnary : public YCode
{
    REP_BODY(YEUnary);
    declaration_t *m_decl;
    YCodePtr m_arg;		// argument
public:
    YEUnary (declaration_t *decl, YCodePtr arg);		// expression
    YEUnary (std::istream & str);
    ~YEUnary ();
    declaration_t *decl () const;
//    YCodePtr arg () const;
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return m_decl->type; }
};


//---------------------------------------------------------
// binary expression (-> declaration_t, arg1, arg2)

class YEBinary : public YCode
{
    REP_BODY(YEBinary);
    declaration_t *m_decl;
    YCodePtr m_arg1;		// argument1
    YCodePtr m_arg2;		// argument2
public:
    YEBinary (declaration_t *decl, YCodePtr arg1, YCodePtr arg2);
    YEBinary (std::istream & str);
    ~YEBinary ();
    declaration_t *decl ();
//    YCodePtr arg1 () const;
//    YCodePtr arg2 () const;
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return m_decl->type; }
};


//---------------------------------------------------------
// Triple (? :) expression (-> bool expr, true value, false value)

class YETriple : public YCode
{
    REP_BODY(YETriple);
    YCodePtr m_expr;		// bool expr
    YCodePtr m_true;		// true value
    YCodePtr m_false;		// false value
public:
    YETriple (YCodePtr a_expr, YCodePtr a_true, YCodePtr a_false);
    YETriple (std::istream & str);
    ~YETriple ();
//    YCodePtr expr () const;
//    YCodePtr iftrue () const;
//    YCodePtr iffalse () const;
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return m_true->type ()->commontype (m_false->type ()); }
};


//---------------------------------------------------------
// is (-> expression, type)

class YEIs : public YCode
{
    REP_BODY(YEIs);
    YCodePtr m_expr;
    constTypePtr m_type;
public:
    YEIs (YCodePtr expr, constTypePtr type);
    YEIs (std::istream & str);
    ~YEIs ();
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return Type::Boolean; }
};


//---------------------------------------------------------
// return (-> expression)

class YEReturn : public YCode
{
    REP_BODY(YEReturn);
    YCodePtr m_expr;
public:
    YEReturn (YCodePtr expr);
    YEReturn (std::istream & str);
    ~YEReturn ();
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return m_expr->type(); }
};


//---------------------------------------------------------
// bracket (-> expression)

class YEBracket : public YCode
{
    REP_BODY(YEBracket);
    YCodePtr m_var;		// (list, map) variable
    YCodePtr m_arg;		// bracket arguments
    YCodePtr m_def;		// default expression
    constTypePtr m_resultType;	// result type according to the parser
public:
    YEBracket (YCodePtr var, YCodePtr arg, YCodePtr def, constTypePtr resultType);
    YEBracket (std::istream & str);
    ~YEBracket ();
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const { return m_resultType; }
};


//---------------------------------------------------------
// Builtin ref (-> declaration_t, type, Args)

class YEBuiltin : public YCode
{
    REP_BODY(YEBuiltin);
    declaration_t *m_decl;
    FunctionTypePtr m_type;

    // symbol parameters (NULL, if no symbolic parameters)
    YBlockPtr m_parameterblock;

    ycodelist_t *m_parameters;
    ycodelist_t *m_last;
public:
    YEBuiltin (declaration_t *decl, YBlockPtr parameterblock = 0, constTypePtr type = 0);
    YEBuiltin (std::istream & str);
    ~YEBuiltin ();
    declaration_t *decl () const;
    // see YEFunction::finalize
    constTypePtr finalize ();
    constTypePtr returnType () const;
    // see YEFunction::attachParameter
    constTypePtr attachParameter (YCodePtr code, constTypePtr type = Type::Unspec);
    // attach symbolic variable parameter to function, return created TableEntry
    constTypePtr attachSymVariable (const char *name, constTypePtr type, unsigned int line, TableEntry *&tentry);
    string toString () const;
    YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type () const;
    YBlockPtr parameterBlock () const;
};

//---------------------------------------------------------
// Function ref (-> SymbolEntry ( param, param, ...) )

class YEFunction : public YCode, public Y2Function
{
    REP_BODY(YEFunction);
    const SymbolEntryPtr m_entry;
    ycodelist_t *m_parameters;
    // no need for m_last since we're counting the parameters anyway in attachParameter()
    string qualifiedName () const;
public:
    YEFunction (const SymbolEntryPtr entry);
    YEFunction (std::istream & str);
    ~YEFunction ();
    const SymbolEntryPtr entry () const;
    /**
     * Attach parameter to external function call
     * @param code: parameter code
     * @param type: parameter type
     * @return NULL if success,
     *    != NULL (expected type) if wrong parameter type
     *    Type::Unspec if bad code (NULL or isError)
     *    Type::Error if excessive parameter
     */
    constTypePtr attachParameter (YCodePtr code, constTypePtr type);
    /**
     * 'close' function, perform final parameter check
     * if ok, return 0
     * if missing parameter, return its type
     * if undefined, return Type::Error (wrong type was already
     *   reported in attachParameter()
     */
    constTypePtr finalize ();
    string toString () const;
    virtual YCPValue evaluate (bool cse = false);
    std::ostream & toStream (std::ostream & str) const;
    constTypePtr type() const;

    // implementation of the Y2Function interface:

    virtual bool attachParameter (const YCPValue& arg, const int pos);
    virtual constTypePtr wantedParameterType () const;
    virtual bool appendParameter (const YCPValue& arg);
    virtual bool finishParameters ();
    virtual YCPValue evaluateCall () { return evaluate (); }
};

#endif   // YExpression_h
