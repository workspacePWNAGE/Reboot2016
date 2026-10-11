#include "stdafx.h"
#include "LANAdmission.h"
#include "../AccountTest.h"
#include "../AuthenticatedAvatar.h"
#include "network/Players.h"
#include "v8datamodel/ContentProvider.h"
#include "util/standardout.h"
#include <map>
#include <boost/thread/recursive_mutex.hpp>
#include <bcrypt.h>
#include <deque>
#include <vector>
using namespace RBX;
struct Admission {
    bool ready=false;
    LocalAccountSession account;
    boost::shared_ptr<AuthenticatedAvatar> avatar;
};
static boost::recursive_mutex admissionMutex;
static std::map<Network::ServerReplicator*,boost::shared_ptr<Admission>> admitted;
struct ExternalProof {LocalAccountSession account;ULONGLONG expires;};
static std::map<std::string,ExternalProof> externalProofs;
static boost::mutex proofMutex;
static std::deque<ULONGLONG> proofAttempts;
bool lanExternalMode(){const char* mode=std::getenv("XBOX_EXTERNAL_ADMISSION");return mode && std::string(mode)=="1";}
static std::string proofHash(const std::string& value) {
    BCRYPT_ALG_HANDLE algorithm=0;BCRYPT_HASH_HANDLE hash=0;DWORD size=0,received=0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,0,0)<0)throw std::runtime_error("Join proof hashing unavailable");
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("Join proof hashing unavailable");}
    std::vector<unsigned char> object(size);std::string digest(32,0);
    bool ok=BCryptCreateHash(algorithm,&hash,object.data(),size,0,0,0)>=0;
    if(ok)ok=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())),ULONG(value.size()),0)>=0 && BCryptFinishHash(hash,reinterpret_cast<PUCHAR>(&digest[0]),ULONG(digest.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok)throw std::runtime_error("Join proof hashing unavailable");return digest;
}
std::string lanIssueExternalTicket(const std::string& cookieToken) {
    if(!lanExternalMode())throw std::runtime_error("External admission is disabled");
    {boost::mutex::scoped_lock lock(proofMutex);ULONGLONG now=GetTickCount64();
        while(!proofAttempts.empty() && proofAttempts.front()+60000<=now)proofAttempts.pop_front();
        if(proofAttempts.size()>=60)throw std::runtime_error("Join validation rate limit reached; try later");
        proofAttempts.push_back(now);
    }
    LocalAccountSession account=validateExternalAccountToken(cookieToken);
    if(!account.token.empty())SecureZeroMemory(&account.token[0],account.token.size());account.token.clear();
    unsigned char random[43];if(BCryptGenRandom(0,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Join proof generator unavailable");
    const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";std::string ticket;
    for(unsigned char byte:random)ticket+=alphabet[byte&63];SecureZeroMemory(random,sizeof(random));
    {boost::mutex::scoped_lock lock(proofMutex);ULONGLONG now=GetTickCount64();
        for(auto it=externalProofs.begin();it!=externalProofs.end();)if(it->second.expires<=now)it=externalProofs.erase(it);else ++it;
        if(externalProofs.size()>=4096)throw std::runtime_error("Join proof capacity reached");
        externalProofs[proofHash(ticket)]={account,now+30000};
    }
    StandardOut::singleton()->printf(MESSAGE_INFO,"[Online] Issued one-use join proof for %s (%d)",account.username.c_str(),account.userId);
    return ticket;
}
static LocalAccountSession redeemExternalProof(const std::string& ticket) {
    if(ticket.size()!=43 || ticket.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)throw std::runtime_error("Invalid public join proof");
    boost::mutex::scoped_lock lock(proofMutex);auto found=externalProofs.find(proofHash(ticket));
    if(found==externalProofs.end())throw std::runtime_error("Unknown or already consumed public join proof");
    ExternalProof entry=found->second;externalProofs.erase(found);
    if(entry.expires<=GetTickCount64())throw std::runtime_error("Public join proof expired");
    return entry.account;
}
void lanAuthenticate(Network::ServerReplicator* replicator,int claimedId,const std::string& ticket) {boost::recursive_mutex::scoped_lock lock(admissionMutex);
    if(admitted.count(replicator)) throw std::runtime_error("Duplicate join proof");
    char limit[8]={};GetEnvironmentVariableA("LAN_SERVER_MAX_PLAYERS",limit,sizeof(limit));
    if(admitted.size()>=size_t(limit[0]?atoi(limit):20))throw std::runtime_error("Server player limit reached");
    StandardOut::singleton()->print(MESSAGE_INFO,"[LAN] Authenticating session");
    boost::shared_ptr<Admission> entry(new Admission);
    entry->account=lanExternalMode()?redeemExternalProof(ticket):redeemLANJoinTicket(ticket);
    if(entry->account.userId!=claimedId) throw std::runtime_error("Claimed UserId does not match authenticated account");
    for(const auto& existing:admitted) if(existing.second->account.userId==claimedId) throw std::runtime_error("Account is already connected");
    StandardOut::singleton()->printf(MESSAGE_INFO,"[LAN] Authenticated user: %s (%d)",entry->account.username.c_str(),claimedId);
    entry->avatar.reset(new AuthenticatedAvatar(entry->account));
    admitted[replicator]=entry;
}
bool lanAuthenticated(Network::ServerReplicator* replicator) {boost::recursive_mutex::scoped_lock lock(admissionMutex);return admitted.count(replicator)!=0;}
void lanPreparePlayer(Network::ServerReplicator* replicator,shared_ptr<Network::Player> player) {boost::recursive_mutex::scoped_lock lock(admissionMutex);
    auto found=admitted.find(replicator);
    if(found==admitted.end()) throw std::runtime_error("Player received before authentication");
    if(player->getUserID()!=found->second->account.userId) throw std::runtime_error("Network Player UserId mismatch");
    
    
    ServiceProvider::create<ContentProvider>(replicator)->setBaseUrl(found->second->account.assetOrigin);
    found->second->avatar->onPlayer(player);
    StandardOut::singleton()->printf(MESSAGE_INFO,"[LAN] Creating network Player\n[LAN] Player joined: %s\n[LAN] Replicating avatar",player->getName().c_str());
}
void lanRelease(Network::ServerReplicator* replicator) {boost::recursive_mutex::scoped_lock lock(admissionMutex);
    auto found=admitted.find(replicator);
    if(found!=admitted.end()) {
        StandardOut::singleton()->printf(MESSAGE_INFO,"[LAN] Player left: %s",found->second->account.username.c_str());
        admitted.erase(found);
    }
}


bool lanCanSend(const void* replicator) {boost::recursive_mutex::scoped_lock lock(admissionMutex);auto it=admitted.find((Network::ServerReplicator*)replicator);return it!=admitted.end() && it->second->ready;}
void lanSetReady(Network::ServerReplicator* replicator) {boost::recursive_mutex::scoped_lock lock(admissionMutex);admitted.at(replicator)->ready=true;}

std::string lanManagementPlayers() {
    boost::recursive_mutex::scoped_lock lock(admissionMutex);shared_ptr<Reflection::ValueArray> rows(new Reflection::ValueArray);
    for(const auto& entry:admitted)if(entry.second->ready){shared_ptr<Reflection::ValueTable> row(new Reflection::ValueTable);
        (*row)["user_id"]=entry.second->account.userId;(*row)["username"]=entry.second->account.username;
        rows->push_back(shared_ptr<const Reflection::ValueTable>(row));}
    std::string json;WebParser::writeJSON(Reflection::Variant(shared_ptr<const Reflection::ValueArray>(rows)),json);return json;
}
