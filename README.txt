H A L L O W E E N + +   -  Readme -  v0.10
------------------------------------------

Halloween++ is an open source scripting language for microcontrollers and embedded systems especially suitable 
for remote controlled systems featuring a restful web interface for IP networks.
It originally was invented for remote controlled Halloween ghosts and candles :-)

The project includes bindings for OpenThread running on a Nordic Semiconductor nRF52840 chip. 
OpenThread (openthread.io) is an open source implementation of the IPv6 based wireless mesh network protocol
Thread (www.threadgroup.org). The code runs on many Nordic nRF52840 based boards. A good point to start is the
nRF52840 USB dongle from Nordic Semiconductor.  
(www.nordicsemi.com/about-us/buyonline?search_token=nRF52840DONGLE)

There is variety of system on chip (SOC) and chipset options supporting IEEE 802.15.4 and Thread. The Nordic
nRF52840 SOC and the OpenThread stack is just one possible combination. Porting this code to other systems 
or even other Thread stacks should be possible.   

This repository contains the source code for the scripting language, bindings for OpenThread and bindings for
certain other functions of the nRF52840 SDK. The Nordic Semiconductor SDK for Thread is not included. It can
be downloaded from the Nordic Semiconductor web-site. 
The scripting language itself can also be used in different context. The respective sources "hppVarStorage.c" and
"hppParser.c" should compile in any POSIX conform environment, e.g. standard gcc in linux will do it.

This repository also includes binaries compiled for nRF52840. If you do not wish to modify the C source code
and rather prefer to just write H++ scripts, you can directly upload the pre-compiled code and use a CoAP browser
to add scripts to the device.   


HISTORY

Version	Date		Changes
------+---------------+----------------------------------------------------------------------------------------
0.90  |	Oct 6, 2018   |	Initial version for Halloween 2018
------+---------------+----------------------------------------------------------------------------------------
0.95  |	Feb 25, 2019  |	Start of this document and plan to open source the code  
------+---------------+----------------------------------------------------------------------------------------
1.00  |	June 8, 2019  |	First published open source version
------+---------------+----------------------------------------------------------------------------------------


CONTENT

1) Web-Links and downloads 
2) Setup development environment
3) Flash devices
4) Border router setup
5) Device discovery
6) H++ scripting core language description
7) Thread bindings
8) nRF52840 hardware driver bindings



1) Web-Links and downloads
--------------------------

The following chapters refer to the links below for setting up the toolchain and explanation of underlaying
protocols. 

Getting Started with Thread - Toolchain
LINK 1: infocenter.nordicsemi.com/topic/com.nordic.infocenter.thread_zigbee.v3.0.0/thread_zigbee__intro.html
        
Open Thread Border Router with Nordic nR52840 
Link 2: infocenter.nordicsemi.com/topic/com.nordic.infocenter.thread_zigbee.v3.0.0/thread_border_router.html

nRF52840 Dongle User Guide
Link 3: infocenter.nordicsemi.com/topic/ug_nrf52840_dongle/UG/nrf52840_Dongle/intro.html

OpenThread CLI Reference
Link 4: https://github.com/openthread/openthread/blob/master/src/cli/README.md

Thread Group
Link 5: threadgroup.org 

Open Thread Project 
Link 6: openthread.io

CoAP -The Constrained Application Protocol - RFC
Link 7: tools.ietf.org/html/rfc7252

CoAP -The Constrained Application Protocol - Overview
Link 8: coap.technology

coap-client - CoAP Client based on libcoap
Link 9: http://manpages.ubuntu.com/manpages/cosmic/man5/coap-client.5.html



2) Setup development environment
--------------------------------

Step 1:
Download all files of this github repository.

Step 2:

Case A) Compile H++ script parser in POSIX environment:   
The core components of the H++ scripting language compile in any POSIX conform environment. To test it with
a simple command line application in linux, just copy hpp_POSIX.c from examples/posix folder to the project 
main folder and compile it with "g++ hpp_POSIX.c".
This file includes "hppVarStorage.c" and "hppParser.c" from "src" directory. No need for a makefile.
--> Continue at chapter 6 if you only want to try the scripting language on a computer

Case B) Use the pre-compiled Firmware for nRF52840:
--> Continue at chapter 3 

Case C) Compile code for a nRF52840 board and run Thread:
Follow instructions in Link 1 to set up the development environment. The nRF52840 SOC is based on an ARM
processor core. The You will need to download the GNU Tools for Arm embedded systems.
The Nordic SDK is also needed. This code is made for and tested with the "Thread and Zigbee SDK version 3.0.0."
SDK version: nRF5_SDK_for_Thread_and_Zigbee_v3.0.0_d310e71
The SDK includes pre-compiled OpenThread libraries. It's easiest to use this version. The Nordic guide
also includes instructions how to upgrade the OpenThread version with sources from the OpenThread github 
project. The following instructions are made having a windows development environment in mind 
(e.g paths having '\'). Development tools are also available for Linux.   

Step 3:
Create a new folder "MyPrograms" inside the SDK folder (nRF5_SDK_for_Thread_and_Zigbee_v3.0.0_d310e71\MyPrograms).
Create a new folder "Halloween" inside the "MyPrograms" folder.
Create a new folder "HppCodeParser" inside the "Halloween" folder.   
The "HppCodeParser" folder is also called <project folder> in the following text.

Copy the content of the "<SDK folder>\examples\thread\cli" folder of the SDK in the new <project folder>.  
The folder names are not essential. Only the number of sub-folders is important to match with the relative
paths in the SDK makefile.
Now you should be able to compile the Nordic CLI demo using the makefile for the USB bootloader version 
(start "make" in the folder <project folder>\ftd\usb\pca10059\mbr\armgcc) or start "DoMake.cmd"
in the <project folder> to do the same in a Windows environment.

Step 4:
Rename "main.c" in the "HppCode" folder to "org_cli_main.c" or delete it.
Copy all files of the github repository in the folder "HppCodeParser".
The project "main.c" file includes all necessary additional components, such that we can keep the CLI example 
makefile from the SDK unchanged and reuse it. This is easiest when upgrading to a newer SDK version. 

Now run make with the SDK makefile in the subfolder "<project folder>\ftd\usb\pca10059\mbr\armgcc"
or run DoMake.cmd again.
If everything runs smooth without errors, you should now have a new "nrf52840_xxaa.hex" file in the 
"<project folder>\ftd\usb\pca10059\mbr\armgcc\_build" folder.
This file will be uploaded to the nRF52840 flash memory in the next chapter.



3) Flash devices
----------------

Follow Programming instructions in the Dongle user guide (Link 3) to flash the nRF52840 Dongle with the
newly compiled firmware "<project folder>\ftd\usb\pca10059\mbr\armgcc\_build\nrf52840_xxaa.hex" or the
pre-compiled binaries in the folder "<project folder>\tools\firmware".
   
The nRF Connect tool is very easy to use. Just make sure you program the right HEX file. Only HEX files
in the "usb\pca10059" folders of the SDK are meant for the USB dongle.

After successful programming, the green LED on the dongle starts blinking. This is already a demo H++ script
being executed on the chip.    
Pushing the white button on the top of the dongle starts another demo script sending a multicast message to
all other nodes in the mesh network (if any available). Both the sending node and all receiving nodes will
toggle their LED in reaction to the button push.  

You can also connect to the command line interface (CLI) of the dongle using a terminal program. Follow
instruction at Link 1. Documentation for the Thread CLI commands can be found at Link 4.
With the CLI you can explore which IP addresses the device has (type "ipaddr") or check if there is other
nodes in the network (type "router table" to find devices in the network). In fact, multiple dongles flashed
with this firmware will immediately form a mesh network using the default settings and credentials in the
firmware. This is good for testing but should obviously be changed for productive use.

The Halloween project adds one more CLI command to the Thread CLI: hpp <x> 
The command "hpp <x>" starts the H++ script with the name <x> or returns an error if unavailable.
Try: hpp do_pwm();
This should make the LED brightness go up and down in a loop using the PWM peripheral.  
You can find this and other demo scripts in the function LoadDemoScripts() inside the "main.c" file.
     
Running scripts from the CLI is nice, but we want to use wireless communication to modify and run scripts.
This requires a border router to connect the wireless Thread network with your home network.  



4) Border router setup
----------------------

The number of commercially available border routers is currently limited. Most border routers are combined 
with home automation products having also other than router functions. A popular example is the Google/NEST
thermostat. The easiest way to build an application agnostic border router is using a Raspberry Pi together
with another Thread Dongle.     

Link 2 has instructions how to set up a border router connecting a thread mesh network with an ethernet
backbone or WIFI home network using a Raspberry Pi 2B and a Nordic Semiconductor nR5852840 dongle. For
the following instructions we assume the border router is connected to the home network using a LAN cable. 
The default settings and credentials in the border router project and NCP firmware allow the border router
to immediately connect with other nRF52840 dongles. Again, this is easy for testing purpose, but should
be changed for productive use. Settings and credentials can easily be modified using the web interface
of the border router.

Once the border router is online, other Thread devices in the network may automatically register additional
IPv6 addresses based on IPv6 network prefixes distributed by the border router. You can check that with the
ipaddr CLI command. Which additional IPv6 addresses devices register very much depends on the settings in your
home router and even your internet service provider (ISP). Not every ISP provides native IPv6 internet connectivity.
Even if no addresses are added, the OpenThread border router allows Thread devices to talk to the internet 
using NAT64 (see LINK 2). 
 
For the purpose of this document, we don't want to depend on any router settings or ISP capability. We rather
use a local IPv6 subnet on the home network to connect a computer with Thread devices.
   
Open a terminal connection to the raspberry pi and edit the '/etc/rc.local' file with superuser rights.
e.g. type sudo nano /etc/rc.local 
Add the following at the end of the file before the exit command. 

ip -6 addr add FD10:CA1:FEED::B001/64 dev eth0
ip -6 route add FD10:CA1:FEED::/64 dev wpan0
ip -6 route add FD11:22::/64 dev eth0

This will add an IPv6 address (FD10:CA1:FEED::B001) to the raspberry eth0 interface and configure routes
between the subnet with the prefix FD10:CA1:FEED::/64 on eth0 and the subnet FD11:22::/64 on the Thread
mesh network (wpan0). The on-mesh network prefix FD11:22::/64 should be configured by default on the border
router. If Thread devices to not configure IPv6 addresses with this prefix, you need to add the on-mesh
prefix through the border router web interface or add an address with this prefix to the Thread
interface using the command 'ip -6 addr add FD11:22::1/64 dev wpan0'. This should not be needed if you use
the Nordic border router image described in LINK 2 with default settings.

The raspberry pi border router does NOT automatically announce the new subnet on the home network. So, your
computer will not know anything about the new subnet prefix unless you install and configure the radvd
(router advertisement daemon) service on the raspberry.
For the purpose of this introduction it is enough to manually configure an IPv6 address with this prefix
and a route for the Thread on-mesh network prefix on the computer. 

For a Windows PC you can do this with the following commands in a shell with administrator rights:   
   
netsh interface ipv6 add address "WLAN" FD10:CA1:FEED::C001
netsh interface ipv6 add route FD11:22::/64 "WLAN" FD10:CA1:FEED::B001

This configures the address FD10:CA1:FEED::C001 on the WLAN interface of the PC (assuming the PC is
connected via WLAN to the home network). You can change this to LAN if the PC is connected via LAN cable.
The second command configures the raspberry pi IPv6 address as a gateway for the Thread network.
Add option "-p" for netsh to make this setting permanent after reboot. 



5) Device discovery
-------------------

Thread devices usually use SLAAC to configure unique IPv6 addresses. This essentially means they select a
random interface ID (last 8 bytes) for the prefixes advertised by the border router and verify there is no
address duplication in the same subnet. If you are connected via USB to the CLI of the device, you can
find out the IP addresses with the ipaddr CLI command. It a real IoT application, this may not be
practicable. There are several methods known for device discovery. A popular method is muticast discovery.
Alternatively, devices may register themselves to a resource directory.

For the purpose of this introduction we choose the quick and dirty method and just add a fixed IPv6 address 
with the on-mesh prefix. You can find the H++ script command ip_addr_add("FD11:22::xxx") doing that in the
startup script created by the function LoadDemoScripts() inside the "main.c" file.
Makes sure you change that address for every device you flash with the H++ firmware. The fixed IPv6 addresses
are used to individually address the nodes of the mesh network e.g. with a CoAP browser or any CoAP client.

Link 8 introduces CoAP technology and available tools for CoAP. Basic understanding of CoAP is essential for
using Thread and the Thread bindings in this project.    
A popular choice for a CoAP Browser is the Firefox extension Copper. Unfortunately, Copper has not been
updated for the latest Firefox version. A simple CoAP.net based Windows CoAP application is included in
this repository in the "<project folder>\tools\coap_browser" folder.  
A popolar linux command line option for a CoAP client is 'coap-client' (see Link 9).  

    
 
6) H++ scripting core language description
------------------------------------------

H++ is a lightweight interpreter. The syntax of the language is similar to the C, Java, Javascript, C#, ...
family of languages. But variables in H++ are always byte strings. Integer numbers are encoded as decimal
strings as well as floating point numbers. This makes it very easy to interact with H++ language resources in a CoAP
or HTTP/restful way. All variable and the program code itself will be presented as CoAP resources once the
Thread binding is included in the project (unless deactivated by a corresponding H++ command). 
             
A CoAP browser application implementing .well-known/core type resource discovery should be able to discover
show and modify all H++ language resources under the resource path:
coap://[device_IPv6_addr]/var/<var name>

Encoding numbers as strings is computationally not very efficient. Therefore H++ should not be used for bigger
numerical calculation tasks. It works fine for code focusing on program logic rather than number crunching.
It is also useful for rapid prototyping of code which shall later be translated in C or C++ code.


Variables:

Because string is the only supported variable type, there is no need to declare variables before use in H++.
However, reading an unassigned variable causes an error #205, unless there is a '?' directly before the 
variable. In this case an empty string, corresponding to numerical value 0, is returned when reading an
unassigned variable.

Byte string literals can be written with " or ' to allow quotation marks ('"' and "'") in string literals.
There is no escape codes like "\n" in H++ (so far).

Variable names with an upper-case letter in front stand for global variables and variable names with a lower-
case letter in front stand for local variables. Internally, local variables are stored with a number in front.
This number denotes the stack level of the local variable. Local variables are automatically deleted at the end
of a function.

Unlike zero terminated C strings, H++ strings can contain zero bytes.

a = 1;			// local variable
txt1 = "hello";		// local variable
txt2 = 'hello';		// another local variable
B = 2;			// global variable
writeln(B + a);		// writes a line with text '3' to the output terminal
writeln(unknown);       // Error #205  --> UnknownVariable
writeln(?unknown);      // writes an empty line - no error

From the calling C code, H++ variables can be accessed using hppVarPut(... ), hppVarPutStr(...) and hppVarGet(..).
See file hppVarStorage.h for details. Also, lower case global variables with no number in front can be created
with hppVarPUT. However, those variables can't be accessed from H++ using the variable name, because a local
variable would be expected in that case. It is anyway useful to inject lower case variables containing H++ code
from calling C code or through a CoAP operation. Such code variables can be executed but not overwritten easily.

  
Code Execution:

The '(' operator after a variable name executes H++ code inside a variable. Up to 9 parameters can be passed
to a function and later be accessed as local variables 'param1' to 'param9'. For readability reasons it is
recommendable to assign the paramter values to variables with more speaking names at the beginning of the
function. The leading '?' operator may be used for optional parameters. Often H++ functions are very simple
and still readable if a limited number of 'paramX' parameter variables are used directly.  

In a Thread based project code can also be injected with a CoAP PUT operation:
PUT coap://[device_IPv6_addr]/var/<function_name> - Example payload: writeln('hello world');
A POST operation on the same path executes the code. GET and DELETE operations also work as expected.

my_code();    // executes the H++ code in the injected variable 'my_code' or reports Error #214 if it does not exist
my_code = "writeln('hello local world');";
my_code();    // executes the H++ code in the local variable 'my_code'  (local has priority over injected global)

MyCode = "writeln('hello global world'); writeln(?param1); writeln(?param2);";
MyCode(1,2);  // executes the H++ code in the global variable 'MyCode' with value 1 & 2 for param1 & param2.

CoAP URI paths are not case sensitive. New variables are always lowercase if created by a PUT operation.
A CoAP PUT may also overwrite an existing upper case global variable created by H++ code before. This
preserves the capitalization of the existing variable.


Operators:

H++ supports many of the operators known from C and C++ like languages.

Unitary Operators: { ( [ . :: & ++ -- 
Binary Operators:  + - * / % & | ^ == != < > <= >= && ||

Currently not supported are: += -= *= /= &= |= ^= ?: -> and leading *    
  

Key differences compared to C / C++:

1) [] (without '.' behind) operator: Bytewise reads and writes the content of a byte string.  
   Writing behind an existing string enlarges the string automatically. Bytes between the existing byte string
   and the newly assigned value (if any) are initialized with zero. E.g. a[5] = 1; 
2) :type[] operator: Interprets a byte string as a binary coded array of type 'type.   
   e.g. a:int16[0] = 10000; a:int16[1] = 10001;  --> creates a 4 byte string with two 16 bit integer numbers
   See Data structures section for a list of supported types.  
3) :type*[] operator: Same as above, but the index between the squared bracket is interpreted a byte index.
   e.g. a:int8*[0] = 100; a:int16*[1] = 10001;  --> creates a 3 byte string with one 8 bit and one 16 bit integer     
4) . operator: Accesses members of a binary coded structure created with the build in function struct(..). 
   Values are automatically initialized with zero. E.g.  mystruct = struct('int16:i,int32:l'); zero = mystruct.l;
5) [index]. operator: Accesses an array of structures. Writing behind an existing structure enlarges the structure
   automatically and initializes values with zero. E.g. mystruct[5].i = 1;
6) :: operator: Denotes that a method name or sub item following. E.g. v = 'hallo'; return v::len();  // methode call
   if no '(' operator is following, :: justs adds '.' as part of the variable name. E.g.  a::1 = 5;  // accesses /var/a.1   
   The character '.' was chosen because ':' is not a valid character inside a URI string; a::1 corresponds to a.1 in URI notion 
7) == and != operators: Bytewise comparea two strings. Comparing values requires that numbers are coded in the
   same format. So 1.0 == 1 is false(!), but val(1.0) == val(1) is true.     
   Unlike the above < > >= <= compare values. So 1.0 <= 1 is true.  
8) & operator (leading): Results in a string with the variable name including stack level prefix in case of local 
   variable. This can be used for passing/returning values by reference. (see <..> operator below).   


Additional operators in H++:

1) ~ operator: Concatenates two byte strings. Remember, unlike C-strings H++ strings can contain zero bytes.  
2) <exp> operator: Evaluates the expression exp and interprets it as variable name or part of a variable name.
3) ? operator (leading): Results in empty string for unassigned variables (see above) 
      
a::1 = 1;
a::2 = 2;
s = a::1 ~ a::2;   // results in s = "12"  

i = 1;
a::<i>;           // results in a.1  --> 1
a::<++i>;         // results in a.2  --> 2     

v = "hello";
ref = &v;
s = <ref>;       // results in s = "hello" 

Code = "<param1> = 'new'";
Code(&s);        // results in s = "new"

The expression name::<i> can be used to imitate arrays in cases where binary arrays 'name[i]'  or name[i].member are
not sufficient. However, large arrays of that kind should be avoided. Every element name.<i> is stored as an individual
variable. A high number of variables makes H++ execution slow because every access operation to a variable is a search
operation. Also the name of every variable must be stored, making this a very memory unefficient method.

 
Control Structures:

H++ support two types of control structures if and while. Like in C language if(..) can be followed by one single expression
followed by ';' of by a set of expressions inside curly brackets. The if(..) control structure may have and else part.
The while loop has similar syntax like in C langauge, but also allows for an else the option. The else branch will only be
executed if the while part was never executed. The key words break, continue and return work similar to C language.
 
i = param1; 
a = param2;      			

while(i <= a)
{
   if(i > 100) return 'truncated';	// stop at 100
   if(i % 2 == 1) i++;     		// skip odd numbers
   writeln('i = ' ~ i++);  		// print even numbers
}
else
{
   writeln('none');
}

return 'ok';
  
  
Comments:

All text behind '//' up to the end of the line is ignored by the H++ interpreter, such that comments can be put behind.
Be aware that comments are stored together with the program code and therefore cost memory resources in the microcontroller.
Also code with comments runs slighity slower.   
The usual C style comments with /* ... */ are not allowed in H++. This commenting style is reserved for future editors or upload
tools automatically removing such comments while uploading or exporting.


Data Structures:

In H++ data structures are dynamically typed. Each structure has a header holding the definition of the structure and the binary
content. The binary content can be none, one or multiple instances of the structure. 
Members can be accesses with the . or [index]. operators behind a variable name. E.g. a.member = 1; or a[0].member = 1;
Structures are created using the struct(..) function. It has one paramter holding a comma separated list of struct elements with
each element holding a 'type_name:member_name pair'. The cormat string shall not contain whitespaces. Member names are alphanumeric.
Pure numeric member names are also allowed. See also documentation for the  '.' and '[index].' operators above.

Allowed data types: int8, int16, int32, bool, var, array, string, float, double, fixstr, uint8, uint16, uint32
The same data types are also available with the :type[] and :type*[] operators (see above).   

The integer types exist in 8, 16 and 32 bit lenght and unsinged versions. Float, double and bool are the equivalent to the
respective C types. Bool is stored as one byte. 
The type fixstr is a zero terminated C string with fixed record lenght of HPP_FIXSTR_MAX_LEN + 1 bytes, such the maximum string
len is HPP_FIXSTR_MAX_LEN (equals 32, unless changed in the file "hppVarStorage.h").

The types var, array and string all store the name of another H++ variable with maximum lenght HPP_MAX_MEMBER_NAME_LEN (= 15),
which itself contains another byte string with a H++ string, struct or struct array inside. All three types are basically
interchangable. However, they are kept separate for documentation reasons and to support data type conversion in other
structured data formats like JSON, CBOR or XML.
Variable names for var, array and string are automatically generated based on the main structure upon read or write access.
 
MyStruct = struct('int16:i,var:v,double:num');
MyStruct.num = 1.1;
MyStruct.i = 2; 
MyStruct.v = struct('string:5,fixstr:fix'); 
MyStruct.v.5 = "str"; 
MyStruct.v.fix = "cstr";

The above code results in three variables: MyStruct, MyStruct.17 and MyStruct.17.12. MyStruct contains the root of the structure
with the members i, v and num. The second and third variables are automatically create. The value of MyStruct.v is actually stored
in MyStruct.17, because v is of the type var and itself contains another structure. MyStruct.17.12 contains the value of MyStruct.v.s.
Automatically generated variables should never be directly used. They just serve as a container for structure elements.  
   
It is important to note that ++, -- and :: operators cannot be applied to binary data types and fixstr.
For var, string and array the trailing ++, -- operators and method :: can be used.   
For those types '.' and '[index].' operators can also be nested. (e.g. see MyStruct.v.fix in the example above)


Build-in functions:

The following mathmatical functions are supported by H++: abs(), int(), val(), sin(), cos(), tan()
Most functions have similar function like the respective C functions. 

Other functions:
- int(a): 	 Returns the integer part of the number a
- val(a):	 Converts a number string in a standard format number string. See operator '=='
- struct(a):	 Creates a data structure with members defined in a. See Data Structures section.  
- writeln(str):  Writes the string str and a terminating '\n' (nex line) to stdout. The primary use-case for H++ is
     		 embedded systems, where stdout usually does not exist. For that reason, no other printf / scanf like
		 functions are available.   
 	  
Adding other functions is quite straigt forward. Look at hppEvaluateFuction(..) in the the file hppParser.c to learn.
Custom function libraries can be added using hppAddExternalFunctionLibrary(..).


Build-in methods:

The following build-in methods are supported by H++:
(a stands for an arbitrary variable name the method is applied to)

- a::len():	    Returns the lenght for the byte string a in bytes. E.g. a = "Hallo";  l = a::len();  --> l = 5
		    Remember, unlike C language, H++ byte strings can contain zero bytes. 
                    If a does not exist len() does not produce and error but returns -1.    
- a::count():       Returns the number of elements of an array of structs. The number of elements includes incomplete structs, which
               	    have not been fully initialized. E.g.  a = struct('int16:i, int32:l'); a.i = 0; num = a::count();  --> num = 1  
- a::typeof(n):     Returns a string with the type name of the member n. It returns 'unvalid' in case no member with name n exists.
- a::vars_count():  Returns the number of variables starting with 'a::' ('a.' in variable name and '/var/a.' in URI notion) but not
                    have further sub items like a::x::y.
- a::vars():	    Returns a comma separated list sub items without further sub items.
- a::roots():	    Returns a comma separated list sub items with further sub itemsb, but excluing sub items without sub items.                  
- a::alloc(n):	    Allocates n bytes of memory for the variable a and initializes additional memory with zero bytes.
                    The present content of a remains unchanged.
- a::realloc(n):    Re-Allocates n bytes of memory for the variable a and does NOT initialize additional memory.
- a::item(n):	    Returns the n'th item of a comma separated list. item(n) does not trim the entries. Possible leading or trailing
		    whitespaces are included in the result.
- a::find(s):	    Searches for the content of s in a and returns the position or -1 of not found. find(s) works with any text or
		    binary content.
- a::sub(i[,n]):    Returns a substring or binary sub-content of a starting with bytewise index i and maximum lenght n. n is optional.
- a::replace(i, c): Replaces the content of a wigth c starting from position i. It may enlarge the lenght of a to hold the new content.
                    Any gap between the present content and the new content staring at position i is filld with ' ' (space).
          	       

Error Codes:

150: Timeout
200: FatalError                 (Typically means out of memory)
201: BreakWithoutWhile
202: ContinueWithoutWhile
203: ClosingBracketExpected
204: ClosingAngleBracketsExpected
205: UnknownVariable
206: BooleanValueExpected
207: ClosingSquaredBracketExpected
208: SemicolonExpected
209: InvalidOperator
210: MissingArgument
211: DivisionByZero
212: FirstOperand_BooleanValueExpected
213: SecondOperand_BooleanValueExpected
214: UnknownFunctionName
215: CannotCallMethodOnResult
216: UnknownArrayType
217: OpeningSquaredBracketExpected
218: StructVarNameTooLong
219: UnknownMemberType
220: StackOverflow


7) Thread bindings
------------------

Thread bindings are functions added to the core H++ scripting language.

- get_time_ms():	Returns a platform time in milliseconds.
- timeout(t):		Sets the timout value in ms for H++ parsing. The H++ interpreter produces an error 150 (Timeout) if processing
			of H++ code exceeds the timout value. Returns the longest processing time measured since power up of the
			system. The defualt timeout time is 1000ms. It is recommended to test code with samller timeout value
			and increase the timeout value for productive use.

- cli_writeln(t):	Stens text t to the Thread CLI (command line interpreter, e.g. accessable through USB) console.
- cli_put(cmd, hnd):	Sends the command cmd to the Thread CLI interpreter. See Thread CLI documentation for information about
			available commands. The H++ handler function is called if the CLI interpreter creates output to the CLI
			console. Inside the hander function the variable cli_output contains the text sent to the CLI. It is important 
			to note that such console output may cause multiple calls of the handler in reaction ot asynchronous output.
- cli_add_cmd(n):	Adds a new custom CLI command to the Thread CLI interpreter.
			If such a custom CLI command is used at the CLI console, the H++ handler function cli_parse is called.
			Inside the handler funtion the variable 'command' contains the name of the custom command, the variable 'argc'
			contains the numbner of parameters passed to the custom command and the variables 'paramX' contain the 
			parameters.    

- ip_addr_add(a):	Adds IPv6 address a to the host. Format of a: xxxx:xxxx::xxxx. Both unicast and multicast is supported. 	
- ip_addr_eid():	Returns the EID address of the host.
- ip_addr_gua(n):	Returns the n'th global unique address (GUA) of the host. Returns 'false' if no n'th GUA address exists.
- ip_addr_ula(n):	Returns the n'th unique local address (ULA) of the host. Returns 'false' if no n'th ULA address exists.

- coap_add_resource(uri_p, info, hnd): Adds a CoAP resource with the URI path uri_p to the host. For service discovery, with 
			/.well-known/core, additional information (e.g. rt=xyz for resource type) can be added. See respective 
			RFCs for details.
			The H++ handler funtion hnd will be called upon receiving a request on the URI p_uri. Inside the handle
			the variable 'payload' contains the payload of the response and the variable 'content_format' contains the
			content format (numerical) of the response.
- coaps_add_resource(uri_p, info, hnd):	Same as above but for secure connection. Security credentials must have been set before 
			using coaps_set_psk(..) or coaps_set_cert(..). 					 

PUT:
- coap_put(a, uri_p, p [, c, hnd]): Sends a CoAP PUT request to the host with the IPv6 address a using the URI path option uri_p and
			includes the payload p. The message is sent confirmable if c is true. The H++ handler function hnd will
			be called upon receiving a response on the request (not usually needed for PUT). Inside the handler the
			variable 'payload' contains the payload of the response and the variable 'content_format' contains the
			content format (numerical) of the response.
- coaps_put(id, uri_p, p [, c, hnd]): Same as above but for secure connection. The parameter 'id' is a DTLS connection identifier
			from coaps_connect(..).  

GET:    
- coap_get(a, uri_p, hnd, c): Sends a CoAP GET request to the host with the IPv6 address a using the URI path option uri_p.
			The H++ handler funtion hnd will be called upon receiving a response on the request. Inside the handler the
			variable 'payload' contains the payload of the response and the variable 'content_format' contains the
			content format (numerical) of the response. The message is sent confirmable if c is true. 
- coaps_get(id, uri_p, hnd, c): Same as above but for secure connection. The parameter 'id' is a DTLS connection identifier
			from coaps_connect(..).

POST:
- coap_post(a, uri_p, p, hnd, c): Sends a CoAP POST request to the host with the IPv6 address a using the URI path option uri_p and 
			includes the payload p. The H++ handler funtion hnd will be called upon receiving a response on the request. Inside
			the handler the variable 'payload' contains the payload of the response and the variable 'content_format'
			contains the content format (numerical) of the response. The message is sent confirmable if c is true. 
- coaps_post(id, uri_p, p, hnd, c): Same as above but for secure connection. The parameter 'id' is a DTLS connection identifier
			from coaps_connect(..).

 

	
>> The following functions shall only be used inside a CoAP request handler set with coap_add_resource(..):

- coap_is_get():	Determines if the present CoAP request is of type GET. Responds true or false.
- coap_is_put():	Determines if the present CoAP request is of type PUT. Responds true or false.
- coap_is_post():	Determines if the present CoAP request is of type POST. Responds true or false.
- coap_is_delete():	Determines if the present CoAP request is of type DELETE. Responds true or false.
- coap_is_multicast():	Determines if the present CoAP request is a mulitcast request. Responds true or false. 
 
- coap_respond(d[,f]):	Responds with the content d and optional content format f to a coap request. 
- coap_context():	Returns context information of the present request, such that it can be passed to coap_respond_to(..)
			for a later response out of the present CoAP request handler.       			 

>> Response outside a CoAP request handler. E.g. a timer handler: 

- coap_respond_to(c, d[,f]):  Responds to a given coap context c with the content d and optional content format (numerical) f to 
			      a coap request.


>> Secure connection - (additional application layer encryption with CoAPs):

- coaps_hide_vars(b):	Determines if H++ variables are visible as coap resources (/var/x) with b = true or false.
			The global variable Var_Hide_PW sets the password for unlocking the H++ variable access using the
			none discoverable resource '/var_hide'. The default password is 'openvar'. (if Var_Hide_PW does not exist)
			   
- coaps_set_psk(IdentityName, Password, connected_handler): Sets the security credentials for the next DTLS connection.
			The H++ handler function connected_handler (if any) is called after a connection is established or closed.
			Inside the connection hander, the variable 'connected' indicates if the connection was established (true) 
			or closed (false). 
 
- coaps_set_cert(X509Cert, PrivateKey[,  RootCert, connected_handler]): As above but with certificate based security credentials.
			If a RootCert is provided (different from empty string), the certificate of the peer is verified against
			the root certificate.  

- coaps_connect(a, hnd): Opens a DTLS (e.g. CoAPs) connection to a peer host with the IPv6 address a. The H++ handler function 
			hnd (if any) is called after a connection is established or closed. Inside the connection hander, the
			variable 'connected' indicates if the connection was established (true) or closed (false).
			Returns and identifier for the DTLS connection which can subsequently be passed instead of a peer 
			IPv6 address to coap_get(..), coap_put(..), ...   
			At present only one DTLS connection at a time is supported!

- coaps_disconnect(a):	Closes a DTLS connection. At present only one DTLS connection at a time is supported. The parameter 'a'
			can be omitted.

- coaps_stop(a):	Stops the DTLS module and deletes the security credentials. At present only one DTLS connection at a time
			is supported. The parameter 'a' can be omitted. 


8) nRF52840 hardware driver bindings

Upon power up or reset the firmware starts the H++ function 'startup' with no parameters after downloading code variables
from flash memory. Other global variables are not loaded from flash automatically. Use flash_restore() to restore global 
variables during startup. 

nRF52840 hardware related bindings are functions added to the core H++ scripting language. See nRF52840 specification
for details on the hardware capabilities.

- io_pin(port, pin_nr):	Returns an i/o pin identifier for a given pin number 'pin_nr' in a given 'port'.
- io_set(p, val):	Sets the status of pin 'p' to val (1 or 0) or high and low.
- io_get(p):		Reads the logic value of a given io pin 'p'.
- io_cfg_output(p[,h]): Configures the pin 'p' as output. If 'h' is true, the output is set to high maimum current,
 			otherwise normal maximum current.
- io_cfg_input(p[,pl]): Configures the pin 'p' as input. If 'pl' is 1 the input has a pull up source activated.
			If 'pl' is 0 the input has a pull down sink activated. If the parameter 'pl' is not provided,
			no pull up or down is activated.

- io_pwm_start(count_stop, seq, val_repeat, seq_repeat, pin1 [, pin2, pin3, pin4]): Activates a PWM unit with 
			PWM frequency 16MHz / dount_stop. Duty cycle values are provided in the byte string 'seq' in
			groups of one, two or four subsequent duty cycle values in uint16 format, depending on how
			many pins are active. Duty cycle values range from 0 to count_stop (100%).
			Every PWM setting is repeated 'val_repeat' times until the next values are taken. If 'val_repeat'
			is null then the PMW values are static. The whole sequence is repeated 'seq_repeat' times.
			If 'seq_repeat' is >0 then the length of the sequence must be minimum two value groups.
			The parameters pin1, .. pin4 are the pin identifiers of the PWM output pins. Those pins must
			be configures as output pins before. One, two or four output pins can be configured. Groups
			of three pwm outputs cannot be configured due to a hardware restriction; however the physical
			output of a fourth pin can be deactivated by providing -1 as the pin identifier.
- io_pwm_stop():	Stops the pwm controller.
- io_pwm_restart():	Restarts the pwm controller with settings provided before.

- io_cfg_btn(p [, lohi, pull]): Configures the pin 'p' as input for a button. The default pin is port 1 pin_nr 6. A button
			press event is reported 100ms after the first electrical signal. This is to prevent repeated 
			events due to bounce of button contacts. The parameter 'lohi' controls if the button is active
  			low (0) or high (1). The parameter 'pull' controls if a pull up (1) or down (0) source/sink is
			activated. If no 'pull' parameter is provide, no pull up or down is activ.    
			The H++ handler function 'btn_push' is called after a the button was pressed with 100 ms delay.
			Inside the handler function the variable 'pin' indicates the pin identifier of the button pushed.
			The H++ handler function 'btn_release' is called after a the button was released.
			Inside the handler function the variable 'pin' indicates the pin identifier of the button released
			and the variable 'hold_time' indicates the hold time before release.

- flash_save():		Saves all global (capitalized) variables to the flash memory starting at 0xb0000 or alternating
			0xb8000 (max. 32kb).
- flash_save_code():	Saves all H++ code (lower case) variables to the flash memory starting at 0xba0000 (max. 64kb).
- flash_restore():	Restores all global variables stored in the flash memory with flash_save().
- flash_delete():	Deletes global variables stored in the flash memory.
- flash_delete_code():	Deletes code variables stored in the flash memory.

- timer_start(id, t, hdn): 	Starts a timer with the given id calling the H++ handler 'hdn' every 't' milliseconds.
- timer_once(id, t, hdn): 	Starts a timer with the given id calling the H++ handler 'hdn' once fter 't' milliseconds.
				Up to 8 timers (in total) are supported id = 0 .. 7.
- timer_stop(id):		Stops the timer with a given id.	


    

 






