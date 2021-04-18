/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppZephir.c												            */
/* ----------------------------------------------------------------------------	*/
/* Zephire RTOS Support Library										            */
/*																	            */
/*   - Initialization of Zephire resources       					            */
/*   - Main thread processing events invoking halloween parser                  */
/*   - Timer function bindings                                                  */
/*   - Peripherals function bindings                                            */       
/* ----------------------------------------------------------------------------	*/
/* Platform: Zephire RTOS on Nordic nRF52840						            */
/* Nordic SDK Version: Connect SDK v1.4.1	       								*/
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


#ifndef __INCL_HPP_ZEPHYR_
#define __INCL_HPP_ZEPHYR_

#include <stdbool.h>
#include <ctype.h>

// Zephir Libraries
#include <zephyr.h>

// Include H++ scripting libraries
#include "../include/hppVarStorage.h"
#include "../include/hppParser.h"

// Thread libary (only needed for hppMyCurrentCoapMessageContext setting in hppAsyncSetCoapContext(...)),
// printing to Thread cli and CoAP response
#include "../include/hppThread.h"



// ------------------------------------------------------------------
// Synchronized function calls for relevant hppVarXXX functions
// ------------------------------------------------------------------

/*
// put operations are blocking while the parser is running
// hppSyncVarPut returns a boolean value indicating if put was successful insted of a pointer which would not be thread safe
bool hppSyncVarPut(const char aszKey[], const char apValue[], size_t acbValueLen);
bool hppSyncVarPutStr(const char aszKey[], const char aszValue[], size_t* apcbResultLen_Out);
// Lock mutex
char* hppSyncVarPutBegin(const char aszKey[], const char apValue[], size_t acbValueLen);
// Unlock mutex
void hppSyncVarPutEnd();

// New function to just check if a variable exists and otionally what lenght it has
bool hppSyncVarExists(const char aszKey[], size_t* apcbValueLen_Out);
// Lock mutex
char* hppSyncVarGetBegin(const char aszKey[], size_t* apcbValueLen_Out);
// Unlock mutex
void hppSyncVarGetEnd();

// Lock mutex
const char* hppSyncVarGetKeyBegin(const char aszKey[], bool abCaseSensitive);
// Unlock mutex
void  hppSyncVarGetKeyEnd();

// delete operations are blocking while the parser is running
bool hppSyncVarDelete(const char aszKey[]);
void hppSyncVarDeleteAll(const char aszKeyPrefix[]);


int hppSyncVarCount(const char aszKeyPrefix[], bool abExludeRootElements);
int hppSyncVarGetAll(char* aszResult, size_t acbMaxLen, const char aszKeyPrefix[], const char aszFormat[]);

// ------------------------------------------------------------------
// Synchronized function calls for H++ parser functions
// ------------------------------------------------------------------

// hppSyncParseExpression returns a boolean value indicating if the resutl value variable 'aszResultVarKey' has been created
bool hppSyncParseExpression(const char* aszCode, const char* aszResultVarKey);
char* hppSyncParseExpressionBegin(const char* aszCode, const char* aszResultVarKey);
void hppSyncParseExpressionEnd();

// Try Locking parser mutex within mximum 'int iWaitTimeInMs' milliseconds. Return value indicates if the mutext was locked. 
bool hppSyncTryLockParserMutext(int iWaitTimeInMs);
// Unlock the Parser Mutex
void hppSyncUnlockParserMutext();
*/


// ------------------------------------------------------------------
// Synchronized function calls for H++ parser functions
// -----------------------------------------------------------------

bool hppSyncAddExternalFunctionLibrary(hppExternalFunctionType aExternalFunctionType);
hppExternalPollFunctionType hppSyncAddExternalPollFunction(hppExternalPollFunctionType aNewExternalPollFunction);


// ------------------------------------------------------------------
// Asynchronous function call for H++ variable and parser functions
// ------------------------------------------------------------------

// Put a CoAP get request in queue for processing. The variable name is non-case-sensitve (may come from a CoAP URI).
// Returns true if successful and false if not (no buffer).
// Responds to the present Thread CoAP context with the actual variable value or CoAP code not_found if the variable does not exist.
bool hppAsyncVarGetFromUriCoapResponse(const char aszVarName[]);

// Put variable value update in queue for processing. Returns true if successful and false if not (no buffer).
// If 'abSyncWithNext' is true, this command will be executed with the next one. If successful it therefore must be
// followed by another hppAsyncXXX command immediatelly to avoid permanently locking the parser and the variable mutex.
bool hppAsyncVarPut(const char aszVarName[], const char apValue[], uint16_t acbValueLen, bool abSyncWithNext);

// Put string variable value update in queue for processing. Returns true if successful and false if not (no buffer).
// If 'abSyncWithNext' is true, this command will be executed with the next one. If successful it therefore must be
// followed by another hppAsyncXXX command immediatelly to avoid permanently locking the parser and the variable mutex.
bool hppAsyncVarPutStr(const char aszVarName[], const char aszValue[], bool abSyncWithNext);

// Put variable value update in queue for processing. The variable name is non-case-sensitve (may come from a CoAP URI).
// Returns true if successful and false if not (no buffer).
bool hppAsyncVarPutFromUri(const char aszVarName[], const char apValue[], uint16_t acbValueLen);

// Put string variable detetion in queue for processing. Returns true if successful and false if not (no buffer).
bool hppAsyncVarDelete(const char aszVarName[]);

// Put H++ code in queue for processing. Returns true if successful and false if not (no buffer).
// Writes the return value or error to the Thread CLI if aWriteResultToCli is true
bool hppAsyncParseExpression(const char* aszCode, bool aWriteResultToCli);

// Put H++ variable name to be executed in queue for processing. Returns true if successful and false if not (no buffer).
// hppAsyncParseVar("") may can be used to unlock the parser mutex in case of abSyncWithNext=true operation if the regular
// parse operation could not be put in queue. A safty buffer guarantees this is always possible at least once.
bool hppAsyncParseVar(const char* aszVarName);

// Put H++ variable name to be executed in queue for processing. Returns true if successful and false if not (no buffer).
// Responds to the present Thread CoAP context with the H++ result or error text if no response was given by the H++ code already
bool hppAsyncParseVarCoapResponse(const char* aszVarName);

// Put .wellknown/core get request in queue for processing. Returns true if successful and false if not (no buffer).
// Responds to the present Thread CoAP context with the actual list of resources in CoRE link format (40).
bool hppAsyncVarGetWellKnownCore();

// Put lock/unlock request for variables in .well-known/core queires in queue for processing. Returns true if successful and false if not (no buffer).
// Calling this function with aszPassword set to a value different from the value of the variable "Var_Hide_PW" or its default value enable hiding valiables.  
bool hppAsyncVarHide(const char* aszPassword);


// ------------------------------------------------------------------
// Asynchronous function call for H++ Thread functions
// ------------------------------------------------------------------

// Put CoAP context update in queue for processing. Returns true if successful and false if not (no buffer).
// If 'abSyncWithNext' is true, this command will be executed with the next one. If successful it therefore must be
// followed by another hppAsyncXXX command immediatelly to avoid permanently locking the parser and the variable mutex.
bool hppAsyncSetCoapContext(hppCoapMessageContext* apCoapMessageContext, bool abSyncWithNext);


// ------------------------------------------------------------------
// USB Communication
// ------------------------------------------------------------------

// Send data to the USB interface.
// Returns the number of bytes sent or 0 in case of a buffer overflow.
int hppZephyrUsbSend(const uint8_t apData[], uint16_t acbDataLen);


// ------------------------------------------------------------------
// Zephyr main Loop
// ------------------------------------------------------------------

void hppMainLoop();


// ------------------------------------------------------------------
// Zephyr initialization
// ------------------------------------------------------------------

void hppZephyrInit();

#endif
