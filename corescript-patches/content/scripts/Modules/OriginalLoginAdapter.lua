local root = game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules = root:WaitForChild('Modules')
local settings = require(modules:WaitForChild('OriginalLoginSettings'))
local adapter = {}
function adapter.Apply(manager)
    if not settings.Enabled then return end
    local bridge = modules:WaitForChild('OriginalLoginBridge')
    local errors=require(modules.Errors)
    errors.Authentication[18]={Title='Verification required',Msg='The account server requires a captcha. This client needs the compatible login flow before it can sign in.',Code=919}
    errors.Authentication[19]={Title='Login request rejected',Msg='The account server rejected the request format. Please update the client/backend compatibility.',Code=920}

    function manager:BeginAuthenticationAsync()
        local profile=game:GetService('HttpService'):JSONDecode(bridge:Profile())
        return profile.user_id and 0 or 2
    end
    function manager:LinkAccountAsync(username,password) return bridge:Login(username,password) end
    function manager:UnlinkAccountAsync() return bridge:ForgetSession() end
    function manager:SignInAsync() return bridge:Validate() end
    function manager:HasLinkedAccountAsync() return 2 end
    function manager:HasRobloxCredentialsAsync() return 0 end
    function manager:GenerateAccountAsync(username,password) return bridge:Register(username,password) end
    function manager:IsValidUsernameAsync(username)
        local valid=type(username)=='string' and #username>=3 and #username<=20 and username:match('^[A-Za-z0-9_]+$')~=nil
        return valid,valid and nil or 'Use 3-20 letters, numbers or underscores.'
    end
    function manager:IsValidPasswordAsync(username,password)
        local valid=type(password)=='string' and #password>=8 and #password<=72
        return valid,valid and nil or 'Use a password between 8 and 72 bytes.'
    end
end
function adapter.Scale(container)
    if not settings.Enabled then return end
    local originals = {}
    local function collect(parent)
        for _,child in pairs(parent:GetChildren()) do
            if child:IsA('GuiObject') then
                originals[child] = {Size=child.Size, Position=child.Position}
                if child:IsA('TextLabel') or child:IsA('TextButton') then originals[child].FontSize=child.FontSize end
            end
            collect(child)
        end
    end
    collect(container)
    local utility = require(modules.Utility)
    local sizes = {14,18,24,28,32,36,42,48,60}
    local function update()
        local scale = math.min(root.AbsoluteSize.X/1920,root.AbsoluteSize.Y/1080)
        if scale <= 0 then return end
        for object,data in pairs(originals) do
            local s,p=data.Size,data.Position
            object.Size=UDim2.new(s.X.Scale,s.X.Offset*scale,s.Y.Scale,s.Y.Offset*scale)
            object.Position=UDim2.new(p.X.Scale,p.X.Offset*scale,p.Y.Scale,p.Y.Offset*scale)
            if data.FontSize then
                local target=utility.ConvertFontSizeEnumToInt(data.FontSize)*scale
                local chosen=sizes[1]
                for _,size in ipairs(sizes) do if math.abs(size-target)<math.abs(chosen-target) then chosen=size end end
                object.FontSize=Enum.FontSize['Size'..chosen]
            end
        end
    end
    local connection=root.Changed:connect(function(prop) if prop=='AbsoluteSize' then update() end end)
    container.AncestryChanged:connect(function() if not container.Parent then connection:disconnect() end end)
    update()
end
return adapter
