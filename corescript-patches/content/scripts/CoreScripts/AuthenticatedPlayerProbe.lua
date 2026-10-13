-- Reads the actual replicated Player/character, not menu labels.
local core = game:GetService('CoreGui'):WaitForChild('RobloxGui')
local snapshot = require(core:WaitForChild('Modules'):WaitForChild('OfflineAccountProfile'))
if not snapshot.Enabled then return end
local profile = snapshot.Profile
local players = game:GetService('Players')
local deadline = tick() + 25
local player
repeat player = players.LocalPlayer; wait() until player or tick() > deadline
assert(player, 'Authenticated LocalPlayer missing')
assert(player.Name == profile.username, 'Player.Name differs from authenticated username')
assert(player.UserId == profile.user_id, 'Player.UserId differs from account ID')
local display = player:WaitForChild('DisplayName')
local avatar = player:WaitForChild('AvatarName')
assert(display.Value == profile.display_name and avatar.Value == profile.avatar_name, 'Replicated profile names mismatch')
local character
repeat character = player.Character; wait() until character or tick() > deadline
assert(character, 'Authenticated character missing')
local colors = character:WaitForChild('Body Colors')
local fields = {head='HeadColor', torso='TorsoColor', left_arm='LeftArmColor', right_arm='RightArmColor', left_leg='LeftLegColor', right_leg='RightLegColor'}
local parts = {head='Head', torso='Torso', left_arm='Left Arm', right_arm='Right Arm', left_leg='Left Leg', right_leg='Right Leg'}
for field, property in pairs(fields) do
    local part = character:WaitForChild(parts[field])
    repeat wait() until (colors[property].Number == profile.appearance.body_colors[field] and part.BrickColor.Number == profile.appearance.body_colors[field]) or tick() > deadline
    assert(colors[property].Number == profile.appearance.body_colors[field], 'BodyColors mismatch: '..field)
    assert(part.BrickColor.Number == profile.appearance.body_colors[field], 'Rendered part color mismatch: '..field)
end
local gui = Instance.new('TextLabel')
gui.Name = 'AuthenticatedPlayerStatus'
gui.Size = UDim2.new(0,620,0,28)
gui.Position = UDim2.new(0,12,1,-40)
gui.BackgroundTransparency = 0.4
gui.BackgroundColor3 = Color3.new(0,0,0)
gui.TextColor3 = Color3.new(1,1,1)
gui.Font = Enum.Font.SourceSans
gui.FontSize = Enum.FontSize.Size18
gui.Text = display.Value..' | '..player.Name..' | UserId '..player.UserId..' | Avatar: '..avatar.Value
gui.Active = false
gui.Parent = core
print('[Player] AUTHENTICATED PLAYER PASS: '..player.Name..' UserId '..player.UserId..'; character and all six body colors replicated')

-- Bundled integration fixtures are checked only when the saved profile selects them.
local function requireAsset(id, name)
    local selected = profile.appearance.body_package == id or profile.appearance.clothing.shirt == id
    for _, value in pairs(profile.appearance.accessories) do selected = selected or value == id end
    if selected then
        repeat wait() until character:FindFirstChild(name) or tick() > deadline
        assert(character:FindFirstChild(name), 'Bundled appearance missing: '..name)
    end
end
requireAsset(900100001, 'LocalDemoShirt')
requireAsset(900100002, 'LocalDemoTorso')
requireAsset(900100003, 'LocalDemoHat')
requireAsset(527365852, 'Gift13')
if character:FindFirstChild('Gift13') then
    assert(character.Gift13.Handle.Mesh.MeshId=='rbxasset://offline-avatar/cache/527245069','Imported hat mesh did not replicate')
    assert(character.Gift13.Handle.Mesh.TextureId=='rbxasset://offline-avatar/cache/527244862','Imported hat texture did not replicate')
    print('[ImportedHatTest] Gift13 gameplay character and mesh/texture replication PASS')
end
print('[Avatar] AVATAR COMPONENTS PASS: available saved-profile assets replicated; fallback retains body colors and identity')
