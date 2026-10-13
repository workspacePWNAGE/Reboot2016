-- Data-only compatibility for the original Xbox AvatarPane, AvatarTile and OutfitTile.
local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules=root:WaitForChild('Modules')
local account=require(modules.OfflineAccountProfile)
local Utility=require(modules.Utility)
local EventHub=require(modules.EventHub)
local http=game:GetService('HttpService')
local bridge=modules:WaitForChild('LocalLibraryBridge')
local this={Enabled=account.Profile and account.Profile.user_id and account.Profile.user_id>0,PackageData={},OutfitData={}}
local profile,catalog;local items={};local outfits={};local version;local wearingRequest=false
local function request(op,body)return http:JSONDecode(bridge:Profile(op,body and http:JSONEncode(body)or''))end
local function copy(value)return http:JSONDecode(http:JSONEncode(value))end
local function update(value)
 profile=value;account.Profile=copy(value)
 require(modules.UserData):Initialize()
 for _,item in pairs(items)do item:IsWearing()end
 for _,outfit in pairs(outfits)do outfit:IsWearing()end
end
local function makeItem(asset)
 local item={IsWearingChanged=Utility.Signal(),OwnershipChanged=Utility.Signal()};local previous=false
 function item:GetAssetId()return asset.asset_id end
 function item:GetName()return asset.name end
 function item:GetFullName()return asset.name end
 function item:GetRobuxPrice()return nil end -- No commerce; these are actually owned local files.
 function item:IsOwned()return true end
 function item:IsWearing()
  local wearing=false
  if asset.kind=='body_package'then wearing=profile.appearance.body_package==asset.asset_id
  elseif asset.kind=='accessory'then for _,id in pairs(profile.appearance.accessories)do if id==asset.asset_id then wearing=true end end
  else wearing=profile.appearance.clothing[asset.kind]==asset.asset_id end
  if wearing~=previous then previous=wearing;self.IsWearingChanged:fire(wearing)end
  return wearing
 end
 function item:WearAsync()
  wearingRequest=true
  local ok,reason=pcall(function()
   local current=request('get');local appearance=copy(current.appearance)
   if asset.kind=='body_package'then appearance.body_package=asset.asset_id
   elseif asset.kind=='accessory'then local exists=false;for _,id in pairs(appearance.accessories)do if id==asset.asset_id then exists=true end end;if not exists then table.insert(appearance.accessories,asset.asset_id)end
   else appearance.clothing[asset.kind]=asset.asset_id end
   update(request('save',{profile_version=current.profile_version,appearance=appearance}))
  end)
  wearingRequest=false
  if not ok then warn('[AvatarCollection] Equip failed: '..tostring(reason));return{success=false}end
  EventHub:dispatchEvent(EventHub.Notifications.AvatarEquipBegin,asset.asset_id)
  EventHub:dispatchEvent(EventHub.Notifications.DonnedDifferentPackage,asset.asset_id)
  print('[AvatarCollection] Persisted '..asset.kind..' '..asset.asset_id)
  return{success=true}
 end
 return item
end
local function makeOutfit(value)
 local outfit={IsWearingChanged=Utility.Signal()};local previous=false
 function outfit:GetId()return value.outfit_id end
 function outfit:GetOutfitId()return value.outfit_id end
 function outfit:GetUserId()return profile.user_id end
 function outfit:GetName()return value.name end
 function outfit:IsOwned()return true end
 function outfit:IsWearing()
  local wearing=profile.equipped_outfit_id==value.outfit_id
  if wearing~=previous then previous=wearing;self.IsWearingChanged:fire(wearing)end
  return wearing
 end
 function outfit:WearAsync()
  local current=request('get')
  update(request('equip:'..value.outfit_id,{profile_version=current.profile_version}))
  EventHub:dispatchEvent(EventHub.Notifications.DonnedDifferentOutfit,value.outfit_id)
  print('[AvatarCollection] Persisted outfit selection')
  return{success=true}
 end
 return outfit
end
function this.Refresh()
 if not this.Enabled then return false end
 local ok,data=pcall(function() return {catalog=request('catalog'),current=request('get'),values=request('outfits').outfits} end)
 if not ok then
  profile=profile or account.Profile
  warn('[AvatarCollection] Account inventory/outfits unavailable: '..tostring(data)..'; shell and identity retained')
  return false
 end
 catalog=data.catalog;local current=data.current;local values=data.values;local key=tostring(current.profile_version)..http:JSONEncode(values);local changed=version~=key
 update(current)
 if changed or not version then
  items={};outfits={}
  -- Backend only lists supported, present bundled assets. Match its inventory, never invent stock.
  local owned={};for _,v in pairs(current.inventory or{})do owned[v.asset_id]=true end
  for _,asset in pairs(catalog.items)do if owned[asset.asset_id]then table.insert(items,makeItem(asset))end end
  for _,value in pairs(values)do table.insert(outfits,makeOutfit(value))end
 end
 version=key
 return changed
end
function this.PackageData:GetXboxCatalogPackagesAsync()if not profile then this.Refresh()end;return items end
function this.PackageData:GetCurrentlyWearingPackageAssetIdAsync()return profile and profile.appearance.body_package end
function this.PackageData:GetCachedWearingPackage()return profile and profile.appearance.body_package end
function this.PackageData:AwaitWearAssetRequest()while wearingRequest do wait()end end
function this.OutfitData:GetMyOutfitsAsync()if not profile then this.Refresh()end;return outfits end
function this.OutfitData:GetCachedWearingOutfitId()return profile and profile.equipped_outfit_id end
local fallback='rbxasset://textures/ui/Shell/Icons/DefaultProfileIcon.png'
function this.HasAsset(id)
 for _,item in pairs(items)do if item:GetAssetId()==tonumber(id)then return true end end
 return false
end
function this.Thumbnail(id,kind)
 -- Player/outfit PNG renderer is not implemented. Preserve the original loader's failure result.
 -- This shirt texture really exists locally; it is not a fabricated rendered avatar thumbnail.
 if kind=='asset' and tonumber(id)==900100001 then return{uri='rbxasset://offline-avatar/demo-shirt.png',available=true}end
 if kind=='player' then return{uri='',available=false}end
 return{uri=fallback,available=false}
end
function this.OpenEditor()
 local screen=require(modules.RecoveredAvatarEditor)(root)
 require(modules.ScreenManager):OpenScreen(screen)
end
if this.Enabled then this.Refresh()end
return this
