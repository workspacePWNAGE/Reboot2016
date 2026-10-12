#pragma once
#include "../GameLibrary.h"
#include <boost/thread.hpp>
#include <atomic>
class LibraryCatalogHost {
    std::atomic<bool> stop;
    SOCKET listener;
    boost::thread worker;
public:
    LibraryCatalogHost(boost::shared_ptr<GameLibrary> library,int activeGame,const std::string& address,int gamePort);
    ~LibraryCatalogHost();
};
