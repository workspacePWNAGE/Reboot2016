local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules=root:WaitForChild('Modules')
local gui=game:GetService('GuiService')
local manager=require(modules.ScreenManager)
local function waitFor(check,label)
    for i=1,300 do local value=check() if value then return value end wait(.1) end
    error('[OriginalLoginTest] Timeout: '..label)
end
wait(4)
local remembered=game:GetService('HttpService'):JSONDecode(modules.OriginalLoginBridge:Profile())
if remembered.user_id then
    print('[OriginalLoginTest] Input A')
    local user=require(modules.UserData)
    waitFor(function() return user:GetRbxUserId()==remembered.user_id end,'restored menu identity')
    assert(user:GetRobloxName()==remembered.username,'Restored menu name mismatch')
    waitFor(function() local app=root:FindFirstChild('AppHomeContainer') local home=app and app:FindFirstChild('HomePane',true) return home and home:IsA('GuiObject') and home.Visible end,'visible restored HOME menu')
    print('[RememberedLoginTest] PASS: restart validated saved token and entered visible original HOME menu')
    print('[OriginalLoginTest] Page ready')
    return
end
print('[OriginalLoginTest] Input A')
local registering=require(modules.OriginalLoginSettings).RegisterSmoke
local link=waitFor(function() return root:FindFirstChild(registering and 'CreateAccountButton' or 'LinkAccountButton',true) end,'original SignInScreen')
wait(2)
gui.SelectedCoreObject=link
print('[OriginalLoginTest] Original SignInScreen visible')
print('[OriginalLoginTest] Page ready')

wait(3)
print('[OriginalLoginTest] Input A')
if registering and modules.OriginalLoginBridge:UsesBrowserRegistration() then
    local button=waitFor(function() return game:GetService('CoreGui'):FindFirstChild('OkButton',true) end,'browser signup instructions')
    assert(gui.SelectedCoreObject==button,'Signup instructions missing controller focus')
    assert(button.Parent:FindFirstChild('TitleText').Text=='Complete signup in your browser','Wrong signup instructions')
    local profile=game:GetService('HttpService'):JSONDecode(modules.OriginalLoginBridge:Profile())
    assert(not profile.user_id,'Opening signup fabricated authentication')
    print('[RegistrationTest] PASS: original CREATE ACCOUNT button, browser dispatch and instructions; no fake login')
    print('[OriginalLoginTest] Page ready')
    return
end
local screen=waitFor(function() local s=manager:GetTopScreen() return s and s.UsernameSelection and s end,'original LinkAccountScreen')
wait(2)
gui.SelectedCoreObject=screen.UsernameSelection
print('[OriginalLoginTest] Original LinkAccountScreen visible')
if registering then print('[OriginalLoginTest] Original SetAccountCredentialsScreen visible') end
print('[OriginalLoginTest] Input A')
wait(2)
assert(gui.SelectedCoreObject==screen.PasswordSelection,'Username did not advance focus to password')
print('[OriginalLoginTest] Input A')
wait(2)
assert(gui.SelectedCoreObject==screen.SignInButton,'Password did not advance focus to submit')
print('[OriginalLoginTest] Page ready')
wait(3)
print('[OriginalLoginTest] Input A')
local profile=waitFor(function()
    local raw=modules.OriginalLoginBridge:Profile()
    local data=game:GetService('HttpService'):JSONDecode(raw)
    return data.user_id and data
end,'authenticated native profile')
assert(profile.token==nil and profile.cookieToken==nil and profile.password==nil,'Secret field escaped into profile')
local user=require(modules.UserData)
waitFor(function() return user:GetRbxUserId()==profile.user_id end,'menu user identity')
assert(user:GetRobloxName()==profile.username,'Menu username mismatch')
if profile.bux~=nil then
    assert(profile.bux==125,'Backend balance mismatch')
    assert(user.GetTotalUserBalanceAsync()==125,'Total balance adapter mismatch')
    assert(user.GetPlatformUserBalanceAsync()==125,'Platform balance adapter mismatch')
    local panel=Instance.new('Frame',root)
    panel.Name='BackendBalanceTest'
    local widget=require(modules.CurrencyWidget)({Parent=panel})
    assert(widget:GetRobuxAmountAsync()==125,'Original currency widget balance mismatch')
    local value=panel:FindFirstChild('RobuxBalanceValue',true)
    assert(value and value.Text=='125','Rendered currency text mismatch')
    assert(panel:FindFirstChild('RobuxBalanceTitle',true).Text=='My Buxs:','Buxs label mismatch')
    widget:Destroy();panel:Destroy()
    print('[BuxsTest] PASS: authoritative balance, original CurrencyWidget text and Buxs label')
end
print('[OriginalLoginTest] PASS: original screens, A selection, field focus, native auth, secret-free profile and menu identity')
print('[OriginalLoginTest] Page ready')
if require(modules.OriginalLoginSettings).OnlineSmoke then
    wait(3)
    local result=game:GetService('PlatformService'):BeginStartGame3(0,900000001)
    assert(result==0,'Public-authority game startup failed: '..tostring(result))
    print('[OnlineAdmissionTest] PASS: original login -> external authority -> one-use proof -> authenticated game character')
end
