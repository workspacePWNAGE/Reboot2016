-- Local profile adapter for original Xbox shell components; no service credentials in Lua.
local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules=root:WaitForChild('Modules')
local Utility=require(modules.Utility)
local Style=require(modules.GlobalSettings)
local BaseTile=require(modules.BaseTile)
local gui=game:GetService('GuiService')
local http=game:GetService('HttpService')
local bridge=modules:WaitForChild('LocalLibraryBridge')
local function copy(v)return http:JSONDecode(http:JSONEncode(v))end
return function(parent)
 local this={};local saved,draft,catalog,outfits;local focused=false;local busy=false
 local camera,oldCamera,oldType,backgroundState,effects
 local frame=Utility.Create'Frame'{Name='LocalAvatarEditor',Size=UDim2.new(1,0,1,0),BackgroundTransparency=1,Visible=false,Parent=parent}
 local nameLabel=Utility.Create'TextLabel'{Name='MyNameLabel',Size=UDim2.new(.38,0,.06,0),TextXAlignment=Enum.TextXAlignment.Left,BackgroundTransparency=1,Font=Style.RegularFont,FontSize=Style.SubHeaderSize,TextColor3=Style.WhiteTextColor,Parent=frame}
 local panel=Utility.Create'Frame'{Name='AvatarControls',Position=UDim2.new(.4,0,0,0),Size=UDim2.new(.6,0,.84,0),BackgroundTransparency=1,Parent=frame}
 local title=Utility.Create'TextLabel'{Size=UDim2.new(1,0,.1,0),BackgroundTransparency=1,TextXAlignment=Enum.TextXAlignment.Left,Font=Style.RegularFont,FontSize=Style.SubHeaderSize,TextColor3=Style.WhiteTextColor,Parent=panel}
 local tabs=Utility.Create'Frame'{Position=UDim2.new(0,0,.12,0),Size=UDim2.new(1,0,.12,0),BackgroundTransparency=1,Parent=panel}
 local content=Utility.Create'Frame'{Name='AvatarCollection',Position=UDim2.new(0,0,.28,0),Size=UDim2.new(1,0,.53,0),BackgroundTransparency=1,Parent=panel}
 local actions=Utility.Create'Frame'{Position=UDim2.new(0,0,.83,0),Size=UDim2.new(1,0,.17,0),BackgroundTransparency=1,Parent=panel}
 local footer=Utility.Create'Frame'{Position=UDim2.new(.4,0,.86,0),Size=UDim2.new(.6,0,.09,0),BackgroundTransparency=1,Parent=frame}
 local status=Utility.Create'TextLabel'{Name='AvatarStatus',Position=UDim2.new(.4,0,.96,0),Size=UDim2.new(.6,0,.04,0),BackgroundTransparency=1,TextColor3=Style.WhiteTextColor,Font=Style.RegularFont,FontSize=Enum.FontSize.Size18,Text='Changes apply on next game entry/rejoin.',TextWrapped=true,Parent=frame}
 local hint=Utility.Create'TextButton'{Name='OpenAvatarEditor',Text='Avatar Editor',TextXAlignment=Enum.TextXAlignment.Left,Position=UDim2.new(.05,45,.87,0),Size=UDim2.new(.3,-45,.08,0),BackgroundTransparency=1,TextColor3=Style.WhiteTextColor,Font=Style.RegularFont,FontSize=Style.SubHeaderSize,Parent=frame}
 local yIcon=Utility.Create'ImageLabel'{Image='rbxasset://textures/ui/Shell/ButtonIcons/YButton.png',Position=UDim2.new(.05,0,.87,0),Size=UDim2.new(0,38,0,38),BackgroundTransparency=1,Parent=frame}
 local category='collection';local page=1;local selected;local buttons={};local selectedOutfit;local render
 local function request(op,body)return http:JSONDecode(bridge:Profile(op,body and http:JSONEncode(body)or''))end
 local function preview()
  bridge:Preview(http:JSONEncode(draft.appearance))
  local model=workspace:FindFirstChild('LocalAvatarPreview')
  if model and model:FindFirstChild('Humanoid')then model.Humanoid.DisplayDistanceType=Enum.HumanoidDisplayDistanceType.None end
  camera=workspace.CurrentCamera
  if not oldCamera then oldCamera=camera.CoordinateFrame;oldType=camera.CameraType end
  local manager=require(modules.CameraManager);manager:DisableCameraControl()
  game:GetService('RunService'):BindToRenderStep('LocalAvatarPreviewCamera',Enum.RenderPriority.Camera.Value+1,function()
   camera.CameraType=Enum.CameraType.Scriptable
   camera.CoordinateFrame=CFrame.new(Vector3.new(19995.5,10001,19992),Vector3.new(19995.5,10000,20000))
  end)
  local platform=game:GetService('PlatformService')
  if not effects then effects={BlurIntensity=platform.BlurIntensity,Brightness=platform.Brightness,Contrast=platform.Contrast,GrayscaleLevel=platform.GrayscaleLevel,TintColor=platform.TintColor}end
  platform.BlurIntensity=0;platform.Brightness=0;platform.Contrast=0;platform.GrayscaleLevel=0;platform.TintColor=Color3.new(1,1,1)
  local home=root:FindFirstChild('AppHomeContainer')
  if home then
   if not backgroundState then backgroundState={home.BackgroundTransparency,home.Background.BackgroundTransparency,home.Background.ImageTransparency,home.Background.CrossfadeBackground.ImageTransparency}end
   home.BackgroundTransparency=1;home.Background.BackgroundTransparency=1;home.Background.ImageTransparency=1;home.Background.CrossfadeBackground.ImageTransparency=1
  end
 end
 local function clearPreview()
  pcall(function()bridge:Preview('')end)
  game:GetService('RunService'):UnbindFromRenderStep('LocalAvatarPreviewCamera')
  if oldCamera and camera then camera.CoordinateFrame=oldCamera;camera.CameraType=oldType end
  oldCamera=nil
  local home=root:FindFirstChild('AppHomeContainer')
  if home and backgroundState then home.BackgroundTransparency=backgroundState[1];home.Background.BackgroundTransparency=backgroundState[2];home.Background.ImageTransparency=backgroundState[3];home.Background.CrossfadeBackground.ImageTransparency=backgroundState[4]end
  backgroundState=nil
  if effects then local platform=game:GetService('PlatformService');for name,value in pairs(effects)do platform[name]=value end;effects=nil end
  if Utility.IsFastFlagEnabled('Durango3DBackground') then require(modules.CameraManager):EnableCameraControl()end
 end
 local function refreshAccountView(profile)
  require(modules.OfflineAccountProfile).Profile=copy(profile)
  require(modules.UserData):Initialize()
  local badge=root:FindFirstChild('LocalAccountBadge')
  if badge then badge.Text='  Local account: '..profile.username..' | ID: '..profile.user_id..'\n  Avatar name: '..profile.avatar_name end
  local home=root:FindFirstChild('HomePane',true)
  if home then local name=home:FindFirstChild('NameLabel',true);if name then name.Text=string.upper(profile.display_name)end end
 end

 local function action(fn)
  if busy then return end;busy=true;local ok,err=pcall(fn);busy=false
  if not ok then status.Text=tostring(err);warn('[AvatarEditor] '..tostring(err))end
 end
 local function reload()saved=request('get');draft=copy(saved);catalog=request('catalog');outfits=request('outfits').outfits;nameLabel.Text=saved.display_name end
 local function refresh()outfits=request('outfits').outfits;render()end
 local function small(parent,name,text,x,y,w,h,fn)
  local b=Utility.Create'TextButton'{Name=name,Text=text,Position=UDim2.new(x,0,y,0),Size=UDim2.new(w,-8,h,-8),BackgroundColor3=Style.BadgeOwnedColor,BorderSizePixel=0,AutoButtonColor=false,TextColor3=Style.WhiteTextColor,Font=Style.RegularFont,FontSize=Style.SmallTitleSize,TextWrapped=true,Parent=parent}
  b.SelectionGained:connect(function()b.BackgroundColor3=Style.BlueButtonColor end)
  b.SelectionLost:connect(function()b.BackgroundColor3=Style.BadgeOwnedColor end)
  b.MouseButton1Click:connect(function()action(fn)end);table.insert(buttons,b);return b
 end
 local function prompt(name,initial,fn)local value=bridge:Prompt(name,initial);if value~=''then fn(value)end end
 local function tile(name,text,index,color,fn,equipped)
  local tile=BaseTile();tile:SetActive(true);tile:SetPopupText('')
  local b=tile:GetGuiObject();b.Name=name;b.Parent=content;b.Position=UDim2.new((index-1)%3/3,0,math.floor((index-1)/3)*.5,0);b.Size=UDim2.new(1/3,-14,.5,-14)
  tile.AvatarImage.Image='rbxasset://textures/ui/Shell/Icons/CustomizeIcon@1080.png'
  tile.AvatarImage.Size=UDim2.new(.36,0,.36,0);tile.AvatarImage.Position=UDim2.new(.32,0,.13,0)
  if color then b.BackgroundColor3=color end
  if category=='collection'or category=='outfits'then tile.AvatarImage.ImageTransparency=.65 end
  Utility.Create'TextLabel'{Name='DisplayName',Text=text,Position=UDim2.new(.05,0,.56,0),Size=UDim2.new(.9,0,.4,0),BackgroundTransparency=1,Font=Style.RegularFont,FontSize=Style.SmallTitleSize,TextColor3=Style.WhiteTextColor,TextWrapped=true,ZIndex=3,Parent=b}
  tile.EquippedCheckmark.Visible=equipped or false
  b.SelectionGained:connect(function()tile:Focus();selected=b end);b.SelectionLost:connect(function()tile:RemoveFocus()end)
  b.MouseButton1Click:connect(function()action(fn)end);table.insert(buttons,b);return b
 end
 local function equip(o,default)
  saved=request('equip:'..o.outfit_id,{profile_version=saved.profile_version,default_on_login=default or false});draft=copy(saved);refreshAccountView(saved);preview();status.Text='Equipped '..o.name..'. Applies on rejoin.';render()
 end
 render=function()
  local preferred=gui.SelectedCoreObject and gui.SelectedCoreObject.Name
  tabs:ClearAllChildren();content:ClearAllChildren();actions:ClearAllChildren();footer:ClearAllChildren();buttons={}
  title.Text=category=='collection' and 'My Collection' or 'AVATAR EDITOR'
  tabs.Visible=category~='collection'
  content.Position=category=='collection' and UDim2.new(0,0,.12,0)or UDim2.new(0,0,.28,0)
  content.Size=category=='collection' and UDim2.new(1,0,.7,0)or UDim2.new(1,0,.53,0)
  hint.Visible=category=='collection';yIcon.Visible=hint.Visible
  if category~='collection'then
   for i,label in pairs({'Colors','Items','Profile','Outfits'})do local cat=string.lower(label);small(tabs,'Category_'..cat,label,(i-1)/4,0,.25,1,function()category=cat;page=1;render()end)end
  end
  if category=='colors'then
   for i,part in pairs({'head','torso','left_arm','right_arm','left_leg','right_leg'})do local key=part;tile('Color_'..key,string.gsub(key,'_',' '),i,BrickColor.new(draft.appearance.body_colors[key]).Color,function()
    local index=1;for j,v in pairs(catalog.body_colors)do if v==draft.appearance.body_colors[key]then index=j+1 end end
    draft.appearance.body_colors[key]=catalog.body_colors[(index-1)%#catalog.body_colors+1];preview();render()
   end)end
  elseif category=='items'then
   for i,item in pairs(catalog.items)do local asset=item;local equipped=false
    if asset.kind=='accessory'then for _,id in pairs(draft.appearance.accessories)do if id==asset.asset_id then equipped=true end end
    elseif asset.kind=='body_package'then equipped=draft.appearance.body_package==asset.asset_id
    else equipped=draft.appearance.clothing[asset.kind]==asset.asset_id end
    tile('Asset_'..asset.asset_id,asset.name,i,nil,function()
     if asset.kind=='accessory'then local ids={};for _,id in pairs(draft.appearance.accessories)do if id~=asset.asset_id then table.insert(ids,id)end end;if not equipped then table.insert(ids,asset.asset_id)end;draft.appearance.accessories=ids
     elseif asset.kind=='body_package'then draft.appearance.body_package=not equipped and asset.asset_id or nil
     else draft.appearance.clothing[asset.kind]=not equipped and asset.asset_id or nil end
     preview();render()
    end,equipped)
   end
  elseif category=='profile'then
   tile('DisplayName','Display name\n'..draft.display_name,1,nil,function()prompt('Display name',draft.display_name,function(v)draft.display_name=v;render()end)end)
   tile('AvatarName','Avatar name\n'..draft.avatar_name,2,nil,function()prompt('Avatar name',draft.avatar_name,function(v)draft.avatar_name=v;render()end)end)
  else
   local entries={};if category=='collection'then table.insert(entries,{name='Current Avatar',appearance=saved.appearance})end
   for _,o in pairs(outfits)do table.insert(entries,o)end
   for i=(page-1)*6+1,math.min(page*6,#entries)do local o=entries[i];tile('OutfitCard_'..tostring(o.outfit_id or 'current'),o.name,(i-1)%6+1,nil,function()
    selectedOutfit=o
    status.Text=o.name..(saved.default_outfit_id==o.outfit_id and ' — Login default' or '')
    if category=='collection'then if o.outfit_id then equip(o)else draft=copy(saved);preview()end
    else render()end
   end,o.outfit_id and saved.equipped_outfit_id==o.outfit_id)
   end
   if #entries>6 then small(actions,'PreviousPage','Previous',0,0,.25,.5,function()page=math.max(1,page-1);render()end);small(actions,'NextPage','Next',.25,0,.25,.5,function()page=math.min(math.ceil(#entries/6),page+1);render()end)end
   if category=='outfits'then
    small(actions,'SaveOutfit','Save new outfit',0,.5,.25,.5,function()prompt('Outfit name','My outfit',function(v)request('outfit-save',{name=v,appearance=draft.appearance});refresh()end)end)
    if selectedOutfit and selectedOutfit.outfit_id then local o=selectedOutfit
     small(actions,'Equip_'..o.outfit_id,'Equip',.25,.5,.25,.5,function()equip(o)end)
     small(actions,'Default_'..o.outfit_id,'Login default',.5,.5,.25,.5,function()equip(o,true)end)
     small(actions,'Rename_'..o.outfit_id,'Rename',.75,.5,.25,.5,function()prompt('Rename outfit',o.name,function(v)request('rename:'..o.outfit_id,{name=v});selectedOutfit=nil;refresh()end)end)
     small(actions,'Delete_'..o.outfit_id,'Delete '..o.name,.5,0,.5,.5,function()request('delete:'..o.outfit_id);saved=request('get');selectedOutfit=nil;refresh()end)
    end
   end
  end
  if category~='collection'then
   small(footer,'ResetAvatar','Reset',0,0,.25,1,function()draft.appearance=copy(catalog.default_appearance);preview();render()end)
   small(footer,'SaveAvatar','Save & apply',.25,0,.25,1,function()saved=request('save',{profile_version=saved.profile_version,display_name=draft.display_name,avatar_name=draft.avatar_name,appearance=draft.appearance});draft=copy(saved);nameLabel.Text=saved.display_name;refreshAccountView(saved);preview();render();status.Text='Saved. Applies on next game entry/rejoin.'end)
   small(footer,'CancelAvatar','Cancel edits',.5,0,.25,1,function()draft=copy(saved);preview();render();status.Text='Unsaved edits discarded.'end)
   small(footer,'BackToCollection','Collection',.75,0,.25,1,function()draft=copy(saved);category='collection';page=1;preview();render()end)
  end
  for i,b in pairs(buttons)do b.NextSelectionLeft=buttons[i-1]or b;b.NextSelectionRight=buttons[i+1]or b;b.NextSelectionUp=buttons[i-3]or b;b.NextSelectionDown=buttons[i+3]or buttons[#buttons]or b end
  if focused then
   local nextFocus=buttons[1]or hint
   for _,b in pairs(buttons)do if b.Name==preferred then nextFocus=b;break end end
   gui.SelectedCoreObject=nextFocus
  end
 end
 local function openEditor()category='colors';page=1;render()end
 hint.MouseButton1Click:connect(function()action(openEditor)end)
 function this:GetName()return'Avatar'end
 function this:IsFocused()return focused end
 function this:GetDefaultSelectableObject()return buttons[1]or hint end
 function this:OnSelectAction()end
 function this:OpenEquippedPackage()end
 function this:Show()frame.Visible=true;action(function()reload();preview();render()end)
  game:GetService('ContextActionService'):BindCoreAction('LocalAvatarEditorY',function(_,state)if focused and state==Enum.UserInputState.Begin then action(openEditor)end end,false,Enum.KeyCode.ButtonY)
 end
 function this:Hide()game:GetService('ContextActionService'):UnbindCoreAction('LocalAvatarEditorY');frame.Visible=false;clearPreview()end
 function this:Focus()focused=true;gui.SelectedCoreObject=buttons[1]or hint end
 function this:RemoveFocus()focused=false end
 function this:SetPosition(p)frame.Position=p end
 function this:SetParent(p)frame.Parent=p end
 return this
end
