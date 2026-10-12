#pragma once
#include "AccountTest.h"
#include "v8datamodel/Game.h"
#include "rbx/signal.h"
#include "network/Player.h"

class AuthenticatedAvatar {
    const LocalAccountSession& account;
    rbx::signals::scoped_connection playerAdded;
public:
    explicit AuthenticatedAvatar(const LocalAccountSession& account):account(account) {}
    void install(RBX::DataModel* server);
    void onPlayer(boost::shared_ptr<RBX::Instance> instance);
    void onCharacter(boost::weak_ptr<RBX::Network::Player> player, boost::shared_ptr<RBX::Instance> character);
};
void previewAvatarModel(RBX::DataModel* shell, const LocalAccountSession& account, const std::string& appearance);
std::string canonicalAvatarAppearance(const std::string& appearance);

