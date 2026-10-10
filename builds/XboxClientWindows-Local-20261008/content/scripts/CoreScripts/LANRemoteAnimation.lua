-- The offline R6 script drives local Motor angles. Other players' LocalScripts
-- do not run here; reuse the same bundled animation against their replicated
-- Humanoid state on this client. Identity, movement and physics remain native.
local players=game:GetService('Players')
local modules=game:GetService('CoreGui'):WaitForChild('RobloxGui'):WaitForChild('Modules')
local animate=require(modules:WaitForChild('OfflineR6Animation'))
local observed={}
local function observe(player)
    if not player:IsA('Player') or player==players.LocalPlayer or observed[player] then return end
    observed[player]=true
    local function characterAdded(character)
        spawn(function()
            print('[LAN] Starting remote offline R6 animation: '..player.Name)
            local ok,reason=pcall(function() animate(character) end)
            if not ok and character.Parent then
                warn('[LAN] Remote animation failed for '..player.Name..': '..tostring(reason))
            end
        end)
    end
    player.CharacterAdded:connect(characterAdded)
    if player.Character then characterAdded(player.Character) end
end
-- PlayerAdded is disabled on clients by this revision's historical flag.
players.ChildAdded:connect(observe)
for _,player in pairs(players:GetPlayers()) do observe(player) end
players.ChildRemoved:connect(function(player) observed[player]=nil end)
