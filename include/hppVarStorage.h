/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppVarStorage.h											            */
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

#ifndef __INCL_HPP_VAR_
#define __INCL_HPP_VAR_

#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

// Number precisions in string format  
#define HPP_NUMERIC_MAX_DIGITS 12     // maximum number of digits in number produced with sprintf() -> 8 more characters needed for full number
#define HPP_NUMERIC_MAX_MEM HPP_NUMERIC_MAX_DIGITS + 8     // Shall never be less than 12 digits to support a full int32_t number incl. zero byte 


// Global variables
extern bool hppFloatPrintOn;
extern bool hppIsFloatTested;


// Binary encoding type IDs and settings

enum hppBinaryTypeEnum { hppBinaryType_int8, hppBinaryType_int16, hppBinaryType_int32, hppBinaryType_bool,
	                     hppBinaryType_var, hppBinaryType_array, hppBinaryType_string,
	                     hppBinaryType_float, hppBinaryType_double, hppBinaryType_fixstr,
	                     hppBinaryType_uint8, hppBinaryType_uint16, hppBinaryType_uint32,
	                     hppBinaryType_unvalid = 1000 }; 
	                     
#define HPP_MIN_UNSIGNED_TYPE_ID hppBinaryType_uint8    // unit8
#define HPP_MAX_TYPE_ID hppBinaryType_uint32            // unit32
#define HPP_VAR_NAME_MAX_LEN 24        				    // max. string lenght of var names in arrays, not including ending null byte
#define HPP_FIXSTR_MAX_LEN 32        				    // max. string lenght of null terminated string with fixed max. len, not including ending null byte
#define HPP_MAX_MEMBER_NAME_LEN 15						// max. member name lenght, not including ending null byte


// Tables with binary type properties  
// Types var, array and string are represented by the name of the variable holding the content

extern const char* hppTypeNameList[HPP_MAX_TYPE_ID + 1];
extern const size_t hppTypeSizeof[HPP_MAX_TYPE_ID + 1];
extern const size_t hppTypeNameLenght[HPP_MAX_TYPE_ID + 1];


// Structure for Key-Value pairs

struct hppVarListStruct
{
	char* szKey;
	char* pValue;
	size_t cbValueLen;
	struct hppVarListStruct* pNext;
};

extern struct hppVarListStruct* pFirstVar;

extern const char* hppNoInitValue; 
extern const char* hppInitValueWithZero;

extern const char* hppVarGetAllEndNodes;
extern const char* hppVarGetAllRootNodes;
extern const char* hppVarGetAllNodes;
extern const char* hppVarGetAllKeyValuePair;


/* ====================== */
/* Key-Value pair storage */
/* ====================== */

// Update variable (key value pair) with the key 'aszKey' or create new variable with that key if it did not exist before.
// The new value of the variable is a 'acbValueLen' bytes long binary copy of the 'apValue' array content plus an exta null byte at the end
// to make sure it can safely be interpreted as a zero terminated string or as a byte arry.
// The variable is deleted if 'apValue' is NULL. The variable is not initalized if 'apValue' is hppNoInitValue.
// The variable is initalized with null bytes if 'apValue' is hppInitValueWithZero.
// Returns a pointer to the newly created or updated stored value or NULL if unsuccessfull or deleted. 
char* hppVarPut(const char aszKey[], const char apValue[], size_t acbValueLen);

// Update variable with the key 'aszKey' or create new variable with that key if it did not exist before.
// The new value of the variable is a copy of the 'aszValue' string.
// The variable is deleted if 'apValue' is NULL. The variable is not initalized if 'apValue' is hppNoInitValue.
// The variable is not initalized with null bytes if 'apValue' is hppInitValueWithZero.
// Returns a pointer to the newly created or updated stored value or NULL if unsuccessfull or deleted.
char* hppVarPutStr(const char aszKey[], const char aszValue[], size_t* apcbResultLen_Out);

// Get variable with the key 'aszKey' and provide the value array lenght in 'apcbValueLen_Out' 
char* hppVarGet(const char aszKey[], size_t* apcbValueLen_Out);

// Get variable key name reference based on URI string (not case sensitve search --> aCaseSensitive = false),
// or based on the the key name itself (aCaseSensitive = true). 
// The reference is stable unless the variable it deleted. Updating the value keeps the reference intact.  
const char* hppVarGetKey(const char aszKey[], bool abCaseSensitive);

// Delete variable with the key 'aszKey'
bool hppVarDelete(const char aszKey[]);

// Delete all variables starting with the key 'aszKeyPrefix'
void hppVarDeleteAll(const char aszKeyPrefix[]);

// Count variables starting with the key 'aszKeyPrefix'
// If 'abExludeRootElements' is true then varaibles including another '.' behind the search text are not counted
int hppVarCount(const char aszKeyPrefix[], bool abExludeRootElements);

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
int hppVarGetAll(char* aszResult, size_t acbMaxLen, const char aszKeyPrefix[], const char aszFormat[]);



/* ============================================ */
/* Conversion between Binary Types and Strings  */
/* ============================================ */

// NULL tolerant versions of atoi(), atof(), etc.
// TODO: Verify mapping of int and long to int16_t, int32_t, uint16_t and uint32_t when chaning to a new processor core
int hppAtoI(const char* szArg);
int16_t hppAtoI16(const char* szArg);
uint16_t hppAtoUI16(const char* szArg);
int32_t hppAtoI32(const char* szArg);
uint32_t hppAtoUI32(const char* szArg);
double hppAtoF(const char* szArg);


// Convert double number in a string with HPP_NUMERIC_MAX_DIGITS digits. The result can be up to HPP_NUMERIC_MAX_DIGITS + 8 bytes long.
// e.g. 12 numbers => max 19 characters + null byte: -1.23456789012+e123
char* hppD2A(char* aszNumeric_out, double adValue);

// Convert uint16_t number in a string with up to 5 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppUI16toA(char* aszNumeric_out, uint16_t aiValue);

// Convert int16_t number in a string with up to 6 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppI16toA(char* aszNumeric_out, int16_t aiValue);

// Convert uint32_t number in a string with up to 10 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppUI32toA(char* aszNumeric_out, uint32_t aiValue);

// Convert int32_t number in a string with up to 11 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppI32toA(char* aszNumeric_out, int32_t aiValue);

// Convert int number in a string with up to 11 (<= HPP_NUMERIC_MAX_DIGITS) digits. 
char* hppI2A(char* aszNumeric_out, int aiValue);


// Get index for hppTypeXXXX type information tables for the type name in aszType
// Does not verify if the 'aszType' string ends after the type name 
enum hppBinaryTypeEnum hppGetTypeID(const char* aszType);	


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
size_t hppGetMemberFromStruct(const char* apchStruct, size_t acbStructLen, const char* aszMember, enum hppBinaryTypeEnum *apBinaryType, size_t *apRecordSize);



// Calculate the number of records in a structure of binary values with a 'struct' header created by hppGetStructHeader(..).
// Returns the calculated number of records or zero if 'apchStruct' is NULL. The calculated size includes partial records 
// (minimum one more byte beyond the last full record) if bIncludeIncomplete is true; 
// The value 'acbStructLen' must be set to the total lenght of the structure including the header. E.g. is can be the lenght of a variable
// created with hppVarPut(...).
size_t hppGetStructRecordCount(const char* apchStruct, size_t acbStructLen, bool bIncludeIncomplete);


// Create header of a complex data type (structure) with multiple binary coded members for the type definition provided with 'szTypeDef'.
// Format "type1:name1,type2:name2,...", were typeN is a type name (int8, int16, ...) and nameN is the name of the n-th member. 
// The struct header is stored at 'pchHeader_out'. The expected size of the header can be calculated with hppGetStructHeaderLenght(..)
// before calling hppGetStructHeader(..) in order to reserve enough memory for the header.
// The header consists of a zero terminated string with member information in a more compact form than the format string and a int16_t
// value with the size of one binary record of the structure behind the zero terminated string. The return value is the record size. 
// The total memory needed for a structure or an array of n stucture elements is the size of the header + n times the record size.
// After calling hppGetStructHeader(..) 'pchHeader_out' may be realloc'ed to hold the desired number of structure elements (unless
// known at compile time already).
int16_t hppGetStructHeader(char* pchHeader_out, const char* szTypeDef);


// Calculate expected size of the header of a complex data type (structure) with multiple binary coded members for the type definition
// provided with 'szTypeDef'. szStruct must point to the structure including a proper header before the acutal binary data. No checking
// for invalid (NULL) pointer is performed. The value 'acbStructLen' bust be set to the total lenght of the structure including the header.
size_t hppGetStructHeaderLenght(const char* szTypeDef);


#endif
