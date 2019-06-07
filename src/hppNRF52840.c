/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppNRF52840.c												            */
/* ----------------------------------------------------------------------------	*/
/* OpenThread Support Library										            */
/*																	            */
/*   - Timer handling                       						            */
/*   - GPIO & PWM handling 			       							           	*/
/*   - CoAP support functions										            */
/*   - Flash memory hndling 										            */
/*   - H++ bindings for Timer, GPIO and PWM                                     */
/* ----------------------------------------------------------------------------	*/
/* Platform: OpenThread on Nordic nRF52840							            */
/* Dependencies: Nordic SDK; If activated: hppVarStorage, hppParser             */
/* Tested with SDK Version:  nRF5_SDK_for_Thread_and_Zigbee_v3.0.0_d310e71	    */
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

#include "../include/hppNRF52840.h"
#include "../hppConfig.h"

// Platform SDK
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// SDK Settings
#include "sdk_config.h"

// GPIO
#include "nrf_gpio.h"
#include "nrf_nvmc.h"


// ---------------------------
// Byte alignment
// ---------------------------

#ifdef __arm__
#define __hpp_packed __packed            
#else
#define __hpp_packed
#warning no arm environment
#endif

// -----------------------
// Static Variables
// -----------------------

#ifdef __INCL_HPP_PARSER_

#define HPP_MIN_TIMER_TIME 20

// Timer Resources
APP_TIMER_DEF(m_HPP_timer_1);
APP_TIMER_DEF(m_HPP_timer_2);
APP_TIMER_DEF(m_HPP_timer_3);
APP_TIMER_DEF(m_HPP_timer_4);
APP_TIMER_DEF(m_HPP_timer_5);
APP_TIMER_DEF(m_HPP_timer_6);
APP_TIMER_DEF(m_HPP_timer_7);
APP_TIMER_DEF(m_HPP_timer_8);

static app_timer_id_t const* hppTimerResourceTable[HPP_TIMER_RESOURCE_MAX] = {  &m_HPP_timer_1, &m_HPP_timer_2,
                                                                                &m_HPP_timer_3, &m_HPP_timer_4,
                                                                                &m_HPP_timer_5, &m_HPP_timer_6,
                                                                                &m_HPP_timer_7, &m_HPP_timer_8 }; 

#ifdef __INCL_HPP_THREAD_
static uint32_t hppButtoPressTime = 0;
#endif

#endif

// --------------------
// PWM Functions 
// --------------------

// Start a PMW Sequence for up to four GIO pins (-1 = not active).
// The frequency is 16MHz / uiCountStop. Duty cycle values are provided in the array puiSeqence in groups of 
// one, two or four duty cycle values in uint16_t format, depending on how many pins are active.
// If only iPin1 is activ (>= 0) than one uint16_t is consumed per sequence to set the PWM value for pin1.
// If iPin1 and iPin2 are active but not iPin3 and iPin4, two uint16_t are consumed per sequence respectively.
// Otherwise always(!) four uint16_t are consumed. The use of three channels is not possible by a hardware restriction
// even if iPin4 is not activ (-1). 

// The cSequenceLen is the number of sequence values (1x, 2x or 4x uint16_t values respectively) provided.
// The values are not not buffered. Therefore puiSeqence must remain in RAM wihile PWM is active.
// Duty cycle values range from 0 to uiCountStop (100%). 
// Every PWM setting is repeated 'uiStepRepeat' times until the next values are taken. If 'uiStepRepeat' is null then
// the PMW values are static. The Sequence is repeated uiLoop times (uiLoop = 0 --> no Loop)
// If uiLoop is >0 cSequenceLen must be minimum 8 (two value groups).

void hppStartPWM(unsigned int uiCountStop, int iPin1, int iPin2, int iPin3, int iPin4, uint16_t* puiSeqence, size_t cSequenceLen, unsigned int uiStepRepeat, unsigned int uiLoop)
{
    if(uiLoop > 0 && cSequenceLen < 2) return;  //error

    NRF_PWM0->TASKS_SEQSTART[0] = 0;
    NRF_PWM0->TASKS_SEQSTART[1] = 0;

    if(iPin1 < 0) NRF_PWM0->PSEL.OUT[0] = PWM_PSEL_OUT_CONNECT_Disconnected << PWM_PSEL_OUT_CONNECT_Pos;
    else          NRF_PWM0->PSEL.OUT[0] = (iPin1 << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
    
    if(iPin3 >= 0 || iPin4 >= 0)
    {
        if(iPin2 < 0) NRF_PWM0->PSEL.OUT[1] = PWM_PSEL_OUT_CONNECT_Disconnected << PWM_PSEL_OUT_CONNECT_Pos;
        else          NRF_PWM0->PSEL.OUT[1] = (iPin2 << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
        
        if(iPin3 < 0) NRF_PWM0->PSEL.OUT[2] = PWM_PSEL_OUT_CONNECT_Disconnected << PWM_PSEL_OUT_CONNECT_Pos;
        else          NRF_PWM0->PSEL.OUT[2] = (iPin3 << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
    }
    else  // Attache PWM Counter 1 to pin and 3 PWM Counter 3 to pin 2 if only one or two pins are active   
    {
        if(iPin2 < 0) NRF_PWM0->PSEL.OUT[2] = PWM_PSEL_OUT_CONNECT_Disconnected << PWM_PSEL_OUT_CONNECT_Pos;
        else          NRF_PWM0->PSEL.OUT[2] = (iPin2 << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
        
        if(iPin3 < 0) NRF_PWM0->PSEL.OUT[1] = PWM_PSEL_OUT_CONNECT_Disconnected << PWM_PSEL_OUT_CONNECT_Pos;
        else          NRF_PWM0->PSEL.OUT[1] = (iPin3 << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
    }
    
        
    if(iPin4 < 0) NRF_PWM0->PSEL.OUT[3] = PWM_PSEL_OUT_CONNECT_Disconnected << PWM_PSEL_OUT_CONNECT_Pos;
    else          NRF_PWM0->PSEL.OUT[3] = (iPin4 << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);

    NRF_PWM0->ENABLE = (PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM0->MODE = (PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos);
    NRF_PWM0->PRESCALER = (PWM_PRESCALER_PRESCALER_DIV_1 << PWM_PRESCALER_PRESCALER_Pos); 
    NRF_PWM0->COUNTERTOP = (uiCountStop << PWM_COUNTERTOP_COUNTERTOP_Pos);  // 16000 => 1 msec

    if (uiLoop == 0) NRF_PWM0->LOOP = (PWM_LOOP_CNT_Disabled << PWM_LOOP_CNT_Pos);
    else             NRF_PWM0->LOOP = (uiLoop << PWM_LOOP_CNT_Pos);

    if(iPin3 < 0 && iPin4 < 0)
    {
        if(iPin2 < 0) NRF_PWM0->DECODER = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) | (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
        else 
        {
            cSequenceLen = cSequenceLen * 2;   // two int16_t per sequence item
            NRF_PWM0->DECODER = (PWM_DECODER_LOAD_Grouped << PWM_DECODER_LOAD_Pos) | (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);  
        }
    }
    else 
    {
        cSequenceLen = cSequenceLen * 4;  // four int16_t per sequence item
        NRF_PWM0->DECODER = (PWM_DECODER_LOAD_Individual << PWM_DECODER_LOAD_Pos) | (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
    }

    if(uiLoop == 0)
    {
        NRF_PWM0->SEQ[0].PTR = (uint32_t)(puiSeqence) << PWM_SEQ_PTR_PTR_Pos;
        NRF_PWM0->SEQ[0].CNT = cSequenceLen << PWM_SEQ_CNT_CNT_Pos;
        NRF_PWM0->SEQ[0].REFRESH = uiStepRepeat;
        NRF_PWM0->SEQ[0].ENDDELAY = 0;
    }
    else
    {
        NRF_PWM0->SEQ[0].PTR = (uint32_t)(puiSeqence) << PWM_SEQ_PTR_PTR_Pos;
        NRF_PWM0->SEQ[0].CNT = 4 << PWM_SEQ_CNT_CNT_Pos;
        NRF_PWM0->SEQ[0].REFRESH = uiStepRepeat;
        NRF_PWM0->SEQ[0].ENDDELAY = 0;
        NRF_PWM0->SEQ[1].PTR = (uint32_t)(puiSeqence + 4) << PWM_SEQ_PTR_PTR_Pos;
        NRF_PWM0->SEQ[1].CNT = (cSequenceLen - 4) << PWM_SEQ_CNT_CNT_Pos;
        NRF_PWM0->SEQ[1].REFRESH = uiStepRepeat;
        NRF_PWM0->SEQ[1].ENDDELAY = 0;
    }

    NRF_PWM0->TASKS_SEQSTART[0] = 1; 
}

void hppRestartPWM()
{
    NRF_PWM0->TASKS_SEQSTART[0] = 0;
    NRF_PWM0->ENABLE = (PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM0->TASKS_SEQSTART[0] = 1;
}

void hppStopPWM()
{
    NRF_PWM0->TASKS_SEQSTART[0] = 0;
    NRF_PWM0->ENABLE = (PWM_ENABLE_ENABLE_Disabled << PWM_ENABLE_ENABLE_Pos);
}

// ---------------------------------------
// Flash Memory Functions & HppVar Bindigs 
// ---------------------------------------

#ifdef __INCL_HPP_VAR_

// Store variables in flash memory starting with page pFlashMeoryPage (4k aligned memory address).
// Stores all variables starting with a lower case letter if bGlobal == false (injected H++ code) or all upper case
// variabes if bGlobal == true (global scope H++ valiables). Local scope variables are not stored.   
// Writes the file ID 'auiID' at the beginning of the flash memory page after all variables have been written.
// Do not use reserved IDs 0xfffffffe (invalid = HPP_INVALID_FLASH_ID) and 0xffffffff (empty). 
// Returns the number of bytes written or 0 in case of error. 
size_t hppWriteVar2Flash(uint32_t aFlashMemoryPage, bool abGlobal, size_t acbMaxLen, uint32_t auiID)
{
	struct hppVarListStruct* searchVar;
    char chBegin = 'a';
    char chEnd = 'z';
    size_t cbSize = sizeof(uint32_t) + 1;  // Leave extra bytes for the ID at the begining and 1 byte of EOF marker
    size_t iWritePos = 0; 
    
	if(aFlashMemoryPage == 0) return 0;              
    if(auiID == 0xffffffff || auiID == HPP_INVALID_FLASH_ID) return 0;   // not a valid ID 

    if((aFlashMemoryPage & 0xfff) != 0) return 0;   // Must start at the beginning of a page 
    if(abGlobal) { chBegin = 'A'; chEnd = 'Z'; }
	searchVar = pFirstVar;

	// Search for the right entries in the list and calculate the total memroy consuption
	while(searchVar != NULL)
	{
		if(searchVar->szKey[0] >= chBegin && searchVar->szKey[0] <= chEnd) 
            cbSize += strlen(searchVar->szKey) + 1 + sizeof(size_t) + searchVar->cbValueLen;
		searchVar = searchVar->pNext;
	}

    // Check if the total memory required is within the given flash budget
    if(cbSize > acbMaxLen) return 0;

    // Delete enough flash pages to store the variables 
    while(iWritePos < cbSize)
    {
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
        //wait_for_flash_ready();
        nrf_nvmc_page_erase(aFlashMemoryPage + iWritePos);
        iWritePos += 4096;
    }

    // Write varibales in flash memory
    iWritePos = sizeof(uint32_t);   // Leave extra bytes for the ID at the beginning
    searchVar = pFirstVar;

	// Search for the right entries in the list and calculate the total memroy consuption
	while(searchVar != NULL)
	{
		if(searchVar->szKey[0] >= chBegin && searchVar->szKey[0] <= chEnd) 
        {
            while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
            nrf_nvmc_write_bytes(aFlashMemoryPage + iWritePos, (uint8_t*)(searchVar->szKey), strlen(searchVar->szKey) + 1);
            iWritePos += strlen(searchVar->szKey) + 1;
            while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
            nrf_nvmc_write_bytes(aFlashMemoryPage + iWritePos, (uint8_t*)(&(searchVar->cbValueLen)), sizeof(size_t));
            iWritePos += sizeof(size_t);
            while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
            nrf_nvmc_write_bytes(aFlashMemoryPage + iWritePos, (uint8_t*)(searchVar->pValue), searchVar->cbValueLen);
            iWritePos += searchVar->cbValueLen;
        }

		searchVar = searchVar->pNext;
	}

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
    nrf_nvmc_write_byte(aFlashMemoryPage + iWritePos, 0);   // EOF
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
    nrf_nvmc_write_word(aFlashMemoryPage, auiID);           // Lastly write ID indicating the file is complete
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}

	return cbSize;
}


// Read variables from flash memory starting from address 'aFlashMemory' after successful verification that 
// the ID at the beginning of the flash memory page matches with auiID
// Returns the number of bytes read from flash (including overhead for ID and EOF marker) or 0 in case 
// of non matching ID
size_t hppReadVarFromFlash(uint32_t aFlashMemory, uint32_t auiID)
{
    char* pchReadAddress = (char*)aFlashMemory;

    if(aFlashMemory == 0) return 0;
    if(*((__hpp_packed uint32_t*)pchReadAddress) != auiID) return 0;

    pchReadAddress += sizeof(uint32_t);

    while(*pchReadAddress != 0)
    {
        size_t cbKeyLen = strlen(pchReadAddress);
        size_t cbVarLen = *((__hpp_packed size_t*)(pchReadAddress + cbKeyLen + 1));

        hppVarPut(pchReadAddress, pchReadAddress + cbKeyLen + 1 + sizeof(size_t), cbVarLen);
        pchReadAddress += cbKeyLen + 1 + sizeof(size_t) + cbVarLen;
    }

    return (uint32_t)pchReadAddress - aFlashMemory + 1;
}

#endif


// Read file ID for the flash address 'aFlashMemory'  
uint32_t hppReadFlashID(uint32_t aFlashMemory)
{
    char* pchReadAddress = (__hpp_packed char*)aFlashMemory;
    uint32_t iID = *((__hpp_packed uint32_t*)pchReadAddress);

    if(iID == 0xffffffff) iID = HPP_INVALID_FLASH_ID;   // not valid (empty or not completely written) 

    return iID;
}

// Delete 4k flash memory page starting from address 'aFlashMemoryPage'
void hppDeleteFlash(uint32_t aFlashMemoryPage)
{

    if(aFlashMemoryPage == 0) return;
    if((aFlashMemoryPage & 0xfff) != 0) return;   // Must start at the beginning of a page  

     while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
     nrf_nvmc_page_erase(aFlashMemoryPage); 
     while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {;}
}


// ------------------------------------------
// Bindings for NRF functions to H++ commands 
// ------------------------------------------

#ifdef __INCL_HPP_PARSER_

// Handler function for timers started with timer_start() or timer_once()

void hppTimerRequestHandler(void * apContext)
{
    const char* pchCode;
    if(apContext == NULL) return;   // unknown H++ function

    pchCode = hppVarGet((char*)apContext, NULL);
    
    if(pchCode != NULL)
    {
        hppParseExpression(pchCode, "ReturnWithoutError");
        hppVarDelete("ReturnWithoutError");   // result not used
    }
}

void hppButtonChangeHandler(uint8_t pin_no, uint8_t button_action)
{
    const char* pchCode = NULL;
    char szNumeric[HPP_NUMERIC_MAX_MEM];

    if(button_action == APP_BUTTON_PUSH) 
    {
        pchCode = hppVarGet("btn_push", NULL);
#ifdef __INCL_HPP_THREAD_
        hppButtoPressTime = otPlatAlarmMilliGetNow();
#endif
    }

    if(button_action == APP_BUTTON_RELEASE) pchCode = hppVarGet("btn_release", NULL);
    
    if(pchCode != NULL)
    {
        hppVarPutStr("0000:pin", hppI2A(szNumeric, pin_no), NULL);

#ifdef __INCL_HPP_THREAD_
        if(button_action == APP_BUTTON_RELEASE)
            hppVarPutStr("0000:hold_time", hppUI32toA(szNumeric, otPlatAlarmMilliGetNow() - hppButtoPressTime), NULL);
#endif

        hppParseExpression(pchCode, "ReturnWithoutError");
        hppVarDelete("ReturnWithoutError");   // result not used
    }
}

// --------------------------------------
// Handler for coap reladed H++ functions 
// --------------------------------------

// Evaluate Function 'aszFunctionName' with paramaters stored in variables with the name 'aszParamName' whereby
// variable names have the format '%04x_Param1'. The byte 'aszParamName[HPP_PARAM_PREFIX_LEN - 1]' is modifiyed to read the
// respectiv parameter. 
// The result is stored in the variable 'aszResultVarKey. Must return a pointer to the respective byte array of the result variable
// and change 'apcbResultLen_Out' to the respective number of bytes     
char* hppEvaluateNRF52840Function(char aszFunctionName[], char aszParamName[], const char aszResultVarKey[], size_t* apcbResultLen_Out)
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
        if(strcmp(aszFunctionName, "io_pin") == 0)   // port , pin
            return hppVarPutStr(aszResultVarKey, hppI2A(szNumeric, NRF_GPIO_PIN_MAP(atoi(pchParam1),atoi(pchParam2))), apcbResultLen_Out);

        if(strcmp(aszFunctionName, "io_set") == 0)    // pin, value (0, !=0)
        {
            if(atoi(pchParam2) == 0) nrf_gpio_pin_clear(atoi(pchParam1));
            else nrf_gpio_pin_set(atoi(pchParam1));
                
            return hppVarPutStr(aszResultVarKey, atoi(pchParam2) == 0 ? "0" : "1", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_get") == 0)    // pin
        {
            uint32_t pin = atoi(pchParam1);
            NRF_GPIO_Type* reg = nrf_gpio_pin_port_decode(&pin);
            pin = ((nrf_gpio_port_in_read(reg) >> pin) & 1UL);

            return hppVarPutStr(aszResultVarKey, hppI2A(szNumeric, pin), apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_cfg_output") == 0)    // pin [, high_driver (false, true)]
        {
            if(strcmp(pchParam2, "true") == 0) 
                nrf_gpio_cfg(atoi(pchParam1), NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL,
                             NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
            else if(*pchParam2 == 0 || strcmp(pchParam2, "false") == 0)
                nrf_gpio_cfg(atoi(pchParam1), NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, NRF_GPIO_PIN_NOPULL,
                             NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
            else return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);            
        }

        if(strcmp(aszFunctionName, "io_cfg_input") == 0)    // pin [, pull (up = 1, down = 0)]
        {
            if(*pchParam2 == 0) 
                nrf_gpio_cfg(atoi(pchParam1), NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_CONNECT, NRF_GPIO_PIN_NOPULL,
                             NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
            else if(atoi(pchParam2) == 0)
                nrf_gpio_cfg(atoi(pchParam1), NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_CONNECT, NRF_GPIO_PIN_PULLDOWN,
                             NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
            else if(atoi(pchParam2) == 1)
                nrf_gpio_cfg(atoi(pchParam1), NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_CONNECT, NRF_GPIO_PIN_PULLUP,
                             NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
            else return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
                
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_pwm_restart") == 0)   // no param 
        {
            if(NRF_PWM0->SEQ[0].REFRESH > 0) hppRestartPWM();
            return hppVarPutStr(aszResultVarKey, NRF_PWM0->SEQ[0].REFRESH > 0 ? "true" : "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_pwm_stop") == 0)   // no param 
        {
            hppStopPWM();
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
        }
                    
        if(strcmp(aszFunctionName, "io_pwm_start") == 0)   // count_stop, seq, val_repeat, seq_repeat, pin1 [, pin2, pin3, pin4]
        {
            size_t cSequenceLen;
            int iPin1, iPin2, iPin3, iPin4; 

            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            char* pchParam3 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '4';
            char* pchParam4 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '5';
            char* pchParam5 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '6';
            char* pchParam6 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '7';
            char* pchParam7 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '8';
            char* pchParam8 = hppVarGet(aszParamName, NULL);

            if(pchParam3 == NULL) pchParam3 = "1";
            if(pchParam4 == NULL) pchParam4 = "0";
            if(pchParam5 == NULL) iPin1 = -1; else iPin1 = atoi(pchParam5);
            if(pchParam6 == NULL) iPin2 = -1; else iPin2 = atoi(pchParam6);
            if(pchParam7 == NULL) iPin3 = -1; else iPin3 = atoi(pchParam7);
            if(pchParam8 == NULL) iPin4 = -1; else iPin4 = atoi(pchParam8);

            if(iPin3 < 0 && iPin4 < 0)    
            {
                if(iPin2 < 0) cSequenceLen = cbParam2 / 2;    // only one pin active --> 2 bytes per sequence
                else cSequenceLen = cbParam2 / 4;                       // two pins active --> 4 bytes per sequence
            }
            else cSequenceLen = cbParam2 / 8;   // three or four pins active --> 8 bytes per sequence

            if(cSequenceLen >= 2 || (atoi(pchParam4) == 0 && cSequenceLen >= 1))
            {
                hppStartPWM(atoi(pchParam1), iPin1, iPin2, iPin3, iPin4, (uint16_t*) pchParam2,
                            cSequenceLen, atoi(pchParam3), atoi(pchParam4));
                return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
            }

            return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "io_cfg_btn") == 0)    // pin [, activ_low/high(0/1), pull_up/down (1/0)] 
        {
            uint32_t error = NRF_ERROR_INVALID_PARAM;
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            char* pchParam3 = hppVarGet(aszParamName, NULL);
            if(*pchParam1 == 0) pchParam1 = "38";    // default pin is NRF_GPIO_PIN_MAP(1, 6); 
            if(pchParam3 == NULL) pchParam3 = "";
            if(*pchParam2 == 0 && *pchParam3 == 0) pchParam3 = "1";    // active low with pull up as default if param2 and 3 are missing 
   
            if(hppActiveButtonCount < HPP_BUTTON_RESOURCE_MAX)
            {
                hppButtonConfigTable[hppActiveButtonCount].pin_no = atoi(pchParam1);
                hppButtonConfigTable[hppActiveButtonCount].active_state = atoi(pchParam2) == 0 ? APP_BUTTON_ACTIVE_LOW : APP_BUTTON_ACTIVE_HIGH;
                //hppButtonConfigTable[hppActiveButtonCount].hi_accuracy = false;
                if(*pchParam3 != 0) hppButtonConfigTable[hppActiveButtonCount].pull_cfg = atoi(pchParam3) == 1 ? NRF_GPIO_PIN_PULLUP : NRF_GPIO_PIN_PULLDOWN;
                else hppButtonConfigTable[hppActiveButtonCount].pull_cfg = NRF_GPIO_PIN_NOPULL;
                hppButtonConfigTable[hppActiveButtonCount].button_handler = hppButtonChangeHandler; 
            
                hppActiveButtonCount++;

                error = app_button_init(hppButtonConfigTable, hppActiveButtonCount, APP_TIMER_TICKS(100));   // button push is reportet after 100ms    
                app_button_enable();
            }

            return hppVarPutStr(aszResultVarKey,  error == NRF_SUCCESS ? "true" : "false", apcbResultLen_Out);
        }
              
   }

    // flash_XXX commands
    if(strncmp(aszFunctionName, "flash_", 6) == 0)
    {
        if(strcmp(aszFunctionName, "flash_save") == 0 || strcmp(aszFunctionName, "flash_save_code") == 0)   
        {
            bool bGlobalVar = aszFunctionName[10] == 0 ? true : false;
            uint32_t uiAddr = 0xa0000;     // Flash address space for lower case variables (injected code): 0xa0000...0xb0000 (64k)
            size_t cbMaxLen = 0x10000;     // 64k
            uint32_t uiID = HPP_FLASH_CODE_ID;    // ID for valid code block 
            size_t cbLen;

            // Use two different 32k address ranges for upper case (global) variables, such we can go back to an older version in case of incomplete write    
            if(bGlobalVar) 
            {
                // find next higher ID
                uiID = hppReadFlashID(0xb0000);
                if(uiID != HPP_INVALID_FLASH_ID)
                { 
                    if(hppReadFlashID(0xb8000) > uiID && hppReadFlashID(0xb8000) != HPP_INVALID_FLASH_ID) uiID = hppReadFlashID(0xb8000);
                    uiID++; // go to next higher ID and alternate between the two memory spaces for gloabl variables
                }
                else uiID = 0;   // first time save

                uiAddr = 0xb0000 + (uiID & 1) * 0x8000; 
                cbMaxLen = 0x08000;   // 32k
            } 

            cbLen = hppWriteVar2Flash(uiAddr,  bGlobalVar, cbMaxLen, uiID);
         
            return hppVarPutStr(aszResultVarKey, hppUI32toA(szNumeric, cbLen), apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "flash_restore") == 0)    
        {
            size_t cbLen = 0;
            uint32_t uiID;
            uint32_t uiAddr;

            uiID = hppReadFlashID(0xb0000);
            if(uiID != HPP_INVALID_FLASH_ID)
            {
                if(hppReadFlashID(0xb8000) > uiID && hppReadFlashID(0xb8000) != HPP_INVALID_FLASH_ID) uiID = hppReadFlashID(0xb8000);
                uiAddr = 0xb0000 + (uiID & 1) * 0x8000;

                cbLen = hppReadVarFromFlash(uiAddr, uiID);
            }
                        
            return hppVarPutStr(aszResultVarKey, hppUI32toA(szNumeric, cbLen), apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "flash_delete") == 0)    
        {
            hppDeleteFlash(0xb0000);
            hppDeleteFlash(0xb8000);

            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "flash_delete_code") == 0)    
        {
            hppDeleteFlash(0xa0000);
          
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
        }


        // ----------------------------------------------------
        // More expert flash functions specifying the addresses 
        // ----------------------------------------------------
        /*
        if(strcmp(aszFunctionName, "flash_write") == 0 || strcmp(aszFunctionName, "flash_write_code") == 0)   // flash_address [, ID, max_len]  
        {
            size_t cbLen;
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            char* pchParam3 = hppVarGet(aszParamName, NULL);
            if(pchParam3 == NULL) pchParam3 = "102400";   // default max 100k
            cbLen = hppWriteVar2Flash(atol(pchParam1), aszFunctionName[11] == 0 ? true : false, atol(pchParam3), atol(pchParam2));
            
            return hppVarPutStr(aszResultVarKey, hppUI32toA(szNumeric, cbLen), apcbResultLen_Out);
        }
        
        
        if(strcmp(aszFunctionName, "flash_read") == 0)   // flash_address [, ID]  
        {
            size_t cbLen;
            cbLen = hppReadVarFromFlash(atol(pchParam1), atol(pchParam2));
                       
            return hppVarPutStr(aszResultVarKey, hppUI32toA(szNumeric, cbLen), apcbResultLen_Out);

        }

        if(strcmp(aszFunctionName, "flash_get_ID") == 0)   // flash_address  
        {
            uint32_t iID;
            iID =  hppReadFlashID(atol(pchParam1));

            return hppVarPutStr(aszResultVarKey, hppUI32toA(szNumeric, iID), apcbResultLen_Out);
        }
        */
    }

    // timer_XXX commands
    if(strncmp(aszFunctionName, "timer_", 6) == 0)
    {
        // Parmeters: timer_id, time (in ms), function name  --->  timer_id =  0..HPP_TIMER_RESOURCE_MAX - 1 (7)     
        if(strcmp(aszFunctionName, "timer_start") == 0 || strcmp(aszFunctionName, "timer_once") == 0)   
        {
            char* pchParam3;
            unsigned int uiTimer;
            uint32_t uiTime;
            uint32_t error = NRF_ERROR_INVALID_PARAM;

            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            pchParam3 = hppVarGet(aszParamName, NULL);
            if(pchParam3 == NULL) pchParam3 = "";
   
            uiTimer = atoi(pchParam1);
            uiTime = atol(pchParam2);
            if(uiTime < HPP_MIN_TIMER_TIME) uiTime = HPP_MIN_TIMER_TIME;  // minimum time for HPP interpreter  

            if(uiTimer <  HPP_TIMER_RESOURCE_MAX && *pchParam3 != 0) 
            {
                // The function can be called again on the timer instance and will re-initialize the instance if
                // the timer is not running
                error = app_timer_create(hppTimerResourceTable[uiTimer], 
                                         aszFunctionName[6] == 's' ? APP_TIMER_MODE_REPEATED : APP_TIMER_MODE_SINGLE_SHOT,
                                         hppTimerRequestHandler);
                
                // When calling this method on a timer that is already running, the second start operation is ignored
                if(error == NRF_SUCCESS)
                    error = app_timer_start(*hppTimerResourceTable[uiTimer], APP_TIMER_TICKS(uiTime), 
                                            (void*)hppVarGetKey(pchParam3, true));
            }

            return hppVarPutStr(aszResultVarKey, error == NRF_SUCCESS ? "true" : "false", apcbResultLen_Out);
        }

        // Parmeter: timer_id     
        if(strcmp(aszFunctionName, "timer_stop") == 0)   
        {
            unsigned int uiTimer;
            uint32_t error = NRF_ERROR_INVALID_PARAM;

            uiTimer = atoi(pchParam1);   
            if(uiTimer <  HPP_TIMER_RESOURCE_MAX) error = app_timer_stop(*hppTimerResourceTable[uiTimer]);
            
            return hppVarPutStr(aszResultVarKey, error == NRF_SUCCESS ? "true" : "false", apcbResultLen_Out);
        }
    }
    
    return NULL;
}		

#endif


// ------------ --------------
// Initialization on nRF 52840
// ---------------------------

void hppNRF52840Init()
{
	
#ifdef __INCL_HPP_PARSER_
    // Regiter NRF52840 related functions
    hppAddExternalFunctionLibrary(hppEvaluateNRF52840Function);
#endif
}