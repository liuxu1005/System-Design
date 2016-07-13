#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include "c150exceptions.h"
#include "declarations.h"
#include "functiondeclaration.h"
#include "typedeclaration.h"

#define MYMAX 1024

using namespace std;

const int WIDTH = 80;

unsigned int length(string s);
void createBadFunction(ofstream *out);
void createSend(ofstream *out, int type);
void createMsg(TypeMap &types, ofstream *out);
void createReadMsg(TypeMap &types, ofstream *out);
void createCountSize(TypeMap &types, ofstream *out);
void createReceive(ofstream *out, int type);
void createProxyAuxilary(TypeMap &types, ofstream *myproxy);
void createStubMains(string fn, FunctionDeclaration *fp, 
                     ofstream *mystub, unsigned int id);
void createStubAuxilary(TypeMap &types, ofstream *mystub);
void createDispatch(FunctionMap &fm, ofstream *mystub);
void createProxyMains(string fn, FunctionDeclaration *fp, 
                      ofstream *myproxy, unsigned int id);
void createProxyHead(string s, ofstream *out);
void createStubHead(string s,  ofstream *out);
void createStubs(Declarations *parseTree, const char f[]) ;
void processIDLFile(const char fileName[]);


int main(int argc, char* argv[]) {

    //examine arguments
    if (argc != 2) {
        fprintf(stderr, "Syntax: %s idlfile1\n", argv[0]);
        exit(8);
    }
    
    
    //process idl file
    
    try {
        processIDLFile(argv[1]);
    }
    
    catch (C150Exception e) {
        fprintf(stderr, "%s: caught C150Exception: %s \n", argv[0],
                e.formattedExplanation().c_str());
        fprintf(stderr, "...Giving up on file %s...\n",argv[1]);
        exit(EXIT_FAILURE);
    
    }
    return 0;
}

void processIDLFile(const char fileName[]) {
    
    // Open the file
    ifstream idlFile(fileName);        

    if (!idlFile.is_open()) {
        stringstream ss;
        ss << "Could not open IDL file: " << fileName;
        throw C150Exception(ss.str());
    }
    //Parse the file
    Declarations parseTree(idlFile);
    
    //create proxy and stub files
    createStubs(&parseTree, fileName);

}

void createStubs(Declarations *parseTree, const char f[]) {
    
    ofstream myproxy;
    string proxyout = f;
    int tmpposition = proxyout.find('.');
    proxyout = proxyout.substr(0, tmpposition);
    proxyout += ".proxy.cpp";
    myproxy.open(proxyout.c_str());
    ofstream mystub;
    string stubout = f;
    tmpposition = stubout.find('.');
    stubout = stubout.substr(0, tmpposition);
    stubout += ".stub.cpp";
    mystub.open(stubout.c_str());
    
    //create file header
    createProxyHead(f, &myproxy);
    createStubHead(f,  &mystub);
    
    //create auxilary function
    createStubAuxilary(parseTree->types, &mystub);
    createProxyAuxilary(parseTree->types, &myproxy);
    
    //create each RPC function 
    
    FunctionDeclaration *functionp;
    
    string functionName;
    std::map<std::string, FunctionDeclaration*>::iterator iter;  
    unsigned int i = 0;
    for (iter = parseTree->functions.begin(); iter != parseTree->functions.end(); 
       ++iter) {
        functionp = iter -> second;
        functionName = iter->first;
        createProxyMains(functionName, functionp, &myproxy, i);
        createStubMains(functionName, functionp, &mystub, i++);
     
    }
    
    //create dipatch function
    createDispatch(parseTree->functions, &mystub);
    //close files
    myproxy.close();
    mystub.close();
}

void createProxyHead(string s, ofstream *out) {
    
    *out<<"#include \"rpcproxyhelper.h\"\n"
        <<"#include <cstdio>\n"
        <<"#include <cstring>\n"
        <<"#include \"c150debug.h\"\n"
        <<"#include <sstream>\n"
        <<"#include <cstdlib>\n"
        <<"\n#define MYMAX "<< MYMAX<<"\n"
        <<"using namespace C150NETWORK;\n"
        <<"using namespace std;\n";
    *out<<"\n"<<"#include \""<<s<<"\"\n";

}
void createStubHead(string s,  ofstream *out) {
     
    *out<<"#include \"rpcstubhelper.h\"\n"
        <<"#include <cstdio>\n"
        <<"#include <cstring>\n"
        <<"#include <cstdlib>\n"
        <<"#include \"c150debug.h\"\n"
        <<"#include <sstream>\n"
        <<"\n#define MYMAX "<<MYMAX<<"\n"
        <<"using namespace C150NETWORK;\n"
        <<"using namespace std;\n";
    *out<<"\n"<<"#include \""<<s<<"\"\n";
}


void createProxyMains(string fn, FunctionDeclaration *fp, 
                 ofstream *myproxy, unsigned int id) {

    *myproxy<<"\n";
    
    //create function head
    string tmp;
    if (fp->getReturnType()->isArray()) {
        tmp = fp->getReturnType()->getName().substr(2).c_str();
    } else {
        tmp = fp->getReturnType()->getName().c_str();
    }
    *myproxy<<tmp<<"\n";    
    *myproxy<<fn<<"(";
    
    ArgumentVector& args = fp->getArgumentVector();
    for (unsigned int i = 0; i < args.size(); i++) {
     
        string tmpname;
        if (args[i]->getType()->isArray()) {
            int tmpposition = args[i]->getType()->getName().find("[");
            tmp = args[i]->getType()->getName().substr(2, tmpposition - 2);
            tmpname = args[i]->getName() + args[i]->getType()->getName().substr(tmpposition);
        } else {
            tmp = args[i]->getType()->getName();
            tmpname = args[i]->getName();
        }
 
        *myproxy<<tmp;
   
   
        *myproxy<<" "<<tmpname;
        if (i < args.size() - 1)
            *myproxy<<","; 
            
    }
    *myproxy<<")\n";
    
    //create function body
    
    *myproxy<<"{\n";
    
    *myproxy<<"    unsigned int sendSize = 0;\n";

  
    
    for (unsigned int i = 0; i < args.size(); i++) {
    
        TypeDeclaration* t = args[i]->getType();
        if (t->isArray()) {
            int tposition =t->getName().find("[");
                    
            unsigned int bound = length(t->getName().substr(2));
            *myproxy<<"    for (int j = 0; j < "<<bound<<"; j++) {\n";
            *myproxy<<"        sendSize += countSize("
                <<args[i]->getName()<<" + j, \""
                <<t->getName().substr(2, tposition - 2)<<"\");\n";
            *myproxy<<"    }\n";           
 
                    
        } else {
    
            *myproxy<<"    sendSize += countSize(&"
                    <<args[i]->getName()<<", \""
                    <<args[i]->getType()->getName()<<"\");\n";
        }
    
    }
  
    
    *myproxy<<"    sendSize += 66;\n"
            <<"    char *sendBuffer = (char *)malloc(sendSize);\n"
            <<"    char *p = sendBuffer;\n"
            <<"    sprintf(p, \"%u\", sendSize);\n"
            <<"    p += strlen(p) + 1;\n"
            <<"    sprintf(p, \"%u\", "<<id<<");\n"
            <<"    p += strlen(p) + 1;\n";
    for (unsigned int i = 0; i < args.size(); i++) {
    
        if (args[i]->getType()->isArray()) {
            int tposition =args[i]->getType()->getName().find("[");
            unsigned int bound = length(args[i]->getType()->getName().substr(2));
            *myproxy<<"    for (int j = 0; j < "<<bound<<"; j++) {\n";
            *myproxy<<"        p += createMsg("<<args[i]->getName()<<" + j, \""
               <<args[i]->getType()->getName().substr(2, tposition - 2)
               <<"\", p);\n"
               <<"    }\n";
        
        } else {
    
            *myproxy<<"    p += createMsg(&"<<args[i]->getName()<<", \""
                    <<args[i]->getType()->getName()<<"\", p);\n";
        }
    
    }
    *myproxy<<"    c150debug->printf(C150RPCDEBUG, \""
            <<fn<<" invoked\");\n";
    //send parameters
    *myproxy<<"    sendParameter(sendBuffer, sendSize);\n";
    
    //receive result
    *myproxy<<"    char *readBuffer;\n"; 
    *myproxy<<"    readBuffer = receiveResult();\n";
    
    
    //process result
  
    *myproxy<<"    char *curposition = readBuffer + strlen(readBuffer) + 1;\n";
    *myproxy<<"    if (strcmp(curposition, \"DONE\") != 0) {\n";
    *myproxy<<"        throw C150Exception(\""<<fn
            <<": received invalid response from the server\");\n";
    *myproxy<<"    }\n";    
    if (strcmp(fp->getReturnType()->getName().c_str(), "void") != 0) {
        //reconstruct result
        string vName = " rlt";
        unsigned int tposition;
        if (fp->getReturnType()->isArray()) {
            tposition = fp->getReturnType()->getName().find("[");
            *myproxy<<"    "<<fp->getReturnType()->getName().substr(2, tposition -2);
            *myproxy<<vName<<fp->getReturnType()->getName().substr(tposition)<<"\n";
        } else {
            *myproxy<<"    "<<fp->getReturnType()->getName()<<vName<<";\n";
        }
        
        //read result
        *myproxy<<"    curposition = curposition + strlen(curposition) + 1;\n\n";
    
        if (fp->getReturnType()->isArray()) {
    
            int tposition =fp->getReturnType()->getName().find("[");
            unsigned int bound = length(fp->getReturnType()->getName().substr(2));
            *myproxy<<"    for (int j = 0; j < "<<bound<<"; j++) {\n";
            *myproxy<<"        curposition += readMsg(rlt + j, "
               <<fp->getReturnType()->getName().substr(2, tposition - 2)
               <<", curposition);\n"
               <<"    }\n";
            
    
        } else {
            *myproxy<<"    curposition += readMsg(&rlt, \""
               <<fp->getReturnType()->getName()
               <<"\", curposition);\n";
  
        }
       
            
       
        //return result
        *myproxy<<"    free(sendBuffer);\n";
        *myproxy<<"    free(readBuffer);\n"; 
        *myproxy<<"    return rlt;\n";
    } else {
    
        *myproxy<<"    free(sendBuffer);\n";
        *myproxy<<"    free(readBuffer);\n";
    }
    *myproxy<<"\n}\n";    

}


void createStubMains(string fn, FunctionDeclaration *fp, 
                     ofstream *mystub, unsigned int id) {
    *mystub<<"\n";
    
    //create function head
    ostringstream out;
    out<<"char *"<<"__func"<<id<<"(char* para, unsigned int *amount) \n{\n";
  
    out<<"    char *rltp;\n";
    //create function body
    string vName = " rlt";
    unsigned int tposition;
    if (fp->getReturnType()->isArray()) {
        tposition = fp->getReturnType()->getName().find("[");
        out<<"    "<<fp->getReturnType()->getName().substr(2, tposition -2);
        out<<vName<<fp->getReturnType()->getName().substr(tposition)<<"\n";
    } else if (strcmp(fp->getReturnType()->getName().c_str(), "void") != 0) {
        out<<"    "<<fp->getReturnType()->getName()<<vName<<";\n";
    }
 
    
    ArgumentVector& args = fp->getArgumentVector();
    vName = " p";
    for (unsigned int i = 0; i < args.size(); i++) {
        
        if (args[i]->getType()->isArray()) {
            tposition = args[i]->getType()->getName().find("[");
            out<<"    "<<args[i]->getType()->getName().substr(2, tposition - 2);
            out<<vName<<i
               <<args[i]->getType()->getName().substr(tposition);
        } else {
            out<<"    "<<args[i]->getType()->getName();
            out<<vName<<i;
        }
        out<<";\n";
            
    }
    
    
    //read parameters
    out<<"    char *curposition = para;\n\n";
    for (unsigned int i = 0; i < args.size(); i++) {
        
        if (args[i]->getType()->isArray()) {
            int tposition =args[i]->getType()->getName().find("[");
            unsigned int bound = length(args[i]->getType()->getName().substr(2));
            out<<"    for (int j = 0; j < "<<bound<<"; j++) {\n";
            out<<"        curposition += readMsg(p"<<i<<" + j, \""
               <<args[i]->getType()->getName().substr(2, tposition - 2)
               <<"\", curposition);\n"
               <<"    }\n";
            
    
        } else {
            out<<"    curposition += readMsg(&p"<<i
               <<", \""<<args[i]->getType()->getName()
               <<"\", curposition);\n";
  
        }
       
            
    }   
    out<<"\n"; 
    //call function
    if (strcmp(fp->getReturnType()->getName().c_str(), "void") != 0) {
        out<<"    rlt = "<<fn<<"(";
    
    } else {
        out<<"    "<<fn<<"(";
    }
    for (unsigned int i = 0; i < args.size(); i++) {
       
        if (i < args.size() -1)
            out<<"p"<<i<<", ";
        else
            out<<"p"<<i;    
    } 
    out<<");\n\n";   
    
    
    
    if (strcmp(fp->getReturnType()->getName().c_str(), "void") != 0) {
        out<<"    unsigned int sendSize = 0;\n";
        if (fp->getReturnType()->isArray()) {
            //count result size
            int tposition =fp->getReturnType()->getName().find("[");
                    
            unsigned int bound = length(fp->getReturnType()->getName().substr(2));
            out<<"    for (int j = 0; j < "<<bound<<"; j++) {\n"
               <<"        sendSize += countSize(rlt + j, \""
               <<fp->getReturnType()->getName().substr(2, tposition - 2)<<"\");\n"
               <<"    }\n";   
                   
            //create result message
            out<<"    sendSize = sendSize + 33 + 6;\n"
               <<"    *amount = sendSize;\n"
               <<"    rltp = (char *)malloc(*amount);\n"
               <<"    sprintf(rltp, \"%u\", sendSize);\n"
               <<"    curposition = rltp;\n"
               <<"    curposition += (strlen(rltp) + 1);\n"
               <<"    strcpy(curposition, \"DONE\");\n"
               <<"    curposition += 5;\n";
            
            out<<"    for (int j = 0; j < "<<bound<<"; j++) {\n"
               <<"        rltp += createMsg(rlt + i, \"" 
               <<fp->getReturnType()->getName().substr(2, tposition - 2)<<"\");\n"
               <<", curposition);\n"
               <<"    }\n";        
       
 
                    
        } else {
            //count result size
            out<<"    sendSize += countSize(&rlt, \""
                    <<fp->getReturnType()->getName()<<"\");\n";
            //create result message
            out<<"    char rltsize[33];\n"
               <<"    sendSize = sendSize + 33 + 6;\n"
               <<"    *amount = sendSize;\n"
               <<"    rltp = (char *)malloc(*amount);\n"
               <<"    sprintf(rltsize, \"%u\", sendSize);\n"
               <<"    strcpy(rltp, rltsize);\n"
               <<"    curposition = rltp;\n"
               //<<"    rltp[rltsize.length()] = '\\0';\n"
               <<"    curposition += (strlen(rltsize) + 1);\n"
               <<"    strcpy(curposition, \"DONE\");\n"
               <<"    curposition += 5;\n"
               <<"    createMsg(&rlt, \""
               <<fp->getReturnType()->getName()
               <<"\", curposition);\n";
              
        }
 
        
    } else {
        //count Done message;
        out<<"    rltp = (char *)malloc(10);\n"
           <<"    strcpy(rltp, \"10\");\n"
           <<"    curposition = rltp + strlen(\"10\") + 1;\n"
           //<<"    rltp[rltsize.length()] = '\\0';\n"
           <<"    strcpy(curposition, \"DONE\");\n"
           <<"    *amount = 8;\n";      
    }    
    out<<"\n    return rltp;\n}\n";
    *mystub<<out.str();

}

void createBadFunction(ofstream *out) {
    *out<<"\n";
    //create function head
  
    *out<<"char *"<<"__badFunction (unsigned int *amount) \n{\n";
  
    *out<<"    char *done = (char *)malloc(10);\n"    
       <<"    c150debug->printf(C150RPCDEBUG,\" received call for nonexistent function\");\n"
       <<"    strcpy(done, \"10\");\n"
       <<"    strcpy((done + strlen(\"10\") + 1), \"BAD\");\n"
       <<"    *amount = 7;\n"
       <<"    return done;\n";
       
    *out<<"}\n";

}

void createStubAuxilary(TypeMap &types,  ofstream *mystub) {
    //create sendParameter function
    createSend(mystub, 1);
    //create receive function
    createReceive(mystub, 1);
    //create countSize
    createCountSize(types, mystub);
    //create createMsg
    createMsg(types, mystub);
    //create readMsg();
    createReadMsg(types, mystub);
    //create bad function
    createBadFunction(mystub);
    

}

void createDispatch(FunctionMap &fm, ofstream *mystub) {

    *mystub<<"\nvoid dispatchFunction() {\n"
           <<"    char *received = receiveResult();\n"
           <<"    if (received == NULL) return;\n"
           <<"    char *sendBuffer;\n"
           <<"    char *curposition = received + (strlen(received) + 1);\n"
           <<"    unsigned int amount = 0;\n" 
           <<"    unsigned int id = atoi(curposition);\n"
           <<"    curposition += (strlen(curposition) + 1);\n"
           <<"    if (!RPCSTUBSOCKET-> eof()) {\n"
           <<"        switch (id) {\n";
    unsigned int number = fm.size();
    for(unsigned int j = 0; j < number; j++) {
        *mystub<<"            case "<<j<<":\n"
               <<"                sendBuffer = __func"
               <<j<<"(curposition, &amount);\n"
               <<"                break;\n\n";
    }
    *mystub<<"            default:\n"
           <<"                sendBuffer = __badFunction(&amount);\n"
           <<"        }\n    }\n";
          
    *mystub<<"   sendParameter(sendBuffer, amount);\n"
           <<"   free(received);\n"
           <<"   free(sendBuffer);\n}\n";
}

void createReadMsg(TypeMap &types, ofstream *out) {
    *out<<"\n";
    *out<<"unsigned int readMsg(void *v, string type, char *p) {\n";
    
    *out<<"    char *start = p;\n   ";
    *out<<"    char *tmpposition = p;\n   ";
    
    TypeDeclaration *typep;
    
    string typeName;
    std::map<std::string, TypeDeclaration*>::iterator iter; 
    for (iter = types.begin(); iter != types.end(); 
       ++iter) {
        typep = iter -> second;
        typeName = iter->first;
        
        if (!typep->isArray() && !typep->isStruct() 
                        && strcmp(typeName.c_str(), "void") != 0) {
            *out<<" if (strcmp(\""<<typeName<<"\", type.c_str()) == 0) {\n\n";
            if(strcmp(typeName.c_str(), "string") == 0) {
            
                *out<<"        *(string *)v = tmpposition;\n";
            
            } else {
            
                *out<<"        istringstream buff(tmpposition);\n";
                *out<<"        buff>>*("<<typeName<<" *)v;\n";
            }
            
            
            *out<<"        tmpposition += (strlen(tmpposition) + 1);\n";
            *out<<"    } else ";
       
          
        } else if (typep->isStruct()) {
            *out<<" if (strcmp(\""<<typeName<<"\", type.c_str()) == 0) {\n\n";
            vector<Arg_or_Member_Declaration *> structMem = typep->getStructMembers();
            
            int size = structMem.size();
            for (int i = 0; i < size; i++) {
                TypeDeclaration* t = structMem[i]->getType();
                if (!t->isArray() && !t->isStruct() 
                    && strcmp(t->getName().c_str(), "void") != 0) {
                    
                    *out<<"        tmpposition += readMsg(&(((struct "<<typeName
                        <<" *)v)->"<<structMem[i]->getName() <<"), \""
                        <<t->getName()<<"\", tmpposition);\n";
                        
                    
                } else if (t->isStruct()) {
                    *out<<"        tmpposition += readMsg(&(((struct "<<typeName
                        <<" *)v)->"<<structMem[i]->getName() <<"), \""
                        <<t->getName()<<"\", tmpposition);\n";
                
                } else if (t->isArray()) {
                    int tposition =t->getName().find("[");
                    
                    unsigned int bound = length(t->getName().substr(2));
                    *out<<"        for (int j = 0; j < "<<bound<<"; j++) {\n";
                   // *out<<"            "<<t->getName().substr(2, tposition - 2)
                    *out<<"            void *memv = "<<"((struct "
                        <<typeName<<" *)v)->"
                        <<structMem[i]->getName() <<"; \n";
                    *out<<"             tmpposition += readMsg(("
                        <<t->getName().substr(2, tposition - 2)<<" *)memv + j, \""
                        <<t->getName().substr(2, tposition - 2)
                        <<"\", tmpposition);\n";
                    *out<<"        }\n";
                }              
            }
            *out<<"    } else ";
        }  
     
    }
    *out<<"{ }\n";
    *out<<"    return tmpposition - start;\n}\n\n";
    
}

void createProxyAuxilary(TypeMap &types, ofstream *myproxy) {
    //create sendParameter function
    createSend(myproxy, 0);
    //create receive function
    createReceive(myproxy, 0);
    //create countSize
    createCountSize(types, myproxy);
    //create createMsg
    createMsg(types, myproxy);
    //create readMsg();
    createReadMsg(types, myproxy);
}

void createSend(ofstream *out, int type) {
    //create sendParameter function
    *out<<"\n";
    
    *out<<"void sendParameter(char *send, unsigned int size) {\n";
    *out<<"    char *position = send;\n";
    *out<<"    char *end = send + size;\n\n";
    
    *out<<"    while (position < end) {\n";
    *out<<"        unsigned int length = MYMAX > (end - position) ? (end - position) : MYMAX;\n";
    if (type == 0) {
        *out<<"        RPCPROXYSOCKET->write(position, length);\n";
    } else {
        *out<<"        RPCSTUBSOCKET->write(position, length);\n";
    }
    *out<<"        position += length;\n"<<"    }\n";   

    *out<<"}\n";
}

void createReceive(ofstream *out, int type) {    
    //create receiveResult
    *out<<"\n";
    
    *out<<"char *receiveResult() {\n\n";
    
    *out<<"    char *result = (char *)malloc(MYMAX);\n";
    if (type == 0) {
        *out<<"    int readlen = RPCPROXYSOCKET->read(result, MYMAX);\n";
    } else {
        *out<<"    int readlen = RPCSTUBSOCKET->read(result, MYMAX);\n";
    }
    *out<<"    unsigned int bufsize = 0;\n";
    *out<<"    if (readlen <= 0) return NULL;\n";
    *out<<"    else bufsize = atoi(result);\n";
    *out<<"    if (bufsize > MYMAX) {\n";
    *out<<"        char *tmp = (char *)malloc(bufsize);\n";
    *out<<"        memcpy(tmp, result, MYMAX);\n";
    *out<<"        free(result);\n";
    *out<<"        result = tmp;\n";
    *out<<"        char *position = (result + MYMAX);\n";    
    *out<<"        char *end = result + bufsize;\n";   
    *out<<"        unsigned int length = 0;\n";
    *out<<"        while (position < end) {\n";
    *out<<"            length = MYMAX > (end - position) ? (end - position) : MYMAX;\n";
    if (type == 0) {
        *out<<"            RPCPROXYSOCKET->read(position, length);\n";
    } else {
        *out<<"            RPCSTUBSOCKET->read(position, length);\n";
    }
    *out<<"            position += length;\n";
    *out<<"        }\n";
    *out<<"    }\n";
    *out<<"    return result;\n}\n";
    
}  

void createCountSize(TypeMap &types, ofstream *out) {
    
    
    *out<<"\n";
    *out<<"unsigned int countSize(void *v, string type) {\n";
    *out<<"    ostringstream buff;\n";
    *out<<"    unsigned int tmp = 0;\n   ";
    
    TypeDeclaration *typep;
    
    string typeName;
    std::map<std::string, TypeDeclaration*>::iterator iter; 
    for (iter = types.begin(); iter != types.end(); 
       ++iter) {
        typep = iter -> second;
        typeName = iter->first;
        
        if (!typep->isArray() && !typep->isStruct() 
                        && strcmp(typeName.c_str(), "void") != 0) {
            *out<<" if (strcmp(\""<<typeName<<"\", type.c_str()) == 0) {\n\n";
            *out<<"        buff<<(*("<<typeName<<" *)v);\n";
            *out<<"        tmp = buff.str().length() + 1;\n";
            *out<<"    } else ";
       
          
        } else if (typep->isStruct()) {
            *out<<" if (strcmp(\""<<typeName<<"\", type.c_str()) == 0) {\n\n";
            vector<Arg_or_Member_Declaration *> structMem = typep->getStructMembers();
            
            int size = structMem.size();
            for (int i = 0; i < size; i++) {
                TypeDeclaration* t = structMem[i]->getType();
                if (!t->isArray() && !t->isStruct() 
                    && strcmp(t->getName().c_str(), "void") != 0) {
                    
                    *out<<"        tmp += countSize(&(((struct "<<typeName
                        <<" *)v)->"<<structMem[i]->getName() <<"), \""
                        <<t->getName()<<"\");\n";
                    
                } else if (t->isStruct()) {
                    *out<<"        tmp += countSize(&(((struct "<<typeName
                        <<" *)v)->"<<structMem[i]->getName() <<"), \""
                        <<t->getName()<<"\");\n";
                
                } else if (t->isArray()) {
                    int tposition =t->getName().find("[");
                    
                    unsigned int bound = length(t->getName().substr(2));
                    *out<<"        for (int j = 0; j < "<<bound<<"; j++) {\n";
                    //*out<<"            "<<t->getName().substr(2, tposition - 2)
                    *out<<"            void *memv = "<<"((struct "
                        <<typeName<<" *)v)->"
                        <<structMem[i]->getName() <<"; \n";
                    *out<<"             tmp += countSize(("
                        <<t->getName().substr(2, tposition - 2)<<" *)memv + j, \""
                        <<t->getName().substr(2, tposition - 2)
                        <<"\");\n";
                    *out<<"        }\n";
                }              
            }
            *out<<"    } else ";
        }  
     
    }
    *out<<"{ }\n";
    *out<<"    return tmp;\n}\n\n";
    

} 

unsigned int length(string s) {
     
    if (s.length() == 0) return 1;
    size_t found = s.find("[");
    if (found == std::string::npos)
        return 1;
    size_t found2 = s.find("]");
    
    return
    atoi(s.substr(found + 1, found2 - found -1).c_str()) * length(s.substr(found2 + 1));
      
}

void createMsg(TypeMap &types, ofstream *out) {
      
    *out<<"\n";
    *out<<"unsigned int createMsg(void *v, string type, char *p) {\n";
    *out<<"    ostringstream buff;\n";
    *out<<"    char *start = p;\n   ";
    *out<<"    char *tmpposition = p;\n   ";
    
    TypeDeclaration *typep;
    
    string typeName;
    std::map<std::string, TypeDeclaration*>::iterator iter; 
    for (iter = types.begin(); iter != types.end(); 
       ++iter) {
        typep = iter -> second;
        typeName = iter->first;
        
        if (!typep->isArray() && !typep->isStruct() 
                        && strcmp(typeName.c_str(), "void") != 0) {
            *out<<" if (strcmp(\""<<typeName<<"\", type.c_str()) == 0) {\n\n";
            *out<<"        buff<<(*("<<typeName<<" *)v);\n";
            *out<<"        strcpy(tmpposition, buff.str().c_str());\n";
            *out<<"        tmpposition += (buff.str().length() + 1);\n";
            *out<<"    } else ";
       
          
        } else if (typep->isStruct()) {
            *out<<" if (strcmp(\""<<typeName<<"\", type.c_str()) == 0) {\n\n";
            vector<Arg_or_Member_Declaration *> structMem = typep->getStructMembers();
            
            int size = structMem.size();
            for (int i = 0; i < size; i++) {
                TypeDeclaration* t = structMem[i]->getType();
                if (!t->isArray() && !t->isStruct() 
                    && strcmp(t->getName().c_str(), "void") != 0) {
                    
                    *out<<"        tmpposition += createMsg(&(((struct "<<typeName
                        <<" *)v)->"<<structMem[i]->getName() <<"), \""
                        <<t->getName()<<"\", tmpposition);\n";
                        
                    
                } else if (t->isStruct()) {
                    *out<<"        tmpposition += createMsg(&(((struct "<<typeName
                        <<" *)v)->"<<structMem[i]->getName() <<"), \""
                        <<t->getName()<<"\", tmpposition);\n";
                
                } else if (t->isArray()) {
                    int tposition =t->getName().find("[");
                    
                    unsigned int bound = length(t->getName().substr(2));
                    *out<<"        for (int j = 0; j < "<<bound<<"; j++) {\n";
                    //*out<<"            "<<t->getName().substr(2, tposition - 2)
                    *out<<"            void *memv = "
                        <<"((struct "<<typeName<<" *)v)->"
                        <<structMem[i]->getName() <<"; \n";
                    *out<<"             tmpposition += createMsg(("
                        <<t->getName().substr(2, tposition - 2)<<" *)memv + j, \""
                        <<t->getName().substr(2, tposition - 2)
                        <<"\", tmpposition);\n";
                    *out<<"        }\n";
                }              
            }
            *out<<"    } else ";
        }  
     
    }
    *out<<"{ }\n";
    *out<<"    return tmpposition - start;\n}\n\n";
    
 
} 




 
