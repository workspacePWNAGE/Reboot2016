#include "stdafx.h"
#include "AuthenticatedAvatar.h"
#include "network/Players.h"
#include "humanoid/Humanoid.h"
#include "v8datamodel/CharacterAppearance.h"
#include "v8datamodel/CharacterMesh.h"
#include "v8datamodel/Accoutrement.h"
#include "v8datamodel/FileMesh.h"
#include "v8datamodel/Decal.h"
#include "v8datamodel/ContentProvider.h"
#include "v8datamodel/Value.h"
#include "v8datamodel/Workspace.h"
#include "v8datamodel/ModelInstance.h"
#include "v8datamodel/PartInstance.h"
#include "v8datamodel/JointInstance.h"
#include "v8xml/WebParser.h"
#include "util/standardout.h"
#include <boost/filesystem.hpp>
#include <set>
#include <cmath>
using namespace RBX;
namespace {
typedef shared_ptr<const Reflection::ValueTable> Json;
const Reflection::Variant& field(const Json& object,const char* key) {
    auto it=object->find(key);if(it==object->end()) throw std::runtime_error(std::string("Missing profile field: ")+key);return it->second;
}
Json object(const Json& parent,const char* key) {return field(parent,key).get<Json>();}
std::string assetId(const Reflection::Variant& value) {
    if(value.isVoid()) return "";
    if(!value.isNumber()) throw std::runtime_error("Non-numeric avatar asset ID");
    double id=value.get<double>();
    if(id<1 || id>9007199254740991.0 || floor(id)!=id) throw std::runtime_error("Invalid avatar asset ID");
    return RBX::format("%.0f",id);
}
void nameValue(Instance* parent,const char* name,const std::string& text) {
    shared_ptr<StringValue> value=Creatable<Instance>::create<StringValue>();value->setName(name);value->setValue(text);value->setParent(parent);
}
void localReference(const ContentId& id) {
    if(id.isNull()) return;
    boost::filesystem::path root=boost::filesystem::canonical(ContentProvider::assetFolder());
    boost::filesystem::path path;
    std::string url=id.toString();
    if(url.compare(0,11,"rbxasset://")==0) path=root/url.substr(11);
    else if(url.compare(0,13,"rbxassetid://")==0) {
        std::string number=url.substr(13);
        if(number.empty() || number.find_first_not_of("0123456789")!=std::string::npos) throw std::runtime_error("Invalid dependency ID: "+url);
        path=root/"offline-avatar"/"cache"/number;
    } else throw std::runtime_error("Avatar dependency must be bundled locally: "+url);
    if(!boost::filesystem::is_regular_file(path)) throw std::runtime_error("Missing avatar dependency: "+url);
    std::string resolved=boost::filesystem::canonical(path).generic_string(), sandbox=root.generic_string()+"/";
    if(resolved.compare(0,sandbox.size(),sandbox)!=0) throw std::runtime_error("Avatar dependency outside content folder: "+url);
}
void validateTree(Instance* instance) {
    std::string type=instance->getClassNameStr();
    
    static const std::set<std::string> allowed={"Model","Folder","Shirt","Pants","ShirtGraphic","CharacterMesh","Hat","Accessory","Accoutrement","Part","SpecialMesh","BlockMesh","CylinderMesh","FileMesh","Decal","Texture","Attachment","Weld"};
    if(!allowed.count(type)) throw std::runtime_error("Unsupported avatar class: "+type);
    if(Shirt* value=Instance::fastDynamicCast<Shirt>(instance)) localReference(value->getTemplate());
    if(Pants* value=Instance::fastDynamicCast<Pants>(instance)) localReference(value->getTemplate());
    if(ShirtGraphic* value=Instance::fastDynamicCast<ShirtGraphic>(instance)) localReference(value->graphic);
    if(FileMesh* value=dynamic_cast<FileMesh*>(instance)) {localReference(value->getMeshId());localReference(value->getTextureId());}
    if(Decal* value=dynamic_cast<Decal*>(instance)) localReference(value->getTexture());
    if(CharacterMesh* value=Instance::fastDynamicCast<CharacterMesh>(instance)) {
        localReference(value->getMeshId());localReference(value->getBaseTextureId());localReference(value->getOverlayTextureId());
    }
    for(size_t i=0;i<instance->numChildren();++i) validateTree(instance->getChild(i));
}
void applyNode(Network::Player* player,const shared_ptr<Instance>& node,const std::string& slot,int& applied,ModelInstance* preview) {
    std::string type=node->getClassNameStr();
    if(type=="Folder" || type=="Model") {
        Instances children;for(size_t i=0;i<node->numChildren();++i) children.push_back(shared_from(node->getChild(i)));
        for(const auto& child:children) applyNode(player,child,slot,applied,preview);
        return;
    }
    bool compatible=slot=="equipped" ? (type=="Shirt" || type=="Pants" || type=="ShirtGraphic" || type=="CharacterMesh" || dynamic_cast<Accoutrement*>(node.get())) :
        slot=="body_package" ? type=="CharacterMesh" : slot=="shirt" ? type=="Shirt" : slot=="pants" ? type=="Pants" : slot=="tshirt" ? type=="ShirtGraphic" : dynamic_cast<Accoutrement*>(node.get())!=0;
    if(!compatible) throw std::runtime_error("Asset class "+type+" does not match "+slot);
    if(player) player->loadCharacterAppearanceScript(node); else node->setParent(preview);++applied;
}
void applyAsset(Network::Player* player,const std::string& id,const std::string& slot,std::set<std::string>& seen,ModelInstance* preview=0) {
    if(id.empty() || !seen.insert(id).second) return;
    try {
        std::string relative="offline-avatar/assets/"+id+".rbxm";
        if(!boost::filesystem::is_regular_file(ContentProvider::assetFolder()+relative)) throw std::runtime_error("Bundled model missing: "+relative);
        Instances instances;ServiceProvider::create<ContentProvider>(player?static_cast<Instance*>(player):static_cast<Instance*>(preview))->blockingLoadInstances(ContentId::fromAssets(relative.c_str()),instances);
        if(instances.empty()) throw std::runtime_error("Bundled model contains no instances");
        
        for(const auto& item:instances) validateTree(item.get());
        int applied=0;for(const auto& item:instances) applyNode(player,item,slot,applied,preview);
        if(!applied) throw std::runtime_error("No compatible appearance instances");
        StandardOut::singleton()->printf(MESSAGE_INFO,"[Avatar] Applied %s asset %s (%d instances)",slot.c_str(),id.c_str(),applied);
    } catch(const std::exception& e) {
        StandardOut::singleton()->printf(MESSAGE_WARNING,"[Avatar] Failed %s asset %s: %s; retained identity and other components",slot.c_str(),id.c_str(),e.what());
    }
}
}
void AuthenticatedAvatar::install(DataModel* server) {
    server->create<ContentProvider>()->setBaseUrl(account.assetOrigin);
    playerAdded=server->create<Network::Players>()->playerAddedEarlySignal.connect(boost::bind(&AuthenticatedAvatar::onPlayer,this,_1));
}
void AuthenticatedAvatar::onPlayer(shared_ptr<Instance> instance) {
    shared_ptr<Network::Player> player=Instance::fastSharedDynamicCast<Network::Player>(instance);
    if(!player) return;
    if(player->getUserID()!=account.userId) {StandardOut::singleton()->printf(MESSAGE_ERROR,"[Player] Unexpected local UserId: %d",player->getUserID());return;}
    player->setName(account.username);
    
    player->setCharacterAppearance("");player->setCanLoadCharacterAppearance(false);
    nameValue(player.get(),"DisplayName",account.displayName);nameValue(player.get(),"AvatarName",account.avatarName);
    StandardOut::singleton()->printf(MESSAGE_INFO,"[Player] Creating authenticated Player\n[Player] Username: %s\n[Player] DisplayName: %s\n[Player] UserId: %d",player->getName().c_str(),account.displayName.c_str(),player->getUserID());
    weak_ptr<Network::Player> weak=player;
    player->characterAddedSignal.connect(boost::bind(&AuthenticatedAvatar::onCharacter,this,weak,_1));
}
static void applyAppearance(const LocalAccountSession& account,Network::Player* player,shared_ptr<Instance> character) {
    ModelInstance* preview=player?nullptr:Instance::fastDynamicCast<ModelInstance>(character.get());
    Security::Impersonator identity(Security::COM);
    try {
        Json profile;if(!WebParser::parseJSONTable(account.profileJson,profile)) throw std::runtime_error("Invalid saved profile JSON");
        Json appearance=object(profile,"appearance"),colors=object(appearance,"body_colors");
        StandardOut::singleton()->print(MESSAGE_INFO,"[Avatar] Loading profile\n[Avatar] Applying body colors");
        shared_ptr<BodyColors> body=Creatable<Instance>::create<BodyColors>();
        BodyColors::prop_HeadColor.setValue(body.get(),BrickColor(field(colors,"head").get<int>()));
        BodyColors::prop_TorsoColor.setValue(body.get(),BrickColor(field(colors,"torso").get<int>()));
        BodyColors::prop_LeftArmColor.setValue(body.get(),BrickColor(field(colors,"left_arm").get<int>()));
        BodyColors::prop_RightArmColor.setValue(body.get(),BrickColor(field(colors,"right_arm").get<int>()));
        BodyColors::prop_LeftLegColor.setValue(body.get(),BrickColor(field(colors,"left_leg").get<int>()));
        BodyColors::prop_RightLegColor.setValue(body.get(),BrickColor(field(colors,"right_leg").get<int>()));
        if(BodyColors* old=character->findFirstChildOfType<BodyColors>()) old->setParent(0);
        if(Skin* old=character->findFirstChildOfType<Skin>()) old->setParent(0);
        if(player) player->loadCharacterAppearanceScript(body);else body->setParent(preview);
        std::set<std::string> seen;
        StandardOut::singleton()->print(MESSAGE_INFO,"[Avatar] Applying body package");
        applyAsset(player,assetId(field(appearance,"body_package")),"body_package",seen,preview);
        StandardOut::singleton()->print(MESSAGE_INFO,"[Avatar] Applying clothing");
        Json clothing=object(appearance,"clothing");
        for(const char* slot:{"shirt","pants","tshirt"}) applyAsset(player,assetId(field(clothing,slot)),slot,seen,preview);
        StandardOut::singleton()->print(MESSAGE_INFO,"[Avatar] Applying accessories");
        for(const auto& id:*field(appearance,"accessories").get<shared_ptr<const Reflection::ValueArray>>()) applyAsset(player,assetId(id),"accessory",seen,preview);
        StandardOut::singleton()->print(MESSAGE_INFO,"[Avatar] Applying equipped assets");
        for(const auto& id:*field(appearance,"equipped_assets").get<shared_ptr<const Reflection::ValueArray>>()) applyAsset(player,assetId(id),"equipped",seen,preview);
        if(player){player->setAppearanceDidLoad(true);player->setCharacterAppearanceLoaded(true);}
        StandardOut::singleton()->printf(MESSAGE_INFO,"[Avatar] Character spawned: %s, UserId %d (engine R6 character)",character->getName().c_str(),account.userId);
    } catch(const std::exception& e) {StandardOut::singleton()->printf(MESSAGE_ERROR,"[Avatar] Profile application failed: %s; Player identity retained",e.what());}
}

void AuthenticatedAvatar::onCharacter(weak_ptr<Network::Player> weak,shared_ptr<Instance> character) {
    shared_ptr<Network::Player> player=weak.lock();if(player)applyAppearance(account,player.get(),character);
}
std::string canonicalAvatarAppearance(const std::string& appearance) {
    Json parsed;if(!WebParser::parseJSONTable(appearance,parsed))throw std::runtime_error("Invalid appearance JSON");
    shared_ptr<Reflection::ValueTable> data(new Reflection::ValueTable(*parsed));
    if(data->find("body_package")==data->end())(*data)["body_package"]=Reflection::Variant();
    shared_ptr<Reflection::ValueTable> clothing(new Reflection::ValueTable);
    auto old=data->find("clothing");if(old!=data->end()){
        if(old->second.isType<Json>())*clothing=*old->second.get<Json>();
        else if(!old->second.isType<shared_ptr<const Reflection::ValueArray>>() || !old->second.get<shared_ptr<const Reflection::ValueArray>>()->empty())throw std::runtime_error("Invalid clothing object");
    }
    for(const char* slot:{"shirt","pants","tshirt"})if(clothing->find(slot)==clothing->end())(*clothing)[slot]=Reflection::Variant();
    (*data)["clothing"]=Json(clothing);
    for(const char* list:{"accessories","equipped_assets"})if(data->find(list)==data->end())(*data)[list]=shared_ptr<const Reflection::ValueArray>(new Reflection::ValueArray);
    std::string result;WebParser::writeJSON(Reflection::Variant(Json(data)),result);return result;
}
void previewAvatarModel(DataModel* shell,const LocalAccountSession& account,const std::string& appearance) {
    Workspace* workspace=shell->getWorkspace();
    if(Instance* old=workspace->findFirstChildByName("LocalAvatarBackdrop"))old->setParent(NULL);
    if(Instance* old=workspace->findFirstChildByName("LocalAvatarPreview"))old->setParent(NULL);
    if(appearance.empty())return;
    Json draft;if(!WebParser::parseJSONTable(canonicalAvatarAppearance(appearance),draft))throw std::runtime_error("Invalid preview appearance");
    Json profile;if(!WebParser::parseJSONTable(account.profileJson,profile))throw std::runtime_error("Invalid account profile");
    shared_ptr<Reflection::ValueTable> changed(new Reflection::ValueTable(*profile));(*changed)["appearance"]=draft;
    LocalAccountSession preview=account;preview.token.clear();WebParser::writeJSON(Reflection::Variant(Json(changed)),preview.profileJson);
    shell->create<ContentProvider>()->setBaseUrl(account.assetOrigin);
    Instances instances;shell->create<ContentProvider>()->blockingLoadInstances(ContentId::fromAssets("fonts/character.rbxm"),instances);
    if(instances.size()!=1)throw std::runtime_error("Engine R6 character model missing");
    shared_ptr<ModelInstance> model=Instance::fastSharedDynamicCast<ModelInstance>(instances[0]);
    if(!model)throw std::runtime_error("Invalid engine R6 model");
    
    
    
    for(size_t i=0;i<model->numChildren();++i)if(auto part=Instance::fastDynamicCast<PartInstance>(model->getChild(i)))part->setAnchored(true);
    model->setName("LocalAvatarPreview");model->setParent(workspace);
    if(auto humanoid=model->findFirstChildOfType<Humanoid>()){humanoid->setNameDisplayDistance(0);humanoid->setHealthDisplayDistance(0);}
    applyAppearance(preview,nullptr,model);
    
    
    
    model->visitDescendants([](shared_ptr<Instance> item) {
        Weld* weld=Instance::fastDynamicCast<Weld>(item.get());
        if(!weld || !weld->getPart0() || !weld->getPart1())return;
        PartInstance* a=weld->getPart0();PartInstance* b=weld->getPart1();
        if(dynamic_cast<Accoutrement*>(b->getParent())) {
            b->setCoordinateFrame(a->getCoordinateFrame()*weld->getC0()*weld->getC1().inverse());b->setAnchored(true);
        } else if(dynamic_cast<Accoutrement*>(a->getParent())) {
            a->setCoordinateFrame(b->getCoordinateFrame()*weld->getC1()*weld->getC0().inverse());a->setAnchored(true);
        }
    });
    model->setPrimaryCFrame(CoordinateFrame(Vector3(20000,10000,20000)));
    for(size_t i=0;i<model->numChildren();++i)if(auto part=Instance::fastDynamicCast<PartInstance>(model->getChild(i)))part->setAnchored(true);
    StandardOut::singleton()->print(MESSAGE_INFO,"[AvatarEditor] Engine R6 preview updated");
}
