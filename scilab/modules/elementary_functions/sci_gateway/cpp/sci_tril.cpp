/*
 * Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
 * Copyright (C) 2012 - Scilab Enterprises - Cedric DELAMARRE
 *
 * This file must be used under the terms of the CeCILL.
 * This source file is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at
 * http://www.cecill.info/licences/Licence_CeCILL_V2-en.txt
 *
 */
/*--------------------------------------------------------------------------*/
#include "elem_func_gw.hxx"
#include "function.hxx"
#include "double.hxx"
#include "string.hxx"
#include "overload.hxx"
#include "execvisitor.hxx"
#include "int.hxx"

extern "C"
{
#include "Scierror.h"
#include "localization.h"
}

template<class T> types::InternalType* tril_const(T* _pL, int iOffset);

/*--------------------------------------------------------------------------*/
types::Function::ReturnValue sci_tril(types::typed_list &in, int _iRetCount, types::typed_list &out)
{
    int iOffset = 0;

    if (in.size() < 1 || in.size() > 2)
    {
        Scierror(77, _("%s: Wrong number of input argument(s): %d to %d expected.\n"), "tril", 1, 2);
        return types::Function::Error;
    }

    if (_iRetCount > 1)
    {
        Scierror(78, _("%s: Wrong number of output argument(s): %d expected.\n"), "tril", 1);
        return types::Function::Error;
    }

    if (in[0]->isGenericType() == false)
    {
        ast::ExecVisitor exec;
        std::wstring wstFuncName = L"%" + in[0]->getShortTypeStr() + L"_tril";
        return Overload::call(wstFuncName, in, _iRetCount, out, &exec);
    }

    if (in[0]->getAs<types::GenericType>()->getDims() > 2)
    {
        ast::ExecVisitor exec;
        std::wstring wstFuncName = L"%" + in[0]->getShortTypeStr() + L"_tril";
        return Overload::call(wstFuncName, in, _iRetCount, out, &exec);
    }

    // get offset
    if (in.size() == 2)
    {
        if (in[1]->isDouble() == false)
        {
            Scierror(999, _("%s: Wrong type for input argument #%d : A real scalar expected.\n"), "tril", 2);
            return types::Function::Error;
        }

        types::Double* pDblOffset = in[1]->getAs<types::Double>();

        if (pDblOffset->isScalar() == false || pDblOffset->isComplex())
        {
            Scierror(999, _("%s: Wrong type for input argument #%d : A real scalar expected.\n"), "tril", 2);
            return types::Function::Error;
        }

        iOffset = (int)pDblOffset->get(0);
    }

    // perform operation
    if (in[0]->isDouble() || in[0]->isInt()) // double
    {
        types::InternalType* pOut = NULL;
        switch (in[0]->getType())
        {
            case types::InternalType::ScilabDouble:
                pOut = tril_const(in[0]->getAs<types::Double>(), iOffset);
                break;
            case types::InternalType::ScilabInt8:
                pOut = tril_const(in[0]->getAs<types::Int8>(), iOffset);
                break;
            case types::InternalType::ScilabInt16:
                pOut = tril_const(in[0]->getAs<types::Int16>(), iOffset);
                break;
            case types::InternalType::ScilabInt32:
                pOut = tril_const(in[0]->getAs<types::Int32>(), iOffset);
                break;
            case types::InternalType::ScilabInt64:
                pOut = tril_const(in[0]->getAs<types::Int64>(), iOffset);
                break;
            case types::InternalType::ScilabUInt8:
                pOut = tril_const(in[0]->getAs<types::UInt8>(), iOffset);
                break;
            case types::InternalType::ScilabUInt16:
                pOut = tril_const(in[0]->getAs<types::UInt16>(), iOffset);
                break;
            case types::InternalType::ScilabUInt32:
                pOut = tril_const(in[0]->getAs<types::UInt32>(), iOffset);
                break;
            case types::InternalType::ScilabUInt64:
                pOut = tril_const(in[0]->getAs<types::UInt64>(), iOffset);
                break;
            default:
            {} // never occurred
        }
        out.push_back(pOut);
    }
    else if (in[0]->isPoly()) // polynom
    {
        types::Polynom* pPolyOut = in[0]->getAs<types::Polynom>()->clone()->getAs<types::Polynom>();
        int iRows = pPolyOut->getRows();
        int iCols = pPolyOut->getCols();

        if (pPolyOut->isComplex())
        {
            for (int i = 0; i < iCols; i++)
            {
                int iSize = min(max(i - iOffset, 0), iRows);
                for (int j = 0; j < iSize; j++)
                {
                    types::SinglePoly* pSP = new types::SinglePoly();
                    pSP->setComplex(true);
                    pPolyOut->set(i * iRows + j, pSP);
                    delete pSP;
                }
            }
        }
        else
        {
            for (int i = 0; i < iCols; i++)
            {
                int iSize = min(max(i - iOffset, 0), iRows);
                for (int j = 0; j < iSize; j++)
                {
                    types::SinglePoly* pSP = new types::SinglePoly();
                    pPolyOut->set(i * iRows + j, pSP);
                    delete pSP;
                }
            }
        }

        out.push_back(pPolyOut);
    }
    else
    {
        ast::ExecVisitor exec;
        std::wstring wstFuncName = L"%" + in[0]->getShortTypeStr() + L"_tril";
        return Overload::call(wstFuncName, in, _iRetCount, out, &exec);
    }

    return types::Function::OK;
}
/*--------------------------------------------------------------------------*/

template<class T> types::InternalType* tril_const(T* _pL, int iOffset)
{
    T* pOut = (T*)_pL->clone();
    int iRows = pOut->getRows();
    int iCols = pOut->getCols();
    typename T::type* pOutReal = pOut->get();

    if (pOut->isComplex())
    {
        typename T::type* pOutImg = pOut->getImg();
        for (int i = 0; i < iCols; i++)
        {
            int iSize = min(max(i - iOffset, 0), iRows);
            memset(&pOutReal[i * iRows], 0x00, iSize * sizeof(typename T::type));
            memset(&pOutImg[i * iRows], 0x00, iSize * sizeof(typename T::type));
        }
    }
    else
    {
        for (int i = 0; i < iCols; i++)
        {
            int iSize = min(max(i - iOffset, 0), iRows);
            memset(&pOutReal[i * iRows], 0x00, iSize * sizeof(typename T::type));
        }
    }

    return pOut;
}