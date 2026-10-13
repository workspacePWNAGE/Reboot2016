-- Compatibility header for archived AvatarStarterScriptV1, not a replacement UI.
return function(parent)
local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local shellModules=root:WaitForChild('Modules')
local bridge=shellModules:WaitForChild('LocalLibraryBridge')
local http=game:GetService('HttpService')
local gui=game:GetService('GuiService')
local actions=game:GetService('ContextActionService')
local function copy(v)return http:JSONDecode(http:JSONEncode(v))end
local function request(op,body)return http:JSONDecode(bridge:Profile(op,body and http:JSONEncode(body)or''))end
local saved=request('get');local draft=copy(saved)
local catalog=request('catalog');local outfits=request('outfits').outfits
-- Catalog can grant newly installed bundled items to an existing account.
saved=request('get');draft=copy(saved)
local screenGui=bridge:RecoveredEditor()
-- Three archived template references use an absent atlas. Reuse known original
-- shell components; do not claim a different sprite sheet has the same layout.
local missingAtlas='rbxasset://textures/AvatarEditorIcons/Sheet1.png'
local function repairAtlas(parent)
 for _,child in pairs(parent:GetChildren())do
  if (child:IsA('ImageLabel')or child:IsA('ImageButton'))and child.Image==missingAtlas then
   child.ImageRectOffset=Vector2.new(0,0);child.ImageRectSize=Vector2.new(0,0)
   if child.Name=='SelectionFrameTemplate'then child.Image='rbxasset://textures/ui/Shell/Icons/EquippedOverlay.png'
   elseif child.Name=='ExitFullViewButton'then child.Image='rbxasset://textures/ui/Shell/ButtonIcons/BButton.png'
   else child.Image='';child.BackgroundColor3=Color3.new(1,1,1);child.BackgroundTransparency=0 end
   warn('[RecoveredEditor] Missing original Sheet1 atlas: '..child.Name..'; original shell fallback applied')
  end
  repairAtlas(child)
 end
end
repairAtlas(screenGui)
local modulesParent=shellModules:WaitForChild('RecoveredEditorModules')
local isCoreSetup=true
local alive=true;local focused=false;local connections={};local activations={}
local choose;local colorPart='HeadColor'
local function connect(signal,fn)
 local conn=signal:connect(function(...)if alive then fn(...)end end)
 table.insert(connections,conn);return conn
end
local function bindClick(button,fn)
 activations[button]=fn
 connect(button.SelectionGained,function()
  if button.Parent and button.Parent:IsA('ScrollingFrame')then
   local list=button.Parent;local y=button.Position.Y.Offset
   if y<list.CanvasPosition.Y then list.CanvasPosition=Vector2.new(0,y)
   elseif y+button.AbsoluteSize.Y>list.CanvasPosition.Y+list.AbsoluteSize.Y then list.CanvasPosition=Vector2.new(0,y+button.AbsoluteSize.Y-list.AbsoluteSize.Y)end
  end
 end)
 return connect(button.MouseButton1Click,fn)
end
local oldCamera=workspace.CurrentCamera.CoordinateFrame
local oldType=workspace.CurrentCamera.CameraType
local oldFov=workspace.CurrentCamera.FieldOfView
local oldFocus=workspace.CurrentCamera.Focus
local oldSubject=workspace.CurrentCamera.CameraSubject
local manager=require(shellModules.CameraManager);manager:DisableCameraControl()
local home=root:FindFirstChild('AppHomeContainer')
local homeState
if home then homeState={home.BackgroundTransparency,home.Background.BackgroundTransparency,home.Background.ImageTransparency,home.Background.CrossfadeBackground.ImageTransparency};home.BackgroundTransparency=1;home.Background.BackgroundTransparency=1;home.Background.ImageTransparency=1;home.Background.CrossfadeBackground.ImageTransparency=1 end
local platform=game:GetService('PlatformService');local effects={}
for _,name in pairs({'BlurIntensity','Brightness','Contrast','GrayscaleLevel','TintColor'})do effects[name]=platform[name]end
platform.BlurIntensity=0;platform.Brightness=0;platform.Contrast=0;platform.GrayscaleLevel=0;platform.TintColor=Color3.new(1,1,1)
local environment=bridge:RecoveredEnvironment()
local environmentOffset=Vector3.new(20000-15.276206,10000-3.71000099,20000+16.8211784)
local function moveScene(node)
 if node:IsA('BasePart') then node.Anchored=true;node.CanCollide=false;node.CFrame=node.CFrame+environmentOffset end
 for _,child in pairs(node:GetChildren())do moveScene(child)end
end
moveScene(environment)
local lighting=game:GetService('Lighting');local oldLighting={};local oldSky={}
for key,value in pairs(http:JSONDecode(environment.OriginalLighting.Value))do
 if type(value)=='table'then value=Color3.new(value[1],value[2],value[3])end
 local ok,old=pcall(function()return lighting[key]end)
 if ok then oldLighting[key]=old;local applied,reason=pcall(function()lighting[key]=value end);if not applied then warn('[RecoveredEnvironment] Lighting '..key..': '..tostring(reason))end end
end
for _,child in pairs(lighting:GetChildren())do if child:IsA('Sky')then table.insert(oldSky,child);child.Parent=nil end end
environment.Parent=workspace
workspace.CurrentCamera.FieldOfView=70
print('[RecoveredEnvironment] Original lighting, particles and room mounted')
bridge:Preview(http:JSONEncode(draft.appearance))
screenGui.Parent=root.Parent
local mainFrame = screenGui:WaitForChild('Frame')
local userInputService = game:GetService('UserInputService')
local httpService = game:GetService('HttpService')
local runService = game:GetService('RunService')
local marketplaceService = game:GetService('MarketplaceService')
local insertService = game:GetService('InsertService')
local guiService = game:GetService('GuiService')
local sharedStorage = game:GetService('ReplicatedStorage')
local camera = game.Workspace.CurrentCamera
local tabList = mainFrame:WaitForChild('TabList')
local scrollingFrame = mainFrame:WaitForChild('ScrollingFrame')
local templateCharacterR6 = workspace:FindFirstChild('LocalAvatarPreview')
local templateCharacterR15 = templateCharacterR6 -- R6 is the supported offline rig
local cameraController = require(modulesParent.CameraController)
local tweenPropertyController = require(modulesParent.TweenPropertyController)
local tweenProperty = tweenPropertyController.tweenProperty
local easeFilters = require(modulesParent.EaseFilters)
local exitFullViewButton = mainFrame.ExitFullViewButton
local shadeLayer = screenGui.ShadeLayer
local menuFrame = screenGui.MenuFrame
local menuTitleLabel = menuFrame:WaitForChild('TitleLabel')
local menuCancelButton = menuFrame:WaitForChild('CancelButton')
local particleScreen = environment:WaitForChild('ParticleScreenPart'):WaitForChild('ParticleEmitter')
local particleScreen2 = environment:WaitForChild('ParticleScreenPart'):WaitForChild('ParticleEmitter2')
local avatarTypeSwitchFrame = screenGui:WaitForChild('AvatarTypeSwitch')
local avatarTypeButton = avatarTypeSwitchFrame:WaitForChild('ButtonSoak')
local r6Label = avatarTypeSwitchFrame:WaitForChild('R6Label')
local r15Label = avatarTypeSwitchFrame:WaitForChild('R15Label')
local avatarTypeSwitch = avatarTypeSwitchFrame:WaitForChild('Switch')
local accessoriesColumn = screenGui:WaitForChild('AccessoriesColumn')
local selectionFrameTemplate = screenGui:WaitForChild('SelectionFrameTemplate')
local whiteFrameTemplate = screenGui:WaitForChild('WhiteFrameTemplate')
local fakeScrollBar = mainFrame:WaitForChild('FakeScrollBar')
local detailsMenuFrame = screenGui:WaitForChild('DetailsFrame')
local detailsNameLabel = detailsMenuFrame:WaitForChild('NameLabel')
local detailsScrollingFrame = detailsMenuFrame:WaitForChild('ScrollingDescription')
local detailsDescriptionLabel = detailsScrollingFrame:WaitForChild('TextLabel')
local detailsCreatorLabel = detailsMenuFrame:WaitForChild('CreatorLabel')
local detailsImageLabel = detailsMenuFrame:WaitForChild('ImageLabel')
local detailsCloseButton = detailsMenuFrame:WaitForChild('CloseButton')
local detailsCloseImageLabel = detailsCloseButton:WaitForChild('ImageLabel')

local tabWidth = 60
local tabHeight = 50
local buttonsPerRow = 4
local gridPadding = 6
local itemsPerPage = 24		--number of items per http request for infinite scrolling
local characterRotationSpeed = .0065
local rotationalInertia = .9
local numberOfAllowedHats = 3
local fakeScrollBarWidth = 5
local maxNumberOfRecentAssets = 30
local tapDistanceThreshold = 10	--Defines the maximum allowable distance between input began and ended for input to be considered a tap
local doubleTapThreshold = .25

local domainUrl = "roblox.com"	--"sitetest3.robloxlabs.com"
local urlPrefix = "https://inventory."..domainUrl
local avatarUrlPrefix = "https://avatar."..domainUrl
local assetImageUrl = "https://www."..domainUrl.."/Thumbs/Asset.ashx?width=110&height=110&assetId="
local assetImageUrl150 = "https://www."..domainUrl.."/Thumbs/Asset.ashx?width=150&height=150&assetId="
local characterFetch = "https://api."..domainUrl.."/v1.1/avatar-fetch/?placeId=12&userId="
local pages = require(modulesParent.PagesInfo)
local assetTypeNames = require(modulesParent.AssetTypeNames)

--[[
{	"bodyColorsUrl":"http://assetgame.roblox.com/Asset/BodyColors.ashx?avatarHash=ce7c94f78b547c26f171bdc7f69f4e7a",
	"accessoryVersionIds":["76925267"],
	"equippedGearVersionIds":["27531684"],
	"backpackGearVersionIds":["27531684","468377669","116509933"],
	"resolvedAvatarType":"R6",
	"bodyColors":{"HeadColor":1027,"LeftArmColor":5,"LeftLegColor":1001,"RightArmColor":1001,"RightLegColor":1025,"TorsoColor":9}
}
]]

local character = templateCharacterR6
local hrp = character:FindFirstChild('HumanoidRootPart') or character:WaitForChild('Torso')
local humanoid = character:WaitForChild('Humanoid')


local defaultCamera = CFrame.new(10.2426682, 5.1197648, -30.9536419, -0.946675897, 0.123298854, -0.297661126, -7.4505806e-009, 0.92387563, 0.382692933, 0.322187454, 0.36228618, -0.874610782)
local fullViewCameraCF = CFrame.new(13.2618074, 4.74155569, -22.701086, -0.94241035, 0.0557777137, -0.329775006, -3.7252903e-009, 0.98599577, 0.166770056, 0.334458828, 0.157165825, -0.92921263)	--13.3677721, 4.88436604, -23.2210388, -0.951382637, 0.0386046842, -0.305582821, -3.7252903e-009, 0.992114604, 0.125335142, 0.308011681, 0.119241677, -0.943880498)	--12.7946997, 4.23308134, -24.8994789, -0.957343042, 3.03125148e-006, -0.288954049, -0, 1.00000012, 1.04904275e-005, 0.288954049, 1.00429379e-005, -0.957343042)
local page = nil


local currentlyWearing = {}
local savedWearingAssets = {}
local currentHats = {}
local avatarType = 'R6'
local savedAvatarType = avatarType
local bodyColors = {
	["HeadColor"] = 194,
	["LeftArmColor"] = 194,
	["LeftLegColor"] = 194,
	["RightArmColor"] = 194,
	["RightLegColor"] = 194,
	["TorsoColor"] = 194,
}
local savedBodyColors = {}
local recentAssetList = {}		--list of recently used/equipped assets
local assetsLinkedContent = {}

local skinColorList = {}
local skinColorListNames = {
'Dark taupe','Brown','Linen','Nougat','Light orange',
'Dirt brown','Reddish brown','Cork','Burlap','Brick yellow',
'Sand red','Dusty Rose','Medium red','Pastel orange','Carnation pink',
'Sand blue','Steel blue','Pastel Blue','Pastel violet','Lilac',
'Bright bluish green','Shamrock','Moss','Medium green','Br. yellowish orange',
'Bright yellow','Daisy orange','Dark stone grey','Mid grey','Institutional white',
}
for i,v in pairs(skinColorListNames) do
	skinColorList[i] = BrickColor.new(v)
end
skinColorListNames = nil


tabList.CanvasSize = UDim2.new(0, #pages * (tabWidth+1), 0, 0)
local userId = saved.user_id 
---temporary code---	--todo:remove this code
if userId <= 10 then
	-- Authenticated ID is never replaced
end
--------------------


-- Data/service hooks for the recovered editor. All writes use native validated sessions.
local assets={};local owned={};local outfitMap={}
local typeIds={shirt=11,pants=12,tshirt=2,accessory=8,body_package=27}
local typeNames={shirt='Shirt',pants='Pants',tshirt='TShirt',accessory='Hat',body_package='Torso'}
for _,v in pairs(saved.inventory or{})do owned[v.asset_id]=true end
for _,asset in pairs(catalog.items)do if owned[asset.asset_id]then assets[asset.asset_id]=asset end end
for _,outfit in pairs(outfits)do outfitMap[outfit.outfit_id]=outfit end
local colorMap={HeadColor='head',TorsoColor='torso',LeftArmColor='left_arm',RightArmColor='right_arm',LeftLegColor='left_leg',RightLegColor='right_leg'}
-- The original starter calls createR6Rig before its HTTP profile fetch. Seed
-- its state first so preview initialization cannot overwrite saved body colors.
for name,key in pairs(colorMap)do bodyColors[name]=saved.appearance.body_colors[key]end
skinColorList={};for _,id in pairs(catalog.body_colors)do table.insert(skinColorList,BrickColor.new(id))end
local function appearanceIds(appearance)
 local ids={}
 if appearance.body_package then table.insert(ids,appearance.body_package)end
 for _,id in pairs(appearance.clothing)do table.insert(ids,id)end
 for _,id in pairs(appearance.accessories)do table.insert(ids,id)end
 for _,id in pairs(appearance.equipped_assets)do table.insert(ids,id)end
 return ids
end
local function syncDraft()
 for name,key in pairs(colorMap)do draft.appearance.body_colors[key]=bodyColors[name]end
end
local previewOrigin=CFrame.new(20000,10000,20000,0.766051888,0,0.642794013,0,1,0,-0.642794013,0,0.766051888)
local function refreshPreview()
 syncDraft()
 bridge:Preview(http:JSONEncode(draft.appearance))
 character=workspace:FindFirstChild('LocalAvatarPreview')
 hrp=character:FindFirstChild('HumanoidRootPart')or character:FindFirstChild('Torso')
 humanoid=character:FindFirstChild('Humanoid')
 character:SetPrimaryPartCFrame(previewOrigin)
 humanoid.DisplayDistanceType=Enum.HumanoidDisplayDistanceType.None
 print('[RecoveredEditor] R6 preview applied authenticated draft')
end
local function assetInfo(id)
 local asset=assets[tonumber(id)]
 if not asset then return nil end
 return{Name=asset.name,AssetId=asset.asset_id,AssetTypeId=typeIds[asset.kind],Creator={Name='Bundled local asset'},Description='Available local avatar component'}
end
local function thumbnail(id)
 local n=tonumber(tostring(id):match('(%d+)$'))
 if n==900100001 then return 'rbxasset://offline-avatar/demo-shirt.png'end
 -- No account/outfit thumbnail renderer exists. Original placeholder, never a fabricated avatar PNG.
 return 'rbxasset://textures/ui/Shell/Icons/DefaultProfileIcon.png'
end
local function itemName(id)
 local asset=assets[tonumber(id)];if asset then return asset.name end
 local outfit=outfitMap[tostring(id):match('^Outfit(.+)$')];return outfit and outfit.name
end
local function offlineGet(url)
 if string.find(url,'avatar-fetch',1,true)then
  local colors={};for name,key in pairs(colorMap)do colors[name]=draft.appearance.body_colors[key]end
  return http:JSONEncode({bodyColors=colors,resolvedAvatarType='R6'})
 elseif string.find(url,'currently-wearing',1,true)then return http:JSONEncode({assetIds=appearanceIds(draft.appearance)})
 elseif string.find(url,'/inventory/',1,true)then
  local kind=string.match(url,'/inventory/([^?]+)');local ids={}
  if tonumber(string.match(url,'pageNumber=(%d+)')or'1')==1 then for id,asset in pairs(assets)do if typeNames[asset.kind]==kind then table.insert(ids,id)end end end
  table.sort(ids);return http:JSONEncode({data=ids})
 elseif string.find(url,'/outfits?',1,true)then
  local result={};if tonumber(string.match(url,'page=(%d+)')or'1')==1 then for _,o in pairs(outfits)do table.insert(result,{id=o.outfit_id,name=o.name})end end
  return http:JSONEncode({data=result})
 elseif string.find(url,'/details',1,true)then
  local id=string.match(url,'/outfits/([^/]+)/details');local o=assert(outfitMap[id],'Outfit unavailable')
  local colors={};local ids={};for name,key in pairs(colorMap)do colors[key:gsub('_([a-z])',string.upper)..'ColorId']=o.appearance.body_colors[key]end
  for _,asset in pairs(appearanceIds(o.appearance))do table.insert(ids,{id=asset})end
  return http:JSONEncode({assets=ids,bodyColors=colors})
 end
 error('Unsupported historical avatar request: '..tostring(url))
end
local function changeAsset(id,equip)
 id=tonumber(id);local asset=assets[id]
 if not asset then warn('[RecoveredEditor] Asset '..tostring(id)..' unavailable; other components retained');return end
 local appearance=draft.appearance
 if asset.kind=='accessory'then
  local ids={};for _,v in pairs(appearance.accessories)do if v~=id then table.insert(ids,v)end end
  if equip then table.insert(ids,id)end;appearance.accessories=ids
 elseif asset.kind=='body_package'then appearance.body_package=equip and id or nil
 else appearance.clothing[asset.kind]=equip and id or nil end
 currentlyWearing=appearanceIds(appearance);refreshPreview()
 for _,button in pairs(scrollingFrame:GetChildren())do
  local assetId=tonumber(button.Name:match('^AssetButton(%d+)$'))
  if assetId then
   local old=button:FindFirstChild('SelectionFrame');if old then old:Destroy()end
   for _,worn in pairs(currentlyWearing)do if worn==assetId then local mark=selectionFrameTemplate:Clone();mark.Name='SelectionFrame';mark.Visible=true;mark.ZIndex=button.ZIndex+2;mark.Parent=button;break end end
  end
 end
 print('[RecoveredEditor] Draft '..(equip and 'equipped 'or'unequipped ')..asset.name)
end
local function publishProfile()
 require(shellModules.OfflineAccountProfile).Profile=copy(saved)
 require(shellModules.UserData):Initialize()
end
local function saveProfile()
 syncDraft()
 saved=request('save',{profile_version=saved.profile_version,appearance=draft.appearance})
 draft=copy(saved)
 publishProfile()
 print('[RecoveredEditor] Authenticated appearance saved version '..saved.profile_version)
 return saved
end
local function loadOutfit(id)
 local outfit=assert(outfitMap[id],'Saved outfit no longer available')
 draft.appearance=copy(outfit.appearance)
 for name,key in pairs(colorMap)do bodyColors[name]=draft.appearance.body_colors[key]end
 currentlyWearing=appearanceIds(draft.appearance);refreshPreview()
 print('[RecoveredEditor] Draft outfit '..outfit.name)
end

function fastSpawn(func)
	coroutine.resume(coroutine.create(func))
end

function swait(a)
	if a and a>.0333 then
		wait(a)
	else
		runService.RenderStepped:wait()
	end
end

local function setErrorMessage(text)
	screenGui.ErrorLabel.Visible = true
	screenGui.ErrorLabel.Text = text
end

function copyTable(originalTable)
	local copy = {}
	for index,value in pairs(originalTable) do
		copy[index] = value
	end
	return copy
end

function addTables(table1,table2)
	for i,v in ipairs(table2) do
		table.insert(table1,#table1+1,v)
	end
end

function findFirstChildOfType(parent, typeName)
	if parent then
		for _,child in pairs(parent:GetChildren()) do
			if child:IsA(typeName) then
				return child
			end
		end
	end
end


local cachedAssetInfo = {}
function getAssetInfo(assetId)
	if true then return assetInfo(assetId) end
	if assetId then
		local assetInfo = cachedAssetInfo['id'..assetId]
		if assetInfo then
			return assetInfo
		end

		local success, assetData = pcall(function()
			return marketplaceService:GetProductInfo(assetId)
		end)
		if success and assetData then
			assetInfo = assetData
			cachedAssetInfo['id'..assetId] = assetInfo
			return assetInfo
		end
	end
	warn('failed to get asset info', assetId)
end


--todo: remove error debug guis from get and post
function httpGet(...)
	if true then return offlineGet(...) end
	if not isCoreSetup then
		--This is code to test in studio
		wait(math.random()*.5)
		local fakeStuff2 = {92142841,243778818,125013849,91676048,30331986,20010032,15967743,31117267,23932048,6340227,6340101,442477167,424141444,431412239}
		local fakeStuff = {assetIds = fakeStuff2}
		return httpService:JSONEncode(fakeStuff)

	else
		--This is the important code
		local tuple = {...}
		local url = tuple[1]
		local v = {pcall(function()
			return game:HttpGetAsync(url)
		end)}
		if not v[1] then
			setErrorMessage("Get "..url)
			return "[]"
		end
		return select(2, unpack(v))

	end
end

function httpPost(...)
	if true then return error('Historical avatar write disabled; use authenticated Save') end
	if not isCoreSetup then
		--This is code to test in studio
		wait(math.random()*.5)
		return '{"invalidAssetIds":[0],"success":true}'

	else
		--This is the important code
		local tuple = {...}
		local url = tuple[1]
		local v = {pcall(function()
			return game:HttpPostAsync(unpack(tuple))
		end)}
		if not v[1] then
			setErrorMessage("Set "..url)
			print(unpack(tuple))
			print(v[1],v[2])
			--return "[]"
			return false
		end
		--return select(2, unpack(v))

		if v[2] then
			local response = httpService:JSONDecode(v[2])
			if response then
				return response.success
			end
		end
				
		return false
	end
end


local bodyColorNameMap = {
	["HeadColor"] = 'headColorId',
	["LeftArmColor"] = 'leftArmColorId',
	["LeftLegColor"] = 'leftLegColorId',
	["RightArmColor"] = 'rightArmColorId',
	["RightLegColor"] = 'rightLegColorId',
	["TorsoColor"] = 'torsoColorId',
}
savedBodyColors = copyTable(bodyColors)

function characterSave(doWait) return saveProfile() end

function recursiveDisable(parent)
	if parent then
		if parent:IsA('Script') then
			parent.Disabled = true
		end
		for _,child in pairs(parent:GetChildren()) do
			recursiveDisable(child)
		end
	end
end


function adjustCameraPropperties(desiredState)
	camera.CameraType = Enum.CameraType.Scriptable
	mainFrame.Size = UDim2.new(1,0,.5,18)
	mainFrame.Position = UDim2.new(0,0,.5,-18)
	camera.CoordinateFrame = defaultCamera
	camera.FieldOfView = 70
end
if not isCoreSetup then
	wait(1)	--I have to do this wait because something overwrites changes to the camera when I do it too soon.
end
adjustCameraPropperties()


local toolHoldAnimationTrack = nil
function holdToolPos(state)
	if toolHoldAnimationTrack then
		toolHoldAnimationTrack:Stop()
		toolHoldAnimationTrack = nil
	end
	if character and character.Parent and humanoid and humanoid:IsDescendantOf(game.Workspace) then
		if state == 'Up' then
			local animationsFolder = character:FindFirstChild('Animations')
			if animationsFolder then
				local toolHoldAnimationObject = animationsFolder:FindFirstChild('Tool')
				if toolHoldAnimationObject then
					toolHoldAnimationTrack = humanoid:LoadAnimation(toolHoldAnimationObject)
					toolHoldAnimationTrack:Play(0)
				end
			end
		end
	end
end

function findFirstMatchingAttachment(model, name)
	if model and name then
		for _, child in pairs(model:GetChildren()) do
			if child then
				if child:IsA('Attachment') then
					return child
				elseif child:IsA('Accoutrement') ~= true and child:IsA('Tool') ~= true then
					local foundAttachment = findFirstMatchingAttachment(child, name)
					if foundAttachment then
						return foundAttachment
					end
				end
			end
		end
	end
end

function buildWeld(part0, part1, c0, c1, weldName)
	local weld = Instance.new('Weld')
	weld.C0 = c0
	weld.C1 = c1
	weld.Part0 = part0
	weld.Part1 = part1
	weld.Name = weldName
	weld.Parent = part0
	return weld
end

function sortAndEquipItemToCharacter(thing, assetInsertedContentList)
	if thing then
		if thing.className == 'ShirtGraphic' then
			return
		end

		recursiveDisable(thing)

		if thing:IsA("DataModelMesh") then				-- Head mesh
			local head = character:FindFirstChild('Head')
			if head then
				local replacedAsset = findFirstChildOfType(head, "DataModelMesh")
				if replacedAsset then
					replacedAsset:Destroy()
				end
				thing.Parent = head
				table.insert(assetInsertedContentList, thing)
			end
	
		elseif thing:IsA("Decal") then					-- Face
			local head = character:FindFirstChild('Head')
			if head then
				local replacedAsset = findFirstChildOfType(head, "Decal")
				if replacedAsset then
					replacedAsset:Destroy()
				end
				thing.Parent = head
				table.insert(assetInsertedContentList, thing)
			end
	
		elseif thing:IsA("CharacterAppearance") then	-- Thing, just parent it.
			thing.Parent = character
			table.insert(assetInsertedContentList, thing)
	
		elseif thing:IsA("Accoutrement") then			-- Hat
			equipItemToCharacter(thing)
			table.insert(assetInsertedContentList, thing)
	
		elseif thing:IsA("Tool") then					-- Gear
			equipItemToCharacter(thing)
			holdToolPos('Up')
			table.insert(assetInsertedContentList, thing)
		end
	end
end

function equipItemToCharacter(item)		-- Item should be an accessory or tool.
	if item and character then
		item.Parent = character
		local handle = item:FindFirstChild('Handle')
		if handle then
			handle.CanCollide = false

			local attachment = findFirstChildOfType(handle,'Attachment')
			local matchingAttachment = nil
			local matchingAttachmentPart = nil
			if attachment then
				matchingAttachment = findFirstMatchingAttachment(character)
				if matchingAttachment then
					matchingAttachmentPart = matchingAttachment.Parent
				end
			end
			if matchingAttachmentPart then	-- This infers that both attachments were found
				buildWeld(handle, matchingAttachmentPart, attachment.CFrame, matchingAttachment.CFrame, "AccessoryWeld")

			else
				if item:IsA('Accoutrement') then
					local head = character:FindFirstChild('Head')
					if head then
						buildWeld(handle, head, item.AttachmentPoint, CFrame.new(0,.5,0), "AccessoryWeld")
					end
				elseif item:IsA('Tool') then
					local rightHand = character:FindFirstChild('RightHand')
					local rightArm = character:FindFirstChild('Right Arm')
					if rightHand then
						local gripCF = CFrame.new(0.0108650923, -0.168664441, -0.0154389441, 1, 0, -0, 0, 6.12323426e-017, 1, 0, -1, 6.12323426e-017)	--todo: magic numbers that need to be swapped out with an algorithm
						buildWeld(handle, rightHand, item.Grip, gripCF, "RightGrip")
					elseif rightArm then
						local gripCF = CFrame.new(Vector3.new(0,-1,0))*CFrame.Angles(-math.pi*.5,0,0)
						buildWeld(handle, rightArm, item.Grip, gripCF, "RightGrip")
					end
				end

			end
		end
	end
end

function findIfEquipped(assetId)
	for _,currentAssetId in pairs(currentlyWearing) do
		if currentAssetId == assetId then
			return true
		end
	end
end

local bodyColorMappedParts = {
	['Head']			 = 'HeadColor',

	['Torso']			 = 'TorsoColor',
	['UpperTorso']		 = 'TorsoColor',
	['LowerTorso']		 = 'TorsoColor',

	['Left Arm']		 = 'LeftArmColor',
	['LeftUpperArm']	 = 'LeftArmColor',
	['LeftLowerArm']	 = 'LeftArmColor',
	['LeftHand']		 = 'LeftArmColor',

	['Left Leg']		 = 'LeftLegColor',
	['LeftUpperLeg']	 = 'LeftLegColor',
	['LeftLowerLeg']	 = 'LeftLegColor',
	['LeftFoot']		 = 'LeftLegColor',

	['Right Arm']		 = 'RightArmColor',
	['RightUpperArm']	 = 'RightArmColor',
	['RightLowerArm']	 = 'RightArmColor',
	['RightHand']		 = 'RightArmColor',

	['Right Leg']		 = 'RightLegColor',
	['RightUpperLeg']	 = 'RightLegColor',
	['RightLowerLeg']	 = 'RightLegColor',
	['RightFoot']		 = 'RightLegColor',
}

function updateCharacterBodyColors()
	if character then
		for _,v in pairs(character:GetChildren()) do
			local foundLink = bodyColorMappedParts[v.Name]
			if v:IsA('BasePart') and foundLink then
				local bodyColorNumber = bodyColors[foundLink]
				if bodyColorNumber then
					v.BrickColor = BrickColor.new(bodyColorNumber)
				end
			end
		end
	end
end

local buildingCharacterLock = false
local queuedRebuild = false
function createR15Rig()
	if true then return refreshPreview() end
	if not buildingCharacterLock then
		buildingCharacterLock = true
		--get r15 character
		local newCharacter = templateCharacterR15:clone()
		newCharacter.Name = 'Character'
		hrp = newCharacter:WaitForChild('HumanoidRootPart')
		humanoid = newCharacter:WaitForChild('Humanoid')
		hrp.Anchored = true
		local oldCharacter = character

		character = newCharacter
		for assetId,stuff in pairs(assetsLinkedContent) do
			for _,thing in pairs(stuff) do
				if thing then
					thing:Destroy()
				end
			end
			assetsLinkedContent[assetId] = nil
		end
		--get all assets
		local r15BodyStuff = {}
		local otherStuff = {}
		for index,assetId in pairs(currentlyWearing) do				--todo: Load everything on seperate threads and wait for them all to complete.
			local assetModel = insertService:LoadAsset(assetId)
			local stuff = {assetModel}
			if stuff[1].className == 'Model' then
				stuff = assetModel:GetChildren()
			end
			for _,thing in pairs(stuff) do
				if string.lower(thing.Name) == 'r15' then
					for _,piece in pairs(thing:GetChildren()) do
						table.insert(r15BodyStuff, piece)
					end
				elseif thing.Name.className ~= 'Folder' then
					table.insert(otherStuff, thing)
				end
			end
		end

		--Iterate assets and replace body parts on rig
		for _,thing in pairs(r15BodyStuff) do
			if thing.className == 'MeshPart' then
				local thingToReplace = newCharacter:FindFirstChild(thing.Name)
				if thingToReplace then
					thingToReplace:Destroy()
					thing.Parent = newCharacter
				end
			else
				table.insert(otherStuff, thing)
			end
		end

		--Create breadth first rig joints
		local allRigAttachments = {}
		local function populateAttachments(thing)
			for _,child in pairs(thing:GetChildren()) do
				local nameLength = string.len(child.Name)
				if child.className == 'Attachment' and string.sub(child.Name,nameLength-12) == 'RigAttachment' then
					allRigAttachments[child] = true
				end
				populateAttachments(child)
			end
		end
		populateAttachments(newCharacter)
	
		local function recursiveRig(thing)
			for _,rigAttachment in pairs(thing:GetChildren()) do
				local nameLength = string.len(rigAttachment.Name)
				if rigAttachment.className == 'Attachment' and string.sub(rigAttachment.Name,nameLength-12) == 'RigAttachment' and allRigAttachments[rigAttachment] then
					allRigAttachments[rigAttachment] = nil
					for matchingAttachment,_ in pairs(allRigAttachments) do
						if rigAttachment.Name == matchingAttachment.Name and allRigAttachments[matchingAttachment] then
							allRigAttachments[matchingAttachment] = nil
							local matchingPart = matchingAttachment.Parent
							if matchingPart and matchingPart:IsA('BasePart') then
								local joint = Instance.new('Motor6D')
								joint.Name = string.sub(rigAttachment.Name, 1, nameLength-13)
								joint.Part0 = thing
								joint.C0 = rigAttachment.CFrame
								joint.Part1 = matchingPart
								joint.C1 = matchingAttachment.CFrame
								joint.Parent = matchingPart
								recursiveRig(matchingPart)
							end
						end
					end
				end
			end
		end
		recursiveRig(hrp)

		updateCharacterBodyColors()

		--Apply leftover assets to rig
		local toolFound = false
		for _,thing in pairs(otherStuff) do
			if thing and thing:IsA('Tool') then
				toolFound = true
			end
			sortAndEquipItemToCharacter(thing, {})
		end
		newCharacter.Parent = game.Workspace
		if oldCharacter and oldCharacter.Parent then
			oldCharacter:Destroy()
		end

		--Play tool animation if there was a tool loaded onto the character
		if toolFound then
			holdToolPos('Up')
		end

		--if more assets were attempted to be equipped while building, build again.
		buildingCharacterLock = false
		if queuedRebuild then
			queuedRebuild = false
			createR15Rig()
		end
	else
		queuedRebuild = true
	end
end

function createR6Rig()
	if true then return refreshPreview() end
	if not buildingCharacterLock then
		buildingCharacterLock = true
		local newCharacter = templateCharacterR6:clone()
		newCharacter.Name = 'Character'
		hrp = newCharacter:WaitForChild('HumanoidRootPart')
		humanoid = newCharacter:WaitForChild('Humanoid')
		hrp.Anchored = true
		character:Destroy()
		character = newCharacter

		newCharacter.Parent = game.Workspace	--Character has to be in workspace before tools are equipped so that the toolhold animation can run

		for assetId,stuff in pairs(assetsLinkedContent) do
			for _,thing in pairs(stuff) do
				if thing then
					thing:Destroy()
				end
			end
			assetsLinkedContent[assetId] = nil
		end

		--put on assets
		for _,assetId in pairs(currentlyWearing) do
			local assetModel = insertService:LoadAsset(assetId) --Get all waiting overwith early
	
			local insertedStuff = {}
			if not assetsLinkedContent[assetId] then
				assetsLinkedContent[assetId] = insertedStuff
			else
				insertedStuff = assetsLinkedContent[assetId]
			end
			local stuff = {assetModel}
			if stuff[1].className == 'Model' then
				stuff = assetModel:GetChildren()
			end
			for _,thing in pairs(stuff) do						-- Equip asset differently depending on what it is.
				if string.lower(thing.Name) == 'r6' then
					for _,r6SpecificThing in pairs(thing:GetChildren()) do
						sortAndEquipItemToCharacter(r6SpecificThing, insertedStuff)
					end
				elseif thing.className ~= 'Folder' then
					sortAndEquipItemToCharacter(thing, insertedStuff)
				end
			end
		end

		updateCharacterBodyColors()

		buildingCharacterLock = false
	end
end


function updateAccessoriesColumnVisuals()
	local animateShake = false
	for i=1, numberOfAllowedHats do
		local accessoryButton = accessoriesColumn:FindFirstChild('AccessoryButton'..tostring(i))
		if accessoryButton then
			--local startImage = accessoryButton.Image
			local assetId = currentHats[i]
			if assetId then
				if accessoryButton.Image ~= assetImageUrl..tostring(assetId) then
					local numberOfHatsScale = (i-1)/(numberOfAllowedHats-1)
					delay(numberOfHatsScale*.25,function()
						local basePosition = UDim2.new(.5,-24, numberOfHatsScale, numberOfHatsScale*-48)
						accessoryButton:TweenPosition(basePosition + UDim2.new(0,0,0,15),'Out','Quad',.05,true,function()
							accessoryButton:TweenPosition(basePosition,'Out','Back',.15,true)
						end)
						accessoryButton.Image = assetImageUrl..tostring(currentHats[i])
					end)
				end
			else
				if accessoryButton.Image ~= '' then
					accessoryButton:TweenPosition(UDim2.new(.5,-39,accessoryButton.Position.Y.Scale,accessoryButton.Position.Y.Offset),'Out','Quad',.05,true,function()
						accessoryButton:TweenPosition(UDim2.new(.5,-9,accessoryButton.Position.Y.Scale,accessoryButton.Position.Y.Offset),'InOut','Quad',.05,true,function()
							accessoryButton:TweenPosition(UDim2.new(.5,-24,accessoryButton.Position.Y.Scale,accessoryButton.Position.Y.Offset),'Out','Back',.25,true)
						end)
					end)
					accessoryButton.Image = ''
				end
			end

			local fadeIcon = accessoryButton:FindFirstChild('FadeIcon')	--This is the faded icon of an accessory
			if fadeIcon then
				fadeIcon.Visible = not assetId	--Make the faded icon visible if there is no asset in the slot
			end
		end
	end
end

function unequipAsset(assetId)
	if true then return changeAsset(assetId,false) end
	--Remove from wearing list
	for i=#currentlyWearing, 1, -1 do
		local currentAssetId = currentlyWearing[i]
		if currentAssetId == assetId then
			table.remove(currentlyWearing, i)
		end
	end

	local assetInfo = getAssetInfo(assetId)
	local assetTypeName = nil
	if assetInfo then
		assetTypeName = assetTypeNames[assetInfo.AssetTypeId]
	end

	if assetTypeName == 'Hat' then
		--Remove from hats list
		for i=#currentHats, 1, -1 do
			local currentAssetId = currentHats[i]
			if currentAssetId == assetId then
				table.remove(currentHats, i)
			end
		end
		updateAccessoriesColumnVisuals()
	end

	--Remove selectionBoxes
	local assetButtonName = 'AssetButton'..tostring(assetId)
	for _,assetButton in pairs(scrollingFrame:GetChildren()) do
		if assetButton.Name == assetButtonName then
			local selectionFrame = assetButton:FindFirstChild('SelectionFrame')
			if selectionFrame then
				selectionFrame:Destroy()
			end
		end
	end

	fastSpawn(function()
		if avatarType == 'R15' then
			createR15Rig()
		else
			--Destroy rendered content
			local currentAssetContent = assetsLinkedContent[assetId]
			if currentAssetContent then
				for _,thing in pairs(currentAssetContent) do
					if thing and thing.Parent then
						thing:Destroy()
					end
				end
				assetsLinkedContent[assetId] = nil
			end
		
			--Special cases where we need to replace removed asset with a default
			if assetTypeName == 'Head' then
				if character and character.Parent then
					local head = character:FindFirstChild('Head')
					if head then
						local defaultHeadMesh = Instance.new('SpecialMesh')
						defaultHeadMesh.MeshType = 'Head'
						defaultHeadMesh.Scale = Vector3.new(1.2,1.2,1.2)
						defaultHeadMesh.Parent = head
					end
				end
			elseif assetTypeName == 'Face' then
				if character and character.Parent then
					local head = character:FindFirstChild('Head')
					if head then
						local currentFace = findFirstChildOfType(head,'Decal')
						if not currentFace then
							local face = Instance.new('Decal')
							face.Name = 'face'
							face.Texture = "rbxasset://textures/face.png"
							face.Parent = head
						end
					end
				end
			elseif assetTypeName == 'Left Arm' or assetTypeName == 'Right Arm' then
			elseif assetTypeName == 'Hat' then
			elseif assetTypeName == 'Gear' then
				holdToolPos('Down')
			end
		end
	end)
end

function equipAsset(assetId)
	if true then return changeAsset(assetId,true) end
	local assetInfo = getAssetInfo(assetId)
	if assetInfo and not findIfEquipped(assetId) then
		local assetTypeName = assetTypeNames[assetInfo.AssetTypeId]
		--print('Equipping Asset',assetInfo.AssetTypeId, assetInfo.Name, assetTypeName)

		--unequip assets of similar type
		if assetTypeName == 'Hat' then
			while #currentHats >= numberOfAllowedHats do
				local currentAssetId = currentHats[#currentHats]
				if currentAssetId then
					unequipAsset(currentAssetId)	--This could technically yield, but shouldn't because asset info has already been cached when the item was equipped.
				end
			end
			table.insert(currentHats,1,assetId)
			updateAccessoriesColumnVisuals()
		else
			for i=#currentlyWearing, 1, -1 do
				local currentAssetId = currentlyWearing[i]
				if currentAssetId then
					local currentAssetInfo = getAssetInfo(currentAssetId)
					if currentAssetInfo and currentAssetInfo.AssetTypeId == assetInfo.AssetTypeId then
						unequipAsset(currentAssetId)
					end
				end
			end
		end

		--equip asset
		table.insert(currentlyWearing, 1, assetId)

		--create selection frames
		local assetButtonName = 'AssetButton'..tostring(assetId)
		for _,assetButton in pairs(scrollingFrame:GetChildren()) do
			if assetButton.Name == assetButtonName then
				local selectionFrame = assetButton:FindFirstChild('SelectionFrame')
				if not selectionFrame then
					local selectionFrame = selectionFrameTemplate:clone()
					selectionFrame.Name = 'SelectionFrame'
					selectionFrame.ZIndex = assetButton.ZIndex+1
					selectionFrame.Visible = true
					selectionFrame.Parent = assetButton
				end
			end
		end

		local assetModel = insertService:LoadAsset(assetId)
		--after loading model, check to make sure it is still equipped before dressing character
		if findIfEquipped(assetId) then
	
			--render changes
			if avatarType == 'R6' then
				local insertedStuff = {}
				if not assetsLinkedContent[assetId] then
					assetsLinkedContent[assetId] = insertedStuff
				else
					insertedStuff = assetsLinkedContent[assetId]
				end
				local stuff = {assetModel}
				if stuff[1].className == 'Model' then
					stuff = assetModel:GetChildren()
				end
				for _,thing in pairs(stuff) do						-- Equip asset differently depending on what it is.
					if string.lower(thing.Name) == 'r6' then
						for _,r6SpecificThing in pairs(thing:GetChildren()) do
							sortAndEquipItemToCharacter(r6SpecificThing, insertedStuff)
						end
					elseif thing.className ~= 'Folder' then
						sortAndEquipItemToCharacter(thing, insertedStuff)
					end
				end

			else
				createR15Rig()
			end
			if character and character.Parent and humanoid and humanoid:IsDescendantOf(game.Workspace) then
				local animationFolder = character:FindFirstChild('Animations')
				if animationFolder then
					local animation = animationFolder:FindFirstChild('InspectChange')
					if animation then
						--todo: Enable code with non-looping animations
						--[[print('Play inspect animation')
						local animationTrack = humanoid:LoadAnimation(animation)
						animationTrack:Play()]]
					end
				end
			end

		end

	end
end

for index=1, numberOfAllowedHats do
	local accessoryButton = accessoriesColumn:WaitForChild('AccessoryButton'..tostring(index))
	if accessoryButton then

		local takeOffFunction = function()
			local assetId = currentHats[index]
			if assetId then
				runParticleEmitter()
				unequipAsset(assetId)
				closeMenu()
				updateAccessoriesColumnVisuals()
			end
		end

		bindClick(accessoryButton,takeOffFunction)
		connect(accessoryButton.TouchLongPress,function()
			local assetId = currentHats[index]
			if assetId then
				openMenu('',
					{
						{text = 'Take Off', func = takeOffFunction},
						{text = 'View details',func = function() openDetails(assetId) end},
					},
					assetId
				)
			end
		end)
	end
end


local viewMode = false
function setViewMode(desiredViewMode)
	if desiredViewMode ~= viewMode then
		viewMode = desiredViewMode
		local tweenTime = .5
		if viewMode then

			fastSpawn(function()
				exitFullViewButton.Visible = true
				tweenProperty(exitFullViewButton, 'ImageTransparency', nil, 0, tweenTime, easeFilters.quad, easeFilters.easeInOut)
			end)
			accessoriesColumn:TweenPosition(UDim2.new(-.1,-50,.05, 0),'InOut','Quad',tweenTime,true)
			avatarTypeSwitchFrame:TweenPosition(UDim2.new(1,-88,0,-86),'InOut','Quad',tweenTime,true)
			mainFrame:TweenPosition(UDim2.new(0,0,1,0),'InOut','Quad',tweenTime,true)
			local fullViewCameraCF = fullViewCameraCF	--CFrame.new(12.7946997, 4.23308134, -24.8994789, -0.957343042, 3.03125148e-006, -0.288954049, -0, 1.00000012, 1.04904275e-005, 0.288954049, 1.00429379e-005, -0.957343042)
			cameraController.tweenCamera(nil, fullViewCameraCF, tweenTime, easeFilters.quad, easeFilters.easeInOut)
			
		else

			fastSpawn(function()
				local tweenComplete = tweenProperty(exitFullViewButton, 'ImageTransparency', nil, 1, tweenTime, easeFilters.quad, easeFilters.easeInOut)
				if tweenComplete then
					exitFullViewButton.Visible = false
				end
			end)
			accessoriesColumn:TweenPosition(UDim2.new(.05, 25,.05, 0),'InOut','Quad',tweenTime,true)
			avatarTypeSwitchFrame:TweenPosition(UDim2.new(1,-88,0,24),'InOut','Quad',tweenTime,true)
			mainFrame:TweenPosition(UDim2.new(.06,0,.14,0),'InOut','Quad',tweenTime,true)
			cameraController.tweenCamera(nil, defaultCamera, tweenTime, easeFilters.quad, easeFilters.easeInOut)

		end
	end
end


do
	local particleEmittionCount = 0
	function runParticleEmitter()
		fastSpawn(function()				--product didn't like the smoke mask
			if particleScreen then particleScreen.Enabled = true end
			if particleScreen2 then particleScreen2.Enabled = true end
			particleEmittionCount = particleEmittionCount + 1
			local thisParticleEmittionCount = particleEmittionCount
			wait(.3)
			if particleEmittionCount == thisParticleEmittionCount then
				if particleScreen then particleScreen.Enabled = false end
				if particleScreen2 then particleScreen2.Enabled = false end
			end
		end)
	end
end


function closeDetails()
	local tweenTime = .25
	detailsMenuFrame:TweenPosition(UDim2.new(0,15,-.7,-40),'InOut','Quad',tweenTime,true)
	fastSpawn(function()
		local shadeTweenCompleted = tweenProperty(shadeLayer, 'BackgroundTransparency', nil, 1, tweenTime, easeFilters.quad, easeFilters.easeInOut)
		if shadeTweenCompleted then
			shadeLayer.Visible = false
		end
	end)
	detailsCloseImageLabel.ImageTransparency = .25
end

local detailsMenuCount = 0
function openDetails(assetId)
	--[[if isCoreSetup then
		closeMenu()
		guiService:OpenBrowserWindow("https://www.roblox.com/catalog/"..tostring(assetId).."/name")
	else]]
		detailsMenuCount = detailsMenuCount + 1
		local myDetailsMenuCount = detailsMenuCount
	
		local tweenTime = .35
		detailsCloseImageLabel.ImageTransparency = .7
	
		if assetId then
			fastSpawn(function()
				local assetInfo = getAssetInfo(assetId)
				if assetInfo and detailsMenuCount == myDetailsMenuCount then
					if assetInfo.Name then
						detailsNameLabel.Text = assetInfo.Name
					end
					if assetInfo.Description then
						detailsDescriptionLabel.Text = assetInfo.Description
						detailsScrollingFrame.CanvasSize = UDim2.new(1,-30,0,detailsDescriptionLabel.TextBounds.Y)
						detailsScrollingFrame.CanvasPosition = Vector2.new(0,0)
					end
					if assetInfo.Creator and assetInfo.Creator.Name then
						detailsCreatorLabel.Text = 'By '..assetInfo.Creator.Name
					end
					detailsImageLabel.Image = assetImageUrl150..tostring(assetId)
				end
			end)
		end
	
		closeMenu()
	
		detailsMenuFrame:TweenPosition(UDim2.new(0,15,.15,0),'InOut','Quad',tweenTime,true)
		shadeLayer.Visible = true
		fastSpawn(function()
			wait()	--This is to make sure that the fade in happens after the fadeout of the menu, to overwrite that shade tween
			tweenProperty(shadeLayer, 'BackgroundTransparency', nil, .45, tweenTime, easeFilters.quad, easeFilters.easeInOut)
		end)
	--end
end

bindClick(detailsCloseButton,function()
	closeDetails()
end)
connect(detailsCloseButton.MouseButton1Down,function()
	detailsCloseImageLabel.ImageTransparency = .25
end)

local openMenuCount = 0
function openMenu(title, tableOfButtons, assetId)
	openMenuCount = openMenuCount + 1
	local myOpenMenuCount = openMenuCount

	local tweenTime = .25
	menuTitleLabel.Text = title

	if assetId then
		fastSpawn(function()
			local assetInfo = getAssetInfo(assetId)
			if assetInfo and openMenuCount == myOpenMenuCount then
				menuTitleLabel.Text = assetInfo.Name
			end
		end)
	end

	--destroy previous buttons
	for _,v in pairs(menuFrame:GetChildren()) do
		if v.Name == 'OptionButton' then
			v:Destroy()
		end
	end

	for i,v in pairs(tableOfButtons) do
		local button = Instance.new('TextButton')
		button.Name = 'OptionButton'
		button.ZIndex = menuCancelButton.ZIndex
		button.AutoButtonColor = false
		button.Size = UDim2.new(1,0,0,48)
		button.Position = UDim2.new(0,0,0,48*i)
		button.BorderSizePixel = 0
		button.Text = v.text
		button.BackgroundColor3 = Color3.new(1,1,1)	--Color3.fromRGB(255,255,255)
		button.TextColor3 = Color3.new(.295,.295,.295)	--Color3.fromRGB(75,75,75)
		button.FontSize = 'Size18'
		button.Font = 'SourceSansLight'
		local divider = Instance.new('Frame')
		divider.ZIndex = button.ZIndex
		divider.BorderSizePixel = 0
		divider.BackgroundColor3 = Color3.new(.816,.816,.816) --Color3.fromRGB(208,208,208)
		divider.Size = UDim2.new(1,0,0,1)
		divider.Position = UDim2.new(0,0,1,-1)
		divider.Parent = button
		button.Parent = menuFrame
		if v.func then
			bindClick(button,v.func)
		end
	end
	menuFrame.Size = UDim2.new(1,-30,0,48*(#tableOfButtons+2))
	menuFrame:TweenPosition(UDim2.new(0,15,1,-menuFrame.Size.Y.Offset),'InOut','Quad',tweenTime,true)
	shadeLayer.Visible = true
	fastSpawn(function()
		tweenProperty(shadeLayer, 'BackgroundTransparency', nil, .45, tweenTime, easeFilters.quad, easeFilters.easeInOut)
	end)
end

function closeMenu()
	local tweenTime = .13
	menuFrame:TweenPosition(UDim2.new(0,15,1,0),'InOut','Quad',tweenTime,true)
	fastSpawn(function()
		local shadeTweenCompleted = tweenProperty(shadeLayer, 'BackgroundTransparency', nil, 1, tweenTime, easeFilters.quad, easeFilters.easeInOut)
		if shadeTweenCompleted then
			shadeLayer.Visible = false
		end
	end)
end

bindClick(menuCancelButton,closeMenu)
bindClick(shadeLayer,function()
	closeMenu()
	closeDetails()
end)



do
	local avatarToggleDebounce = false
	local toggleTime = .1
	
	local function renderAvatarSwitch()
		r6Label.TextColor3 = avatarType == 'R6' and Color3.new(1,1,1) or Color3.new(.44,.44,.44)
		r15Label.TextColor3 = avatarType == 'R15' and Color3.new(1,1,1) or Color3.new(.44,.44,.44)
		local desiredPosition = avatarType == 'R6' and UDim2.new(0,2,0,2) or UDim2.new(1,-32,0,2)
		avatarTypeSwitch:TweenPosition(desiredPosition, 'InOut', 'Quad', toggleTime, true)--'InOut', 'Quad', toggleTime, true)
		runParticleEmitter()
		--todo: play a switch sound
	end

	function updateAvatarType()
		renderAvatarSwitch()
		if avatarType == 'R15' then
			createR15Rig()
		else
			createR6Rig()
		end
	end

	function toggleAvatarType()
		if not avatarToggleDebounce then
			avatarToggleDebounce = true
			avatarType = avatarType == 'R6' and 'R15' or 'R6'
			updateAvatarType()
			wait(toggleTime)
			avatarToggleDebounce = false
		end
	end
	bindClick(avatarTypeButton,toggleAvatarType)
	updateAvatarType()	--loads character and properly sets defaults
end


function addToRecentAssetsList(assetId)
	if #recentAssetList > 0 then
		for i=#recentAssetList, 1, -1 do
			if recentAssetList[i] == assetId then
				table.remove(recentAssetList,i)
			end
		end
	end
	table.insert(recentAssetList,1,assetId)
	while #recentAssetList > maxNumberOfRecentAssets do	--remove any excess
		table.remove(recentAssetList,#recentAssetList)
	end
end

function scrapeXMLData(s, pattern)
	local pattern = pattern..'">'
	local findStart, findEnd = string.find(s, pattern, 1, true)
	if findEnd then
		local valueStart = findEnd + 1
		local findStart, findEnd = string.find(s, '<', valueStart, true)
		if findStart then
			local value = string.sub(s, valueStart, findStart-1)
			if value then
				return value
			end
		end
	end
end

--populate currentlyWearing and recentAssetsList--
local characterFetchRequest = httpGet(characterFetch..userId)
print('characterFetch:', characterFetchRequest)
characterFetchRequest = httpService:JSONDecode(characterFetchRequest)
if characterFetchRequest then
	print('found character fetch')
	local bodyColorsRequest = characterFetchRequest['bodyColors']
	if bodyColorsRequest then
		bodyColors = copyTable(bodyColorsRequest)
		savedBodyColors = copyTable(bodyColors)
		updateCharacterBodyColors()
	else
		-- time to get hacky
		local bodyColorsUrlRequest = characterFetchRequest['bodyColorsUrl']
		if bodyColorsUrlRequest then
			local bodyColorXMLData = httpGet(bodyColorsUrlRequest)
			print('Body Color Data:   ', bodyColorXMLData)
			if bodyColorXMLData then
				for bodyColorIndex,_ in pairs(bodyColors) do
					local bodyColorRip = scrapeXMLData(bodyColorXMLData, bodyColorIndex)
					print('ripped', bodyColorIndex, bodyColorRip)
					if bodyColorRip then
						bodyColors[bodyColorIndex] = tonumber(bodyColorRip)
					end
				end
				savedBodyColors = copyTable(bodyColors)
				updateCharacterBodyColors();syncDraft()
			end
		end
	end
	local requestedAvatarType = characterFetchRequest['resolvedAvatarType']
	if requestedAvatarType then
		local avatarTypeDifferent = requestedAvatarType ~= avatarType
		avatarType = requestedAvatarType
		savedAvatarType = avatarType
		if avatarTypeDifferent then
			updateAvatarType()
		end
	end
end

local waitingForInitialLoad = false
local currentlyWearingRequest = httpGet(avatarUrlPrefix.."/v1/users/"..userId.."/currently-wearing")
currentlyWearingRequest = httpService:JSONDecode(currentlyWearingRequest)
if currentlyWearingRequest and currentlyWearingRequest['assetIds'] then
	recentAssetList = copyTable(currentlyWearingRequest['assetIds'])
	local waitingAssets = {}
	for _,assetId in pairs(currentlyWearingRequest['assetIds']) do
		waitingAssets[assetId] = false
		fastSpawn(function()
			equipAsset(assetId)
			waitingAssets[assetId] = true
		end)
	end
	
	--after all fetched assets are equipped, we can update the savedWearingAssets table.
	waitingForInitialLoad = true
	fastSpawn(function()
		local startTime = tick()
		while tick()-startTime < 15 do
			local allAssetsLoaded = true
			for assetId,handled in pairs(waitingAssets) do
				if not handled then
					allAssetsLoaded = false
					break
				end
			end
			if allAssetsLoaded then
				local timeToLoadAssets = tick()-startTime
				print('AllAssetsLoaded',timeToLoadAssets)
				if timeToLoadAssets < 5 then
					savedWearingAssets = copyTable(currentlyWearing)
				end
				break
			end
			wait()
		end
		waitingForInitialLoad = false
	end)
end

--------------------------------------------------

local function wearOutfit(outfitId)
    if true then return loadOutfit(outfitId) end
	--unequip all current assets
	for i=#currentlyWearing, 1, -1 do
		local currentAssetId = currentlyWearing[i]
		unequipAsset(currentAssetId)
	end

	local outfitData = httpGet(avatarUrlPrefix.."/v1/outfits/"..outfitId.."/details")
	if outfitData then
		--equip outfit assets
		local outfitData = httpService:JSONDecode(outfitData)
		if outfitData then
			local outfitAssets = outfitData['assets']
			if outfitAssets then
				for _,assetInfo in pairs(outfitAssets) do
					local assetId = assetInfo.id
					fastSpawn(function()
						equipAsset(assetId)
					end)
				end
			end
		end

		--body colors
		local outfitBodyColors = outfitData['bodyColors']
		if outfitBodyColors then
			for name, mapName in pairs(bodyColorNameMap) do
				local color = outfitBodyColors[mapName]
				if color then
					bodyColors[name] = color
				end
			end
		end
		updateCharacterBodyColors()

	end
end

local function outfitImageFetch(outfitId, attemptNumber)
	return thumbnail("Outfit"..outfitId)
	--the below code works and is the retry functionality for a working image that we want, but the url handed back does not work for asset images
	--[[local attemptNumber = (attemptNumber or 0) + 1
	if attemptNumber > 10 then
		return 'OutfitFailedToLoad'
	end
	local returnString = httpGet("http://www.roblox.com/Outfit-Thumbnail/Json?userOutfitId="..outfitId.."&width=352&height=352&format=png")
	if returnString then
		local returnData = httpService:JSONDecode(returnString)
		if returnData and returnData.Final then
			return returnData.Url
		end
	end
	wait(1)
	return outfitImageFetch(outfitId, attemptNumber)]]
end

local function renderCard(i, cardName, image, buttonSize, extraVerticalShift)
	local row = math.ceil(i/buttonsPerRow)
	local column = ((i-1)%buttonsPerRow)+1
	local assetButton = Instance.new('ImageButton')
	assetButton.Name = 'AssetButton'..cardName
	assetButton.AutoButtonColor = false
	assetButton.BackgroundColor3 = Color3.fromRGB(255,255,255)
	assetButton.BorderColor3 = Color3.fromRGB(208,208,208)
	assetButton.Image = "rbxasset://textures/AvatarEditorIcons/ingame/gr-card corner.png" --@2x	--assetImageUrl..tostring(assetId)
	assetButton.ScaleType = Enum.ScaleType.Slice
	assetButton.SliceCenter = Rect.new(6,6,7,7)--Rect.new(13,13,13,13)
	assetButton.BackgroundTransparency = 1
	assetButton.Size = UDim2.new(0,buttonSize+6,0,buttonSize+6)	--+10 for 2x
	assetButton.Position = UDim2.new(0,gridPadding + (column-1)*(buttonSize+gridPadding) -3,0,gridPadding + (row-1)*(buttonSize+gridPadding) -3 + extraVerticalShift)
	assetButton.ZIndex = scrollingFrame.ZIndex
	local assetImageLabel = Instance.new('ImageLabel')
	assetImageLabel.BackgroundTransparency = 1
	assetImageLabel.BorderSizePixel = 0
	assetImageLabel.Image = thumbnail(cardName)
	assetImageLabel.Size = UDim2.new(1,-8,1,-8)		-- -14 for 2x
	assetImageLabel.Position = UDim2.new(0,4,0,4)
	assetImageLabel.ZIndex = assetButton.ZIndex+1
	assetImageLabel.Parent = assetButton
    if itemName(cardName) then local label=Instance.new("TextLabel");label.Name="ItemName";label.Text=itemName(cardName);label.BackgroundTransparency=.3;label.BackgroundColor3=Color3.new(0,0,0);label.TextColor3=Color3.new(1,1,1);label.Font=Enum.Font.SourceSans;label.FontSize=Enum.FontSize.Size18;label.TextWrapped=true;label.Size=UDim2.new(1,0,.35,0);label.Position=UDim2.new(0,0,.65,0);label.ZIndex=assetButton.ZIndex+2;label.Parent=assetButton end
	return assetButton
end


local extraVerticalShift = 25 --this is to make space for page title labels
local loadingContent = false
local renderedRecommended = false
local currentLoadingContentCall = 0
local assetList = {}	--all owned assets of type
local renderedAssets = {}

function loadMoreListContent()
	print('load content')
	loadingContent = true
	currentLoadingContentCall = currentLoadingContentCall + 1
	local thisLoadingContentCall = currentLoadingContentCall
	local reachedBottom = false

	if page.typeName then
		--Any basic asset type page
		local desiredPageNumber = math.ceil((#assetList)/itemsPerPage)+1
		local typeStuff = httpGet(urlPrefix.."/v1/users/"..userId.."/inventory/"..page.typeName.."?pageNumber="..desiredPageNumber.."&itemsPerPage="..itemsPerPage)
		if thisLoadingContentCall == currentLoadingContentCall then
			typeStuff = httpService:JSONDecode(typeStuff)
			if typeStuff and typeStuff['data'] then
				local pageAssets = typeStuff['data']
				addTables(assetList, pageAssets)
				if #pageAssets < itemsPerPage then
					reachedBottom = true
				end
			end
		end

	elseif page.name == 'Recent' then
		assetList = copyTable(recentAssetList)

	elseif page.name == 'Outfits' then
		--[[local outfitStuff = httpGet(avatarUrlPrefix.."/v1/users/"..userId.."/outfits")
		local packagesStuff = httpGet(urlPrefix.."/v1/users/"..userId.."/inventory/package")
		if thisLoadingContentCall == currentLoadingContentCall then

			local outfitsVerticalShift = 0
			local packagesVerticalShift = 0
			local buttonSize = (scrollingFrame.AbsoluteSize.X - ((buttonsPerRow+1) * gridPadding)) / buttonsPerRow

			outfitStuff = httpService:JSONDecode(outfitStuff)
			if outfitStuff and outfitStuff['data'] then
				for i, outfitData in pairs(outfitStuff['data']) do
					local outfitName = outfitData.name
					local outfitId = outfitData.id
					local assetButton = renderCard(i, "Outfit"..tostring(outfitId),"", buttonSize, extraVerticalShift)

					local wearFunction = function()
						closeMenu()
						runParticleEmitter()
						wearOutfit(outfitId)
					end
					bindClick(assetButton,wearFunction)
					connect(assetButton.TouchLongPress,wearFunction)

					assetButton.Parent = scrollingFrame
					fastSpawn(function()
						local imgUrl = outfitImageFetch(outfitId)
						if imgUrl and assetButton and assetButton.Parent then
							local imageLabel = assetButton:FindFirstChild('ImageLabel')
							if imageLabel then
								imageLabel.Image = imgUrl
							end
						end
					end)
				end

				print('#outfits:',#outfitStuff['data'])
				outfitsVerticalShift = math.ceil((#outfitStuff['data'])/buttonsPerRow) * (buttonSize+gridPadding)
			end

			--todo: generate label for "Packages"
			packagesStuff = httpService:JSONDecode(packagesStuff)
			if packagesStuff and packagesStuff['data'] then
				for i, assetId in pairs(packagesStuff['data']) do
					--todo: calculate additional vertical shift from outfits
					local assetButton = renderCard(i, tostring(assetId), assetImageUrl..tostring(assetId), buttonSize, extraVerticalShift * 2 + outfitsVerticalShift)
					local wearFunction = function()
						closeMenu()
						local containingAssetsRaw = httpGet("http://assetgame.roblox.com/Game/GetAssetIdsForPackageId?packageId="..assetId)
						if containingAssetsRaw then
							local containingAssets = httpService:JSONDecode(containingAssetsRaw)
							if containingAssets then
								runParticleEmitter()
								for i, pieceAssetId in pairs(containingAssets) do
									fastSpawn(function()
										addToRecentAssetsList(pieceAssetId)
										equipAsset(pieceAssetId)
									end)
								end
							end
						end
					end
					bindClick(assetButton,wearFunction)
					connect(assetButton.TouchLongPress,function()
						openMenu('',
							{
								{text = 'Wear Pieces', func = wearFunction},
								{text = 'View details',func = function() openDetails(assetId) end},
							},
							assetId
						)
					end)
					assetButton.Parent = scrollingFrame
				end

				print('#packages:',#packagesStuff['data'])
				packagesVerticalShift = math.ceil((#packagesStuff['data'])/buttonsPerRow) * (buttonSize+gridPadding)
			end

			print('CanvasSize:', packagesVerticalShift + outfitsVerticalShift + gridPadding*2 + extraVerticalShift*3)
			scrollingFrame.CanvasSize = UDim2.new(0,0,0,packagesVerticalShift + outfitsVerticalShift + gridPadding*2 + extraVerticalShift*3)

		end]]

		local desiredPageNumber = math.ceil((#assetList)/itemsPerPage)+1
		local typeStuff = httpGet(avatarUrlPrefix.."/v1/users/"..userId.."/outfits?page="..desiredPageNumber.."&itemsPerPage="..itemsPerPage)
		if thisLoadingContentCall == currentLoadingContentCall then
			typeStuff = httpService:JSONDecode(typeStuff)
			if typeStuff and typeStuff['data'] then
				local pageData = typeStuff['data']
				local outfitIds = {}
				for i,v in pairs(pageData) do
					outfitIds[i] = v.id
				end
				addTables(assetList, outfitIds)
				if #outfitIds < itemsPerPage then
					reachedBottom = true
				end
			end
		end

	elseif page.name == 'Skin Tone' then
		local buttonsPerRow = 5	--This is in local scope because skincolor is layed out differently
		local gridPadding = 12	--6
		local availibleWidth = scrollingFrame.AbsoluteSize.X
		local buttonSize = (availibleWidth - ((buttonsPerRow+1) * gridPadding)) / buttonsPerRow

		local selectionFrame = Instance.new('ImageLabel')
		selectionFrame.Image = "rbxasset://textures/AvatarEditorIcons/ColorSelection/gr-ring-selector@2x.png" --"rbxasset://textures/AvatarEditorIcons/ColorSelection/gr-ring-selector@2x.png"--"rbxasset://textures/AvatarEditorIcons/ColorSelection/gr-ring-selector@2x.png"--431878555"		--todo: Replace this with an image from content folder
		selectionFrame.BackgroundTransparency = 1
		selectionFrame.Size = UDim2.new(1.2,0,1.2,0)
		selectionFrame.Position = UDim2.new(-.1,0,-.1,0)
		selectionFrame.ImageColor3 = Color3.new(.2,.65,.9)
		selectionFrame.ZIndex = scrollingFrame.ZIndex + 3

		scrollingFrame.CanvasSize = UDim2.new(0,0,0,math.ceil(#skinColorList/buttonsPerRow) * (buttonSize+gridPadding) + gridPadding + extraVerticalShift)
		for i,brickColor in pairs(skinColorList) do
			local row = math.ceil(i/buttonsPerRow)
			local column = ((i-1)%buttonsPerRow)+1
			local colorButton = Instance.new('ImageButton')
			colorButton.AutoButtonColor = false
			colorButton.BorderSizePixel = 0
			colorButton.BackgroundTransparency = 1
			colorButton.Image = "rbxasset://textures/AvatarEditorIcons/ColorSelection/gr-circle-white@2x.png"--"rbxasset://textures/AvatarEditorIcons/ColorSelection/gr-circle-white@2x.png"--"rbxasset://textures/AvatarEditorIcons/ColorSelection/gr-circle-white@2x.png"	--todo: Replace this with an image from content folder
			colorButton.ImageColor3 = brickColor.Color
			colorButton.Size = UDim2.new(0,buttonSize,0,buttonSize)
			colorButton.Position = UDim2.new(0,gridPadding + (column-1)*(buttonSize+gridPadding),0,gridPadding + (row-1)*(buttonSize+gridPadding) + extraVerticalShift)
			colorButton.ZIndex = scrollingFrame.ZIndex + 2
			local selectFunction = function()
				runParticleEmitter()
				selectionFrame.Parent = colorButton
				bodyColors[colorPart] = brickColor.number
				updateCharacterBodyColors();syncDraft()
			end
			bindClick(colorButton,selectFunction)

			local colorButtonShadow = Instance.new('ImageLabel')
			colorButtonShadow.Name = 'DropShadow'
			colorButtonShadow.Image = "rbxasset://textures/AvatarEditorIcons/ColorSelection/gr-circle-shadow@2x.png"
			colorButtonShadow.BackgroundTransparency = 1
			colorButtonShadow.ZIndex = colorButton.ZIndex - 1
			colorButtonShadow.Size = UDim2.new(1.13,0,1.13,0)
			colorButtonShadow.Position = UDim2.new(-.07,0,-.07,0)
			colorButtonShadow.Parent = colorButton

			colorButton.Name="Color_"..brickColor.number
            colorButton.Parent = scrollingFrame
			if bodyColors[colorPart] == brickColor.number then
				selectionFrame.Parent = colorButton
			end
		end

	end

	-- This is for rendering a generically layed out page
	if thisLoadingContentCall == currentLoadingContentCall then
		local availibleWidth = scrollingFrame.AbsoluteSize.X
		local buttonSize = (availibleWidth - ((buttonsPerRow+1) * gridPadding)) / buttonsPerRow

		if not page.special then
			scrollingFrame.CanvasSize = UDim2.new(0,0,0,math.ceil(#assetList/buttonsPerRow) * (buttonSize+gridPadding) + gridPadding + extraVerticalShift)
			for i,assetId in pairs(assetList) do
				if not renderedAssets['index'..i] then
					local assetButton = nil

					if page.name == 'Outfits' then
						assetButton = renderCard(i, "Outfit"..tostring(assetId),"", buttonSize, extraVerticalShift)
	
						local wearFunction = function()
							closeMenu()
							runParticleEmitter()
							wearOutfit(assetId)
						end
						bindClick(assetButton,wearFunction)
						connect(assetButton.TouchLongPress,wearFunction)
	
						assetButton.Parent = scrollingFrame
						fastSpawn(function()
							local imgUrl = outfitImageFetch(assetId)
							if imgUrl and assetButton and assetButton.Parent then
								local imageLabel = assetButton:FindFirstChild('ImageLabel')
								if imageLabel then
									imageLabel.Image = imgUrl
								end
							end
						end)

					else
						assetButton = renderCard(i, tostring(assetId), assetImageUrl..tostring(assetId), buttonSize, extraVerticalShift)

						if findIfEquipped(assetId) then
							local selectionFrame = selectionFrameTemplate:clone()
							selectionFrame.Name = 'SelectionFrame'
							selectionFrame.ZIndex = assetButton.ZIndex+2
							selectionFrame.Visible = true
							selectionFrame.Parent = assetButton
						end
						local wearFunction = function()
							runParticleEmitter()
							addToRecentAssetsList(assetId)
							equipAsset(assetId)
							closeMenu()
						end
						local takeOffFunction = function()
							runParticleEmitter()
							unequipAsset(assetId)
							closeMenu()
						end
						bindClick(assetButton,function()
							if findIfEquipped(assetId) then
								takeOffFunction()
							else
								wearFunction()
							end
						end)
						connect(assetButton.TouchLongPress,function()
							local wearOrUnwearOption = nil
							if findIfEquipped(assetId) then
								wearOrUnwearOption = {text = 'Take Off', func = takeOffFunction}
							else
								wearOrUnwearOption = {text = 'Wear', func = wearFunction}
							end
							openMenu('',
								{
									wearOrUnwearOption,
									{text = 'View details',func = function() openDetails(assetId) end},
								},
								assetId
							)
						end)
					end

					assetButton.Parent = scrollingFrame
					renderedAssets['index'..i] = assetButton
				end
			end

		end

		if reachedBottom and not renderedRecommended and page.RecommendedSort then
			renderedRecommended = true

			local recommendedLabel = Instance.new('TextLabel')
			recommendedLabel.Text = "RECOMMENDED"--"Recommended"
			recommendedLabel.BackgroundTransparency = 1
			recommendedLabel.TextColor3 = Color3.fromRGB(65,78,89)
			recommendedLabel.Size = UDim2.new(1,-14,0,25)
			recommendedLabel.Position = UDim2.new(0,7,0,scrollingFrame.CanvasSize.Y.Offset -3)
			recommendedLabel.Font = "SourceSansLight"
			recommendedLabel.FontSize = "Size18"
			recommendedLabel.TextXAlignment = "Left"
			recommendedLabel.ZIndex = 3
			recommendedLabel.Parent = scrollingFrame

			local recommendedAssetList = {1,2,3,4}	--todo: get an actual recommended list from the website
			for i,assetId in pairs(recommendedAssetList) do
				local row = math.ceil(i/buttonsPerRow)
				local column = ((i-1)%buttonsPerRow)+1

				local assetButton = Instance.new('ImageButton')
				assetButton.Name = 'RecommendedAssetButton'..tostring(assetId)
				assetButton.AutoButtonColor = false
				assetButton.BackgroundColor3 = Color3.fromRGB(255,255,255)
				assetButton.BorderColor3 = Color3.fromRGB(208,208,208)
				assetButton.Image = "rbxasset://textures/AvatarEditorIcons/ingame/gr-card corner.png" --@2x	--assetImageUrl..tostring(assetId)
				assetButton.ScaleType = Enum.ScaleType.Slice
				assetButton.SliceCenter = Rect.new(6,6,7,7)--Rect.new(13,13,13,13)
				assetButton.BackgroundTransparency = 1
				assetButton.Size = UDim2.new(0,buttonSize+6,0,buttonSize+6)	--+10 for 2x
				assetButton.Position = UDim2.new(0,gridPadding + (column-1)*(buttonSize+gridPadding) -3,0,scrollingFrame.CanvasSize.Y.Offset + gridPadding + (row-1)*(buttonSize+gridPadding) -10 + extraVerticalShift)
				assetButton.ZIndex = scrollingFrame.ZIndex
				local assetImageLabel = Instance.new('ImageLabel')
				assetImageLabel.BackgroundTransparency = 1
				assetImageLabel.BorderSizePixel = 0
				assetImageLabel.Image = assetImageUrl..tostring(assetId)
				assetImageLabel.Size = UDim2.new(1,-8,1,-8)		-- -14 for 2x
				assetImageLabel.Position = UDim2.new(0,4,0,4)
				assetImageLabel.ZIndex = assetButton.ZIndex+1
				assetImageLabel.Parent = assetButton
    if itemName(cardName) then local label=Instance.new("TextLabel");label.Name="ItemName";label.Text=itemName(cardName);label.BackgroundTransparency=.3;label.BackgroundColor3=Color3.new(0,0,0);label.TextColor3=Color3.new(1,1,1);label.Font=Enum.Font.SourceSans;label.FontSize=Enum.FontSize.Size18;label.TextWrapped=true;label.Size=UDim2.new(1,0,.35,0);label.Position=UDim2.new(0,0,.65,0);label.ZIndex=assetButton.ZIndex+2;label.Parent=assetButton end

				bindClick(assetButton,function()
					--todo: open asset webpage
				end)
				connect(assetButton.TouchLongPress,function()
					--todo: open asset webpage
				end)

				assetButton.Parent = scrollingFrame
			end

			scrollingFrame.CanvasSize = UDim2.new(0,0,0,(math.ceil(#assetList/buttonsPerRow) + 1) * (buttonSize+gridPadding) + gridPadding + extraVerticalShift*2)
		end

	end

	loadingContent = false
end

function selectPage(index, desiredPage)
	if desiredPage ~= page then
		print('Switching to page:', desiredPage.name)
		scrollingFrame:ClearAllChildren()
		scrollingFrame.CanvasPosition = Vector2.new(0,0)
		desiredPage.button.BackgroundColor3 = Color3.fromRGB(246,136,2)
		local imageLabel = desiredPage.button:FindFirstChild('ImageLabel')
		if imageLabel then
			imageLabel.Image = desiredPage.iconImageSelected
		end
		local textLabel = desiredPage.button:FindFirstChild('TextLabel')
		if textLabel then
			textLabel.TextColor3 = Color3.fromRGB(255,255,255)
		end

		if page then
			page.button.BackgroundColor3 = Color3.fromRGB(255,255,255)
			local imageLabel = page.button:FindFirstChild('ImageLabel')
			if imageLabel then
				imageLabel.Image = page.iconImage
			end
			local textLabel = page.button:FindFirstChild('TextLabel')
			if textLabel then
				textLabel.TextColor3 = Color3.fromRGB(25,25,25)
			end
		end

		local pageLabel = Instance.new('TextLabel')
		pageLabel.Text = string.upper(desiredPage.name)
		pageLabel.BackgroundTransparency = 1
		pageLabel.TextColor3 = Color3.new(1,1,1)
		pageLabel.Size = UDim2.new(1,-14,0,25)
		pageLabel.Position = UDim2.new(0,7,0,3)
		pageLabel.Font = "SourceSansLight"
		pageLabel.FontSize = "Size18"
		pageLabel.TextXAlignment = "Left"
		pageLabel.ZIndex = 3
		pageLabel.Parent = scrollingFrame

		--Different background color for the body color choice page
		--mainFrame.BackgroundColor3 = desiredPage.name == 'Skin Tone' and Color3.fromRGB(255,255,255) or Color3.fromRGB(227,227,227)
		if desiredPage.name == 'Skin Tone' then
			local whiteFrame = whiteFrameTemplate:clone()
			whiteFrame.Visible = true
			whiteFrame.Parent = scrollingFrame
		end

		accessoriesColumn.Visible = desiredPage.name == 'Hats'

		page = desiredPage
		loadingContent = false
		renderedRecommended = false
		assetList = {}
		renderedAssets = {}

		loadMoreListContent()
	end
end


--This function fades the fake scrollbar out after not being used for 3 seconds
local lastScrollPosition = fakeScrollBar.AbsolutePosition.Y
local lastScrollCount = 0
function updateScrollBarVisibility()
	local thisScrollPosition = fakeScrollBar.AbsolutePosition.Y
	if thisScrollPosition ~= lastScrollPosition then

		--this stops any existing tween
		if tweenPropertyController.tweens[fakeScrollBar] then
			tweenPropertyController.tweens[fakeScrollBar] = nil
		end

		lastScrollPosition = thisScrollPosition

		lastScrollCount = lastScrollCount + 1
		local thisScrollCount = lastScrollCount

		fakeScrollBar.ImageTransparency = .65
		wait(2)
		if thisScrollCount == lastScrollCount then
			tweenProperty(fakeScrollBar, 'ImageTransparency', nil, 1, 1, easeFilters.quad, easeFilters.easeInOut)
		end

	end
end

connect(scrollingFrame.Changed,function(prop)
	local barSize = 0
	local scrollingFrameSize = scrollingFrame.AbsoluteSize.Y
	local canvasPosition = scrollingFrame.CanvasPosition.Y
	local canvasSize = scrollingFrame.CanvasSize.Y.Offset
	if scrollingFrameSize < canvasSize then
		barSize = (scrollingFrameSize / canvasSize) * scrollingFrameSize
	end
	if barSize > 0 then
		fakeScrollBar.Visible = true
		fakeScrollBar.Size = UDim2.new(0,fakeScrollBarWidth,0,barSize)
		local scrollPercent = canvasPosition/(canvasSize-scrollingFrameSize)
		fakeScrollBar.Position = UDim2.new(1,-fakeScrollBarWidth,0,(scrollingFrameSize-barSize) * scrollPercent) + scrollingFrame.Position
	else
		fakeScrollBar.Visible = false
	end

	fastSpawn(updateScrollBarVisibility)

	if prop == 'CanvasPosition' then
		if canvasPosition >= canvasSize - (scrollingFrameSize+2) then
			if not loadingContent and not renderedRecommended and page.infiniteScrolling then
				loadMoreListContent()
			end
		end
	end
end)

function renderTab(index,page)
	local tabButton = Instance.new('ImageButton')
	tabButton.Image = ''
	tabButton.BackgroundColor3 = Color3.fromRGB(255,255,255)
	tabButton.BorderSizePixel = 0
	tabButton.AutoButtonColor = false
	tabButton.Size = UDim2.new(0, tabWidth, 0, tabHeight)
	tabButton.Position = UDim2.new(0, (index-1) * (tabWidth+1), 0, 0)
	tabButton.ZIndex = tabList.ZIndex
	tabButton.Parent = tabList
	page.button = tabButton

	if page.iconImage == '' then
		--If tab has no image, then use placeholder text
		local nameLabel = Instance.new('TextLabel')
		nameLabel.BackgroundTransparency = 1
		nameLabel.Size = UDim2.new(1,0,1,0)
		nameLabel.Text = page.name
		nameLabel.Font = Enum.Font.SourceSansBold
		nameLabel.FontSize = 'Size14'
		nameLabel.TextColor3 = Color3.fromRGB(25,25,25)
		nameLabel.ZIndex = tabButton.ZIndex
		nameLabel.Parent = tabButton
	else
		local imageFrame = Instance.new('ImageLabel')
		imageFrame.BackgroundTransparency = 1
		imageFrame.Size = UDim2.new(0,28,0,28)
		imageFrame.Position = UDim2.new(.5,-14,.5,-14)
		imageFrame.Image = page.iconImage
		imageFrame.ZIndex = tabButton.ZIndex
		imageFrame.Parent = tabButton
	end

	if index > 1 then
		local divider = Instance.new('Frame')
		divider.Name = 'Divider'
		divider.BackgroundColor3 = Color3.fromRGB(227,227,227)
		divider.BorderSizePixel = 0
		divider.Size = UDim2.new(0, 1, .6, 0)
		divider.Position = UDim2.new(0, (index-1) * (tabWidth+1) - 1, .2, 0)
		divider.ZIndex = tabButton.ZIndex + 1
		divider.Parent = tabList
	end

	bindClick(tabButton,function()
		choose(index)
	end)
end

for index, page in pairs(pages) do
	renderTab(index, page)
end

selectPage(1, pages[1])

pcall(function()
	if not isCoreSetup then
		wait(1)
		local starterGui = game:GetService('StarterGui')
		starterGui:SetCore("TopbarEnabled", false)
	end
end)

character.Parent = game.Workspace

local rotation = 0
local lastRotation = 0
local downKeys = {}
local lastTouchInput = nil
local lastTouchPosition = nil
local lastInputBeganPosition = Vector3.new(0,0,0)
local lastEmptyInput = 0

function handleInput(input, soaked)
	if input.UserInputState == Enum.UserInputState.Begin then
		downKeys[input.KeyCode] = true
		if not soaked then

			if input.UserInputType == Enum.UserInputType.MouseButton1 then
				downKeys[Enum.UserInputType.MouseButton1] = true
				lastTouchPosition = input.Position
				lastTouchInput = input
			end
			if input.UserInputType == Enum.UserInputType.Touch then
				lastTouchInput = input
				lastTouchPosition = input.Position
			end

			if input.UserInputType == Enum.UserInputType.Touch or input.UserInputType == Enum.UserInputType.MouseButton1 then

				--This code is here to manually detect when the button is pressed so that upswipes on it also work without thinking that the input was processed.
				if exitFullViewButton.ImageTransparency == 0 and exitFullViewButton.Visible then
					local eventX = input.Position.x
					local eventY = input.Position.y
					local buttonPosX = exitFullViewButton.AbsolutePosition.x
					local buttonPosY = exitFullViewButton.AbsolutePosition.y
					if eventX >= buttonPosX and eventX < buttonPosX + exitFullViewButton.AbsoluteSize.X and eventY >= buttonPosY and eventY < buttonPosY + exitFullViewButton.AbsoluteSize.y then
						setViewMode(false)
					end
				end

				--This is used for doubletap detection
				lastInputBeganPosition = input.Position
			end

		end
	elseif input.UserInputState == Enum.UserInputState.End then
		downKeys[input.KeyCode] = false
		if input.UserInputType == Enum.UserInputType.MouseButton1 then
			downKeys[Enum.UserInputType.MouseButton1] = false
		end
		if lastTouchInput == input or input.UserInputType == Enum.UserInputType.MouseButton1 then
			lastTouchInput = nil
		end

		if not soaked and input.UserInputType == Enum.UserInputType.Touch or input.UserInputType == Enum.UserInputType.MouseButton1 then
			if (lastInputBeganPosition and lastInputBeganPosition - input.Position).magnitude <= tapDistanceThreshold then
				local thisEmptyInput = tick()
				if thisEmptyInput - lastEmptyInput <= doubleTapThreshold then
					setViewMode(not viewMode)
				end
				lastEmptyInput = thisEmptyInput
			end
		end
	elseif input.UserInputState == Enum.UserInputState.Change then
		if not soaked then
			if (lastTouchInput == input and input.UserInputType == Enum.UserInputType.Touch)
				or (input.UserInputType == Enum.UserInputType.MouseMovement and downKeys[Enum.UserInputType.MouseButton1]) then

				local touchDelta = (input.Position - lastTouchPosition)
				lastTouchPosition = input.Position
				rotation = rotation + touchDelta.x * characterRotationSpeed
			end
		end
	end
end

connect(userInputService.InputBegan,handleInput)
connect(userInputService.InputChanged,handleInput)
connect(userInputService.InputEnded,handleInput)

connect(userInputService.TouchSwipe,function(swipeDirection, numberOfTouches, soaked)
	if not soaked then
		if viewMode and swipeDirection == Enum.SwipeDirection.Up then
			setViewMode(false)
		elseif not viewMode and swipeDirection == Enum.SwipeDirection.Down then
			setViewMode(true)
		end
	end
end)

local baseCharacterCFrame = CFrame.new(15.276206, 3.71000099, -16.8211784, 0.766051888, 2.24691483e-039, 0.642794013, 2.93314189e-039, 1, 0, -0.642794013, -1.88538683e-039, 0.766051888)


-- Xbox shell lifecycle/navigation overlay on the original editor functions and GUI.
local this={};local busy=false
local function guarded(fn)
 if busy then return end;busy=true
 local ok,reason=pcall(fn);busy=false
 if not ok then screenGui.ErrorLabel.Text=tostring(reason);screenGui.ErrorLabel.Visible=true;warn('[RecoveredEditor] '..tostring(reason))end
end
local function layout()
 mainFrame.Position=viewMode and UDim2.new(0,0,1,0)or UDim2.new(.06,0,.14,0);mainFrame.Size=UDim2.new(.34,0,.72,0)
 mainFrame.BackgroundColor3=Color3.new(.04,.08,.12);mainFrame.BackgroundTransparency=.25
 tabList.Position=UDim2.new(0,0,0,0);tabList.Size=UDim2.new(1,0,.14,0);tabList.ScrollBarThickness=0;tabList.ClipsDescendants=true
 scrollingFrame.Position=UDim2.new(0,0,.16,0);scrollingFrame.Size=UDim2.new(1,0,.84,0);scrollingFrame.ScrollBarThickness=0
 fakeScrollBar.Visible=false;accessoriesColumn.Visible=false;avatarTypeSwitchFrame.Visible=false
end
layout()
local hint=Instance.new('TextLabel');hint.Name='RecoveredEditorHint';hint.BackgroundTransparency=1;hint.Position=UDim2.new(.06,0,.89,0);hint.Size=UDim2.new(.88,0,.07,0);hint.Font=Enum.Font.SourceSans;hint.FontSize=Enum.FontSize.Size24;hint.TextColor3=Color3.new(1,1,1);hint.Text='A Select   B Back   X Save   Y Full View   LB/RB Category   R3 Body Part   Menu Outfits';hint.Parent=screenGui
local status=Instance.new('TextLabel');status.Name='RecoveredEditorStatus';status.BackgroundTransparency=1;status.Position=UDim2.new(.06,0,.03,0);status.Size=UDim2.new(.8,0,.08,0);status.Font=Enum.Font.SourceSans;status.FontSize=Enum.FontSize.Size24;status.TextColor3=Color3.new(1,1,1);status.Text='AVATAR — '..saved.display_name;status.TextXAlignment=Enum.TextXAlignment.Left;status.Parent=screenGui
-- Keep original MouseButton1Click behavior for A. The generator adds register
-- at the original callback sites rather than inventing a separate equip flow.
local currentCategory=1
choose=function(index)
 currentCategory=(index-1)%#pages+1
 layout();selectPage(currentCategory,pages[currentCategory])
 status.Text=pages[currentCategory].name..' — '..saved.display_name
 local choices={}
 for _,child in pairs(scrollingFrame:GetChildren())do if child:IsA('GuiButton')then table.insert(choices,child)end end
 table.sort(choices,function(a,b)return a.Position.Y.Offset==b.Position.Y.Offset and a.Position.X.Offset<b.Position.X.Offset or a.Position.Y.Offset<b.Position.Y.Offset end)
 local columns=pages[currentCategory].name=='Skin Tone'and 5 or 4
 for i,b in pairs(choices)do b.NextSelectionLeft=choices[i-1]or b;b.NextSelectionRight=choices[i+1]or b;b.NextSelectionUp=choices[i-columns]or pages[currentCategory].button;b.NextSelectionDown=choices[i+columns]or b end
 for i,p in pairs(pages)do p.button.NextSelectionLeft=pages[i-1]and pages[i-1].button or p.button;p.button.NextSelectionRight=pages[i+1]and pages[i+1].button or p.button;p.button.NextSelectionDown=choices[1]or p.button end
 gui.SelectedCoreObject=choices[1]or pages[currentCategory].button
 tabList.CanvasPosition=Vector2.new(math.max(0,(currentCategory-1)*61-tabList.AbsoluteSize.X+61),0)
 print('[RecoveredEditor] Category '..pages[currentCategory].name)
end
local originalCloseMenu=closeMenu
closeMenu=function()originalCloseMenu();if focused then choose(currentCategory)end end
local function showOptions(title,options)
 openMenu(title,options)
 local buttons={};for _,b in pairs(menuFrame:GetChildren())do if b:IsA('GuiButton')then table.insert(buttons,b)end end
 table.sort(buttons,function(a,b)return a.Position.Y.Scale*menuFrame.Size.Y.Offset+a.Position.Y.Offset<b.Position.Y.Scale*menuFrame.Size.Y.Offset+b.Position.Y.Offset end)
 for i,b in pairs(buttons)do b.NextSelectionUp=buttons[i-1]or b;b.NextSelectionDown=buttons[i+1]or b;b.NextSelectionLeft=b;b.NextSelectionRight=b end
 gui.SelectedCoreObject=buttons[1]
end
local function bodyPartMenu()
 local options={}
 for _,name in pairs({'HeadColor','TorsoColor','LeftArmColor','RightArmColor','LeftLegColor','RightLegColor'})do local part=name;table.insert(options,{text=colorMap[part]:gsub('_',' '),func=function()colorPart=part;closeMenu();for i,p in pairs(pages)do if p.name=='Skin Tone'then page=nil;choose(i);break end end;status.Text='Body color: '..colorMap[part]:gsub('_',' ')end})end
 showOptions('BODY PART',options)
end
local function outfitMenu()
 if catalog.supports_outfits==false then status.Text='Public accounts: body colors supported; inventory and outfits unavailable';return end
 local options={{text='Save current outfit',func=function()local name=bridge:Prompt('Outfit name','My outfit');if name~=''then syncDraft();request('outfit-save',{name=name,appearance=draft.appearance});outfits=request('outfits').outfits;outfitMap={};for _,o in pairs(outfits)do outfitMap[o.outfit_id]=o end end;closeMenu();page=nil;choose(2)end}}
 table.insert(options,{text='Reset draft to default',func=function()
  draft.appearance=copy(catalog.default_appearance)
  for name,key in pairs(colorMap)do bodyColors[name]=draft.appearance.body_colors[key]end
  currentlyWearing=appearanceIds(draft.appearance);refreshPreview();closeMenu()
  page=nil;choose(currentCategory);status.Text='Default draft — X Save or B Cancel'
 end})
 local selected=gui.SelectedCoreObject
 local id=selected and selected.Name:match('^AssetButtonOutfit(.+)$')
 local o=id and outfitMap[id]
 if o then options={
   {text='Equip and save',func=function()saved=request('equip:'..o.outfit_id,{profile_version=saved.profile_version});draft=copy(saved);publishProfile();loadOutfit(o.outfit_id);closeMenu()end},
   {text='Use on login',func=function()saved=request('equip:'..o.outfit_id,{profile_version=saved.profile_version,default_on_login=true});draft=copy(saved);publishProfile();loadOutfit(o.outfit_id);closeMenu()end},
   {text='Rename',func=function()local name=bridge:Prompt('Outfit name',o.name);if name~=''then request('rename:'..o.outfit_id,{name=name});o.name=name end;closeMenu();page=nil;choose(2)end},
   {text='Delete',func=function()request('delete:'..o.outfit_id);saved=request('get');publishProfile();outfits=request('outfits').outfits;outfitMap[o.outfit_id]=nil;closeMenu();page=nil;choose(2)end}
  }end
 showOptions(o and o.name or 'OUTFITS — select an outfit tile for more actions',options)
end
-- Camera controller interpolation and original resting camera are retained;
-- preview is moved to its original scene coordinates by the existing pipeline.
local cameraTarget=defaultCamera
local cameraOffset=environmentOffset
cameraController.OnCamera=function(value)cameraTarget=value end
cameraController.GetCamera=function()return cameraTarget end
runService:BindToRenderStep('RecoveredEditorCamera',Enum.RenderPriority.Camera.Value+1,function()
 -- The shell camera's delayed transition tweens can outlive DisableCameraControl.
 -- Keep their post effects away from this preview until the editor is closed.
 platform.BlurIntensity=0;platform.Brightness=0;platform.Contrast=0;platform.GrayscaleLevel=0;platform.TintColor=Color3.new(1,1,1)
 camera.CameraType=Enum.CameraType.Scriptable;camera.CoordinateFrame=cameraTarget+cameraOffset
 camera.Focus=previewOrigin;camera.CameraSubject=humanoid
 if character and character.Parent then character:SetPrimaryPartCFrame(previewOrigin*CFrame.Angles(0,rotation,0))end
end)
connect(userInputService.InputChanged,function(input)
 if focused and input.KeyCode==Enum.KeyCode.Thumbstick2 then rotation=rotation+input.Position.X*.045 end
end)
refreshPreview()
function this:GetName()return 'Avatar Customization'end
function this:GetDefaultSelectableObject()return pages[currentCategory].button end
local function reflow()
 fastSpawn(function()
  runService.RenderStepped:wait()
  if alive and not shadeLayer.Visible then
   local selected=gui.SelectedCoreObject;local name=selected and selected.Name
   page=nil;choose(currentCategory)
   if name then gui.SelectedCoreObject=scrollingFrame:FindFirstChild(name)or gui.SelectedCoreObject end
  end
 end)
end
function this:Show()screenGui.Parent=root.Parent;layout();choose(currentCategory);reflow();print('[RecoveredEditor] Original V1 editor opened')end
function this:Focus()
 focused=true
 actions:BindCoreAction('RecoveredEditorActions',function(_,state,input)
  if state~=Enum.UserInputState.Begin or not focused then return end
  guarded(function()
   local key=input.KeyCode
   if key==Enum.KeyCode.ButtonA then local fn=activations[gui.SelectedCoreObject];if fn then fn()end
   elseif key==Enum.KeyCode.ButtonB then if viewMode then setViewMode(false);layout()elseif shadeLayer.Visible then closeMenu()else require(shellModules.ScreenManager):CloseCurrent()end
   elseif key==Enum.KeyCode.ButtonX then saveProfile();status.Text='Saved — applies on next game entry/rejoin'
   elseif key==Enum.KeyCode.ButtonY then setViewMode(not viewMode)
   elseif key==Enum.KeyCode.ButtonL1 then choose(currentCategory-1)
   elseif key==Enum.KeyCode.ButtonR1 then choose(currentCategory+1)
   elseif key==Enum.KeyCode.ButtonR3 then bodyPartMenu()
   elseif key==Enum.KeyCode.ButtonStart then outfitMenu()end
  end)
 end,false,Enum.KeyCode.ButtonA,Enum.KeyCode.ButtonB,Enum.KeyCode.ButtonX,Enum.KeyCode.ButtonY,Enum.KeyCode.ButtonL1,Enum.KeyCode.ButtonR1,Enum.KeyCode.ButtonR3,Enum.KeyCode.ButtonStart)
 choose(currentCategory)
end
function this:RemoveFocus()focused=false;actions:UnbindCoreAction('RecoveredEditorActions')end
function this:Hide()
 self:RemoveFocus();alive=false
 for _,conn in pairs(connections)do conn:disconnect()end
 runService:UnbindFromRenderStep('RecoveredEditorCamera');cameraController.Cancel();cameraController.OnCamera=nil;cameraController.GetCamera=nil
 for _,values in pairs(tweenPropertyController.tweens)do for key in pairs(values)do values[key]=nil end end
 bridge:Preview('');environment:Destroy();screenGui:Destroy()
 for key,value in pairs(oldLighting)do lighting[key]=value end
 for _,child in pairs(oldSky)do child.Parent=lighting end
 camera.FieldOfView=oldFov;camera.Focus=oldFocus;camera.CameraSubject=oldSubject
 print('[RecoveredEnvironment] Room released; shell lighting and sky restored')
 camera.CoordinateFrame=oldCamera;camera.CameraType=oldType
 if home and homeState then home.BackgroundTransparency=homeState[1];home.Background.BackgroundTransparency=homeState[2];home.Background.ImageTransparency=homeState[3];home.Background.CrossfadeBackground.ImageTransparency=homeState[4]end
 for name,value in pairs(effects)do platform[name]=value end
 if require(shellModules.Utility).IsFastFlagEnabled('Durango3DBackground')then manager:EnableCameraControl()end
 print('[RecoveredEditor] Closed; unsaved draft discarded and preview released')
end
connect(screenGui.Changed,function(name)if name=='AbsoluteSize'then layout();reflow()end end)
-- A one-shot native regression drives the same action callbacks as the controller.
local command=Instance.new('BindableFunction');command.Name='RecoveredEditorTest';command.Parent=screenGui
command.OnInvoke=function(op,arg)
 if op=='category'then for i,p in pairs(pages)do if p.name==arg then choose(i);return end end;error('Unknown category')
 elseif op=='save'then return saveProfile().profile_version
 elseif op=='appearance'then syncDraft();return http:JSONEncode(draft.appearance)
 elseif op=='fullview'then setViewMode(not viewMode)
 end
end
return this

end
