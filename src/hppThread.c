/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hppThread.c												            */
/* ----------------------------------------------------------------------------	*/
/* OpenThread Support Library										            */
/*																	            */
/*   - Initialization and resource managment						            */
/*   - Thread Main loop  			       							           	*/
/*   - CoAP support functions										            */
/*   - hppVarStorage bindings (PUT, GET var/x) if compiled with __INCL_HPP_VAR_ */
/*   - hppParser bindings (POST var/x) if compiled with __INCL_HPP_PARSER_      */
/* ----------------------------------------------------------------------------	*/
/* Platform: OpenThread on Nordic nRF52840							            */
/* Dependencies: OpenThread, Nordic SDK; If activated: hppVarStorage, hppParser */
/* Nordic SDK Version: nRF5_SDK_for_Thread_and_Zigbee_v3.0.0_d310e71	        */
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

#include "../include/hppThread.h"
#include "../hppConfig.h"


// Platform SDK
#include "app_scheduler.h"
#include "app_timer.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// Thread
#include "thread_utils.h"
#include <openthread/platform/alarm-milli.h>
#include <openthread/cli.h>

// SDK Settings
#include "sdk_config.h"


// ---------------------------
// Thread Settings
// ---------------------------

#define SCHED_QUEUE_SIZE      32                                  // Maximum number of events in the scheduler queue. 
#define SCHED_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE     // Maximum app_scheduler event size. 


// ---------------------------
// Byte alignment
// ---------------------------

#ifdef __arm__
#define __hpp_packed __packed            
#else
#define __hpp_packed
#warning no arm environment
#endif


// ---------------------------
// H++ Thread Library Settings
// ---------------------------

#define HPP_MAX_PAYLOAD_LENGTH 1280       // Maximum Payload size for assignment to hppVar resources. 1280 is the maximum expected for regular (non-blockwise) coap

#define HPP_HIDE_VAR_RESOURCE_PASSWORD "openvar"

#define HPP_MAX_CLI_HPP_CMD_LEN 80
#define HPP_MAX_CLI_COUNT 10


// -----------------------
// Global Variables
// -----------------------

char* hppWellknownCoreStr = NULL;
bool hppIsCoapsInitialized = false;
bool hppIsCoapsShutdownStarted = false;

char hppCoapsPSK[HPP_COAPS_PSK_MAX_LEN + 1];     // one Byte more for zero temrinated String
char hppCoapsID[HPP_COAPS_ID_MAX_LEN + 1];

char* hppCoapsCert = NULL;
char* hppCoapsCertKey = NULL;
char* hppCoapsCaCert = NULL;

#ifdef __INCL_HPP_PARSER_

static uint32_t hppThreadTimeoutTime = 1000;   // stop H++ interpreter after 1000 ms by default -> 0 means no timeout

// The maximum time consumed by a H++ code execution call should always stay well below hppThreadTimeoutTime to avoid random timeout errors
static uint32_t hppThreadMaxTimeMeasured = 0;  // maximum time span measured for any H++ script being executed

// Current CoAP Mesage and MessageInfo used in CoAP related H++ commands 
struct hppCurrentCoapContextStruct 
{
    otMessage* pCurrentCoapMessage;
    const otMessageInfo* pCurrentCoapMessageInfo;
    bool bCurrentCoapResponseDone;   // Keeps track if there was an answer provide on the current message already 
};

typedef struct hppCurrentCoapContextStruct hppCurrentCoapContext;
static hppCurrentCoapContext hppMyCurrentCoapContext;

// User define CLI Commands 
static otCliCommand* hppCliCommandTable = NULL;
static size_t hppCliCommandTableSize = 0; 

#endif

#ifdef __INCL_HPP_VAR_
static bool hppHideVarResources = false;
#endif

// -----------------------
// Send CoAP Messages
// -----------------------

// Send CoAP Message with CoAP code aCoapCode to URI aszUriPathAddress/aszUriPathOptions. Include Payload if acbPayloadLen > 0. 
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapSend(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* apPayload, size_t acbPayloadLen, otCoapCode aCoapCode, 
                    otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable)
{
    otError error = OT_ERROR_NONE;
    otMessage* pMessage;
    otMessageInfo myMessageInfo;
    bool bSecure = false;

    pMessage = otCoapNewMessage(thread_ot_instance_get(), NULL);
    if (pMessage == NULL) return OT_ERROR_NO_BUFS;

    if(abConfirmable) otCoapMessageInit(pMessage, OT_COAP_TYPE_CONFIRMABLE, aCoapCode);
    else otCoapMessageInit(pMessage, OT_COAP_TYPE_NON_CONFIRMABLE, aCoapCode);

    otCoapMessageGenerateToken(pMessage, 2);                                //TODO: Always include token, or only if a Response hander is provided 
    otCoapMessageAppendUriPathOptions(pMessage, aszUriPathOptions);
    if(acbPayloadLen > 0) otCoapMessageSetPayloadMarker(pMessage);

	if(acbPayloadLen > 0) error = otMessageAppend(pMessage, apPayload, acbPayloadLen);

    if (error == OT_ERROR_NONE)
	{
		memset(&myMessageInfo, 0, sizeof(myMessageInfo));     // myMessageInfo not relevant in case of secure connection
		myMessageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;
		myMessageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;    
        if(aszUriPathAddress != NULL)
        {
            if(strcmp(aszUriPathAddress, "DTLS") == 0) bSecure = true;    // use existing secure connection
            else otIp6AddressFromString(aszUriPathAddress, &myMessageInfo.mPeerAddr);    
        }

        if(bSecure == true) error = otCoapSecureSendRequest(thread_ot_instance_get(), pMessage, aResponseHandler, aContext); 
		else error = otCoapSendRequest(thread_ot_instance_get(), pMessage, &myMessageInfo, aResponseHandler, aContext);
	}

    if (error != OT_ERROR_NONE)
    {
        //NRF_LOG_INFO("Failed to send CoAP Request: %d\r\n", error);
        otMessageFree(pMessage);
    }

    return error;
}


// Send CoAP PUT Message to URI apUriPathAddress/apUriPathOptions. Include Payload if cbPayloadLen > 0. 
// Both Confirmable and Non Confirmabe is supported. 
otError hppCoapPUT(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* apPayload, size_t acbPayloadLen, bool abConfirmable)
{
    return hppCoapSend(aszUriPathAddress, aszUriPathOptions, apPayload, acbPayloadLen, OT_COAP_CODE_PUT, NULL, NULL, abConfirmable);
}

// Send CoAP PUT Message to URI apUriPathAddress/apUriPathOptions. Include payload string apPayloadString if strlen(apPayloadString) > 0. 
// Both Confirmable and Non Confirmabe is supported. 
otError hppCoapStringPUT(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* aszPayloadString, bool abConfirmable)
{
    return hppCoapSend(aszUriPathAddress, aszUriPathOptions, aszPayloadString, strlen(aszPayloadString), OT_COAP_CODE_PUT, NULL, NULL, abConfirmable);
}

// Send CoAP GET Message to URI apUriPathAddress/apUriPathOptions.  
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapGET(const char* aszUriPathAddress, const char* aszUriPathOptions, otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable)
{
    return hppCoapSend(aszUriPathAddress, aszUriPathOptions, NULL, 0, OT_COAP_CODE_GET, aResponseHandler, aContext, abConfirmable);
}

// Send CoAP POST Message to URI apUriPathAddress/apUriPathOptions. Include Payload if cbPayloadLen > 0. 
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapPOST(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* apPayload, size_t acbPayloadLen, otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable)
{
    return hppCoapSend(aszUriPathAddress, aszUriPathOptions, apPayload, acbPayloadLen, OT_COAP_CODE_POST, aResponseHandler, aContext, abConfirmable);
}

// Send CoAP POST Message to URI apUriPathAddress/apUriPathOptions. Include payload string apPayloadString if strlen(apPayloadString) > 0. 
// Any resonse will be send to aResponseHandler if other than NULL. Both Confirmable and Non Confirmabe is supported. 
// The pointer aContext will be send to the aResponseHandler e.g. for request-response mapping.
otError hppCoapStringPOST(const char* aszUriPathAddress, const char* aszUriPathOptions, const char* aszPayloadString, otCoapResponseHandler aResponseHandler, void* aContext, bool abConfirmable)
{
    return hppCoapSend(aszUriPathAddress, aszUriPathOptions, aszPayloadString, strlen(aszPayloadString), OT_COAP_CODE_POST, aResponseHandler, aContext, abConfirmable);
}


// -----------------------
// Receive CoAP Messages
// -----------------------

// Check if incomming a CoAP message has the specified code 
bool hppIsCoapCode(otMessage* apMessage, otCoapCode aCoapCode)
{
    if(apMessage == NULL) return false;
	if(otCoapMessageGetCode(apMessage) != aCoapCode) return false;
    return true;
}

// Check if a CoAP message is an inital type message (confirmable or non confirmable) and is a PUT/GET/POST/DELETE message respectively  
bool hppIsCoapPUT(otMessage* apMessage) { return hppIsCoapCode(apMessage, OT_COAP_CODE_PUT); }
bool hppIsCoapGET(otMessage* apMessage) { return hppIsCoapCode(apMessage, OT_COAP_CODE_GET); }
bool hppIsCoapPOST(otMessage* apMessage) { return hppIsCoapCode(apMessage, OT_COAP_CODE_POST); }
bool hppIsCoapDELETE(otMessage* apMessage) { return hppIsCoapCode(apMessage, OT_COAP_CODE_DELETE); }


// Read CoAP Playload into a buffer with up to aLenght bytes. Returns the acutal number of bytes read
int hppCoapReadPayload(otMessage *apMessage, void *apBuf, uint16_t aLength)
{
    return otMessageRead(apMessage, otMessageGetOffset(apMessage), apBuf, aLength);
}

// Get CoAP Playload lenght 
int hppCoapGetPayloadLenght(otMessage *apMessage)
{
    return otMessageGetLength(apMessage) - otMessageGetOffset(apMessage);
}


// ----------------------------------
// CoAP Response and Confirm Messages
// ----------------------------------

// Return an empty acknowledge message if the original message was a confirmable message 
otError hppCoapConfirmWithCode(otMessage* apMessage, const otMessageInfo* apMessageInfo, otCoapCode aCoapCode)
{
    otError error = OT_ERROR_NO_BUFS;
    otMessage* pMyMessage;

    // Do not sent acknoledgment if the requesting messege in non confirmable  
    if(otCoapMessageGetType(apMessage) != OT_COAP_TYPE_CONFIRMABLE) return OT_ERROR_NONE;

    pMyMessage = otCoapNewMessage(thread_ot_instance_get(), NULL);
    if (pMyMessage == NULL) return error;

    otCoapMessageInit(pMyMessage, OT_COAP_TYPE_ACKNOWLEDGMENT, aCoapCode);
    otCoapMessageSetMessageId(pMyMessage, otCoapMessageGetMessageId(apMessage));
    
	if(otCoapMessageGetToken(apMessage) != NULL) 
		otCoapMessageSetToken(pMyMessage, otCoapMessageGetToken(apMessage), otCoapMessageGetTokenLength(apMessage));
   
    if(apMessageInfo->mSockPort == OT_DEFAULT_COAP_SECURE_PORT) error = otCoapSecureSendResponse(thread_ot_instance_get(), pMyMessage, apMessageInfo);
    else error = otCoapSendResponse(thread_ot_instance_get(), pMyMessage, apMessageInfo);

    if(error != OT_ERROR_NONE) otMessageFree(pMyMessage);

    return error;
}

// Return an empty acknowledge message with code 2.00 (OT_COAP_CODE_RESPONSE_MIN) if the original message was a confirmable message 
otError hppCoapConfirm(otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    return hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_VALID);
}
 

// Resond to a CoAP Message with given MessageInfo from original message. Both confirmable and non comfirmable is supported. 
// The the response payload is piggybacked with the acknoledgment in case of confirmable requests.
// Adds a CoAP format option of 'aContentFormat' is different from the defaut format 'OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN' 
otError hppCoapRespond(otMessage* apMessage, const otMessageInfo* apMessageInfo, const char* apPayload, size_t cbPayloadLen, otCoapOptionContentFormat aContentFormat)
{
    otError error;
    otMessage* pMyMessage;
	otMessageInfo myMessageInfo = *apMessageInfo;

    if(apPayload == NULL) return OT_ERROR_NONE;

    pMyMessage = otCoapNewMessage(thread_ot_instance_get(), NULL);
    if(pMyMessage == NULL) return OT_ERROR_NO_BUFS;

	if(otCoapMessageGetType(apMessage) == OT_COAP_TYPE_CONFIRMABLE) 
	{
		otCoapMessageInit(pMyMessage, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CONTENT);
		otCoapMessageSetMessageId(pMyMessage, otCoapMessageGetMessageId(apMessage));
	}
	else otCoapMessageInit(pMyMessage, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_CONTENT);
	
	if(otCoapMessageGetToken(apMessage) != NULL) 
		otCoapMessageSetToken(pMyMessage, otCoapMessageGetToken(apMessage), otCoapMessageGetTokenLength(apMessage));

	if(aContentFormat != OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN)    // default format
	{
		otCoapMessageAppendContentFormatOption(pMyMessage, aContentFormat);

		//uint8_t myContenFormat = aContentFormat;
		//otCoapOption  myContentFormatOption;
		//myContentFormatOption.mNumber = OT_COAP_OPTION_CONTENT_FORMAT;
		//myContentFormatOption.mLength = 1;
		//myContentFormatOption.mValue = &myContenFormat;
		//otCoapMessageAppendOption(pMyMessage, &myContentFormatOption);
	}

	otCoapMessageSetPayloadMarker(pMyMessage);
	memset(&myMessageInfo.mSockAddr, 0, sizeof(myMessageInfo.mSockAddr));    // from nordic demo (not sure why this is needed)

    error = otMessageAppend(pMyMessage, apPayload, cbPayloadLen);
    if(error == OT_ERROR_NONE) 
	{
        if(myMessageInfo.mSockPort == OT_DEFAULT_COAP_SECURE_PORT) error = otCoapSecureSendResponse(thread_ot_instance_get(), pMyMessage, &myMessageInfo);
        else error = otCoapSendResponse(thread_ot_instance_get(), pMyMessage, &myMessageInfo);
	}

    if(error != OT_ERROR_NONE) otMessageFree(pMyMessage);

    return error;
}

// Resond text string to a CoAP Message with given MessageInfo from original message. Both confirmable and non comfirmable is supported.
// The the response payload is piggybacked with the acknoledgment in case of confirmable requests.
otError hppCoapRespondString(otMessage* apMessage, const otMessageInfo* apMessageInfo, const char* aszPayloadString)
{
    return hppCoapRespond(apMessage, apMessageInfo, aszPayloadString, strlen(aszPayloadString), OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN);
}

// Resond text string to a CoAP Message with given MessageInfo from original message. Both confirmable and non comfirmable is supported.
// The the response payload is piggybacked with the acknoledgment in case of confirmable requests.
// Adds a CoAP format option of 'aContentFormat' is different from the defaut format 'OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN' 
otError hppCoapRespondFormattedString(otMessage* apMessage, const otMessageInfo* apMessageInfo, const char* aszPayloadString, otCoapOptionContentFormat aContentFormat)
{
    return hppCoapRespond(apMessage, apMessageInfo, aszPayloadString, strlen(aszPayloadString), aContentFormat);
}

// Store CoAP message context for a delayed answer
void hppCoapStoreMessageContext(hppCoapMessageContext* apCoapMessageContext, otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    apCoapMessageContext->mMessageInfo = *apMessageInfo;
    apCoapMessageContext->mCoapType = otCoapMessageGetType(apMessage); 
    apCoapMessageContext->mCoapMessageId =  otCoapMessageGetMessageId(apMessage);  // uint16_t
    strcpy(apCoapMessageContext->szHeaderText, "context:");

    if(otCoapMessageGetToken(apMessage) != NULL)
    {
        apCoapMessageContext->mCoapMessageTokenLength = otCoapMessageGetTokenLength(apMessage);
        memcpy(apCoapMessageContext->mCoapMessageToken, otCoapMessageGetToken(apMessage), apCoapMessageContext->mCoapMessageTokenLength);
    }
    else apCoapMessageContext->mCoapMessageTokenLength = 0; 
}

// Resond to a CoAP Message with given hppCoapMessageContext original message. Both confirmable and non comfirmable is supported. 
// The the response payload is piggybacked with the acknoledgment in case of confirmable requests.
// Adds a CoAP format option of 'aContentFormat' is different from the defaut format 'OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN' 
otError hppCoapRespondTo(const hppCoapMessageContext* apCoapMessageContext, const char* apPayload, size_t cbPayloadLen, otCoapOptionContentFormat aContentFormat)
{
    otError error;
    otMessage* pMyMessage;
	otMessageInfo myMessageInfo = apCoapMessageContext->mMessageInfo;

    if(apPayload == NULL) return OT_ERROR_NONE;

    pMyMessage = otCoapNewMessage(thread_ot_instance_get(), NULL);
    if(pMyMessage == NULL) return OT_ERROR_NO_BUFS;

	if(apCoapMessageContext->mCoapType == OT_COAP_TYPE_CONFIRMABLE) 
	{
		otCoapMessageInit(pMyMessage, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CONTENT);
		otCoapMessageSetMessageId(pMyMessage, apCoapMessageContext->mCoapMessageId);
	}
	else otCoapMessageInit(pMyMessage, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_CONTENT);
	
	if(apCoapMessageContext->mCoapMessageTokenLength != 0) 
		otCoapMessageSetToken(pMyMessage, apCoapMessageContext->mCoapMessageToken, apCoapMessageContext->mCoapMessageTokenLength);

	if(aContentFormat != OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN)  
		otCoapMessageAppendContentFormatOption(pMyMessage, aContentFormat);

	otCoapMessageSetPayloadMarker(pMyMessage);
	memset(&myMessageInfo.mSockAddr, 0, sizeof(myMessageInfo.mSockAddr));

    error = otMessageAppend(pMyMessage, apPayload, cbPayloadLen);
    if(error == OT_ERROR_NONE) 
	{
        if(myMessageInfo.mSockPort == OT_DEFAULT_COAP_SECURE_PORT) error = otCoapSecureSendResponse(thread_ot_instance_get(), pMyMessage, &myMessageInfo);
        else error = otCoapSendResponse(thread_ot_instance_get(), pMyMessage, &myMessageInfo);
	}

    if(error != OT_ERROR_NONE) otMessageFree(pMyMessage);

    return error;
}

// -----------------
// Read Coap Options
// -----------------

// Search for the first occurrence of a a specified coap option and return the value in apOptionBuffer
// Returns the length of the content in bytes or 0 if the option as not found or the buffer was to small.
const uint16_t hppGetCoapOption(otMessage* apMessage, otCoapOptionType aCoapOption, uint8_t* apOptionBuffer, uint16_t cbMaxLen)
{
	const otCoapOption* pCoapOption;
	
	pCoapOption = otCoapMessageGetFirstOption(apMessage);
	while(pCoapOption != NULL)
	{
		if(pCoapOption->mNumber == aCoapOption) 
		{
			if(pCoapOption->mLength > cbMaxLen) return 0;

            otCoapMessageGetOptionValue(apMessage, apOptionBuffer);
            return pCoapOption->mLength;
		}

		pCoapOption = otCoapMessageGetNextOption(apMessage);
	}

	return 0;
}


// Read content format from coap message
otCoapOptionContentFormat hppGetCoapContentFormat(otMessage* apMessage)
{
	otCoapOptionContentFormat mCoapContentFomat = OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN;
	uint8_t pui8Value[2];
    uint16_t mLenght = hppGetCoapOption(apMessage, OT_COAP_OPTION_CONTENT_FORMAT, pui8Value, 2);

	if(mLenght == 1) mCoapContentFomat = *pui8Value;
	if(mLenght == 2) mCoapContentFomat = *((__hpp_packed int16_t*)pui8Value);
	
	return mCoapContentFomat;
}


// Verify if a certain content format is accepted for an answer
// TODO: Check if multiple accepted formats really come as different coap options of if they e.g. come as
// an array of int16_t
bool hppIsAcceptedCoapContentFormat(otMessage* apMessage, otCoapOptionContentFormat aContentFormat)
{
	const otCoapOption* pCoapOption;
	
	pCoapOption = otCoapMessageGetFirstOption(apMessage);
	while(pCoapOption != NULL)
	{
		if(pCoapOption->mNumber == OT_COAP_OPTION_ACCEPT) 
		{
			if(pCoapOption->mLength == 1) 
            {
                uint8_t uiFormat;
                otCoapMessageGetOptionValue(apMessage, &uiFormat);
                if(uiFormat == aContentFormat) return true;
            }
			
			if(pCoapOption->mLength == 2) 
            {
                uint16_t uiFormat;
                otCoapMessageGetOptionValue(apMessage, &uiFormat);
                if(uiFormat == aContentFormat) return true;
            }
		}
        
		pCoapOption = otCoapMessageGetNextOption(apMessage);
	}

	return false;
}


// Get CoAP Path option. Returns a heap allocated pointer which needs to be freed in the calling program. 
char* hppGetCoapPath(otMessage* apMessage)
{
    const otCoapOption* pCoapOption;
	char* szPath = NULL;
    size_t cbPathLen = 0;
    size_t cbWritePos = 0;
	
    // Calculate path length 
    pCoapOption = otCoapMessageGetFirstOption(apMessage);
    
    while(pCoapOption != NULL)
	{
		if(pCoapOption->mNumber == OT_COAP_OPTION_URI_PATH)
		{
            if(cbPathLen > 0) cbPathLen++;
            cbPathLen += pCoapOption->mLength;  
		}

		pCoapOption = otCoapMessageGetNextOption(apMessage);
	}

	pCoapOption = otCoapMessageGetFirstOption(apMessage);
    szPath = malloc(cbPathLen + 1);
    if(szPath == NULL) return NULL;  // Out of Memory
    
	while(pCoapOption != NULL)
	{
		if(pCoapOption->mNumber == OT_COAP_OPTION_URI_PATH)
		{
            if(cbWritePos != 0) szPath[cbWritePos++] = '/';
            
            otCoapMessageGetOptionValue(apMessage, szPath + cbWritePos);
            cbWritePos += pCoapOption->mLength;
		}

		pCoapOption = otCoapMessageGetNextOption(apMessage);
	}

    szPath[cbWritePos] = 0;   // terminate the string with null byte
    return szPath;
}


// -------------------
// Secure CoAP (CoAPs)
// -------------------

// Default handler for connection events
void hppCoapsConnectedDefaultHandler(bool abConnected, void *apContext) 
{
    // Stop Coaps if aConnected == false and connection is down  (not used so far)
    if(abConnected == false && hppIsCoapsShutdownStarted == true)
    {
        //otCoapRemoveResource(thread_ot_instance_get(), ???);   ---> schould have been done before setting hppIsCoapsInitialized to false 
        otCoapSecureStop(thread_ot_instance_get());
        hppIsCoapsInitialized = false;
        hppIsCoapsShutdownStarted = false;
    }
}

// Set PSK for Coaps connections and start Coaps module 
// Sets 'aConnectHandler' as a handler and passes 'aContext' to the handler in case of a connect or disconnect event.
// If 'aConnectHandler' is NULL a default handler (hppCoapsConnectedDefaultHandler) is ussed. 
// Return true if the process was successfully started, or false if something went wrong.
// Currently only one CoAPs security context is supported.
otError hppCreateCoapsContextWithPSK(const char* szIdentityName, const char* szPassword, otHandleCoapSecureClientConnect aConnectHandler, void* aContext)
{
    otError error = OT_ERROR_INVALID_ARGS;
    
    if(hppIsCoapsInitialized) return error;    // Currently only one CoAPs security context is supported
    if(szIdentityName == NULL || szPassword == NULL) return error;
    if(aConnectHandler == NULL) aConnectHandler = hppCoapsConnectedDefaultHandler;  // default handler
    if(strlen(szIdentityName) > HPP_COAPS_ID_MAX_LEN || strlen(szPassword) > HPP_COAPS_PSK_MAX_LEN) return error;

    strcpy(hppCoapsID, szIdentityName);
    strcpy(hppCoapsPSK, szPassword);

    error = otCoapSecureSetPsk(thread_ot_instance_get(), (uint8_t*)hppCoapsPSK, strlen(hppCoapsPSK), (uint8_t*)hppCoapsID, strlen(hppCoapsID));
    if(error != OT_ERROR_NONE) return error;

    otCoapSecureSetSslAuthMode(thread_ot_instance_get(), false);   // verifyPeerCert = false
    otCoapSecureSetClientConnectedCallback(thread_ot_instance_get(), aConnectHandler, aContext);
    // otCoapSecureSetDefaultHandler(thread_ot_instance_get(), &DefaultHandler, NULL);   // no context
    error = otCoapSecureStart(thread_ot_instance_get(), OT_DEFAULT_COAP_SECURE_PORT);
    
    if(error != OT_ERROR_NONE) return error;
    hppIsCoapsInitialized = true;
    return error;
}


// Set Certificates for Coaps connections and start Coaps module  
// Sets 'aConnectHandler' as a handler and passes 'aContext' to the handler in case of a connect or disconnect event.
// If 'aConnectHandler' is NULL a default handler (hppCoapsConnectedDefaultHandler) is ussed. 
// Return true if the process was successfully started, or false if something went wrong.
// Currently only one CoAPs security context is supported.
otError hppCreateCoapsContextWithCert(const char* szX509Cert, const char* szPrivateKey, const char* szRootCert, otHandleCoapSecureClientConnect aConnectHandler, void* aContext)
{
    otError error = OT_ERROR_INVALID_ARGS;

    if(hppIsCoapsInitialized) return OT_ERROR_INVALID_STATE;    // Currently only one CoAPs security context is supported  
    if(szX509Cert == NULL || szPrivateKey == NULL) return error;
    if(aConnectHandler == NULL) aConnectHandler = hppCoapsConnectedDefaultHandler;  // default handler

    if(hppCoapsCert != NULL) free(hppCoapsCert);               // Free certificate information used before
    if(hppCoapsCertKey != NULL) free(hppCoapsCertKey);
    if(hppCoapsCaCert != NULL) free(hppCoapsCaCert);

    hppCoapsCert = (char*)malloc(strlen(szX509Cert)) + 1;      // Copy certificate to heap 
    if(hppCoapsCert == NULL) return OT_ERROR_NO_BUFS;
    strcpy(hppCoapsCert, szX509Cert);

    hppCoapsCertKey = (char*)malloc(strlen(szPrivateKey)) + 1;   // Copy private key to heap
    if(hppCoapsCertKey == NULL) return OT_ERROR_NO_BUFS;
    strcpy(hppCoapsCertKey, szPrivateKey);

    hppCoapsCaCert = NULL;
    if(szRootCert != NULL)
    {
        if(*szRootCert != 0)
        { 
            hppCoapsCaCert = (char*)malloc(strlen(szRootCert)) + 1;   // Copy CA certificate to heap
            if(hppCoapsCaCert == NULL) return OT_ERROR_NO_BUFS;
            strcpy(hppCoapsCaCert, szRootCert);
        }
    }

    error = otCoapSecureSetCertificate(thread_ot_instance_get(), (const uint8_t *)hppCoapsCert, strlen(hppCoapsCert),
                      		           (const uint8_t *)hppCoapsCertKey, strlen(hppCoapsCertKey));
    if(error != OT_ERROR_NONE) return error;
    if(hppCoapsCaCert != NULL)
    { 
        error = otCoapSecureSetCaCertificateChain(thread_ot_instance_get(), (const uint8_t *)hppCoapsCaCert, strlen(hppCoapsCaCert));
        if(error != OT_ERROR_NONE) return error;
        otCoapSecureSetSslAuthMode(thread_ot_instance_get(), true);   // verifyPeerCert = true
    }
    else otCoapSecureSetSslAuthMode(thread_ot_instance_get(), false);   // verifyPeerCert = false
 
    otCoapSecureSetClientConnectedCallback(thread_ot_instance_get(), aConnectHandler, aContext);
    // otCoapSecureSetDefaultHandler(thread_ot_instance_get(), &DefaultHandler, NULL);   // no context
    error = otCoapSecureStart(thread_ot_instance_get(), OT_DEFAULT_COAP_SECURE_PORT);

    if(error != OT_ERROR_NONE) return error;
    hppIsCoapsInitialized = true;
    return error;
}

// Start DTLS Connection with a host with the address 'aszUriPathAddress'
// Sets 'aConnectHandler' as a handler and passes 'aContext' to the handler in case of a connect or disconnect event.
// If 'aConnectHandler' is NULL a default handler (hppCoapsConnectedDefaultHandler) is ussed. 
// Return true if the process was successfully started, or false if something went wrong. 
otError hppCoapsConnect(const char* aszUriPathAddress, otHandleCoapSecureClientConnect aConnectHandler, void* aContext)
{
    otSockAddr theSockAddr;
    otError error = OT_ERROR_NO_BUFS;   // Any error

    if(!hppIsCoapsInitialized || aszUriPathAddress == NULL) return error;
    if(aConnectHandler == NULL) aConnectHandler = hppCoapsConnectedDefaultHandler;  // default handler

    memset(&theSockAddr, 0, sizeof(theSockAddr)); 
    error = otIp6AddressFromString(aszUriPathAddress, &theSockAddr.mAddress);
    if(error != OT_ERROR_NONE) return error;

    theSockAddr.mPort = OT_DEFAULT_COAP_SECURE_PORT;
    theSockAddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;

    error = otCoapSecureConnect(thread_ot_instance_get(), &theSockAddr, aConnectHandler, aContext);

    return error;
}

// Disconnect open DTLS connection
void hppCoapsDisconnect()
{
    if(otCoapSecureIsConnectionActive(thread_ot_instance_get())) otCoapSecureDisconnect(thread_ot_instance_get());
}


// Stop Coaps session
void hppDeleteCoapsContext()
{
    if(otCoapSecureIsConnectionActive(thread_ot_instance_get())) 
    {
        hppIsCoapsShutdownStarted = true;
        otCoapSecureDisconnect(thread_ot_instance_get());
    }
    else
    {
        otCoapSecureStop(thread_ot_instance_get());
        hppIsCoapsInitialized = false;
    }   
}


// -----------------------
// Manage CoAP Resources
// -----------------------

// Add CoAP resource for a specified resource name and assign a handler
// Adds the resource and 'szResourceInformation' to the '.wellknown/core' resource string if szResourceInformation != NULL
// Only adds the resource name if 'szResourceInformation' is an empty string 
// The parameter 'abSecure' determines if a CoAPs resource (true) or regular CoAP (false) resource is added
bool hppCoapAddResource(const char* apResourceName, const char* szResourceInformation, otCoapRequestHandler apHandler, void* apContext, bool abSecure)
{
    otCoapResource* pCoapResource;
    char* pResourceName;
    otError error;

    if(apResourceName == NULL) return false;
    if(strlen(apResourceName) == 0) return false;

    pCoapResource = malloc(sizeof(otCoapResource));
    if(pCoapResource == NULL) return false;
    pResourceName =  malloc(strlen(apResourceName) + 1 + 3);   // Extra three bytes for the duplicate search below
    if(pResourceName == NULL) return false;

    // It seems otCoapAddResource does not find duplicates --> try finding duplicates before
    sprintf(pResourceName, "</%s>", apResourceName);
    if(hppWellknownCoreStr != NULL)
    {
        if(strstr(hppWellknownCoreStr, pResourceName) != NULL) 
        { 
            free(pResourceName); 
            free(pCoapResource); 
            return false; 
        }
    }

    strcpy(pResourceName, apResourceName);
    pCoapResource->mUriPath = pResourceName;
    pCoapResource->mHandler = apHandler;
    pCoapResource->mContext = apContext;
    pCoapResource->mNext = NULL;

    if(abSecure) error = otCoapSecureAddResource(thread_ot_instance_get(), pCoapResource);
    else error = otCoapAddResource(thread_ot_instance_get(), pCoapResource);

    //if(szResourceInformation == NULL) apResourceName = NULL; 

    if(error == OT_ERROR_NONE && szResourceInformation != NULL)
    {
        size_t cbLen = strlen(apResourceName) + strlen(szResourceInformation) + 6;  // add </>;, + null byte
        if(hppWellknownCoreStr == NULL) 
        {
            hppWellknownCoreStr = (char*)malloc(cbLen);
            if(hppWellknownCoreStr != NULL) hppWellknownCoreStr[0] = 0;
        } 
        else hppWellknownCoreStr = (char*)realloc(hppWellknownCoreStr, strlen(hppWellknownCoreStr) + cbLen);

        if(hppWellknownCoreStr != NULL)
        {
            if(hppWellknownCoreStr[0] != 0) strcat(hppWellknownCoreStr, ",</");
            else strcat(hppWellknownCoreStr, "</");

            strcat(hppWellknownCoreStr, apResourceName);
            if(szResourceInformation[0] != 0) strcat(hppWellknownCoreStr, ">;");
            else strcat(hppWellknownCoreStr, ">");
            strcat(hppWellknownCoreStr, szResourceInformation);
        }
    }
    
    if(error != OT_ERROR_NONE)
    { 
        free(pResourceName); 
        free(pCoapResource); 
        return false; 
    }
    else return true;
}


// Coap Handlers for Resource Discovery
static void hppCoapHandler_Wellknown_Core(void* apContext, otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    if (hppIsCoapGET(apMessage))
    {

#ifdef __INCL_HPP_VAR_
        char* pchBuf;
        size_t cbVarLen;
        size_t cbWellknownCoreLen = 0;

        if(hppHideVarResources == false) cbVarLen = hppVarGetAll(NULL, 0, "",  "\\1,</var/%s>");
        else cbVarLen = 0;

        if(hppWellknownCoreStr != NULL) cbWellknownCoreLen = strlen(hppWellknownCoreStr);
        pchBuf = malloc(cbVarLen + cbWellknownCoreLen + 2);   // One extra comma + null byte

        if(pchBuf != NULL && cbVarLen != -1)
        {
            if(hppWellknownCoreStr != NULL) strcpy(pchBuf, hppWellknownCoreStr);
            else pchBuf[0] = 0;

            if(pchBuf[0] != 0 && cbVarLen > 0) strcat(pchBuf, ",");
            if(hppHideVarResources == false) hppVarGetAll(pchBuf + strlen(pchBuf), cbVarLen + 1, "",  "\\1,</var/%s>");

            //TODO: Do somethng more smart with multicaset - if(apMessageInfo->mSockAddr.mFields.m8[0] != 0xff) ...

            if(strlen(pchBuf) > 0)
                hppCoapRespondFormattedString(apMessage, apMessageInfo, pchBuf, OT_COAP_OPTION_CONTENT_FORMAT_LINK_FORMAT);
            else 
                hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_VALID);   // no Playload
        }       
        else hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_REQUEST_TOO_LARGE);

        free(pchBuf);
#else

        if(hppWellknownCoreStr != NULL)
            hppCoapRespondFormattedString(apMessage, apMessageInfo, hppWellknownCoreStr, OT_COAP_OPTION_CONTENT_FORMAT_LINK_FORMAT);
#endif
        
    }
}


// -----------------------
// Manage IP Addresses
// -----------------------

// Add unicast or multicast address to the interface
bool hppAddAddress(const char* aAddress)
{
    otNetifAddress myNetifAddress;
    otError theError;

    if(aAddress == NULL) return false;

    if(otIp6AddressFromString(aAddress, &myNetifAddress.mAddress) == OT_ERROR_NONE)
    {
        myNetifAddress.mNext = NULL;
        myNetifAddress.mPreferred = true;
        myNetifAddress.mPrefixLength = 64;
        myNetifAddress.mRloc = false;
        myNetifAddress.mScopeOverride  = 0;
        myNetifAddress.mScopeOverrideValid  = false;
        myNetifAddress.mValid = true;

        while(*aAddress == ' ') aAddress++;  //Ingore leading spaces
        if(strlen(aAddress) < 2) return false;

        // (Re-)regiter address
        if(tolower(aAddress[0]) == 'f' && tolower(aAddress[1]) == 'f') 
            theError = otIp6SubscribeMulticastAddress(thread_ot_instance_get(), &myNetifAddress.mAddress);
        else
            theError = otIp6AddUnicastAddress(thread_ot_instance_get(), &myNetifAddress);

        if(theError == OT_ERROR_NONE) return true; 
    }

    return false;
}

// Convert IPv6 address in a string and write it in aszText_out.
// aszText_out mut be 40 bytes long to hold the address string including terminating null byte.
// Returns aszText_out.
// Does NOT check if aszText_out is NULL!
char* hppGetAddrString(const otIp6Address *apIp6Address, char* aszText_out)
{
    char pTmp[6];

    aszText_out[0] = 0;
    for (int i=0; i < 16; i+=2)
    {
        sprintf(pTmp, "%02x%02x", apIp6Address->mFields.m8[i], apIp6Address->mFields.m8[i + 1]);
        strcat(aszText_out, pTmp);
        if(i < 14) strcat(aszText_out, ":");
    }

    return aszText_out;
}

// -----------------------
// Thread Callback Handler
// -----------------------

void hppThreadStateChangedCallback(uint32_t flags, void * pContext)
{
    if (flags & OT_CHANGED_THREAD_ROLE)
    {
        switch (otThreadGetDeviceRole(pContext))
        {
            case OT_DEVICE_ROLE_CHILD:
            case OT_DEVICE_ROLE_ROUTER:
            case OT_DEVICE_ROLE_LEADER:
                break;

            case OT_DEVICE_ROLE_DISABLED:
            case OT_DEVICE_ROLE_DETACHED:
            default:
                break;
        }
    }
 
}


// -----------------------------
// Default CoAP Callback Handler
// -----------------------------

#ifndef __INCL_HPP_VAR_

static void hppCoapDefaultHandler(void* apContext, otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
}

#endif


// -----------------------------------------
// Coap Bindings for Varables and H++ Parser 
// -----------------------------------------

// Common code

#ifdef __INCL_HPP_PARSER_

// Store current coap Message and MessagInfo pointers and initialize palyoad and content type variables
static void hppPrepareHppCallContext(otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    char szNumeric[HPP_NUMERIC_MAX_MEM];
    int cbPayloadLength = hppCoapGetPayloadLenght(apMessage);
 
    // Read playload
    if(cbPayloadLength > 0)
    {
        char* pchBuf = hppVarPut("0000:payload", hppNoInitValue, cbPayloadLength);
        if(pchBuf != NULL) hppCoapReadPayload(apMessage, pchBuf, cbPayloadLength);
    }
    else hppVarPut("0000:payload", "", 0);

    // Read content format
    hppVarPutStr("0000:content_format", hppI2A(szNumeric, hppGetCoapContentFormat(apMessage)), NULL);

    hppMyCurrentCoapContext.pCurrentCoapMessage = apMessage;
    hppMyCurrentCoapContext.pCurrentCoapMessageInfo = apMessageInfo;
    hppMyCurrentCoapContext.bCurrentCoapResponseDone = false;
}

// Restore current coap Message and MessagInfo pointers from apCurrentCoapContext               
static void hppInvalidateHppCallContext()
{
    hppMyCurrentCoapContext.pCurrentCoapMessage = NULL;
    hppMyCurrentCoapContext.pCurrentCoapMessageInfo = NULL;
    hppMyCurrentCoapContext.bCurrentCoapResponseDone = true; 
}

#endif

// ------------------------------------
// Handler for PUT on /var_hide
// ------------------------------------

// Makes /var/x visible and changeable if a matching PW is sent (PUT).
// The PW is stored in var "Var_Hide_PW". Otherwise the default PW HPP_HIDE_VAR_RESOURCE_PASSWORD is used.    

#ifdef __INCL_HPP_VAR_

static void hppCoapHandler_VarHide(void* apContext, otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    if(hppIsCoapPUT(apMessage))
    {
        char* pchBuf;
        const char* pchPW;
        size_t cb;

        pchPW = hppVarGet("Var_Hide_PW", &cb);
        if(pchPW == NULL)
        {
            pchPW = HPP_HIDE_VAR_RESOURCE_PASSWORD; 
            cb = strlen(HPP_HIDE_VAR_RESOURCE_PASSWORD);
        }

    	pchBuf = malloc(cb);
        if(pchBuf == NULL) return;
        hppHideVarResources = true;

        if(hppCoapReadPayload(apMessage, pchBuf, cb) == cb)
        {
            if(memcmp(pchBuf, pchPW, cb) == 0) hppHideVarResources = false;
        }

        hppCoapConfirm(apMessage, apMessageInfo);

        free(pchBuf);
    }
}

#endif


// ------------------------------------
// Handler for PUT,GET,POST on /var/xxx  
// ------------------------------------


#ifdef __INCL_HPP_VAR_

static void hppCoapVarHandler(void* pContext, otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    int cbPayloadLength = hppCoapGetPayloadLenght(apMessage);
    char* pchPayload = NULL;
    char* szPath;

    if(hppHideVarResources) return;    // Nothing to do
	
    szPath = hppGetCoapPath(apMessage);

    // Check if the message is ment for any variable
    if(szPath != NULL && strlen(szPath) >= 5)     // minimum 1 character of var name
    {
        if(strncmp(szPath, "var/", 4) == 0)
        {
            const char* szVarPath;

            otCoapCode theCoapResultCode = OT_COAP_CODE_BAD_REQUEST;
            szVarPath = hppVarGetKey(szPath + 4, false);     // Don non case senesitve search for the varibale key
            if(szVarPath == NULL) szVarPath = szPath + 4;    // New variable?
 
            if(hppIsCoapPUT(apMessage) && cbPayloadLength > 0)
            {
                bool bNewVar = false;
                theCoapResultCode = OT_COAP_CODE_REQUEST_TOO_LARGE;   // if no success

                if(cbPayloadLength <= HPP_MAX_PAYLOAD_LENGTH)
                {
                    if(hppVarGet(szVarPath, NULL) == NULL) bNewVar = true;
                    pchPayload = (char*) malloc(cbPayloadLength);
                    if(pchPayload != NULL) 
                    {
                        hppCoapReadPayload(apMessage, pchPayload, cbPayloadLength);
                        if(hppVarPut(szVarPath, pchPayload, cbPayloadLength) != NULL)
                        {
                            if(bNewVar) theCoapResultCode = OT_COAP_CODE_CREATED;
                            else theCoapResultCode = OT_COAP_CODE_CHANGED;
                        }
                    }
                    
                    free(pchPayload);
                }

                hppCoapConfirmWithCode(apMessage, apMessageInfo, theCoapResultCode);
            }
            else if(hppIsCoapGET(apMessage))
            {
                const char* pchValue;
                size_t cbLength = 0;
                pchValue = hppVarGet(szVarPath, &cbLength);

                if(pchValue != NULL)
                {
                    if(hppCoapRespond(apMessage, apMessageInfo, pchValue, cbLength, OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN) != OT_ERROR_NONE);
                    else hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_REQUEST_TOO_LARGE); 
                }
                else hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_NOT_FOUND); 
            }
            if(hppIsCoapDELETE(apMessage))
            {
                hppVarDelete(szVarPath);
                hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_DELETED); 
            }

#ifdef __INCL_HPP_PARSER_
            else if(hppIsCoapPOST(apMessage))
            {
                const char* pchCode;
                char* pchResult;
                pchCode = hppVarGet(szVarPath, NULL);

                if(pchCode != NULL) 
                {
                    // Store current coap Message and MessagInfo pointers and initialize palyoad and content type variables
                    hppPrepareHppCallContext(apMessage, apMessageInfo);       
                    pchResult = hppParseExpression(pchCode, "ReturnWithError");

                    if(pchResult != NULL)
                    {
                        if(hppMyCurrentCoapContext.bCurrentCoapResponseDone == false)    // Only automatically respond if there was no programmatical response
                        {
                            if(pchResult[0] != 0) hppCoapRespondString(apMessage, apMessageInfo, pchResult);
                            else hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_VALID);   // no Playload
                        }
                    }
                    else hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_VALID);

                    hppVarDelete("ReturnWithError");

                    // Invalidate current coap Message and MessagInfo pointers 
                    hppInvalidateHppCallContext();
                } 
                else hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_NOT_FOUND); 
            }
#endif
            else hppCoapConfirmWithCode(apMessage, apMessageInfo, theCoapResultCode);  // bad request
        }
    }
             
	free(szPath);
}

#endif


// ----------------------------------------------------
// Handler function for H++ coap requests and responses
// ----------------------------------------------------

#ifdef __INCL_HPP_PARSER_

// Handler functions for coap resources added with 'coap_add_URI'
static void hppCoapGenericRequestHandler(void* apContext, otMessage* apMessage, const otMessageInfo* apMessageInfo)
{
    const char* pchCode;
    char* pchResult;
             
    if(apContext == NULL) return;   // unknown H++ function

    pchCode = hppVarGet((char*)apContext, NULL);
    
    if(pchCode != NULL)
    {
        hppPrepareHppCallContext(apMessage, apMessageInfo);
        pchResult = hppParseExpression(pchCode, "ReturnWithoutError");

        if(hppMyCurrentCoapContext.bCurrentCoapResponseDone == false && apMessageInfo->mSockAddr.mFields.m8[0] != 0xff)    // never automatically respond to a multicast
        {
            if(pchResult != NULL) hppCoapConfirm(apMessage, apMessageInfo);
            else hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_INTERNAL_ERROR);
        }

        // Invalidate current coap Message and MessagInfo pointers
        hppInvalidateHppCallContext();
    }
    else 
    {
        // never automatically respond to a multicast
        if(apMessageInfo->mSockAddr.mFields.m8[0] != 0xff) hppCoapConfirmWithCode(apMessage, apMessageInfo, OT_COAP_CODE_NOT_FOUND);
    }

    hppVarDelete("ReturnWithoutError");   // result not used
}


// Coap response handler parsing H++ code for 'coap_GET' and 'coap_POST'
static void hppCoapGenericResponseHandler(void* apContext, otMessage* apMessage, const otMessageInfo* apMessageInfo, otError aError)
{
    const char* pchCode;
    char* pchResult;
             
    if(apContext == NULL) return;   // unknown H++ function

    pchCode = hppVarGet((char*)apContext, NULL);

    if(pchCode != NULL)
    {
        hppPrepareHppCallContext(apMessage, apMessageInfo);
        pchResult = hppParseExpression(pchCode, "ReturnWithoutError");
        if(pchResult != NULL) hppVarDelete("ReturnWithoutError");   // Not used

        // Invalidate current coap Message and MessagInfo pointers
        hppInvalidateHppCallContext();
    }
}


// Handler for H++ triggered connection events
void hppCoapsConnectedHppHandler(bool abConnected, void *apContext) 
{
    const char* pchCode;
    char* pchResult;
             
    if(apContext != NULL) 
    {
        pchCode = hppVarGet((char*)apContext, NULL);

        if(pchCode != NULL)
        {
            hppVarPutStr("0000:connected", abConnected ? "true" : "false", NULL);

            pchResult = hppParseExpression(pchCode, "ReturnWithoutError");
            if(pchResult != NULL) hppVarDelete("ReturnWithoutError");   // Not used
        }
    }

    hppCoapsConnectedDefaultHandler(abConnected, apContext);
}

// ----------------------
// Handler for CLI output
// ----------------------

static int hppCliOutputHandler(const char *apCliBuf, uint16_t aBufLength, void *apContext)
{
    const char* pchCode;
    if(apContext == NULL) return 0;   // unknown H++ function
    if(apCliBuf == NULL) return 0;   // Nothing to process   

    pchCode = hppVarGet((char*)apContext, NULL);
    
    if(pchCode != NULL)
    {
        hppVarPut("0000:cli_output", apCliBuf, aBufLength);
        hppParseExpression(pchCode, "ReturnWithoutError");
        hppVarDelete("ReturnWithoutError");   // result not used
    }

    return aBufLength;
}

// -----------------------------------------
// Handler for user CLI user command parsing
// -----------------------------------------

// generic H++ hander
static void hppCliCommandHandler(int argc, char *argv[], const char* aszCmd)
{
    const char* pchCode;
    char szNumeric[HPP_NUMERIC_MAX_MEM];
    char szParamName[HPP_PARAM_PREFIX_LEN + 1];
    int iArg = 0;
    
    if(argv == NULL) return;   
    pchCode = hppVarGet("cli_parse", NULL);
    
    if(pchCode != NULL)
    {
        sprintf(szParamName, HPP_PARAM_PREFIX, 0);
        hppVarPutStr("0000:command", aszCmd, NULL);
        hppVarPutStr("0000:argc", hppI2A(szNumeric, argc), NULL);

        do hppVarPutStr(szParamName, argv[iArg], NULL);
        while(++iArg < argc && ++szParamName[HPP_PARAM_PREFIX_LEN - 1] <='9'); 

        hppParseExpression(pchCode, "ReturnWithoutError");
        hppVarDelete("ReturnWithoutError");   // result not used
    }
}


// CLI handers do not have context parameter. Therefore we need individual callback fundtions for all user definde clie commands
//
static void hppCliCommandHandler01(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[1].mName); }
static void hppCliCommandHandler02(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[2].mName); }
static void hppCliCommandHandler03(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[3].mName); }
static void hppCliCommandHandler04(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[4].mName); }
static void hppCliCommandHandler05(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[5].mName); }
static void hppCliCommandHandler06(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[6].mName); }
static void hppCliCommandHandler07(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[7].mName); }
static void hppCliCommandHandler08(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[8].mName); }
static void hppCliCommandHandler09(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[9].mName); }
static void hppCliCommandHandler10(int argc, char *argv[]) { hppCliCommandHandler(argc, argv, hppCliCommandTable[10].mName); }

static void (*hppCliCommandHandlerList[HPP_MAX_CLI_COUNT])(int argc, char *argv[]) = {  hppCliCommandHandler01, hppCliCommandHandler02, hppCliCommandHandler03, 
                                                                                        hppCliCommandHandler04, hppCliCommandHandler05, hppCliCommandHandler06,
                                                                                        hppCliCommandHandler07, hppCliCommandHandler08, hppCliCommandHandler09,
                                                                                        hppCliCommandHandler10 };

// Print string to CLI console with LF to CRLF translation. Adds  another CRLF if abNewLine is true. 
void hppCliOutputStr(const char* aszText, bool abNewLine)
{
    if(aszText == NULL) return; 
    
    for(int i = 0; i < strlen(aszText); i++)
    {
        if(aszText[i] == '\n') otCliOutputFormat("\r\n");
        else otCliOutputFormat("%c", aszText[i]);
    }

    if(abNewLine) otCliOutputFormat("\r\n");
}

// handler for 'hpp' command executing H++ code passed as parameters 
static void hppCliHppCommandHandler(int argc, char *argv[])
{
    char* pchResult;
    char szCmd[HPP_MAX_CLI_HPP_CMD_LEN + 1];

    szCmd[0] = 0;
    for(int i = 0; i < argc; i++)    // build command string from mutliple paramters with ' ' in between 
    {
        strncat(szCmd, argv[i], HPP_MAX_CLI_HPP_CMD_LEN - strlen(szCmd));
        strcat(szCmd, " ");
        szCmd[HPP_MAX_CLI_HPP_CMD_LEN] = 0;   // make sure it is always null terminated
    }  

    pchResult = hppParseExpression(szCmd, "ReturnWithError");
    
    hppCliOutputStr(pchResult, true);
    hppVarDelete("ReturnWithError");
}
     

// --------------------------------------
// Handler for coap reladed H++ functions 
// --------------------------------------

// Evaluate Function 'aszFunctionName' with paramaters stored in variables with the name 'aszParamName' whereby
// variable names have the format '%04x_Param1'. The byte 'aszParamName[HPP_PARAM_PREFIX_LEN - 1]' is modifiyed to read the
// respectiv parameter. 
// The result is stored in the variable 'aszResultVarKey. Must return a pointer to the respective byte array of the result variable
// and change 'apcbResultLen_Out' to the respective number of bytes     
char* hppEvaluateCoapFunction(char aszFunctionName[], char aszParamName[], const char aszResultVarKey[], size_t* apcbResultLen_Out)
{
	char szNumeric[HPP_NUMERIC_MAX_MEM];
    bool bSecure = false;

    if(strcmp(aszFunctionName, "get_time_ms") == 0)     // no parameters
        return hppVarPutStr(aszResultVarKey, hppUI32toA(szNumeric, otPlatAlarmMilliGetNow()), apcbResultLen_Out);

    if(strcmp(aszFunctionName, "timeout") == 0)     // [timeout time in ms]     --> returns the maximum consumed time in ms by any H++ script start after boottime
    {
        char* pchParam1 = hppVarGet(aszParamName, NULL);
        if(pchParam1 != NULL) hppThreadTimeoutTime = atoi(pchParam1);
        return hppVarPutStr(aszResultVarKey, hppUI32toA(szNumeric, hppThreadMaxTimeMeasured), apcbResultLen_Out);
    }

    if(strncmp(aszFunctionName, "ip_", 3) == 0)
    {
        char* pchParam1 = hppVarGet(aszParamName, NULL);

        if(strcmp(aszFunctionName, "ip_add_addr") == 0)   // IPv6 address
        {
            bool bResult;
        
            bResult = hppAddAddress(pchParam1);  // hppAddAddress returns false in case the address string is NULL
            
            return hppVarPutStr(aszResultVarKey, bResult ? "true" : "false", apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "ip_get_eid") == 0)    // no paramters
        {
            const otIp6Address *pIPaddr;
            char szAddr[40];
        
            pIPaddr = otThreadGetMeshLocalEid(thread_ot_instance_get()); 
           
            return hppVarPutStr(aszResultVarKey, hppGetAddrString(pIPaddr, szAddr), apcbResultLen_Out);
        }

        if(strcmp(aszFunctionName, "ip_get_gua") == 0 || strcmp(aszFunctionName, "ip_get_ula") == 0)      // [zero based number of the GUA or ULA address  --> default 0]
        {
            char szAddr[40];
            const otNetifAddress* pUnicastAddr;
            const otIp6Address *pIPlocalEID;
            int iNum, i=0;
            uint8_t chMask = 0xFE;    // ULA
            uint8_t chValue = 0xFC;   // ULA

            if(pchParam1 == NULL) iNum = 0;
            else iNum = atoi(pchParam1); 

            pUnicastAddr = otIp6GetUnicastAddresses(thread_ot_instance_get());  
            pIPlocalEID = otThreadGetMeshLocalEid(thread_ot_instance_get());        
            if(aszFunctionName[7] == 'g') { chMask = 0xE0; chValue = 0x20; }    // GUA

            while(pUnicastAddr != NULL)
            {
                if((pUnicastAddr->mAddress.mFields.m8[0] & chMask) == chValue && memcmp(&pUnicastAddr->mAddress, pIPlocalEID, 8) != 0) i++;  // found
                if(i > iNum) break;
                pUnicastAddr = pUnicastAddr->mNext;
            }

            if(pUnicastAddr != NULL) return hppVarPutStr(aszResultVarKey, hppGetAddrString(&(pUnicastAddr->mAddress), szAddr), apcbResultLen_Out);
            else return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        }
    }
    
    // coap_XXXX commands
    if(strncmp(aszFunctionName, "coap", 4) == 0)
    {
        if(aszFunctionName[4] == 's') { bSecure = true; aszFunctionName++; }  
        aszFunctionName += 4;

        if(strcmp(aszFunctionName, "_hide_vars") == 0)  // hide (true, false)
        {
            char* pchParam1 = hppVarGet(aszParamName, NULL);

            if(pchParam1 == NULL) pchParam1 = "true";  // default
            
            if(strcmp(pchParam1, "false") == 0) hppHideVarResources = false;
            else hppHideVarResources = true;

            return hppVarPutStr(aszResultVarKey, hppHideVarResources ? "true" : "false", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_respond") == 0)  // payload, content format
        {
            otError theError = OT_ERROR_NO_BUFS; 
            size_t cbParam1 = 0;
            char* pchParam1 = hppVarGet(aszParamName, &cbParam1);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
            char* pchParam2 = hppVarGet(aszParamName, NULL);

            if(pchParam2 == NULL) pchParam2 = "0";   // content format is plain text by default
            if(hppMyCurrentCoapContext.pCurrentCoapMessage != NULL && hppMyCurrentCoapContext.pCurrentCoapMessageInfo != NULL)
                theError = hppCoapRespond(hppMyCurrentCoapContext.pCurrentCoapMessage, hppMyCurrentCoapContext.pCurrentCoapMessageInfo, pchParam1, cbParam1, atoi(pchParam2));

            hppMyCurrentCoapContext.bCurrentCoapResponseDone = true;    // do not autmoatically respond or acknowledge

            return hppVarPutStr(aszResultVarKey, theError == OT_ERROR_NONE ? "true" : "false", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_respond_to") == 0)  // coap_context, payload, content format
        {
            otError theError = OT_ERROR_NO_BUFS;
            size_t cbParam1 = 0;
            char* pchParam1 = hppVarGet(aszParamName, &cbParam1);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
            size_t cbParam2 = 0;
            char* pchParam2 = hppVarGet(aszParamName, &cbParam2);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            char* pchParam3 = hppVarGet(aszParamName, NULL);

            if(pchParam3 == NULL) pchParam3 = "0";   // content format is plain text by default

            if(cbParam1 == sizeof(hppCoapMessageContext))
                if(memcmp(((hppCoapMessageContext*)pchParam1)->szHeaderText, "context:", 9) == 0)
                    theError = hppCoapRespondTo((hppCoapMessageContext*)pchParam1, pchParam2, cbParam2, atoi(pchParam3));

            //hppMyCurrentCoapContext.bCurrentCoapResponseDone = true;  --> this is not for the present context

            return hppVarPutStr(aszResultVarKey, theError == OT_ERROR_NONE ? "true" : "false", apcbResultLen_Out);
        }
        else if(strncmp(aszFunctionName, "_is_", 4) == 0)
        {
            bool bResult;
            if(strcmp(aszFunctionName, "_is_get") == 0) bResult = hppIsCoapGET(hppMyCurrentCoapContext.pCurrentCoapMessage);  // hppIsCoapXXX returns false if aCoapMessage is NULL
            else if(strcmp(aszFunctionName, "_is_put") == 0) bResult = hppIsCoapPUT(hppMyCurrentCoapContext.pCurrentCoapMessage);
            else if(strcmp(aszFunctionName, "_is_post") == 0) bResult = hppIsCoapPOST(hppMyCurrentCoapContext.pCurrentCoapMessage);
            else if(strcmp(aszFunctionName, "_is_delete") == 0) bResult = hppIsCoapDELETE(hppMyCurrentCoapContext.pCurrentCoapMessage);
            else if(strcmp(aszFunctionName, "_is_multicast") == 0 && hppMyCurrentCoapContext.pCurrentCoapMessageInfo != NULL) bResult = hppMyCurrentCoapContext.pCurrentCoapMessageInfo->mSockAddr.mFields.m8[0] == 0xff;
            else return NULL;
            
            return hppVarPutStr(aszResultVarKey, bResult ? "true" : "false", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_context") == 0)   // no parameters
        {
            hppCoapMessageContext myCoapMessageContext;

            if(hppMyCurrentCoapContext.pCurrentCoapMessage != NULL && hppMyCurrentCoapContext.pCurrentCoapMessageInfo != NULL)
            {
                hppCoapStoreMessageContext(&myCoapMessageContext, hppMyCurrentCoapContext.pCurrentCoapMessage, hppMyCurrentCoapContext.pCurrentCoapMessageInfo);
                *apcbResultLen_Out = sizeof(hppCoapMessageContext);
            }
            else 
            {
                strcpy(myCoapMessageContext.szHeaderText, "error");
                *apcbResultLen_Out = 5;
            }

            return hppVarPut(aszResultVarKey, (const char*) &myCoapMessageContext, *apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_set_cert") == 0 && bSecure)  // X509Cert, PrivateKey,  RootCert, connected handler
        {
            otError theError = OT_ERROR_NO_BUFS;     // Any error indicating bad result is fine here
            char* pchParam1 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
            char* pchParam2 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            char* pchParam3 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '4';
            char* pchParam4 = hppVarGet(aszParamName, NULL);

            theError = hppCreateCoapsContextWithCert(pchParam1, pchParam2, pchParam3, hppCoapsConnectedHppHandler, (void*)hppVarGetKey(pchParam4, true));
            return hppVarPutStr(aszResultVarKey, theError == OT_ERROR_NONE ? "true" : "false", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_set_psk") == 0 && bSecure)  // IdentityName, Password, connected handler
        {
            otError theError = OT_ERROR_NO_BUFS;     // Any error indicating bad result is fine here
            char* pchParam1 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
            char* pchParam2 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            char* pchParam3 = hppVarGet(aszParamName, NULL);
          
            theError = hppCreateCoapsContextWithPSK(pchParam1, pchParam2, hppCoapsConnectedHppHandler, (void*)hppVarGetKey(pchParam3, true));
            return hppVarPutStr(aszResultVarKey, theError == OT_ERROR_NONE ? "true" : "false", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_connect") == 0 && bSecure)  // peer IPv6 address, connected handler
        {
            otError theError = OT_ERROR_NO_BUFS;     // Any error indicating bad result is fine here
            char* pchParam1 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
            char* pchParam2 = hppVarGet(aszParamName, NULL);
          
            theError = hppCoapsConnect(pchParam1, hppCoapsConnectedHppHandler, (void*)hppVarGetKey(pchParam2, true));
            return hppVarPutStr(aszResultVarKey, theError == OT_ERROR_NONE ? "DTLS" : "invalid", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_disconnect") == 0 && bSecure) 
        {
            hppCoapsDisconnect();
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "_stop") == 0 && bSecure) 
        {
            hppDeleteCoapsContext();
            return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
        }
        else
        {
            otError theError = OT_ERROR_NO_BUFS;     // Any error indicating bad result is fine here
            size_t cbParam3;

            char* pchParam1 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
            char* pchParam2 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '3';
            char* pchParam3 = hppVarGet(aszParamName, &cbParam3);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '4';
            char* pchParam4 = hppVarGet(aszParamName, NULL);
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '5';
            char* pchParam5 = hppVarGet(aszParamName, NULL);

            if(pchParam1 == NULL) pchParam1 = "";
            if(pchParam2 == NULL) pchParam2 = "";
            if(pchParam3 == NULL) pchParam3 = "";
            if(pchParam4 == NULL) pchParam4 = "";
            if(pchParam5 == NULL) pchParam5 = "";


            if(strcmp(aszFunctionName, "_put") == 0)  // addr, path, payload[, confirm, confirm_handler]  
                theError = hppCoapSend(pchParam1, pchParam2, pchParam3, cbParam3, OT_COAP_CODE_PUT, *pchParam4 == 't' ? hppCoapGenericResponseHandler : NULL, (void*)hppVarGetKey(pchParam5, true), *pchParam4 == 't' ? true : false);
            else if(strcmp(aszFunctionName, "_get") == 0)  // addr, path, handler, confirm  
                theError = hppCoapSend(pchParam1, pchParam2, NULL, 0, OT_COAP_CODE_GET, hppCoapGenericResponseHandler, (void*)hppVarGetKey(pchParam3, true), *pchParam4 == 't' ? true : false);
            else if(strcmp(aszFunctionName, "_post") == 0)  // addr, path, payload, handler, confirm
                theError = hppCoapSend(pchParam1, pchParam2, pchParam3, cbParam3, OT_COAP_CODE_POST, hppCoapGenericResponseHandler, (void*)hppVarGetKey(pchParam4, true), *pchParam5 == 't' ? true : false);
            else if(strcmp(aszFunctionName, "_add_resource") == 0) // path extension, resource type (e.g. rt=xyz), handler
            {
                const char* pchKey = hppVarGetKey(pchParam3, true);
                
                if(pchKey != NULL)
                {
                    if(hppCoapAddResource(pchParam1, pchParam2, hppCoapGenericRequestHandler, (void*)pchKey, bSecure)) theError = OT_ERROR_NONE;
                }
            } 
            else return NULL;    
            
            return hppVarPutStr(aszResultVarKey, theError == OT_ERROR_NONE ? "true" : "false", apcbResultLen_Out);
        }
    }

     // cli_XXXX commands
    if(strncmp(aszFunctionName, "cli_", 4) == 0)
    {
        size_t cbParam1;
        char* pchParam1 = hppVarGet(aszParamName, &cbParam1);

        if(strcmp(aszFunctionName, "cli_put") == 0)    // cmd_string
        {
            aszParamName[HPP_PARAM_PREFIX_LEN - 1] = '2';
            char* pchParam2 = hppVarGet(aszParamName, NULL);
            void* szKey = (void*)hppVarGetKey(pchParam2, true);                      // Get H++ fuction key 
            otCliConsoleInit(thread_ot_instance_get(), hppCliOutputHandler, szKey);  // Set CLI Output Handler (Handler will not no anythin if szKey == NULL)

            if(pchParam1 != NULL) otCliConsoleInputLine(pchParam1, cbParam1);    // CLI Output Handler must be set before 

            return hppVarPutStr(aszResultVarKey, pchParam1 != NULL ? "true" : "false", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "cli_writeln") == 0)    // cmd_name, callback_function
        {
            if(pchParam1 != NULL) hppCliOutputStr(pchParam1, true);

            return hppVarPutStr(aszResultVarKey, pchParam1 != NULL ? "true" : "false", apcbResultLen_Out);
        }
        else if(strcmp(aszFunctionName, "cli_add_cmd") == 0)    // cmd_name
        {
            if(pchParam1 != NULL && hppCliCommandTableSize <= HPP_MAX_CLI_COUNT)
            {
                hppCliCommandTableSize++;
                hppCliCommandTable = (otCliCommand*)realloc(hppCliCommandTable, sizeof(otCliCommand) * hppCliCommandTableSize);
            
                if(hppCliCommandTable != NULL)
                {
                    char* szDup = (char*)malloc(strlen(pchParam1) + 1);  // never freed!  Happens only HPP_MAX_CLI_COUNT times at max
                    strcpy(szDup, pchParam1);
                    hppCliCommandTable[hppCliCommandTableSize - 1].mName = szDup;
                    hppCliCommandTable[hppCliCommandTableSize - 1].mCommand = hppCliCommandHandlerList[hppCliCommandTableSize - 2];
                    otCliSetUserCommands(hppCliCommandTable, hppCliCommandTableSize);
                }

                return hppVarPutStr(aszResultVarKey, "true", apcbResultLen_Out);
            }
            else return hppVarPutStr(aszResultVarKey, "false", apcbResultLen_Out);
        }
    }

    return NULL;
}		


// -------------------------------------------------
// Poll function regularly called by H++ interpreter
// -------------------------------------------------

void hppThreadPollFunction(struct hppParseExpressionStruct* apParseContext, enum hppExternalPollFunctionEventEnum aPollFunctionEvent)
{
    if(aPollFunctionEvent == hppExternalPollFunctionEvent_Poll && hppThreadTimeoutTime != 0)
    {
        if(otPlatAlarmMilliGetNow() - apParseContext->uiExternalTimer > hppThreadTimeoutTime)
            apParseContext->eReturnReason = hppReturnReason_Timeout;   // Stop H++ exection with timeout error
    }
    else if(aPollFunctionEvent == hppExternalPollFunctionEvent_Begin)
    {
        // Remember start time
        apParseContext->uiExternalTimer = otPlatAlarmMilliGetNow();
    }
    else if(aPollFunctionEvent == hppExternalPollFunctionEvent_End)
    {
        // Keep track of the longest execution time
        uint32_t uiTimeDiff = otPlatAlarmMilliGetNow() - apParseContext->uiExternalTimer;
        if(hppThreadMaxTimeMeasured < uiTimeDiff) hppThreadMaxTimeMeasured = uiTimeDiff;
    }
}

#endif

// ----------------------------------
// Thread Initialization on nRF 52840
// ----------------------------------

static void hppThreadInit()
{
	uint32_t error_code;
	ret_code_t err_code;
	otError error;
    thread_configuration_t thread_configuration =
    {
        .radio_mode            = THREAD_RADIO_MODE_RX_ON_WHEN_IDLE,
        .autocommissioning     = true,
        .default_child_timeout = 10,
    };
	

	// Needed for Thread to work?  
    // TODO: Check if this block can be moved to hppNRF527840.c
	err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    error_code = app_timer_init();
    APP_ERROR_CHECK(error_code);

	APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);   
   
   /*
    thread_configuration.role = RX_ON_WHEN_IDLE;
    thread_configuration.autocommissioning = true;
    thread_configuration.poll_period = 2500;
    thread_configuration.default_child_timeout = 10;
	*/

    thread_init(&thread_configuration);
    thread_cli_init();
    otSetStateChangedCallback(thread_ot_instance_get(), hppThreadStateChangedCallback, thread_ot_instance_get());

    error = otCoapStart(thread_ot_instance_get(), OT_DEFAULT_COAP_PORT);
    ASSERT(error == OT_ERROR_NONE);

    hppCoapAddResource(".well-known/core", "", hppCoapHandler_Wellknown_Core, NULL, false);
	
#ifdef __INCL_HPP_VAR_
	otCoapSetDefaultHandler(thread_ot_instance_get(), hppCoapVarHandler, NULL);
    hppCoapAddResource("var_hide", NULL, hppCoapHandler_VarHide, NULL, false);      // NULL in second parameter means not discoverable
#else
    otCoapSetDefaultHandler(thread_ot_instance_get(), hppCoapDefaultHandler, NULL);
#endif

#ifdef __INCL_HPP_PARSER_
    hppAddExternalFunctionLibrary(hppEvaluateCoapFunction);

    hppCliCommandTable = (otCliCommand*) malloc(sizeof(otCliCommand));
    if(hppCliCommandTable != NULL)
    {
        hppCliCommandTableSize = 1;
        hppCliCommandTable[0].mName = "hpp";
        hppCliCommandTable[0].mCommand = hppCliHppCommandHandler;
        otCliSetUserCommands(hppCliCommandTable, hppCliCommandTableSize);
    }

    hppAddExternalPollFunction(hppThreadPollFunction);

#endif
}



// ---------------------------------
// Main Loop for Thread on nRF 52840
// ---------------------------------

static void hppMainLoop()
{
	while (true)
    {
        thread_process();
        app_sched_execute();

        if (NRF_LOG_PROCESS() == false)
        {
            thread_sleep();
        }
    }
}

