/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppVarStorage.c											            */
/* ----------------------------------------------------------------------------	*/
/* Halloween++ Variable Storage									            	*/
/*																	            */
/*   - Memory management (put, get, delete, ..) for byte strings        		*/
/*   - Formatted enumeration of variables 										*/
/*   - Conversion between binary types and strings                              */
/* ----------------------------------------------------------------------------	*/
/* Platform: POSIX and alike											        */
/* Dependencies: none													        */
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

#include "../include/hppVarStorage.h"

#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

struct hppVarListStruct* pFirstVar = NULL;

const char* hppNoInitValue = (const char*) 0x01; 
const char* hppInitValueWithZero = (const char*) 0x02;

const char* hppVarGetAllEndNodes = "\\e,%s";
const char* hppVarGetAllRootNodes = "\\r,%s";
const char* hppVarGetAllNodes = "\\b,%s";
const char* hppVarGetAllKeyValuePair = "\\1;%s=\"%s\"";


#ifdef __arm__
#define __hpp_packed __packed            
#else
#define __hpp_packed
#endif


/* ====================== */
/* Key-Value pair storage */
/* ====================== */

// Update variable (key value pair) with the key 'aszKey' or create new variable with that key if it did not exist before.
// The new value of the variable is a 'acbValueLen' bytes long binary copy of the 'apValue' array content plus an exta null byte at the end
// to make sure it can safely be interpreted as a zero terminated string or as a byte arry.
// The variable is deleted if 'apValue' is NULL. The variable is not initalized if 'apValue' is hppNoInitValue.
// The variable is initalized with null bytes if 'apValue' is hppInitValueWithZero.
// Returns a pointer to the newly created or updated stored value or NULL if unsuccessfull or deleted. 
char* hppVarPut(const char aszKey[], const char apValue[], size_t acbValueLen)
{
	struct hppVarListStruct* newVar;
	struct hppVarListStruct** newVarRef;

	//printf("PUT: %s=%s\n", aszKey, apValue);
    
	if(aszKey == NULL) return NULL;
	newVar = pFirstVar;
	newVarRef = &pFirstVar;

	// Search for the right entry in the list
	while(newVar != NULL)
	{
		if(strcmp(newVar->szKey, aszKey) == 0) break;
		newVarRef = &newVar->pNext;
		newVar = newVar->pNext;
	}

	//  Create new key if it did not already exist or just update the length of the value
	if(newVar == NULL)
	{
		if(apValue == NULL) return NULL;  // Entry not valid
	
		newVar = (struct hppVarListStruct*) malloc(sizeof(struct hppVarListStruct));
		if(newVar == NULL) return NULL;

		newVar->szKey = (char*) malloc(strlen(aszKey) + 1);
		if(newVar->szKey == NULL)
		{
			free(newVar);
			return NULL;
		}

		strcpy(newVar->szKey, aszKey);
		newVar->pValue = (char *) malloc(acbValueLen + 1); // Add one byte for zero termination
	}
	else
	{
		// Remove entry found for the list such it can later be added back if everything went ok
		*newVarRef = newVar->pNext;

		// Just delete entry?
		if(apValue == NULL)
		{
			free(newVar->szKey);
			free(newVar->pValue);
			free(newVar);
			return NULL;
		}

		// Realloc value array to accommodate new length
		newVar->pValue = (char *) realloc(newVar->pValue, acbValueLen + 1); // Add one byte for zero termination
		
		// If only the size of the variable changes, it must be possbible to initialize the newly added bytes with zero     
		if(apValue == hppInitValueWithZero && acbValueLen > newVar->cbValueLen) 
		{
			if(newVar->pValue != NULL) memset(newVar->pValue + newVar->cbValueLen, 0, acbValueLen - newVar->cbValueLen);
			apValue = hppNoInitValue;   // Make sure the present value bytes are not overwritten further below
		}
	}

	newVar->cbValueLen = acbValueLen;
	if(newVar->pValue == NULL)
	{
		free(newVar->szKey);
		free(newVar);
		return NULL;
	}

	if(apValue == hppInitValueWithZero) memset(newVar->pValue, 0, acbValueLen); 
	else if(apValue != hppNoInitValue) memcpy(newVar->pValue, apValue, acbValueLen);
	newVar->pValue[acbValueLen] = 0;   // Always terminate the array wiht zero to make sure it can always be interpretet as a zero terminted string

    // Add the new value to the list
	newVar->pNext = pFirstVar;
	pFirstVar = newVar;

	return newVar->pValue;
}


// Update variable with the key 'aszKey' or create new variable with that key if it did not exist before.
// The new value of the variable is a copy of the 'aszValue' string.
// The variable is deleted if 'apValue' is NULL. The variable is not initalized if 'apValue' is hppNoInitValue.
// The variable is not initalized with null bytes if 'apValue' is hppInitValueWithZero.
// Returns a pointer to the newly created or updated stored value or NULL if unsuccessfull or deleted.
char* hppVarPutStr(const char aszKey[], const char aszValue[], size_t* apcbResultLen_Out)
{
	size_t cbValLen = 0;
	if(aszValue != NULL) cbValLen = strlen(aszValue);
	if(apcbResultLen_Out != NULL) *apcbResultLen_Out = cbValLen;
	return hppVarPut(aszKey, aszValue, cbValLen);
}


// Get variable with the key 'aszKey' and provide the value array lenght in 'apcbValueLen_Out' 
char* hppVarGet(const char aszKey[], size_t* apcbValueLen_Out)
{
	struct hppVarListStruct* searchVar;

	if(aszKey == NULL) return NULL;
	searchVar = pFirstVar;

	// Search for the right entry in the list
	while(searchVar != NULL)
	{
		if(strcmp(searchVar->szKey, aszKey) == 0) break;
		searchVar = searchVar->pNext;
	}

	if(apcbValueLen_Out != NULL)
	{
		if(searchVar != NULL) *apcbValueLen_Out = searchVar->cbValueLen;
		else *apcbValueLen_Out = 0; 
	}

	if(searchVar == NULL) return NULL;

	return searchVar->pValue;
}


// Get variable key name reference based on URI string (not case sensitve search --> aCaseSensitive = false),
// or based on the the key name itself (aCaseSensitive = false). 
// The reference is stable unless the variable it deleted. Updating the value keeps the reference intact.  
const char* hppVarGetKey(const char aszKey[], bool abCaseSensitive)
{
	struct hppVarListStruct* searchVar;
	size_t i;

	if(aszKey == NULL) return NULL;
	searchVar = pFirstVar;

	// Search for the right entry in the list
	while(searchVar != NULL)
	{
		if(abCaseSensitive) { if(strcmp(searchVar->szKey, aszKey) == 0) break; }
		else
		{
			for(i = 0; aszKey[i] != 0; i++) 
				if(tolower(aszKey[i]) != tolower(searchVar->szKey[i])) break;
			
			if(searchVar->szKey[i] == 0 && aszKey[i] == 0) break;  				
		}
			
		searchVar = searchVar->pNext;
	}

	if(searchVar != NULL) return searchVar->szKey;
	
	return NULL;
}



// Delete variable with the key 'aszKey'
bool hppVarDelete(const char aszKey[])
{
	return hppVarPut(aszKey, NULL, 0);
}


// Delete all variables starting with the key 'aszKeyPrefix'
void hppVarDeleteAll(const char aszKeyPrefix[])
{
	struct hppVarListStruct* searchVar;
	struct hppVarListStruct** searchVarRef;
	size_t cbKeyPrefixLen = strlen(aszKeyPrefix);

	if(aszKeyPrefix == NULL) return;
	searchVar = pFirstVar;
	searchVarRef = &pFirstVar;

	while(searchVar != NULL)
	{
		if(strncmp(searchVar->szKey, aszKeyPrefix, cbKeyPrefixLen) == 0) 
		{
			*searchVarRef = searchVar->pNext;   // Remove entry from list and keep the reference of pointer for next element
			free(searchVar->pValue);			// free memory of this element
			free(searchVar->szKey);
			free(searchVar);
		}
		else searchVarRef = &searchVar->pNext;   // store reference of pointer for next element

		searchVar = *searchVarRef;				 // go to next element
	}
}


// Count variables starting with the key 'aszKeyPrefix'
// If 'abExludeRootElements' is true then varaibles including another '.' behind the search text are not counted
int hppVarCount(const char aszKeyPrefix[], bool abExludeRootElements)
{
	struct hppVarListStruct* searchVar;
	size_t cbKeyPrefixLen = strlen(aszKeyPrefix);
	int iCount = 0;

	if(aszKeyPrefix == NULL) return 0;
	searchVar = pFirstVar;

	while(searchVar != NULL)
	{
		if(strncmp(searchVar->szKey, aszKeyPrefix, cbKeyPrefixLen) == 0)
			if(abExludeRootElements == false || strchr(searchVar->szKey + cbKeyPrefixLen, '.') == NULL) iCount++;

		searchVar = searchVar->pNext;				 // go to next element
	}
	
	return iCount;
}


// Report all variables starting with the key 'aszKeyPrefix' and list them in a zero terminated string. 
// The result is written in 'aszResult' using up to 'acbMaxLen' bytes of storage including the ending null byte.
// The format of the output is controlled with 'aszFormat' using the same syntax like in the format string of
// "printf(...)", whereby a first '%s' in the format stands for the name (key) of the variable and an optional
// second '%s' in the format stands for the value of the key value pair. 
// Similar to 'snprintf(...)', behavior the return value equals the number of characters needed to store the full
// result not truncated to 'acbMaxLen' bytes WITHOUT the terminating zero byte. A negative number indicates an error.
//
// In Case of hppVarGetAllRootNodes and hppVarGetAllNodes option (see below) the result may exceed the acutally needed
// memory to store the result, because duplicate root elements cannot be found without storing them. It is anway always save to
// allocate memory (+1 for ending null byte) according the return value and to re-run hppVarGetAll(..) with that memory.  
//
// A leading '\\x' sequence in 'aszFormat' introduces a special formatting option. The leading two charcter are not
// part of the format itself. If 'x' is a one digit numercial, the leading x characters of the format are ignored
// for the first item found by hppVarGetAll, such that these characters can be used for separating items. 
// E.g. "//2, %1" stands for a comma separated list of key names starting with the first key without leading comma.
//
// Special Fromat values for 'aszKeyPrefix':
// hppVarGetAllEndNodes: Gets a comma separated table of all variables without furhter '.' behind the 'aszKeyPrefix'
// hppVarGetAllRootNodes: Gets all unique key items on the level behind 'aszKeyPrefix' (no' .' at the end)
// hppVarGetAllNodes: Gets both of the above in a comma separated table format but root elements end with '.' 
// hppVarGetAllKeyValuePair: Gets a semicolon separaed list of all key value pairs on all levels including all sub items
int hppVarGetAll(char* aszResult, size_t acbMaxLen, const char aszKeyPrefix[], const char aszFormat[])
{
	struct hppVarListStruct* searchVar = pFirstVar;
	size_t cbKeyPrefixLen = strlen(aszKeyPrefix);
	int cbLen = 0;
	size_t cbWritePos = 0;
	char chFlag = 0;
	size_t cbOmitSeparator = 0;
	const char* szEmptyStr = "";

	if(aszKeyPrefix == NULL || aszFormat == NULL) return -1;
	if(aszResult == NULL) acbMaxLen = 0;  	// No Output, just measure lenght
	if(acbMaxLen > 0) *aszResult = 0; 
	
	if(aszFormat[0] == '\\') chFlag = aszFormat[1];		// Get Format Flag
	if(chFlag != 0) aszFormat += 2;						// Read fromat behind flag 
	if(chFlag >= '0' && chFlag <= '9') cbOmitSeparator = chFlag - '0';		// "\\n" -> Omit n format bytes in first element
	if(chFlag == 'e' || chFlag == 'r' || chFlag == 'b') cbOmitSeparator = 1;
	if(cbOmitSeparator > strlen(aszFormat)) cbOmitSeparator = 0;	
	
	// Search for the right entries in the list
	while(searchVar != NULL)
	{
		int iComp = strncmp(searchVar->szKey, aszKeyPrefix, cbKeyPrefixLen);
		int cbActualLen = 0;

		if(iComp == 0 && (chFlag == 'e' || chFlag == 'r'|| chFlag == 'b'))
		{ 
			char *pchPoint = strchr(searchVar->szKey + cbKeyPrefixLen, '.'); 

			if(pchPoint != NULL)
			{
				iComp = 1; // Do not add any elements with '.' inside (substructure)
				if(chFlag == 'r' || chFlag == 'b') 
				{
					char chTemp = pchPoint[1];
					pchPoint[1] = 0;
					if(strstr(aszResult != NULL ? aszResult : szEmptyStr, searchVar->szKey) == NULL)
						cbActualLen = snprintf(aszResult + cbWritePos, acbMaxLen - cbWritePos, aszFormat + cbOmitSeparator, searchVar->szKey, "{...}"); 
					
					pchPoint[1] = chTemp; 
					cbOmitSeparator = 0;  // Include separator string from next element on
				}
			}
		}
	
		if(iComp == 0 && chFlag != 'r') 
		{ 
			cbActualLen = snprintf(aszResult + cbWritePos, acbMaxLen - cbWritePos, aszFormat + cbOmitSeparator, searchVar->szKey, searchVar->pValue);  
			cbOmitSeparator = 0;  // Include separtor format fron next element on
		}

		if(cbActualLen < 0) return -1;
		cbLen += cbActualLen;
		cbWritePos += cbActualLen;
		if(cbWritePos > acbMaxLen) cbWritePos = acbMaxLen;  // Never write behind acbMaxLen

		searchVar = searchVar->pNext;
	}
	
	// Remove '.' at the end of all elements in case of 'hppVarGetAllRootNodes' format
	if(aszResult != NULL && chFlag == 'r') 
	{
		char* pchRead = aszResult;
		char* pchWrite = aszResult;
		
		while(*pchRead != 0)
		{
			if(*pchRead != '.' || (*(pchRead + 1) != ',' && *(pchRead + 1) != 0))
			{
				*pchWrite = *pchRead;
				pchWrite++;
			}
			
			pchRead++;
		}	
		
		*pchWrite = 0;
		cbLen = pchWrite - aszResult;
	}

	return cbLen;
}



/* ============================================ */
/* Conversion between Binary Types and Strings  */
/* ============================================ */


#if UINT_MAX < 4294967295 || INT_MAX < 2147483647
#error int is smaller than 4 bytes
#endif


// NULL tolerant versions of atoi(), atof(), etc.
// TODO: Verify mapping of int and long to int16_t, int32_t, uint16_t and uint32_t when chaning to a new processor core

int hppAtoI(const char* szArg)
{
	if(szArg == NULL) return 0;
	else return atoi(szArg);
}

int16_t hppAtoI16(const char* szArg)
{
	if(szArg == NULL) return 0;
	else return atoi(szArg);
}

uint16_t hppAtoUI16(const char* szArg)
{
	if(szArg == NULL) return 0;
	else return atoi(szArg);        // on a 32-bit processor int usually is int32_t. This can be convertet to unit16_t without losses. 
}

int32_t hppAtoI32(const char* szArg)
{
	if(szArg == NULL) return 0;
	else return atoi(szArg);        // on a 32-bit processor int usually is int32_t.
}

uint32_t hppAtoUI32(const char* szArg)
{
	if(szArg == NULL) return 0;
	else return atol(szArg);        // on a 32-bit processor long usually is int64_t. This can be convertet to unit32_t without losses.
}

double hppAtoF(const char* szArg)
{
	if(szArg == NULL) return 0;
	else return atof(szArg);
}


// Convert double number in a string with HPP_NUMERIC_MAX_DIGITS digits. The result can be up to HPP_NUMERIC_MAX_LEN (= HPP_NUMERIC_MAX_DIGITS + 8) bytes long.
// e.g. 12 numbers => max 19 characters + null byte: -1.23456789012+e123
char* hppD2A(char* aszNumeric_out, double adValue)
{
	size_t cb;
	double rest;
	
	if(hppFloatPrintOn) sprintf(aszNumeric_out, "%.*g", HPP_NUMERIC_MAX_DIGITS, adValue);
	else
	{
		sprintf(aszNumeric_out, "%d", (int)adValue);
		rest = fabs(adValue - trunc(adValue));
		if(rest != 0 ) strcat(aszNumeric_out, "."); 
		cb = strlen(aszNumeric_out);
		
		while(cb < HPP_NUMERIC_MAX_DIGITS + 2 && rest != 0)   // same number of digits like with sprintf("%.*g", HPP_NUMERIC_MAX_DIGITS, adValue)
		{
			rest *= 10;
			aszNumeric_out[cb++] = '0' + ((int)rest);
			rest -= trunc(rest);
		}
		
		aszNumeric_out[cb] = 0;
	}
		
	return aszNumeric_out;
}

// Convert uint16_t number in a string with up to 5 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppUI16toA(char* aszNumeric_out, uint16_t aiValue)
{
	sprintf(aszNumeric_out, "%u", (unsigned int)aiValue);      
	return aszNumeric_out;									 
}

// Convert int16_t number in a string with up to 6 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppI16toA(char* aszNumeric_out, int16_t aiValue)
{
	sprintf(aszNumeric_out, "%d", (int)aiValue);    
	return aszNumeric_out;							 
}

// Convert uint32_t number in a string with up to 10 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppUI32toA(char* aszNumeric_out, uint32_t aiValue)
{
	sprintf(aszNumeric_out, "%u", (unsigned int)aiValue);     // TODO: Check if uint32_t fits with format %u when changing to another system (gcc my give a warning)  
	return aszNumeric_out;									  // Despite int is 32-bit, type conversion is needed for uint32_t in GNU Tools for Arm Embedded Processors (Version 7)
}

// Convert int32_t number in a string with up to 11 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppI32toA(char* aszNumeric_out, int32_t aiValue)
{
	sprintf(aszNumeric_out, "%d", (int)aiValue);     // TODO: Check if int32_t fits with format %d when changing to another system (gcc my give a warning)
	return aszNumeric_out;							 // Despite int is 32-bit, type conversion is needed for int32_t in GNU Tools for Arm Embedded Processors (Version 7)
}

// Convert int number in a string with up to 11 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppI2A(char* aszNumeric_out, int aiValue)
{
	sprintf(aszNumeric_out, "%d", aiValue);     // TODO: Check if int32_t fits with format %d when changing to another system (gcc my give a warning)
	return aszNumeric_out;
}


// Get type id for the type name in aszType
// Does not verify if the 'aszType' string ends after the type name 
enum hppBinaryTypeEnum hppGetTypeID(const char* aszType)
{
	int iResult = 0;
	
	if(*aszType == 'u') iResult = HPP_MIN_UNSIGNED_TYPE_ID;    // just for performance optimization
	
	while(iResult <= HPP_MAX_TYPE_ID)
	{
		if(strncmp(aszType, hppTypeNameList[iResult], hppTypeNameLenght[iResult]) == 0) return (enum hppBinaryTypeEnum)iResult;
		iResult++;
	}
	
	return hppBinaryType_unvalid;
}
	
	
// Find member variable with matching name in a structure of binary values with a 'struct' header created by hppGetStructHeader(..).
// Returns the offset in bytes for the binary value of the member in szStruct behind the zero terminated format string.
// Also returns the type of the member at apBinaryType (if != NULL) and the length of on record of the struct at apRecordSize (if != NULL)
// In case the member was not found, the offset value returend will be zero and '*apBinaryType' is set to 'hppBinaryType_unvalid'.
// The returned offset includes the lenght of the header. Members can be accessed using the term: value = *((type*)(apchStruct + offset))
// If 'apchStruct' holds an array of structures, members can be accessed with: value = *((type*)(apchStruct + offset + n * apRecordSize))
// szStruct must point to the structure including a proper header before the acutal binary data. No checking for invalid (NULL) pointer is
// performed. szMember must point to the name of the member. No checking for invalid (NULL) pointer is performed.
// The value 'acbStructLen' must be set to the total lenght of the structure including the header. E.g. is can be the lenght of a variable
// created with hppVarPut(...).
size_t hppGetMemberFromStruct(const char* apchStruct, size_t acbStructLen, const char* aszMember, enum hppBinaryTypeEnum *apBinaryType, size_t *apRecordSize)
{
	const char* szSearchPos = apchStruct;
	size_t cbMemberNameLen = strlen(aszMember);
	size_t cbMemberListLen = strlen(apchStruct);
	size_t cbPos = 0;
	size_t nTypeIndex = cbMemberListLen + 5;
	size_t nItems;
	enum hppBinaryTypeEnum theBinaryType;
	
	if(acbStructLen >= nTypeIndex) 
	{
		if(apRecordSize != NULL) *apRecordSize = *((__hpp_packed int16_t*)(apchStruct + cbMemberListLen + 1));
		nItems = *((__hpp_packed int16_t*)(apchStruct + cbMemberListLen + 3));
		
		while(*szSearchPos != 0)
		{		
			theBinaryType = (enum hppBinaryTypeEnum)apchStruct[nTypeIndex++];
			if((unsigned int)theBinaryType > HPP_MAX_TYPE_ID) break; 
					
			if(strncmp(szSearchPos, aszMember, cbMemberNameLen) == 0)    
			{
				szSearchPos += cbMemberNameLen;
				if(*szSearchPos == 0 || *szSearchPos == ',')  // check if the found item is not longer than the search string
				{
					if(apBinaryType != NULL) *apBinaryType = theBinaryType;
					return cbMemberListLen + 5 + nItems + cbPos;
				}
			}
				
			cbPos += hppTypeSizeof[theBinaryType];	
			szSearchPos = strchr(szSearchPos, ',');
			if(szSearchPos == NULL) break;
			szSearchPos++;
		}
	}
	
	if(apBinaryType != NULL) *apBinaryType = hppBinaryType_unvalid;
	return 0;
}


// Calculate the number of records in a structure of binary values with a 'struct' header created by hppGetStructHeader(..).
// Returns the calculated number of records or zero if 'apchStruct' is NULL. The calculated size includes partial records 
// (minimum one more byte beyond the last full record) if bIncludeIncomplete is true; 
// The value 'acbStructLen' must be set to the total lenght of the structure including the header. E.g. is can be the lenght of a variable
// created with hppVarPut(...).
size_t hppGetStructRecordCount(const char* apchStruct, size_t acbStructLen, bool bIncludeIncomplete)
{
	size_t cbMemberListLen;
	size_t nRecordCount = 0;
	size_t nRecordSize;
	size_t nItems;
	size_t nPayloadLen;
	
	if(apchStruct == NULL) return 0;
	cbMemberListLen = strlen(apchStruct);
	if(acbStructLen < cbMemberListLen + 5) return 0;
	
	nRecordSize = *((__hpp_packed int16_t*)(apchStruct + cbMemberListLen + 1));
	nItems = *((__hpp_packed int16_t*)(apchStruct + cbMemberListLen + 3));
	nPayloadLen = acbStructLen - cbMemberListLen - 5 - nItems;
	
	if(nPayloadLen > 0 && nRecordSize != 0) 
	{
		nRecordCount = nPayloadLen / nRecordSize;
		if(bIncludeIncomplete && nPayloadLen % nRecordSize > 0) nRecordCount++;
	}
	
	return nRecordCount;
}
		

// Create header of a complex data type (structure) with multiple binary coded members for the type definition provided with 'szTypeDef'.
// Format "type1:name1,type2:name2,...", were typeN is a type name (int8, int16, ...) and nameN is the name of the n-th member. 
// The struct header is stored at 'pchHeader_out'. The expected size of the header can be calculated with hppGetStructHeaderLenght(..)
// before calling hppGetStructHeader(..) in order to reserve enough memory for the header.
// The header consists of a zero terminated string with member information in a more compact form than the format string and a int16_t
// value with the size of one binary record of the structure behind the zero terminated string. The return value is the record size. 
// The total memory needed for a structure or an array of n stucture elements is the size of the header + n times the record size.
// After calling hppGetStructHeader(..) 'pchHeader_out' may be realloc'ed to hold the desired number of structure elements (unless
// known at compile time already).
int16_t hppGetStructHeader(char* pchHeader_out, const char* szTypeDef)
{
	size_t nWrite = 0;
	size_t nRead = 0;
	size_t nStructLen = 0;
	size_t nItems = 0;
	size_t nItemsLeft;
	enum hppBinaryTypeEnum nTypeID;
	size_t cbLen = hppGetStructHeaderLenght(szTypeDef);
	
	if(cbLen == 0) return 0;
	if(szTypeDef == NULL) return 0;
	if(pchHeader_out == NULL) return 0;
		
	while(szTypeDef[nRead] != 0) if(szTypeDef[nRead++] == ':') nItems++;
	nRead = 0;
	nItemsLeft = nItems;
	
	while(szTypeDef[nRead] != 0)
	{
		nTypeID = hppGetTypeID(szTypeDef + nRead);
		if(nTypeID == hppBinaryType_unvalid) break;
	
		nRead += hppTypeNameLenght[nTypeID];
		nStructLen += hppTypeSizeof[nTypeID];	
		if(szTypeDef[nRead++] != ':') break;
		pchHeader_out[cbLen - nItemsLeft] = (char)nTypeID;
		nItemsLeft--;
				
		while(isalnum(szTypeDef[nRead])) pchHeader_out[nWrite++] = szTypeDef[nRead++];
		if(szTypeDef[nRead] == ',') pchHeader_out[nWrite++] = ',';
		else break;
			
		nRead++;
	}	
	
	if(szTypeDef[nRead] != 0) { nWrite = 0; nStructLen = 0; }
	pchHeader_out[nWrite++] = 0;
	*((__hpp_packed int16_t*)(pchHeader_out + nWrite)) = nStructLen;
	nWrite += 2;
	*((__hpp_packed int16_t*)(pchHeader_out + nWrite)) = nItems;
	
	return nStructLen;
}


// Calculate expected size of the header of a complex data type (structure) with multiple binary coded members for the type definition
// provided with 'szTypeDef'.
size_t hppGetStructHeaderLenght(const char* szTypeDef)
{
	size_t cbLen = 0;
	const char* pchNameStart;

	if(szTypeDef == NULL) return 0;
	
	while((pchNameStart = strchr(szTypeDef, ':')) != NULL)
	{
		szTypeDef = strchr(pchNameStart, ',');
		if(szTypeDef != NULL) cbLen += szTypeDef - pchNameStart + 1;
		else { cbLen += strlen(pchNameStart); break; }
	}
	
	if(pchNameStart == NULL) return 0;
	return cbLen + 5;  // Add 1 byte for terminating null byte + 2 bytes for record length length + 2 bytes for the numner of items
 }

