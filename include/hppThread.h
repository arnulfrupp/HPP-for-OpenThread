/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppThread.h												            */
/* ----------------------------------------------------------------------------	*/
/* OpenThread Support Library										            */
/*																	            */
/*   - Initialization and resource managment						            */
/*   - CoAP support functions										            */
/*   - hppVarStorage bindings (PUT, GET var/x)                                  */
/*   - hppParser bindings (POST var/x)                                          */
/* ----------------------------------------------------------------------------	*/
/* Platform: OpenThread integrated in Zephire RTOS  				            */
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

#ifndef __INCL_HPP_THREAD_
#define __INCL_HPP_THREAD_


// ATTENTION - Thread safety:
// Functions in this library are usually called from an openthread callback, thus from openthread context
// Calling functions from outside requires locking the openthread mutex with:
//
// openthread_api_mutex_lock(hppOpenThreadContext);
// function call(s)
// openthread_api_mutex_unlock(hppOpenThreadContext);


#include <stdbool.h>
#include <ctype.h>

// Thread
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <openthread/coap.h>
#include <openthread/coap_secure.h>


// Settings
#define HPP_COAPS_PSK_MAX_LEN 32
#define HPP_COAPS_ID_MAX_LEN 32

#define HPP_HIDE_VAR_RESOURCE_PASSWORD "openvar"


// -----------------------
// Global Variables
// -----------------------

extern struct openthread_context *hppOpenThreadContext;
extern otInstance *hppOpenThreadInstance;               // hppOpenThreadContext->instance

// Hiding variables
extern bool hppHideVarResources;
      

//  CoAP message context for a delayed answer
struct hppCoapMessageContextStruct
{
    char szHeaderText[9];    // holds the text 'context:' to indicate a valid context
    otMessageInfo mMessageInfo;
    otCoapType mCoapType;
    otCoapCode mCoapCode;
    uint8_t mCoapMessageTokenLength;
    uint8_t mCoapMessageToken[OT_COAP_MAX_TOKEN_LENGTH];
    uint8_t mHasResponded;
};

typedef struct hppCoapMessageContextStruct hppCoapMessageContext;

// Current CoAP context for coap_respond() H++ command 
extern hppCoapMessageContext hppMyCurrentCoapMessageContext;


// -----------------------
// Send CoAP Messages
// -----------------------

// Send CoAP Message with CoAP code aCoapCode to URI aszUriPathAddress/aszUriPathOptions. Include Payload if acbPayloadLen > 0. 
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapSend(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* apPayload, size_t acbPayloadLen, otCoapCode aCoapCode, 
                    otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable);

// Send CoAP PUT Message to URI apUriPathAddress/apUriPathOptions. Include Payload if cbPayloadLen > 0. 
// Both Confirmable and Non Confirmabe is supported. 
otError hppCoapPUT(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* apPayload, size_t acbPayloadLen, bool abConfirmable);

// Send CoAP PUT Message to URI apUriPathAddress/apUriPathOptions. Include payload string apPayloadString if strlen(apPayloadString) > 0. 
// Both Confirmable and Non Confirmabe is supported. 
otError hppCoapStringPUT(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* aszPayloadString, bool abConfirmable);

// Send CoAP GET Message to URI apUriPathAddress/apUriPathOptions.  
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapGET(const char* aszUriPathAddress, const char* aszUriPathOptions, otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable);

// Send CoAP POST Message to URI apUriPathAddress/apUriPathOptions. Include Payload if cbPayloadLen > 0. 
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapPOST(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* apPayload, size_t acbPayloadLen, otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable);

// Send CoAP POST Message to URI apUriPathAddress/apUriPathOptions. Include payload string apPayloadString if strlen(apPayloadString) > 0. 
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapStringPOST(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* aszPayloadString, otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable);

// -----------------------
// Receive CoAP Messages
// -----------------------

// Check if incomming a CoAP message has the specified code 
bool hppIsCoapCode(otMessage* apMessage, otCoapCode aCoapCode);

// Check if a CoAP message is an inital type message (confirmable or non confirmable) and is a PUT/GET/POST/DELETE message respectively  
bool hppIsCoapPUT(otMessage* apMessage);
bool hppIsCoapGET(otMessage* apMessage);
bool hppIsCoapPOST(otMessage* apMessage);
bool hppIsCoapDELETE(otMessage* apMessage);

// Read CoAP Playload into a buffer with up to aLenght bytes. Returns the acutal number of bytes read
int hppCoapReadPayload(otMessage *apMessage, void *apBuf, uint16_t aLength);

// Get CoAP Playload lenght 
int hppCoapGetPayloadLenght(otMessage *apMessage);


// ----------------------------------
// CoAP Response and Confirm Messages
// ----------------------------------

// Return an acknowledge message with no payload if the original message was a confirmable message 
otError hppCoapConfirmWithCode(otMessage* apMessage, const otMessageInfo* apMessageInfo, otCoapCode aCoapCode);

// Return an acknowledge message with no playload with code 2.00 (OT_COAP_CODE_RESPONSE_MIN) if the original message was a confirmable message 
otError hppCoapConfirm(otMessage* apMessage, const otMessageInfo* apMessageInfo);

// Resond to a CoAP Message with MessageInfo from original message. Both confirmable and non comfirmable is supported. 
// The the response payload is piggybacked with the acknoledgment in case of confirmable requests.
// Adds a CoAP format option of 'aContentFormat' is different from the defaut format 'OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN' 
otError hppCoapRespond(otMessage* apMessage, const otMessageInfo* apMessageInfo, const char* apPayload, size_t cbPayloadLen, otCoapOptionContentFormat aContentFormat);

// Resond text string to a CoAP Message with MessageInfo from original message. Both confirmable and non comfirmable is supported.
// The the response payload is piggybacked with the acknoledgment in case of confirmable requests.
otError hppCoapRespondString(otMessage* apMessage, const otMessageInfo* apMessageInfo, const char* aszPayloadString);

// Resond text string to a CoAP Message with MessageInfo from original message. Both confirmable and non comfirmable is supported.
// The the response payload is piggybacked with the acknoledgment in case of confirmable requests.
// Adds a CoAP format option of 'aContentFormat' is different from the defaut format 'OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN' 
otError hppCoapRespondFormattedString(otMessage* apMessage, const otMessageInfo* apMessageInfo, const char* aszPayloadString, otCoapOptionContentFormat aContentFormat);

// Resond with CoAP code empty to a CoAP Message with given MessageInfo from original message if it was confirmable. 
// The empty response has no payload and and indicates to the client that a later response with the same token may come.
otError hppCoapRespondEmpty(otMessage* apMessage, const otMessageInfo* apMessageInfo);

// Store CoAP message context for a delayed answer
void hppCoapStoreMessageContext(hppCoapMessageContext* apCoapMessageContext, otMessage* apMessage, const otMessageInfo* apMessageInfo);

// Respond to a CoAP Message with given hppCoapMessageContext form original message. Both confirmable and non comfirmable is supported. 
// Adds a CoAP format option of 'aContentFormat' is different from the defaut format 'OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN'
// If apPayload is NULL, a response without palyoad and  CoAP code OT_COAP_CODE_NOT_FOUND is sent.   
otError hppCoapRespondTo(const hppCoapMessageContext* apCoapMessageContext, const char* apPayload, size_t cbPayloadLen, otCoapOptionContentFormat aContentFormat);


// -----------------
// Read Coap Options
// -----------------

// Search for the first occurrence of a a specified coap option and return the value in apOptionBuffer
// Returns the length of the content in bytes or 0 if the option as not found or the buffer was to small.
const uint16_t hppGetCoapOption(otMessage* apMessage, otCoapOptionType aCoapOption, uint8_t* apOptionBuffer, uint16_t cbMaxLen);

// Read content format from coap message
otCoapOptionContentFormat hppGetCoapContentFormat(otMessage* apMessage);

// Verify if a certain content format is accepted for an answer
// TODO: Check if multiple accepted formats really come as different coap options of if they e.g. come as
// an array of int16_t
bool hppIsAcceptedCoapContentFormat(otMessage* apMessage, otCoapOptionContentFormat aContentFormat);

// Get CoAP Path option. Returns a heap allocated pointer which needs to be freed in the calling program. 
char* hppGetCoapPath(otMessage* apMessage);

// Invalidate current coap meassge context after H++ script execution triggered by a CoAP message              
void hppInvalidateHppCallContext();

// -------------------
// Secure CoAP (CoAPs)
// -------------------

// Default handler for connection events
void hppCoapsConnectedDefaultHandler(bool aConnected, void *aContext);

// Set PSK for Coaps connections and start Coaps module 
// Sets 'aConnectHandler' as a handler and passes 'aContext' to the handler in case of a connect or disconnect event.
// If 'aConnectHandler' is NULL a default handler (hppCoapsConnectedDefaultHandler) is ussed. 
// Return true if the process was successfully started, or false if something went wrong. 
// Currently only one CoAPs security context is supported.
otError hppCreateCoapsContextWithPSK(const char* szIdentityName, const char* szPassword, otHandleCoapSecureClientConnect aConnectHandler, void* aContext);

// Set Certificates for Coaps connections and start Coaps module  
// Sets 'aConnectHandler' as a handler and passes 'aContext' to the handler in case of a connect or disconnect event.
// If 'aConnectHandler' is NULL a default handler (hppCoapsConnectedDefaultHandler) is ussed. 
// Return true if the process was successfully started, or false if something went wrong.
// Currently only one CoAPs security context is supported.
otError hppCreateCoapsContextWithCert(const char* szX509Cert, const char* szPrivateKey, const char* szRootCert, otHandleCoapSecureClientConnect aConnectHandler, void* aContext);

// Start DTLS Connection with a host with the address 'aszUriPathAddress'
// Sets 'aConnectHandler' as a handler and passes 'aContext' to the handler in case of a connect or disconnect event.
// If 'aConnectHandler' is NULL a default handler (hppCoapsConnectedDefaultHandler) is ussed. 
// Return true if the process was successfully started, or false if something went wrong. 
otError hppCoapsConnect(const char* aszUriPathAddress, otHandleCoapSecureClientConnect aConnectHandler, void* aContext);

// Disconnect open DTLS connection
void hppCoapsDisconnect();

// Stop Coaps session
void hppDeleteCoapsContext();


// -----------------------
// Manage CoAP Resources
// -----------------------

// Add CoAP resource for a specified resource name and assign a handler
// Adds the resource and 'szResourceInformation' to the '.wellknown/core' resource string if szResourceInformation != NULL
// Only adds the resource name if 'szResourceInformation' is an empty string 
// The parameter 'abSecure' determines if a CoAPs resource (true) or regular CoAP (false) resource is added
bool hppCoapAddResource(const char* apResourceName, const char* szResourceInformation, otCoapRequestHandler apHandler, void* apContext, bool abSecure);

// Return string with all CoAP resources in CoAP .wellknown/core format or NULL in case of an error.
// The calling function must free the memory allocated by this function unless NULL was returned. 
char* hppNew_WellknownCore();


// -------------------
// Manage IP Addresses
// -------------------

// Add unicast or multicast address to the interface
bool hppAddAddress(const char* aAddress);

// Convert IPv6 address in a string and write it in aszText_out.
// aszText_out mut be 40 bytes long to hold the address string including terminating null byte.
// Returns aszText_out.
// Does NOT check if aszText_out is NULL!
// This does not require locking the thread mutex
char* hppGetAddrString(const otIp6Address *apIp6Address, char* aszText_out);


// ---------
// CLI Tools
// ---------

// Print string to CLI console with LF to CRLF translation. Adds  another CRLF if abNewLine is true. 
void hppCliOutputStr(const char* aszText, bool abNewLine);


// ----------------------------------
// Thread Initialization on nRF 52840
// ----------------------------------

void hppThreadInit();


// ---------------------------------
// Main Loop for Thread on nRF 52840
// ---------------------------------

void hppMainLoop();

#endif
