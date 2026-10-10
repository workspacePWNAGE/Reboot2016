-- Profile snapshot only: native code never exposes the bearer token.
local gui = game:GetService('CoreGui'):WaitForChild('RobloxGui')
local account = require(gui:WaitForChild('Modules'):WaitForChild('OfflineAccountProfile'))
if not account.Enabled then return end
local profile = account.Profile
local badge = Instance.new('TextLabel')
badge.Name = 'LocalAccountBadge'
badge.BackgroundColor3 = Color3.new(0,0,0)
badge.BackgroundTransparency = 0.4
badge.BorderSizePixel = 0
badge.Position = UDim2.new(0,12,1,-58)
badge.Size = UDim2.new(0,650,0,44)
badge.Font = Enum.Font.SourceSans
badge.FontSize = Enum.FontSize.Size18
badge.TextColor3 = Color3.new(1,1,1)
badge.TextXAlignment = Enum.TextXAlignment.Left
badge.Text = '  Local account: '..profile.username..'  |  ID: '..profile.user_id..'\n  Avatar name: '..profile.avatar_name
badge.Active = false
badge.Parent = gui
print('Local account profile attached to original Xbox menu; user ID '..profile.user_id)
