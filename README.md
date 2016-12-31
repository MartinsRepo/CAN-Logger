
# CAN-Logger
CAN Logging on SD card

Often, engineers face the problem of logging CAN bus communication via Vector CANOE or DSPACE Deskcontrol or similar tools. For a better interpretation of the collected data MatLab my be the tool of choice oe Excel. So it takes some time to export the data from the one tool and import them into the other.
For this usecase a cheap environment has been build. Based on a Arduino Mega platform with enough internal memory of 8k, the device is connected with a MCP2515 CAN card, a SD card reader, a real time clock RTC and in future with a bluetooth adapter for starting and stopping the measurement.

The connections are shown in <logger.png>.

Usage:
The software is reading the file config.txt (which has to be copied on a formatted SD card (hint: if SD card formatting is not working and the sd card is not recognized, you can use the SD card formatting example from the library. Also a 128Gb card is working.)

config.txt example:

The VectorDBC file is a text file. So you can copy the messages to be logged into config.txt. Take aware, that: 
- not more then 40 messages can be logged
- consecutive bytes may not more the 4 bytes (long in Arduino has only 4 bytes). 

+++Configurator
+
+ Martin Hummel 2017
+
+ Usage:
+ Copy the messages and signals to be logged from the vector DBC file 
+ between #msg and #endmsg.
+ Example:
+ #msg
+ BO_ 800 motor .... ==> BO_ tag for a message; 800 is the identifier
+                        in dez.; motor the message name
+  SG_ drehzahl : 48|8@1+ (25,0) [0|5000] "1/min" Vector__XXX ==>
+  SG_ tag for a signal; the follows the signal name, 48 is the bit starting position
+  and after | 8 is the bit length; @1+ points to the INTEL format, @1- to the Motorola format;
+  (25,0) with 25 as a muliplicator factor for the phys. value and 0 is the offest value
+  The value between [] ist the phys. range. Then follows the unit
+  
+  calculation: 8bit = 0..255 max; max phys. range: 6350
+              multiplicator = 635 /255 =25  
+
+++++ Logging Botschaften
   
#msg
BO_ 900 Motor: 8 Diesel
 SG_ M_Sig19: 56|8@1+ (0.4,0) [0|101.6] "%" Vector__XXX
 SG_ M_Sig18: 56|8@1+ (25,0) [0|6350] "%/s" Vector__XXX
 SG_ M_Sig17 : 48|8@1+ (25,0) [0|6350] "1/min" Vector__XXX
 SG_ M_Sig16 : 40|8@1+ (0.39,0) [0|100] "%"  Vector__XXX
 SG_ M_Sig15 : 39|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig14 : 38|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig13 : 37|1@1+ (1,0) [0|0] "" Vector__XXX
 SG_ M_Sig12 : 36|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig11 : 24|12@1+ (0.1,0) [0|409.5] "-"   Vector__XXX
 SG_ M_Sig10 : 16|8@1+ (0.4,0) [0|101.6] "%"   Vector__XXX
 SG_ M_Sig9 : 8|8@1+ (0.75,-48) [-48|142.5] "°C" Vector__XXX
 SG_ M_Sig8 : 7|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig7 : 6|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig6 : 5|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig5 : 4|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig4 : 3|1@1+ (1,0) [0|1] ""  Vector__XXX
 SG_ M_Sig3 : 2|1@1+ (1,0) [0|0] "" Vector__XXX
 SG_ M_Sig2 : 1|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ M_Sig1 : 0|1@1+ (1,0) [0|1] "" Vector__XXX
BO_ 1500 Lenkwinkelsensor_Init: 4 XXX
 SG_ L_Sig2 : 8|24@1+ (1,0) [0|16777215] ""  steering angle sensor
 SG_ L_Sig1 : 0|8@1+ (1,0) [0|255] ""   steering angle sensor
#endmsg
