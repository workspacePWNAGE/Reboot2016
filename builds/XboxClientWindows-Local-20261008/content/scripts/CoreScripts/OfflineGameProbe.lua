local player=game:GetService('Players').LocalPlayer
while not player do wait(); player=game:GetService('Players').LocalPlayer end
local scripts=player:WaitForChild('PlayerScripts')
local controls=scripts:WaitForChild('ControlScript')
local camera=scripts:WaitForChild('CameraScript')
print('Local gameplay: original ControlScript and CameraScript present')
local character=player.Character or player.CharacterAdded:wait()
local root=character:WaitForChild('HumanoidRootPart')
local humanoid=character:WaitForChild('Humanoid')
local animate=character:WaitForChild('Animate')
local shoulder=character:WaitForChild('Torso'):WaitForChild('Right Shoulder')
local ok,source=pcall(function() return animate.Source end)
print('Local animation: Animate present, disabled',animate.Disabled,'source length',ok and #source or 'unreadable')
humanoid.Jumping:connect(function(active) print('Local animation: Jumping',active,'shoulder',shoulder.CurrentAngle) end)
humanoid.Running:connect(function(speed) if speed>1 then print('Local animation: Running',speed,'shoulder',shoulder.CurrentAngle) end end)
wait(3)
local start=root.Position
for i=1,35 do
    wait(0.25)
    print('Local animation sample:',i,'position',root.Position,'shoulder',shoulder.CurrentAngle,'desired',shoulder.DesiredAngle)
end
local delta=root.Position-start
print('Local gameplay: settled horizontal displacement',Vector3.new(delta.X,0,delta.Z).magnitude)
