-- Remote assemblies receive native physics poses, which overwrite their local
-- Motor angle commands. Present the original R6 animator with its same four
-- joint controls, applying the visual offsets locally through Motor6D.C0.
return function(character)
    local joints={}
    local connection
    connection=game:GetService('RunService').RenderStepped:connect(function(dt)
        if not character.Parent then connection:disconnect();return end
        for _,joint in pairs(joints) do
            local delta=joint.desired-joint.CurrentAngle
            local step=math.abs(joint.MaxVelocity)*dt*60
            joint.CurrentAngle=joint.CurrentAngle+math.max(-step,math.min(step,delta))
            joint.motor.C0=joint.base*CFrame.Angles(0,0,joint.CurrentAngle)
        end
    end)
    return function(motor)
        local joint={motor=motor,base=motor.C0,CurrentAngle=0,MaxVelocity=motor.MaxVelocity,desired=0}
        function joint:SetDesiredAngle(angle) self.desired=angle end
        joints[#joints+1]=joint
        return joint
    end
end
