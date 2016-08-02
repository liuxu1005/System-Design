
//
//  fileclient.cpp
//  filecopy homework Comp 150IDS
//  Yu Lei & Xu Liu
//
////////////////////////////////////////////////////

#include "c150nastyfile.h"        // for c150nastyfile & framework
#include "c150grading.h"
#include "message.h"
#include "c150nastydgmsocket.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <cstring>                // for errno string formatting
#include <cerrno>
#include <cstring>               // for strerro
#include <iostream>               // for cout
#include <fstream>                // for input files

#define FILEID 1000000

int sent;
int resent;
int receive;
int ack;
int filecontent;
int empty;

//
// Always use namespace C150NETWORK with COMP 150 IDS framework!
//
using namespace C150NETWORK;

void calculateSHA(string sourceName, int filenastiness, unsigned char *hashcode );
bool isFile(string fname);
void checkDirectory(char *dirname);
void checkCommandLine(int argc, char *argv[]);
string makeFileName(string dir, string name);
  void buildMsgOut(struct Message *MsgOut, string data1, string data2, uint64_t id);
enum Type checkfile(string dir, string file, char *sbuffer, char *rbuffer, C150NastyDgmSocket *sock, int filenastiness);
enum Type sendpacket(char *sbuffer, char *rbuffer, C150NastyDgmSocket *sock);
enum Type sendfile(string dir, string file, char *sbuffer, char *rbuffer, C150NastyDgmSocket *sock, uint64_t id, int filenastiness);
const int SENDTIMES = 100;

// ------------------------------------------------------
//                   Main program
// ------------------------------------------------------

int
main(int argc, char *argv[]) {

  int filenastiness;
  int networknastiness;
  DIR *SRC;                   // Unix descriptor for open directory
  struct dirent *sourceFile;  // Directory entry for source file

  //
  //  DO THIS FIRST OR YOUR ASSIGNMENT WON'T BE GRADED!
  //
  GRADEME(argc, argv);

  //
  // Check command line and parse arguments
  //
  checkCommandLine(argc, argv);

  //
  // Parse arguments
  //
  filenastiness = atoi(argv[3]);   // convert command line string to integer
  networknastiness = atoi(argv[2]);
  string sourceDir = argv[4];

  //
  // Make sure source and target dirs exist
  //
  checkDirectory(argv[4]);

  //
  // Open the source directory
  //
  SRC = opendir(argv[4]);
  if (SRC == NULL) {
    fprintf(stderr,"Error opening source directory %s\n", argv[4]);
    exit(8);
  }

  //
  // open a socket and set timeout
  //
  C150NastyDgmSocket *sock = new C150NastyDgmSocket(networknastiness);
  sock -> setServerName(argv[1]);
  sock -> turnOnTimeouts(50);

  // SRC dir remains open for loop below

  //
  //  Loop copying the files
  //
  uint64_t id = 0;
  char *rbuffer = (char *)malloc(512);
  char *sbuffer = (char *)malloc(512);
  *GRADING << "Sending files in directory " << sourceDir<< endl;
  cout<< "Sending files in directory " << sourceDir<< endl;
  
  //statics variables
  int filenumber = 0;
  int retry = 0;
  int failfile = 0;
  while ((sourceFile = readdir(SRC)) != NULL) {
      // skip the . and .. names
      string fileName = sourceFile->d_name;
      if ((strcmp(fileName.c_str(), ".") == 0) ||
              (strcmp(fileName.c_str(), "..")  == 0 ))
          continue;          // never copy . or ..

    filenumber++;  

    int sendtimes = 0;
    enum Type sendStatus;
    enum Type checkstatus;
    id += FILEID;
 
    while(sendtimes < SENDTIMES) {
        retry++;
        *GRADING << "Sending file <" << fileName<<"> on attempt "<<sendtimes<< endl;
        cout<< "Sending file <" << fileName<<"> on attempt "<<sendtimes<< endl;
        sendStatus = sendfile(sourceDir, fileName, sbuffer, rbuffer, sock, id, filenastiness);

        if(sendStatus == FAILURE) {
           
          sendtimes++;
          continue;
        }
    
        // request for check
     
        checkstatus = checkfile(sourceDir, fileName, sbuffer, rbuffer, sock, filenastiness);
        if (checkstatus == FAILURE) {
            *GRADING << "Check file <" << fileName<<"> failed on attempt "<<sendtimes<< endl;
            cout<< "Check files <" << fileName<<"> failed on attempt "<<sendtimes<< endl;
            sendtimes++;
            continue;
        }
       
        break;
    }
    if(sendtimes >= SENDTIMES) {
        failfile++;
        *GRADING << "Final failure on sending file <" << fileName<<"> on attempt "<<sendtimes<< endl;
        cout<< "Final failure on sending file <" << fileName<<"> on attempt "<<sendtimes<< endl;
    } else {
        *GRADING << "Successfully send file <" << fileName<<"> on attempt "<<sendtimes<< endl;
        cout<< "Successfully send file <" << fileName<<"> on attempt "<<sendtimes<< endl;
        
        
    }
  }
  *GRADING<<"Totally send "<<filenumber<<" files;"<<" failed "<<failfile<<" files;"<<" retry "<<retry<<" times."<<endl;
  cout<<"Totally send "<<filenumber<<" files;"<<" failed "<<failfile<<" files;"<<" retry "<<retry<<" times."<<endl;
}

enum Type sendfile(string dir, string file, char *sbuffer, char *rbuffer, C150NastyDgmSocket *sock, uint64_t id, int filenastiness) {
  void *fopenretval;
  size_t len;
  char *buffer;
  struct stat statbuf;
  size_t sourceSize;
  string sourceName = makeFileName(dir, file);

  //
  // make sure the file we're copying is not a directory
  //
  if (!isFile(sourceName)) {
      cerr << "Input file " << sourceName << " is a directory or other non-regular file. Skipping" << endl;
      exit(1);
  }

  //
  // Read info about input file
  //
  if (lstat(sourceName.c_str(), &statbuf) != 0) {
      fprintf(stderr,"copyFile: Error stating supplied source file %s\n", sourceName.c_str());
      exit(20);
  }

  //
  // Make an input buffer large enough for
  // the whole file
  //
  sourceSize = statbuf.st_size;
  buffer = (char *)malloc(sourceSize);

  //
  // Define the wrapped file descriptors
  //
  NASTYFILE inputFile(filenastiness);      // See c150nastyfile.h for interface
  // remind you of FILE
  //  It's defined as:
  // typedef C150NastyFile NASTYFILE

  // do an fopen on the input file
  fopenretval = inputFile.fopen(sourceName.c_str(), "rb");
  // wraps Unix fopen
  // Note rb gives "read, binary"
  // which avoids line end munging

  if (fopenretval == NULL) {
      cerr << "Error opening input file " << sourceName <<
           " errno=" << strerror(errno) << endl;
      exit(12);
  }

  //
  // Read the whole file
  //
  len = inputFile.fread(buffer, 1, sourceSize);
  if (len != sourceSize) {
      cerr << "Error reading file " << sourceName <<
           "  errno=" << strerror(errno) << endl;
      exit(16);
  }

  if (inputFile.fclose() != 0 ) {
      cerr << "Error closing input file " << sourceName <<
           " errno=" << strerror(errno) << endl;
      exit(16);
  }


  //
  // build file name msg
  //
 
  uint64_t fileId = id;
  enum Type sendStatus;
  struct Message *MsgOut = (struct Message *)sbuffer;
  strcpy(MsgOut->head, "FILENAME\0");
  buildMsgOut(MsgOut, file, "", fileId);
  
  // send file name
  sendStatus = sendpacket(sbuffer, rbuffer, sock);
  if(sendStatus == FAILURE) {
  
    return FAILURE;
  }
  //
  // build content msg
  //
  char *position = buffer;
  char *end = buffer + sourceSize;
  uint64_t contentId = id;
   
  MsgOut = (struct Message *)sbuffer;
  
  
  
  while(position < end)
  {
      strcpy(MsgOut->head, "CONTENT\0");
      int length = (480 <= (end - position)) ? 480: (end - position);

      MsgOut->id = ++contentId;
      MsgOut->data.filecontent.size = length;
      strncpy(MsgOut->data.filecontent.content, position, length);
      sendStatus = sendpacket(sbuffer, rbuffer, sock);
      if (sendStatus == FAILURE)
          return FAILURE;
      position += length;
  }
    strcpy(MsgOut->head, "FINISH\0");
    MsgOut->id = ++ contentId;
    sendStatus = sendpacket(sbuffer, rbuffer, sock);
    return sendStatus;
}


enum Type sendpacket(char *sbuffer, char *rbuffer, C150NastyDgmSocket *sock) {
    struct Message *MsgIn = (struct Message *)rbuffer;
    struct Message *MsgOut = (struct Message *)sbuffer;

    int sendtimes = 0;
    sock->write(sbuffer, 512);
    while(sendtimes < SENDTIMES) {
        *GRADING<<"Sending packet "<<MsgOut->head<<" id "<<MsgOut->id<<endl;
        sock->write(sbuffer, 512);
        sock->read(rbuffer, 512);
        if( sock -> timedout() ) {
            sendtimes++;
            continue;
        }
        if ( MsgIn->id != MsgOut->id) {
            *GRADING<<"Not received ACK for "<<" id "<<MsgOut->id<<endl;
         
            sendtimes++;
         
            *GRADING<<"Resend packet "<<" id "<<MsgOut->id<<endl;
         
            continue;
        }
        *GRADING<<"Received ACK for "<<" id "<<MsgOut->id<<endl;
       
        return SUCCESS;
        
    }
    return FAILURE;
}

enum Type checkfile(string dir, string file, char *sbuffer, char *rbuffer, C150NastyDgmSocket *sock, int filenastiness){
    struct Message *MsgIn = (struct Message *)rbuffer;
    struct Message *MsgOut = (struct Message *)sbuffer;
    string sourceName = makeFileName(dir, file);
    uint64_t fileId = MsgOut->id + 1;
    //create message
    strcpy(MsgOut->head, "CHECK\0");
    buildMsgOut(MsgOut, file, sourceName, fileId);
    enum Type checkresult = sendpacket(sbuffer, rbuffer, sock);
    if (checkresult == FAILURE){
      return FAILURE;
    }
    if(MsgIn->data.ack.type == SUCCESS)
      return SUCCESS;
    else
      return FAILURE;
}

void calculateSHA(string sourceName, int filenastiness, unsigned char *hashcode ) {
      NASTYFILE  inputFile(filenastiness);
      char *fbuffer;
      void *fopenretval;
      size_t len;


      struct stat statbuf;
      size_t sourceSize;


      if (lstat(sourceName.c_str(), &statbuf) != 0) {
          fprintf(stderr,"copyFile: Error stating supplied source file %s\n", sourceName.c_str());
          exit(20);
      }

      //
      // Make an input buffer large enough for
      // the whole file
      //


      sourceSize = statbuf.st_size;
      fbuffer = (char *)malloc(sourceSize+1);
      // do an fopen on the input file
      fopenretval = inputFile.fopen(sourceName.c_str(), "rb");
      // wraps Unix fopen
      // Note rb gives "read, binary"
      // which avoids line end munging

      if (fopenretval == NULL) {
          cerr << "Error opening input file " << sourceName <<
               " errno=" << strerror(errno) << endl;
          exit(12);
      }


      //
      // Read the whole file
      //
      len = inputFile.fread(fbuffer, 1, sourceSize);

      if (len != sourceSize) {
          cerr << "Error reading file " << sourceName <<
               "  errno=" << strerror(errno) << endl;
          exit(16);
      }

      if (inputFile.fclose() != 0 ) {
          cerr << "Error closing input file " << sourceName <<
               " errno=" << strerror(errno) << endl;
          exit(16);
      }

      fbuffer[sourceSize] = '\0';
      //calculate SHA hashcode
      SHA1((const unsigned char*)fbuffer, sourceSize, hashcode);
      free(fbuffer);

  }


 void buildMsgOut(struct Message *MsgOut, string data1, string data2, uint64_t id) {
      if(strcmp(MsgOut->head, "CHECK\0") == 0) {
           
          // data1 is filename, data2 is dir+filename, id is file number
          strncpy(MsgOut->data.check.filename, data1.c_str(), data1.length());
          MsgOut->data.check.filename[data1.length()] = '\0';
          MsgOut->id = id;
          calculateSHA(data2, 0,  MsgOut->data.check.hashcode);
          MsgOut->data.check.hashcode[20] = '\0';
      }
      else if(strcmp(MsgOut->head, "ACK\0") == 0) {
          
          // data1 is status, id is file number
          MsgOut->id = id;
          if(data1 == "FAILURE")
                  MsgOut->data.ack.type = FAILURE;
          else if(data1 == "SUCCESS")
                  MsgOut->data.ack.type = SUCCESS;
          else if(data1 == "RECEIVED")
                  MsgOut->data.ack.type = RECEIVED;
      }
       else if(strcmp(MsgOut->head, "FILENAME\0") == 0) {
           
          // data1 is filename, data2 is null, id is file number
          MsgOut->id = id;
          strcpy(MsgOut->data.filename.filename, data1.c_str());
      }

       else if(strcmp(MsgOut->head, "FINISH\0") == 0) {
          
          // data1 is filename, data2 is null, id is file number
          MsgOut->id = id;
          strcpy(MsgOut->data.finish.filename, data1.c_str());
      }
}
void checkCommandLine(int argc, char *argv[]) {

      if (argc != 5)  {
          fprintf(stderr,"Correct syntxt is: %s <server> <networknastiness_number> <filenastiness_number> <SRC dir>\n", argv[0]);
          exit(1);
      }

      if (strspn(argv[2], "0123456789") != strlen(argv[2])) {
          fprintf(stderr,"Networknastiness %s is not numeric\n", argv[2]);
          fprintf(stderr,"Correct syntxt is: %s <server> <networknastiness_number> <filenastiness_number> <SRC dir>\n", argv[0]);
          exit(4);
      }

      if (strspn(argv[3], "0123456789") != strlen(argv[3])) {
          fprintf(stderr,"Filenastiness %s is not numeric\n", argv[3]);
          fprintf(stderr,"Correct syntxt is: %s <server> <networknastiness_number> <filenastiness_number> <SRC dir>\n", argv[0]);
          exit(4);
      }
}


// ------------------------------------------------------
//
//                   isFile
//
//  Make sure the supplied file is not a directory or
//  other non-regular file.
//
// ------------------------------------------------------

bool
isFile(string fname) {
    const char *filename = fname.c_str();
    struct stat statbuf;
    if (lstat(filename, &statbuf) != 0) {
        fprintf(stderr,"isFile: Error stating supplied source file %s\n", filename);
        return false;
    }

    if (!S_ISREG(statbuf.st_mode)) {
        fprintf(stderr,"isFile: %s exists but is not a regular file\n", filename);
        return false;
    }
    return true;
}

// ------------------------------------------------------
//
//                   checkDirectory
//
//  Make sure directory exists
//
// ------------------------------------------------------

void
checkDirectory(char *dirname) {
    struct stat statbuf;
    if (lstat(dirname, &statbuf) != 0) {
        fprintf(stderr,"Error stating supplied source directory %s\n", dirname);
        exit(8);
    }

    if (!S_ISDIR(statbuf.st_mode)) {
        fprintf(stderr,"File %s exists but is not a directory\n", dirname);
        exit(8);
    }
}

// ------------------------------------------------------
//
//                   makeFileName
//
// Put together a directory and a file name, making
// sure there's a / in between
//
// ------------------------------------------------------

string
makeFileName(string dir, string name) {
    stringstream ss;

    ss << dir;
    // make sure dir name ends in /
    if (dir.substr(dir.length()-1,1) != "/")
        ss << '/';
    ss << name;     // append file name to dir
    return ss.str();  // return dir/name

}

