// --------------------------------------------------------------
//
//                        fileserver.cpp
//
//        Author:
//              Xu Liu, Yu Lei
//
//        COMMAND LINE
//
//             fileserver <networknastiness> <filenastiness> <targetdir>
//
//        OPERATION
//
//              fileserver will loop receiving files from
//              any client.
//
//        Copyright: Some codes are adopted from Noah with his consent.
//
// --------------------------------------------------------------

#include "c150nastyfile.h"        // for c150nastyfile & framework
#include "c150nastydgmsocket.h"
#include "c150grading.h"          // for grading
#include "c150debug.h"
#include <fstream>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <unistd.h>
#include "message.h"
#include <stdint.h>
 
#define WAIT 100

using namespace C150NETWORK;  // for all the comp150 utilities

void checkDirectory(char *dirname);
enum Type e2eCheck(char *dirname, char *filename, unsigned char *shaVal, int fileNastiness);
void printSHA(unsigned char* shaValue);
string makeFileName(string dirname, string filename);
void buildSHA(string sourceName, int fileNastiness, unsigned char *obuf);
int receiveFile(string filename, char *sbuffer, char *rbuffer, int fileNastiness, C150NastyDgmSocket *sock);
uint64_t idle(C150NastyDgmSocket * sock, char* rbuffer, char* sbuffer);
int checkFile(string filename, char *sbuffer, char *rbuffer, int fileNastiness, C150NastyDgmSocket *sock);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                           main program
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int
main(int argc, char *argv[])
{

    //
    // Variable declarations
    //
    int networkNastiness;        // how aggressively do we drop packets, etc?
    int fileNastiness;           // how aggressively do we crash the file?
    DIR *TARGET;                 // Directory for target

    //
    //  For the purpose of grading
    //
    GRADEME(argc, argv);
    //
    // Check command line and parse arguments
    //
    if (argc != 4)  {
        fprintf(stderr,"Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }
    if (strspn(argv[1], "0123456789") != strlen(argv[1])) {
        fprintf(stderr,"networkNastiness %s is not numeric\n", argv[1]);
        fprintf(stderr,"Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }
    if (strspn(argv[2], "0123456789") != strlen(argv[2])) {
        fprintf(stderr,"fileNastiness %s is not numeric\n", argv[2]);
        fprintf(stderr,"Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(4);
    }
    // Open the target directory (we don't really need to do
    // this here, but it will give cleaner error messages than
    // waiting for individual file writes to fail.
    //
    checkDirectory(argv[3]);

    TARGET = opendir(argv[3]);
    if (TARGET == NULL) {
        fprintf(stderr,"Error opening target directory %s \n", argv[3]);
        exit(8);
    }
    closedir(TARGET);          // we just wanted to be sure it was there

    networkNastiness = atoi(argv[1]);   // convert command line string to integer
    fileNastiness = atoi(argv[2]);

    //
    // Create socket, loop receiving and responding
    //
    try {

        C150NastyDgmSocket *sock = new C150NastyDgmSocket(networkNastiness);
	    sock->turnOnTimeouts(50);
        char rbuffer[512];
        struct Message *received = (struct Message *)rbuffer;
        char sbuffer[512];
 
 
	    //uint64_t fileid;
        //
        // infinite loop processing messages
        //
        while(1)	{
	        //waiting for file to be received  
	        *GRADING << "Loop on waiting copy request"<< endl; 
	        cout<< "Loop on waiting copy request"<< endl; 	    	
            idle(sock, sbuffer,rbuffer);
             
            //receive file
            string file = makeFileName(argv[3], received->data.filename.filename);
            *GRADING << "Begin receive:<"<< received->data.filename.filename <<">"<< endl; 
            cout<<"Begin receive:<"<< received->data.filename.filename <<">"<< endl;
            if (receiveFile(file, sbuffer, rbuffer, fileNastiness, sock) != 0) {
                //delete the failed file           
	            unlink(file.c_str());  
                *GRADING << "Failed on receiving:<"<< received->data.filename.filename <<">"<< endl; 
                continue;
            } 
            *GRADING << "Waiting check request for <"<<file<<">"<<endl; 
           
	        if (checkFile(file, sbuffer, rbuffer, fileNastiness, sock) != 0) {
	            *GRADING << "Check failure on <"<<file<<">"<<endl; 
                cout<<"Check failure on <"<<file<<">"<<endl; 
		        unlink(file.c_str());
            } else {
                *GRADING << "Check success on <"<<file<<">"<<endl; 
                cout<<"Check success on <"<<file<<">"<<endl;
            }
            

        }
    }

    catch (C150NetworkException e) {
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation() << endl;
    }


    // This only executes if there was an error caught above
    return 4;
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
//              e2eCheck
//
//  check the correctness of received file
//
// ------------------------------------------------------
enum Type
e2eCheck(string filename, unsigned char *shaVal, int fileNastiness) {
    unsigned char *obuf; // sha value
    obuf = (unsigned char *)malloc(sizeof(unsigned char) * 21);

    

    // calculate sha value
    buildSHA(filename, fileNastiness, obuf);

    if(strcmp((char*)shaVal, (char*) obuf) == 0) {
        return SUCCESS;
    }
    return FAILURE;
}

// ------------------------------------------------------
//
//              printSHA
//
// print SHA for the sake of debugging
//
// ------------------------------------------------------
void
printSHA(unsigned char* shaValue) {
    for (int i = 0; i < 20; i++) {
        printf ("%02x", (unsigned int) shaValue[i]);
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
  //make sure dir name ends in /
  if (dir.substr(dir.length()-1,1) != "/")
  	ss << '/';
  	ss << name;     // append file name to dir
  	return ss.str();  // return dir/name
           
}
  
// ------------------------------------------------------
//
//              buildSHA
//
// Build SHA for a given file
//
// ------------------------------------------------------
void
buildSHA(string sourceName, int nastiness, unsigned char *obuf) {
    struct stat statbuf;
    size_t sourceSize;
    char *buffer;
    void *fopenretval;
    size_t len;

    try {
        //
        // Read whole input file
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
        NASTYFILE inputFile(nastiness);
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
        // Read the whole file into buffer
        //
        len = inputFile.fread(buffer, 1, sourceSize);
        if (len != sourceSize) {
            cerr << "Error reading file " << sourceName <<
                 "  errno=" << strerror(errno) << endl;
            exit(16);

        }
        //
        //  Generate SHA
        //
        SHA1((const unsigned char *)buffer, sourceSize, obuf);
        obuf[20] = '\0';

        if (inputFile.fclose() != 0 ) {
            exit(16);
        }
    }
    catch (C150Exception e) {
        cerr << "nastyfiletest:copyfile(): Caught C150Exception: " <<
             e.formattedExplanation() << endl;
    }
}
int receiveFile(string filename, char *sbuffer, char *rbuffer, int fileNastiness, C150NastyDgmSocket *sock) {

    void *fopenretval;
    struct stat statbuf;
    NASTYFILE outputFile(fileNastiness);
    unsigned int len;
    struct Message *received = (struct Message *)rbuffer;
    struct Message *send = (struct Message *)sbuffer;
    uint64_t lastid = received->id;
    
    if (lstat(filename.c_str(), &statbuf) == 0) {
         unlink(filename.c_str());   
    }
    fopenretval = outputFile.fopen(filename.c_str(), "wb");
    if (fopenretval == NULL) {
      cerr << "Error opening input file " << filename << 
	      " errno=" << strerror(errno) << endl;
      exit(12);
    } 


    *GRADING <<"Creat new file: <"<<filename<<">"<<endl;
    
    int countreceive = 0;
    strcpy(send->head, "ACK\0");
	send->id = lastid;      
	send->data.ack.type = RECEIVED;
	       
    do {
        sock->read(rbuffer, 512);

        if (countreceive >= WAIT) { 
	       
            outputFile.fclose(); 
            return 1;
        }

        if (sock->timedout()) { 
                sock->write(sbuffer, 512); 
                countreceive++;
                continue;
        }
        if (( strcmp(received->head, "FINISH\0") != 0
            && strcmp(received->head, "CONTENT\0") != 0) || 
             (received->id != lastid + 1)  ) {
        
            sock->write(sbuffer, 512);  
            countreceive++;
            continue;
        }
  
       
        countreceive = 0;
        if ( strcmp(received->head, "CONTENT\0") == 0) {

            len = outputFile.fwrite(received->data.filecontent.content, 1, received->data.filecontent.size);
            if (len != received->data.filecontent.size) {
            	//send ACK
            	*GRADING<<"Error on saving packet "<<received-> id <<" of "<<filename<< endl;
            	strcpy(send->head, "ACK\0");
	    	    send->id = received->id;      
	    	    send->data.ack.type = FAILURE;
	    	    sock->write(sbuffer, 512);
            	outputFile.fclose();
             
            	return 1;
            }
            
            
            strcpy(send->head, "ACK\0");
	        send->id = received->id;      
	        send->data.ack.type = RECEIVED;
            lastid = received->id;
        
	        sock->write(sbuffer, 512);
            continue;
        }
 
        if( strcmp(received->head, "FINISH\0") == 0) {
            //send ACK
     
            strcpy(send->head, "ACK\0");
	        send->id = received->id;      
	        send->data.ack.type = RECEIVED;
	        sock->write(sbuffer, 512);
 	 
	        *GRADING<<"Finished file: "<< filename<<" packet "<<received->id<<endl;
	        cout<<"Finished file: "<< filename<<" packet "<<received->id<<endl;     
	   
            outputFile.fclose();
            
            return 0;
        }

       


    } while(1);       
    return 1;
}

uint64_t idle(C150NastyDgmSocket * sock, char* sbuffer, char* rbuffer) {
            struct Message *received = (struct Message *)rbuffer;
            struct Message *send = (struct Message *)sbuffer;
         
            do {

        		sock->read(rbuffer, 512);
                 
	        } while (sock->timedout() || (strcmp(received->head, "FILENAME\0")) != 0);	 
              
	        //send ACK
       	    strcpy(send->head, "ACK\0");
	        send->id = received->id;      
	        send->data.ack.type = RECEIVED;
	        sock->write(sbuffer, 512);
            return received->id;
}

int checkFile(string filename, char *sbuffer, char *rbuffer, int fileNastiness, C150NastyDgmSocket *sock) {

    struct Message *received = (struct Message *)rbuffer;
    struct Message *send = (struct Message *)sbuffer;
    uint64_t lastid = received->id;    

    // waiting for check request
    *GRADING<<"Waiting for check request "<<send->id<<endl;
  
    int countwaiting = 0;
    strcpy(send->head, "ACK\0");
	send->id = lastid;      
	send->data.ack.type = RECEIVED;
	 
 
    while (countwaiting < WAIT){
        sock -> read(rbuffer, 512);
        if( sock->timedout()) {
            sock->write(sbuffer, 512);    
	        countwaiting++;
			continue;
		}
      
        if (received->id != lastid + 1 ) {
 
            sock->write(sbuffer, 512);
			countwaiting++;
            continue;
        }
		break;
    } 
    if(countwaiting >= WAIT) {
		//send ACK  
 
	    send->data.ack.type = FAILURE;
	    sock->write(sbuffer, 512);
		return 1;
    }
         
      // Do end to end check
      char* fname = received->data.check.filename;
      unsigned char* shaValue = received->data.check.hashcode;

      *GRADING << "File:<" << fname << "> check request received, beginning end-to-end check" << endl;
       
      enum Type checkResult = e2eCheck(filename, shaValue, fileNastiness);
      if(checkResult == SUCCESS) {
            *GRADING << "File:<" << fname << "> end-to-end check succeeded" << endl;
      }
        else {
            *GRADING << "File:<" << fname << "> end-to-end check failed" << endl;
      }


            //
            //  Create the return eessage
            //
      strcpy(send->head, "ACK\0");
      send->data.ack.type = checkResult;
      send->id =  received->id;
      lastid = received->id;

            //
            // Send back check result
            //
      

            //
            // Retry up to 5 times if no ACK received
            //
      int resend = 0;
      while(resend < 10) {
            sock -> write(sbuffer, 512);
            resend++;
      }
      if(checkResult == SUCCESS)
        return 0;
      else
        return 1;

}
