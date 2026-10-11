#pragma once
#include <windows.h>
#include <string>
#include "v8xml/WebParser.h"

struct LocalAccountSession {
    int userId=0;
    bool external=false;
    std::string username,displayName,avatarName,profileJson,assetOrigin,token;
};
int authenticateOriginalLogin(const std::string& username,const std::string& password,bool external,LocalAccountSession& output,bool registerFirst=false);
int validateOriginalLogin(LocalAccountSession& account);
LocalAccountSession validateExternalAccountToken(const std::string& token);
int runAccountTest(HINSTANCE instance, bool automated);
bool loginForXboxMenu(HINSTANCE instance, bool automated, LocalAccountSession& output);
std::string accountProfileModule(const LocalAccountSession& account);
bool logoutLocalAccount(LocalAccountSession& account);

std::string issueLANJoinTicket(const LocalAccountSession& account);
LocalAccountSession redeemLANJoinTicket(const std::string& ticket);
boost::shared_ptr<const RBX::Reflection::ValueTable> accountLibraryRequest(
    const LocalAccountSession& account,const wchar_t* method,const std::wstring& path,const std::string& body="");
boost::shared_ptr<const RBX::Reflection::ValueTable> fetchLANLibrary();
std::string fetchLANLibraryImage(int gameId,const std::string& name);
void verifyLANAccountService();
