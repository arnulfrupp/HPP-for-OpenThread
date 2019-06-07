/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppNRF52840.h												            */
/* ----------------------------------------------------------------------------	*/
/* OpenThread Support Library										            */
/*																	            */
/*   - Time handling                           						            */
/*   - GPIO & PWM handling 			       							           	*/
/*   - CoAP support functions										            */
/*   - Flash memory hndling 										            */
/*   - H++ bindings for Timer, GPIO and PWM                                     */
/* ----------------------------------------------------------------------------	*/
/* Platform: OpenThread on Nordic nRF52840							            */
/* Dependencies: Nordic SDK; If activated: hppVarStorage, hppParser             */
/* Tested with SDK Version: nRF5_SDK_for_Thread_and_Zigbee_v3.0.0_d310e71	    */
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


#ifndef __INCL_HPP_NRF52840_
#define __INCL_HPP_NRF52840_

#include <stdbool.h>
#include <stdint.h>

// Platform SDK
#include "app_scheduler.h"
#include "app_timer.h"
#include "app_button.h"

// SDK Settings
#include "sdk_config.h"

// GPIO
#include "nrf_gpio.h"
#include "nrf_nvmc.h"

#define HPP_INVALID_FLASH_ID 0xfffffffe
#define HPP_FLASH_CODE_ID 0x8732a3fb

// ----------------
// Global Variables
// ----------------

#define HPP_TIMER_RESOURCE_MAX 8
#define HPP_BUTTON_RESOURCE_MAX 8

app_button_cfg_t hppButtonConfigTable[HPP_BUTTON_RESOURCE_MAX];
size_t hppActiveButtonCount = 0;


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
// The values are not buffered. Therefore puiSeqence must remain in RAM wihile PWM is active.
// Duty cycle values range from 0 to uiCountStop (100%). 
// Every PWM setting is repeated 'uiStepRepeat' times until the next values are taken. If 'uiStepRepeat' is null then
// the PMW values are static. The Sequence is repeated uiLoop times (uiLoop = 0 --> no Loop)
// If uiLoop is >0 cSequenceLen must be minimum 8 (two value groups).
void hppStartPWM(unsigned int uiCountStop, int iPin1, int iPin2, int iPin3, int iPin4, uint16_t* puiSeqence, size_t cSequenceLen, unsigned int uiStepRepeat, unsigned int uiLoop);

void hppRestartPWM();

void hppStopPWM();

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
size_t hppWriteVar2Flash(uint32_t aFlashMemoryPage, bool abGlobal, size_t acbMaxLen, uint32_t auiID);

// Read variables from flash memory starting from address 'aFlashMemory' after successful verification that 
// the ID at the beginning of the flash memory page matches with auiID
// Returns the number of bytes read from flash (including overhead for ID and EOF marker) or 0 in case 
// of non matching ID
size_t hppReadVarFromFlash(uint32_t aFlashMemory, uint32_t auiID);

#endif


// Read file ID for the flash address 'aFlashMemory'  
uint32_t hppReadFlashID(uint32_t aFlashMemory);

// Delete 4k flash memory page starting from address 'aFlashMemoryPage'  
void hppDeleteFlash(uint32_t aFlashMemoryPage);


// ------------------------------------------
// Bindings for NRF functions to H++ commands 
// ------------------------------------------

#ifdef __INCL_HPP_PARSER_

// Handler function for timers started with timer_start() or timer_once()

void hppTimerRequestHandler(void * apContext);

void hppButtonChangeHandler(uint8_t pin_no, uint8_t button_action);

// --------------------------------------
// Handler for coap reladed H++ functions 
// --------------------------------------

// Evaluate Function 'aszFunctionName' with paramaters stored in variables with the name 'aszParamName' whereby
// variable names have the format '%04x_Param1'. The byte 'aszParamName[HPP_PARAM_PREFIX_LEN - 1]' is modifiyed to read the
// respectiv parameter. 
// The result is stored in the variable 'aszResultVarKey. Must return a pointer to the respective byte array of the result variable
// and change 'apcbResultLen_Out' to the respective number of bytes     
char* hppEvaluateNRF52840Function(char aszFunctionName[], char aszParamName[], const char aszResultVarKey[], size_t* apcbResultLen_Out);

#endif


// ------------ --------------
// Initialization on nRF 52840
// ---------------------------

void hppNRF52840Init();

#endif