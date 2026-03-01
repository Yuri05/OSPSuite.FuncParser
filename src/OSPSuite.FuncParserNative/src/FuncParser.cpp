#include "FuncParser/FuncParser.h"
#include "FuncParser/ElemFunction.h"
#include "FuncParser/Constant.h"
#include "FuncParser/Constants.h"
#include "FuncParser/ElemFunctions.h"
#include "FuncParser/FuncNode.h"
#include "FuncParser/FuncParserErrorData.h"
#include "FuncParser/Math.h"
#include "FuncParser/PInvokeHelper.h"

#ifdef _WINDOWS
#pragma warning( disable : 4996)
#include <windows.h>
#include <string>
#ifdef _DEBUG
//#include <vld.h> //uncomment to compile with Visual Leak Detector support
//s. https://github.com/Open-Systems-Pharmacology/OSPSuite.SimModel/wiki/Find-memory-leaks-with-Visual-Leak-Detector
#endif
#endif

#ifdef linux
#include <cstring>
#include <stdlib.h>
#endif

namespace FuncParserNative
{
const  std::string FuncParser::conNOTSymbol = std::string(1, (unsigned char)0xAC); //"¬"
const  std::string FuncParser::conORSymbol = "|";
const  std::string FuncParser::conANDSymbol = "&";
const  std::string FuncParser::conListDelimiter = ";";

bool FuncParser::IsNumeric (const std::string & str)
{
	char *end;

	strtod(str.c_str(), &end);

	if (*end != '\0')
		return false;

	return true;
}

double FuncParser::StringToDouble (const std::string & str) const
{
	char *end;
	double x;

	x = strtod(str.c_str(), &end);

	if (*end != '\0')
		return Math::GetNaN();

	return x;
}

std::string FuncParser::ToUpper (const std::string & Source)
{
	char * NewStr = new char[Source.size()+1];
	const char *csource = Source.c_str();
    for(size_t i=0; i<Source.size(); i++)
		NewStr[i]=toupper(csource[i]);
	NewStr[Source.size()] = '\0';
	std::string newString = NewStr;
	delete[] NewStr;
	return newString;
}

std::string FuncParser::StringReplace (const std::string & source, const char * find, const char * replace, bool CaseSensitive /*= true*/)
{
	size_t iPos=0;
	std::string str=source;

	if(!find)
		return str;

	char * _find = new char[strlen(find)+1];
	strcpy(_find, find);

	if(!CaseSensitive)
	{
        for(size_t i=0; i<strlen(find); i++)
			_find[i] = toupper(_find[i]);
	}

	iPos=str.find_first_of(_find[0]);
	while(iPos != std::string::npos)
	{
		std::string substr = str.substr(iPos, strlen(_find));
		if (!CaseSensitive)
			substr=ToUpper(substr);
		if (substr == _find)
		{
			str.replace(iPos, strlen(_find), replace);
		}
		else
			iPos++;
		iPos=str.find_first_of(_find[0],iPos);
	}

	delete[] _find;
	return str;
}

std::string FuncParser::StringReplace (const std::string & source, const std::string & find, const std::string & replace, bool CaseSensitive /*= true*/)
{
	return StringReplace(source,find.c_str(), replace.c_str(), CaseSensitive);
}

std::string FuncParser::ParsedStringToDisplayString (const std::string & str)
{
	std::string dispStr;

	dispStr = StringReplace(str,conNOTSymbol," NOT ");
	dispStr = StringReplace(dispStr,conANDSymbol," AND ");
	dispStr = StringReplace(dispStr,conORSymbol," OR ");

	return dispStr;
}

FuncParser::FuncParser ()
{
	_elemFunctions = ElemFunctions::GetInstance();
}

FuncParser::~FuncParser ()
{
}

const Constants & FuncParser::GetConstants () const
{
    return _constants;
}

bool FuncParser::IsBracketed(const string& SubExpr)
{
    if (SubExpr.empty() || SubExpr[0] != '(' || SubExpr[SubExpr.size()-1] != ')')
        return false;

	//We already know that SubExpr[0]="("
	//So we can set the brackets counter to 1 and start with the 2nd character of the string
    long BracketCounter = 1; 
    for(size_t i = 1; i < SubExpr.size(); i++)
    {
        if (SubExpr[i] == '(')
            BracketCounter++;
        else if (SubExpr[i] == ')')
            BracketCounter--;

		//If we found the closing bracket for the initial opening bracket before the end of the string: 
		//the whole string is not a bracketed expression. E.g. the string can be something like (x+y)*(z+t)
        if (BracketCounter == 0 && i < SubExpr.size()-1)
            return false;
    }
    return BracketCounter == 0;
}

std::string FuncParser::RemoveBrackets (const std::string & SubExpr) const
{
   auto expressionWithoutOuterBrackets = SubExpr;

	while(IsBracketed(expressionWithoutOuterBrackets))
	{
      expressionWithoutOuterBrackets = expressionWithoutOuterBrackets.substr(1, expressionWithoutOuterBrackets.length()-2);
	}

   return expressionWithoutOuterBrackets;
}

FuncNode * FuncParser::Parse (const std::string & ParsedString, const std::vector < std::string > & VariableNames, const std::vector < std::string > & ParameterNames, bool CaseSensitive, bool LogicOperatorsAllowed, double ComparisonTolerance, bool LogicalNumericMixAllowed)
{
	FuncNode * newNode = NULL;
	const std::string ERROR_SOURCE = "FuncParser::Parse";

	try
	{
		//cache string to parse for using it in error message if parse fails
		_stringToParse = ParsedString;

		//All checks of Parameter/Variable Names (different, not reserved functions, etc.) must
		//be done before by caller (ParsedFunction)
		FuncParserErrorData ED;

        size_t i;
        size_t j;

		StringVector NOTPreDelimiter;
		StringVector NOTPostDelimiter;
		StringVector ORANDPreDelimiter;
		StringVector ORANDPostDelimiter;
		std::string _ParsedString;

		_ParsedString = ParsedString;
		_variableNames = VariableNames;
		_parameterNames = ParameterNames;

		//count brackets
		long BracketsCount = 0;

		for (i=0; i<_ParsedString.length(); i++)
		{
			if (_ParsedString.substr(i,1) == "(")
				BracketsCount++;
			else if (_ParsedString.substr(i,1) == ")")
				BracketsCount--;
		}

		if (BracketsCount < 0)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Number of opening brackets is less than number of closing brackets");
		if (BracketsCount > 0)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Number of closing brackets is less than number of opening brackets");

		//if NOT case sensitive: convert parsing string and all variables/parameters to upper case
		if (!CaseSensitive)
		{
			for (i=0; i<_variableNames.size(); i++)
				_variableNames[i] = ToUpper(_variableNames[i]);
			for (i=0; i<_parameterNames.size(); i++)
				_parameterNames[i] = ToUpper(_parameterNames[i]);
			_ParsedString = ToUpper(_ParsedString);
		}

		//check if variable/parameter names are valid
		CheckVarParamNames(_variableNames, _parameterNames);

		//replace logical operators "AND", "OR", "NOT" with predefined symbols
		NOTPreDelimiter.push_back(" ");
		NOTPreDelimiter.push_back("(");

		NOTPostDelimiter.push_back(" ");
		NOTPostDelimiter.push_back("(");

	    ORANDPreDelimiter.push_back(" ");
	    ORANDPreDelimiter.push_back(")");

	    ORANDPostDelimiter.push_back(" ");
	    ORANDPostDelimiter.push_back("(");

		_ParsedString.insert(0," ");
		_ParsedString.append(" ");

		for (i=0; i<NOTPreDelimiter.size(); i++)
			for (j=0; j<NOTPostDelimiter.size(); j++)
				_ParsedString = StringReplace(_ParsedString,
				                               NOTPreDelimiter[i]+"NOT"+NOTPostDelimiter[j],
				                               NOTPreDelimiter[i]+conNOTSymbol+NOTPostDelimiter[j], false);
		for (i=0; i<ORANDPreDelimiter.size(); i++)
			for (j=0; j<ORANDPostDelimiter.size(); j++)
			{
				_ParsedString = StringReplace(_ParsedString,
				                               ORANDPreDelimiter[i]+"OR"+ORANDPostDelimiter[j],
				                               ORANDPreDelimiter[i]+conORSymbol+ORANDPostDelimiter[j], false);
				_ParsedString = StringReplace(_ParsedString,
				                               ORANDPreDelimiter[i]+"AND"+ORANDPostDelimiter[j],
				                               ORANDPreDelimiter[i]+conANDSymbol+ORANDPostDelimiter[j], false);
			}

		//check for logic operators
		if (!LogicOperatorsAllowed)
		{
			if ((_ParsedString.find_first_of(conNOTSymbol) != std::string::npos) ||
			    (_ParsedString.find_first_of(conORSymbol) != std::string::npos) ||
			    (_ParsedString.find_first_of(conANDSymbol) != std::string::npos) ||
			    (_ParsedString.find_first_of('<') != std::string::npos) ||
			    (_ParsedString.find_first_of('>') != std::string::npos) ||
			    (_ParsedString.find_first_of('!') != std::string::npos) ||
			    (_ParsedString.find_first_of('=') != std::string::npos))
				throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
				                          "Logical operators are not allowed");
		}

		//remove all spaces
		_ParsedString = StringReplace(_ParsedString, " ", "");

		//replace comma with point
		_ParsedString = StringReplace(_ParsedString, ",", ".");

		//replace point with current decimal separator USED BY C++ RUNTIME LIBRARY
		//This decimal separator is used by strtod function while converting strings to
		//double numbers
		if (strcmp(".", localeconv()->decimal_point))
			_ParsedString = StringReplace(_ParsedString, ".", localeconv()->decimal_point);

		//remove newline characters
		_ParsedString = StringReplace(_ParsedString, "\r\n", "");
		_ParsedString = StringReplace(_ParsedString, "\n", "");

		//remove tabulator characters
		_ParsedString = StringReplace(_ParsedString, "\t", "");

		//create new func node
		newNode = new FuncNode;

		EvalExpression(newNode, _ParsedString, LOA_IF);

		//check if numeric/logical operators are not illegally mixed
		if(LogicOperatorsAllowed && !LogicalNumericMixAllowed)
		{
			newNode->IsNumericNode(); //will throw Funcparser exception in case of not allowed mix
		}

		//no simplifying here!
		//if function tree should be simplified here ONLY for constants (NOT for parameters),
		//than uncomment following two lines
//		double Dummy;
//		newNode->SimplifyNode(Dummy, ComparisonTolerance);
	}
	catch(FuncParserErrorData &)
	{
		if (newNode != NULL)
		{
			delete newNode;
			newNode = NULL;
		}
      throw;
	}
	catch(...)
	{
		if (newNode != NULL)
		{
			delete newNode;
			newNode = NULL;
		}
      throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}

	return newNode;
}

void FuncParser::EvalExpression (FuncNode * SubNode, const std::string & SubExpr, enmLevelOfAbstraction LevelOfAbstraction) 
{
	const char * ERROR_SOURCE = "FuncParser::EvalExpression";

	try
	{
		std::string Op1, Op2;                         //operation (strings) to search
		ElemFunction * EF1 = NULL, * EF2 = NULL;      //functions to insert if operations strings are found
		enmLevelOfAbstraction NextLevelOfAbstraction; //

		bool FirstTerm = true;
		size_t FirstPos = 0;
		std::string NewOp;
		std::string NextTerm;

      auto expression = SubExpr;

		if (LevelOfAbstraction == LOA_IF)
         expression = RemoveBrackets(SubExpr);
		else
			if (IsBracketed(expression))
			{
				EvalExpression(SubNode, expression, LOA_IF);
				return;
			}

		switch(LevelOfAbstraction)
		{
			case LOA_IF:
				EvalIF(SubNode, expression);
				return;
			case LOA_OR:
				EF1 = (*_elemFunctions)[ElemFunction::EF_OR];
				EF2 = EF1;
				NextLevelOfAbstraction = LOA_AND;
				break;
			case LOA_AND:
				EF1 = (*_elemFunctions)[ElemFunction::EF_AND];
				EF2 = EF1;
				NextLevelOfAbstraction = LOA_NOT;
				break;
			case LOA_NOT:
				EvalNOTOperand(SubNode, expression);
				return;
			case LOA_Comparison:
				EvalComparison(SubNode, expression);
				return;
			case LOA_PLUSMINUS:
				EF1 = (*_elemFunctions)[ElemFunction::EF_PLUS];
				EF2 = (*_elemFunctions)[ElemFunction::EF_MINUS];
				NextLevelOfAbstraction = LOA_MULDIV;
				//replace expression beginning with - (e.g. -2*x+3) with 0-<old expr.>
				if (expression.substr(0,1) == "-")
					expression = "0"+expression;
				break;
			case LOA_MULDIV:
				EF1 = (*_elemFunctions)[ElemFunction::EF_MUL];
				EF2 = (*_elemFunctions)[ElemFunction::EF_DIV];
				NextLevelOfAbstraction = LOA_FACTOR;
				break;
			case LOA_FACTOR:
				EvalFactor(SubNode, expression);
				return;
		}

		assert (EF1 && EF2);

		Op1 = EF1->GetFuncString();
		Op2 = EF2->GetFuncString();

		if ((LevelOfAbstraction == LOA_PLUSMINUS) || (LevelOfAbstraction == LOA_MULDIV))
		{
			RearrangeTerms(expression, LevelOfAbstraction, Op1, Op2);
		}

		while(true)
		{
			// get next term
			NextTerm = GetNextTerm(expression, LevelOfAbstraction, Op1, Op2, FirstPos, NewOp);

			if (NewOp.empty())
			{
				// last term found
				if (FirstTerm)
				{
					EvalExpression(SubNode, expression, NextLevelOfAbstraction);
				}
				else
				{
					SubNode->SetSecondOperand(new FuncNode);
					EvalExpression(SubNode->GetSecondOperand(), NextTerm, NextLevelOfAbstraction);
				}
				return;
			}
			else //Op1 or Op2 reached
			{
				if (FirstTerm)
				{
					FirstTerm = false;
					SubNode->SetNodeType(FuncNode::NT_FUNCTION);
                    SubNode->SetNodeFunction(NewOp==Op1 ? EF1 : EF2);
					SubNode->SetFirstOperand(new FuncNode);
					EvalExpression(SubNode->GetFirstOperand(), NextTerm, NextLevelOfAbstraction);
				}
				else
				{
					SubNode->SetSecondOperand(new FuncNode);
					SubNode = SubNode->GetSecondOperand();
					SubNode->SetNodeType(FuncNode::NT_FUNCTION);
					SubNode->SetNodeFunction(NewOp==Op1 ? EF1 : EF2);

                    NewOp = SubNode->GetNodeFunction()->GetFuncString();

					SubNode->SetFirstOperand(new FuncNode);
					EvalExpression(SubNode->GetFirstOperand(), NextTerm, NextLevelOfAbstraction);
				}
			}
		}
	}
	catch(FuncParserErrorData & ED)
	{
      adjustErrorDescription(ED, SubExpr);
      throw ED;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}

}

void FuncParser::EvalNOTOperand (FuncNode * SubNode, const std::string & SubExpr)
{
	const std::string ERROR_SOURCE = "FuncParser::EvalNOTOperand";

	try
	{
		if (SubExpr.substr(0,1) == conNOTSymbol)
		{
			SubNode->SetNodeType(FuncNode::NT_FUNCTION);
            SubNode->SetNodeFunction((*_elemFunctions)[ElemFunction::EF_NOT]);
			SubNode->SetFirstOperand(new FuncNode);
			EvalExpression(SubNode->GetFirstOperand(), SubExpr.substr(1), LOA_Comparison);
			return;
		}

		EvalExpression(SubNode, SubExpr, LOA_Comparison);
	}
	catch(FuncParserErrorData & ED)
	{
      adjustErrorDescription(ED, SubExpr);
      throw ED;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}
}

void FuncParser::EvalComparison (FuncNode * SubNode, const std::string & SubExpr)
{
	const std::string ERROR_SOURCE = "FuncParser::EvalComparison";

	try
	{
        long BracketsCount;
        unsigned int i;
		std::string NextCharacter;
		std::string SubSubExpr;
		size_t FirstOperandLastPosition, SecondOperandFirstPosition;

		if (SubExpr.length() == 0)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Empty factor");

		BracketsCount = 0;
		for (i=0; i<SubExpr.length(); i++)
		{
			NextCharacter = SubExpr.substr(i,1);
			if (NextCharacter == "(")
				BracketsCount++;
			else if (NextCharacter == ")")
				BracketsCount--;
			else if (((NextCharacter == "<") || (NextCharacter == ">") ||
                      (NextCharacter == "=") || (NextCharacter == "!")) &&
                     (BracketsCount == 0))
            {
            	if (i == 0)
					throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
					                          "Missing first operand in logical statement");
            	if (i == SubExpr.length())
					throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
					                          "Missing second operand in logical statement");

          		FirstOperandLastPosition = i - 1;
          		SubSubExpr = SubExpr.substr(FirstOperandLastPosition+1, 2);

          		if ((NextCharacter == "!") && (SubSubExpr.substr(1,1) != "="))
					throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
					                          "Missing '=' after '!'");
          		if ((SubSubExpr == "<=") || (SubSubExpr == ">=") ||
          		    (SubSubExpr == "!=") || (SubSubExpr == "<>"))
          			SecondOperandFirstPosition = FirstOperandLastPosition + 3;
		        else
        			SecondOperandFirstPosition = FirstOperandLastPosition + 2;

        		SubNode->SetNodeType(FuncNode::NT_FUNCTION);

				std::string FuncString = SubExpr.substr(FirstOperandLastPosition + 1,
        		                                        SecondOperandFirstPosition - FirstOperandLastPosition - 1);
        		SubNode->SetNodeFunction((*_elemFunctions)[FuncString]);
        		assert(SubNode->GetNodeFunction()!=NULL);

				SubNode->SetFirstOperand(new FuncNode);
				SubNode->SetSecondOperand(new FuncNode);

				EvalExpression(SubNode->GetFirstOperand(), SubExpr.substr(0, FirstOperandLastPosition+1),
				               LOA_PLUSMINUS);

				EvalExpression(SubNode->GetSecondOperand(), SubExpr.substr(SecondOperandFirstPosition),
				               LOA_PLUSMINUS);

				return;
           }
		}

		EvalExpression(SubNode, SubExpr, LOA_PLUSMINUS);
	}
	catch(FuncParserErrorData & ED)
	{
      adjustErrorDescription(ED, SubExpr);
      throw ED;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}

}

void FuncParser::EvalFactor (FuncNode * SubNode, const std::string & SubExpr)
{
	const std::string ERROR_SOURCE = "FuncParser::EvalFactor";

	try
	{
        unsigned int i;
		std::string FuncName, VarName;
		long BracketsCount;
        size_t CharPos;
		std::string NextCharacter;

		if (SubExpr.length() == 0)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Empty factor");

		if (IsBracketed(SubExpr)) //'(<Expr.>) -> 'Expression'
		{
			EvalExpression(SubNode, SubExpr, LOA_IF);
			return;
		}

		if (IsNumeric(SubExpr)) //Constant
		{
			SubNode->SetNodeType(FuncNode::NT_CONST);
			SubNode->SetNodeValue(StringToDouble(SubExpr));
			return;
		}

		//check, if SubExpr is a^b
        CharPos = std::string::npos;

		while(true)
		{
			CharPos = SubExpr.find_first_of('^',CharPos + 1);
            if (CharPos == std::string::npos)
				break;
			if (CharPos == 0)
				throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                              "Missing first operand in x^y");

			BracketsCount = 0;
			for (i=0; i<=CharPos - 1; i++)
			{
				NextCharacter = SubExpr.substr(i,1);
				if (NextCharacter == "(")
					BracketsCount++;
				else if (NextCharacter == ")")
					BracketsCount--;
			}
			if (BracketsCount == 0) //so a^b^c will be evaluated as a^(b^c)
			{
				SubNode->SetNodeType(FuncNode::NT_FUNCTION);
                SubNode->SetNodeFunction((*_elemFunctions)[ElemFunction::EF_POWER]);
				SubNode->SetFirstOperand(new FuncNode);
				SubNode->SetSecondOperand(new FuncNode);
				EvalFactor(SubNode->GetFirstOperand(), SubExpr.substr(0, CharPos));
				EvalFactor(SubNode->GetSecondOperand(), SubExpr.substr(CharPos+1));
				return;
			}
		}

		//check if variable
		for (i=0; i<_variableNames.size(); i++)
			if (_variableNames[i] == SubExpr)
			{
				SubNode->SetNodeType(FuncNode::NT_VARIABLE);
				SubNode->SetVarOrParamIndex(i);
				return;
			}

		//check if variable with argument <Var>(<Expr>)
		if (SubExpr.substr(SubExpr.length()-1,1) == ")")
		{
			CharPos = SubExpr.find_first_of('(');
			if (CharPos > 0)
			{
				VarName = SubExpr.substr(0,CharPos);

				for (i=0; i<_variableNames.size(); i++)
				{
					if (_variableNames[i] == VarName)
					{
						//set node type VARIABLE
						SubNode->SetNodeType(FuncNode::NT_VARIABLE);
						SubNode->SetVarOrParamIndex(i);

						//read variable argument
						SubNode->SetVariableArgument(new FuncNode);
						EvalExpression(SubNode->GetVariableArgument(), SubExpr.substr(CharPos+1,SubExpr.length()-CharPos-2), LOA_IF);

						return;
					}
				}
			}
		}

		//check if parameter
		for (i=0; i<_parameterNames.size(); i++)
			if (_parameterNames[i] == SubExpr)
			{
				SubNode->SetNodeType(FuncNode::NT_PARAMETER);
				SubNode->SetVarOrParamIndex(i);
				return;
			}

		//Check, if constant
		//Comparison with constants are NOT case sensitive, so PI, Pi, pi are all accepted
		if (_constants.Exists(ToUpper(SubExpr)))
		{
			SubNode->SetNodeType(FuncNode::NT_CONST);
			SubNode->SetNodeValue(_constants[ToUpper(SubExpr)]->GetValue());
			return;
		}

		//check for NaN
		if ((ToUpper(SubExpr) == "NAN") || (ToUpper(SubExpr) == "N.DEF.") || (ToUpper(SubExpr) == "N. DEF."))
		{
			SubNode->SetNodeType(FuncNode::NT_CONST);
			SubNode->SetNodeValue(Math::GetNaN());
			return;
		}

		//check if SubExpr is known (elementary) function <f>(<Expr>)
		if (SubExpr.substr(SubExpr.length()-1,1) != ")")
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Invalid Expression");
		CharPos = SubExpr.find_first_of('(');
        if (CharPos == std::string::npos)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Invalid Expression");

		FuncName = ToUpper(SubExpr.substr(0,CharPos));
		ElemFunction * pElemFunc = (*_elemFunctions)[FuncName];
		if (pElemFunc == NULL)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Unknown function: "+FuncName);

		//special treatment: random functions (Rnd(), Srnd())
		if (pElemFunc->IsRandomFunction ())
		{
			std::string rndArgs = SubExpr.substr(CharPos+1,SubExpr.length()-CharPos-2);
			if (rndArgs.length()>0)
				throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE, 
	  			                          "Arguments list for the "+FuncName + " must be empty");
	  		
	  		//setup node
			SubNode->SetNodeType(FuncNode::NT_FUNCTION);
			SubNode->SetNodeFunction(pElemFunc);
			
			return;
		}
		
		//special treatment:
		// Min(x;y), Max(x;y)
		// EQ(X;Y), GT(X;Y), GEQ(X;Y), LT(X;Y), LEQ(X;Y), NEQ(X;Y)
		// POW(X;Y)
		if ((pElemFunc->GetType() == ElemFunction::EF_MIN) || (pElemFunc->GetType() == ElemFunction::EF_MAX) ||
		    (pElemFunc->GetType() == ElemFunction::EF_EQUAL_F) || (pElemFunc->GetType() == ElemFunction::EF_GREATER_F) ||
		    (pElemFunc->GetType() == ElemFunction::EF_GREATEREQUAL_F) || (pElemFunc->GetType() == ElemFunction::EF_LESS_F) ||
		    (pElemFunc->GetType() == ElemFunction::EF_LESSEQUAL_F) || (pElemFunc->GetType() == ElemFunction::EF_UNEQUAL_F) ||
		    (pElemFunc->GetType() == ElemFunction::EF_POWER_F) )
		{
			std::string FirstArg, SecondArg, Rest, NewOp;
			size_t FirstPos = 0;
			std::string ArgList = SubExpr.substr(CharPos+1,SubExpr.length()-CharPos-2);

			//get first argument
			FirstArg = GetNextTerm(ArgList, LOA_FACTOR, conListDelimiter, conListDelimiter, FirstPos, NewOp);

			//get second argument
			SecondArg = GetNextTerm(ArgList, LOA_FACTOR, conListDelimiter, conListDelimiter, FirstPos, NewOp);

			//get the rest of arguments list (just to be sure it's empty
			Rest = GetNextTerm(ArgList, LOA_FACTOR, conListDelimiter, conListDelimiter, FirstPos, NewOp);

			//check exactly 2 operands are given
			if ((FirstArg.length() == 0) || (SecondArg.length() == 0) || (Rest.length() > 0))
				throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
  			                              "Invalid arguments list for the "+FuncName + " function");

			//setup node and 2 operands
			SubNode->SetNodeType(FuncNode::NT_FUNCTION);
			SubNode->SetNodeFunction((*_elemFunctions)[FuncName]);

			SubNode->SetFirstOperand(new FuncNode);
			EvalExpression(SubNode->GetFirstOperand(), FirstArg, LOA_IF);

			SubNode->SetSecondOperand(new FuncNode);
			EvalExpression(SubNode->GetSecondOperand(), SecondArg, LOA_IF);

			return;
		}

		//only numeric unary functions are allowed here
		if (pElemFunc->IsLogical() || ! pElemFunc->IsUnary())   //should never happen
			throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE,
			                          "Standard elementary functions must be numeric and unary (\""+FuncName+"\")");

		SubNode->SetNodeType(FuncNode::NT_FUNCTION);
		SubNode->SetNodeFunction((*_elemFunctions)[FuncName]);
		SubNode->SetFirstOperand(new FuncNode);
		EvalExpression(SubNode->GetFirstOperand(), SubExpr.substr(CharPos+1,SubExpr.length()-CharPos-2), LOA_IF);
	}
	catch(FuncParserErrorData & ED)
	{
		adjustErrorDescription(ED, SubExpr);
      throw ED;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}

}

void FuncParser::EvalIF (FuncNode * SubNode, const std::string & SubExpr)
{
	const std::string ERROR_SOURCE = "FuncParser::EvalIF";
	const std::string Op1 = "?";
	const std::string Op2 = ":";

	try
	{
		std::string IfStatement, ThenStatement, ElseStatement;
		size_t FirstPos = 0;
		std::string NewOp;

		IfStatement = GetNextTerm(SubExpr, LOA_IF, Op1, Op1, FirstPos,NewOp);

		if (NewOp.empty()) //no "?" found ==> not an if..then..else
		{
			EvalExpression(SubNode, SubExpr, LOA_OR);
			return;
		}

		//"?" found - check if subexpression has form a ? b : c
		ThenStatement = GetNextTerm(SubExpr, LOA_IF, Op2, Op2, FirstPos,NewOp);

		if (!NewOp.empty())
			ElseStatement = GetNextTerm(SubExpr, LOA_IF, Op1, Op2, FirstPos,NewOp);

		//check if subexpression is correct
		if ((IfStatement.empty()) || (ThenStatement.empty()) || (ElseStatement.empty()) || (!NewOp.empty()))
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Invalid IF statement (must be <IfPart> ? <ThenPart> : <ElsePart>)");

		//everything OK - populate node
		SubNode->SetNodeType(FuncNode::NT_FUNCTION);
        SubNode->SetNodeFunction((*_elemFunctions)[ElemFunction::EF_IF]);
		SubNode->SetFirstOperand(new FuncNode);
		SubNode->SetSecondOperand(new FuncNode);
		SubNode->SetBranchCondition(new FuncNode);

		EvalExpression(SubNode->GetBranchCondition(), IfStatement, LOA_IF);
		EvalExpression(SubNode->GetFirstOperand(), ThenStatement, LOA_IF);
		EvalExpression(SubNode->GetSecondOperand(), ElseStatement, LOA_IF);
	}
	catch(FuncParserErrorData & ED)
	{
      adjustErrorDescription(ED, SubExpr);
      throw ED;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}

}

void FuncParser::CheckVarParamNames (const std::vector < std::string > & VariableNames, const std::vector < std::string > & ParameterNames)
{
	const std::string ERROR_SOURCE = "FuncParser::CheckVarParamNames";

	try
	{
        size_t i;
        size_t j;
		const std::string IllegalChars = " + - * \\ / ^ . , < > = ! ( ) [ ] { } '\" ? : "+
		                                 conNOTSymbol+" "+conANDSymbol+" "+conORSymbol+" "+conListDelimiter;

		for(i=0; i<VariableNames.size(); i++)
		{
			std::string VarName = VariableNames[i];

			//check if variable name is not empty
			if (VarName.empty())
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Variable name may not be empty");

			//check if variable name doesn't contain illegal characters
			if (VarName.find_first_of(IllegalChars) != std::string::npos)
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Variable name '"+VarName+"' contains illegal character(s)\n"+
				                          "Illegal characters are "+IllegalChars+" and blank");

			//check if variable name is none of the logical operators
			std::string VarToUpper = ToUpper(VarName);
			if ((VarToUpper == "OR") || (VarToUpper == "AND") || (VarToUpper=="NOT"))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Variable name may not be a logical operator ('"+VarName+"')\n");

			//check if var_i<>ElemFunction_k
			if(_elemFunctions->Exists(VarToUpper))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Variable name must be different from all standard functions ('"+VarName+"')\n"+
				                          "Available standard functions are: "+_elemFunctions->GetList());

			//check if var_i<>ElemConst_k
			if(_constants.Exists(VarToUpper))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Variable name must be different from all standard constants ('"+VarName+"')\n"+
				                          "Available standard constants are: "+_constants.GetList());

			//check if variable name is not numeric
			if(IsNumeric(VarName))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Variable name may not be numeric ('"+VarName+"')\n");

			//check if variable names are different
			for(j=i+1; j<VariableNames.size(); j++)
				if(VarName == VariableNames[j])
					throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
					                          "Variable name '"+VarName+"' is not unique");
		}

		for(i=0; i<ParameterNames.size(); i++)
		{
			std::string ParamName = ParameterNames[i];

			//check if parameter name is not empty
			if (ParamName.empty())
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Parameter name may not be empty");

			//check if parameter name doesn't contain illegal characters
			if (ParamName.find_first_of(IllegalChars) != std::string::npos)
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Parameter name '"+ParamName+"' contains illegal character(s)\n"+
				                          "Illegal characters are "+IllegalChars+" and blank");

			//check if parameter name is none of the logical operators
			std::string VarToUpper = ToUpper(ParamName);
			if ((VarToUpper == "OR") || (VarToUpper == "AND") || (VarToUpper=="NOT"))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Parameter name may not be a logical operator ('"+ParamName+"')\n");

			//check if param_i<>ElemFunction_k
			if(_elemFunctions->Exists(VarToUpper))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Parameter name must be different from all standard functions ('"+ParamName+"')\n"+
				                          "Available standard functions are: "+_elemFunctions->GetList());

			//check if param_i<>ElemConst_k
			if(_constants.Exists(VarToUpper))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Parameter name must be different from all standard constants ('"+ParamName+"')\n"+
				                          "Available standard constants are: "+_constants.GetList());

			//check if variable name is not numeric
			if(IsNumeric(ParamName))
				throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
				                          "Parameter name may not be numeric ('"+ParamName+"')\n");

			//check if parameter names are different
			for(j=i+1; j<ParameterNames.size(); j++)
				if(ParamName == ParameterNames[j])
					throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
					                          "Parameter name '"+ParamName+"' is not unique");
		}

		//check if var_i<>param_j
		for(i=0; i<VariableNames.size(); i++)
			for(j=0; j<ParameterNames.size(); j++)
				//check if var_i<>param_j
				if(VariableNames[i] == ParameterNames[j])
					throw FuncParserErrorData(FuncParserErrorData::err_BADARG, ERROR_SOURCE,
					                          "Variable and parameter have the same name ('"+VariableNames[i]+"')");

	}
	catch(FuncParserErrorData &)
	{
      throw;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}
}

bool FuncParser::IsScientificNumber (const std::string & SubExpr, size_t OpPos)
{
    if ((OpPos>=2) && (OpPos<=SubExpr.length()-2))
	{
		char PreChar1 = SubExpr[OpPos-1];
		char PreChar2 = SubExpr[OpPos-2];
		char PostChar = SubExpr[OpPos+1];

		//check if the Substring is of the form *dE�d* (or *de�d*), where
		// <d> is any digit and
		// <�> is + or - (this is checked by caller!)
		if( ((PreChar1=='E') || (PreChar1=='e')) &&
		     (PreChar2>='0') && (PreChar2<='9')  &&
		     (PostChar>='0') && (PostChar<='9'))
		{
			const std::string Delimiters = "+-*\\/^,<>=!()[]{}'"+conNOTSymbol+conANDSymbol+conORSymbol;
			long i;

			//search for the first non-numeric character BEFORE and check if it is any delimiter
			for(i = (long)OpPos-3; i >= 0; i--)
			{
				PreChar1 = SubExpr[i];
				if ( (PreChar1=='.') || ((PreChar1>='0') && (PreChar1<='9')))
					continue;
				if ((long)Delimiters.find_first_of(PreChar1) >= 0)
					break;
				else
					return false;  //1st non-numeric char is NOT a delimiter ==> string is not a number in scientific notation
			}

			//search for the first non-numeric character AFTER and check if it is any delimiter
            for (i = (long)OpPos+2; i<(long)SubExpr.length(); i++)
			{
				PostChar = SubExpr[i];
				if ((PostChar>='0') && (PostChar<='9'))
					continue;
				if ((long)Delimiters.find_first_of(PostChar) >= 0)
					break;
				else
					return false; //1st non-numeric char is NOT a delimiter ==> string is not a number in scientific notation
			}

			return true;
		}
		else
			return false;
	}
	else
		return false;
}

void FuncParser::RearrangeTerms (std::string & SubExpr, enmLevelOfAbstraction LevelOfAbstraction, const std::string & Op1,
                                 const std::string & Op2)
{
	const char * ERROR_SOURCE = "FuncParser::RearrangeTerms";

	try
	{
		std::vector<std::string> Op1_Terms;
		std::vector<std::string> Op2_Terms;
		std::string NextTerm;
		size_t FirstPos = 0;
		std::string NewOp, PreviousOp;
		bool FirstTerm = true;
        size_t i;

		// parse expression and put the terms in 2 lists depending on their operand
		while(true)
		{
			NextTerm = GetNextTerm(SubExpr, LevelOfAbstraction, Op1, Op2, FirstPos, NewOp);

			if (FirstTerm)
			{
				// 1st term is automatically in Op1-List
				Op1_Terms.push_back(NextTerm);
				FirstTerm = false;
			}
			else
			{
				// put current term in the list depending on operand BEFORE it (<PreviousOp>)
				if (PreviousOp == Op1)
					Op1_Terms.push_back(NextTerm);
				else
					Op2_Terms.push_back(NextTerm);
			}

			// check if last term and exit loop if so
			if (NewOp.empty())
				break;

			// remember current operand for the next loop cycle
			PreviousOp = NewOp;
		}

		// check if rearrangement required (at least one <Op2> found)
		if (Op2_Terms.empty())
			return;

		// ------------------------------------------------
		// rearrange items so that only single <Op2> occurs
		// ------------------------------------------------

		// Add <Op1>-Terms
		SubExpr = Op1_Terms[0];
		for (i=1; i<Op1_Terms.size(); i++)
			SubExpr += Op1 + Op1_Terms[i];

		//Add <Op2>-Terms
		// check if only one <Op2> available (in this case, no additional bracketing is required)
		bool SingleOp2 = (Op2_Terms.size() == 1);
		if (SingleOp2)
			SubExpr += Op2;
		else
			SubExpr += Op2 + "(";
		for(i=0; i<Op2_Terms.size()-1; i++)
			SubExpr += Op2_Terms[i] + Op1;
		SubExpr += Op2_Terms[Op2_Terms.size() - 1];
		if(!SingleOp2)
			SubExpr += ")";
	}
	catch(FuncParserErrorData & ED)
	{
      adjustErrorDescription(ED, SubExpr);
      throw ED;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}
}

std::string FuncParser::GetNextTerm (const std::string & SubExpr, enmLevelOfAbstraction LevelOfAbstraction, const std::string
                                     & Op1, const std::string & Op2, size_t & FirstPos, std::string & NewOp)
{
	// ______________________________________________________________________________________________________________________________
	// Parses the string <SubExpr> starting at <FirstPos> till <Op1> or <Op2> is found (or the end of the string is reached)
	// and returns next term in the expression .
	// Returns empty string if no more terms available.
	//
	// <NewOp> returns reached operand (<Op1> or <Op2>) or empty string for last term.
	//
	// <FirstPos> is adjusted to the start position of the next term at the end of the function
	// ______________________________________________________________________________________________________________________________

	const char * ERROR_SOURCE = "FuncParser::GetNextTerm";

	std::string NextTerm;    // next found term (return value)

	try
	{
		size_t LastPos = FirstPos;   //index of the next character to be analyzed
		long BracketsCount = 0;    // No. of brackets in the expression
		std::string NextCharacter; // next char to be investigated

		// check expression is not empty
		if (SubExpr.length() == 0)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Empty expression");

		// reset next operation to be returned (return empty value for last term)
		NewOp = "";

		// if <FirstPos> is beyond the end of expression - return empty string
        if(FirstPos >= SubExpr.length())
			return "";

		// go along the expression <SubExpr> till <Op1> or <Op2> is found (or the end of the string is reached)
		while(true)
		{
			// check if the end of the string was not reached
            if(LastPos >= SubExpr.length())
				break;

			// read next char and analyze it
			NextCharacter = SubExpr.substr(LastPos,1);
			if (NextCharacter == "(")
				BracketsCount++;
			else if(NextCharacter == ")")
				BracketsCount--;
			else if (((NextCharacter == Op1) || (NextCharacter == Op2)) && (BracketsCount == 0))
			{
				//if it's not a number in scientific notation - stop
				//<IsScientificNumber> checks NOT whether +/- is reached, so check it here
				if (! ((LevelOfAbstraction == LOA_PLUSMINUS) && IsScientificNumber(SubExpr,LastPos)))
				{
					// set return value for the operand reached
					NewOp = NextCharacter;
					break;
				}
			}
			LastPos++;
		}

		// check that expression does not end with an operand
        if (LastPos == SubExpr.length()-1)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Expression ends with "+NextCharacter);

		// check [No. of opening brackets = No. of closing brackets]
		if (BracketsCount != 0)
			throw FuncParserErrorData(FuncParserErrorData::err_PARSE, ERROR_SOURCE,
			                          "Wrong bracketing");

		// get next term
		NextTerm = SubExpr.substr(FirstPos, LastPos-FirstPos);

		// let <FirstPos> point to the 1st character of the next term (required for the next call)
		FirstPos = LastPos + 1;
	}
	catch(FuncParserErrorData & ED)
	{
      adjustErrorDescription(ED, SubExpr);
      throw ED;
	}
	catch(...)
	{
		throw FuncParserErrorData(FuncParserErrorData::err_RUNTIME, ERROR_SOURCE, "Unknown error");
	}

	// return found term
	return NextTerm;
}

void FuncParser::adjustErrorDescription(FuncParserErrorData & ED, const std::string & subExpression)
{
   if (ED.GetNumber() == FuncParserErrorData::err_PARSE)
   {
      ED.SetNumber(FuncParserErrorData::err_RUNTIME); //to avoid multiple adjusting in case of nested exceptions 
      ED.SetDescription("Error parsing substring '" + ParsedStringToDisplayString(subExpression) + "'\n" + ED.GetDescription() +
         "\n" + "String to parse: '" + _stringToParse + "'");
   }
}

//-------------- C interface for PInvoke -----------------------------------------
bool IsValidVariableOrParameterName(const char* name, char** errorMessage)
{
   bool success = false;

   try
   {
      FuncParser funcParser;

      if(name==NULL)
      {
         *errorMessage = MarshalString("Variable name may not be empty");
         return false;
      }

      //will throw an exception if the given name is not valid
      funcParser.CheckVarParamNames({name}, {});

      *errorMessage = MarshalString("");
      success = true;
   }
   catch (FuncParserErrorData& ED)
   {
      *errorMessage = ErrorMessageFrom(ED);
      success = false;
   }
   catch (...)
   {
      *errorMessage = ErrorMessageFromUnknown("IsValidVariableOrParameterName");
      success = false;
   }

   return success;
}

}//.. end "namespace FuncParserNative"

#ifdef _WINDOWS
extern "C" BOOL WINAPI DllMain(HINSTANCE const instance, DWORD const reason, LPVOID const reserved)
{
   switch (reason)
   {
      case DLL_PROCESS_ATTACH:
         // Initialize once for each new process.
         // Return FALSE to fail DLL load.
         break;

      case DLL_THREAD_ATTACH:
         // Do thread-specific initialization.
         break;

      case DLL_THREAD_DETACH:
         // Do thread-specific cleanup.
         break;

      case DLL_PROCESS_DETACH:
         // Perform any necessary cleanup.
         try
         {
            delete FuncParserNative::ElemFunctions::GetInstance();
         }
         catch(...){}
         break;
   }
   return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
#endif
