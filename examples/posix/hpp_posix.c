/* ----------------------------------------------------------------------------	*/
/* Halloween++														            */
/* ----------------------------------------------------------------------------	*/
/* Open source scripting language for microcontrollers and embedded	            */
/* systems.															            */
/* Invented for remote controlled Halloween ghosts and candles :-)	            */
/* ----------------------------------------------------------------------------	*/
/* File: hpp_POSIX.c												            */
/* ----------------------------------------------------------------------------	*/
/* Test application for H++ Parser on 							            	*/
/*																	            */
/*   - H++ demonstration for POSIX environment													            */
/* ----------------------------------------------------------------------------	*/
/* Platform: POSIX and alike											        */
/* Dependencies: hppVarStorage.c, hppParser.c							        */
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


#include "src/hppVarStorage.c"
#include "src/hppParser.c"




int main(int argc, char** argv)
{
	const char* szCode;
	
	szCode = "a = 1;\n"					// local variable
			 "txt1 = \"hello\";\n"		// local variable
		     "txt2 = 'hello';\n"		// another local variable
			 "B = 2;\n"					// global variable
			 "writeln(B + a);\n"		// writes a line with text '3' to the output terminal
			 "//writeln(unknown);\n"    // Error #205  --> UnknownVariable
			 "writeln('unknown:' ~ ?unknown);\n";    // writes an empty line - no error
			 
	// hppParseExpression(..) returns an arror message if the esult var name is "ReturnWithError", NULL otherwise
	printf("return=%s\n\n", hppParseExpression(szCode, "ReturnWithError"));
	hppVarDelete("ReturnWithError");   // Delete result variable
	
	
	// Put code in global variable from C code
	hppVarPutStr("MyCode", "writeln('hello global world');\n"
	                       "writeln(?param1);\n" 
	                       "writeln(?param2);\n", NULL);
	
	szCode = "my_code = \"writeln('hello local world');\";\n"
			 "my_code();\n"
			 "MyCode(1,2);\n"
			 "Code = \"<param1> = 'new';\";\n"
			 "Code(&s);\n"
			 "return s;";        
       

	printf("return=%s\n", hppParseExpression(szCode, "ReturnWithError"));	
	hppVarDelete("ReturnWithError");    // Delete result variable
	
	
	hppVarPutStr("EvenNum",	"i = param1;\n" 
							 "a = param2;\n"      						// must be even number

							 "while(i <= a)\n"
							 "{\n"
							 "	if(i > 100) return 'truncated';\n"		// stop at 100
							 "	if(i % 2 == 1) i++;\n"     				// skip odd numbers
							 "	writeln('i = ' ~ i++);\n"  				// print even numbers
							 "}\n"
							 "else\n"
							 "{\n"
							 "	writeln('none');\n"
							 "}\n"
							 "return 'ok';", NULL);     
				 			   
	szCode = "return EvenNum(10,20);";
	printf("return=%s\n", hppParseExpression(szCode, "ReturnWithError"));	
	szCode = "return EvenNum(90,200);";
	printf("return=%s\n", hppParseExpression(szCode, "ReturnWithError"));	
	
	hppVarDelete("ReturnWithError");    // Delete result variable

	return 0;
}
 
