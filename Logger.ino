/*******************************************************************************************
 *                                CAN - DATA - LOGGER
 *                  
 *  autor:    Martin Hummel, 01/2017
 *  email:    jupp@linuxmail.org
 *  licence:  GPL 3.0
 *  
 *  function: Logging of CAN traffic in a CSV format on SD card
 *  
 *  usage:    define the set of messages / signals to be logged in config.txt on the SD card
 *            in this case, copy messages / signals of the VECTOR DBC file (text only file)
 *            start of measurement via bluetooth
 */
 
#include <SPI.h>
#include "SdFat.h"            // SD card
#include <mcp_can.h>          // MCP CAN
#include <mcp_can_dfs.h>
#include <HC05.h>             // HC-05 Bluetooth
#include <string.h>

#define DEBUG_LOGGER

// SD configuration
#define SD_CHIPSEL 4
const uint8_t SD_CHIP_SELECT = SD_CHIPSEL;
const int8_t DISABLE_CHIP_SELECT = -1;
SdFat sd;
File logging[10];  
String kline;

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
//char msgString[128];                        // Array to store serial string

// CAN configuration
#define CAN0_INT 2                            // Set INT to pin 2
#define CAN_CHIPSEL 10
MCP_CAN CAN0(CAN_CHIPSEL);                    // Set CS to pin 10
unsigned long previousMillis=0;

// control flags
bool can_active=false;
bool sd_active=false;
bool header[10]={false,false,false,false,false,false,false,false,false,false};

// config.txt 
File file;
int lsize=0;
int qsize=0;
int msgtot=0;

// bluetooth
bool meastoggleon=false;
bool meastoggleoff=false;

char serbuf[3];
byte sercnt=0;

class MESSAGE {
  public:
    MESSAGE(){};
    ~MESSAGE(){};
    
    String msgname;
    int  id;
    String signame;
    int  start;
    int  siglen;
    String sigformat;
    double  factor;
    double  offset;
}; 

MESSAGE *canmsg[40]; 

class WHICHMSG {
  public:
    WHICHMSG(){};
    ~WHICHMSG(){};
    
    int  id;
    int  start;
    int  counts;
}; 

WHICHMSG *msgtab[10]; 

class ArrayList 
{
  public:
   ArrayList(String); //ArrayStringList(void);
  ~ArrayList(void);

  void display_string_list();
  void add_string_item(char* item);
  void set_string_item(char* item, int index);
  void remove_selected_item(int index);
  void empty_list();
  void set_stringlist(char** stringlist);
  char** get_stringlist();
  String get_stringlist_item(int index);
  int get_size();

private:
  char** stringlist;
  int size;  
};

char* convert(String in)
{
  char *out = new char[in.length()+1];
  in.toCharArray(out, in.length()+1);
  return out;
}

ArrayList::ArrayList(String init)
{
  char* buf;
  stringlist = (char**)malloc(10*sizeof(char*));
  buf=convert(init);
  stringlist[0] = buf;
  this->size = 1; 
}

ArrayList::~ArrayList(void)
{
}

void ArrayList::add_string_item(char* item)
{
  char **newlist = (char**)malloc((size+1)*sizeof(char*));
  for(int i=0; i<size; i++){
    newlist[i] = stringlist[i];
  }
   newlist[size] = item;
   stringlist = newlist;
   size = size + 1;
}

void ArrayList::set_string_item(char* item, int index)
{
  stringlist[index] = item;
}

void ArrayList::remove_selected_item(int index)
{
  char **newlist = (char**)malloc((size-1)*sizeof(char*));
  //From Begining
  for(int i=0; i<index; i++){
    newlist[i] = stringlist[i]; 
  }

  //From next Index  

  for(int i=index; i<=size-1; i++){
    newlist[i] = stringlist[i+1];
  }

  //free(matrix);

  stringlist = newlist;
  size = size - 1;
}

void ArrayList::empty_list()
{
   String str;
   char *buf;
   
   size = 1;
   char **newlist = (char**)malloc((size)*sizeof(char*));   
   stringlist = newlist;
   str="EMPTY";
   buf=convert(str);
   stringlist[0] = buf;
}

void ArrayList::display_string_list()
{
  for(int i=0; i<size; i++){
    Serial.println(stringlist[i]);
   }
}

String ArrayList::get_stringlist_item(int index)
{
  return stringlist[index];
}

char** ArrayList::get_stringlist()
{
  return this->stringlist;
}

void ArrayList::set_stringlist(char** stringlist)
{
  this->stringlist = stringlist;
}

int ArrayList::get_size()
{
  return this->size;
}

bool readLine(File &f, char* line, size_t maxLen) 
{
  for (size_t n = 0; n < maxLen; n++) {
    int c = f.read();
    if ( c < 0 && n == 0) return false; // EOF
    if (c < 0 || c == '\n') {
      line[n] = 0;
      return true;
    } 
    line[n] = c;
  }
  return false; // line to long
}

int delimiterpos(char* line, char del, size_t cnt)
{
   int i=0;
   int counter=0;
   while(line[i]) {
      if(i>79) break;
      if(line[i]==del) counter++;
      if(counter==cnt) break;
      i++;
   }
   return i;
}

// Check bluetooth activation of the measurement
// bluetooth command string: on of
bool startmeas(void) 
{
    bool measstate=false;
    char nextChar;
    String btcmd; //Befehlsbuffer

    if(Serial1.available()>0) {
      delay(3);
      while (Serial1.available()>0){
         nextChar = Serial1.read();
         btcmd += nextChar;
         if(nextChar=='\n') break;
      }
    }
    
    if(btcmd.substring(btcmd.indexOf('o'),2)=="on") {
         measstate=true;
         meastoggleon=false;
    }
    if(btcmd.substring(btcmd.indexOf('o'),2)=="of") {
         measstate=false;
         meastoggleoff=false;
    }
    //if(btcmd!=""){Serial.print(btcmd.substring(btcmd.indexOf('o'),2));Serial.println();}
    btcmd="";
    
  return measstate;
}


void setup() 
{
  char line[80];
  String strtmp;
  String msg, id, start, signame, siglen, format, factor, offset/*, smin, smax, unit*/; 
  int tokenstart;
  char *buf;
  
  ArrayList *list = new ArrayList("CAN Signal List");
  
#ifdef DEBUG_LOGGER  
  // Open serial communications and wait for port to open:
   Serial.begin(115200);
  
   while (!Serial) {
     ; // wait for serial port to connect. Needed for native USB port only
  }
#endif

   // ===
   // === Bluetooth initialisation
   // ===
   Serial1.begin(9600);
   
   // ===
   // === SD card initialisation
   // ===

   if (!sd.begin(SD_CHIPSEL)) {                          // check for inserted SD card
#ifdef DEBUG_LOGGER
    Serial.println("initialization failed!");
#endif
     sd_active=false;
   } else {
#ifdef DEBUG_LOGGER
    Serial.println("initialization done.");
#endif
    if (sd.exists("config.txt")) {                      // check for file config.txt on SD card
#ifdef DEBUG_LOGGER
      Serial.println("config.txt exists.");
#endif
      // Create the file.
      file = sd.open("config.txt", FILE_READ);          // check for file config.txt can be opened
      if (!file) {
#ifdef DEBUG_LOGGER
        Serial.println("open failed");
#endif
        sd_active=false;
      } else {
        // Rewind the file for read.
        file.seek(0);
        sd_active=true;
      }
    } // if (sd.exists("config.txt")) 
   } //  if (!sd.begin(SD_CHIPSEL))

   // ===
   // === CAN initialisation
   // ===
   
   if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
#ifdef DEBUG_LOGGER
     Serial.println("MCP2515 Initialized Successfully!");
#endif
     CAN0.setMode(MCP_NORMAL);                           // Set operation mode to normal so the MCP2515 sends acks to received data.
     pinMode(CAN0_INT, INPUT);                           // Configuring pin for /INT input
#ifdef DEBUG_LOGGER
     Serial.println("CAN ready...");
#endif
     can_active=true;
   } else{
#ifdef DEBUG_LOGGER
     Serial.println("Error Initializing MCP2515...");
#endif
     can_active=false;
   } // if(CAN0.begin
  
   // ===
   // === Reading of config.txt
   // ===
   
   if(can_active && sd_active) {                         // process, if prerequisites are done
 
      char ch;
      int cnt=0; 

      if(lsize>39) lsize=39;                             // size over array is limited to 40

      while(ch = file.read()!='#') {                     // overread comments and goto #msg
        while(ch=file.read()!='\n') {                    // goto next line
        }
      }

      while(ch=file.read()!='\n') {                      // goto next line
      }

      while (line[0]!='#'){                              // read CAN data
        memset(line, 0, sizeof(line));  // clear line
        if (!readLine(file, line, sizeof(line))) {
           break;  // EOF or too long
        } else {
                              
          strtmp="";
          for(int i=0;i<sizeof(line);i++) strtmp+=line[i]; // convert to string
      
          tokenstart = strtmp.indexOf('B');              // position of token BO_ message indication

          if ((strtmp.substring(tokenstart,tokenstart+3) == "BO_") && (tokenstart > -1)) {    // get BO_ message information
              // get identifier
              cnt=delimiterpos(line, ' ', 2);
              id=strtmp.substring(tokenstart+4,cnt);
              
             
              // Get message
              tokenstart=cnt+1;
              cnt=delimiterpos(line, ':', 1);
              msg=strtmp.substring(tokenstart,cnt);
           } // if ((strtmp.substring

           // analyse signal block
           tokenstart = strtmp.indexOf('S');
          
           if ((strtmp.substring(tokenstart,tokenstart+3) == "SG_") && (tokenstart > -1)) {   // get SG_ signal information

              bool sspace=false;
              // get signal name
              cnt=delimiterpos(line, ':', 1);
              //canmsg->signame=strtmp.substring(tokenstart+4,cnt);
              signame=strtmp.substring(tokenstart+4,cnt-1);
              int space = signame.indexOf(' ');
            
              // get signal start position substring
              if(space>0) {
                cnt=delimiterpos(line, ' ', 4); 
                sspace=true;
              } else {
                cnt=delimiterpos(line, ' ', 3); 
                sspace=false;
              }
                
              tokenstart=tokenstart+cnt+1;
              if(sspace)
                 cnt=delimiterpos(line, ' ', 6);
              else
                 cnt=delimiterpos(line, ' ', 5);
             
              String subtmp=strtmp.substring(tokenstart+1,cnt);
              
              // get signal start position
              start=subtmp.substring(0,subtmp.indexOf('|'));

              // get signal length
              siglen=subtmp.substring(subtmp.indexOf('|')+1,subtmp.indexOf('@'));
              
               // get signal format: Intel 1+, Motorola 1-
              format=subtmp.substring(subtmp.indexOf('@')+1,subtmp.indexOf('@')+3);
             
              // get signal factor / offset substring
              tokenstart=strtmp.indexOf('@')+5;
              if(sspace)
                cnt=delimiterpos(line, ' ', 7)-1;
              else
                cnt=delimiterpos(line, ' ', 6)-1;
                
              subtmp=strtmp.substring(tokenstart,cnt);
            
              // get signal scaling factor
              factor=subtmp.substring(0,subtmp.indexOf(','));
    
              // get signal offset
              offset=subtmp.substring(subtmp.indexOf(',')+1,subtmp.indexOf(' '));
                      
              // update list
              strtmp=id+","+msg+","+signame+","+start+","+siglen+","+format+","+factor+","+offset;
              
              buf=convert(strtmp);
              list->add_string_item(buf);
              
           } // if ((strtmp.substring

        } // if (!readLine(file, line
       
      } // while (line[0]
 
      file.close();
      
#ifdef DEBUG_LOGGER
      list->display_string_list();
#endif
    lsize=list->get_size();
    //if(lsize>40) lsize=40;                                        // limitate to 40 signals
   
    // extract CAN information from list array
    for(int i=1;i<lsize;i++) {
      canmsg[i] = new MESSAGE();
      strtmp=list->get_stringlist_item(i);  // get line
      strtmp.toCharArray(line, strtmp.length()+1);
      cnt=delimiterpos(line, ',', 1);
      // id
      tokenstart=0;
      id=strtmp.substring(tokenstart,cnt);
      canmsg[i]->id=id.toInt();
      // msg name
      tokenstart=cnt+1;
      cnt=delimiterpos(line, ',', 2);
      msg=strtmp.substring(tokenstart,cnt);
      canmsg[i]->msgname=msg;
      // signal name
      tokenstart=cnt+1;
      cnt=delimiterpos(line, ',', 3);
      signame=strtmp.substring(tokenstart,cnt);
      canmsg[i]->signame=signame;
      // start position
      tokenstart=cnt+1;
      cnt=delimiterpos(line, ',', 4);
      start=strtmp.substring(tokenstart,cnt);
      canmsg[i]->start=start.toInt();
      // signal length
      tokenstart=cnt+1;
      cnt=delimiterpos(line, ',', 5);
      siglen=strtmp.substring(tokenstart,cnt);
      canmsg[i]->siglen=siglen.toInt();
      // sig format I=Intel, M=Motorola
      tokenstart=cnt+1;
      cnt=delimiterpos(line, ',', 6);
      format=strtmp.substring(tokenstart,cnt);
      if(format=="1-")
        canmsg[i]->sigformat="M";
      else
        canmsg[i]->sigformat="I";
      // factor
      tokenstart=cnt+1;
      cnt=delimiterpos(line, ',', 7);
      factor=strtmp.substring(tokenstart,cnt);
      canmsg[i]->factor=factor.toFloat(); 
      // factor
      tokenstart=cnt+1;
      cnt=delimiterpos(line, ',', 8);
      offset=strtmp.substring(tokenstart,cnt);
      canmsg[i]->offset=offset.toFloat(); 
    } 
  } // if(can_active && sd_active)
  delete list;
  lsize--;

  // message table: id, first event and how often
  int tmpid=-1, ipos=0, tmpnb=0, tmpst=-1;
  int idold=canmsg[1]->id;
  
  for(int i=1;i<lsize+1;i++) {
    
      msgtab[ipos]=new WHICHMSG();
      
      if(ipos>9) break;
      if(canmsg[i]->id==idold) {
        tmpst=i;
      } 
      while(canmsg[i]->id==idold) {
         tmpid=canmsg[i]->id;
         tmpnb++;
         if(i++>lsize) break;
      }
     
      if(tmpid!=canmsg[i]->id) {
         idold=canmsg[i]->id;
         //fentry=false;
         msgtab[ipos]->id=tmpid;
         msgtab[ipos]->start=tmpst;
         msgtab[ipos]->counts=tmpnb;
        
         tmpnb=0;
         i--;
         
         ipos++;
      } 
   }
   qsize=ipos--;
  
   // open file for log writing
   String fname;
   int sigpos=0,counts=0;
   for(int i=0;i<qsize;i++){
      sigpos=msgtab[i]->start;
      counts=msgtab[i]->counts;
      
      if(header[i]==false) {
          logging[i].seek(0);
          fname=String(msgtab[i]->id)+".txt";
          logging[i] = sd.open(fname, FILE_WRITE);
          
          // write signal names
          for(int j=sigpos;j<(sigpos+counts);j++) {
              if(j==sigpos) {
                String ss="Delta Time,"+canmsg[j]->signame+",";
                logging[i].print(ss);
              } else {
                logging[i].print(canmsg[j]->signame);
                logging[i].print(",");  
              }
          }
          logging[i].println(); 
         // logging[i].flush(); 
          header[i]=true;
        }
   }

   msgtot=0;
   kline="";
   previousMillis=millis(); 
}

void loop() 
{ 
  int startpos, siglen;
  int bnumber=0; int bpos=0, mpos=0;
  double physval=0;
  bool sigbyteplus=false;
  char masking[32];
  unsigned long currentMillis;
  int interval[10]={0,0,0,0,0,0,0,0,0,0};

  if (can_active && sd_active) {
   
        if(startmeas()) {
#ifdef DEBUG_LOGGER
          if(!meastoggleon){
             Serial.println("Measurement started.");
             meastoggleon=true;
           }
#endif
          if(!digitalRead(CAN0_INT)) {                        // If CAN0_INT pin is low, read receive buffer
          
            CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
      
            // check relevant message id
            int qcount=0, starting=0, ending=0;
           
            while(qcount<qsize) {
                starting  = msgtab[qcount]->start;
                ending = msgtab[qcount]->counts;
                qcount++;
               
                for(int i=starting; i<starting+ending;i++) {
                   if(rxId==canmsg[i]->id) {
      
                     // find position in msgtab
                     for(int i=0;i<qsize;i++) {
                         if(msgtab[i]->id==rxId) mpos=i;
                     }
      
                    if(i==starting) {
                       // calculating dela time of messages
                       currentMillis = millis();
                       interval[mpos]=(int)(currentMillis - previousMillis);
                       previousMillis = currentMillis;
                    }
      
                     // find start position
                     startpos=canmsg[i]->start;
                     siglen=canmsg[i]->siglen;
                     
                     // byte number
                     for(int j=1;j<9;j++) {
                         bnumber=j;
                         if( ((startpos)/(j*8))==0) break;
                     }
                     bnumber--;
                     if(bnumber<0) bnumber=0;
                     bpos=startpos%8;
          
                     if((bpos+canmsg[i]->siglen)>8){       // check. whether signal + offset more then one byte
                        sigbyteplus=true;
                     } else {
                        sigbyteplus=false;
                     }
          
                     if(!sigbyteplus) {                    // signal matches one byte - filter asignal
                         
                        int tmpval=rxBuf[bnumber];
                        int tmpvalval=0;
                       
                        // calculate masking vector
                        for(int i=0;i<8;i++) masking[i]='0';
                        for(int i=0;i<8;i++){
                          if((i>=bpos)&&(i<bpos+siglen)) masking[7-i]='1'; else masking[7-i]='0';
                        }
          
                        // extract value
                        for(int i=0;i<8;i++) {
                          if(masking[7-i]=='1') {
                            if ((tmpval >> i) & 1) {
                               tmpvalval=tmpvalval|(1<<i);  
                            }
                          }
                       }
                     
                       physval=(tmpvalval>>bpos)*canmsg[i]->factor+canmsg[i]->offset;
                       kline=kline+String(physval)+",";
               
                     } else {  // concatenated signals over more bytes
                        
                        for(int i=0;i<32;i++) masking[i]='0';
                        int total=bpos+siglen;
                        if(total>32) total=32;
                        
                        // calculate masking vector
                        for(int i=0;i<=total;i++){
                          if((i>bpos)&&(i<=total)) masking[total-i]='1'; else masking[total-i]='0';
                        }
                        
                        // get bytes
                        int nbbytes=0, j=1, rest=0;
                       
                        while(1){
                            rest=total-j*8;
                            if(rest>1) nbbytes++; else break;
                            j++;
                        }
                        unsigned long tmplong=0UL;  //==> supports only 4 byte!!!
                        unsigned long tmpvalval=0UL;
                        
                        if(nbbytes<4) {
                           
                            for(int i=nbbytes;i>=0;i--) {
                                tmplong=tmplong +(unsigned long)(rxBuf[bnumber+i]<<(8*i));
                            }
         
                            // extract value
                            for(int i=0;i<total;i++) {
                              if(masking[i]=='1') {
                                if ((tmplong >> i) & 1) {
                                   tmpvalval=tmpvalval|(1UL<<i); 
                                } 
                              }
                            }
                    
                           physval=(tmpvalval>>bpos)*canmsg[i]->factor+canmsg[i]->offset;
                           kline=kline+String(physval)+",";
                     
                     } //if(nbbytes<4)
                      
                }// else
          
              } // if(rxId==canmsg[i]->id)
               
             }// for(int i=starting; i<starting+ending;i++)
      
             if(qcount==qsize) {
                kline=String(interval[mpos])+","+kline;
                
#ifdef DEBUG_LOGGER
               Serial.println(kline);
#endif
                logging[mpos].println(kline); 
                logging[mpos].flush();
                kline="";
             }
            } // while
           
           } //if(!digitalRead(CAN0_INT))
        } else {
#ifdef DEBUG_LOGGER
         if(!meastoggleoff) {
            Serial.println("Measurement stopped.");
            meastoggleoff=true;
         } 
#endif
        }  // bluetooth activation
      } //  if (can_active && sd_active)

}// loop
