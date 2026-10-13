local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules=root:WaitForChild('Modules')
local adapter={}
function adapter.Apply(http)
    local oldPlatform,oldTotal=http.GetPlatformUserBalanceAsync,http.GetTotalUserBalanceAsync
    local function cachedBalance()
        local bridge=modules:FindFirstChild('OriginalLoginBridge')
        if not bridge then return nil end
        local ok,profile=pcall(function() return game:GetService('HttpService'):JSONDecode(bridge:Profile()) end)
        if not ok or not profile.user_id then return nil end
        local value=profile.bux
        if type(value)=='number' and value>=0 and value==math.floor(value) then return value end
        return nil
    end
    local function balance()
        local bridge=modules:FindFirstChild('OriginalLoginBridge')
        if bridge then
            local ok,value=pcall(function() return bridge:RefreshBalance() end)
            if ok and type(value)=='number' and value>=0 then return value end
        end
        return cachedBalance()
    end
    function http.GetPlatformUserBalanceAsync()
        local value=balance()
        if value~=nil then return {Robux=value} end
        return oldPlatform()
    end
    function http.GetTotalUserBalanceAsync()
        local value=balance()
        if value~=nil then return {robux=value} end
        return oldTotal()
    end
    local strings=require(modules.LocalizedStrings)
    local original=strings.LocalizedString
    function strings:LocalizedString(key,...)
        if cachedBalance()~=nil then
            if key=='CatalogWord' then return 'BUXS' end
            if key=='RobuxBalanceTitle' then return 'My Buxs' end
            if key=='RobuxBalanceOverlayTitle' then return 'Buxs Balance' end
            if key=='RobuxBalanceOverlayPhrase' then return 'Your saved Reboot16 account balance.' end
        end
        return original(self,key,...)
    end
end
return adapter
