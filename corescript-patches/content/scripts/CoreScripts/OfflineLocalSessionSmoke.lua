wait(5)
local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
require(root.Modules.OfflineMenuPreview).Begin()
wait(3)
local function findTile(parent)
    if parent.Name == 'LocalTestGameButton' then return parent end
    for _,child in pairs(parent:GetChildren()) do local result=findTile(child); if result then return result end end
end
local tile=findTile(root)
assert(tile,'Local gameplay tile missing')
game:GetService('GuiService').SelectedCoreObject=tile
print('Local session smoke: selected real menu test tile')
