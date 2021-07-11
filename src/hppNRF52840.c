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
/*   - PWM library              	       							           	*/
/* ----------------------------------------------------------------------------	*/
/* Platform: Zephyr RTOS with Nordic nRF52840						            */
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

#include "../include/hppNRF52840.h"

#include <zephyr.h>

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
// If uiLoop is >0 cSequenceLen must be minimum 2.

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

