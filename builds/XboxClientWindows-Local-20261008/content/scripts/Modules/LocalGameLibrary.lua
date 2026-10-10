local modules=game:GetService('CoreGui'):WaitForChild('RobloxGui'):WaitForChild('Modules')
local bridge=modules:FindFirstChild('LocalLibraryBridge')
local session=require(modules.OfflineLocalSession)
local library={Enabled=bridge~=nil and not session.DemoTest,Games={},Recent={}}
local fetching=false
function library.Refresh()
    if not library.Enabled then return end
    while fetching do wait() end
    fetching=true
    local ok,result=pcall(function() return game:GetService('HttpService'):JSONDecode(bridge:Refresh()) end)
    if ok then
        library.Id=result.library_id;library.Games=result.games or {};library.ById={}
        library.ActiveGame=result.active_game_id
        for _,item in pairs(library.Games) do library.ById[item.game_id]=item end
        library.Favourites={}
        for _,id in pairs(result.favourites or {}) do
            if library.ById[id] then table.insert(library.Favourites,library.ById[id]) end
        end
        library.Recent={}
        for _,record in pairs(result.recent or {}) do
            local item=library.ById[record.game_id]
            if item then
                item.last_played=record.last_played;item.play_count=record.play_count
                table.insert(library.Recent,item)
            end
        end
        print('[Library] Menu refreshed: '..#library.Games..' games, '..#library.Recent..' recent')
    else
        warn('[Library] Refresh failed: '..tostring(result))
        library.Games={};library.Recent={};library.ById={}
    end
    fetching=false
end
function library.Find(id) return library.ById and library.ById[id] end
function library.Collection(kind)
    local collection={}
    function collection:GetSortAsync(startIndex,pageSize)
        local data=kind=='recent' and library.Recent or kind=='favourites' and (library.Favourites or {}) or kind=='empty' and {} or library.Games
        local page={Data={},Count=0}
        for i=startIndex+1,math.min(#data,startIndex+pageSize) do table.insert(page.Data,data[i]) end
        page.Count=#page.Data
        local function fields(key,fallback)
            local result={};for i,item in pairs(page.Data) do result[i]=item[key] or fallback end;return result
        end
        function page:GetPagePlaceIds() return fields('game_id') end
        function page:GetPagePlaceNames() return fields('title','Local Game') end
        function page:GetPageIconIds() return fields('thumbnail',false) end
        function page:GetCreatorNames() return fields('creator','Local Library') end
        function page:GetPageCreatorUserIds() return fields('creator_id',0) end
        function page:GetPageVoteData() local rows={};for i in pairs(page.Data) do rows[i]={UpVotes=0,DownVotes=0} end;return rows end
        return page
    end
    return collection
end
function library.Launch(id)
    if not library.Find(id) then library.Refresh() end
    if not library.Find(id) then warn('[Library] Game unavailable: '..tostring(id));return end
    -- Original Xbox join/error/view handling and existing native session.
    require(modules.GameJoin):StartGame(0,id,0)
end
library.Placeholder='rbxasset://textures/ui/Shell/Icons/NoGameImageIcon.png'
function library.IsImage(uri)
    return type(uri)=='string' and (uri==library.Placeholder or
        string.match(uri,'^rbxasset://library%-images/[a-f0-9]+_%d+_%d+_[%w]+%.png$')~=nil)
end
function library.GameData(id)
    local item=library.Find(id)
    if not item then error('Selected game is unavailable') end
    local data={LocalLibrary=true,Data={Name=item.title}}
    local preferences
    local function request(operation)
        local ok,result=pcall(function()
            return game:GetService('HttpService'):JSONDecode(bridge:Preferences(id,operation))
        end)
        if ok then preferences=result;return true end
        warn('[GameDetails] '..id..' '..operation..': '..tostring(result))
        return false
    end
    function data:GetCreatorName() return item.creator or 'Local Library' end
    function data:GetDescription() return item.description or 'A local game from this library.' end
    function data:GetIsFavoritedByUser()
        if not preferences then request('get') end
        return preferences and preferences.favourite or false
    end
    function data:GetVoteDataAsync()
        if not preferences then request('get') end
        local p=preferences
        if not p then return {UpVotes=0,DownVotes=0,CanVote=false,CantVoteReason='PlayGame',Unavailable=true} end
        local vote='null';if p.vote==1 then vote=true elseif p.vote==-1 then vote=false end
        return {UpVotes=p.positive,DownVotes=p.negative,UserVote=vote,CanVote=true,CantVoteReason=''}
    end
    function data:PostVoteAsync(vote)
        return request(vote==true and 'positive' or vote==false and 'negative' or 'clear')
    end
    function data:PostFavoriteAsync()
        if not preferences and not request('get') then return false,'Failed' end
        return request(preferences.favourite and 'unfavourite' or 'favourite'), 'Failed'
    end
    function data:GetGameIconIdAsync() return item.thumbnail~='' and item.thumbnail or library.Placeholder end
    function data:GetThumbnailIdsAsync()
        return {self:GetGameIconIdAsync(),item.image1~='' and item.image1 or library.Placeholder,
            item.image2~='' and item.image2 or library.Placeholder}
    end
    function data:GetLastUpdated() return '' end
    function data:GetCreationDate() return '' end
    function data:GetMaxPlayers() return 20 end
    function data:GetOverridesDefaultAvatar() return false end
    function data:GetCreatorUserId() return 0 end
    function data:GetRecommendedGamesAsync() return {} end
    function data:GetBadgeDataAsync() return {} end
    print('[GameDetails] Open '..id..': '..item.title)
    return data
end
return library
