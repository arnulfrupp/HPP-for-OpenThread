/* ----------------------------------------------------------------------------	*/
/* Halloween++														            													*/
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.																		*/
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: mein.c																	*/
/* ----------------------------------------------------------------------------	*/
/* OpenThread Support Library - main file		       							*/
/*																				*/
/*   - Create a simple coap server controlling the LED	            			*/
/*   - No use of the H++ Scriting library										*/
/* ----------------------------------------------------------------------------	*/
/* Platform: OpenThread on Nordic nRF52840										*/
/* Dependencies: OpenThread, Nordic SDK											*/
/* Nordic SDK Version: nRF5_SDK_for_Thread_and_Zigbee_2.0.0_29775ac	        	*/
/* ----------------------------------------------------------------------------	*/
/* Copyright (c) 2018 - 2019, Arnulf Rupp										*/
/* arnulf.rupp@web.de															*/
/* All rights reserved.															*/
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Halloween Libraries 
#include "hppConfig.h"

// Variable Storage for general put and get operations on coap://var/<varname>
//#include "hppVarStorage.c"           

// Include Thread and nRF52840 support libraries   (remove  if included in a makefile)
#include "hppThread.c"
// Support library for GPIO & PWM operations
#include "hppNRF52840.c"


// ---------------------------
// Settings
// ---------------------------

#define DEVICE_ADDR "FD11:22::200"

// ---------------
// Timer Instances
// ---------------

APP_TIMER_DEF(m_Addr_registration_timer);


// -----------------------
// CoAP Callback Handler
// -----------------------

// Coap Handler for device identification with LED

static void CoapHandler_Led(void* apContext, otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    char chPayload = '0';

    if (hppIsCoapPUT(apMessage))
    {
        hppCoapReadPayload(apMessage, &chPayload, 1);

        if(chPayload == '0') nrf_gpio_pin_set(6);
        if(chPayload == '1') nrf_gpio_pin_clear(6);
     
        hppCoapConfirm(apMessage, apMessageInfo);
    }
}


// -----------------------
// Button Callback Handler
// -----------------------

void hppButtonChangeHandler(uint8_t pin_no, uint8_t button_action)
{
    if(button_action == APP_BUTTON_PUSH) 
    {
		otCliOutputFormat("Button Push: %d\r\n", (int)pin_no);
    }

    if(button_action == APP_BUTTON_RELEASE) 
	{
		otCliOutputFormat("Button Release: %d\r\n", (int)pin_no);
	}
}


// -------------
// Timer Handler
// -------------

static void TimerHandler_Addr_Registration(void * p_context)
{
	hppAddAddress(DEVICE_ADDR);
}


// --------------
// Initialization
// --------------

void InitTimer()
{
    app_timer_create(&m_Addr_registration_timer, APP_TIMER_MODE_REPEATED, TimerHandler_Addr_Registration);
}


void InitButton()
{
	// One Button wiht index 0 (< HPP_BUTTON_RESOURCE_MAX)
    hppButtonConfigTable[0].pin_no = NRF_GPIO_PIN_MAP(1, 6);
	hppButtonConfigTable[0].active_state = APP_BUTTON_ACTIVE_LOW;
	hppButtonConfigTable[0].pull_cfg = NRF_GPIO_PIN_PULLUP;
	hppButtonConfigTable[0].button_handler = hppButtonChangeHandler; 

	app_button_init(hppButtonConfigTable, 1, APP_TIMER_TICKS(100));   // button push is reportet after 100ms    
	app_button_enable();
}

// -------------
// Main
// -------------


int main(int argc, char * argv[])
{
	// Initialize Thread and neccessary other libraries
	hppThreadInit();
	hppNRF52840Init();

	// Initialize application timers and button
	InitTimer();
	InitButton();

    // Initialize GPIO
    nrf_gpio_cfg_output(6); 
    nrf_gpio_pin_set(6);

	// Initialize application
    hppCoapAddResource("led", "", CoapHandler_Led, NULL, false);

    // Start timer
    app_timer_start(m_Addr_registration_timer, APP_TIMER_TICKS(5000), NULL);

	// Start main loop
	hppMainLoop();
}
