/*
 *  Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
 *  Copyright (C) 2014 - Scilab Enterprises - Calixte DENIZET
 *
 *  This file must be used under the terms of the CeCILL.
 *  This source file is licensed as described in the file COPYING, which
 *  you should have received as part of this distribution.  The terms
 *  are also available at
 *  http://www.cecill.info/licences/Licence_CeCILL_V2-en.txt
 *
 */

#ifndef __ANALYSIS_VISITOR_HXX__
#define __ANALYSIS_VISITOR_HXX__

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "visitor.hxx"
#include "debugvisitor.hxx"
#include "execvisitor.hxx"
#include "printvisitor.hxx"
#include "allexp.hxx"
#include "allvar.hxx"
#include "analyzers/CallAnalyzer.hxx"
#include "checkers/Checkers.hxx"
#include "Chrono.hxx"
#include "ForList.hxx"
#include "Result.hxx"
#include "SymInfo.hxx"
#include "Temporary.hxx"
#include "TIType.hxx"
#include "ConstantVisitor.hxx"
#include "gvn/SymbolicList.hxx"

#include "data/DataManager.hxx"
#include "data/PolymorphicMacroCache.hxx"
#include "gvn/ConstraintManager.hxx"
#include "logging/Logger.hxx"
#include "dynlib_ast.h"

namespace analysis
{

class EXTERN_AST AnalysisVisitor : public ast::Visitor, public Chrono
{

public:

    typedef std::map<symbol::Symbol, SymInfo> MapSymInfo;
    typedef unordered_map<std::wstring, std::shared_ptr<CallAnalyzer>> MapSymCall;
    typedef std::vector<Call *> Calls;

private:

    MapSymInfo symsinfo;
    Result _result;
    Temporary temp;
    Calls allCalls;
    DataManager dm;
    PolymorphicMacroCache pmc;
    ConstraintManager cm;
    ConstantVisitor cv;
    ast::DebugVisitor dv;
    ast::PrintVisitor pv;
    std::vector<Result> multipleLHS;
    logging::Logger logger;

    static MapSymCall symscall;
    static MapSymCall initCalls();

public:

    static bool asDouble(ast::Exp & e, double & out);
    static bool asDouble(types::InternalType * pIT, double & out);
    static bool isDoubleConstant(const ast::Exp & e);
    static bool asDoubleMatrix(ast::Exp & e, types::Double *& data);
    static void analyze(ast::SelectExp & e);

    AnalysisVisitor() : cv(*this), pv(std::wcerr, true, false), logger("/tmp/analysis.log")
    {
        start_chrono();
    }

    virtual ~AnalysisVisitor()
    {
        //std::cerr << "delete AnalysisVisitor" << std::endl;
    }

    inline CallAnalyzer * getAnalyzer(const symbol::Symbol & sym)
    {
        MapSymCall::iterator it = symscall.find(sym.getName());
        if (it == symscall.end())
        {
            return nullptr;
        }
        else
        {
            return it->second.get();
        }
    }

    inline DataManager & getDM()
    {
        return dm;
    }

    inline GVN & getGVN()
    {
        return dm.getGVN();
    }

    inline PolymorphicMacroCache & getPMC()
    {
        return pmc;
    }

    inline ConstantVisitor & getCV()
    {
        return cv;
    }

    inline ast::PrintVisitor & getPV()
    {
        return pv;
    }

    inline ast::ExecVisitor & getExec()
    {
        return cv.getExec();
    }

    // Only for debug use
    inline void print_info()
    {
        stop_chrono();

        //std::wcout << getGVN() << std::endl << std::endl; function z=foo(x,y);z=argn(2);endfunction;jit("x=123;y=456;t=foo(x,y)")
        std::wcerr << L"Analysis: " << *static_cast<Chrono *>(this) << std::endl;
        //std::wcout << temp << std::endl;

        std::wcerr << dm << std::endl;

        std::wcerr << std::endl;
    }

    inline void finalize()
    {
        //dm.finalize(nullptr);
    }

    inline void setResult(Result & val)
    {
        _result = val;
    }

    inline void setResult(Result && val)
    {
        _result = val;
    }

    inline Result & getResult()
    {
        return _result;
    }

    inline const Temporary & getTemp() const
    {
        return temp;
    }

    inline Temporary & getTemp()
    {
        return temp;
    }

    inline const Calls & getCalls() const
    {
        return allCalls;
    }

    inline std::vector<Result> & getLHSContainer()
    {
        return multipleLHS;
    }

    inline ConstraintManager & getCM()
    {
        if (FunctionBlock * fblock = getDM().topFunction())
        {
            return fblock->getConstraintManager();
        }
        else
        {
            return cm;
        }
    }

    template<typename T>
    inline void visitArguments(const std::wstring & name, const unsigned int lhs, const TIType & calltype, ast::CallExp & e, T && args)
    {
        std::vector<Result> resargs;
        std::vector<TIType> vargs;
        vargs.reserve(args.size());
        resargs.reserve(args.size());

        for (typename T::const_iterator i = args.begin(), end = args.end(); i != end; ++i)
        {
            (*i)->accept(*this);
            resargs.push_back(getResult());
            vargs.push_back(getResult().getType());
        }

        const symbol::Symbol & sym = static_cast<ast::SimpleVar &>(e.getName()).getSymbol();
        int tempId = -1;
        if (lhs > 1)
        {
            std::vector<TIType> types = dm.call(*this, lhs, sym, vargs, &e);
            multipleLHS.clear();
            multipleLHS.reserve(types.size());
            for (const auto & type : types)
            {
                multipleLHS.emplace_back(type);
            }
        }
        else
        {
            std::vector<TIType> out = dm.call(*this, lhs, sym, vargs, &e);
            if (lhs == 1)
            {
                e.getDecorator().res = Result(out[0], tempId);
                e.getDecorator().setCall(name, vargs);
                setResult(e.getDecorator().res);
            }
        }
    }

    inline Info & getSymInfo(const symbol::Symbol & sym)
    {
        return dm.getInfo(sym);
    }

    inline logging::Logger & getLogger()
    {
        return logger;
    }

    bool analyzeIndices(TIType & type, ast::CallExp & ce);

private:

    bool getDimension(SymbolicDimension & dim, ast::Exp & arg, bool & safe, SymbolicDimension & out);

    inline void pushCall(Call * c)
    {
        if (c)
        {
            allCalls.push_back(c);
        }
    }

    bool operGVNValues(ast::OpExp & oe);
    bool operSymbolicRange(ast::OpExp & oe);

    void visit(ast::SelectExp & e);
    void visit(ast::ListExp & e);
    void visit(ast::MatrixExp & e);
    void visit(ast::OpExp & e);
    void visit(ast::NotExp & e);
    void visit(ast::TransposeExp & e);
    void visit(ast::AssignExp & e);
    void visit(ast::IfExp & e);

    void visit(ast::MatrixLineExp & e)
    {
        /* treated in MatrixExp */
    }
    void visit(ast::OptimizedExp & e) { }
    void visit(ast::MemfillExp & e) { }
    void visit(ast::DAXPYExp & e) { }
    void visit(ast::IntSelectExp & e) { }
    void visit(ast::StringSelectExp & e) { }
    void visit(ast::CommentExp & e) { }
    void visit(ast::NilExp & e) { }
    void visit(ast::ColonVar & e) { }

    void visit(ast::SimpleVar & e)
    {
        logger.log(L"SimpleVar", e.getSymbol().getName(), e.getLocation());
        symbol::Symbol & sym = e.getSymbol();
        Info & info = dm.read(sym, &e);
        Result & res = e.getDecorator().setResult(info.type);
        res.setConstant(info.getConstant());
        res.setRange(info.getRange());
	res.setMaxIndex(info.getMaxIndex());
        setResult(res);
    }

    void visit(ast::DollarVar & e)
    {
        logger.log(L"DollarVar", e.getLocation());
        Result & res = e.getDecorator().setResult(TIType(dm.getGVN(), TIType::POLYNOMIAL, 1, 1));
        res.getConstant() = getGVN().getValue(symbol::Symbol(L"$"));
        setResult(res);
    }

    void visit(ast::ArrayListVar & e)
    {
        logger.log(L"ArrayListVar", e.getLocation());
        const ast::exps_t & vars = e.getVars();
        for (auto var : vars)
        {
            var->accept(*this);
        }
    }

    void visit(ast::DoubleExp & e)
    {
        logger.log(L"DoubleExp", e.getLocation());
        if (!e.getConstant())
        {
            e.accept(cv.getExec());
            cv.getExec().setResult(nullptr);
        }
        types::Double * pDbl = static_cast<types::Double *>(e.getConstant());
        Result & res = e.getDecorator().setResult(TIType(dm.getGVN(), TIType::DOUBLE, pDbl->getRows(), pDbl->getCols()));
        res.getConstant() = e.getConstant();
        setResult(res);
    }

    void visit(ast::BoolExp & e)
    {
        logger.log(L"BoolExp", e.getLocation());
        if (!e.getConstant())
        {
            e.accept(cv.getExec());
            cv.getExec().setResult(nullptr);
        }
        types::Bool * pBool = static_cast<types::Bool *>(e.getConstant());
        Result & res = e.getDecorator().setResult(TIType(dm.getGVN(), TIType::BOOLEAN, pBool->getRows(), pBool->getCols()));
        res.getConstant() = e.getConstant();
        setResult(res);
    }

    void visit(ast::StringExp & e)
    {
        logger.log(L"StringExp", e.getLocation());
        if (!e.getConstant())
        {
            e.accept(cv.getExec());
            cv.getExec().setResult(nullptr);
        }
        types::String * pStr = static_cast<types::String *>(e.getConstant());
        Result & res = e.getDecorator().setResult(TIType(dm.getGVN(), TIType::STRING, pStr->getRows(), pStr->getCols()));
        res.getConstant() = e.getConstant();
        setResult(res);
    }

    void visit(ast::CallExp & e, const unsigned int lhs)
    {
        // TODO: e.getName() is not always a simple var: foo(a)(b)
        if (e.getName().isSimpleVar())
        {
            ast::SimpleVar & var = static_cast<ast::SimpleVar &>(e.getName());
            symbol::Symbol & sym = var.getSymbol();
            const std::wstring & name = sym.getName();
            Info & info = getSymInfo(sym); // that put the sym in the current block !
	    Result & res = e.getName().getDecorator().setResult(info.type);
	    res.setConstant(info.getConstant());
	    res.setRange(info.getRange());
	    res.setMaxIndex(info.getMaxIndex());

            logger.log(L"CallExp", e.getLocation(), name);

            if (info.type.type == TIType::MACRO || info.type.type == TIType::MACROFILE || info.type.type == TIType::FUNCTION)
            {
                if (name == L"error")
                {
                    getDM().getCurrent()->setReturn(true);
                }

                // Special analysis cases: size, zeros, ones, ...
                MapSymCall::iterator it = symscall.find(sym.getName());
                if (it != symscall.end())
                {
                    if (getCM().checkGlobalConstant(sym) && it->second.get()->analyze(*this, lhs, e))
                    {
                        pushCall(e.getDecorator().getCall());
                        return;
                    }
                }

                visitArguments(name, lhs, info.type, e, e.getArgs());
                pushCall(e.getDecorator().getCall());
            }
            else
            {
                analyzeIndices(info.type, e);
            }
        }
    }

    void visit(ast::CallExp & e)
    {
        visit(e, 1);
    }

    void visit(ast::CellCallExp & e)
    {
        logger.log(L"CellCallExp", e.getLocation());
        visit(static_cast<ast::CallExp &>(e));
    }

    void visit(ast::LogicalOpExp & e)
    {
        logger.log(L"LogicalOpExp", e.getLocation());
        visit(static_cast<ast::OpExp &>(e));
    }

    void visit(ast::WhileExp & e)
    {
        logger.log(L"WhileExp", e.getLocation());
        e.getTest().accept(*this);
        e.getBody().accept(*this);
    }

    void visit(ast::ForExp & e)
    {
        // TODO: pb de propagation de constante ds le corps du loop
        // b = 0; for....; b=b+..; end => le 2nd "b" est replaced by 0

        logger.log(L"ForExp", e.getLocation());
        dm.addBlock(Block::LOOP, &e);
        e.getVardec().accept(*this);
        dm.addBlock(Block::NORMAL, &e.getBody());
        e.getBody().accept(*this);

        if (false && dm.requiresAnotherTrip())
        {
            std::wcerr << "Invalid forexp: types or refcount are not the same before and after the loop" << std::endl;

            dm.finalizeBlock();
            dm.addBlock(Block::NORMAL, &e.getBody());
            e.getBody().accept(*this);
        }

        dm.finalizeBlock();
        dm.finalizeBlock();
    }

    void visit(ast::BreakExp & e)
    {
        logger.log(L"BreakExp", e.getLocation());
        // nothing to do
    }

    void visit(ast::ContinueExp & e)
    {
        logger.log(L"ContinueExp", e.getLocation());
        // nothing to do
    }

    void visit(ast::TryCatchExp & e)
    {
        logger.log(L"TryCatchExp", e.getLocation());
        e.getTry().accept(*this);
        e.getCatch().accept(*this);
    }

    void visit(ast::CaseExp & e)
    {
        logger.log(L"CaseExp", e.getLocation());
        e.getTest()->accept(*this);
        e.getBody()->accept(*this);
    }

    void visit(ast::ReturnExp & e)
    {
        logger.log(L"ReturnExp", e.getLocation());
        getDM().getCurrent()->setReturn(true);
        // Bug with return;
        //e.exp_get().accept(*this);

    }

    void visit(ast::FieldExp & e)
    {
        logger.log(L"FieldExp", e.getLocation());
        // a.b.c <=> (a.b).c where a.b is the head and c is the tail

        //e.head_get()->accept(*this);
        //e.tail_get()->accept(*this);
    }

    void visit(ast::CellExp & e)
    {
        logger.log(L"CellExp", e.getLocation());
        visit(static_cast<ast::MatrixExp &>(e));
    }

    void visit(ast::SeqExp & e)
    {
        logger.log(L"SeqExp", e.getLocation());
        for (const auto exp : e.getExps())
        {
            if (exp->isCallExp())
            {
                visit(*static_cast<ast::CallExp *>(exp), /* LHS */ 0);
            }
            else
            {
                exp->accept(*this);
            }
        }
	if (!e.getParent())
	{
	    //e.accept(dv);
	}
    }

    void visit(ast::ArrayListExp & e)
    {
        logger.log(L"ArrayListExp", e.getLocation());
        const ast::exps_t & exps = e.getExps();
        for (const auto exp : e.getExps())
        {
            exp->accept(*this);
        }
    }

    void visit(ast::AssignListExp & e)
    {
        logger.log(L"AssignListExp", e.getLocation());
        visit(static_cast<ast::ArrayListExp &>(e));
    }

    void visit(ast::VarDec & e)
    {
        // VarDec is only used in For loop for iterator declaration
        logger.log(L"VarDec", e.getLocation());
        const symbol::Symbol & sym = e.getSymbol();
        if (e.getInit().isListExp())
        {
            ast::ListExp & le = static_cast<ast::ListExp &>(e.getInit());
            double start, step, end;
            if (asDouble(le.getStart(), start) && asDouble(le.getStep(), step) && asDouble(le.getEnd(), end))
            {
                ForList64 fl(start, step, end);
                e.setListInfo(fl);
                dm.define(sym, fl.getType(), &e).isint = true;
                // No need to visit the list (it has been visited just before)
            }
            else
            {
                e.setListInfo(ForList64());
                le.accept(*this);
                Result & res = getResult();
                Info & info = dm.define(sym, res.getType(), &e);
                info.setRange(res.getRange());
            }
        }
    }

    void visit(ast::FunctionDec & e)
    {
        /*e.args_get().accept(*this);
          e.returns_get().accept(*this);
          e.body_get().accept(*this);*/
        logger.log(L"FunctionDec", e.getLocation());
        dm.macrodef(&e);
    }
};

} // namespace analysis

#endif // __ANALYSIS_VISITOR_HXX__