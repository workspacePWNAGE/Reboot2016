#pragma once
#include <string>
#include <stdexcept>
#include <cstdlib>



struct DedicatedEndpoint {
    std::string host;
    int port;
    static bool validHost(const std::string& value) {
        if(value.empty() || value.size()>253) return false;
        size_t start=0;
        for(size_t i=0;i<=value.size();++i) {
            if(i==value.size() || value[i]=='.') {
                size_t size=i-start;
                if(!size || size>63 || value[start]=='-' || value[i-1]=='-') return false;
                start=i+1;
            } else {
                unsigned char c=value[i];
                if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-')) return false;
            }
        }
        
        if(value.find_first_not_of("0123456789.")==std::string::npos) {
            int parts=0;start=0;
            for(size_t i=0;i<=value.size();++i) if(i==value.size() || value[i]=='.') {
                if(i-start>3 || (i-start>1 && value[start]=='0')) return false;
                int octet=atoi(value.substr(start,i-start).c_str());
                if(octet>255) return false;
                ++parts;start=i+1;
            }
            if(parts!=4) return false;
        }
        return true;
    }
    static DedicatedEndpoint parse(std::string host,std::string port) {
        DedicatedEndpoint endpoint;
        endpoint.host=host.empty()?"127.0.0.1":host;
        if(!validHost(endpoint.host)) throw std::runtime_error("Game server address must be an IPv4 address or DNS hostname (without scheme, path or port)");
        if(port.empty())port="53640";
        if(port.size()>5 || port.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid game server port");
        endpoint.port=atoi(port.c_str());
        
        if(endpoint.port<1 || endpoint.port>65534)throw std::runtime_error("Game server port must be 1..65534; the catalog uses the next TCP port");
        return endpoint;
    }
    static DedicatedEndpoint environment() {
        const char* host=std::getenv("LAN_SERVER_ADDRESS");
        const char* port=std::getenv("LAN_SERVER_PORT");
        return parse(host?host:"",port?port:"");
    }
};
