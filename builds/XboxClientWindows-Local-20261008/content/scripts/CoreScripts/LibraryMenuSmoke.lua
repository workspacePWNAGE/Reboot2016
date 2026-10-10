wait(5)
local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
local modules=root:WaitForChild('Modules')
-- OfflineAccountMenuSmoke owns the one menu entry for this test.
local settings=require(modules.LibrarySmokeSettings)
local id=settings.GameId
local function find(parent,name)
    if parent.Name==name and parent:IsA('GuiObject') then return parent end
    for _,child in pairs(parent:GetChildren()) do local item=find(child,name);if item then return item end end
end
local deadline=tick()+25
local card
repeat
    wait(.1)
    local pane=find(root,settings.Recent and 'HomePane' or 'GamePane')
    if pane and pane.Visible then
        if settings.Recent then
            local function recentCard(parent)
                if parent.Name=='SortTitle' and parent:IsA('TextLabel') and string.find(string.upper(parent.Text),'RECENT') then return find(parent.Parent,'LocalGame_'..id) end
                for _,child in pairs(parent:GetChildren()) do local result=recentCard(child);if result then return result end end
            end
            card=recentCard(pane)
        else card=find(pane,'LocalGame_'..id) end
    end
until card or tick()>deadline
assert(card,'Selected place missing from actual Games tab')
-- The pane asynchronously replaces its grid and sets its default selection.
wait(2)
local visiblePane=assert(find(root,settings.Recent and 'HomePane' or 'GamePane'))
if settings.Recent then
    local function recent(parent)
        if parent.Name=='SortTitle' and parent:IsA('TextLabel') and string.find(string.upper(parent.Text),'RECENT') then return find(parent.Parent,'LocalGame_'..id) end
        for _,child in pairs(parent:GetChildren()) do local item=recent(child);if item then return item end end
    end
    card=assert(recent(visiblePane),'Recent card disappeared during refresh')
else card=assert(find(visiblePane,'LocalGame_'..id),'Selected card disappeared during refresh') end
game:GetService('GuiService').SelectedCoreObject=card
wait(.25)
-- AppHub focuses the pane on its first selection and chooses a default card.
-- Select the requested card once that original focus transition has finished.
game:GetService('GuiService').SelectedCoreObject=card
wait(.25)
assert(game:GetService('GuiService').SelectedCoreObject==card,'Selected game focus did not settle')
print('[LibraryTest] Selected Games-tab card '..id)
if settings.Recent then print('[LibraryTest] Selected persisted Recent Games card '..id) end
spawn(function()
    local limit=tick()+20
    local page
    repeat wait(.1);page=find(root,'LocalGameDetail_'..id) until page or tick()>limit
    assert(page,'Original details page did not open from card A')
    local library=require(modules.LocalGameLibrary)
    local item=assert(library.Find(id))
    repeat wait(.1) until find(page,'ThumbsUpButton') and find(page,'ThumbsDownButton') or tick()>limit
    local play=assert(find(page,'PlayButton'))
    local favourite=assert(find(page,'FavoriteButton'))
    local up=assert(find(page,'ThumbsUpButton'))
    local down=assert(find(page,'ThumbsDownButton'))
    assert(play.NextSelectionRight==favourite,'Original Play/Favourite focus link missing')
    wait(3)
    local data=library.GameData(id)
    local initial=data:GetIsFavoritedByUser()
    print('[DetailsTest] Page ready '..id..' '..item.title)
    print('[DetailsTest] Description: '..data:GetDescription())
    local function press(key)
        print('[DetailsTest] Input '..key);wait(1)
    end
    game:GetService('GuiService').SelectedCoreObject=play
    press('Right')
    assert(game:GetService('GuiService').SelectedCoreObject==favourite,'Right did not navigate Play to Favourite')
    press('A')
    local saved=library.GameData(id)
    assert(saved:GetIsFavoritedByUser()~=initial,'Native controller Favourite did not persist')
    assert(favourite.FavoriteText.TextFits,'Favourite label does not fit its original button')
    press('Up')
    assert(game:GetService('GuiService').SelectedCoreObject==up,'Up did not navigate Favourite to rating')
    press('A')
    local votes=library.GameData(id):GetVoteDataAsync()
    assert(votes.UserVote==true or votes.UserVote=='null','Native up-vote failed')
    press('Right')
    assert(game:GetService('GuiService').SelectedCoreObject==down,'Right did not navigate between rating controls')
    press('A')
    assert(library.GameData(id):GetVoteDataAsync().UserVote==false,'Native down-vote failed')
    press('B')
    wait(3)
    assert(not find(root,'LocalGameDetail_'..id),'B did not close details page')
    local restored=game:GetService('GuiService').SelectedCoreObject
    assert(restored and restored.Name=='LocalGame_'..id,'B did not restore selected game focus')
    press('A')
    page=assert(find(root,'LocalGameDetail_'..id))
    wait(2)
    game:GetService('GuiService').SelectedCoreObject=assert(find(page,'PlayButton'))
    print('[DetailsTest] Controller Favourite/votes/Back PASS '..id)
    if item.description=='Missing-game acceptance fixture' then
        local entered=false
        game:GetService('PlatformService').ViewChanged:connect(function(view) if view==1 then entered=true end end)
        print('[DetailsTest] Delete fixture now')
        wait(4)
        press('A')
        wait(4)
        assert(not entered,'Deleted game unexpectedly entered gameplay')
        print('[DetailsTest] Missing game safely rejected PASS')
        return
    end
    press('A')
end)
local entered=false
game:GetService('PlatformService').ViewChanged:connect(function(view)
    if view==1 then entered=true end
    if view==0 and entered then
        entered=false
        spawn(function()
            wait(1)
            if find(root,'LocalGameDetail_'..id) then print('[DetailsTest] Input B');wait(1) end
            require(modules.LocalGameLibrary).Refresh()
            local recent=require(modules.LocalGameLibrary).Recent
            assert(recent[1] and recent[1].game_id==id,'Successful launch absent from Recent Games')
            print('[LibraryTest] Recent launch recorded '..id)
            local limit=tick()+15
            repeat
                wait(.2)
                local home=find(root,'HomePane')
                -- The original Home pane renders a separate Recent grid.
                if home and home.Visible then
                    local function hasRecent(parent)
                        if parent.Name=='SortTitle' and string.find(string.upper(parent.Text),'RECENT') and find(parent.Parent,'LocalGame_'..id) then return true end
                        for _,child in pairs(parent:GetChildren()) do if hasRecent(child) then return true end end
                        return false
                    end
                    if hasRecent(home) then print('[LibraryTest] Home Recent Games visible '..id);return end
                end
            until tick()>limit
            error('Recent Games did not render on the original Home pane')
        end)
    end
end)
