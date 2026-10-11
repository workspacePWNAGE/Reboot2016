#include "stdafx.h"
#include <objbase.h>
#include "Server.h"
#include "network/api.h"
#include "v8datamodel/Game.h"
#include "v8datamodel/ContentProvider.h"
#include "v8datamodel/StarterPlayerService.h"
#include "v8datamodel/PlayerScripts.h"
#include "v8datamodel/Workspace.h"
#include "script/ScriptContext.h"
#include "util/RunStateOwner.h"
#include "util/standardout.h"
#include "v8xml/Serializer.h"
#include "FastLog.h"
#include "v8datamodel/PlatformService.h"
#include "v8datamodel/GameSettings.h"
#include <fstream>
#include <atomic>
#include "../GameLibrary.h"
#include "LibraryCatalogHost.h"
#include "LANAdmission.h"
#include <ws2tcpip.h>
#include <iostream>
using namespace RBX;
RBX_REGISTER_CLASS(PlatformService);
RBX_REGISTER_ENUM(XboxKeyBoardType);
RBX_REGISTER_ENUM(VoiceChatState);
static Reflection::BoundProp<float> previewOverscanX("OverscanPX", "Video", &GameSettings::overscanPX);
static Reflection::BoundProp<float> previewOverscanY("OverscanPY", "Video", &GameSettings::overscanPY);
static std::atomic<bool> stopping(false);
static std::ofstream logFile;
static BOOL WINAPI stopHandler(DWORD) {stopping=true;return TRUE;}
static void logMessage(const StandardOutMessage& message) {logFile<<message.message<<std::endl;}
static void syncFlag(const std::string& name,const std::string& value,void*) {FLog::SetValueFromServer(name,value);}
static int managementPort=53640,managementLimit=20;
static std::string managementName="Reboot 16 LAN",managementBind="127.0.0.1";
static std::wstring statusPath;
static std::string envValue(const char* key,const char* fallback) {
    char text[256]={};DWORD n=GetEnvironmentVariableA(key,text,sizeof(text));
    if(n>=sizeof(text))throw std::runtime_error(std::string("Configuration too long: ")+key);
    return n?text:fallback;
}
static int integerConfig(const char* key,int fallback,int maximum) {
    std::string value=envValue(key,std::to_string(fallback).c_str());
    if(value.empty() || value.size()>10 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error(std::string("Invalid integer: ")+key);
    unsigned long number=std::stoul(value);if(!number || number>unsigned(maximum))throw std::runtime_error(std::string("Out of range: ")+key);return int(number);
}
static void managementStatus(const char* state,const std::string& error="",bool accountReady=true) {
    if(statusPath.empty())return;
    shared_ptr<Reflection::ValueTable> value(new Reflection::ValueTable);
    (*value)["state"]=std::string(state);(*value)["error"]=error;(*value)["pid"]=int(GetCurrentProcessId());
    (*value)["server_name"]=managementName;(*value)["bind_address"]=managementBind;
    (*value)["port"]=managementPort;(*value)["catalog_port"]=managementPort+1;(*value)["max_players"]=managementLimit;
    (*value)["account_ready"]=accountReady;(*value)["players_json"]=lanManagementPlayers();
    std::wstring temporary=statusPath+L".tmp";
    {std::ofstream output(temporary,std::ios::binary);output<<libraryJSON(shared_ptr<const Reflection::ValueTable>(value));}
    MoveFileExW(temporary.c_str(),statusPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
int main() {
    CoInitializeEx(0,COINIT_MULTITHREADED);SetConsoleCtrlHandler(stopHandler,TRUE);
    char exe[MAX_PATH];GetModuleFileNameA(0,exe,MAX_PATH);
    std::string folder(exe);folder=folder.substr(0,folder.find_last_of("\\/"));
    SetCurrentDirectoryA(folder.c_str());
    logFile.open("LANServer.log",std::ios::trunc);
    rbx::signals::scoped_connection logging=StandardOut::singleton()->messageOut.connect(&logMessage);
    wchar_t configuredStatus[32768]={};GetEnvironmentVariableW(L"LAN_STATUS_PATH",configuredStatus,32768);
    statusPath=configuredStatus[0]?configuredStatus:L"RevivalServer.status.json";
    try {
        managementPort=integerConfig("LAN_SERVER_PORT",53640,65534);
        managementLimit=integerConfig("LAN_SERVER_MAX_PLAYERS",20,128);
        managementName=envValue("LAN_SERVER_NAME","Reboot 16 LAN");
        if(managementName.empty() || managementName.size()>64 || managementName.find_first_of("\r\n\t")!=std::string::npos)throw std::runtime_error("Invalid server display name");
        managementBind=envValue("LAN_SERVER_BIND","127.0.0.1");IN_ADDR checkedAddress;
        if(InetPtonA(AF_INET,managementBind.c_str(),&checkedAddress)!=1)throw std::runtime_error("Bind address must be numeric IPv4");
        if(lanExternalMode() && managementBind!="127.0.0.1")throw std::runtime_error("External admission requires loopback bind and a local HTTPS tunnel connector");
        managementStatus("starting");verifyLANAccountService();
        Game::globalInit(false);ContentProvider::setAssetFolder((folder+"/../../content").c_str());
        wchar_t wideFolder[MAX_PATH];GetModuleFileNameW(0,wideFolder,MAX_PATH);
        std::wstring installation(wideFolder);installation=installation.substr(0,installation.find_last_of(L"\\/"))+L"\\..\\..";
        shared_ptr<GameLibrary> library(new GameLibrary(installation));
        char selected[16]={};GetEnvironmentVariableA("LAN_SERVER_GAME_ID",selected,sizeof(selected));
        int selectedGame=selected[0]?integerConfig("LAN_SERVER_GAME_ID",900000001,2147483647):900000001;
        std::wstring selectedPath;
        if(selectedGame!=900000001){Sleep(1100);selectedPath=library->path(selectedGame);}
        FLog::SetValue("OfflineAuthenticatedSession","true",FASTVARTYPE_STATIC);
        FLog::SetValue("OfflineStudioSession","true",FASTVARTYPE_STATIC);
        FLog::SetValue("UseStarterPlayerCharacterScripts","true",FASTVARTYPE_DYNAMIC);
        UnsecuredStudioGame game(0,"http://127.0.0.1:1",true,false);
        Network::initWithCloudEditSecurity();
        shared_ptr<DataModel> model=game.getDataModel();
        {
            DataModel::LegacyLock lock(model.get(),DataModelJob::Write);
            Security::Impersonator identity(Security::COM);
            char customPlace[MAX_PATH]={};GetEnvironmentVariableA("LAN_SERVER_PLACE",customPlace,sizeof(customPlace));
            std::ifstream place;
            if(selectedGame!=900000001)place.open(selectedPath,std::ios::binary);
            else place.open(customPlace[0]?customPlace:ContentProvider::assetFolder()+"../LocalTest.legacy.rbxlx",std::ios::binary);
            if(!place) throw std::runtime_error("LAN place is missing");
            if(selectedGame!=900000001) library->load(selectedGame,model.get());
            else Serializer().load(place,model.get());
            model->processAfterLoad();
            model->getWorkspace()->setNetworkFilteringEnabled(true);
            StarterPlayerService* starter=model->create<StarterPlayerService>();starter->setupPlayerScripts();starter->setLoadCharacterAppearance(false);
            StarterCharacterScripts* scripts=starter->findFirstChildOfType<StarterCharacterScripts>();
            if(!scripts) {
                auto created=Creatable<Instance>::create<StarterCharacterScripts>();
                created->setParent(starter);scripts=created.get();
            }
            Instances animation;model->create<ContentProvider>()->blockingLoadInstances(ContentId::fromAssets("fonts/humanoidAnimateLocal.rbxm"),animation);
            bool installedAnimation=false;
            for(const auto& script:animation) if(script->getName()=="Animate") {
                if(Instance* existing=scripts->findFirstChildByName("Animate")) existing->setParent(NULL);
                script->setParent(scripts);installedAnimation=true;
            }
            if(!installedAnimation) throw std::runtime_error("Bundled offline Animate script is missing");
            StandardOut::singleton()->printf(MESSAGE_INFO,"[LAN] Offline Animate installed in existing StarterCharacterScripts");
            char portText[8]={},bindAddress[64]={};GetEnvironmentVariableA("LAN_SERVER_PORT",portText,sizeof(portText));GetEnvironmentVariableA("LAN_SERVER_BIND",bindAddress,sizeof(bindAddress));
            int port=portText[0]?atoi(portText):53640;if(port<1 || port>65535) throw std::runtime_error("LAN port out of range");
            Network::Server* network=model->create<Network::Server>();network->start(port,20);
            network->incommingConnectionSignal.connect([](std::string address,shared_ptr<Instance>){StandardOut::singleton()->printf(MESSAGE_INFO,"[LAN] Client connected: %s",address.c_str());});
            FLog::ForEachVariable(&syncFlag,0,FASTVARTYPE_SYNC);
            model->create<ScriptContext>()->addCoreScriptLocal("ServerStarterScript",shared_ptr<Instance>());
            model->create<RunService>()->run();model->gameLoaded();
            StandardOut::singleton()->printf(MESSAGE_INFO,"[LAN] Server listening on %s:%d",bindAddress[0]?bindAddress:"127.0.0.1",port);
        }
        char portForCatalog[8]={},addressForCatalog[64]={};GetEnvironmentVariableA("LAN_SERVER_PORT",portForCatalog,sizeof(portForCatalog));GetEnvironmentVariableA("LAN_SERVER_BIND",addressForCatalog,sizeof(addressForCatalog));
        LibraryCatalogHost catalog(library,selectedGame,addressForCatalog[0]?addressForCatalog:"127.0.0.1",portForCatalog[0]?atoi(portForCatalog):53640);
        StandardOut::singleton()->printf(MESSAGE_INFO,"[Library] Hosting approved game %d",selectedGame);
        managementStatus("running");std::cout<<managementName<<" ready on "<<managementBind<<":"<<managementPort<<" (UDP), catalog TCP "<<managementPort+1<<", limit "<<managementLimit<<". Reachable LAN required."<<std::endl;
        DWORD healthTick=GetTickCount();bool healthy=true;std::string healthError;
        while(!stopping) {
            
            wchar_t stopFile[32768]={};GetEnvironmentVariableW(L"LAN_STOP_PATH",stopFile,32768);std::wstring stopPath=stopFile[0]?stopFile:L"LANServer.stop";
            std::ifstream stop(stopPath);if(stop) {stop.close();stopping=true;DeleteFileW(stopPath.c_str());}
            if(GetTickCount()-healthTick>5000){try{verifyLANAccountService();healthy=true;healthError.clear();}catch(const std::exception& e){healthy=false;healthError="Account service unavailable; new joins will fail: "+std::string(e.what());}healthTick=GetTickCount();}
            managementStatus("running",healthError,healthy);Sleep(250);
        }
        {DataModel::LegacyLock lock(model.get(),DataModelJob::Write);model->find<Network::Server>()->stop();}
        game.shutdown();model.reset();Game::globalExit();
        managementStatus("stopped");CoUninitialize();logFile<<"[LAN] Server shutdown cleanly"<<std::endl;return 0;
    } catch(const std::exception& e) {managementStatus("failed",e.what(),false);std::cerr<<"[LAN] Server failed: "<<e.what()<<std::endl;logFile<<"[LAN] Server failed: "<<e.what()<<std::endl;CoUninitialize();return 1;}
}

