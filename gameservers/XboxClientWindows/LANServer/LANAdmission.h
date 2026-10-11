#pragma once
#include "ServerReplicator.h"
void lanAuthenticate(RBX::Network::ServerReplicator*,int,const std::string&);
bool lanAuthenticated(RBX::Network::ServerReplicator*);
void lanPreparePlayer(RBX::Network::ServerReplicator*,boost::shared_ptr<RBX::Network::Player>);
void lanRelease(RBX::Network::ServerReplicator*);


void lanSetReady(RBX::Network::ServerReplicator*);

bool lanCanSend(const void*);
std::string lanManagementPlayers();
bool lanExternalMode();
std::string lanIssueExternalTicket(const std::string& cookieToken);
