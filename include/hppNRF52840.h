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


#ifndef __INCL_HPP_NRF52840_
#define __INCL_HPP_NRF52840_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


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

#endif