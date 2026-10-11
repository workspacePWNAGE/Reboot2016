#include "stdafx.h"
#include "GameLibrary.h"
#include "OriginalLogin.h"
#include "AuthenticatedAvatar.h"
#include "compat/AvatarTextPrompt.h"
#include "v8tree/Instance.h"
#include "reflection/YieldFunction.h"
#include "v8xml/WebParser.h"
#include "v8datamodel/DataModel.h"
#include "v8datamodel/ContentProvider.h"
#include "v8xml/Serializer.h"
#include "v8datamodel/PartOperation.h"
#include "v8datamodel/Value.h"
#include "v8world/TriangleMesh.h"
#include "../App/lz4/lz4.h"
#include <objbase.h>
#include "util/standardout.h"
#include <xmllite.h>
#include <shlwapi.h>
#include <shellapi.h>
#pragma comment(lib,"shell32.lib")
#include <bcrypt.h>
#include <wincodec.h>
#include "../Library/zlib/include/zlib/zlib.h"
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"uuid.lib")
#include <fstream>
#include <set>
#include <sstream>
#include <algorithm>
using namespace RBX;
namespace {
std::string utf8(const std::wstring& s) {
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),0,0,0,0);std::string r(n,0);
    if(n) WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),&r[0],n,0,0);return r;
}
std::wstring wide(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),0,0);
    if(!n && !s.empty()) throw std::runtime_error("Invalid library filename");
    std::wstring r(n,0);if(n) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),&r[0],n);return r;
}
template<class T> std::string number(T value) {std::ostringstream out;out<<value;return out.str();}
struct RegistryLock {
    HANDLE handle;
    explicit RegistryLock(const std::wstring& path) {
        wchar_t absolute[32768];DWORD n=GetFullPathNameW(path.c_str(),32768,absolute,0);
        if(!n || n>=32768)throw std::runtime_error("Registry path cannot be resolved");
        std::wstring key(absolute,n);std::transform(key.begin(),key.end(),key.begin(),towlower);
        std::wstring name=L"Local\\XboxGameLibrary_"+std::to_wstring(std::hash<std::wstring>()(key));
        handle=CreateMutexW(0,FALSE,name.c_str());if(!handle)throw std::runtime_error("Registry lock unavailable");
        DWORD result=WaitForSingleObject(handle,5000);
        if(result!=WAIT_OBJECT_0 && result!=WAIT_ABANDONED){CloseHandle(handle);handle=0;throw std::runtime_error("Library registry is busy");}
    }
    ~RegistryLock(){if(handle){ReleaseMutex(handle);CloseHandle(handle);}}
};
typedef shared_ptr<const Reflection::ValueTable> Json;
std::string stringValue(const Json& j,const char* key) {auto i=j->find(key);return i!=j->end() && i->second.isType<std::string>()?i->second.get<std::string>():"";}
int intValue(const Json& j,const char* key,int fallback=0) {auto i=j->find(key);return i!=j->end() && i->second.isType<int>()?i->second.get<int>():fallback;}
struct File {
    HANDLE handle;
    explicit File(const std::wstring& path):handle(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,0)) {}
    ~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
};
std::wstring finalPath(HANDLE handle) {
    wchar_t path[32768];DWORD n=GetFinalPathNameByHandleW(handle,path,32768,FILE_NAME_NORMALIZED);
    if(!n || n>=32768) throw std::runtime_error("Cannot resolve library path");return std::wstring(path,n);
}
void contained(HANDLE file,const std::wstring& root) {
    if(GetFileAttributesW(root.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Games directory became a reparse point");
    HANDLE dir=CreateFileW(root.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,0,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,0);
    if(dir==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot resolve games directory");
    std::wstring prefix;
    try{prefix=finalPath(dir)+L"\\";}catch(...){CloseHandle(dir);throw;}
    CloseHandle(dir);std::wstring target=finalPath(file);
    if(target.size()<=prefix.size() || _wcsnicmp(target.c_str(),prefix.c_str(),prefix.size()))
        throw std::runtime_error("Place escapes games directory");
}
bool validXML(const std::wstring& path,std::wstring* title=0) {
    IStream* stream=0;IXmlReader* reader=0;
    HRESULT hr=SHCreateStreamOnFileEx(path.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,FILE_ATTRIBUTE_NORMAL,FALSE,0,&stream);
    if(FAILED(hr)) return false;
    hr=CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(&reader),0);
    bool valid=false,rootSeen=false,itemSeen=false;
    if(SUCCEEDED(hr)) {
        reader->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit);
        reader->SetProperty(XmlReaderProperty_MaxElementDepth,256);
        hr=reader->SetInput(stream);XmlNodeType node;bool titleText=false;std::wstring metadata;UINT rootDepth=0;
        while(SUCCEEDED(hr) && (hr=reader->Read(&node))==S_OK) {
            if(node==XmlNodeType_Element) {
                const wchar_t* name=0;reader->GetLocalName(&name,0);
                if(!rootSeen) {
                    rootSeen=true;if(wcscmp(name,L"roblox")) {hr=E_FAIL;break;}
                    const wchar_t* version=0;
                    if(reader->MoveToAttributeByName(L"version",0)!=S_OK || reader->GetValue(&version,0)!=S_OK || _wtoi(version)<4){hr=E_FAIL;break;}
                    reader->MoveToElement();
                    reader->GetDepth(&rootDepth);
                }
                else if(!wcscmp(name,L"Item")) itemSeen=true;
                UINT depth=0;reader->GetDepth(&depth);
                if(title && depth==rootDepth+1 && !wcscmp(name,L"Meta")) {
                    const wchar_t* key=0;
                    titleText=reader->MoveToAttributeByName(L"name",0)==S_OK && reader->GetValue(&key,0)==S_OK && !_wcsicmp(key,L"Title");
                    reader->MoveToElement();
                }
            } else if(titleText && (node==XmlNodeType_Text || node==XmlNodeType_CDATA)) {
                const wchar_t* value=0;UINT length=0;reader->GetValue(&value,&length);
                if(metadata.size()+length<=100)metadata.append(value,length);else titleText=false;
            } else if(node==XmlNodeType_EndElement && titleText) {
                const wchar_t* name=0;reader->GetLocalName(&name,0);
                if(!wcscmp(name,L"Meta")){if(!metadata.empty() && metadata.find_first_of(L"\r\n\t")==std::wstring::npos)*title=metadata;titleText=false;}
            }
        }
        valid=hr==S_FALSE && rootSeen && itemSeen;reader->Release();
    }
    stream->Release();return valid;
}
bool validBinary(const std::wstring& path,ULONGLONG size) {
    std::ifstream in(path,std::ios::binary);char header[32];
    if(!in.read(header,32) || memcmp(header,"<roblox!\x89\xff\x0d\x0a\x1a\x0a",14) || header[14] || header[15]) return false;
    unsigned types=0,objects=0;memcpy(&types,header+16,4);memcpy(&objects,header+20,4);
    if(types>5000 || objects>500000) return false;
    ULONGLONG pos=32;
    while(pos+16<=size) {
        char chunk[16];if(!in.read(chunk,16))return false;
        unsigned compressed=0,raw=0;memcpy(&compressed,chunk+4,4);memcpy(&raw,chunk+8,4);
        ULONGLONG length=compressed?compressed:raw;
        if(raw>64*1024*1024 || length>64*1024*1024 || length>size-pos-16) return false;
        pos+=16+length;
        if(!memcmp(chunk,"END\0",4)) {char ending[9];return pos==size && !compressed && raw==9 && in.read(ending,9) && !memcmp(ending,"</roblox>",9);}
        if(compressed) {
            if(!raw)return false;
            std::vector<char> encoded(compressed),decoded(raw);
            if(!in.read(encoded.data(),compressed) || LZ4_decompress_safe(encoded.data(),decoded.data(),compressed,raw)!=int(raw))return false;
        }else in.seekg(static_cast<std::streamoff>(length),std::ios::cur);
    }
    return false;
}
bool validatePlace(const std::wstring& path,ULONGLONG size,std::wstring* title=0) {
    std::ifstream input(path,std::ios::binary);char magic[8]={};input.read(magic,8);
    return !memcmp(magic,"<roblox!",8)?validBinary(path,size):validXML(path,title);
}
}
std::string libraryJSON(const Json& table) {
    std::string output;if(!WebParser::writeJSON(Reflection::Variant(table),output)) throw std::runtime_error("Cannot encode game catalog");return output;
}
std::string GameLibrary::randomId() {
    unsigned char bytes[16];if(BCryptGenRandom(0,bytes,16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Library ID generation failed");
    const char hex[]="0123456789abcdef";std::string s;for(auto b:bytes){s+=hex[b>>4];s+=hex[b&15];}return s;
}
GameLibrary::GameLibrary(const std::wstring& installation):root(installation+L"\\games"),registry(installation+L"\\local-game-library.json"),installation(installation) {
    RegistryLock guard(registry);
    CreateDirectoryW(root.c_str(),0);
    DWORD attr=GetFileAttributesW(root.c_str());
    if(attr==INVALID_FILE_ATTRIBUTES || !(attr&FILE_ATTRIBUTE_DIRECTORY) || (attr&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Games directory is missing or a reparse point");
    std::ifstream input(registry);std::string body((std::istreambuf_iterator<char>(input)),{});Json j;
    input.close();
    if(!body.empty()) {
        if(body.size()>4*1024*1024 || !WebParser::parseJSONTable(body,j) || !j)throw std::runtime_error("Library registry is invalid; preserved for recovery");
        libraryId=stringValue(j,"library_id");nextId=intValue(j,"next_id",900000100);
        auto rows=j->find("games");
        if(rows!=j->end() && rows->second.isType<shared_ptr<const Reflection::ValueArray>>()) {
            for(auto& value:*rows->second.get<shared_ptr<const Reflection::ValueArray>>()) {
                if(!value.isType<Json>())continue;Json row=value.get<Json>();LibraryGame g;
                g.id=intValue(row,"game_id");g.file=stringValue(row,"file");g.identity=stringValue(row,"identity");
                if(g.id>=900000100 && g.id<2147483647 && g.file.find_first_of("\\/:")==std::string::npos && g.file!="..")games[g.id]=g;
            }
        }
    }
    if(libraryId.empty())libraryId=randomId();
    if(libraryId.size()!=32 || libraryId.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid persistent library ID");
    for(auto& row:games)nextId=std::max(nextId,row.first+1);
    save();scan();
}
void GameLibrary::save() {
    shared_ptr<Reflection::ValueTable> data(new Reflection::ValueTable);
    (*data)["library_id"]=libraryId;(*data)["next_id"]=nextId;
    shared_ptr<Reflection::ValueArray> rows(new Reflection::ValueArray);
    for(auto& pair:games) {const LibraryGame& g=pair.second;shared_ptr<Reflection::ValueTable> row(new Reflection::ValueTable);
        (*row)["game_id"]=g.id;(*row)["file"]=g.file;(*row)["identity"]=g.identity;rows->push_back(Json(row));}
    (*data)["games"]=shared_ptr<const Reflection::ValueArray>(rows);
    std::string body=libraryJSON(Json(data));std::wstring temp=registry+L".tmp";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<body;if(!out)throw std::runtime_error("Library registry could not be saved");}
    if(!MoveFileExW(temp.c_str(),registry.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Library registry commit failed");
}
void GameLibrary::scan() {
    boost::recursive_mutex::scoped_lock lock(mutex);RegistryLock guard(registry);std::set<int> found;
    
    
    std::ifstream persisted(registry);std::string text((std::istreambuf_iterator<char>(persisted)),{});Json saved;
    persisted.close();
    if(text.size()>4*1024*1024 || !WebParser::parseJSONTable(text,saved) || !saved)throw std::runtime_error("Library registry changed to invalid JSON");
    if(stringValue(saved,"library_id")!=libraryId)throw std::runtime_error("Library identity changed while running");
    nextId=std::max(nextId,intValue(saved,"next_id",nextId));
    auto existing=saved->find("games");
    if(existing!=saved->end() && existing->second.isType<shared_ptr<const Reflection::ValueArray>>()) {
        for(auto& value:*existing->second.get<shared_ptr<const Reflection::ValueArray>>()) {
            if(!value.isType<Json>())continue;Json row=value.get<Json>();int id=intValue(row,"game_id");std::string file=stringValue(row,"file");
            if(!games.count(id) && id>=900000100 && id<2147483647 && file.find_first_of("\\/:")==std::string::npos && file!="..") {
                LibraryGame g;g.id=id;g.file=file;g.identity=stringValue(row,"identity");games[id]=g;nextId=std::max(nextId,id+1);
            }
        }
    }
    WIN32_FIND_DATAW data;HANDLE search=FindFirstFileW((root+L"\\*").c_str(),&data);
    if(search!=INVALID_HANDLE_VALUE) {
        do {
            if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
            std::wstring file=data.cFileName;std::string name=utf8(file);
            auto dot=file.find_last_of(L'.');if(dot==std::wstring::npos)continue;
            std::wstring ext=file.substr(dot);std::transform(ext.begin(),ext.end(),ext.begin(),towlower);
            if(ext!=L".rbxlx" && ext!=L".rbxl")continue;
            try {
                if(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Reparse points are not allowed");
                File f(root+L"\\"+file);if(f.handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Busy/incomplete file");
                contained(f.handle,root);BY_HANDLE_FILE_INFORMATION info={};if(!GetFileInformationByHandle(f.handle,&info))throw std::runtime_error("File information unavailable");
                ULONGLONG size=(ULONGLONG(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
                ULONGLONG stamp=(ULONGLONG(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
                if(size<16 || size>512ULL*1024*1024)throw std::runtime_error("Empty/oversized place");
                std::string sig=number(size)+":"+number(stamp);
                Observation& observation=observed[name];
                if(observation.signature!=sig){observation.signature=sig;observation.since=GetTickCount64();throw std::runtime_error("Waiting for stable copy");}
                if(GetTickCount64()-observation.since<1000)throw std::runtime_error("Waiting for stable copy");
                std::string identity=number(info.dwVolumeSerialNumber)+":"+number(info.nFileIndexHigh)+":"+number(info.nFileIndexLow);
                int id=0;for(auto& pair:games)if(pair.second.identity==identity){id=pair.first;break;}
                if(!id)for(auto& pair:games)if(pair.second.file==name){id=pair.first;break;}
                std::wstring metadataTitle;
                bool changed=!id || games[id].signature!=sig || games[id].file!=name;
                if(changed && !validatePlace(root+L"\\"+file,size,&metadataTitle))throw std::runtime_error("Corrupt/incomplete place format");
                if(!id){if(nextId>=2147483647)throw std::runtime_error("Game ID range exhausted");id=nextId++;}
                LibraryGame& game=games[id];
                if(game.signature!=sig || game.file!=name)StandardOut::singleton()->printf(MESSAGE_INFO,"[Library] Discovered/updated game %d: %s",id,name.c_str());
                game.id=id;game.file=name;game.identity=identity;game.signature=sig;game.type=utf8(ext);
                std::wstring title=file.substr(0,dot);if(title.size()>100){title.resize(100);if(title.back()>=0xd800 && title.back()<=0xdbff)title.pop_back();}
                if(changed){game.title=utf8(metadataTitle.empty()?title:metadataTitle);if(metadataTitle.empty())std::replace(game.title.begin(),game.title.end(),'_',' ');}
                game.modified=double(stamp/10000000ULL-11644473600ULL);game.available=true;found.insert(id);
            } catch(const std::exception& e){StandardOut::singleton()->printf(MESSAGE_WARNING,"[Library] Rejected %s: %s",name.c_str(),e.what());}
        }while(FindNextFileW(search,&data));FindClose(search);
    }
    for(auto& pair:games)if(!found.count(pair.first)){if(pair.second.available)StandardOut::singleton()->printf(MESSAGE_INFO,"[Library] Unavailable game %d",pair.first);pair.second.available=false;}
    save();
}
LibraryGame GameLibrary::resolve(int id) {
    boost::recursive_mutex::scoped_lock lock(mutex);scan();auto found=games.find(id);
    if(found==games.end() || !found->second.available)throw std::runtime_error("Game is unavailable or still being copied");return found->second;
}
std::wstring GameLibrary::path(int id) {
    LibraryGame game=resolve(id);return root+L"\\"+wide(game.file);
}
void GameLibrary::load(int id,DataModel* model) {
    boost::recursive_mutex::scoped_lock lock(mutex);std::wstring selected=path(id);
    File held(selected);if(held.handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Selected place is busy/unavailable");
    contained(held.handle,root);
    
    
    BY_HANDLE_FILE_INFORMATION info={};GetFileInformationByHandle(held.handle,&info);
    ULONGLONG size=(ULONGLONG(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    if(!validatePlace(selected,size))throw std::runtime_error("Selected place is corrupt/incomplete");
    std::ifstream place(selected,std::ios::binary);if(!place)throw std::runtime_error("Selected place could not be opened");
    Serializer().load(place,model);
    
    
    
    
    model->visitDescendants([](const shared_ptr<Instance>& instance) {
        if(PartOperation* part=dynamic_cast<PartOperation*>(instance.get())) {
            int version=-1;
            if(!TriangleMesh::validateDataVersions(part->getNonKeyPhysicsData(),version) && version>=0 && version<PHYSICS_SERIAL_VERSION)
                throw std::runtime_error("Unsupported legacy CSG collision data on "+part->getName()+" (version "+number(version)+"); resave with compatible collision data before playing");
        }
    });
}
namespace {
std::string gameAsset(const std::wstring& root,int id,const std::string& name,size_t limit) {
    std::wstring assets=root+L"\\assets",folder=assets+L"\\"+std::to_wstring(id);
    for(auto p:{root,assets,folder}) {
        DWORD attr=GetFileAttributesW(p.c_str());
        if(attr==INVALID_FILE_ATTRIBUTES || !(attr&FILE_ATTRIBUTE_DIRECTORY) || attr&FILE_ATTRIBUTE_REPARSE_POINT)
            throw std::runtime_error("Missing/reparse game asset directory");
    }
    std::wstring path=folder+L"\\"+wide(name);
    if(GetFileAttributesW(path.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Reparse game asset rejected");
    File held(path);if(held.handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Asset missing/busy");contained(held.handle,folder);
    LARGE_INTEGER size;if(!GetFileSizeEx(held.handle,&size) || size.QuadPart<=0 || size.QuadPart>limit)throw std::runtime_error("Asset size invalid");
    std::string bytes(size_t(size.QuadPart),0);DWORD got=0;
    if(!ReadFile(held.handle,&bytes[0],DWORD(bytes.size()),&got,0) || got!=bytes.size())throw std::runtime_error("Incomplete asset");return bytes;
}
bool validImage(const std::string& bytes) {
    if(bytes.size()<24 || bytes.size()>4194304 || memcmp(bytes.data(),"\x89PNG\r\n\x1a\n",8))return false;
    auto u32=[&bytes](size_t i)->unsigned {return (unsigned(BYTE(bytes[i]))<<24)|(unsigned(BYTE(bytes[i+1]))<<16)|(unsigned(BYTE(bytes[i+2]))<<8)|BYTE(bytes[i+3]);};
    size_t at=8;bool end=false;
    while(at+12<=bytes.size()) {
        unsigned length=u32(at);if(length>bytes.size()-at-12)return false;
        if(crc32(0,reinterpret_cast<const Bytef*>(bytes.data()+at+4),length+4)!=u32(at+8+length))return false;
        if(!memcmp(bytes.data()+at+4,"IEND",4)){if(length || at+12!=bytes.size())return false;end=true;break;}
        at+=length+12;
    }
    if(!end)return false;
    IWICImagingFactory* factory=0;IWICStream* stream=0;IWICBitmapDecoder* decoder=0;IWICBitmapFrameDecode* frame=0;IWICFormatConverter* convert=0;
    
    const CLSID factoryId={0xcacaf262,0x9370,0x4615,{0xa1,0x3b,0x9f,0x55,0x39,0xda,0x4c,0x0a}};
    HRESULT hr=CoCreateInstance(factoryId,0,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);
    if(SUCCEEDED(hr))hr=stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())),DWORD(bytes.size()));
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream,0,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
    UINT w=0,h=0;if(SUCCEEDED(hr))hr=frame->GetSize(&w,&h);
    if(!w || !h || w>2048 || h>2048)hr=E_FAIL;
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&convert);
    if(SUCCEEDED(hr))hr=convert->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,0,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(hr)){std::vector<BYTE> pixels(w*h*4);hr=convert->CopyPixels(0,w*4,UINT(pixels.size()),pixels.data());}
    if(convert)convert->Release();if(frame)frame->Release();if(decoder)decoder->Release();if(stream)stream->Release();if(factory)factory->Release();return SUCCEEDED(hr);
}
}
std::string GameLibrary::imageBytes(int id,const std::string& name) {
    boost::recursive_mutex::scoped_lock lock(mutex);
    if(name!="thumbnail.png" && name!="image1.png" && name!="image2.png")throw std::runtime_error("Invalid image key");
    if(!games.count(id) || !games[id].available)throw std::runtime_error("Game unavailable");
    std::string bytes=gameAsset(root,id,name,4194304);if(!validImage(bytes))throw std::runtime_error("Invalid/truncated/oversized PNG");return bytes;
}
Json GameLibrary::prepareImages(Json catalog,bool lan) {
    shared_ptr<Reflection::ValueTable> result(new Reflection::ValueTable(*catalog));
    std::string identity=stringValue(catalog,"library_id");
    if(identity.size()!=32 || identity.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid host library identity");
    wchar_t executable[32768];DWORD length=GetModuleFileNameW(0,executable,32768);
    if(!length || length>=32768)throw std::runtime_error("Cannot resolve image cache installation");
    std::wstring binary(executable,length),content=binary.substr(0,binary.find_last_of(L"\\/"))+L"\\..\\..\\content";
    DWORD contentAttr=GetFileAttributesW(content.c_str());
    if(contentAttr==INVALID_FILE_ATTRIBUTES || contentAttr&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Content directory unavailable/reparse");
    std::wstring cache=content+L"\\library-images";CreateDirectoryW(cache.c_str(),0);
    DWORD attr=GetFileAttributesW(cache.c_str());
    if(attr==INVALID_FILE_ATTRIBUTES || attr&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Image cache directory unavailable/reparse");
    auto source=catalog->find("games");shared_ptr<Reflection::ValueArray> rows(new Reflection::ValueArray);
    if(source!=catalog->end() && source->second.isType<shared_ptr<const Reflection::ValueArray>>()) for(auto& v:*source->second.get<shared_ptr<const Reflection::ValueArray>>()) {
        if(!v.isType<Json>())continue;Json old=v.get<Json>();shared_ptr<Reflection::ValueTable> row(new Reflection::ValueTable(*old));int id=intValue(old,"game_id");
        for(auto name:{"thumbnail.png","image1.png","image2.png"}) {
            const char* key=!strcmp(name,"thumbnail.png")?"thumbnail":!strcmp(name,"image1.png")?"image1":"image2";
            (*row)[key]=std::string("");
            try {
                if(stringValue(old,key).empty())continue;
                std::string bytes=lan?fetchLANLibraryImage(id,name):imageBytes(id,name);
                if(!validImage(bytes))throw std::runtime_error("Invalid host PNG");
                std::string file=identity+"_"+number(id)+"_"+number(std::hash<std::string>()(bytes))+"_"+name;
                std::wstring target=cache+L"\\"+wide(file);
                HANDLE out=CreateFileW(target.c_str(),GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
                if(out!=INVALID_HANDLE_VALUE){DWORD written=0;BOOL ok=WriteFile(out,bytes.data(),DWORD(bytes.size()),&written,0);CloseHandle(out);if(!ok || written!=bytes.size()){DeleteFileW(target.c_str());throw std::runtime_error("Image cache write failed");}}
                else {File held(target);if(held.handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Image cache unavailable");contained(held.handle,cache);}
                (*row)[key]=std::string("rbxasset://library-images/")+file;
            } catch(const std::exception& e){StandardOut::singleton()->printf(MESSAGE_WARNING,"[Library] Game %d %s: %s; using placeholder",id,name,e.what());}
        }
        rows->push_back(Json(row));
    }
    (*result)["games"]=shared_ptr<const Reflection::ValueArray>(rows);return Json(result);
}
Json GameLibrary::catalog() {
    boost::recursive_mutex::scoped_lock lock(mutex);scan();shared_ptr<Reflection::ValueTable> data(new Reflection::ValueTable);
    (*data)["library_id"]=libraryId;shared_ptr<Reflection::ValueArray> rows(new Reflection::ValueArray);
    for(auto& pair:games)if(pair.second.available){auto& g=pair.second;shared_ptr<Reflection::ValueTable> row(new Reflection::ValueTable);
        (*row)["game_id"]=g.id;(*row)["title"]=g.title;(*row)["file_type"]=g.type;(*row)["modified"]=g.modified;
        std::string description="A local game from this library. Select Play to enter.";
        try {std::string bytes=gameAsset(root,g.id,"metadata.json",16384);Json metadata;
            if(!WebParser::parseJSONTable(bytes,metadata) || !metadata)throw std::runtime_error("Invalid metadata JSON");
            std::string title=stringValue(metadata,"title"),text=stringValue(metadata,"description");
            if(!title.empty() && wide(title).size()<=100 && title.find_first_of("\r\n\t")==std::string::npos)(*row)["title"]=title;
            if(!text.empty() && wide(text).size()<=4096)description=text;
            for(auto key:{"thumbnail","image1","image2"}){std::string value=stringValue(metadata,key);if(!value.empty() && value!=std::string(key)+".png")StandardOut::singleton()->printf(MESSAGE_WARNING,"[Library] Game %d rejected metadata image path %s",g.id,key);}
        }catch(const std::exception&){}
        try{std::string text=gameAsset(root,g.id,"description.txt",16384);if(wide(text).size()<=4096)description=text;}catch(const std::exception&){}
        (*row)["description"]=description;
        for(auto key:{"thumbnail","image1","image2"}) {try{imageBytes(g.id,std::string(key)+".png");(*row)[key]=std::string(key)+".png";}catch(const std::exception&){} }
        rows->push_back(Json(row));}
    (*data)["games"]=shared_ptr<const Reflection::ValueArray>(rows);return Json(data);
}

namespace RBX {
extern const char* const sLocalLibraryBridge;
const char* const sLocalLibraryBridge="LocalLibraryBridge";
class LocalLibraryBridge:public DescribedCreatable<LocalLibraryBridge,Instance,sLocalLibraryBridge> {
public:
    shared_ptr<GameLibrary> library;
    LocalAccountSession* account=0;
    bool lan=false;
    void refresh(boost::function<void(std::string)> resume,boost::function<void(std::string)> error) {
        shared_ptr<LocalLibraryBridge> owner=shared_from(this);
        weak_ptr<DataModel> model=weak_from(DataModel::get(this));
        LocalAccountSession session=account?*account:LocalAccountSession();
        boost::thread([owner,model,session,resume,error]() mutable {
            HRESULT apartment=CoInitializeEx(0,COINIT_MULTITHREADED);
            std::string result,reason;
            try {
                if(owner->lan) {fetchLANLibrary();Sleep(1100);} else {owner->library->scan();Sleep(1100);}
                Json catalog=owner->library->prepareImages(owner->lan?fetchLANLibrary():owner->library->catalog(),owner->lan);
                shared_ptr<Reflection::ValueTable> data(new Reflection::ValueTable(*catalog));
                (*data)["enabled"]=true;
                if(session.userId>0) {
                    try {
                        Json recent=accountLibraryRequest(session,L"GET",L"/v1/me/games?library_id="+wide(stringValue(catalog,"library_id")));
                        auto rows=recent->find("games");if(rows!=recent->end())(*data)["recent"]=rows->second;
                        Json favourites=accountLibraryRequest(session,L"GET",L"/v1/me/favourites?library_id="+wide(stringValue(catalog,"library_id")));
                        auto ids=favourites->find("game_ids");if(ids!=favourites->end())(*data)["favourites"]=ids->second;
                    }catch(const std::exception& e){StandardOut::singleton()->printf(MESSAGE_WARNING,"[Library] Recent history unavailable: %s",e.what());}
                }
                result=libraryJSON(Json(data));
            }catch(const std::exception& e){reason=e.what();}
            if(!session.token.empty())SecureZeroMemory(&session.token[0],session.token.size());session.token.clear();
            if(SUCCEEDED(apartment))CoUninitialize();
            if(shared_ptr<DataModel> dm=model.lock()) dm->submitTask([resume,error,result,reason](DataModel*) {
                if(reason.empty())resume(result);else error(reason);
            },DataModelJob::Write);
        }).detach();
    }
    std::string prompt(std::string title,std::string initial) {return utf8(avatarTextPrompt(wide(title),wide(initial)));}
    std::string preview(std::string appearance) {
        if(!account || account->userId<=0)throw std::runtime_error("Avatar editor requires login");
        previewAvatarModel(DataModel::get(this),*account,appearance);return "{}";
    }
    shared_ptr<Instance> recoveredEnvironment() {
        if(!account || account->userId<=0)throw std::runtime_error("Avatar environment requires login");
        Instances roots;
        DataModel::get(this)->create<ContentProvider>()->blockingLoadInstances(ContentId::fromAssets("recovered-avatar/editor-environment.rbxm"),roots);
        for(const auto& root:roots)if(root->getName()=="RecoveredAvatarEnvironment") {
            std::ifstream file(ContentProvider::assetFolder()+"recovered-avatar/environment-lighting.json",std::ios::binary);
            if(!file)throw std::runtime_error("Archived environment lighting missing");
            std::string settings((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
            shared_ptr<StringValue> values=Creatable<Instance>::create<StringValue>();
            values->setName("OriginalLighting");values->setValue(settings);values->setParent(root.get());
            StandardOut::singleton()->print(MESSAGE_INFO,"[RecoveredEnvironment] Loaded original archived 3D room");
            return root;
        }
        throw std::runtime_error("Archived environment missing");
    }
    shared_ptr<Instance> recoveredEditor() {
        if(!account || account->userId<=0)throw std::runtime_error("Avatar editor requires login");
        Instances roots;
        DataModel::get(this)->create<ContentProvider>()->blockingLoadInstances(ContentId::fromAssets("recovered-avatar/editor-gui.rbxm"),roots);
        for(const auto& root:roots)if(root->getName()=="ScreenGuiV1") {
            root->setName("RecoveredAvatarCustomization");
            StandardOut::singleton()->print(MESSAGE_INFO,"[RecoveredEditor] Loaded archived ScreenGuiV1 layout");
            return root;
        }
        throw std::runtime_error("Archived ScreenGuiV1 layout missing");
    }
    void profile(std::string operation,std::string body,boost::function<void(std::string)> resume,boost::function<void(std::string)> error) {
        if(!account || account->userId<=0){error("Avatar editor requires login");return;}
        shared_ptr<LocalLibraryBridge> owner=shared_from(this);weak_ptr<DataModel> model=weak_from(DataModel::get(this));
        LocalAccountSession session=*account;
        boost::thread([owner,model,session,operation,body,resume,error]()mutable {
            std::string result,reason;Json current;
            try {
                const wchar_t* method=L"GET";std::wstring path=L"/v1/me";
                if(operation=="save"){method=L"PATCH";}
                else if(operation=="catalog"){method=L"POST";path+=L"/avatar-catalog";}
                else if(operation=="outfits" || operation=="outfit-save"){path+=L"/outfits";if(operation=="outfit-save")method=L"POST";}
                else if(operation.compare(0,7,"rename:")==0 || operation.compare(0,7,"delete:")==0 || operation.compare(0,6,"equip:")==0) {
                    size_t colon=operation.find(':');std::string id=operation.substr(colon+1);
                    if(id.size()!=32 || id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid outfit ID");
                    path+=L"/outfits/"+wide(id);
                    if(operation.compare(0,6,"equip:")==0){method=L"POST";path+=L"/equip";}
                    else method=operation.compare(0,7,"rename:")==0?L"PATCH":L"DELETE";
                } else if(operation!="get")throw std::runtime_error("Unknown profile operation");
                if(operation=="save" || operation=="outfit-save"){
                    Json payload;if(!WebParser::parseJSONTable(body,payload))throw std::runtime_error("Invalid profile request");
                    auto appearance=payload->find("appearance");
                    if(appearance!=payload->end()){
                        if(!appearance->second.isType<Json>())throw std::runtime_error("Invalid appearance object");
                        Json normalized;WebParser::parseJSONTable(canonicalAvatarAppearance(libraryJSON(appearance->second.get<Json>())),normalized);
                        shared_ptr<Reflection::ValueTable> updated(new Reflection::ValueTable(*payload));(*updated)["appearance"]=normalized;body=libraryJSON(Json(updated));
                    }
                }
                Json response=accountLibraryRequest(session,method,path,body);
                result=libraryJSON(response);
                current=accountLibraryRequest(session,L"GET",L"/v1/me");
                if(intValue(current,"user_id")!=session.userId || stringValue(current,"username")!=session.username)throw std::runtime_error("Profile identity mismatch");
            }catch(const std::exception& e){reason=e.what();}
            if(!session.token.empty())SecureZeroMemory(&session.token[0],session.token.size());session.token.clear();
            if(auto dm=model.lock())dm->submitTask([owner,current,result,reason,resume,error](DataModel*) {
                if(!reason.empty()){error(reason);return;}
                if(owner->account){owner->account->profileJson=libraryJSON(current);owner->account->displayName=stringValue(current,"display_name");owner->account->avatarName=stringValue(current,"avatar_name");}
                resume(result);
            },DataModelJob::Write);
        }).detach();
    }
    void preferences(int id,std::string operation,boost::function<void(std::string)> resume,boost::function<void(std::string)> error) {
        shared_ptr<LocalLibraryBridge> owner=shared_from(this);weak_ptr<DataModel> model=weak_from(DataModel::get(this));
        LocalAccountSession session=account?*account:LocalAccountSession();
        boost::thread([owner,model,session,id,operation,resume,error]() mutable {
            HRESULT apartment=CoInitializeEx(0,COINIT_MULTITHREADED);std::string result,reason;
            try {
                if(session.userId<=0)throw std::runtime_error("Ratings and favourites require an account; local play does not");
                Json catalog=owner->lan?fetchLANLibrary():owner->library->catalog();bool found=false;
                auto items=catalog->find("games");
                if(items!=catalog->end() && items->second.isType<shared_ptr<const Reflection::ValueArray>>())for(auto& row:*items->second.get<shared_ptr<const Reflection::ValueArray>>())if(row.isType<Json>() && intValue(row.get<Json>(),"game_id")==id)found=true;
                if(!found)throw std::runtime_error("Selected game is unavailable");
                std::string body;
                if(operation=="positive")body="{\"vote\":1}";else if(operation=="negative")body="{\"vote\":-1}";
                else if(operation=="clear")body="{\"vote\":null}";else if(operation=="favourite")body="{\"favourite\":true}";
                else if(operation=="unfavourite")body="{\"favourite\":false}";else if(operation!="get")throw std::runtime_error("Invalid game preference operation");
                result=libraryJSON(accountLibraryRequest(session,body.empty()?L"GET":L"PUT",L"/v1/games/"+wide(stringValue(catalog,"library_id"))+L"/"+std::to_wstring(id)+L"/preferences",body));
            }catch(const std::exception& e){reason=e.what();StandardOut::singleton()->printf(MESSAGE_WARNING,"[GameDetails] Preference failed game %d: %s",id,e.what());}
            if(!session.token.empty())SecureZeroMemory(&session.token[0],session.token.size());session.token.clear();if(SUCCEEDED(apartment))CoUninitialize();
            if(shared_ptr<DataModel> dm=model.lock())dm->submitTask([resume,error,result,reason](DataModel*){if(reason.empty())resume(result);else error(reason);},DataModelJob::Write);
        }).detach();
    }
};
static Reflection::BoundYieldFuncDesc<LocalLibraryBridge,std::string()> refreshLibrary(&LocalLibraryBridge::refresh,"Refresh",Security::RobloxScript);
static Reflection::BoundYieldFuncDesc<LocalLibraryBridge,std::string(int,std::string)> preferencesLibrary(&LocalLibraryBridge::preferences,"Preferences","gameId","operation",Security::RobloxScript);
static Reflection::BoundYieldFuncDesc<LocalLibraryBridge,std::string(std::string,std::string)> profileLibrary(&LocalLibraryBridge::profile,"Profile","operation","body",Security::RobloxScript);
static Reflection::BoundFuncDesc<LocalLibraryBridge,std::string(std::string)> previewLibrary(&LocalLibraryBridge::preview,"Preview","appearance",Security::RobloxScript);
static Reflection::BoundFuncDesc<LocalLibraryBridge,shared_ptr<Instance>()> recoveredEnvironmentLibrary(&LocalLibraryBridge::recoveredEnvironment,"RecoveredEnvironment",Security::RobloxScript);
static Reflection::BoundFuncDesc<LocalLibraryBridge,shared_ptr<Instance>()> recoveredEditorLibrary(&LocalLibraryBridge::recoveredEditor,"RecoveredEditor",Security::RobloxScript);
static Reflection::BoundFuncDesc<LocalLibraryBridge,std::string(std::string,std::string)> promptLibrary(&LocalLibraryBridge::prompt,"Prompt","title","initial",Security::RobloxScript);
RBX_REGISTER_CLASS(LocalLibraryBridge);
}
void installLibraryBridge(Instance* parent,shared_ptr<GameLibrary> library,LocalAccountSession* account,bool lan) {
    auto bridge=Creatable<Instance>::create<LocalLibraryBridge>();bridge->setName("LocalLibraryBridge");
    bridge->library=library;bridge->account=account;bridge->lan=lan;bridge->setRobloxLocked(true);bridge->setParent(parent);
}

namespace RBX {
extern const char* const sOriginalLoginBridge;
const char* const sOriginalLoginBridge="OriginalLoginBridge";
class OriginalLoginBridge:public DescribedCreatable<OriginalLoginBridge,Instance,sOriginalLoginBridge> {
public:
    LocalAccountSession* account=0;bool external=true,busy=false;
    bool openRegistration() {
        
        return reinterpret_cast<INT_PTR>(ShellExecuteW(0,L"open",L"https://r16.noli.vip/signup",0,0,SW_SHOWNORMAL))>32;
    }
    std::string profile() {return account && account->userId>0?account->profileJson:"{}";}
    void login(std::string username,std::string password,boost::function<void(int)> resume,boost::function<void(std::string)> error) {
        authenticate(username,password,resume,error,false);
    }
    void registerAccount(std::string username,std::string password,boost::function<void(int)> resume,boost::function<void(std::string)> error) {
        authenticate(username,password,resume,error,true);
    }
    void authenticate(std::string username,std::string password,boost::function<void(int)> resume,boost::function<void(std::string)> error,bool registerFirst) {
        if(busy){resume(1);return;}busy=true;
        shared_ptr<OriginalLoginBridge> owner=shared_from(this);weak_ptr<DataModel> model=weak_from(DataModel::get(this));
        boost::thread([owner,model,username,password,resume,registerFirst]() mutable {
            HRESULT apartment=CoInitializeEx(0,COINIT_MULTITHREADED);LocalAccountSession result;
            int code=authenticateOriginalLogin(username,password,owner->external,result,registerFirst);
            if(!password.empty())SecureZeroMemory(&password[0],password.size());password.clear();
            
            if(shared_ptr<DataModel> target=model.lock()) {
                target->submitTask([owner,result,code,resume](DataModel* activeModel)mutable {
                    owner->busy=false;
                    if(activeModel->isClosed() || activeModel->getIsShuttingDown())return;
                    if(code==0){*owner->account=result;StandardOut::singleton()->printf(MESSAGE_INFO,"[OriginalLogin] Authenticated %s UserId %d",result.username.c_str(),result.userId);}
                    else StandardOut::singleton()->printf(MESSAGE_WARNING,"[OriginalLogin] Authentication failed code %d",code);
                    resume(code);
                },DataModelJob::Write);
            }
            if(SUCCEEDED(apartment))CoUninitialize();
        }).detach();
    }
    void validate(boost::function<void(int)> resume,boost::function<void(std::string)> error) {
        if(busy){resume(1);return;}busy=true;
        shared_ptr<OriginalLoginBridge> owner=shared_from(this);weak_ptr<DataModel> model=weak_from(DataModel::get(this));
        LocalAccountSession session=*account;
        boost::thread([owner,model,session,resume]()mutable {
            HRESULT apartment=CoInitializeEx(0,COINIT_MULTITHREADED);int code=validateOriginalLogin(session);
            if(shared_ptr<DataModel> target=model.lock())target->submitTask([owner,session,code,resume](DataModel* activeModel)mutable {
                owner->busy=false;if(activeModel->isClosed() || activeModel->getIsShuttingDown())return;
                if(code==0)*owner->account=session;
                else {logoutLocalAccount(*owner->account);StandardOut::singleton()->printf(MESSAGE_WARNING,"[OriginalLogin] Session validation failed code %d",code);}
                resume(code);
            },DataModelJob::Write);
            if(SUCCEEDED(apartment))CoUninitialize();
        }).detach();
    }
    void keyboard(std::string title,std::string initial,bool password,boost::function<void(std::string)> resume,boost::function<void(std::string)> error) {
        weak_ptr<DataModel> model=weak_from(DataModel::get(this));
        auto complete=[model,resume](std::string value) {
            if(shared_ptr<DataModel> target=model.lock())target->submitTask([resume,value](DataModel* activeModel){if(!activeModel->isClosed() && !activeModel->getIsShuttingDown())resume(value);},DataModelJob::Write);
        };
        if(GetEnvironmentVariableA("XBOX_ORIGINAL_LOGIN_TEST",0,0)>0) {
            char value[129]={};GetEnvironmentVariableA(password?"XBOX_ACCOUNT_TEST_PASSWORD":"XBOX_ACCOUNT_TEST_USERNAME",value,sizeof(value));
            complete(value);SecureZeroMemory(value,sizeof(value));return;
        }
        auto action=new std::function<void(HWND)>([title,initial,password,complete](HWND) {
            std::wstring result=avatarTextPrompt(wide(title),wide(initial),password,password?128:20);
            complete(utf8(result));if(!result.empty())SecureZeroMemory(&result[0],result.size()*sizeof(wchar_t));
        });
        HWND window=FindWindowA("XboxClientWindows",nullptr);
        if(!window || !PostMessageW(window,OriginalKeyboardMessage,0,reinterpret_cast<LPARAM>(action))){delete action;error("Text entry unavailable");}
    }
};
static Reflection::BoundFuncDesc<OriginalLoginBridge,std::string()> originalProfile(&OriginalLoginBridge::profile,"Profile",Security::RobloxScript);
static Reflection::BoundFuncDesc<OriginalLoginBridge,bool()> originalRegistration(&OriginalLoginBridge::openRegistration,"OpenRegistration",Security::RobloxScript);
static Reflection::BoundYieldFuncDesc<OriginalLoginBridge,int(std::string,std::string)> originalLogin(&OriginalLoginBridge::login,"Login","username","password",Security::RobloxScript);
static Reflection::BoundYieldFuncDesc<OriginalLoginBridge,int(std::string,std::string)> originalRegister(&OriginalLoginBridge::registerAccount,"Register","username","password",Security::RobloxScript);
static Reflection::BoundYieldFuncDesc<OriginalLoginBridge,int()> originalValidate(&OriginalLoginBridge::validate,"Validate",Security::RobloxScript);
static Reflection::BoundYieldFuncDesc<OriginalLoginBridge,std::string(std::string,std::string,bool)> originalKeyboard(&OriginalLoginBridge::keyboard,"Keyboard","title","initial","password",Security::RobloxScript);
RBX_REGISTER_CLASS(OriginalLoginBridge);
}
void installOriginalLoginBridge(Instance* parent,LocalAccountSession* account,bool external) {
    auto bridge=Creatable<Instance>::create<RBX::OriginalLoginBridge>();bridge->account=account;bridge->external=external;
    bridge->setName("OriginalLoginBridge");bridge->setRobloxLocked(true);bridge->setParent(parent);
}
