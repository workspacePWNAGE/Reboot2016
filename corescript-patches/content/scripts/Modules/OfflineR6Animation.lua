local adapter = require(script.Parent.OfflineR6JointAdapter)
local animationData = require(script.Parent.OfflineR6AnimationData)

return function(character)
    local script = { Parent = character }
    local wrapJoint = adapter(character)

    local function waitForChild(parent, childName)
        local child = parent:FindFirstChild(childName)
        if child then return child end
        while true do
            child = parent.ChildAdded:Wait()
            if child.Name == childName then return child end
        end
    end

    -- anim declarations
    local Figure = script.Parent
    local Torso = waitForChild(Figure, "Torso")
    local RightShoulder = wrapJoint(waitForChild(Torso, "Right Shoulder"))
    local LeftShoulder = wrapJoint(waitForChild(Torso, "Left Shoulder"))
    local RightHip = wrapJoint(waitForChild(Torso, "Right Hip"))
    local LeftHip = wrapJoint(waitForChild(Torso, "Left Hip"))
    local Neck = waitForChild(Torso, "Neck")
    local Humanoid = waitForChild(Figure, "Humanoid")
    
    local pose = "idle1"
    local toolAnim = "None"
    local toolAnimTime = 0
    local jumpMaxLimbVelocity = 0.75

    -- idle cycling tracking
    local idleTimer = 0
    local currentIdleStyle = "idle1"

    -- registry; do not edit
    local animationTracks = {}
    local currentTrack = nil
    local trackTime = 0
    local isLooping = true

    local function registerAnimationData(animationName, sequenceData)
        animationTracks[animationName] = sequenceData
    end

    for animationName, sequenceData in pairs(animationData) do
        registerAnimationData(animationName, sequenceData)
    end

    -- pose cframe -> joint angle
    local function poseAngle(cf)
        local _, _, z = cf:ToEulerAnglesYXZ()
        return z
    end

    local function playAnimation(animationName)
        if currentTrack ~= animationTracks[animationName] then
            currentTrack = animationTracks[animationName]
            trackTime = 0
            isLooping = currentTrack == nil or currentTrack.Loop ~= false
        end
    end

    -- event handler
    local function onRunning(speed)
        if speed > 0 then
            pose = "walk"
            playAnimation("walk")
        else
            pose = currentIdleStyle
            playAnimation(currentIdleStyle)
        end
    end

    local function onDied() pose = "Dead"; currentTrack = nil end
    local function onJumping() pose = "jump"; playAnimation("jump") end
    local function onClimbing() pose = "climb"; playAnimation("climb") end
    local function onGettingUp() pose = "GettingUp"; currentTrack = nil end
    local function onFreeFall() pose = "fall"; playAnimation("fall") end
    local function onFallingDown() pose = "FallingDown"; currentTrack = nil end
    local function onSeated() pose = "Seated"; currentTrack = nil end
    local function onPlatformStanding() pose = "PlatformStanding"; currentTrack = nil end
    
    local function onSwimming(speed)
        if speed > 0 then
            pose = "walk"
            playAnimation("walk")
        else
            pose = "idle1"
            playAnimation("idle1")
        end
    end

    -- anim player
    local function updateKeyframeAnimation(timeDelta)
        if not currentTrack or not currentTrack.Keyframes or #currentTrack.Keyframes == 0 then
            return false -- fallback to retro gubby anims
        end

        trackTime = trackTime + timeDelta
        local keyframes = currentTrack.Keyframes
        local maxTime = keyframes[#keyframes].Time

        if trackTime > maxTime then
            if isLooping then
                trackTime = trackTime % maxTime
            else
                trackTime = maxTime
            end
        end

        -- find surrounding keyframes for interpolation
        local key1, key2
        for i = 1, #keyframes - 1 do
            if trackTime >= keyframes[i].Time and trackTime <= keyframes[i+1].Time then
                key1 = keyframes[i]
                key2 = keyframes[i+1]
                break
            end
        end

        if not key1 then
            key1 = keyframes[#keyframes - 1]
            key2 = keyframes[#keyframes]
        end

        local alpha = (trackTime - key1.Time) / math.max(key2.Time - key1.Time, 0.001)
        alpha = math.clamp(alpha, 0, 1)

        -- apply pose cframes to joints based on pose names
        for limbName, cf2 in pairs(key2.Poses) do
            local cf1 = key1.Poses[limbName] or cf2
            local interpolatedCF = cf1:Lerp(cf2, alpha)

            if limbName == "Right Arm" then
                RightShoulder:SetDesiredAngle(poseAngle(interpolatedCF))
            elseif limbName == "Left Arm" then
                LeftShoulder:SetDesiredAngle(poseAngle(interpolatedCF))
            elseif limbName == "Right Leg" then
                RightHip:SetDesiredAngle(poseAngle(interpolatedCF))
            elseif limbName == "Left Leg" then
                LeftHip:SetDesiredAngle(poseAngle(interpolatedCF))
            end
        end

        return true
    end

    -- fallback
    local function proceduralMove(timeDelta, absoluteTime)
        local amplitude = 0
        local frequency = 0
        local climbFudge = 0

        if pose == "idle1" or pose == "idle2" then
            idleTimer = idleTimer + timeDelta
            if idleTimer > 6 then
                currentIdleStyle = (currentIdleStyle == "idle1") and "idle2" or "idle1"
                idleTimer = 0
                pose = currentIdleStyle
            end
        else
            idleTimer = 0
        end

        if pose == "jump" then
            RightShoulder.MaxVelocity = jumpMaxLimbVelocity
            LeftShoulder.MaxVelocity = jumpMaxLimbVelocity
            RightShoulder:SetDesiredAngle(3.14)
            LeftShoulder:SetDesiredAngle(-3.14)
            RightHip:SetDesiredAngle(0)
            LeftHip:SetDesiredAngle(0)
            return
        elseif pose == "fall" then
            RightShoulder.MaxVelocity = jumpMaxLimbVelocity
            LeftShoulder.MaxVelocity = jumpMaxLimbVelocity
            RightShoulder:SetDesiredAngle(2.5)
            LeftShoulder:SetDesiredAngle(-2.5)
            RightHip:SetDesiredAngle(0.5)
            LeftHip:SetDesiredAngle(-0.5)
            return
        elseif pose == "Seated" then
            RightShoulder.MaxVelocity = 0.15
            LeftShoulder.MaxVelocity = 0.15
            RightShoulder:SetDesiredAngle(3.14 / 2)
            LeftShoulder:SetDesiredAngle(-3.14 / 2)
            RightHip:SetDesiredAngle(3.14 / 2)
            LeftHip:SetDesiredAngle(-3.14 / 2)
            return
        end

        if pose == "walk" then
            amplitude = 1
            frequency = 9
        elseif pose == "climb" then
            amplitude = 1
            frequency = 9
            climbFudge = 3.14
        elseif pose == "idle2" then
            amplitude = 0.05
            frequency = 2
        else
            amplitude = 0.1
            frequency = 1
        end

        local desiredAngle = amplitude * math.sin(absoluteTime * frequency)
        RightShoulder:SetDesiredAngle(desiredAngle + climbFudge)
        LeftShoulder:SetDesiredAngle(desiredAngle - climbFudge)
        RightHip:SetDesiredAngle(-desiredAngle)
        LeftHip:SetDesiredAngle(-desiredAngle)
    end

    -- anim loop
    local function move(timeDelta, absoluteTime)
        local playedFromTrack = updateKeyframeAnimation(timeDelta)
        if not playedFromTrack then
            proceduralMove(timeDelta, absoluteTime)
        end
    end

    -- connections
    Humanoid.Died:Connect(onDied)
    Humanoid.Running:Connect(onRunning)
    Humanoid.Jumping:Connect(onJumping)
    Humanoid.Climbing:Connect(onClimbing)
    Humanoid.GettingUp:Connect(onGettingUp)
    Humanoid.FreeFalling:Connect(onFreeFall)
    Humanoid.FallingDown:Connect(onFallingDown)
    Humanoid.Seated:Connect(onSeated)
    Humanoid.PlatformStanding:Connect(onPlatformStanding)
    Humanoid.Swimming:Connect(onSwimming)

    -- loop
    local lastTime = tick()
    while Figure.Parent ~= nil do
        local _, timeDelta = task.wait(0.1)
        local now = tick()
        local actualDelta = now - lastTime
        lastTime = now

        local state = Humanoid:GetState()
        if state == Enum.HumanoidStateType.Running or state == Enum.HumanoidStateType.RunningNoPhysics or state == Enum.HumanoidStateType.Landed then
            local velocity = Torso.Velocity
            onRunning(Vector3.new(velocity.X, 0, velocity.Z).magnitude)
        elseif state == Enum.HumanoidStateType.Jumping then
            onJumping()
        elseif state == Enum.HumanoidStateType.Freefall then
            onFreeFall()
        elseif state == Enum.HumanoidStateType.Climbing then
            onClimbing()
        elseif state == Enum.HumanoidStateType.Seated then
            onSeated()
        end

        move(actualDelta, now)
    end
end