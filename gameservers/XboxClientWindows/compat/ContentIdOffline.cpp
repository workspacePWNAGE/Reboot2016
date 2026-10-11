#include "stdafx.h"

#include "Util/ContentId.h"
#include "Util/Http.h"
#include "Util/Statistics.h"
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>

#include "util/LegacyContentTable.h"
#include "v8datamodel/ContentProvider.h"
#include <boost/filesystem.hpp>



#undef HAVE_MEMCPY
#undef HAVE_CTYPE_H
#include "util/HTW3C.h"


#include "util/URL.h"

DYNAMIC_FASTFLAGVARIABLE(UrlReconstructToAssetGame, false);
FASTFLAG(OfflineAuthenticatedSession);
DYNAMIC_FASTFLAGVARIABLE(UrlReconstructToAssetGameSecure, false);
DYNAMIC_FASTFLAGVARIABLE(UrlReconstructRejectInvalidSchemes, false);

namespace
{			
	const char* kNamedUniverseAssetBase = "rbxgameasset://";
	const char* kNamedUniverseAssetAssetNameParam = "assetName=";

	const char* kValidAssetPaths[] =
	{
		
		"asset",
		"asset/",
		"asset/bodycolors.ashx",
		
		"thumbs/asset.ashx",
		"thumbs/avatar.ashx",
		"thumbs/script.png",
		"thumbs/staticimage",
		"game/tools/thumbnailasset.ashx",
		
		"game/edit.ashx",
		"game/gameserver.ashx",
		"game/join.ashx",
		"game/visit.ashx"
	};

	void createIdUrl(std::string& result, const std::string& baseUrl, const std::string& id)
	{
		result.reserve(baseUrl.size() + id.size() + 16);
		
		result = baseUrl;
		
		
		if (result.empty() || result[result.size() - 1] != '/')
			result += '/';
		
		result += "asset/?id=";
		result += id;
	}
}


namespace RBX
{
    static boost::once_flag legacyContentTableFlag = BOOST_ONCE_INIT;
	static scoped_ptr<LegacyContentTable> legacyContentTable;

    static void initLegacyContentTable()
    {
        legacyContentTable.reset(new LegacyContentTable);
    }

	bool operator<(const ContentId& a, const ContentId& b) {
		return a.id < b.id;
	}
	bool operator!=(const ContentId& a, const ContentId& b) {
		return a.id != b.id;
	}
	bool operator==(const ContentId& a, const ContentId& b) {
		return a.id == b.id;
	}
	ContentId ContentId::fromUrl(const std::string& url) {
		
		return ContentId(url);
	}

	ContentId ContentId::fromGameAssetName(const std::string& gameAssetName)
	{
		return ContentId(kNamedUniverseAssetBase + gameAssetName);
	}

	void ContentId::CorrectBackslash(std::string& id)
	{
		for (size_t i = 0; i < id.size(); ++i)
			if (id[i] == '\\')
				id[i] = '/';
	}
	
	bool ContentId::isNamedAsset() const
	{
		return id.compare(0, strlen(kNamedUniverseAssetBase), kNamedUniverseAssetBase) == 0;
	}

	bool ContentId::isConvertedNamedAsset() const
	{
		return id.find(kNamedUniverseAssetAssetNameParam) != std::string::npos;
	}

	void ContentId::convertAssetId(const std::string& baseUrl, int universeId)
	{
		if(isAssetId())
		{
            
            
            
            const std::string number=id.substr(13);
            if(!number.empty() && number.find_first_not_of("0123456789")==std::string::npos &&
                boost::filesystem::is_regular_file(ContentProvider::assetFolder()+"offline-avatar/cache/"+number))
                id="rbxasset://offline-avatar/cache/"+number;
            else createIdUrl(id, baseUrl, number);
        }
		else if(isRbxHttp()){
			id = baseUrl + id.substr(10);
		}
		else if (isNamedAsset())
		{
			RBXASSERT(!baseUrl.empty());
			if (!baseUrl.empty())
			{
				std::stringstream ss;
				ss << baseUrl;
				if (baseUrl[baseUrl.size() - 1] != '/')
				{
					ss << '/';
				}
				ss << "asset/"
					<< "?universeId=" << universeId
					<< "&" << kNamedUniverseAssetAssetNameParam << RBX::Http::urlEncode(id.substr(strlen(kNamedUniverseAssetBase)))
					<< "&skipSigningScripts=1";

				id = ss.str();
			}
		}
	}
	
	void ContentId::convertToLegacyContent(const std::string& baseUrl)
	{	
		if (isAsset())
		{
            boost::call_once(initLegacyContentTable, legacyContentTableFlag);

			const std::string& mappedAssetId = legacyContentTable->FindEntry(id);
			
			if (!mappedAssetId.empty())
            {
                if (isdigit(mappedAssetId[0]))
                    createIdUrl(id, baseUrl, mappedAssetId);
                else
                    id = mappedAssetId;
            }
		}
	}

	bool ContentId::reconstructUrl(const std::string& baseUrl, const char* const paths[], const int pathCount)
    {
        
        
        if (FFlag::OfflineAuthenticatedSession && !baseUrl.empty()) {
            std::string origin=baseUrl;
            while(!origin.empty() && origin.back()=='/') origin.pop_back();
            const std::string prefix=origin+"/asset/?id=";
            if(id.compare(0,prefix.size(),prefix)==0) {
                const std::string number=id.substr(prefix.size());
                if(!number.empty() && number.find_first_not_of("0123456789")==std::string::npos) return true;
            }
        }
		boost::scoped_array<char> url(new char[id.length()+1]);

		
		char* urlIter = url.get();
		for (size_t i = 0; i < id.length(); ++i)
		{
			if (id[i] != ' ')
			{
				*urlIter = id[i];
				urlIter++;
			}
		}
		*urlIter = '\0';
        
        if (DFFlag::UseNewUrlClass)
        {
            const RBX::Url parsed = RBX::Url::fromString(url.get());

            
            
            
            
            
            
            if (!parsed.hasHttpScheme())
            {
                return !DFFlag::UrlReconstructRejectInvalidSchemes || parsed.hasValidScheme();
            }
            
            
            if (!parsed.isValid() || parsed.pathIsEmpty())
            {
                return false;
            }
            
            
            std::string scheme = parsed.scheme();
            std::string host = parsed.host();
            std::string path = parsed.path();

            static const std::string testsite_domain = "robloxlabs.com";

            const RBX::Url baseUrlParsed = RBX::Url::fromString(baseUrl);

            for (int i = 0; i < pathCount; i++)
            {
                if (parsed.pathEqualsCaseInsensitive(paths[i]))
                {
                    if (DFFlag::UrlReconstructToAssetGame)
                    {
                        
                        if (baseUrlParsed.isSubdomainOf(testsite_domain)
                            && !parsed.isSubdomainOf(testsite_domain))
                        {
                            host = "assetgame.roblox.com";
                        }
                        else
                        {
                            scheme = baseUrlParsed.scheme();
                            host = baseUrlParsed.host();
                            
                            if (boost::starts_with(host, "www."))
                            {
                                host.replace(0, 3, "assetgame");
                            }
                        }

                        if (DFFlag::UrlReconstructToAssetGameSecure)
                        {
                            scheme = "https";
                        }
                    }
                    else
                    {
                        if (parsed.isSubdomainOf(testsite_domain))
                        {
                            scheme = baseUrlParsed.scheme();
                            host = baseUrlParsed.host();
                            path = baseUrlParsed.path() + path;
                        }
                        else
                        {
                            scheme = "http";
                            host = "www.roblox.com";
                        }
                    }

                    id = RBX::Url::fromComponents(scheme, host, path, parsed.query(), parsed.fragment()).asString();

                    return true;
                }
            } 
            
            return false;
        } 

        
        char* simplifiedUrl = url.get();
        simplifiedUrl = HTSimplify(&simplifiedUrl);
        boost::scoped_ptr<char> chost(HTParse(simplifiedUrl, NULL, PARSE_HOST));
        boost::scoped_ptr<char> cscheme(HTParse(simplifiedUrl, NULL, PARSE_ACCESS));
        boost::scoped_ptr<char> cpath(HTParse(simplifiedUrl, NULL, PARSE_PATH));

        std::string scheme = cscheme.get();
        std::string path = cpath.get();
        std::string host = chost.get();

        if (scheme != "http" && scheme != "https")
            return true;

        
        int pathStart = path.find_first_not_of("/");
        if (pathStart == std::string::npos)
            return false; 

        path = path.substr(pathStart);

        for (int i = 0; i < pathCount; i++)
        {
            size_t pathLength = strlen(paths[i]);

            if (boost::istarts_with(path, paths[i]) && (path.size() == pathLength || path[pathLength] == '?'))
            {
                static const char* domain = ".robloxlabs.com";

                if (DFFlag::UrlReconstructToAssetGame)
                {
                    
                    if (baseUrl.find(domain) != std::string::npos && host.find(domain) == std::string::npos)
                        id = "http://www.roblox.com/" + path;
                    else
                        id = baseUrl + (baseUrl[baseUrl.size()-1] != '/' ? "/" : "") + path;

                    if (DFFlag::UrlReconstructToAssetGameSecure)
                    {
                        
                        if (boost::starts_with(id, "http://www."))
                            id = "https://assetgame." + id.substr(11);
                        else if (boost::starts_with(id, "https://www."))
                            id = "https://assetgame." + id.substr(12);
                    }
                    else
                    {
                        if (boost::starts_with(id, "http://www."))
                            id = "http://assetgame." + id.substr(11);
                        else if (boost::starts_with(id, "https://www."))
                            id = "https://assetgame." + id.substr(12);
                    }
                }
                else
                {
                    size_t robloxLabsPos = host.find(domain);
                    if (robloxLabsPos != std::string::npos)
                        id = baseUrl + "/" + path;
                    else
                        id = "http://www.roblox.com/" + path;
                }

                return true;
            }
        } 

		return false;
	}

	bool ContentId::reconstructAssetUrl(const std::string& baseUrl)
	{
		return reconstructUrl(baseUrl, kValidAssetPaths, ARRAYSIZE(kValidAssetPaths));
	}

	std::string ContentId::getAssetId() const
	{
		if(isAssetId()){
			return id.substr(13); 
		}
		else if(isHttp() || isRbxHttp()){
			std::string lower = id;
			std::transform(lower.begin(), lower.end(), lower.begin(), tolower);
			std::string::size_type pos = lower.find("id=");
			if(pos != std::string::npos){
				pos += 3;
				std::string::size_type second_pos = lower.find('&',pos);
				if(second_pos == std::string::npos){
					second_pos = lower.size();
				}
				return lower.substr(pos, (second_pos-pos));
			}
		}
		return "";
	}

	std::string ContentId::getAssetName() const
	{
		if (!isNamedAsset())
		{
			return "";
		}
		return id.substr(strlen(kNamedUniverseAssetBase));
	}

	std::string ContentId::getUnConvertedAssetName() const
	{
		if (!isConvertedNamedAsset())
		{
			return "";
		}
		std::string::size_type namedAssetParamPos = id.find(kNamedUniverseAssetAssetNameParam) +
			strlen(kNamedUniverseAssetAssetNameParam);
		std::string::size_type namedAssetParamEnd = id.find("&", namedAssetParamPos);
		std::string::size_type namedAssetParamLength = 
			namedAssetParamEnd == std::string::npos ?
				std::string::npos : namedAssetParamEnd - namedAssetParamPos;

		std::string urlEncodedNamedAsset = id.substr(namedAssetParamPos, namedAssetParamLength);
		return Http::urlDecode(urlEncodedNamedAsset);
	}

	ContentId ContentId::fromAssets(const char* filePath) {
		std::string header("rbxasset://");
		return ContentId(header + filePath);
	}

	std::size_t hash_value(const ContentId& id)
	{
		boost::hash<std::string> hasher;
		return hasher(id.toString());
	}
}
