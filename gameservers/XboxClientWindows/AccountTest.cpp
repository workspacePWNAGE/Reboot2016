#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <thread>
#include <fstream>
#include <iostream>
#include <vector>
// #include <stdexcept>

#include "AccountTest.h"
#include "DedicatedEndpoint.h"
#include "v8xml/WebParser.h"

namespace {
struct AccountHTTPError:std::runtime_error { DWORD status; AccountHTTPError(DWORD status,const std::string& message):std::runtime_error(message),status(status) {} };
using namespace RBX;
typedef shared_ptr<const Reflection::ValueTable> Json;
struct Internet {
    HINTERNET handle;
    explicit Internet(HINTERNET handle):handle(handle) { if(!handle) throw std::runtime_error("Account service connection failed"); }
    ~Internet() { WinHttpCloseHandle(handle); }
    Internet(const Internet&)=delete;
};
std::string utf8(const std::wstring& value) {
    if(value.empty()) return "";
    int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),0,0,0,0);
    if(!length) throw std::runtime_error("Invalid text encoding");
    std::string result(length,0);
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),&result[0],length,0,0);
    return result;
}
std::wstring wide(const std::string& value) {
    int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),0,0);
    std::wstring result(length,0);
    if(length) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),&result[0],length);
    return result;
}
void wipe(std::string& value) { if(!value.empty()) SecureZeroMemory(&value[0],value.size());value.clear(); }
std::string stringField(const Json& json,const char* key) {
    auto field=json->find(key);
    if(field==json->end() || !field->second.isType<std::string>()) throw std::runtime_error("Invalid account service response");
    return field->second.get<std::string>();
}
int idField(const Json& json) {
    auto field=json->find("user_id");
    if(field==json->end() || !field->second.isType<int>() || field->second.get<int>()<=0)
        throw std::runtime_error("Invalid local account user ID");
    return field->second.get<int>();
}
std::wstring baseUrl() {
    wchar_t url[2048]={};
    DWORD count=GetEnvironmentVariableW(L"XBOX_ACCOUNT_URL",url,2048);
    if(count>=2048) throw std::runtime_error("Account service URL is too long");
    return count?std::wstring(url):L"http://127.0.0.1:8765";
}
std::wstring catalogOrigin() {
    wchar_t value[2048]={};
    DWORD count=GetEnvironmentVariableW(L"LAN_CATALOG_URL",value,2048);
    if(count>=2048)throw std::runtime_error("Catalog URL is too long");
    if(!count) {
        DedicatedEndpoint endpoint=DedicatedEndpoint::environment();
        return L"http://"+wide(endpoint.host)+L":"+std::to_wstring(endpoint.port+1);
    }
    std::wstring origin(value,count);
    URL_COMPONENTS parts={};parts.dwStructSize=sizeof(parts);
    parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwUserNameLength=parts.dwPasswordLength=parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(origin.c_str(),0,0,&parts) || parts.dwUserNameLength || parts.dwPasswordLength || parts.dwExtraInfoLength ||
        (parts.dwUrlPathLength && std::wstring(parts.lpszUrlPath,parts.dwUrlPathLength)!=L"/"))
        throw std::runtime_error("Catalog URL must be an HTTP(S) origin without credentials/query/path");
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    if(parts.nScheme!=INTERNET_SCHEME_HTTPS && !(parts.nScheme==INTERNET_SCHEME_HTTP && (host==L"127.0.0.1" || host==L"localhost" || host==L"::1")))
        throw std::runtime_error("Explicit non-loopback catalog URLs require HTTPS");
    return origin;
}
Json request(const wchar_t* method,const wchar_t* path,std::string body="",const std::string& token="",const std::wstring& publicOrigin=L"",std::string* raw=0) {
    const std::wstring url=publicOrigin.empty()?baseUrl():publicOrigin;
    URL_COMPONENTS parts={};parts.dwStructSize=sizeof(parts);
    parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwUserNameLength=parts.dwPasswordLength=parts.dwExtraInfoLength=static_cast<DWORD>(-1);
    if(!WinHttpCrackUrl(url.c_str(),0,0,&parts)) throw std::runtime_error("Invalid account service URL");
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    bool secure=parts.nScheme==INTERNET_SCHEME_HTTPS;
    if((!secure && parts.nScheme!=INTERNET_SCHEME_HTTP) || parts.dwUserNameLength || parts.dwPasswordLength || parts.dwExtraInfoLength || parts.dwUrlPathLength>1)
        throw std::runtime_error("Account URL must be an HTTP(S) origin without credentials/query/path");
    if(!secure && publicOrigin.empty() && host!=L"127.0.0.1" && host!=L"localhost" && host!=L"::1")
        throw std::runtime_error("Non-loopback account services require HTTPS");
    Internet session(WinHttpOpen(L"XboxClientWindowsAccountTest/1",WINHTTP_ACCESS_TYPE_NO_PROXY,0,0,0));
    WinHttpSetTimeouts(session.handle,2000,3000,8000,8000);
    Internet connection(WinHttpConnect(session.handle,host.c_str(),parts.nPort,0));
    Internet call(WinHttpOpenRequest(connection.handle,method,path,0,0,WINHTTP_DEFAULT_ACCEPT_TYPES,secure?WINHTTP_FLAG_SECURE:0));
    DWORD noRedirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if(!WinHttpSetOption(call.handle,WINHTTP_OPTION_REDIRECT_POLICY,&noRedirect,sizeof(noRedirect)))
        throw std::runtime_error("Unable to enforce account redirect policy");
    std::wstring headers=L"Content-Type: application/json\r\nAccept: application/json\r\n";
    if(!token.empty()) headers+=L"Authorization: Bearer "+wide(token)+L"\r\n";
    BOOL sent=WinHttpSendRequest(call.handle,headers.c_str(),static_cast<DWORD>(headers.size()),
        body.empty()?WINHTTP_NO_REQUEST_DATA:&body[0],static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0);
    wipe(body);
    if(!sent || !WinHttpReceiveResponse(call.handle,0)) throw std::runtime_error("Account service unavailable; start AccountServer\\Start.ps1");
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(call.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))
        throw std::runtime_error("Invalid HTTP account response");
    std::string response;
    char buffer[4096];DWORD read=0;
    do {
        if(!WinHttpReadData(call.handle,buffer,sizeof(buffer),&read)) throw std::runtime_error("Account response could not be read");
        if(response.size()+read>(raw?4194304U:publicOrigin.empty()?131072U:1048576U)) throw std::runtime_error("Service response exceeds size limit");
        response.append(buffer,read);
    } while(read);
    if(raw){if(status!=200)throw std::runtime_error("Host image unavailable");*raw=response;return Json();}
    Json json;
    if(status==204) return Json(new Reflection::ValueTable);
    if(!WebParser::parseJSONTable(response,json) || !json) throw std::runtime_error("Invalid JSON account response");
    if(status<200 || status>=300) {
        auto error=json->find("error");
        if(error!=json->end() && error->second.isType<Json>())
            throw AccountHTTPError(status,stringField(error->second.get<Json>(),"message"));
        
        
        if(status==400 && error!=json->end() && error->second.isType<std::string>()
            && error->second.get<std::string>()=="Please complete the captcha!")
            throw AccountHTTPError(status,"Captcha verification required.");
        
        throw AccountHTTPError(status,"Account request rejected");
    }
    return json;
}
std::string credentialsBody(const std::string& username,const std::string& password) {
    shared_ptr<Reflection::ValueTable> values(new Reflection::ValueTable);
    (*values)["username"]=username;(*values)["password"]=password;
    Reflection::Variant json=shared_ptr<const Reflection::ValueTable>(values);
    std::string result;
    if(!WebParser::writeJSON(json,result)) throw std::runtime_error("Could not encode account request");
    return result;
}
typedef LocalAccountSession Account;
Account authenticate(bool registerFirst,const std::string& username,const std::string& password) {
    if(registerFirst) request(L"POST",L"/v1/auth/register",credentialsBody(username,password));
    Json login=request(L"POST",L"/v1/auth/login",credentialsBody(username,password));
    Account account;
    account.assetOrigin=utf8(baseUrl());
    account.userId=idField(login);account.username=stringField(login,"username");account.token=stringField(login,"access_token");
    if(account.token.size()<40 || account.token.size()>128 || account.token.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::runtime_error("Invalid account session token");
    try {
        Json profile=request(L"GET",L"/v1/me","",account.token);
        if(idField(profile)!=account.userId || stringField(profile,"username")!=account.username)
            throw std::runtime_error("Account profile identity mismatch");
        account.displayName=stringField(profile,"display_name");account.avatarName=stringField(profile,"avatar_name");
        if(!WebParser::writeJSON(Reflection::Variant(profile),account.profileJson))
            throw std::runtime_error("Could not encode local account profile");
    } catch(...) { try { request(L"POST",L"/v1/auth/logout","",account.token); } catch(...) {} wipe(account.token);throw; }
    return account;
}
std::string randomHex(size_t count) {
    unsigned char bytes[32];
    if(count>sizeof(bytes) || BCryptGenRandom(0,bytes,static_cast<ULONG>(count),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
        throw std::runtime_error("Secure random generator failed");
    const char* digits="0123456789abcdef";std::string result;
    for(size_t i=0;i<count;i++){ result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15]; }
    SecureZeroMemory(bytes,sizeof(bytes));return result;
}
struct Result { Account account; std::string error; bool logout=false; };
struct WindowState {
    HWND window=0,username=0,password=0,registerButton=0,loginButton=0,logoutButton=0,summary=0,continueButton=0;
    std::thread worker;
    bool busy=false,closing=false,automated=false,passed=false,menuMode=false,continueToMenu=false;
    Account account;
};
void enable(WindowState* s) {
    if(s->continueButton) EnableWindow(s->continueButton,!s->busy && !s->account.token.empty());
    EnableWindow(s->registerButton,!s->busy && s->account.token.empty());
    EnableWindow(s->loginButton,!s->busy && s->account.token.empty());
    EnableWindow(s->logoutButton,!s->busy && !s->account.token.empty());
    EnableWindow(s->username,!s->busy && s->account.token.empty());
    EnableWindow(s->password,!s->busy && s->account.token.empty());
}
std::string editText(HWND control) {
    int length=GetWindowTextLengthW(control);
    std::wstring value(length+1,0);GetWindowTextW(control,&value[0],length+1);value.resize(length);
    std::string result=utf8(value);if(!value.empty())SecureZeroMemory(&value[0],value.size()*sizeof(wchar_t));return result;
}
void start(WindowState* s,int operation) {
    if(s->busy) return;
    if(s->worker.joinable()) s->worker.join();
    std::string username,password;
    if(operation!=3) {
        username=editText(s->username);password=editText(s->password);
        if(s->automated) {
            wchar_t testUser[64]={},testPassword[129]={};
            DWORD userLength=GetEnvironmentVariableW(L"XBOX_ACCOUNT_TEST_USERNAME",testUser,64);
            DWORD passwordLength=GetEnvironmentVariableW(L"XBOX_ACCOUNT_TEST_PASSWORD",testPassword,129);
            if(userLength && userLength<64 && passwordLength && passwordLength<129) {
                username=utf8(testUser);password=utf8(testPassword);operation=2;
            } else {username="Native_"+randomHex(8);password=randomHex(24);}
            SecureZeroMemory(testPassword,sizeof(testPassword));
            SetEnvironmentVariableW(L"XBOX_ACCOUNT_TEST_PASSWORD",0);
            SetEnvironmentVariableW(L"XBOX_ACCOUNT_TEST_USERNAME",0);
        }
        SetWindowTextW(s->password,L"");
    }
    s->busy=true;enable(s);SetWindowTextW(s->summary,L"Contacting local account service...");
    std::string token=s->account.token;
    s->worker=std::thread([s,operation,username,password,token]() mutable {
        Result* result=new Result;
        result->logout=operation==3;
        try {
            if(operation==3) { request(L"POST",L"/v1/auth/logout","",token);result->logout=true; }
            else result->account=authenticate(operation==1,username,password);
        } catch(const std::exception& e) { result->error=e.what(); }
        wipe(password);wipe(token);
        PostMessageW(s->window,WM_APP+1,0,reinterpret_cast<LPARAM>(result));
    });
    wipe(password);wipe(token);
}
LRESULT CALLBACK accountProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    WindowState* s=reinterpret_cast<WindowState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        s=static_cast<WindowState*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        s->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
    }
    if(!s) return DefWindowProcW(window,message,w,l);
    switch(message) {
    case WM_COMMAND:
        if(LOWORD(w)==4 && HIWORD(w)==BN_CLICKED && !s->busy && !s->account.token.empty()) {
            s->continueToMenu=true;DestroyWindow(window);return 0;
        }
        try { if(LOWORD(w)>=1 && LOWORD(w)<=3 && HIWORD(w)==BN_CLICKED) start(s,LOWORD(w)); }
        catch(const std::exception& e){SetWindowTextW(s->summary,wide(e.what()).c_str());}
        return 0;
    case WM_APP+1: {
        if(s->worker.joinable()) s->worker.join();s->busy=false;
        Result* result=reinterpret_cast<Result*>(l);
        if(!result->error.empty()) {
            SetWindowTextW(s->summary,wide("Account test: "+result->error).c_str());
            if(s->automated){std::ofstream log("AccountTest.log",std::ios::trunc);log<<"FAIL: account registration/login/profile request did not complete."<<std::endl;}
        }
        else if(result->logout) {wipe(s->account.token);s->account=Account();SetWindowTextW(s->summary,L"Signed out; server token revoked.");}
        else {
            s->account=result->account;wipe(result->account.token);
            std::string summary="Logged in: "+s->account.username+"\r\nLocal user ID: "+std::to_string(s->account.userId)+
                "\r\nDisplay name: "+s->account.displayName+"\r\nAvatar name: "+s->account.avatarName+
                "\r\nProfile retrieved successfully.\r\nSession token received (kept in memory; not displayed).\r\nThis is a local service account, not Xbox Live or Roblox authentication.";
            SetWindowTextW(s->summary,wide(summary).c_str());s->passed=true;
            if(s->automated){std::ofstream log("AccountTest.log",std::ios::trunc);log<<"PASS: registered, logged in, received user ID/session token, fetched matching profile.\nLocal user ID: "<<s->account.userId<<"\nUsername: "<<s->account.username<<std::endl;}
        }
        if(s->menuMode && s->automated && s->passed && !s->closing && !result->logout && result->error.empty())
            SetTimer(window,1,1500,0);
        const bool logoutFinished=result->logout;
        delete result;enable(s);
        if(s->closing) {
            if(!s->account.token.empty() && !logoutFinished) start(s,3);
            else DestroyWindow(window);
        }
        return 0;
    }
    case WM_TIMER:
        KillTimer(window,1);SendMessageW(window,WM_COMMAND,MAKEWPARAM(4,BN_CLICKED),0);return 0;
    case WM_CLOSE:
        s->closing=true;
        if(s->busy) return 0;
        if(!s->account.token.empty()){start(s,3);return 0;}
        DestroyWindow(window);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
HWND control(HWND parent,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id=0) {
    HWND result=CreateWindowW(type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,parent,reinterpret_cast<HMENU>(id),GetModuleHandleW(0),0);
    SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return result;
}
}
static int runAccountWindow(HINSTANCE instance,bool automated,LocalAccountSession* output) {
    WindowState state;state.automated=automated;state.menuMode=output!=0;
    if(automated){std::ofstream log("AccountTest.log",std::ios::trunc);log<<"RUNNING: native account service test."<<std::endl;}
    WNDCLASSW type={};type.lpfnWndProc=accountProc;type.hInstance=instance;type.lpszClassName=L"XboxLocalAccountTest";
    type.hCursor=LoadCursor(0,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&type);
    HWND window=CreateWindowW(type.lpszClassName,output?L"XboxClientWindows - sign in to the Xbox menu":L"XboxClientWindows - local account/profile test",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,740,530,0,0,instance,&state);
    if(!window) return 1;
    control(window,L"STATIC",L"Local account service (default: 127.0.0.1:8765). Start AccountServer first.",0,20,16,690,25);
    control(window,L"STATIC",L"Username (3-32 letters, digits or underscore):",0,20,55,330,22);
    state.username=control(window,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,355,52,335,26);
    control(window,L"STATIC",L"Password (12-128 characters):",0,20,95,330,22);
    state.password=control(window,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_PASSWORD|ES_AUTOHSCROLL,355,92,335,26);
    SendMessageW(state.username,EM_SETLIMITTEXT,32,0);SendMessageW(state.password,EM_SETLIMITTEXT,128,0);
    state.registerButton=control(window,L"BUTTON",L"Register + sign in",WS_TABSTOP,20,135,190,32,1);
    state.loginButton=control(window,L"BUTTON",L"Sign in",WS_TABSTOP,225,135,150,32,2);
    state.logoutButton=control(window,L"BUTTON",L"Sign out",WS_TABSTOP,390,135,150,32,3);
    state.summary=control(window,L"EDIT",L"Create a local account, or sign in to an existing one.\r\nPasswords and tokens are never written by this client.",
        WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL,20,190,670,220);
    if(output) state.continueButton=control(window,L"BUTTON",L"Continue to Xbox menu",WS_TABSTOP,20,425,260,32,4);
    enable(&state);ShowWindow(window,SW_SHOW);SetFocus(state.username);
    if(automated) PostMessageW(window,WM_COMMAND,MAKEWPARAM(1,BN_CLICKED),0);
    MSG message;
    while(GetMessageW(&message,0,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    if(state.worker.joinable()) state.worker.join();
    if(output && state.continueToMenu) *output=state.account;
    wipe(state.account.token);
    return automated && !state.passed?1:0;
}

int runAccountTest(HINSTANCE instance,bool automated) { return runAccountWindow(instance,automated,0); }
bool loginForXboxMenu(HINSTANCE instance,bool automated,LocalAccountSession& output) {
    runAccountWindow(instance,automated,&output);return output.userId>0 && !output.token.empty();
}
bool logoutLocalAccount(LocalAccountSession& account) {
    if(account.external) {wipe(account.token);account=LocalAccountSession();return false;}
    bool revoked=true;
    if(!account.token.empty()) {
        try { request(L"POST",L"/v1/auth/logout","",account.token); } catch(...) { revoked=false; }
    }
    wipe(account.token);account=LocalAccountSession();return revoked;
}
std::string accountProfileModule(const LocalAccountSession& account) {
    if(account.userId<=0) return "return {Enabled=false}";
    
    std::string quoted="\"";
    for(unsigned char byte:account.profileJson) {
        char escape[5];sprintf_s(escape,"\\%03u",static_cast<unsigned int>(byte));quoted+=escape;
    }
    quoted+="\"";
    return "return {Enabled=true, Profile=game:GetService('HttpService'):JSONDecode("+quoted+")}";
}

std::string issueLANJoinTicket(const LocalAccountSession& account) {
    std::wstring origin=account.external?catalogOrigin():L"";
    if(account.external) {
        URL_COMPONENTS parts={};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=DWORD(-1);
        if(!WinHttpCrackUrl(origin.c_str(),0,0,&parts))throw std::runtime_error("Invalid join ticket origin");
        std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
        if(parts.nScheme!=INTERNET_SCHEME_HTTPS && host!=L"127.0.0.1" && host!=L"localhost" && host!=L"::1")throw std::runtime_error("Public account tokens require an HTTPS join ticket origin");
    }
    std::string ticket=stringField(request(L"POST",L"/v1/lan/ticket","",account.token,origin),"ticket");
    if(ticket.size()!=43 || ticket.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::runtime_error("Invalid LAN join proof");
    return ticket;
}
LocalAccountSession redeemLANJoinTicket(const std::string& ticket) {
    if(ticket.size()!=43 || ticket.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::runtime_error("Invalid LAN join proof");
    Json profile=request(L"POST",L"/v1/lan/redeem","",ticket);
    LocalAccountSession account;
    account.userId=idField(profile);account.username=stringField(profile,"username");
    account.displayName=stringField(profile,"display_name");account.avatarName=stringField(profile,"avatar_name");
    account.assetOrigin=utf8(baseUrl());
    if(!WebParser::writeJSON(Reflection::Variant(profile),account.profileJson)) throw std::runtime_error("Invalid authoritative profile");
    return account;
}
boost::shared_ptr<const RBX::Reflection::ValueTable> accountLibraryRequest(const LocalAccountSession& account,const wchar_t* method,const std::wstring& path,const std::string& body) {
    if(account.external)throw std::runtime_error("This profile/outfit operation is not supported by the external backend yet");
    if(account.userId<=0 || account.token.empty())throw std::runtime_error("Game history requires login");
    return request(method,path.c_str(),body,account.token);
}
boost::shared_ptr<const RBX::Reflection::ValueTable> fetchLANLibrary() {
    return request(L"GET",L"/catalog","","",catalogOrigin());
}
void verifyLANAccountService() {
    std::wstring url=baseUrl();URL_COMPONENTS parts={};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.c_str(),0,0,&parts))throw std::runtime_error("Invalid account origin");
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    if(host!=L"127.0.0.1" && host!=L"localhost" && host!=L"::1")throw std::runtime_error("Dedicated server account redemption requires a loopback account origin");
    Json health=request(L"GET",L"/health");
    if(stringField(health,"status")!="ok" || stringField(health,"service")!="local-accounts")throw std::runtime_error("Account service is not ready");
}
std::string fetchLANLibraryImage(int gameId,const std::string& name) {
    if(gameId<=0 || (name!="thumbnail.png" && name!="image1.png" && name!="image2.png"))throw std::runtime_error("Invalid host image key");
    std::string bytes;std::wstring path=L"/image/"+std::to_wstring(gameId)+L"/"+wide(name);
    request(L"GET",path.c_str(),"","",catalogOrigin(),&bytes);return bytes;
}

namespace {
std::wstring externalOrigin() {
    wchar_t value[2048]={};DWORD n=GetEnvironmentVariableW(L"XBOX_R16_URL",value,2048);
    if(n>=2048)throw std::runtime_error("External origin too long");
    std::wstring origin=n?value:L"https://r16.noli.vip";
    URL_COMPONENTS parts={};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=DWORD(-1);
    if(!WinHttpCrackUrl(origin.c_str(),0,0,&parts))throw std::runtime_error("Invalid external origin");
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    if(parts.nScheme!=INTERNET_SCHEME_HTTPS && !(parts.nScheme==INTERNET_SCHEME_HTTP && (host==L"127.0.0.1" || host==L"localhost" || host==L"::1")))
        throw std::runtime_error("External accounts require HTTPS");
    return origin;
}
int externalInteger(const Json& table,const char* key,int maximum) {
    auto found=table->find(key);
    if(found==table->end() || !found->second.isType<int>())throw std::runtime_error("Invalid external profile");
    int value=found->second.get<int>();if(value<0 || value>maximum)throw std::runtime_error("Invalid external profile");return value;
}
Json externalTable(const Json& table,const char* key) {
    auto found=table->find(key);if(found==table->end() || !found->second.isType<Json>())throw std::runtime_error("Invalid external profile");return found->second.get<Json>();
}
LocalAccountSession normalizeExternal(const Json& response) {
    LocalAccountSession account;account.external=true;account.assetOrigin=utf8(externalOrigin());
    account.token=stringField(response,"cookieToken");
    if(account.token.size()!=64 || account.token.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid external token response");
    Json user=externalTable(response,"user");account.userId=externalInteger(user,"id",2147483647);
    account.username=stringField(user,"username");
    if(account.userId<=0 || account.username.size()<3 || account.username.size()>20 || account.username.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)
        throw std::runtime_error("Invalid external identity");
    account.displayName=account.avatarName=account.username;
    Json avatar=externalTable(user,"avatar"),colors=externalTable(avatar,"bodycolors");
    
    shared_ptr<Reflection::ValueTable> body(new Reflection::ValueTable);
    const char* parts[]={"head","torso","left_arm","right_arm","left_leg","right_leg"};
    for(auto part:parts)(*body)[part]=externalInteger(colors,part,1032);
    shared_ptr<Reflection::ValueTable> clothing(new Reflection::ValueTable);
    for(auto slot:{"shirt","pants"}) {int id=externalInteger(avatar,slot,2147483647);if(id)(*clothing)[slot]=id;}
    shared_ptr<Reflection::ValueArray> hats(new Reflection::ValueArray);
    for(auto slot:{"hat1","hat2","hat3"}) {int id=externalInteger(avatar,slot,2147483647);if(id)hats->push_back(id);}
    shared_ptr<Reflection::ValueTable> appearance(new Reflection::ValueTable);
    (*appearance)["rig_type"]=std::string("R6");(*appearance)["body_colors"]=Json(body);(*appearance)["clothing"]=Json(clothing);
    (*appearance)["accessories"]=shared_ptr<const Reflection::ValueArray>(hats);
    (*appearance)["equipped_assets"]=shared_ptr<const Reflection::ValueArray>(new Reflection::ValueArray);
    shared_ptr<Reflection::ValueTable> profile(new Reflection::ValueTable);
    (*profile)["schema_version"]=1;(*profile)["profile_version"]=1;(*profile)["user_id"]=account.userId;
    (*profile)["username"]=account.username;(*profile)["display_name"]=account.displayName;(*profile)["avatar_name"]=account.avatarName;
    (*profile)["appearance"]=Json(appearance);(*profile)["settings"]=Json(new Reflection::ValueTable);
    (*profile)["inventory"]=shared_ptr<const Reflection::ValueArray>(new Reflection::ValueArray);
    if(!WebParser::writeJSON(Reflection::Variant(Json(profile)),account.profileJson))throw std::runtime_error("Invalid normalized profile");
    return account;
}
int loginFailureCode(const std::exception& error) {
    const AccountHTTPError* http=dynamic_cast<const AccountHTTPError*>(&error);
    if(http && http->status==429)return 7;
    if(http && http->status==409)return 16;
    if(http && http->status==400 && std::string(http->what())=="Captcha verification required.")return 18;
    if(http && http->status==400)return 19;
    if(http && http->status==401)return 14;
    return 5;
}
}
int authenticateOriginalLogin(const std::string& username,const std::string& password,bool external,LocalAccountSession& output,bool registerFirst) {
    try {
        if(registerFirst && external)request(L"POST",L"/api/register-user",credentialsBody(username,password),"",externalOrigin());
        LocalAccountSession result=external?normalizeExternal(request(L"POST",L"/api/login-user",credentialsBody(username,password),"",externalOrigin())):authenticate(registerFirst,username,password);
        output=result;return 0;
    }catch(const std::exception& e){return loginFailureCode(e);}
}
int validateOriginalLogin(LocalAccountSession& account) {
    if(account.userId<=0 || account.token.empty())return 2;
    try {
        if(account.external) {
            shared_ptr<Reflection::ValueTable> body(new Reflection::ValueTable);(*body)["token"]=account.token;
            std::string json;WebParser::writeJSON(Reflection::Variant(Json(body)),json);
            LocalAccountSession current=normalizeExternal(request(L"POST",L"/api/login-user",json,"",externalOrigin()));
            if(current.userId!=account.userId || current.username!=account.username)throw std::runtime_error("External identity changed");
            account=current;
        }else {
            Json current=request(L"GET",L"/v1/me","",account.token);
            if(idField(current)!=account.userId || stringField(current,"username")!=account.username)throw std::runtime_error("Account identity changed");
        }
        return 0;
    }catch(const std::exception& e){return loginFailureCode(e);}
}
LocalAccountSession validateExternalAccountToken(const std::string& token) {
    if(token.size()!=64 || token.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("Invalid external session token");
    shared_ptr<Reflection::ValueTable> body(new Reflection::ValueTable);(*body)["token"]=token;
    std::string json;WebParser::writeJSON(Reflection::Variant(Json(body)),json);
    return normalizeExternal(request(L"POST",L"/api/login-user",json,"",externalOrigin()));
}
