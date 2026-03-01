#ifndef _FuncParser_H_
#define _FuncParser_H_

#include "FuncParser/Constants.h"
#include "FuncParser/ElemFunctions.h"
#include <vector>
// ReSharper disable once CppUnusedIncludeDirective
#include <assert.h>
#include <string_view>
#include "FuncParser/FuncNode.h"

namespace FuncParserNative
{

class FuncNode;
class FuncParserErrorData;

class FuncParser
{	
	private:
		
		//Is used for error message only! Will be set at the beginning of the Parse function
		std::string _stringToParse;
      void adjustErrorDescription(FuncParserErrorData & ED, std::string_view subExpression);
	
	public:
		const static std::string conNOTSymbol;
		const static std::string conORSymbol;
		const static std::string conANDSymbol;
		const static std::string conListDelimiter;
	
	private:
		Constants _constants;
		ElemFunctions * _elemFunctions;
		enum enmLevelOfAbstraction 
		{
			LOA_IF = 1,
			LOA_OR,
			LOA_AND,
			LOA_NOT,
			LOA_Comparison,
			LOA_PLUSMINUS,
			LOA_MULDIV,
			LOA_FACTOR
		};
		std::vector < std::string > _variableNames;
		std::vector < std::string > _parameterNames;
		bool IsNumeric (const std::string & str);
		double StringToDouble (const std::string & str) const;
	
	public:
		static std::string ToUpper (const std::string & Source);
	
	private:
		std::string StringReplace (const std::string & source, const char * find, const char * replace, bool CaseSensitive = true);
		std::string StringReplace (const std::string & source, const std::string & find, const std::string & replace, bool CaseSensitive = true);
		std::string ParsedStringToDisplayString (const std::string & str);
	
	public:
		FuncParser ();
		~FuncParser ();
		const Constants & GetConstants () const;
	
	private:
		bool IsBracketed (std::string_view SubExpr) const;
		std::string_view RemoveBrackets (std::string_view SubExpr) const;
	
	public:
		FuncNode * Parse (const std::string & ParsedString, const std::vector < std::string > & VariableNames, const std::vector < std::string > & ParameterNames, bool CaseSensitive, bool LogicOperatorsAllowed, double ComparisonTolerance, bool LogicalNumericMixAllowed);
	
	private:
		void EvalExpression (FuncNode * SubNode, std::string_view SubExpr, enmLevelOfAbstraction LevelOfAbstraction);
		void EvalNOTOperand (FuncNode * SubNode, std::string_view SubExpr);
		void EvalComparison (FuncNode * SubNode, std::string_view SubExpr);
		void EvalFactor (FuncNode * SubNode, std::string_view SubExpr);
		void EvalIF (FuncNode * SubNode, std::string_view SubExpr);
		bool IsScientificNumber (std::string_view SubExpr, size_t OpPos);
	
	public:
		void CheckVarParamNames (const std::vector < std::string > & VariableNames, const std::vector < std::string > & ParameterNames);

		//Moves terms of the expression so that max. one occurrence of Op2 appears:
		//e.g. a-b+c-d => a+c-(b+d)
		//        x/y/z/t => x/(y*z*t)
		void RearrangeTerms (std::string & SubExpr, enmLevelOfAbstraction LevelOfAbstraction, const std::string & Op1,
                           const std::string & Op2);

		//Parses the string <SubExpr> starting at <FirstPos> till <Op1> or <Op2> is found (or the end of the string is reached) and returns next term in the expression .
		//Returns empty string_view if no more terms available.
		//
		//<NewOp> returns reached operand (<Op1> or <Op2>) or empty string for last term.
		//
		//<FirstPos> is adjusted to the start position of the next term at the end of the function
		std::string_view GetNextTerm (std::string_view SubExpr, enmLevelOfAbstraction LevelOfAbstraction, const std::string &
                               Op1, const std::string & Op2, size_t & FirstPos, std::string & NewOp);
};

//-------------- C interface for PInvoke -----------------------------------------
extern "C"
{
   FUNCPARSER_EXPORT bool IsValidVariableOrParameterName(const char* name, char** errorMessage);
}

}//.. end "namespace FuncParserNative"


#endif //_FuncParser_H_

