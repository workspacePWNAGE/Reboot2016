local root=game:GetService('CoreGui'):WaitForChild('RobloxGui')
return function(overlay)
    local authored={}
    local objects={}
    local function collect(parent)
        for _,child in ipairs(parent:GetChildren()) do
            table.insert(objects,child);collect(child)
        end
    end
    collect(overlay.Container)
    table.insert(objects,overlay.Container)
    for _,object in ipairs(objects) do
        if object:IsA('GuiObject') then
            local font
            if object:IsA('TextLabel') or object:IsA('TextButton') or object:IsA('TextBox') then
                font=tonumber(tostring(object.FontSize):match('Size(%d+)'))
            end
            table.insert(authored,{object=object,size=object.Size,position=object.Position,font=font})
        end
    end
    local sizes={8,9,10,11,12,14,18,24,28,32,36,42,48,60,96}
    local function scaled(value,scale)
        return UDim2.new(value.X.Scale,value.X.Offset*scale,value.Y.Scale,value.Y.Offset*scale)
    end
    local function layout()
        local scale=math.min(root.AbsoluteSize.X/1920,root.AbsoluteSize.Y/1080)
        for _,entry in ipairs(authored) do
            entry.object.Size=scaled(entry.size,scale)
            entry.object.Position=scaled(entry.position,scale)
            if entry.font then
                local closest=8
                for _,size in ipairs(sizes) do
                    if math.abs(size-entry.font*scale)<math.abs(closest-entry.font*scale) then closest=size end
                end
                entry.object.FontSize=Enum.FontSize['Size'..closest]
            end
        end
    end
    layout()
    local connection=root.Changed:connect(function(property) if property=='AbsoluteSize' then layout() end end)
    local hide=overlay.Hide
    function overlay:Hide() connection:disconnect();hide(self) end
end
