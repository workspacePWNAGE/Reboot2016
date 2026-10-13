local players = game:GetService("Players")
local me = players.LocalPlayer

while not me or not me.Character do
    task.wait(0.1)
    me = players.LocalPlayer
end

print("[LAN] Client ready: " .. me.Name .. " (" .. me.UserId .. ")")

local seen = {}
local jumped = {}
local floors = {}
local ready = tick()
local lastSample = 0
local animationRanges = {}
local animated = {}

local function checkAnimation(p)
    local character = p.Character
    local torso = character and character:FindFirstChild("Torso")
    local shoulder = torso and torso:FindFirstChild("Right Shoulder")
    local humanoid = character and character:FindFirstChild("Humanoid")
    
    if not shoulder or not humanoid then return end
    
    local range = animationRanges[p]
    if not range or range.character ~= character then
        range = {
            character = character,
            min = shoulder.CurrentAngle,
            max = shoulder.CurrentAngle,
            position = torso.Position,
            time = tick(),
            base = shoulder.C0
        }
        animationRanges[p] = range
        animated[p] = nil
    end

    -- Measure rendered joints while actually moving, rather than treating a
    -- Running event or root displacement as proof of walking animation.
    local root = character:FindFirstChild("HumanoidRootPart") or torso
    local now = tick()
    local delta = root.Position - range.position
    local speed = Vector3.new(delta.X, 0, delta.Z).magnitude / math.max(now - range.time, 0.001)
    
    range.position = root.Position
    range.time = now
    
    local state = humanoid:GetState()
    if speed > 3 and (state == Enum.HumanoidStateType.Running or state == Enum.HumanoidStateType.RunningNoPhysics) then
        local _, _, _, r00, _, _, r10 = (range.base:toObjectSpace(shoulder.C0)):components()
        local angle = shoulder.CurrentAngle + math.atan2(r10, r00)
        
        range.min = math.min(range.min, angle)
        range.max = math.max(range.max, angle)
        
        if range.max - range.min > 0.3 and not animated[p] then
            animated[p] = true
            print("[LAN] Walking joints animated: " .. p.Name .. " range=" .. (range.max - range.min))
        end
    end
end

players.ChildRemoved:Connect(function(p)
    if p:IsA("Player") then
        print("[LAN] Remote player removed: " .. p.Name)
        seen[p] = nil
        animationRanges[p] = nil
        animated[p] = nil
    end
end)

while true do
    for _, p in ipairs(players:GetPlayers()) do
        checkAnimation(p)
        if p ~= me and p.Character then
            local root = p.Character:FindFirstChild("HumanoidRootPart") or p.Character:FindFirstChild("Torso")
            local colors = p.Character:FindFirstChild("Body Colors")
            
            if root and colors and p:FindFirstChild("DisplayName") and p:FindFirstChild("AvatarName") then
                local humanoid = p.Character:FindFirstChild("Humanoid")
                
                if tick() - ready < 10 and tick() - lastSample > 0.2 then
                    local torso = p.Character:FindFirstChild("Torso")
                    local shoulder = torso and torso:FindFirstChild("Right Shoulder")
                    print("[LAN] Remote pose: " .. p.Name .. " " .. tostring(root.Position) .. " state=" .. tostring(humanoid:GetState()) .. " shoulder=" .. tostring(shoulder and shoulder.CurrentAngle) .. " velocity=" .. tostring(root.Velocity))
                    lastSample = tick()
                end
                
                floors[p] = math.min(floors[p] or root.Position.Y, root.Position.Y)
                if root.Position.Y - floors[p] > 2 and not jumped[p] then
                    jumped[p] = true
                    print("[LAN] Player jump motion replicated: " .. p.Name)
                end
                
                if not seen[p] then
                    print("[LAN] Remote player ready: " .. p.Name .. " (" .. p.UserId .. ") torso=" .. colors.TorsoColor.Number .. " display=" .. p.DisplayName.Value .. " avatar=" .. p.AvatarName.Value .. " shirt=" .. tostring(p.Character:FindFirstChild("LocalDemoShirt") ~= nil) .. " package=" .. tostring(p.Character:FindFirstChild("LocalDemoTorso") ~= nil) .. " hat=" .. tostring(p.Character:FindFirstChild("LocalDemoHat") ~= nil))
                    seen[p] = root.Position
                elseif Vector3.new(root.Position.X - seen[p].X, 0, root.Position.Z - seen[p].Z).magnitude > 4 then
                    print("[LAN] Player movement replicated: " .. p.Name)
                    seen[p] = root.Position
                end
            end
        end
    end
    task.wait(0.05)
end