#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <boost/thread/recursive_mutex.hpp>
#include "reflection/Type.h"
#include "AccountTest.h"
struct LibraryGame {
    int id=0;
    std::string file,identity,signature,title,type;
    double modified=0;
    bool available=false;
};
class GameLibrary {
    std::wstring root,registry,installation;
    std::string libraryId;
    int nextId=900000100;
    std::map<int,LibraryGame> games;
    struct Observation {std::string signature; ULONGLONG since=0;};
    std::map<std::string,Observation> observed;
    boost::recursive_mutex mutex;
    void save();
public:
    explicit GameLibrary(const std::wstring& installation);
    void scan();
    LibraryGame resolve(int id);
    std::wstring path(int id);
    void load(int id,RBX::DataModel* model);
    std::string id() const {return libraryId;}
    boost::shared_ptr<const RBX::Reflection::ValueTable> catalog();
    std::string imageBytes(int id,const std::string& name);
    boost::shared_ptr<const RBX::Reflection::ValueTable> prepareImages(boost::shared_ptr<const RBX::Reflection::ValueTable> catalog,bool lan);
    static std::string randomId();
};

void installLibraryBridge(RBX::Instance* parent, boost::shared_ptr<GameLibrary> library,
                          LocalAccountSession* account, bool lan);
std::string libraryJSON(const boost::shared_ptr<const RBX::Reflection::ValueTable>& table);
