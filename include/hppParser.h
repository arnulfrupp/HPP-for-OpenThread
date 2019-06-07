/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppParser.h												            */
/* ----------------------------------------------------------------------------	*/
/* Halloween++ (H++) Parser										            	*/
/*																	            */
/*   - Core parser code 											            */
/*   - Build in math, string and enconding functions				          	*/
/*   - Build in methodes for methods for memory management and var enumeration	*/
/* ----------------------------------------------------------------------------	*/
/* Platform: POSIX and alike											        */
/* Dependencies: hppVarStorage.h										        */
/* Dependencies: int is expected to be int32_t. Verify hppAtoX and hppXXtoA.    */
/* ----------------------------------------------------------------------------	*/
/* Copyright (c) 2018 - 2019, Arnulf Rupp							            */
/* arnulf.rupp@web.de												            */
/* All rights reserved.												            */
/* 	                                                                            */
/* Redistribution and use in source and binary forms, with or without           */
/* modification, are permitted provided that the following conditions are met:  */
/* 	                                                                            */
/* 1. Redistributions of source code must retain the above copyright notice,    */
/* this list of conditions and the following disclaimer.                        */
/* 	                                                                            */
/* 2. Redistributions in binary form must reproduce the above copyright notice, */
/* this list of conditions and the following disclaimer in the documentation    */
/* and/or other materials provided with the distribution.                       */
/* 	                                                                            */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  */
/* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    */
/* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   */
/* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE    */
/* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          */
/* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         */
/* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     */
/* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      */
/* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      */
/* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF       */
/* THE POSSIBILITY OF SUCH DAMAGE.                                              */
/* ----------------------------------------------------------------------------	*/

#ifndef __INCL_HPP_PARSER_
#define __INCL_HPP_PARSER_

#include <stdbool.h>
#include <stdint.h>


#define HPP_ERROR_CODE_MIN 200              // Programatic Errors, not including timeout
#define HPP_CALL_FUNCTION_NAME_MAX_LEN 20   // maximum lenght of function names (for error reporting only)

#define HPP_PARAM_PREFIX "%04x:param1"
#define HPP_PARAM_PREFIX_LEN 11

#define HPP_EXP_STACK_SIZE 800


enum hppReturnReasonEnum { hppReturnReason_None, hppReturnReason_Return, hppReturnReason_Break, hppReturnReason_Continue, hppReturnReason_EOF =100,
						   hppReturnReason_Timeout =150, hppReturnReason_FatalError = HPP_ERROR_CODE_MIN, 
						   hppReturnReason_Error_BreakWithoutWhile, hppReturnReason_Error_ContinueWithoutWhile, hppReturnReason_Error_ClosingBracketExpected,
						   hppReturnReason_Error_ClosingAngleBracketsExpected, hppReturnReason_Error_UnknownVariable, hppReturnReason_Error_BooleanValueExpected, 
						   hppReturnReason_Error_ClosingSquaredBracketExpected, hppReturnReason_Error_SemicolonExpected, hppReturnReason_Error_InvalidOperator,
						   hppReturnReason_Error_MissingArgument, hppReturnReason_Error_DivisionByZero, hppReturnReason_Error_FirstOperand_BooleanValueExpected,
						   hppReturnReason_Error_SecondOperand_BooleanValueExpected, hppReturnReason_Error_UnknownFunctionName, hppReturnReason_Error_CannotCallMethodOnResult,
						   hppReturnReason_Error_UnknownArrayType, hppReturnReason_Error_OpeningSquaredBracketExpected, hppReturnReason_StructVarNameTooLong,
						   hppReturnReason_Error_UnknownMemberType, hppReturnReason_Error_StackOverflow };


enum hppExternalPollFunctionEventEnum { hppExternalPollFunctionEvent_Begin, hppExternalPollFunctionEvent_Poll, hppExternalPollFunctionEvent_End }; 

struct hppParseExpressionStruct
{
	const char* szCode;
	char* szExpressionStack;
	char* szExpressionStackPointer;
	size_t cbPos;
	unsigned int iCallDepth;
	unsigned int iFunctionCallDepth;
	enum hppReturnReasonEnum eReturnReason;
	char szErrorCallFunctionName[HPP_CALL_FUNCTION_NAME_MAX_LEN + 1];
	size_t cbErrorCallFunctionPos;
	unsigned int uiExternalPollFunctionTickCount;
	uint32_t uiExternalTimer;
};


// Function Pointer for external Librarys 
typedef char* (*hppExternalFunctionType)(char*, char*, const char*, size_t*);
typedef void (*hppExternalPollFunctionType)(struct hppParseExpressionStruct*, enum hppExternalPollFunctionEventEnum);


// Parse H++ code in 'aszCode' and store return value in the variable 'aszResultVarKey'. Returns a pointer to
// the char array assoziated with that variable 'aszResultVarKey'.
// If the string value of 'aszResultVarKey' equals "ReturnWithError", an error text is created in cas of a parse
// error. Otherwise the result in NULL in case of a parse error. 
char* hppParseExpression(const char* aszCode, const char* aszResultVarKey);

// Add an external function library 
bool hppAddExternalFunctionLibrary(hppExternalFunctionType aExternalFunctionType);

// Add an external poll function which shall be called regularly while parsing h++ code
// Returns an existing poll function, if it exists. Otherwhise returns NULL 
hppExternalPollFunctionType hppAddExternalPollFunction(hppExternalPollFunctionType aNewExternalPollFunction);

#endif
