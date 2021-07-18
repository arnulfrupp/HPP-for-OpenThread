/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppParser.c												            */
/* ----------------------------------------------------------------------------	*/
/* Halloween++ (H++) Parser										            	*/
/*																	            */
/*   - Core parser code 											            */
/*   - Build in math, string and enconding functions				          	*/
/*   - Build in methodes for methods for memory management and var enumeration	*/
/* ----------------------------------------------------------------------------	*/
/* Platform: POSIX and alike											        */
/* Dependencies: hppVarStorage.c										        */
/* Dependencies: int is expected to be int32_t. Verify hppAtoX and hppXXtoA.    */
/* ----------------------------------------------------------------------------	*/
/* Copyright (c) 2018 - 2021, Arnulf Rupp							            */
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

#include "../include/hppParser.h"
#include "../include/hppVarStorage.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <math.h>


#define HPP_EXP_MAX_LEN 80    				// maximum lenght of variable names, numerics and strings

#define HPP_EXP_LOCAL_VAL_PREFIX "%04x:"
#define HPP_EXP_LOCAL_VAL_PREFIX_LEN 5
#define HPP_SECOND_OP_PREFIX "%04x:2ndOp"
#define HPP_SECOND_OP_PREFIX_LEN 10
#define HPP_CALL_FUNCTION_NAME_MAX_LEN 20   // maximum lenght of function names (for error reporting only)
#define HPP_EXTERNAL_FUNCTION_LIBRARY_COUNT 5

#define HPP_EXTERNAL_POLL_FUNCTION_TICK_COUNT 25

// Control Status
enum hppControlStatusEnum { hppControlStatus_None, hppControlStatus_If, hppControlStatus_While, hppControlStatus_Repeat, hppControlStatus_Else, hppControlStatus_Done };

// Static variables
static hppExternalFunctionType hppExternalFunctions[HPP_EXTERNAL_FUNCTION_LIBRARY_COUNT];
static hppExternalPollFunctionType hppExternalPollFunction = NULL;


#ifdef __arm__
#define __hpp_packed __packed            
#else
#define __hpp_packed
#endif
	
// Returns the next operature in 'aszCode' after 'achPos' using a letter code for double character operators.
// Sets 'apcbPos' to the position behind the operator.  
// This function does NOT verify if parameters are NULL pointers! 
char hppGetOperator(struct hppParseExpressionStruct* apParseContext)
{
	char chTerm;
	size_t cbPos = apParseContext->cbPos;
	const char *szCode = apParseContext->szCode;

	// ignore whitspaces before the terminating operator
	while(isspace((int)szCode[cbPos])) cbPos++;

	// Remember operator which terminated the expression and move the cbPos index behind the terminating delimiter (umnless it was null) 
	chTerm = szCode[cbPos];
	if(chTerm != 0) cbPos++;

	// Check if terminator is a double character operator
	if(szCode[cbPos] == chTerm)
	{
		switch(chTerm)
		{
			case '&': chTerm = 'A'; cbPos++; break;
			case '|': chTerm = 'O'; cbPos++; break;
			case '+': chTerm = 'I'; cbPos++; break;
			case '-': chTerm = 'D'; cbPos++; break;
			case '=': chTerm = 'E'; cbPos++; break;
			case ':': chTerm = 'M'; cbPos++; break;
		}
	}
	else if(szCode[cbPos] == '=')
	{
		switch(chTerm)
		{
			case '>': chTerm = 'G'; cbPos++; break;
			case '<': chTerm = 'S'; cbPos++; break;
			case '!': chTerm = 'U'; cbPos++; break;
		}
	}

	apParseContext->cbPos = cbPos;
	if(chTerm == 0) apParseContext->eReturnReason = hppReturnReason_EOF;
	return chTerm;
}


// Get next variable name, function name, number or string literal in 'apParseContext->szCode' after 'apParseContext->cbPos' and copy it in 'aszExpresssion'.
// Sets 'apParseContext->cbPos' to the position behind the terminating operator and reports if the expression a value in 'abNum_Out'.
// Returns the terminating operature using a letter code for double character operators.
// This function does NOT verify if parameters are NULL pointers! 
char hppGetExpression(struct hppParseExpressionStruct* apParseContext, char* aszExpresssion, bool* abValue_Out, size_t achMaxLen)
{
	size_t cbValue;
	size_t cbPos = apParseContext->cbPos;
	size_t cbLen;
	bool bValue_Out_Int = false;
	char chTerm;
	const char* szCode = apParseContext->szCode;

	if(abValue_Out != NULL) *abValue_Out = true;
	else abValue_Out = &bValue_Out_Int;                   // Do not allow values. E.g. begin '.' operator: names by start with a number.
	
	// ignore whitspaces before the terminating operator
	while(isspace((int)szCode[cbPos])) cbPos++;
	
	// ignore comment lines
	while(szCode[cbPos] == '/')
	{
		if(szCode[cbPos + 1] == '/')
		{
			cbPos++;
			while(szCode[cbPos] != 10 && szCode[cbPos] != 0) cbPos++;
			while(isspace((int)szCode[cbPos])) cbPos++;
		}
	}
	
	apParseContext->cbPos = cbPos;
	cbValue = cbPos;


	// Read string value
	if(szCode[cbPos] == '\"' || szCode[cbPos] == '\'')
	{
		cbValue++;
		while(szCode[cbValue] != szCode[cbPos] && szCode[cbValue] != 0) cbValue++;
	}
	else
	{
		if(strncmp(szCode + cbPos, "--", 2) != 0)
		{ 
			if(memchr("1234567890.-", szCode[cbPos], 12) != NULL && *abValue_Out == true)
				while(memchr("1234567890.", szCode[++cbValue], 11) != NULL);
			else
				while(isalnum((int)szCode[cbPos]) || szCode[cbPos] == '_') cbPos++;
		}
	}

	// Check if the expression is numeric
	if(cbValue >= cbPos) cbPos = cbValue;
	else *abValue_Out = false;

	// Store expression in 'aszExpresssion'
	cbLen = cbPos - apParseContext->cbPos;

	// ignore leading and trailing quotation mark in case of string literal
	if(szCode[apParseContext->cbPos] == '\"' || szCode[apParseContext->cbPos] == '\'') { cbLen--; (apParseContext->cbPos)++; cbPos++; }

	if(cbLen > achMaxLen) cbLen = achMaxLen;	
	memcpy(aszExpresssion, szCode + apParseContext->cbPos, cbLen);
	aszExpresssion[cbLen] = 0;
	
	// Return position before the terminating character
	apParseContext->cbPos = cbPos;
	
	if(*abValue_Out == false && strcmp(aszExpresssion, "return") == 0) chTerm = ' '; 
	else chTerm = hppGetOperator(apParseContext);
	
	if(aszExpresssion[0] == 0)      // Process leading operators   	
	{  
		switch(chTerm) 
		{ 
			case '(': *abValue_Out = false; break;   // leading '('
			case '{': chTerm = 'B'; *abValue_Out = false; break;  // leading '{'
			case 'I': chTerm = 'i'; *abValue_Out = false; break;  // leading '++'
			case 'D': chTerm = 'd'; *abValue_Out = false; break;  // leading '--'
			case '<': chTerm = 'N'; *abValue_Out = false; break;  // leading '<'
			case '&': chTerm = 'R'; *abValue_Out = false; break;  // leading '&'
			case '?': chTerm = 'Q'; *abValue_Out = false; break;  // leading '?'
		}
	}
		
	return chTerm;
}


// Move index 'apParseContext->cbPos' behind the next expression closed with either ';' or a curly bracket closing the next code block
// Returns the character which ended the expession.
char hppIgnoreExpression(struct hppParseExpressionStruct* apParseContext)
{
	int iBrackets = 0;
	size_t cbPos = apParseContext->cbPos;
	const char* szCode = apParseContext->szCode;
	char chLast, ch = szCode[cbPos];
	
	do
	{		
		while(strchr("\"\'{};/", ch) == NULL) ch = szCode[++cbPos];
		chLast = ch;

		switch(ch)
		{
			case '\'':
			case '\"':  do ch = szCode[++cbPos]; 
		                while(ch != chLast && ch != 0);
						break;
					
			case '/':	if(szCode[cbPos + 1] == '/')       // ignore comments
							do ch = szCode[++cbPos]; 
							while(ch != 10 && ch != 0);
						break;
		
						
			case '{':   iBrackets++; break;
			case '}':   iBrackets--; break;
		}
		
		if(ch != 0) ch = szCode[++cbPos];
		else break;
	}
	while(iBrackets > 0 || (chLast != '}' && chLast != ';'));

	apParseContext->cbPos = cbPos;

	return chLast;
}


// Evaluate Function 'aszFunctionName' with parameters stored in varibales with name passed by 'aszParamName', whereby
// variable names have the format '%04x_Param1'. The byte 'aszParamName[HPP_PARAM_PREFIX_LEN - 1]' may be modifiyed to read the
// respectiv paramter. The hex number '%04x' stands for the hppParseExpression recusive call count and
// guarantees the variable is unique for that fuction (local vaiable).
// The return value of 'aszParamName' is not defined (typically the last parameter used). The result is stored
// in the variable 'aszResultVarKey. Returns a Pointer to the respective byte array of the result variable    
char* hppEvaluateFuction(char aszFunctionName[], char aszParamName[], const char aszResultVarKey[], size_t* apcbResultLen_Out)
{
	char szNumeric[HPP_NUMERIC_MAX_MEM];

	if(strlen(aszFunctionName) == 3)     // restrict search to the right lenght to gain speed
	{ 
		if(strcmp(aszFunctionName, "abs") == 0) 
			return hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, fabs(hppAtoF(hppVarGet(aszParamName, NULL)))), apcbResultLen_Out);
			
		if(strcmp(aszFunctionName, "int") == 0) 
			return hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoI(hppVarGet(aszParamName, NULL))), apcbResultLen_Out);
		
		if(strcmp(aszFunctionName, "val") == 0)  // just bring a value in a defined format e.g. for comparison with '=='
			return hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoF(hppVarGet(aszParamName, NULL))), apcbResultLen_Out);
		
		if(strcmp(aszFunctionName, "sin") == 0) 
			return hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, sin(hppAtoF(hppVarGet(aszParamName, NULL)))), apcbResultLen_Out);

		if(strcmp(aszFunctionName, "cos") == 0) 
			return hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, cos(hppAtoF(hppVarGet(aszParamName, NULL)))), apcbResultLen_Out);
		
		if(strcmp(aszFunctionName, "tan") == 0) 
			return hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, tan(hppAtoF(hppVarGet(aszParamName, NULL)))), apcbResultLen_Out);
	}
			
	if(strcmp(aszFunctionName, "writeln") == 0)
	{
		char* szText = hppVarGet(aszParamName, NULL);
		if(szText == NULL) szText = (char*)"";
		printf("%s\n",  szText);
		return hppVarPutStr(aszResultVarKey, szText, apcbResultLen_Out);
	}
	
	if(strcmp(aszFunctionName, "struct") == 0)
	{
		char* pchStruct = NULL;
		char* szFormat = hppVarGet(aszParamName, NULL);
		*apcbResultLen_Out = hppGetStructHeaderLenght(szFormat);
		if(*apcbResultLen_Out > 0) pchStruct = hppVarPut(aszResultVarKey, hppNoInitValue, *apcbResultLen_Out); 
		hppGetStructHeader(pchStruct, szFormat);   // does not do anything if a parameter is NULL
		
		return pchStruct;
	}		

	return NULL;
}


// As above but for method calls -> aszFunctionName has a "." inside as a result of '::' operator
char* hppEvaluateMethod(char aszFunctionName[], char aszParamName[], const char aszResultVarKey[], size_t* apcbResultLen_Out)
{
	char szNumeric[HPP_NUMERIC_MAX_MEM];   // 12 numbers => max 19 characters + null byte: -1.23456789012+e123
	char* szMethodName = aszFunctionName;
	char* szMethodTest;
	size_t cbInstanceDataLenght;
	char* pchReturn = NULL;
	char* szInstanceName;
	char* pchInstanceData;
	char* szParam;
	enum hppBinaryTypeEnum theBinaryType;
	int iCount;
	
	// Find method name
	while((szMethodTest = strchr(szMethodName, '.')) != NULL) szMethodName = szMethodTest + 1;	
	szInstanceName = (char *)malloc(szMethodName - aszFunctionName + 1);   // reseve memory for instance name + 1 byte for '.'
	if(szInstanceName == NULL) return NULL; 
	memcpy(szInstanceName, aszFunctionName, szMethodName - aszFunctionName - 1);   // first copy instance name without '.'
	szInstanceName[szMethodName - aszFunctionName - 1] = 0;  		
	pchInstanceData = hppVarGet(szInstanceName, &cbInstanceDataLenght);
			
	switch(*szMethodName)     // look at first character to speed up search and verify full name later  
	{
		case 'l':	if(strcmp(szMethodName, "len") != 0) break;
					pchReturn = hppVarPutStr(aszResultVarKey, pchInstanceData != NULL ? hppI2A(szNumeric, cbInstanceDataLenght) : "-1", apcbResultLen_Out);
					break;
					
		case 'c':	if(strcmp(szMethodName, "count") != 0) break;
					pchReturn = hppVarPutStr(aszResultVarKey, hppI2A(szNumeric, hppGetStructRecordCount(pchInstanceData, cbInstanceDataLenght, true)), apcbResultLen_Out);
					break;
					
		case 't':	if(strcmp(szMethodName, "typeof") != 0) break;	
					szParam = hppVarGet(aszParamName, NULL);
					theBinaryType = hppBinaryType_unvalid;
					if(szParam != NULL && pchInstanceData != NULL) hppGetMemberFromStruct(pchInstanceData, cbInstanceDataLenght, szParam, &theBinaryType, NULL);
					pchReturn = hppVarPutStr(aszResultVarKey, theBinaryType != hppBinaryType_unvalid ? hppTypeNameList[theBinaryType] : "unvalid", apcbResultLen_Out);
					break;
					
		case 'v':	if(strcmp(szMethodName, "vars_count") == 0)   // there is another method with 'v' --> not break at the end of this section
					{
						strcat(szInstanceName, ".");  // Add '.' behind for the search of sub items
						iCount = hppVarCount(szInstanceName, true);
						pchReturn = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, iCount), apcbResultLen_Out);
						break;
					}
					
		case 'r':	if(strcmp(szMethodName, "vars") == 0 || strcmp(szMethodName, "roots") == 0)	  // there is another method with 'r' --> not break at the end of this section					
					{
						strcat(szInstanceName, ".");  // Add '.' behind for the search of sub items
						hppVarPutStr(aszResultVarKey, "error", apcbResultLen_Out);   // make sure the result key already exists in case it is itself part of the enumeration
						iCount = hppVarGetAll(NULL, 0, szInstanceName, *szMethodName =='v' ? hppVarGetAllEndNodes : hppVarGetAllRootNodes);
						if(iCount >= 0)
						{
							pchReturn = hppVarPut(aszResultVarKey, hppNoInitValue, iCount);
							hppVarGetAll(pchReturn, iCount + 1, szInstanceName, *szMethodName =='v' ? hppVarGetAllEndNodes : hppVarGetAllRootNodes);
							*apcbResultLen_Out = iCount;
						}
						break;
					}
					else if(strcmp(szMethodName, "replace") == 0)
					{
						char* pchSource;
						size_t cbSourceLen;
						
						iCount = hppAtoI(hppVarGet(aszParamName, NULL));
						if(iCount < 0) iCount = 0;
						aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
						pchSource = hppVarGet(aszParamName, &cbSourceLen);
						pchReturn = pchInstanceData;
						
						if(iCount + cbSourceLen > cbInstanceDataLenght)   // check if size must be increased
							pchReturn = hppVarPut(szInstanceName, hppNoInitValue, iCount + cbSourceLen);
							
						if(pchReturn != NULL) 
						{	
							memcpy(pchReturn + iCount, pchSource, cbSourceLen);	 // insert
							if(((size_t)iCount) > cbInstanceDataLenght) 
								memset(pchReturn + cbInstanceDataLenght, ' ', ((size_t)iCount) - cbInstanceDataLenght); // fill inbetween spaces
							
							*apcbResultLen_Out = ((size_t)iCount) + cbSourceLen;
							pchReturn = hppVarPut(aszResultVarKey, pchReturn, *apcbResultLen_Out);
						}  
						else pchReturn = hppVarPutStr(aszResultVarKey, "", apcbResultLen_Out);
					}
					
		case 'a':   if(strcmp(szMethodName, "alloc") == 0 || strcmp(szMethodName, "realloc") == 0)
					{
						iCount = hppAtoI(hppVarGet(aszParamName, NULL));
						if(iCount < 0) iCount = 0;
						pchReturn = hppVarPut(szInstanceName, szMethodName[0] == 'a' ? hppInitValueWithZero : hppNoInitValue, iCount);
						pchReturn = hppVarPutStr(aszResultVarKey, pchReturn != NULL ? "true" : "false", apcbResultLen_Out);
					}
					break;
	
		case 'i':	if(strcmp(szMethodName, "item") != 0) break;
					iCount = hppAtoI(hppVarGet(aszParamName, NULL));
			
					if(pchInstanceData != NULL && iCount >= 0)
					{	
						for(int i = 0; i < iCount && pchInstanceData != (char*)1; i++) pchInstanceData = strchr(pchInstanceData, ',') + 1;
						if(pchInstanceData != (char*)1)
						{ 
							pchReturn = pchInstanceData; 
							pchInstanceData = strchr(pchInstanceData, ',');
							if(pchInstanceData != NULL) *apcbResultLen_Out = pchInstanceData - pchReturn;
							else *apcbResultLen_Out = strlen(pchReturn);
							pchReturn = hppVarPut(aszResultVarKey, pchReturn, *apcbResultLen_Out);
						}
						else pchReturn = hppVarPutStr(aszResultVarKey, "", apcbResultLen_Out);  // Index behind last element
					}
					else pchReturn = hppVarPutStr(aszResultVarKey, "", apcbResultLen_Out);  // Instance not found									
					break;
					
		case 'f':	if(strcmp(szMethodName, "find") != 0) break;
					szParam = hppVarGet(aszParamName, NULL);
					iCount = -1;
					if(szParam != NULL && pchInstanceData != NULL) 
					{
						char* pchFound = strstr(pchInstanceData, szParam);
						if(pchFound != NULL) iCount = pchFound - pchInstanceData;
					}
					
					pchReturn = hppVarPutStr(aszResultVarKey, hppI2A(szNumeric, iCount), apcbResultLen_Out);
					break;
								
		case 's':	if(strcmp(szMethodName, "sub") != 0) break;
					iCount = hppAtoI(hppVarGet(aszParamName, NULL));     // start from iCount
					aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
					szParam = hppVarGet(aszParamName, NULL);             // optional: maximum length
					
					if(pchInstanceData == NULL) pchReturn = hppVarPutStr(aszResultVarKey, "", apcbResultLen_Out);
					else
					{
						if(szParam != NULL) *apcbResultLen_Out = hppAtoI(szParam);
						if((unsigned int)iCount > cbInstanceDataLenght) iCount = cbInstanceDataLenght;  
						if(szParam == NULL || *apcbResultLen_Out > cbInstanceDataLenght - iCount) *apcbResultLen_Out = cbInstanceDataLenght - iCount;
						pchReturn = hppVarPut(aszResultVarKey, pchInstanceData + iCount, *apcbResultLen_Out);
					} 
					break; 
	}
						
	free(szInstanceName);
	return pchReturn;
}


// Parse H++ code in 'apParseContext->szCode' from 'apParseContext->cbPos' on. Store result in the variable 'aszResultVarKey' and
// return a pointer to the char array assoziated with that valiable. The length of the result array
// is stored in 'apcbResultLen_Out' (unless NULL). Parsing stops once on operation contained in   
// 'aszTerminatingOperators' is hit. The operator actually terminating parsing is stored in 
// 'apchTerminatingOperator_Out' (unless NULL). The parameter 'aiCallDepth' tracks the numner of sub-routine calls
// and supports creation of local variables by adding call count to the key of the variable name key.  
char* hppParseExpressionInt(struct hppParseExpressionStruct* apParseContext, const char* aszResultVarKey,
						   size_t* apcbResultLen_Out, const char* aszTerminatingOperators, char* apchTerminatingOperator_Out)
{
	bool isValue;
	char chTerm = 32;
	char* pchResult = NULL; 
	size_t cbResultLen;
	char* szExpresssion;
	char* szExpresssionWithPrefix;
	char* szExpressionStackPointerBuf = apParseContext->szExpressionStackPointer;   // Buffer stack level of calling function

	//szExpresssionWithPrefix = (char*) malloc(HPP_EXP_MAX_LEN + HPP_EXP_LOCAL_VAL_PREFIX_LEN + 1);
	apParseContext->szExpressionStackPointer += strlen(szExpressionStackPointerBuf) + 1;  // Put stack pointer behind present expression
	szExpresssionWithPrefix = apParseContext->szExpressionStackPointer;
	if(apParseContext->szExpressionStack + HPP_EXP_STACK_SIZE - HPP_EXP_MAX_LEN - HPP_EXP_LOCAL_VAL_PREFIX_LEN - 1 < szExpresssionWithPrefix)
	{
		apParseContext->eReturnReason = hppReturnReason_Error_StackOverflow;
		return NULL;
	}
	
	//if(szExpresssionWithPrefix == NULL || aszResultVarKey == NULL) return NULL;
	if(aszResultVarKey == NULL) return NULL;
    sprintf(szExpresssionWithPrefix, HPP_EXP_LOCAL_VAL_PREFIX, apParseContext->iFunctionCallDepth); 

	apParseContext->iCallDepth++;
	if(hppExternalPollFunction != NULL && ++apParseContext->uiExternalPollFunctionTickCount == HPP_EXTERNAL_POLL_FUNCTION_TICK_COUNT)
	{
		hppExternalPollFunction(apParseContext, hppExternalPollFunctionEvent_Poll);
		apParseContext->uiExternalPollFunctionTickCount = 0;
	}

	while(strchr(aszTerminatingOperators, chTerm) == NULL && apParseContext->eReturnReason == hppReturnReason_None)
	{
		szExpresssion = szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN;   // Write expression behind the prefix such the prefix can be used selectively
		chTerm = hppGetExpression(apParseContext, szExpresssion, &isValue, HPP_EXP_MAX_LEN);
		if(apParseContext->eReturnReason == hppReturnReason_EOF)
		{ 
			if(pchResult != NULL) break;  // Preserve the result of the last command if hppParseExpressionInt has reached the end of the code
			else  apParseContext->eReturnReason = hppReturnReason_Error_SemicolonExpected;
		}
		
		pchResult = NULL;  	// All code paths should assign pchResult
		
		if(isValue)  
		{ 
			cbResultLen = strlen(szExpresssion);
			pchResult = hppVarPut(aszResultVarKey, szExpresssion, cbResultLen); 
		}
		else 
		{	 
			// Unitary opertors not evaluating the expression keeping 'aszResultVarKey' unchanged
			if(islower((int)szExpresssion[0])) szExpresssion = szExpresssionWithPrefix;  // local variable or function 

			// operators '::', '<' , '[', '.', ':type[' or leading '?'
			// Some code branches use 'aszResultVarKey' to temporarily store parameter values. 
			// This loop end if a code branch calculates a value for further processing and assignes pchResult  
			while(strchr("MN[.Q:", chTerm) != NULL && apParseContext->eReturnReason == hppReturnReason_None && pchResult == NULL)
			{
				size_t cbLen;
				char* pchParam;
				
				// Parameters for binary data access with '[' operator
				size_t iIndex = 0;            	// Index from value between [] operatores. Zero if not present
				size_t cbOffset = 0;            // No intital offset. May come from '.' operator
				size_t nSizeof = 1;             // Standard type size for [ operator;  may be overwritten by : operator
				size_t nIndexMultiplier = 1;    // Must be same as nSizeof or 1 depending on the type addressing  
				enum hppBinaryTypeEnum nTypeID = hppBinaryType_uint8;  // uint8 --> unsigned char
				char* pchArray;

				switch(chTerm)
				{
								// add text to the expression name behind '.' (member or method)
					case 'M':  	cbLen = strlen(szExpresssion);   // The below limit to HPP_EXP_MAX_LEN does not consider extra available bytes for the local var prefix (uncritcal)
								if(cbLen < HPP_EXP_MAX_LEN) strcat(szExpresssion, ".");
								cbLen++;
								chTerm = hppGetExpression(apParseContext, szExpresssion + cbLen, NULL, HPP_EXP_MAX_LEN - cbLen);
								break;
					
								// add evaluated string to the expression name
					case 'N':  	cbLen = strlen(szExpresssion);   // The below limit to HPP_EXP_MAX_LEN does not consider extra available bytes for the local var prefix (uncritcal)
								pchParam = hppParseExpressionInt(apParseContext, aszResultVarKey, NULL , ">;", &chTerm); // Temporary use of result variable
								if(chTerm == '>') 
								{
									strncat(szExpresssion, pchParam, HPP_EXP_MAX_LEN - cbLen);
									chTerm = hppGetOperator(apParseContext);
								}
								else apParseContext->eReturnReason = hppReturnReason_Error_ClosingAngleBracketsExpected;
								break;
								
			   struct_method:				
					case '.':   pchArray = hppVarGet(szExpresssion, &cbResultLen);   // Read array
								if(pchArray != NULL) 
								{
									char szMemberName[HPP_MAX_MEMBER_NAME_LEN + 1];
									chTerm = hppGetExpression(apParseContext, szMemberName, NULL, HPP_MAX_MEMBER_NAME_LEN);
									cbOffset = hppGetMemberFromStruct(pchArray, cbResultLen, szMemberName, &nTypeID, &nIndexMultiplier);
									if(nTypeID == hppBinaryType_unvalid) { apParseContext->eReturnReason = hppReturnReason_Error_UnknownMemberType; break; }
									nSizeof = hppTypeSizeof[nTypeID];
									goto struct_access;
								}
								else apParseContext->eReturnReason = hppReturnReason_Error_UnknownVariable; 
								break;
					
					case ':':   nTypeID = hppGetTypeID(apParseContext->szCode + apParseContext->cbPos);
								if(nTypeID == hppBinaryType_unvalid) { apParseContext->eReturnReason = hppReturnReason_Error_UnknownArrayType; break; }
								nSizeof = nIndexMultiplier = hppTypeSizeof[nTypeID];
								apParseContext->cbPos += hppTypeNameLenght[nTypeID];
								if(apParseContext->szCode[apParseContext->cbPos] == '*') { nIndexMultiplier = 1; apParseContext->cbPos++; } // Bytewise access
								if(apParseContext->szCode[apParseContext->cbPos] == '[') apParseContext->cbPos++;  // Continue with '[' operator
								else { apParseContext->eReturnReason = hppReturnReason_Error_OpeningSquaredBracketExpected; break; }
								
					case '[':   pchParam = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, "]};", &chTerm);
								iIndex = hppAtoI(pchParam);
								if(chTerm != ']') { apParseContext->eReturnReason = hppReturnReason_Error_ClosingSquaredBracketExpected; break; }
								chTerm = hppGetOperator(apParseContext);
								if(chTerm == '.') goto struct_method; // continue with '.' operator, otherwise continue processing the array
								pchArray = hppVarGet(szExpresssion, &cbResultLen);   // Read array
								
			   struct_access:	cbOffset += iIndex * nIndexMultiplier;   // Calcualte offset including array index   
								
								// Increase size to accommodate the new array entry?
								if(cbResultLen < cbOffset + nSizeof) pchArray = hppVarPut(szExpresssion, hppInitValueWithZero, cbOffset + nSizeof);
								if(pchArray == NULL) { apParseContext->eReturnReason = hppReturnReason_FatalError; break; }
						
								// Get new L-value expression from array or structure entry. Length is limited to HPP_VAR_NAME_MAX_LEN.
								if(nTypeID == hppBinaryType_var || nTypeID == hppBinaryType_array || nTypeID == hppBinaryType_string)
								{
									if(*(pchArray + cbOffset) == 0)   // Invent new variable name, based on the name of the base variable, if it was not assigned before
									{
										if(snprintf(pchArray + cbOffset, HPP_VAR_NAME_MAX_LEN + 1, "%s.%d", szExpresssion, (unsigned int)cbOffset) > HPP_VAR_NAME_MAX_LEN)
										{	
											apParseContext->eReturnReason = hppReturnReason_StructVarNameTooLong; 
											break;
										}
									}
									
									strncpy(szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN, pchArray + cbOffset, HPP_VAR_NAME_MAX_LEN + 1);
									szExpresssion = szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN;
									szExpresssion[HPP_VAR_NAME_MAX_LEN] = 0;   // Make sure expressions are always zero terminated
									break;
								}
											
								if(chTerm == '=')  // Write array?
								{
									pchResult = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, ")]};", &chTerm);
									if(pchResult == NULL && apParseContext->eReturnReason == hppReturnReason_None) apParseContext->eReturnReason = hppReturnReason_FatalError;
											
									switch(nTypeID)
									{
										case hppBinaryType_uint8:  	*(unsigned char*)(pchArray + cbOffset) = hppAtoI(pchResult); break;
										case hppBinaryType_int8:   	*(signed char*)(pchArray + cbOffset) = hppAtoI(pchResult); break;
										case hppBinaryType_uint16: 	*(uint16_t*)(pchArray + cbOffset) = hppAtoUI16(pchResult); break;
										case hppBinaryType_int16:  	*(int16_t*)(pchArray + cbOffset) = hppAtoI16(pchResult); break;
										case hppBinaryType_uint32: 	*(uint32_t*)(pchArray + cbOffset) = hppAtoUI32(pchResult); break;
										case hppBinaryType_int32:  	*(int32_t*)(pchArray + cbOffset) = hppAtoI32(pchResult); break;
										case hppBinaryType_bool:  	*(pchArray + cbOffset) = pchResult != NULL ? (strcmp(pchResult, "true") == 0 ? 1 : 0) : 0; break;
										case hppBinaryType_float:  	*(float*)(pchArray + cbOffset) = hppAtoF(pchResult); break;
										case hppBinaryType_double: 	{ 	double d = hppAtoF(pchResult);     // avoid alignment issue for ARM compiler
																		memcpy(pchArray + cbOffset, &d, sizeof(double)); break; 
																	}
					
										case hppBinaryType_fixstr:  if(pchResult != NULL) strncpy(pchArray + cbOffset, pchResult, HPP_FIXSTR_MAX_LEN + 1);
																	*(pchArray + cbOffset + HPP_FIXSTR_MAX_LEN) = 0; // truncate string
																	break;
										default: break;									
									}   
								}
								else       // Read array 
								{
									char szNumeric[HPP_NUMERIC_MAX_MEM];   
					
									switch(nTypeID)
									{
										case hppBinaryType_uint8:  	hppI2A(szNumeric, *(unsigned char*)(pchArray + cbOffset)); break;
										case hppBinaryType_int8:   	hppI2A(szNumeric, *(signed char*)(pchArray + cbOffset)); break;
										case hppBinaryType_uint16: 	hppUI16toA(szNumeric, *(uint16_t*)(pchArray + cbOffset)); break;
										case hppBinaryType_int16:  	hppI16toA(szNumeric, *(int16_t*)(pchArray + cbOffset)); break;
										case hppBinaryType_uint32: 	hppUI32toA(szNumeric, *(uint32_t*)(pchArray + cbOffset)); break;
										case hppBinaryType_int32:  	hppI32toA(szNumeric, *(int32_t*)(pchArray + cbOffset)); break;
										case hppBinaryType_bool:  	strcpy(szNumeric, *(pchArray + cbOffset) == 0 ? "false" : "true"); break;
										case hppBinaryType_float:  	hppD2A(szNumeric, *(float*)(pchArray + cbOffset)); break;
										case hppBinaryType_double: 	{	double d;   // avoid alignment issue for ARM compiler 
																		memcpy(&d, pchArray + cbOffset, sizeof(double));
																		hppD2A(szNumeric, d); break;
																	}
																	
										case hppBinaryType_fixstr:  pchResult = hppVarPutStr(aszResultVarKey, pchArray + cbOffset, &cbResultLen);
										default: *szNumeric = 0;										
									}
								
									if(nTypeID != hppBinaryType_fixstr) pchResult = hppVarPutStr(aszResultVarKey, szNumeric, &cbResultLen);
									if(pchResult == NULL && apParseContext->eReturnReason != hppReturnReason_None) apParseContext->eReturnReason = hppReturnReason_FatalError;
								}
								break;
			
								// Initialze unkonwn variables having a questionmark in front; also works with arrays
					case 'Q':	chTerm = hppGetExpression(apParseContext, szExpresssion, NULL, HPP_EXP_MAX_LEN);
								if(islower((int)szExpresssion[0])) szExpresssion = szExpresssionWithPrefix;  // local variable or function 

								pchParam = hppVarGet(szExpresssion, NULL);
								if(pchParam == NULL) hppVarPutStr(szExpresssion, "", &cbResultLen);
								break;
				}
			}
				
			if(apParseContext->eReturnReason != hppReturnReason_None) break;

			if(pchResult == NULL) // Continue evaluating other L-value operators if no result has been assigned in the loop before  
			{
				char chNewTerm = 32;    // Assign value to a avoid warning
				
				switch(chTerm)    // Unitary opertors evaluating the expression and storing in var 'aszResultVarKey'
				{
					case '(':  	while(isspace((int)apParseContext->szCode[apParseContext->cbPos])) (apParseContext->cbPos)++;    // Ignore white spaces such it can be checked if ')' immediately follows
					
								if(szExpresssion[0] == 0)    // Function call or just brackets?
								{
									pchResult = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, ")", &chTerm);
									if(chTerm !=')' && apParseContext->eReturnReason == hppReturnReason_None) apParseContext->eReturnReason = hppReturnReason_Error_ClosingBracketExpected;
								}
								else
								{
									char szParamName[HPP_PARAM_PREFIX_LEN + 1];
									char *pchLastParam = NULL;
									enum hppControlStatusEnum eControlStatus = hppControlStatus_None;
									size_t cbBoolExpPos = apParseContext->cbPos;
									size_t cbBoolBehindExpPos;
									sprintf(szParamName, HPP_PARAM_PREFIX, apParseContext->iCallDepth);

									if(strcmp(szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN, "if") == 0) eControlStatus = hppControlStatus_If;
									else if(strcmp(szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN, "while") == 0) eControlStatus = hppControlStatus_While;

									do
									{
										apParseContext->cbPos = cbBoolExpPos;   // While may need to reevaluate the condition expression to tell if the loop shall continue  	
										
										if(apParseContext->szCode[apParseContext->cbPos] != ')')    // Verify if there is an argument at all
										{
											do pchLastParam = hppParseExpressionInt(apParseContext, szParamName, NULL, ",)", &chTerm);
											while(chTerm ==',' && ++szParamName[HPP_PARAM_PREFIX_LEN - 1] <='9');
										
											if(chTerm !=')') 
											{
												if(apParseContext->eReturnReason == hppReturnReason_None) apParseContext->eReturnReason = hppReturnReason_Error_ClosingBracketExpected; 
												break;   // Error
											}
										}
										else (apParseContext->cbPos)++;   // move behind the ')'
										
										cbBoolBehindExpPos = apParseContext->cbPos; // Store position behind the logic expression. This is used to igonre the code in case of the code after "continue;"
										szParamName[HPP_PARAM_PREFIX_LEN - 1] = '1';  // Start over with first parameter 
										
										if(eControlStatus == hppControlStatus_None)   // is it a control structure or a fuction call?
										{
											// Build in function or method call?
											if(strchr(szExpresssion, '.') == NULL) 
											{
												size_t iEntry = 0;
												
												pchResult = hppEvaluateFuction(szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN, szParamName, aszResultVarKey, &cbResultLen);
												
												while(pchResult == NULL && iEntry < HPP_EXTERNAL_FUNCTION_LIBRARY_COUNT)
												{
													szParamName[HPP_PARAM_PREFIX_LEN - 1] = '1';  // Start over with first parameter 
													
													if(hppExternalFunctions[iEntry] != NULL) 
														pchResult = (hppExternalFunctions[iEntry])(szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN, szParamName, aszResultVarKey, &cbResultLen);
														
													iEntry++;
												}
											}		
											else pchResult = hppEvaluateMethod(szExpresssion, szParamName, aszResultVarKey, &cbResultLen);
											
											if(pchResult == NULL)  // Not a build in function -> invoke H++ code ? 
											{  
												const char* szCallingCode = apParseContext->szCode;
												size_t cbCallingPosition = apParseContext->cbPos;
												int iCallingFunctionCallDepth = apParseContext->iFunctionCallDepth; 
												 
												apParseContext->szCode = hppVarGet(szExpresssion, NULL);
												// if no non-cap local var or cap global var was found, also try non-cap global var (from PUT or from flash)
												if(apParseContext->szCode == NULL) apParseContext->szCode = hppVarGet(szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN, NULL);
												apParseContext->cbPos = 0;
												apParseContext->iFunctionCallDepth = apParseContext->iCallDepth;
												 
												if(apParseContext->szCode != NULL)
												{
													pchResult = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, "", NULL);
													
													// Store Function Name if error occured and there was no function name stored earlier
													if(apParseContext->eReturnReason >= HPP_ERROR_CODE_MIN && apParseContext->szErrorCallFunctionName[0] == 0)
													{
														strncpy(apParseContext->szErrorCallFunctionName, szExpresssionWithPrefix + HPP_EXP_LOCAL_VAL_PREFIX_LEN, HPP_CALL_FUNCTION_NAME_MAX_LEN);
														// set null byte at the end in case szExpression was too long
														apParseContext->szErrorCallFunctionName[HPP_CALL_FUNCTION_NAME_MAX_LEN] = 0;
														apParseContext->cbErrorCallFunctionPos = apParseContext->cbPos;
													} 	
												}
												else apParseContext->eReturnReason = hppReturnReason_Error_UnknownFunctionName;
												
												apParseContext->szCode = szCallingCode;
												apParseContext->cbPos = cbCallingPosition;
												apParseContext->iFunctionCallDepth = iCallingFunctionCallDepth;
												
												if(apParseContext->eReturnReason == hppReturnReason_EOF ||
												   apParseContext->eReturnReason == hppReturnReason_Return) apParseContext->eReturnReason = hppReturnReason_None;
												   
												if(apParseContext->eReturnReason == hppReturnReason_Break) apParseContext->eReturnReason = hppReturnReason_Error_BreakWithoutWhile;
												if(apParseContext->eReturnReason == hppReturnReason_Continue) apParseContext->eReturnReason = hppReturnReason_Error_ContinueWithoutWhile;				
											}
										}
										else     // control structure 
										{	
											if(pchLastParam == NULL)  // Argument missing?
											{
												apParseContext->eReturnReason = hppReturnReason_Error_MissingArgument;
												eControlStatus = hppControlStatus_Done;
											}	
											else if(strcmp(pchLastParam, "true") == 0) 
											{ 
												apParseContext->eReturnReason = hppReturnReason_None;
												pchResult = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, ";}", &chTerm);  // Temporary use of result variable
												
												if(eControlStatus == hppControlStatus_If) eControlStatus = hppControlStatus_Done;
												else if(apParseContext->eReturnReason == hppReturnReason_Break) 
												{ 
													//chTerm = ';';   // break -> exit loop (done) but continue after the loop    
													apParseContext->cbPos = cbBoolBehindExpPos; // Restore position behind logic expression
													chTerm = hppIgnoreExpression(apParseContext);  // Ingore the following expression or codeblock
													eControlStatus = hppControlStatus_Done;  // End Loop, no else
													apParseContext->eReturnReason = hppReturnReason_None;  // Continue code execusion after loop
												}
												else if(apParseContext->eReturnReason == hppReturnReason_Continue) 
												{ 
													apParseContext->eReturnReason = hppReturnReason_None; // Continue code execusion after loop
													eControlStatus = hppControlStatus_Repeat;
												}
												else if(apParseContext->eReturnReason != hppReturnReason_None) eControlStatus = hppControlStatus_Done;
												else eControlStatus = hppControlStatus_Repeat;  // no error
											}
											else 
											{
												if(strcmp(pchLastParam, "false") != 0) 	
												{ 
													apParseContext->eReturnReason = hppReturnReason_Error_BooleanValueExpected;
													break;
												}
												
												chTerm = hppIgnoreExpression(apParseContext); // Ingore the following expression or codeblock
												if(eControlStatus == hppControlStatus_Repeat) eControlStatus = hppControlStatus_Done;
												else eControlStatus = hppControlStatus_Else;
											}
										}
									}
									while(eControlStatus == hppControlStatus_Repeat);
									
									// Delete Paramters
									szParamName[HPP_PARAM_PREFIX_LEN - 1] = 0;
									hppVarDeleteAll(szParamName);
											
									// Verify if there is an else branch if it was a control structure and no error occured		
									if(eControlStatus != hppControlStatus_None && apParseContext->eReturnReason == hppReturnReason_None)
									{
										while(isspace((int)apParseContext->szCode[apParseContext->cbPos])) (apParseContext->cbPos)++;    // Ignore white spaces such we can check if "else" is the next code element
							
										if(strncmp(apParseContext->szCode + apParseContext->cbPos, "else", 4) == 0 && apParseContext->eReturnReason == hppReturnReason_None)
										{
											apParseContext->cbPos += 4;   // Continue behind 'else'
											if(eControlStatus == hppControlStatus_Else) pchResult = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, ";}", &chTerm);  // Temporary use of result variable
											else chTerm = hppIgnoreExpression(apParseContext);
										}
										
										chTerm = 32;
										continue;  // Expression stops here. Return to main loop for next expression 
									}
								}

								if(apParseContext->eReturnReason == hppReturnReason_None) 
								{	
									chTerm = hppGetOperator(apParseContext);
									if(chTerm == 'M') apParseContext->eReturnReason = hppReturnReason_Error_CannotCallMethodOnResult;  // Nesting of method calls is not supported
								}
								break;

					case '=':   pchResult = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, ")]};", &chTerm);
								hppVarPut(szExpresssion, pchResult, cbResultLen);
								break;
								 
					case 'B':   // Operator '{' result is the the result of the last expression inside the brackets or after return operator
								pchResult = hppParseExpressionInt(apParseContext, aszResultVarKey, &cbResultLen, "}", &chTerm); 
								break;
								
					case 'i':   // Leading '++' or '--' operator
					case 'd':	chNewTerm = hppGetExpression(apParseContext, szExpresssion, NULL, HPP_EXP_MAX_LEN);
								if(islower((int)szExpresssion[0])) szExpresssion = szExpresssionWithPrefix;  // local variable or function
								 
					case 'I':   // Trailing '++' or '--' operator
					case 'D':  	pchResult = hppVarGet(szExpresssion, &cbResultLen);
								if(pchResult != NULL)
								{
									char szNumeric[HPP_NUMERIC_MAX_MEM];   // 12 numbers => max 19 characters + null byte: -1.23456789012+e123
									double dResult = hppAtoF(pchResult);
									if(chTerm == 'D' || chTerm == 'd') dResult--;
									else  dResult++;
									
									if(chTerm == 'I' || chTerm == 'D')   // Trailing -> Put 'aszResultVarKey' with old value first 
									{
										pchResult = hppVarPut(aszResultVarKey, pchResult, cbResultLen);
										hppVarPutStr(szExpresssion, hppD2A(szNumeric, dResult), &cbResultLen);
										chNewTerm = hppGetOperator(apParseContext);
									}
									else  // Leading -> Variable in 'szExpresssion' and in 'aszResultVarKey' get the same new value
									{
										pchResult = hppVarPutStr(szExpresssion, hppD2A(szNumeric, dResult), &cbResultLen);
										pchResult = hppVarPut(aszResultVarKey, pchResult, cbResultLen);
									}
								}
								else apParseContext->eReturnReason = hppReturnReason_Error_UnknownVariable;
								
								chTerm = chNewTerm;
								break;
								
					case 'R':	chTerm = hppGetExpression(apParseContext, szExpresssion, NULL, HPP_EXP_MAX_LEN);
								if(islower((int)szExpresssion[0])) szExpresssion = szExpresssionWithPrefix;  // local variable or function
							
								pchResult = hppVarPutStr(aszResultVarKey, szExpresssion, &cbResultLen);
								break;
			

					default:	do
								{
									if(szExpresssion == szExpresssionWithPrefix)       // Check if it is a keyword to variable
									{
										char *szExpresssionBehindPrefix = szExpresssion + HPP_EXP_LOCAL_VAL_PREFIX_LEN;
										
										if(strcmp(szExpresssionBehindPrefix, "true") == 0 )   { pchResult = hppVarPutStr(aszResultVarKey, "true", &cbResultLen); break; }
										if(strcmp(szExpresssionBehindPrefix, "false") == 0 )  { pchResult = hppVarPutStr(aszResultVarKey, "false", &cbResultLen); break; }
										
										if(strcmp(szExpresssionBehindPrefix, "return") == 0 ) 
										{ 
											pchResult = hppParseExpressionInt(apParseContext, szExpresssion, &cbResultLen, ";", &chTerm);
											pchResult = hppVarPut(aszResultVarKey, pchResult, cbResultLen);
											if(apParseContext->eReturnReason == hppReturnReason_None)
											{
												if(chTerm == ';') apParseContext->eReturnReason = hppReturnReason_Return;
												else apParseContext->eReturnReason = hppReturnReason_Error_SemicolonExpected; 
											}
											break;
										}
										
										if(strcmp(szExpresssionBehindPrefix, "break") == 0) 
										{ 
											if(chTerm == ';') apParseContext->eReturnReason = hppReturnReason_Break;
											else apParseContext->eReturnReason = hppReturnReason_Error_SemicolonExpected;
											break;
										}
										
										if(strcmp(szExpresssionBehindPrefix, "continue") == 0) 
										{ 
											if(chTerm == ';') apParseContext->eReturnReason = hppReturnReason_Continue;
											else apParseContext->eReturnReason = hppReturnReason_Error_SemicolonExpected;
											break;
										}
							
									}	
									
									pchResult = hppVarGet(szExpresssion, &cbResultLen);    // Read variable
									if(pchResult != NULL) pchResult = hppVarPut(aszResultVarKey, pchResult, cbResultLen);
									else apParseContext->eReturnReason = hppReturnReason_Error_UnknownVariable;
									
								} while(false);
				}
			}
		}
			
		// Binary operators 
		if(pchResult == NULL && apParseContext->eReturnReason == hppReturnReason_None)
			apParseContext->eReturnReason = hppReturnReason_FatalError;   // hppVarPutStr could fail due to no memory   
		else 
		{
			char szSecondOpName[HPP_SECOND_OP_PREFIX_LEN + 1];
			sprintf(szSecondOpName, HPP_SECOND_OP_PREFIX, apParseContext->iCallDepth);
			
			while(strchr(aszTerminatingOperators, chTerm) == NULL && apParseContext->eReturnReason == hppReturnReason_None && chTerm != ';')
			{
				char szNumeric[HPP_NUMERIC_MAX_MEM];   // 12 numbers => max 19 characters + null byte: -1.23456789012+e123
				char *pchSecondOperand;
				size_t cbSecondOperandLen;
				const char* hppTerminatingOperators = "%/*-+~<>GSUE&^|AO=)]},;";
				char chNextTerm;
							
				hppTerminatingOperators = strchr(hppTerminatingOperators, chTerm);        // Get terminating operators in line  with operator priority
				if(hppTerminatingOperators == NULL) { apParseContext->eReturnReason = hppReturnReason_Error_InvalidOperator; break; }    // Invalid operator?

				if(hppTerminatingOperators[0] == '*') hppTerminatingOperators -= 2;       // '%/*' have same priority; '/' handled below
				if(strchr("+/E", hppTerminatingOperators[0]) != NULL) hppTerminatingOperators--;  // '+/E' have same priotity like '-%U' respectively     
			
				pchSecondOperand = hppParseExpressionInt(apParseContext, szSecondOpName, &cbSecondOperandLen, hppTerminatingOperators, &chNextTerm);
				if(apParseContext->eReturnReason != hppReturnReason_None) break;

				switch(chTerm)
				{
					case '+':	pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoF(pchResult) + hppAtoF(pchSecondOperand)), &cbResultLen);
								break;
					case '-':	pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoF(pchResult) - hppAtoF(pchSecondOperand)), &cbResultLen);
								break;
					case '*':	pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoF(pchResult) * hppAtoF(pchSecondOperand)), &cbResultLen);
								break;
					case '/':	if(hppAtoF(pchSecondOperand) == 0.0f) apParseContext->eReturnReason = hppReturnReason_Error_DivisionByZero;
								else pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoF(pchResult) / hppAtoF(pchSecondOperand)), &cbResultLen);									
								break;
					case '%':	if(hppAtoI(pchSecondOperand) == 0) apParseContext->eReturnReason = hppReturnReason_Error_DivisionByZero;
								else pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoI(pchResult) % hppAtoI(pchSecondOperand)), &cbResultLen);									
								break;
					case '&':	pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoI(pchResult) & hppAtoI(pchSecondOperand)), &cbResultLen);									
								break;
					case '|':	pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoI(pchResult) | hppAtoI(pchSecondOperand)), &cbResultLen);									
								break;
					case '^':	pchResult = hppVarPutStr(aszResultVarKey, hppD2A(szNumeric, hppAtoI(pchResult) ^ hppAtoI(pchSecondOperand)), &cbResultLen);									
								break;
					case 'E':	if(cbResultLen != cbSecondOperandLen) pchResult = hppVarPutStr(aszResultVarKey, "false", &cbResultLen);
								else pchResult = hppVarPutStr(aszResultVarKey, memcmp(pchResult, pchSecondOperand, cbResultLen) == 0 ? "true": "false", &cbResultLen);
								break;
					case 'U':	if(cbResultLen != cbSecondOperandLen) pchResult = hppVarPutStr(aszResultVarKey, "true", &cbResultLen);
								else pchResult = hppVarPutStr(aszResultVarKey, memcmp(pchResult, pchSecondOperand, cbResultLen) == 0 ? "false": "true", &cbResultLen);
								break;			
					case '<':	pchResult = hppVarPutStr(aszResultVarKey, hppAtoF(pchResult) < hppAtoF(pchSecondOperand) ? "true": "false", &cbResultLen);
								break;
					case '>':	pchResult = hppVarPutStr(aszResultVarKey, hppAtoF(pchResult) > hppAtoF(pchSecondOperand) ? "true": "false", &cbResultLen);
								break;			
					case 'S':	pchResult = hppVarPutStr(aszResultVarKey, hppAtoF(pchResult) <= hppAtoF(pchSecondOperand) ? "true": "false", &cbResultLen);
								break;
					case 'G':	pchResult = hppVarPutStr(aszResultVarKey, hppAtoF(pchResult) >= hppAtoF(pchSecondOperand) ? "true": "false", &cbResultLen);
								break;
												
					case 'O':	// For logic operators '&&' and '||' we assume both operands are either 'true' or 'false'
					case 'A':	if(strcmp(pchResult, "true") != 0 && strcmp(pchResult, "false") != 0) apParseContext->eReturnReason = hppReturnReason_Error_FirstOperand_BooleanValueExpected;
								if(strcmp(pchSecondOperand, "true") != 0 && strcmp(pchSecondOperand, "false") != 0) apParseContext->eReturnReason = hppReturnReason_Error_SecondOperand_BooleanValueExpected;
								if(chTerm == 'A') pchResult = hppVarPutStr(aszResultVarKey, (*pchResult == 't' && *pchSecondOperand == 't') ? "true": "false", &cbResultLen);
								if(chTerm == 'O') pchResult = hppVarPutStr(aszResultVarKey, (*pchResult == 't' || *pchSecondOperand == 't') ? "true": "false", &cbResultLen);
								break;	
								
					case '~':	pchResult = hppVarPut(aszResultVarKey, hppNoInitValue, cbResultLen + cbSecondOperandLen);
								if(pchResult != NULL) memcpy(pchResult + cbResultLen, pchSecondOperand, cbSecondOperandLen);
								cbResultLen += cbSecondOperandLen;  
								break;					
				}

				chTerm = chNextTerm;
			
				if(pchResult == NULL && apParseContext->eReturnReason == hppReturnReason_None)
					apParseContext->eReturnReason = hppReturnReason_FatalError;   // hppVarPutStr could fail due to no memory  
			}
			
			hppVarDelete(szSecondOpName);
		}	
	}
	
	
	apParseContext->iCallDepth--;
	
	if(apParseContext->iFunctionCallDepth == apParseContext->iCallDepth)
	{
		szExpresssionWithPrefix[HPP_EXP_LOCAL_VAL_PREFIX_LEN] = 0;
		hppVarDeleteAll(szExpresssionWithPrefix);  // delete local variables
	}
	
	// free(szExpresssionWithPrefix);
	apParseContext->szExpressionStackPointer = szExpressionStackPointerBuf;   // Restore expression stack level
	if(apchTerminatingOperator_Out != NULL) *apchTerminatingOperator_Out = chTerm;
	if(apcbResultLen_Out != NULL) *apcbResultLen_Out = cbResultLen;

	return pchResult;
}


void hppTestFloatPrint()
{
	//char szNumeric[HPP_NUMERIC_MAX_MEM];
	
	if(hppIsFloatTested) return; 
	hppIsFloatTested = true;

	//sprintf(szNumeric, "%.*g", HPP_NUMERIC_MAX_DIGITS, 1.123);
	//if(strcmp(szNumeric, "1.123") == 0) hppFloatPrintOn = true;
	//else hppFloatPrintOn = false;
        hppFloatPrintOn = false;
}


// Parse H++ code in 'aszCode' and store return value in the variable 'aszResultVarKey'. Returns a pointer to
// the char array assoziated with that variable 'aszResultVarKey'.
// If the string value of 'aszResultVarKey' equals "ReturnWithError", an error text is created in cas of a parse
// error. Otherwise the result in NULL in case of a parse error.
char* hppParseExpression(const char* aszCode, const char* aszResultVarKey)
{
	char* pchResult;
	struct hppParseExpressionStruct* theParseContext;

	if(aszCode == NULL || aszResultVarKey == NULL) return NULL;
	theParseContext = (struct hppParseExpressionStruct*)malloc(sizeof(struct hppParseExpressionStruct));
	if(theParseContext == NULL) return NULL;
	memset(theParseContext, 0, sizeof(struct hppParseExpressionStruct));
	theParseContext->szCode = aszCode;
	theParseContext->eReturnReason = hppReturnReason_None;
	theParseContext->szExpressionStack = (char*)malloc(HPP_EXP_STACK_SIZE);
	theParseContext->szExpressionStackPointer = theParseContext->szExpressionStack;
	if(theParseContext->szExpressionStack == NULL) { free(theParseContext); return NULL; }
	*theParseContext->szExpressionStack = 0;   // Stack starts as empty string
	
	hppTestFloatPrint();
	if(hppExternalPollFunction != NULL) hppExternalPollFunction(theParseContext, hppExternalPollFunctionEvent_Begin);  // Initialize external poll function, e.g. timer

	pchResult = hppParseExpressionInt(theParseContext, aszResultVarKey, NULL, "", NULL);
	
	if(hppExternalPollFunction != NULL) hppExternalPollFunction(theParseContext, hppExternalPollFunctionEvent_End);    // Terminate external poll function activities

	
	if(theParseContext->eReturnReason == hppReturnReason_Break) theParseContext->eReturnReason = hppReturnReason_Error_BreakWithoutWhile;
	if(theParseContext->eReturnReason == hppReturnReason_Continue) theParseContext->eReturnReason = hppReturnReason_Error_ContinueWithoutWhile;

	
	if(theParseContext->eReturnReason != hppReturnReason_EOF && theParseContext->eReturnReason != hppReturnReason_Return)
	{
		if(strcmp(aszResultVarKey, "ReturnWithError") == 0 || strcmp(aszResultVarKey, "ReturnWithDebugInfo") == 0)
		{
			char szErr[49 + HPP_CALL_FUNCTION_NAME_MAX_LEN];
			int nLine = 1;
			size_t cbLineStart = 0;
			size_t cbSearchPos = 0;
			size_t cbErrorPos = theParseContext->cbPos;
			const char* szErrorCode = aszCode;
			
			// Check if the Error occured in a sub function call 
			if(theParseContext->szErrorCallFunctionName[0] != 0) 
			{ 
				szErrorCode = hppVarGet(theParseContext->szErrorCallFunctionName, NULL);
				cbErrorPos = theParseContext->cbErrorCallFunctionPos;
			}
			
			// Count the number of lines until the error position
			if(szErrorCode != NULL)
				while(cbSearchPos < cbErrorPos) 
					if(szErrorCode[cbSearchPos++] == 10) { nLine++; cbLineStart = cbSearchPos; }
				  
			if(theParseContext->szErrorCallFunctionName[0] == 0)
				sprintf(szErr, "#Error %d in line %d near column %d", (int)theParseContext->eReturnReason, nLine, (int)(cbErrorPos - cbLineStart) - 1);
			else
				sprintf(szErr, "#Error %d in line %d near column %d in '%s'", (int)theParseContext->eReturnReason, nLine, (int)(cbErrorPos - cbLineStart) - 1, theParseContext->szErrorCallFunctionName);
					
			pchResult = hppVarPutStr(aszResultVarKey, szErr, NULL);
		}
		else pchResult = NULL;	
	}
	
	if(theParseContext->eReturnReason == hppReturnReason_EOF)
		if(strcmp(aszResultVarKey, "ReturnWithDebugInfo") != 0) pchResult = NULL;   // No return value if code has reached EOF unless needed for debug reasons
		
	free(theParseContext->szExpressionStack);	
	free(theParseContext);
	return pchResult;	
}


// Add an external function library 
bool hppAddExternalFunctionLibrary(hppExternalFunctionType aExternalFunctionType)
{
	size_t iEntry = 0;
	
	while(iEntry < HPP_EXTERNAL_FUNCTION_LIBRARY_COUNT)
	{
		if(hppExternalFunctions[iEntry] == NULL)
		{
			hppExternalFunctions[iEntry] = aExternalFunctionType;
			return true;
		}	 
		
		iEntry++;
	}
	
	return false;
}


// Add an external poll function which shall be called regularly while parsing h++ code
// Returns an existing poll function, if it exists. Otherwhise returns NULL 
hppExternalPollFunctionType hppAddExternalPollFunction(hppExternalPollFunctionType aNewExternalPollFunction)
{
	hppExternalPollFunctionType existingExternalPollFunction = hppExternalPollFunction;
	
	hppExternalPollFunction = aNewExternalPollFunction;
	return existingExternalPollFunction;
}



