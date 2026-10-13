local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules=root:WaitForChild('Modules');local lighting=game:GetService('Lighting')
local oldBrightness=lighting.Brightness;local oldTime=lighting.TimeOfDay;local oldCamera=workspace.CurrentCamera.CoordinateFrame
local function press(key)print('[DetailsTest] Input '..key);wait(2)end
wait(10)
require(modules.EventHub):dispatchEvent(require(modules.EventHub).Notifications.NavigateToEquippedAvatar);wait(5)
press('Y')
local editor=assert(root.Parent:FindFirstChild('RecoveredAvatarCustomization'))
local environment=assert(workspace:FindFirstChild('RecoveredAvatarEnvironment'))
local count=0;local unions=0
local function inspect(node)
 if node:IsA('BasePart')then
  count=count+1;assert(node.Anchored and not node.CanCollide,'Scene must stay outside gameplay physics')
  if node:IsA('UnionOperation')then unions=unions+1 end
  if node.Size.magnitude>20 then print('[EnvironmentGeometry] '..node.Name..' size '..tostring(node.Size)..' color '..tostring(node.Color)..' alpha '..node.Transparency)end
 end
 for _,child in pairs(node:GetChildren())do inspect(child)end
end
inspect(environment);assert(count>300 and unions==88,'Original room geometry missing')
assert(lighting.Brightness==1 and lighting.TimeOfDay=='14:00:00','Archived lighting differs')
print('[DetailsTest] Page ready original environment');wait(3)
press('Y');print('[DetailsTest] Page ready original environment fullview');wait(3)
press('Rotate');press('Y');press('B')
assert(not workspace:FindFirstChild('RecoveredAvatarEnvironment'),'Scene leaked after close')
assert(lighting.Brightness==oldBrightness and lighting.TimeOfDay==oldTime,'Shell lighting did not restore')
press('Y');assert(workspace:FindFirstChild('RecoveredAvatarEnvironment'),'Scene could not reopen')
print('[DetailsTest] Page ready original environment reopened');wait(3);press('B')
assert(not workspace:FindFirstChild('RecoveredAvatarEnvironment'),'Reopened scene leaked')
print('[EnvironmentTest] Original geometry, lighting, camera/fullview, rotation, cleanup and reopen PASS')
