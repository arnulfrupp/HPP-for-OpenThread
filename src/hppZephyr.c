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
/* Nordic SDK Version: Connect SDK v1.5.1	       								*/
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


#include "../include/hppZephyr.h"


// Zephir Libraries
#include <logging/log.h>
#include <ram_pwrdn.h>

#include <device.h>
#include <shell/shell_uart.h>
#include <drivers/gpio.h>
#include <sys/ring_buffer.h>
#include <drivers/uart.h>
#include <usb/usb_device.h>
#include <drivers/usb/usb_dc.h>
#include <settings/settings.h>


// Zephir OpenThread integration Library (for openthread_api_mutex_lock/unlock)
#include <net/openthread.h>



// ----------------
// Settings
// ----------------

#define HPP_EVENT_TIMER_RESOURCE_COUNT  64

#define HPP_MIN_TIMER_TIME              20

#define HPP_ASYNC_RING_BUF_BUFFER_SIZE  2048
#define HPP_ASYNC_MAX_VAR_SIZE          32      // maximum var length for hppAsyncXXXX(...)
#define HPP_ASYNC_MAX_DATA_SIZE         1280    // maximum data length for hppAsyncXXXX(...)  --> Thread IPv6 MTU size

#define HPP_ASYNC_RINGBUF_SAFETY_BUFFER (sizeof(uint16_t) + sizeof(uint8_t) + 10)      // safety buffer for all hppAsyncXXX functions having a abSyncWithNext option

// USB
#define HPP_ZEPHYR_USB_SEND_BUF_SIZE 512
#define HPP_ZEPHYR_USB_RECV_BUF_SIZE 512        // must be <= HPP_ASYNC_MAX_DATA_SIZE



// ---------------------------------
// Zephyr Kernel and logging Objects
// ---------------------------------

// Zephire Logging
LOG_MODULE_REGISTER(halloween_zephyr, CONFIG_HALLOWEEN_ZEPHYR_LOG_LEVEL);

#define CONSOLE_LABEL DT_LABEL(DT_CHOSEN(zephyr_console))


// Halloween variable storage function mutex
//K_MUTEX_DEFINE(hppVarMutex);

// Halloween parser function mutex
// It is not allowed to read or write variables with hppVarXXX(...) withut locking the mutex.
K_MUTEX_DEFINE(hppParseMutex);

// Main Thread Parser Semaphor
K_SEM_DEFINE(hppParserSemaphor, 0, 10000);



// ----------------
// Typedefs
// ----------------

typedef struct hppTimerResource
{
    struct k_timer mTimerResource;                                  ///< Zephyr timer Resource

    hppTimerHandler pHandler;                                       ///< Handler function
    void* pContext;                                                 ///< Application context

} hppTimerResource;


typedef struct hppEventTimerResource
{
    struct k_timer mTimerResource;                                  ///< Zephyr timer Resource

    hppQueuedTimerHandler pHandler;                                 ///< Handler function
    void* pApplicationData;                                         ///< Application data
    uint32_t mApplicationDataLen;                                   ///< Application data lenght in bytes
    void* pContext;                                                 ///< Application context

    bool mInUse;                                                    ///< Resource currently in use 

} hppEventTimerResource;




// ----------------
// Global variables
// ----------------

const struct device *hppGpioDev;

uint8_t hppAsyncRingBufBuffer[HPP_ASYNC_RING_BUF_BUFFER_SIZE];
struct ring_buf hppAsyncRingBuf;
char hppAsyncDataBuffer[HPP_ASYNC_MAX_DATA_SIZE + 1];
char hppAsyncVarName[HPP_ASYNC_MAX_VAR_SIZE + 1];


// USB Interface
const struct device *hppUsbDev;
uint8_t hppZephyrUsbRingBufBuffer[HPP_ZEPHYR_USB_SEND_BUF_SIZE];
struct ring_buf hppZephyrUsbRingBuf;
uint8_t hppZephyrUsbRecvBufBuffer[HPP_ZEPHYR_USB_RECV_BUF_SIZE];
uint16_t hppZephyrUsbRecvBufBufferLen = 0;


// Timer resources
hppTimerResource hppTimerResources[HPP_TIMER_RESOURCE_COUNT];
hppEventTimerResource hppEventTimerResources[HPP_EVENT_TIMER_RESOURCE_COUNT];


// ----------------
// TLV Types
// ----------------

#define HPP_ASYNC_TLV_VAR_GET_COAP_RESPONSE         1
#define HPP_ASYNC_TLV_VAR_PUT                       2
#define HPP_ASYNC_TLV_VAR_PUT_WITH_NEXT             3
#define HPP_ASYNC_TLV_VAR_PUT_NON_CASE_SENSITIVE    4
#define HPP_ASYNC_TLV_VAR_DELETE                    5
#define HPP_ASYNC_TLV_PARSE_TEXT                    6
#define HPP_ASYNC_TLV_PARSE_TEXT_CLI                7
#define HPP_ASYNC_TLV_PARSE_VAR                     8
#define HPP_ASYNC_TLV_PARSE_VAR_COAP_RESPONSE       9
#define HPP_ASYNC_TLV_SET_COAP_CONTEXT              10
#define HPP_ASYNC_TLV_SET_COAP_CONTEXT_WITH_NEXT    11
#define HPP_ASYNC_TLV_VAR_GET_WKC_COAP_RESPONSE     12
#define HPP_ASYNC_TLV_VAR_HIDE                      13
#define HPP_ASYNC_TLV_USB_RECV                      14
#define HPP_ASYNC_TLV_TIMER_EXPIRED                 15
#define HPP_ASYNC_TLV_EVENT_TIMER_EXPIRED           16


// ------------------------------------------
// Flash memory and settings relate functions 
// ------------------------------------------

static int hppDeleteVarSettingHandler(const char *aKey, size_t aLen, settings_read_cb aReadCb, void* apReadCbArg, void *aContext)
{
    uint16_t* piCount = (uint16_t*)aContext; 
    char chBegin = 'a';
    char chEnd = 'z';

    printf("delete:%s\r\n", aKey);

    if(piCount == NULL || strlen(aKey) < 1) return 0;

    if(*piCount & 0x8000) { chBegin = 'A'; chEnd = 'Z'; }

    if(aKey[0] >= chBegin && aKey[0] <= chEnd)
    {
        char* pVarName = (char*)malloc(strlen(aKey) + 5);                      // space for header and zero byte
        if(pVarName == NULL) return 0;

        strcpy(pVarName, "hpp/");                                              // header is 4 bytes long
        strcpy(pVarName + 4, aKey);
        settings_delete(pVarName);
        (*piCount)++;
        free(pVarName);
    }

    return 0;
}


bool hppDeleteVarFromFlash(bool abGlobal)
{
    uint16_t iCount = abGlobal ? 0x8000 : 0x0;

    settings_load_subtree_direct("hpp", hppDeleteVarSettingHandler, &iCount);

    return (iCount & 0x7FFF) > 0; 
}


static int hppReadVarSettingHandler(const char *aKey, size_t aLen, settings_read_cb aReadCb, void* apReadCbArg, void *aContext)
{
    char* pValue;
    uint16_t* piCount = (uint16_t*)aContext; 
    char chBegin = 'a';
    char chEnd = 'z';

    printf("load:%s\r\n", aKey);

    if(piCount == NULL || strlen(aKey) < 1) return 0;

    if(*piCount & 0x8000) { chBegin = 'A'; chEnd = 'Z'; }

    if(aKey[0] >= chBegin && aKey[0] <= chEnd)
    {
        pValue = hppVarPut(aKey, hppNoInitValue, aLen);             // Just reserve memory and write 0 byte behind the data
        
        if(pValue != NULL) 
        {
            aReadCb(apReadCbArg, pValue, aLen);
            (*piCount)++;
        }
    }

    return 0;
}


bool hppReadVarFromFlash(bool abGlobal)
{
    uint16_t iCount = abGlobal ? 0x8000 : 0x0;

    settings_load_subtree_direct("hpp", hppReadVarSettingHandler, &iCount);

    return (iCount & 0x7FFF) > 0; 
}


bool hppWriteVarToFlash(bool abGlobal)
{
	struct hppVarListStruct* searchVar;
    char* pVarName;
    size_t cbVarNameMaxLen = 32;
    size_t cbKeyLen;
    char chBegin = 'a';
    char chEnd = 'z';
  
    if(abGlobal) { chBegin = 'A'; chEnd = 'Z'; }

    pVarName = (char*)malloc(cbVarNameMaxLen + 5);                          // space for header and zero byte
    if(pVarName == NULL) return false;

    strcpy(pVarName, "hpp/");                                               // header is 4 bytes long
	searchVar = pFirstVar;

	// Search for the right entries in save them
	while(searchVar != NULL)
	{
		if(searchVar->szKey[0] >= chBegin && searchVar->szKey[0] <= chEnd)
        { 

            printf("save:%s", searchVar->szKey);

            cbKeyLen = strlen(searchVar->szKey);

            if(cbKeyLen > cbVarNameMaxLen)
            {
                char* pNewVarName = (char*)realloc(pVarName, cbKeyLen + 5); // space for header and zero byte
                
                if(pNewVarName == NULL)
                {
                    free(pVarName);                                         // realloc does not free the original pointer  
                    return false; 
                }

                pVarName = pNewVarName;
                cbVarNameMaxLen = cbKeyLen;
            }

            strcpy(pVarName + 4, searchVar->szKey);

            printf(" (%s)\r\n", pVarName);

            if(settings_save_one(pVarName, searchVar->pValue, searchVar->cbValueLen) != 0)
            {
                free(pVarName);
                return false;
            }
        }

		searchVar = searchVar->pNext;
	}
    
    free(pVarName);
    return true;
}


// --------------------------------------------
// Handler for timer related H++ function calls 
// --------------------------------------------

void hppTimerExecutionHandler(void *apContext)
{
    if(apContext == NULL) return;   // unknown H++ function

    hppAsyncParseVar((const char*)apContext);
}


void hppEventExecutionHandler(void* apData, uint32_t aDataLen, void *apContext)
{
    bool bSuccess;

    if(apContext == NULL) return;   // unknown H++ function

    bSuccess = hppAsyncVarPut("0000:event", apData, aDataLen, true);
    if(bSuccess) hppAsyncParseVar((const char*)apContext);

    // Make sure to unlock the parser mutex after any hppAsyncXXX(...) call with abSyncWithNext=true if hppAsyncParseVarCoapResponse(...) was not executed.
    if(!bSuccess) hppAsyncParseVar("");
}


// ----------------------------------------
// Handler for Zephyr reladed H++ functions 
// ----------------------------------------

// Evaluate Function 'aszFunctionName' with paramaters stored in variables with the name 'aszParamName' whereby
// variable names have the format '%04x_Param1'. The byte 'aszParamName[HPP_PARAM_PREFIX_LEN - 1]' is modifiyed to read the
// respectiv parameter. 
// The result is stored in the variable 'aszResultVarKey. Must return a pointer to the respective byte array of the result variable
// and change 'apcbResultLen_Out' to the respective number of bytes 
//
// No need to use the async hppVar methods (hppAsyncVarXXX). The hppVar mutex is already locked during H++ execution.    
static char* hppEvaluateZephyrFunction(char aszFunctionName[], char aszParamName[], const char aszResultVarKey[], size_t* apcbResultLen_Out)
{
	char szNumeric[HPP_NUMERIC_MAX_MEM];

    size_t cbParam2;
    char* pchParam1 = hppVarGet(aszParamName, NULL);
    aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
    char* pchParam2 = hppVarGet(aszParamName, &cbParam2);
    
    if(pchParam1 == NULL) pchParam1 = "";
    if(pchParam2 == NULL) pchParam2 = "";
    

    // io_XXX commands
    if(strncmp(aszFunctionName, "io_", 3) == 0)
    {
        // GPIO

        if(strcmp(aszFunctionName, "io_pin") == 0)   // port , pin
            return hppVarPutStr(aszResultVarKey, hppI2A(szNumeric, atoi(pchParam1) * 32 + atoi(pchParam2)), apcbResultLen_Out);

        if(strcmp(aszFunctionName, "io_set") == 0)    // pin, value (0, !=0)
        {
            gpio_pin_t pin = atoi(pchParam1);
            int val = atoi(pchParam2) == 0 ? 0 : 1;
            
            if(gpio_pin_set(hppGpioDev, pin, val) != 0) return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
                
            return hppVarPutStr(aszResultVarKey, val == 0 ? "0" : "1", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_get") == 0)    // pin
        {
            gpio_pin_t pin = atoi(pchParam1);
            int val = gpio_pin_get(hppGpioDev, pin);

            if(val < 0) hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);

            return hppVarPutStr(aszResultVarKey, hppI2A(szNumeric, val), apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_cfg_output") == 0)    // pin [, high_driver (false, true)]
        {
            gpio_pin_t pin = atoi(pchParam1);
            gpio_flags_t flags = 0;

            if(strcmp(pchParam2, "true") == 0) flags = GPIO_DS_ALT_LOW | GPIO_DS_ALT_HIGH;
            else if(*pchParam2 == 0 || strcmp(pchParam2, "false")) flags = GPIO_DS_DFLT_LOW | GPIO_DS_DFLT_HIGH;
            else return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);

            gpio_pin_configure(hppGpioDev, pin, GPIO_OUTPUT_LOW | flags); 
        
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);            
        }

        if(strcmp(aszFunctionName, "io_cfg_input") == 0)    // pin [, pull (up = 1, down = 0)]
        {
            gpio_pin_t pin = atoi(pchParam1);

            if(*pchParam2 == 0) gpio_pin_configure(hppGpioDev, pin, GPIO_INPUT | GPIO_INT_DISABLE);
            else if(atoi(pchParam2) == 0) gpio_pin_configure(hppGpioDev, pin, GPIO_INPUT | GPIO_PULL_DOWN | GPIO_INT_DISABLE);
            else if(atoi(pchParam2) == 1) gpio_pin_configure(hppGpioDev, pin, GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_DISABLE);
            else return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
                
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
        }


        // PWM

        if(strcmp(aszFunctionName, "io_pwm_restart") == 0)   // no param 
        {
            return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_pwm_stop") == 0)   // no param 
        {
            return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        }
                    
        if(strcmp(aszFunctionName, "io_pwm_start") == 0)   // count_stop, seq, val_repeat, seq_repeat, pin1 [, pin2, pin3, pin4]
        {
            return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        }

        
        // Buttons

        if(strcmp(aszFunctionName, "io_cfg_btn") == 0)    // pin [, activ_low/high(0/1), pull_up/down (1/0)] 
        {
            return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        }
              
   }

    // flash_XXX commands
    if(strncmp(aszFunctionName, "flash_", 6) == 0)
    {
        if(strcmp(aszFunctionName, "flash_save") == 0)   
        {         
            hppDeleteVarFromFlash(true);        // Delete existing data 
            return hppVarPutStr(aszResultVarKey, hppWriteVarToFlash(true) ? "true" : "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "flash_save_code") == 0)   
        {
            hppDeleteVarFromFlash(false);       // Delete existing code
            return hppVarPutStr(aszResultVarKey, hppWriteVarToFlash(false) ? "true" : "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "flash_restore") == 0)    
        {
            return hppVarPutStr(aszResultVarKey, hppReadVarFromFlash(true) ? "true" : "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "flash_delete") == 0)    
        {
            return hppVarPutStr(aszResultVarKey, hppDeleteVarFromFlash(true) ? "true" : "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "flash_delete_code") == 0)    
        {          
            return hppVarPutStr(aszResultVarKey, hppDeleteVarFromFlash(false) ? "true" : "false", apcbResultLen_Out);
        }
    }

    // timer_XXX commands
    if(strncmp(aszFunctionName, "timer_", 6) == 0)
    {
        // Parameters: timer_id, time (in ms), function name  --->  timer_id =  0..HPP_TIMER_RESOURCE_COUNT - 1 (7)     
        if(strcmp(aszFunctionName, "timer_start") == 0 || strcmp(aszFunctionName, "timer_once") == 0)   
        {
            char* pchParam3;
            unsigned int uiTimer;
            uint32_t uiTime;
            bool bSuccess = false;

            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            pchParam3 = hppVarGet(aszParamName, NULL);
            if(pchParam3 == NULL) pchParam3 = "";
   
            uiTimer = atoi(pchParam1);
            uiTime = atol(pchParam2);
            if(uiTime < HPP_MIN_TIMER_TIME) uiTime = HPP_MIN_TIMER_TIME;  // minimum time for HPP interpreter  

            if(*pchParam3 != 0) 
            {
                bSuccess = hppTimerStart(uiTimer, uiTime, aszFunctionName[6] == 's', hppTimerExecutionHandler, (void*)hppVarGetKey(pchParam3, true));
            }

            return hppVarPutStr(aszResultVarKey, bSuccess ? "true" : "false", apcbResultLen_Out);
        }

        // Parameter: timer_id     
        if(strcmp(aszFunctionName, "timer_stop") == 0)   
        {
            unsigned int uiTimer;
            bool bSuccess = false;

            uiTimer = atoi(pchParam1);   
            bSuccess = hppTimerStop(uiTimer);
            
            return hppVarPutStr(aszResultVarKey, bSuccess ? "true" : "false", apcbResultLen_Out);
        }

        // Parameters: time (in ms), function name, event  (name passed to variable "event")   
        if(strcmp(aszFunctionName, "timer_event") == 0)   
        {
            char* pchParam3;
            uint32_t uiTime;
            bool bSuccess = false;

            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            pchParam3 = hppVarGet(aszParamName, NULL);
            if(pchParam3 == NULL) pchParam3 = "";

            uiTime = atol(pchParam1);
            if(uiTime < HPP_MIN_TIMER_TIME) uiTime = HPP_MIN_TIMER_TIME;  // minimum time for HPP interpreter  

            if(*pchParam2 != 0) 
            {
                bSuccess = hppTimerScheduleEvent(uiTime, hppEventExecutionHandler, pchParam3, strlen(pchParam3) + 1, (void*)hppVarGetKey(pchParam2, true));
            }

            return hppVarPutStr(aszResultVarKey, bSuccess ? "true" : "false", apcbResultLen_Out);
        }
    }
    
    return NULL;
}	



// ------------------------------------------------------------------
// Synchronized function calls for relevant hppVarXXX functions
// ------------------------------------------------------------------

/*

bool hppSyncVarPut(const char aszKey[], const char apValue[], size_t acbValueLen)
{
    bool bRetVal;

    k_mutex_lock(&hppParseMutex, K_FOREVER);
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    bRetVal = (hppVarPut(aszKey, apValue, acbValueLen) != NULL);
    k_mutex_unlock(&hppVarMutex);
    k_mutex_unlock(&hppParseMutex);

    return bRetVal;
}

bool hppSyncVarPutStr(const char aszKey[], const char aszValue[], size_t* apcbResultLen_Out)
{
    bool bRetVal;

    k_mutex_lock(&hppParseMutex, K_FOREVER);
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    bRetVal = (hppVarPutStr(aszKey, aszValue, apcbResultLen_Out) != NULL);
    k_mutex_unlock(&hppVarMutex);
    k_mutex_unlock(&hppParseMutex);

    return bRetVal;
}


char* hppSyncVarPutBegin(const char aszKey[], const char apValue[], size_t acbValueLen)
{
    k_mutex_lock(&hppParseMutex, K_FOREVER);
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    return hppVarPut(aszKey, apValue, acbValueLen);
}

void hppSyncVarPutEnd()
{
    k_mutex_unlock(&hppVarMutex);
    k_mutex_unlock(&hppParseMutex);
}



bool hppSyncVarExists(const char aszKey[], size_t* apcbValueLen_Out)
{
    bool bRetVal;

    k_mutex_lock(&hppVarMutex, K_FOREVER);
    bRetVal =  (hppVarGet(aszKey, apcbValueLen_Out) != NULL);
    k_mutex_unlock(&hppVarMutex);

    return bRetVal;
}

char* hppSyncVarGetBegin(const char aszKey[], size_t* apcbValueLen_Out)
{
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    return hppVarGet(aszKey, apcbValueLen_Out);
}

void hppSyncVarGetEnd()
{
    k_mutex_unlock(&hppVarMutex);
}



const char* hppSyncVarGetKeyBegin(const char aszKey[], bool abCaseSensitive)
{
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    return hppVarGetKey(aszKey, abCaseSensitive);
}

void hppSyncVarGetKeyEnd()
{
    k_mutex_unlock(&hppVarMutex);
}


bool hppSyncVarDelete(const char aszKey[])
{
    bool retVal;

    k_mutex_lock(&hppParseMutex, K_FOREVER);
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    retVal = hppVarDelete(aszKey);
    k_mutex_unlock(&hppVarMutex);
    k_mutex_unlock(&hppParseMutex);

    return retVal;
}


void hppSyncVarDeleteAll(const char aszKeyPrefix[])
{
    k_mutex_lock(&hppParseMutex, K_FOREVER);
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    hppVarDeleteAll(aszKeyPrefix);
    k_mutex_unlock(&hppVarMutex);
    k_mutex_unlock(&hppParseMutex);
}


int hppSyncVarCount(const char aszKeyPrefix[], bool abExludeRootElements)
{
    int retVal;

    k_mutex_lock(&hppVarMutex, K_FOREVER);
    retVal = hppVarCount(aszKeyPrefix, abExludeRootElements);
    k_mutex_unlock(&hppVarMutex);

    return retVal;
}


int hppSyncVarGetAll(char* aszResult, size_t acbMaxLen, const char aszKeyPrefix[], const char aszFormat[])
{
    int retVal;

    k_mutex_lock(&hppVarMutex, K_FOREVER);
    retVal = hppVarGetAll(aszResult, acbMaxLen, aszKeyPrefix, aszFormat);
    k_mutex_unlock(&hppVarMutex);

    return retVal;
}
*/


// ------------------------------------------------------------------
// Synchronized function calls for H++ parser functions
// ------------------------------------------------------------------

/*
bool hppSyncParseExpression(const char* aszCode, const char* aszResultVarKey)
{
    int bRetVal;

    k_mutex_lock(&hppParseMutex, K_FOREVER);     // Parser Mutex is outside to ensure hppSyncTryLockParserMutext(..) makes parser and variable available
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    bRetVal = (hppParseExpression(aszCode, aszResultVarKey) != NULL);
    k_mutex_unlock(&hppVarMutex);
    k_mutex_unlock(&hppParseMutex);

    return bRetVal;
}

char* hppSyncParseExpressionBegin(const char* aszCode, const char* aszResultVarKey)
{
    k_mutex_lock(&hppParseMutex, K_FOREVER);
    k_mutex_lock(&hppVarMutex, K_FOREVER);
    return hppParseExpression(aszCode, aszResultVarKey);
}

void hppSyncParseExpressionEnd()
{
    k_mutex_unlock(&hppVarMutex);
    k_mutex_unlock(&hppParseMutex);
}


bool hppSyncTryLockParserMutext(int iWaitTimeInMs)
{
    if (k_mutex_lock(&hppParseMutex, K_MSEC(iWaitTimeInMs)) == 0) return true; 
    return false;
}

void hppSyncUnlockParserMutext()
{
    k_mutex_unlock(&hppParseMutex);
}
*/



bool hppSyncAddExternalFunctionLibrary(hppExternalFunctionType aExternalFunctionType)
{
    bool retVal;

    //k_mutex_lock(&hppVarMutex, K_FOREVER);
    k_mutex_lock(&hppParseMutex, K_FOREVER);
    retVal = hppAddExternalFunctionLibrary(aExternalFunctionType);
    k_mutex_unlock(&hppParseMutex);
    //k_mutex_unlock(&hppVarMutex);

    return retVal;
}


hppExternalPollFunctionType hppSyncAddExternalPollFunction(hppExternalPollFunctionType aNewExternalPollFunction)
{
    hppExternalPollFunctionType retVal;

    //k_mutex_lock(&hppVarMutex, K_FOREVER);
    k_mutex_lock(&hppParseMutex, K_FOREVER);
    retVal = hppAddExternalPollFunction(aNewExternalPollFunction);
    k_mutex_unlock(&hppParseMutex);
    //k_mutex_unlock(&hppVarMutex);

    return retVal;
}



// ------------------------------------------------------------------
// Asynchronous function call for H++ var and parser functions
// ------------------------------------------------------------------

static bool hppAsyncProcessVarInt(const char* aszVarName, uint8_t auiType)
{
    uint8_t uiType = auiType;
    uint16_t uiLen;
    bool bRetVal;

    if(aszVarName == NULL) return false;
    uiLen = strlen(aszVarName);
    if(uiLen > HPP_ASYNC_MAX_VAR_SIZE) return false;
    
    k_sched_lock();

    if(ring_buf_space_get(&hppAsyncRingBuf) >= uiLen + sizeof(uint16_t) + sizeof(uint8_t))
    {
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&uiType, sizeof(uint8_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&uiLen, sizeof(uint16_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)aszVarName, uiLen);
        k_sem_give(&hppParserSemaphor);
        bRetVal = true;
    }
    else bRetVal = false;

    k_sched_unlock();

    return bRetVal;
}


bool hppAsyncProcessDataInt(const uint8_t apData[], uint16_t acbDataLen, uint8_t auiType)
{
    uint8_t uiType = auiType;
    bool bRetVal;

    if(apData == NULL) return false;
    if(acbDataLen > HPP_ASYNC_MAX_DATA_SIZE) return false;
    
    if(!k_is_in_isr()) k_sched_lock();

    if(ring_buf_space_get(&hppAsyncRingBuf) >= acbDataLen + sizeof(uint16_t))
    {
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&uiType, sizeof(uint8_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&acbDataLen, sizeof(uint16_t));
        ring_buf_put(&hppAsyncRingBuf, apData, acbDataLen);
        k_sem_give(&hppParserSemaphor);
        bRetVal = true;
    }
    else bRetVal = false;

    if(!k_is_in_isr()) k_sched_unlock();

    return bRetVal;
}


bool hppAsyncVarGetFromUriCoapResponse(const char aszVarName[])
{
    return hppAsyncProcessVarInt(aszVarName, HPP_ASYNC_TLV_VAR_GET_COAP_RESPONSE);
}


bool hppAsyncVarPutInt(const char aszVarName[], const char apValue[], uint16_t acbValueLen, uint8_t auiType)
{
    uint8_t uiType = auiType;
    uint16_t uiVarNameLen;
    bool bRetVal;

    if(aszVarName == NULL || apValue == NULL) return false;
    uiVarNameLen = strlen(aszVarName);
    if(uiVarNameLen > HPP_ASYNC_MAX_VAR_SIZE) return false;
    if(acbValueLen > HPP_ASYNC_MAX_DATA_SIZE) return false;
    
    if(!k_is_in_isr()) k_sched_lock();

    if(ring_buf_space_get(&hppAsyncRingBuf) >= acbValueLen + uiVarNameLen + 2*sizeof(uint16_t) + sizeof(uint8_t) + HPP_ASYNC_RINGBUF_SAFETY_BUFFER)
    {
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&uiType, sizeof(uint8_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&uiVarNameLen, sizeof(uint16_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)aszVarName, uiVarNameLen);
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&acbValueLen, sizeof(uint16_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)apValue, acbValueLen);
        k_sem_give(&hppParserSemaphor);
        bRetVal = true;
    }
    else bRetVal = false;

    if(!k_is_in_isr()) k_sched_unlock();

    return bRetVal;
}

bool hppAsyncVarPut(const char aszVarName[], const char apValue[], uint16_t acbValueLen, bool abSyncWithNext)
{
    uint8_t uiType = abSyncWithNext ? HPP_ASYNC_TLV_VAR_PUT_WITH_NEXT : HPP_ASYNC_TLV_VAR_PUT;

    return hppAsyncVarPutInt(aszVarName, apValue, acbValueLen, uiType);
}

bool hppAsyncVarPutStr(const char aszVarName[], const char aszValue[], bool abSyncWithNext)
{
    uint8_t uiType = abSyncWithNext ? HPP_ASYNC_TLV_VAR_PUT_WITH_NEXT : HPP_ASYNC_TLV_VAR_PUT;

    return hppAsyncVarPut(aszVarName, aszValue, strlen(aszValue), uiType);
}

bool hppAsyncVarPutFromUri(const char aszVarName[], const char apValue[], uint16_t acbValueLen)
{
    return hppAsyncVarPutInt(aszVarName, apValue, acbValueLen, HPP_ASYNC_TLV_VAR_PUT_NON_CASE_SENSITIVE);
}


bool hppAsyncVarDelete(const char aszVarName[])
{
    return hppAsyncProcessVarInt(aszVarName, HPP_ASYNC_TLV_VAR_DELETE);
}


bool hppAsyncParseExpression(const char* aszCode, bool aWriteResultToCli)
{
    return hppAsyncProcessDataInt(aszCode, strlen(aszCode), aWriteResultToCli ? HPP_ASYNC_TLV_PARSE_TEXT_CLI : HPP_ASYNC_TLV_PARSE_TEXT);
}


bool hppAsyncParseVar(const char* aszVarName)
{
    return hppAsyncProcessVarInt(aszVarName, HPP_ASYNC_TLV_PARSE_VAR);
}


bool hppAsyncParseVarCoapResponse(const char* aszVarName)
{
    return hppAsyncProcessVarInt(aszVarName, HPP_ASYNC_TLV_PARSE_VAR_COAP_RESPONSE);
}


bool hppAsyncVarGetWellKnownCore()
{
    return hppAsyncProcessVarInt("", HPP_ASYNC_TLV_VAR_GET_WKC_COAP_RESPONSE);
}


bool hppAsyncVarHide(const char* aszPassword)
{
    if(aszPassword == NULL) aszPassword = "";
    return hppAsyncProcessVarInt(aszPassword, HPP_ASYNC_TLV_VAR_HIDE);
}



// ------------------------------------------------------------------
// Asynchronous function call for H++ Thread functions
// ------------------------------------------------------------------

bool hppAsyncSetCoapContext(hppCoapMessageContext* apCoapMessageContext, bool abSyncWithNext)
{
    uint8_t uiType = abSyncWithNext ? HPP_ASYNC_TLV_SET_COAP_CONTEXT_WITH_NEXT : HPP_ASYNC_TLV_SET_COAP_CONTEXT;
    uint16_t uiLen = sizeof(hppCoapMessageContext);
    bool bRetVal;

    if(apCoapMessageContext == NULL) return false;
  
    if(!k_is_in_isr()) k_sched_lock();

    if(ring_buf_space_get(&hppAsyncRingBuf) >= uiLen + sizeof(uint16_t) + sizeof(uint8_t) + HPP_ASYNC_RINGBUF_SAFETY_BUFFER)
    {
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&uiType, sizeof(uint8_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)&uiLen, sizeof(uint16_t));
        ring_buf_put(&hppAsyncRingBuf, (uint8_t*)apCoapMessageContext, uiLen);
        k_sem_give(&hppParserSemaphor);
        bRetVal = true;
    }
    else bRetVal = false;

    if(!k_is_in_isr()) k_sched_unlock();

    return bRetVal;
}


// ------------------------------------------------------------------
// Asynchronous function call for Usb Input
// ------------------------------------------------------------------

bool hppAsyncUsbInput(const uint8_t apData[], uint16_t acbDataLen)
{
    return hppAsyncProcessDataInt(apData, acbDataLen, HPP_ASYNC_TLV_USB_RECV);
}




// ------------------------------------------------------------------
// Timer functions 
// ------------------------------------------------------------------

static void hppTimerHandlerInt(struct k_timer *aTimer)
{
    uint32_t iTimerID = (uint32_t)k_timer_user_data_get(aTimer);

    if(iTimerID < HPP_TIMER_RESOURCE_COUNT)
    {
       hppAsyncProcessDataInt((uint8_t*) &iTimerID, sizeof(uint32_t), HPP_ASYNC_TLV_TIMER_EXPIRED);
    }
}


bool hppTimerStart(uint32_t aTimerID, uint32_t aTime, bool aIsContinous, hppTimerHandler aTimerHandler, void *apContext)
{
    k_timeout_t theDuration = K_MSEC(aTime);
    k_timeout_t thePeriod = aIsContinous ? K_MSEC(aTime) :  K_NO_WAIT;

    if(aTimerID >= HPP_TIMER_RESOURCE_COUNT) return false;
    if(aTimerHandler == NULL) return false;

    hppTimerResources[aTimerID].pHandler = aTimerHandler;
    hppTimerResources[aTimerID].pContext = apContext;

    k_timer_user_data_set(&(hppTimerResources[aTimerID].mTimerResource), (void*) aTimerID);
    k_timer_start(&(hppTimerResources[aTimerID].mTimerResource), theDuration, thePeriod);

    return true;
}


bool hppTimerStop(uint32_t aTimerID)
{
    if(aTimerID >= HPP_TIMER_RESOURCE_COUNT) return false;

    k_timer_stop(&(hppTimerResources[aTimerID].mTimerResource));

    return true;
}


// Scheduled Events
static void hppEventTimerHandlerInt(struct k_timer *apTimer)
{
    uint32_t iTimerID = (uint32_t)k_timer_user_data_get(apTimer);

    if(iTimerID < HPP_EVENT_TIMER_RESOURCE_COUNT)
    {
        if(!hppAsyncProcessDataInt((uint8_t*) &iTimerID, sizeof(uint32_t), HPP_ASYNC_TLV_EVENT_TIMER_EXPIRED))
        {
            // Release resource here if the schedule queue is full
            hppEventTimerResource* pEventTimer = &(hppEventTimerResources[iTimerID]);

            if(pEventTimer->pApplicationData) free(pEventTimer->pApplicationData);
            pEventTimer->pApplicationData = NULL;
            pEventTimer->mInUse = false;
        }
    }
}


bool hppTimerScheduleEvent(uint32_t aDelayFromNow, hppQueuedTimerHandler apHandler, void* apData, uint32_t aDataLen, void *apContext)
{
    k_timeout_t theDuration = K_MSEC(aDelayFromNow);
    void* pAppData;
    uint32_t i;

    if(apHandler == NULL) return false;

    k_sched_lock();
    
    for(i = 0; i < HPP_EVENT_TIMER_RESOURCE_COUNT; i++) 
    {
        if(hppEventTimerResources[i].mInUse == false) 
        {
            hppEventTimerResources[i].mInUse = true;
            break;
        }
    }

    k_sched_unlock();

    if(i >= HPP_EVENT_TIMER_RESOURCE_COUNT) return false;

    if(apData != NULL && aDataLen > 0)
    {
        pAppData = malloc(aDataLen);
        if(pAppData == NULL) 
        {
            hppEventTimerResources[i].mInUse = false;
            return false;
        }

        memcpy(pAppData, apData, aDataLen);
        hppEventTimerResources[i].pApplicationData = pAppData;
        hppEventTimerResources[i].mApplicationDataLen = aDataLen;
    }

    hppEventTimerResources[i].pContext = apContext;
    hppEventTimerResources[i].pHandler = apHandler;

    k_timer_user_data_set(&(hppEventTimerResources[i].mTimerResource), (void*) i);
    k_timer_start(&(hppEventTimerResources[i].mTimerResource), theDuration, K_NO_WAIT);

    return true;
}



// ------------------------------------------------------------------
// Zephyr main Loop
// ------------------------------------------------------------------

static void hppUserModeMain(void *p1, void *p2, void *p3)
{
    uint8_t uiType;
    uint16_t uiLen;
    char* pchCode;
    const char* pchVarKey;
    char* pchCli = NULL;
    bool bSendCoapResponse = false;
    char* pchCoap = NULL;
    size_t iCoapResponseLen = 0;
    char* pchResult;
    bool bKeepLocked = false;
    uint32_t uiTimerID;
    hppTimerResource* pTimer;
    hppEventTimerResource* pEventTimer;

    LOG_INF("main (user mode) priority: %d", k_thread_priority_get(k_current_get()));

    while(1)
    {
        k_sem_take(&hppParserSemaphor, K_FOREVER);

        // Single reader use-case, no locking needed
        ring_buf_get(&hppAsyncRingBuf, (uint8_t*)&uiType, sizeof(uint8_t));
        ring_buf_get(&hppAsyncRingBuf, (uint8_t*)&uiLen, sizeof(uint16_t));

        if(!bKeepLocked)     // already locked from preceeding operation?
        {
            //k_mutex_lock(&hppVarMutex, K_FOREVER);
            k_mutex_lock(&hppParseMutex, K_FOREVER);
        }

        bKeepLocked = false;   // unless needed the next command, the mutexes shall be unlocked after execution 

        switch(uiType)
        {
            case HPP_ASYNC_TLV_VAR_GET_COAP_RESPONSE:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncVarName, uiLen);
                hppAsyncVarName[uiLen] = 0;

                if(uiLen > 0 && hppMyCurrentCoapMessageContext.mCoapMessageTokenLength != 0)
                {
                    bSendCoapResponse = true;
                    pchVarKey = hppVarGetKey(hppAsyncVarName, false);
                    
                    if(pchVarKey != NULL) pchCoap = hppVarGet(pchVarKey, &iCoapResponseLen);
                    else pchCoap = NULL;
                }
                else
                {
                    // Invalidate CoAP call context
                    hppMyCurrentCoapMessageContext.mCoapMessageTokenLength = 0;
                    hppMyCurrentCoapMessageContext.mCoapCode = OT_COAP_CODE_EMPTY;
                }
            break;

            case HPP_ASYNC_TLV_VAR_PUT_WITH_NEXT:
                bKeepLocked = true;
            case HPP_ASYNC_TLV_VAR_PUT_NON_CASE_SENSITIVE:
            case HPP_ASYNC_TLV_VAR_PUT:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncVarName, uiLen);
                hppAsyncVarName[uiLen] = 0;
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)&uiLen, sizeof(uint16_t));
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncDataBuffer, uiLen);

                if(uiType == HPP_ASYNC_TLV_VAR_PUT_NON_CASE_SENSITIVE) pchVarKey = hppVarGetKey(hppAsyncVarName, false);
                else pchVarKey = NULL;

                if(pchVarKey != NULL) hppVarPut(pchVarKey, hppAsyncDataBuffer, uiLen);  
                else hppVarPut(hppAsyncVarName, hppAsyncDataBuffer, uiLen);         // case sensitve or new var
            break;

            case HPP_ASYNC_TLV_VAR_DELETE:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncVarName, uiLen);
                hppAsyncVarName[uiLen] = 0;
                hppVarDelete(hppAsyncVarName);
            break;

            case HPP_ASYNC_TLV_SET_COAP_CONTEXT_WITH_NEXT:
                bKeepLocked = true;
            case HPP_ASYNC_TLV_SET_COAP_CONTEXT:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)&hppMyCurrentCoapMessageContext, sizeof(hppMyCurrentCoapMessageContext));
            break;

            case HPP_ASYNC_TLV_PARSE_TEXT:
            case HPP_ASYNC_TLV_PARSE_TEXT_CLI:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncDataBuffer, uiLen);
                hppAsyncDataBuffer[uiLen] = 0;
                pchResult = hppParseExpression(hppAsyncDataBuffer, "ReturnWithError");
                hppVarDelete("ReturnWithError");

                if(uiType == HPP_ASYNC_TLV_PARSE_TEXT_CLI) pchCli = pchResult;
                else hppVarDelete("ReturnWithError");
            break;

            case HPP_ASYNC_TLV_PARSE_VAR:
            case HPP_ASYNC_TLV_PARSE_VAR_COAP_RESPONSE:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncVarName, uiLen);
                hppAsyncVarName[uiLen] = 0;

                if(uiLen > 0) pchCode = hppVarGet(hppAsyncVarName, NULL);
                else pchCode = NULL;  // hppAsyncParseVar("") is used to unlock the mutex in case of  XXX_WITH_NEXT operation which was not fully executed

                if(pchCode != NULL)
                {
                    pchResult = hppParseExpression(pchCode,  "ReturnWithError");
                }
                else pchResult = "not found";  

                // Create CoAP Response if the H++ itself did not respond yet
                if(uiType == HPP_ASYNC_TLV_PARSE_VAR_COAP_RESPONSE)
                {
                    if(hppMyCurrentCoapMessageContext.mHasResponded == 0 && hppMyCurrentCoapMessageContext.mCoapMessageTokenLength != 0) 
                    {
                        pchCoap = pchResult;
                    }
                    
                    if(pchCoap == NULL)
                    {
                        hppVarDelete("ReturnWithError");
                        // Invalidate CoAP call context
                        hppMyCurrentCoapMessageContext.mCoapMessageTokenLength = 0;
                        hppMyCurrentCoapMessageContext.mCoapCode = OT_COAP_CODE_EMPTY;
                    }
                    else 
                    {
                        bSendCoapResponse = true; 
                        iCoapResponseLen = strlen(pchCoap);
                    }
                }
            break;

            case HPP_ASYNC_TLV_VAR_GET_WKC_COAP_RESPONSE:   
                if(uiLen == 0 && hppMyCurrentCoapMessageContext.mCoapMessageTokenLength != 0)
                {
                    pchCoap = hppNew_WellknownCore();
                    bSendCoapResponse = true;

                    if(pchCoap != NULL) iCoapResponseLen = strlen(pchCoap);   
                }
                else
                {
                    // Invalidate CoAP call context
                    hppMyCurrentCoapMessageContext.mCoapMessageTokenLength = 0;
                    hppMyCurrentCoapMessageContext.mCoapCode = OT_COAP_CODE_EMPTY;
                }
            break;

            case HPP_ASYNC_TLV_VAR_HIDE:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncVarName, uiLen);    // hppAsyncVarName is password
                hppAsyncVarName[uiLen] = 0;

                hppHideVarResources = true;
                pchCode = hppVarGet("Var_Hide_PW", NULL);

                if(pchCode == NULL) pchCode = HPP_HIDE_VAR_RESOURCE_PASSWORD;       // default password 

                if(strcmp(hppAsyncVarName, pchCode) == 0) hppHideVarResources = false;

                LOG_INF("hide: %s == %s", hppAsyncVarName, pchCode);
            break;

            case HPP_ASYNC_TLV_USB_RECV:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)hppAsyncDataBuffer, uiLen);
                hppAsyncDataBuffer[uiLen] = 0;
                shell_execute_cmd(shell_backend_uart_get_ptr(), hppAsyncDataBuffer);   // Send to H++ handler instead?
            break;

            case HPP_ASYNC_TLV_TIMER_EXPIRED:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)&uiTimerID, sizeof(uint32_t));
                if(uiTimerID >= HPP_TIMER_RESOURCE_COUNT) break;
                
                pTimer = &(hppTimerResources[uiTimerID]);
                if(pTimer->pHandler) (pTimer->pHandler)(pTimer->pContext);
            break;

            case HPP_ASYNC_TLV_EVENT_TIMER_EXPIRED:
                ring_buf_get(&hppAsyncRingBuf, (uint8_t*)&uiTimerID, sizeof(uint32_t));
                if(uiTimerID >= HPP_EVENT_TIMER_RESOURCE_COUNT) break;
                
                pEventTimer = &(hppEventTimerResources[uiTimerID]);
                if(pEventTimer->pHandler) (pEventTimer->pHandler)(pEventTimer->pApplicationData, pEventTimer->mApplicationDataLen, pEventTimer->pContext);
                if(pEventTimer->pApplicationData) free(pEventTimer->pApplicationData);
                pEventTimer->pApplicationData = NULL;
                pEventTimer->mInUse = false;
            break;
        }

        if(!bKeepLocked)
        {
            k_mutex_unlock(&hppParseMutex);
            //k_mutex_unlock(&hppVarMutex);
        }


        // Functions in hppThread.c require locking the Thread mutex. Avoid doing that while the variable mutex is locked to prevent deadlock situation.

        // Write result to Thread CLI
        if(pchCli != NULL)
        {
            openthread_api_mutex_lock(hppOpenThreadContext); 
            hppCliOutputStr(pchCli, true);              
            openthread_api_mutex_unlock(hppOpenThreadContext);
          
            k_mutex_lock(&hppParseMutex, K_FOREVER);
            //k_mutex_lock(&hppVarMutex, K_FOREVER);
            hppVarDelete("ReturnWithError");
            //k_mutex_unlock(&hppVarMutex);
            k_mutex_unlock(&hppParseMutex);
        
            pchCli = NULL;
        }

        // Send result with CoAP
        if(bSendCoapResponse == true)
        {
            openthread_api_mutex_lock(hppOpenThreadContext);
            hppCoapRespondTo(&hppMyCurrentCoapMessageContext, pchCoap, iCoapResponseLen, uiType == HPP_ASYNC_TLV_VAR_GET_WKC_COAP_RESPONSE ? 40 : 0);
            
            // Invalidate CoAP call context
            hppMyCurrentCoapMessageContext.mCoapMessageTokenLength = 0;
            hppMyCurrentCoapMessageContext.mCoapCode = OT_COAP_CODE_EMPTY;
            
            openthread_api_mutex_unlock(hppOpenThreadContext);

            if(uiType ==  HPP_ASYNC_TLV_PARSE_VAR || uiType == HPP_ASYNC_TLV_PARSE_VAR_COAP_RESPONSE)
            {

                k_mutex_lock(&hppParseMutex, K_FOREVER);
                //k_mutex_lock(&hppVarMutex, K_FOREVER);
                hppVarDelete("ReturnWithError");
                //k_mutex_unlock(&hppVarMutex);
                k_mutex_unlock(&hppParseMutex);
            }

            if(uiType == HPP_ASYNC_TLV_VAR_GET_WKC_COAP_RESPONSE && pchCoap != NULL)
            {
                free(pchCoap);     // response has been dynamically allocated with hppNew_WellknownCore()
            }
            
            bSendCoapResponse = false;
            iCoapResponseLen = 0;
            pchCoap = NULL;
        }
    }
}


void hppMainLoop()
{ 
    LOG_INF("main priority: %d", k_thread_priority_get(k_current_get()));

    // Downgrade thread to lower priority 
    k_thread_priority_set(k_current_get(), 10);    // Network runs on 7, OpenThreand on 8

    LOG_INF("main priority: %d", k_thread_priority_get(k_current_get()));

    // Downgrade thread to user mode 
    k_thread_user_mode_enter(hppUserModeMain, NULL, NULL, NULL);
}


// ------------------------------------------------------------------
// USB Communication
// ------------------------------------------------------------------

static void hppZephyrUsbInterruptHandler(const struct device *apDev, void *apUuserData)
{
    int iLen;
    uint8_t ch;
	ARG_UNUSED(apUuserData);

	while(uart_irq_update(apDev) && uart_irq_is_pending(apDev))
	{
		if(uart_irq_rx_ready(apDev))
		{
            do 
            {
                if(hppZephyrUsbRecvBufBufferLen == 0)
                {
                    strcpy(hppZephyrUsbRecvBufBuffer, "ot ");       // Simulate OT command to Zephyr shell
                    hppZephyrUsbRecvBufBufferLen = 3;
                }

                iLen = uart_fifo_read(apDev, &ch, 1);
                if(ch == '\"' || ch == '\'') hppZephyrUsbRecvBufBuffer[hppZephyrUsbRecvBufBufferLen++] = '\\';   // escape " and '
                if(iLen == 1) hppZephyrUsbRecvBufBuffer[hppZephyrUsbRecvBufBufferLen++] = ch;
                if(hppZephyrUsbRecvBufBufferLen >= HPP_ZEPHYR_USB_RECV_BUF_SIZE - 1) hppZephyrUsbRecvBufBufferLen = 0;  // Drop data in case of buffer overflow
            }
            while(iLen == 1 && ch != '\n' && ch != '\r');

            if((ch == '\n' || ch == '\r') && hppZephyrUsbRecvBufBufferLen > 0) 
            {
                if(hppZephyrUsbRecvBufBufferLen > 3)
                hppAsyncUsbInput(hppZephyrUsbRecvBufBuffer, hppZephyrUsbRecvBufBufferLen - 1);     // Drop '\n' or '\r'

                hppZephyrUsbRecvBufBufferLen = 0;
            }
		}

		if(uart_irq_tx_ready(apDev))
		{
            iLen = ring_buf_get(&hppZephyrUsbRingBuf, &ch, 1);    // read one byte, uart_irq_tx_ready(..) only guarantees ne more byte 

            if(iLen > 0) uart_fifo_fill(apDev, &ch, iLen);
            else 
            {
				uart_irq_tx_disable(apDev);
				continue;
			}
		}
	}
}


void hppZephyrUsbStatusCallbackHandler(enum usb_dc_status_code cb_status, const uint8_t *param)
{
    switch(cb_status)
    {
        case USB_DC_CONNECTED:      printf("CONNECT\r\n");
                                    break;

        case USB_DC_DISCONNECTED:   printf("DISCONNECT\r\n");
                                    break;

        default:                    break;
    }
}


int hppZephyrUsbSend(const uint8_t apData[], uint16_t acbDataLen)
{
    int iLen;

    if(apData == NULL || acbDataLen == 0) return 0;

    iLen = ring_buf_put(&hppZephyrUsbRingBuf, apData, acbDataLen);
    
    if(iLen <= 0) ring_buf_reset(&hppZephyrUsbRingBuf);   // empty buffer and start over next time  
    else uart_irq_tx_enable(hppUsbDev); 

    return iLen > 0 ? iLen : 0;
}


static void hppZephyrUsbInit()
{
	uint32_t uiBaudrate;
    int ret;

    hppUsbDev = device_get_binding("CDC_ACM_0");
    usb_enable(NULL);
    usb_dc_set_status_callback(hppZephyrUsbStatusCallbackHandler);

    ring_buf_init(&hppZephyrUsbRingBuf, sizeof(hppZephyrUsbRingBufBuffer), hppZephyrUsbRingBufBuffer);    

    ret = uart_line_ctrl_get(hppUsbDev, UART_LINE_CTRL_BAUD_RATE, &uiBaudrate);
    
	if(ret) LOG_WRN("Failed to get USB baudrate, ret code %d", ret); 
	else LOG_INF("USB baudrate: %d", uiBaudrate);

	uart_irq_callback_set(hppUsbDev, hppZephyrUsbInterruptHandler);
    
	/* Enable rx interrupts */
	uart_irq_rx_enable(hppUsbDev);
}


// ------------------------------------------------------------------
// Zephyr initialization
// ------------------------------------------------------------------

void hppZephyrInit()
{
    int i;

    hppGpioDev = device_get_binding("GPIO_0");
    ring_buf_init(&hppAsyncRingBuf, sizeof(hppAsyncRingBufBuffer), hppAsyncRingBufBuffer);
    hppZephyrUsbInit();

    // Init timer objects
    for(i = 0 ; i < HPP_TIMER_RESOURCE_COUNT ; i++)
    {
        k_timer_init(&(hppTimerResources[i].mTimerResource), hppTimerHandlerInt, NULL);
    } 

    for(i = 0 ; i < HPP_EVENT_TIMER_RESOURCE_COUNT ; i++)
    {
        k_timer_init(&(hppEventTimerResources[i].mTimerResource), hppEventTimerHandlerInt, NULL);
    }

  
	LOG_INF("Start Halloween");

	if(IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) 
	{
		power_down_unused_ram();
	}

    hppSyncAddExternalFunctionLibrary(hppEvaluateZephyrFunction);
}