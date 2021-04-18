/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: main.c														            */
/* ----------------------------------------------------------------------------	*/
/* OpenThread Support Library - main file							            */
/*																	            */
/*   - Initialization Halloween library 							            */
/*   - Load demo scripts 														*/
/*   - Runs scripts in in flash memory (if present)					            */
/*   - Runs demo scripts if no other scripts are stored in flash memory 		*/
/* ----------------------------------------------------------------------------	*/
/* Platform: Zephire RTOS with OpenThread			  				            */
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

//#include <stdio.h>
//#include <stdbool.h>
//#include <string.h>

// Halloween Libraries 

// Include H++ scripting libraries
#include "../include/hppVarStorage.h"
#include "../include/hppParser.h"

// Include Thread and Zephyr support libraries
#include "../include/hppThread.h"
#include "../include/hppZephyr.h"


// --------------
// Initialization
// --------------

void LoadDemoScripts()
{
	// initialize LED peripherals, green led and RGB led (RGB LED not used in demo scripts)
	hppVarPutStr("init_led", 	"LED_state = 0;\n"
								"//Led1 = 6; Led2r = 8; Led2g = io_pin(1, 9); Led2b = 12;\n\n"
								"//io_set(6, 0);\n"
								"//io_set(Led2r, 1);\n"
								"//io_set(Led2g, 1);\n"
								"//io_set(Led2b, 1);\n\n"
								"//io_cfg_output(6);\n"
								"//io_cfg_output(Led2r);\n"
								"//io_cfg_output(Led2g);\n"
								"//io_cfg_output(Led2b);\n"
								"io_cfg_output(13);\n"
								"io_set(13, 0);\n",
								NULL);

	// Deactivate blinking and switch LED off
	hppVarPutStr("led_off", 	"io_pwm_stop();\n"
								"timer_stop(0);\n"
								"LED_state = 1;\n"
								"io_set(6, 1);", NULL);

	// Toggle LED. This function is also used as a hander for the timer and for the coap 'toggle' ressource
	hppVarPutStr("switch_led", 	"LED_state = 1 - ?LED_state;\n"
	                           	"io_set(6, LED_state);\n", 
				 				NULL);

	// PWM peripheral example
	hppVarPutStr("do_pwm", 		"timer_stop(0);\n"
								"sqr = 'return param1 * param1;';\n"
								"PWM::alloc(252 * 2);\n"
	                            "while(?i < 126) PWM:uint16[i] = sqr(i++);\n"
								"while(i <= 252) PWM:uint16[i] = sqr(252 - i++));\n"
	                           	"io_pwm_start(16000, PWM, 20, 1000, 6);\n"
								"FirstToggle = true;\n", 
				 				NULL);

	// Demostration for how to save and delete code and global variables to flash memory 
	hppVarPutStr("save_all", 	"flash_save_code();\n"
								"flash_save();\n",
				 				NULL);
	hppVarPutStr("delete_all", 	"flash_delete_code();\n"
								"flash_delete();\n",
				 				NULL);

	// Parse CLI command for 'mycmd' command 
	hppVarPutStr("cli_parse", 	"cli_writeln('receive user command: ' ~ command ~ ' with ' ~ argc ~ ' argument(s)');\n" , NULL);

	// Handlers for button push
	hppVarPutStr("btn_push", 	"coap_put('ff03::fc', 'toggle', '', false);\n"   // addr (=> mulitcast to all CoAP nodes), path, payload, confirm (false => multicast shall not be confirmed) 
								"toggle_handler();", 							 // also switch own LED
								NULL);   

	hppVarPutStr("btn_release", "//cli_writeln('button ' ~ pin ~ ' released after ' ~ hold_time ~ ' ms');\n" , NULL);

	// CoAP handler responding to a GET and switching in PWM on a post
	hppVarPutStr("hello_handler",	"if(coap_is_get()) coap_respond('Hello H++ world');\n"
									"if(coap_is_post()) do_pwm();\n",
				 					NULL);

	// CoAP PUT handler switch off PWM and timer on first call and toggling the LED on next call
	hppVarPutStr("toggle_handler",	"if(FirstToggle)\n"
									"{\n"
									"	FirstToggle = false;\n"
									"	led_off();\n"
									"}\n\n"
									"switch_led();\n", 
									NULL);
									

	// Address registration must be done after the nework is up (prefix received from the boader router)
	hppVarPutStr("registration_handler", "ip_add_addr('fd11:22::100');\n", NULL);	 	// STABLE IP ADDRESS -> CHANGE IID (1xx) TO GIVE ALL DEVICES A UNIQUE IPv6 ADDRESS							 

	/* 
	// Initialization script
	hppVarPutStr("startup", 	"init_led();\n"
								"timer_start(0, 1000, 'switch_led');\n" 				// timer 0 is used for blinking LED
								"cli_add_cmd('mycmd');\n"
								"io_cfg_btn();\n"  										// default is pin 38 (USB dongle top button: port 1 pin 6 ==> 38)   
								"timer_once(1, 3000, 'registration_handler');\n"  	   	// register stable IP address device receifed network data  (after 3 sec) using timer 1
								"coap_add_resource('stop_blinking', 'rt=xyz', 'led_off');\n"
								"FirstToggle = true;\n"
								"coap_add_resource('toggle', '', 'toggle_handler');\n"
								"coap_add_resource('hello', '', 'hello_handler');\n"
								"//Var_Hide_PW = 'password';\n"
								"//coap_hide_vars(true);\n", NULL);
	*/


	// CLI output handler
	hppVarPutStr("cli_out",     "writeln('received:' ~ cli_output);\n", NULL);

	// CLI output handler
	hppVarPutStr("cli_send2",     "cli_put('state', 'cli_out');\n", NULL);

	// CLI output handler
	hppVarPutStr("cli_send",     "cli_put('state');\n", NULL);

	// Initialization script
	hppVarPutStr("startup", 	"init_led();\n"
								"a = 1;\n"
								"b = 2;\n"
								"writeln('Hello World');\n"
								"writeln(a + b);\n"
								"coap_add_resource('hello', '', 'hello_handler');\n"
								"a = ip_get_eid();\n"
								"writeln('eid:' ~ a);\n", NULL);
}

// -------------
// Main
// -------------

void main(void)
{
	// Initialize Thread and neccessary other libraries
	hppThreadInit();
	hppZephyrInit();
	//hppNRF52840Init();

  	//Read code from flash memory if ID matches with predefined ID
    //if(hppReadVarFromFlash(0xa0000, HPP_FLASH_CODE_ID) == 0) 
	LoadDemoScripts();    // Load demos script if no code was in the flash memory

	hppAsyncParseVar("startup");
	
	// Start main loop (returns immediatelly in case of Zephire)
	hppMainLoop();
}
