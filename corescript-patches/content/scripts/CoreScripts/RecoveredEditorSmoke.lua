local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules=root:WaitForChild('Modules');local http=game:GetService('HttpService');local gui=game:GetService('GuiService')
local bridge=modules:WaitForChild('LocalLibraryBridge')
local function request(op,body)return http:JSONDecode(bridge:Profile(op,body and http:JSONEncode(body)or''))end
local function press(key)print('[DetailsTest] Input '..key);wait(2)end
wait(10)
local saved=request('get');saved.appearance.clothing.shirt=nil
request('save',{profile_version=saved.profile_version,appearance=saved.appearance})
local events=require(modules.EventHub)
events:dispatchEvent(events.Notifications.NavigateToEquippedAvatar);wait(5)
press('Y')
local editor=assert(root.Parent:FindFirstChild('RecoveredAvatarCustomization'),'Recovered GUI not opened')
assert(editor:FindFirstChild('Frame')and editor:FindFirstChild('AvatarTypeSwitch'),'Archived hierarchy missing')
assert(workspace:FindFirstChild('LocalAvatarPreview'),'R6 preview missing')
assert(workspace:FindFirstChild('RecoveredAvatarEnvironment'),'Original 3D room missing')
assert(workspace.RecoveredAvatarEnvironment:FindFirstChild('ParticleScreenPart'),'Original particle screen missing')
assert(game:GetService('Lighting').Brightness==1,'Original lighting not applied')
assert(workspace.CurrentCamera.FieldOfView==70,'Original FOV missing')
print('[RecoveredEnvironmentTest] Scene, lighting, particles and camera PASS')
print('[DetailsTest] Page ready recovered editor');wait(3)
local command=assert(editor:FindFirstChild('RecoveredEditorTest'),'Editor initialization failed')
command:Invoke('category','Shirts');wait(2)
local shirt=assert(editor.Frame.ScrollingFrame:FindFirstChild('AssetButton900100001'),'Real local shirt missing')
gui.SelectedCoreObject=shirt;press('A')
local appearance=http:JSONDecode(command:Invoke('appearance'))
assert(appearance.clothing.shirt==900100001,'Controller A did not equip draft shirt')
assert(request('get').appearance.clothing.shirt==nil,'Draft saved without explicit save')
press('X');assert(request('get').appearance.clothing.shirt==900100001,'Controller X did not persist shirt')
print('[RecoveredEditorTest] Archived hierarchy, controller equip and authenticated save PASS')
command:Invoke('category','Skin Tone');wait(1)
local before=gui.SelectedCoreObject;press('Right');assert(gui.SelectedCoreObject~=before,'D-pad navigation failed')
before=gui.SelectedCoreObject;press('StickRight');assert(gui.SelectedCoreObject~=before,'Analog navigation failed')
local colors=editor.Frame.ScrollingFrame:GetChildren();local chosen
for _,b in pairs(colors)do if b:IsA('GuiButton')and b.Name:match('^Color_')and tonumber(b.Name:match('%d+'))~=saved.appearance.body_colors.head then chosen=b;break end end
gui.SelectedCoreObject=assert(chosen,'Color palette missing');press('A');press('X')
local profile=request('get');assert(profile.appearance.body_colors.head~=saved.appearance.body_colors.head,'Color not saved');assert(profile.appearance.body_colors.torso==saved.appearance.body_colors.torso,'Other body part changed')
press('R3');wait(1)
for _,b in pairs(editor.MenuFrame:GetChildren())do if b:IsA('GuiButton')and b.Text=='torso'then gui.SelectedCoreObject=b end end
press('A');gui.SelectedCoreObject=chosen.Parent and chosen or editor.Frame.ScrollingFrame:FindFirstChild(chosen.Name);press('A');press('X')
local originalCamera=workspace.CurrentCamera.CoordinateFrame
press('Y');assert(editor.Frame.Position.Y.Scale>.9,'Full view did not hide controls')
assert((workspace.CurrentCamera.CoordinateFrame.p-originalCamera.p).magnitude<100,'Camera tween escaped preview coordinates')
print('[DetailsTest] Page ready recovered editor fullview');wait(2);press('B')
assert(editor.Frame.Position.Y.Scale<.2,'Full-view Back did not restore console layout')
command:Invoke('category','Hats');wait(1);gui.SelectedCoreObject=assert(editor.Frame.ScrollingFrame:FindFirstChild('AssetButton900100003'),'Bundled hat missing');press('A')
command:Invoke('category','Torsos');wait(1);gui.SelectedCoreObject=assert(editor.Frame.ScrollingFrame:FindFirstChild('AssetButton900100002'),'Bundled package missing');press('A');press('X')
profile=request('get');assert(profile.appearance.accessories[1]==900100003 and profile.appearance.body_package==900100002,'Package/accessory did not persist')
local preview=workspace.LocalAvatarPreview
for _,name in pairs({'Torso','Left Arm','Right Arm','Left Leg','Right Leg'})do
 local part=assert(preview:FindFirstChild(name),'R6 preview part missing: '..name)
 assert(part.Anchored and (part.Position-preview.Head.Position).magnitude<6,'Accessory broke preview pose: '..name)
end
print('[RecoveredEditorTest] Accessory-equipped R6 preview pose PASS')
local hat=assert(preview:FindFirstChild('LocalDemoHat'),'Preview hat missing')
assert(hat.Handle.Anchored and (hat.Handle.Position-preview.Head.Position).magnitude<3,'Preview hat attachment missing')
command:Invoke('category','Hats');wait(1)
gui.SelectedCoreObject=editor.Frame.ScrollingFrame.AssetButton900100003;press('A')
gui.SelectedCoreObject=assert(editor.Frame.ScrollingFrame:FindFirstChild('AssetButton527365852'),'Imported Gift13 hat missing from owned catalog');press('A');press('X')
local gift=assert(workspace.LocalAvatarPreview:FindFirstChild('Gift13'),'Imported hat missing from engine preview')
assert(gift.Handle.Mesh.MeshId=='rbxasset://offline-avatar/cache/527245069' and gift.Handle.Mesh.TextureId=='rbxasset://offline-avatar/cache/527244862','Imported hat lost original dependencies')
assert((gift.Handle.Position-workspace.LocalAvatarPreview.Head.Position).magnitude<3,'Imported hat is not attached')
print('[ImportedHatTest] Gift13 selection, authenticated save and legacy preview PASS')
command:Invoke('category','Torsos');wait(1)
print('[RecoveredEditorTest] D-pad, analog, colors, body-part selection, fullview, package and hat PASS')
print('[AvatarEditorTest] Resize requested');wait(3)
print('[DetailsTest] Page ready recovered editor resized');wait(3)
press('Menu')
for _,b in pairs(editor.MenuFrame:GetChildren())do if b:IsA('GuiButton')and b.Text=='Save current outfit'then gui.SelectedCoreObject=b end end
press('A');wait(1)
local outfits=request('outfits').outfits;local outfit
for _,o in pairs(outfits)do if o.name=='Recovered Outfit'then outfit=o end end
assert(outfit,'UI outfit save failed')
gui.SelectedCoreObject=assert(editor.Frame.ScrollingFrame:FindFirstChild('AssetButtonOutfit'..outfit.outfit_id),'Saved outfit tile missing');press('Menu')
for _,b in pairs(editor.MenuFrame:GetChildren())do if b:IsA('GuiButton')and b.Text=='Equip and save'then gui.SelectedCoreObject=b end end
press('A');assert(request('get').equipped_outfit_id==outfit.outfit_id,'UI outfit equip failed')
gui.SelectedCoreObject=assert(editor.Frame.ScrollingFrame:FindFirstChild('AssetButtonOutfit'..outfit.outfit_id),'Saved outfit tile missing');press('Menu')
for _,b in pairs(editor.MenuFrame:GetChildren())do if b:IsA('GuiButton')and b.Text=='Use on login'then gui.SelectedCoreObject=b end end
press('A');assert(request('get').default_outfit_id==outfit.outfit_id,'Outfit default not saved')
gui.SelectedCoreObject=editor.Frame.ScrollingFrame:FindFirstChild('AssetButtonOutfit'..outfit.outfit_id);press('Menu')
for _,b in pairs(editor.MenuFrame:GetChildren())do if b:IsA('GuiButton')and b.Text=='Rename'then gui.SelectedCoreObject=b end end
press('A')
local renamed=false;for _,o in pairs(request('outfits').outfits)do if o.outfit_id==outfit.outfit_id and o.name=='Renamed Recovered Outfit'then renamed=true end end;assert(renamed,'UI outfit rename failed')
gui.SelectedCoreObject=editor.Frame.ScrollingFrame:FindFirstChild('AssetButtonOutfit'..outfit.outfit_id);press('Menu')
for _,b in pairs(editor.MenuFrame:GetChildren())do if b:IsA('GuiButton')and b.Text=='Delete'then gui.SelectedCoreObject=b end end
press('A');for _,o in pairs(request('outfits').outfits)do assert(o.outfit_id~=outfit.outfit_id,'UI outfit delete failed')end
print('[RecoveredEditorTest] Original outfit tiles and menu save/default/rename/delete PASS')
print('[DetailsTest] Page ready recovered editor equipped');wait(3)
press('B');assert(not root.Parent:FindFirstChild('RecoveredAvatarCustomization'),'Editor not destroyed on return')
assert(not workspace:FindFirstChild('LocalAvatarPreview'),'Preview leaked on return')
press('Y');editor=assert(root.Parent:FindFirstChild('RecoveredAvatarCustomization'),'Reopen failed')
command=editor:FindFirstChild('RecoveredEditorTest');command:Invoke('category','Shirts');wait(2)
gui.SelectedCoreObject=editor.Frame.ScrollingFrame.AssetButton900100001;press('A');press('Menu')
for _,b in pairs(editor.MenuFrame:GetChildren())do if b:IsA('GuiButton')and b.Text=='Reset draft to default'then gui.SelectedCoreObject=b end end
press('A');assert(http:JSONDecode(command:Invoke('appearance')).clothing.shirt==nil,'Default reset did not clear draft clothing');press('B')
assert(request('get').appearance.clothing.shirt==900100001,'Cancel overwrote saved shirt')
events:dispatchEvent(events.Notifications.OpenGameDetail,900000100,'MyTestPlace',false);wait(4)
local function find(parent,name)if parent.Name==name and parent:IsA('GuiObject')then return parent end;for _,child in pairs(parent:GetChildren())do local v=find(child,name);if v then return v end end end
gui.SelectedCoreObject=assert(find(root,'PlayButton'),'Game play missing');press('A');wait(15)
game:GetService('PlatformService'):RequestGameShutdown();wait(5)
events:dispatchEvent(events.Notifications.NavigateToEquippedAvatar);wait(5)
press('Y');editor=assert(root.Parent:FindFirstChild('RecoveredAvatarCustomization'),'Editor unavailable after game')
assert(game:GetService('PlatformService').BlurIntensity==0 and game:GetService('PlatformService').Contrast==0,'Shell post effects leaked into reopened editor')
for _,card in pairs(editor.Frame.ScrollingFrame:GetChildren())do if card:IsA('GuiButton')then assert(card.AbsoluteSize.X<editor.Frame.ScrollingFrame.AbsoluteSize.X*.5,'Reopened editor retained archived phone grid dimensions')end end
print('[DetailsTest] Page ready recovered editor returned');wait(3);press('B')
print('[RecoveredEditorTest] Gameplay appearance, reopen/cancel and menu return PASS')
