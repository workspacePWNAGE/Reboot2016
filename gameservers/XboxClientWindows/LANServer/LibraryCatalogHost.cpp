#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>
#include "LibraryCatalogHost.h"
#include "LANAdmission.h"
#include <algorithm>
#include <cctype>
#include "util/standardout.h"
using namespace RBX;
LibraryCatalogHost::LibraryCatalogHost(shared_ptr<GameLibrary> library,int activeGame,const std::string& address,int gamePort):stop(false),listener(INVALID_SOCKET) {
    if(gamePort<1 || gamePort>=65535)throw std::runtime_error("LAN game port must leave room for catalog port");
    WSADATA data;WSAStartup(MAKEWORD(2,2),&data);
    listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sockaddr_in bindAddress={};bindAddress.sin_family=AF_INET;bindAddress.sin_port=htons(static_cast<u_short>(gamePort+1));
    if(inet_pton(AF_INET,address.c_str(),&bindAddress.sin_addr)!=1 || bind(listener,reinterpret_cast<sockaddr*>(&bindAddress),sizeof(bindAddress)) || listen(listener,8)) {
        closesocket(listener);WSACleanup();throw std::runtime_error("LAN catalog bind failed");
    }
    worker=boost::thread([this,library,activeGame]() {
        HRESULT apartment=CoInitializeEx(0,COINIT_MULTITHREADED);
        while(!stop) {
            fd_set readable;FD_ZERO(&readable);FD_SET(listener,&readable);timeval timeout={0,100000};
            if(select(0,&readable,0,0,&timeout)<=0)continue;
            sockaddr_in peer={};int peerSize=sizeof(peer);
            SOCKET client=accept(listener,reinterpret_cast<sockaddr*>(&peer),&peerSize);if(client==INVALID_SOCKET)continue;
            DWORD readTimeout=2000;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&readTimeout),sizeof(readTimeout));
            setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<char*>(&readTimeout),sizeof(readTimeout));
            char buffer[4096];std::string request,body,type="application/json",status="HTTP/1.1 400 Bad Request\r\n";bool ok=false;
            while(request.find("\r\n\r\n")==std::string::npos && request.size()<8192) {
                int n=recv(client,buffer,sizeof(buffer),0);if(n<=0)break;
                request.append(buffer,n);SecureZeroMemory(buffer,sizeof(buffer));
            }
            bool complete=request.size()<=8192 && request.find("\r\n\r\n")!=std::string::npos;
            if(complete && (request.compare(0,30,"POST /v1/lan/ticket HTTP/1.1\r\n")==0 || request.compare(0,30,"POST /v1/lan/ticket HTTP/1.0\r\n")==0)) {
                status="HTTP/1.1 401 Unauthorized\r\n";
                std::string token;bool valid=lanExternalMode() && ntohl(peer.sin_addr.s_addr)==INADDR_LOOPBACK;
                size_t cursor=request.find("\r\n")+2;
                while(cursor<request.size()) {
                    size_t end=request.find("\r\n",cursor);if(end==std::string::npos || end==cursor)break;
                    std::string line=request.substr(cursor,end-cursor);size_t colon=line.find(':');
                    if(colon==std::string::npos){valid=false;break;}
                    std::string name=line.substr(0,colon),value=line.substr(colon+1);
                    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
                    size_t first=value.find_first_not_of(" \t"),last=value.find_last_not_of(" \t");value=first==std::string::npos?"":value.substr(first,last-first+1);
                    if(name=="authorization") {if(!token.empty() || value.compare(0,7,"Bearer ")!=0)valid=false;else token=value.substr(7);}
                    if(name=="transfer-encoding" || (name=="content-length" && value!="0"))valid=false;
                    if(!value.empty())SecureZeroMemory(&value[0],value.size());if(!line.empty())SecureZeroMemory(&line[0],line.size());
                    cursor=end+2;
                }
                if(valid) {
                    try {std::string ticket=lanIssueExternalTicket(token);body="{\"ticket\":\""+ticket+"\",\"expires_in\":30}";ok=true;}
                    catch(const std::exception& error) {StandardOut::singleton()->printf(MESSAGE_WARNING,"[Online] Join validation failed: %s",error.what());}
                }
                if(!ok)body="{\"error\":{\"message\":\"Session validation failed or join validation capacity unavailable\"}}";
                if(!token.empty())SecureZeroMemory(&token[0],token.size());
            }else if(complete && (request.compare(0,23,"GET /catalog HTTP/1.1\r\n")==0 || request.compare(0,23,"GET /catalog HTTP/1.0\r\n")==0)) {
                try {
                    auto catalog=library->catalog();shared_ptr<Reflection::ValueTable> data(new Reflection::ValueTable(*catalog));
                    (*data)["active_game_id"]=activeGame;body=libraryJSON(shared_ptr<const Reflection::ValueTable>(data));ok=true;
                }catch(const std::exception&){body="{\"error\":{\"message\":\"Host catalog unavailable\"}}";}
            }else if(complete && request.compare(0,11,"GET /image/")==0) {
                try {
                    size_t slash=request.find('/',11),end=request.find(' ',slash);
                    std::string digits=request.substr(11,slash-11),name=request.substr(slash+1,end-slash-1);
                    if(digits.empty() || digits.size()>10 || digits.find_first_not_of("0123456789")!=std::string::npos || slash==std::string::npos || end==std::string::npos)throw std::runtime_error("Invalid image key");
                    int id=std::stoi(digits);library->resolve(id);body=library->imageBytes(id,name);type="image/png";ok=true;
                }catch(const std::exception&){body="{\"error\":{\"message\":\"Game image unavailable\"}}";}
            }else body="{\"error\":{\"message\":\"Unsupported catalog request\"}}";
            if(!request.empty())SecureZeroMemory(&request[0],request.size());
            std::string response=std::string(ok?"HTTP/1.1 200 OK\r\n":status)+
                "Content-Type: "+type+"\r\nConnection: close\r\nCache-Control: no-store\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
            size_t sent=0;while(sent<response.size()){int count=send(client,response.data()+sent,int(response.size()-sent),0);if(count<=0)break;sent+=count;}
            closesocket(client);
        }
        if(SUCCEEDED(apartment))CoUninitialize();
    });
}
LibraryCatalogHost::~LibraryCatalogHost(){stop=true;if(worker.joinable())worker.join();if(listener!=INVALID_SOCKET)closesocket(listener);WSACleanup();}
