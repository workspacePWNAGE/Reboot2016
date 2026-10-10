local adapter=require(script.Parent.OfflineR6JointAdapter)
return function(character)
local script={Parent=character}
local wrapJoint=adapter(character)


 local function waitForChild(parent, childName)
	local child = parent:findFirstChild(childName)
	if child then return child end
	while true do
		child = parent.ChildAdded:wait()
		if child.Name==childName then return child end
	end
end

-- ANIMATION

-- declarations

local Figure = script.Parent
local Torso = waitForChild(Figure, "Torso")
local RightShoulder = wrapJoint(waitForChild(Torso, "Right Shoulder"))
local LeftShoulder = wrapJoint(waitForChild(Torso, "Left Shoulder"))
local RightHip = wrapJoint(waitForChild(Torso, "Right Hip"))
local LeftHip = wrapJoint(waitForChild(Torso, "Left Hip"))
local Neck = waitForChild(Torso, "Neck")
local Humanoid = waitForChild(Figure, "Humanoid")
local pose = "Standing"

local toolAnim = "None"
local toolAnimTime = 0

local jumpMaxLimbVelocity = 0.75

-- functions

local function onRunning(speed)
	if speed>0 then
		pose = "Running"
	else
		pose = "Standing"
	end
end

local function onDied()
	pose = "Dead"
end

local function onJumping()
	pose = "Jumping"
end

local function onClimbing()
	pose = "Climbing"
end

local function onGettingUp()
	pose = "GettingUp"
end

local function onFreeFall()
	pose = "FreeFall"
end

local function onFallingDown()
	pose = "FallingDown"
end

local function onSeated()
	pose = "Seated"
end

local function onPlatformStanding()
	pose = "PlatformStanding"
end

local function onSwimming(speed)
	if speed>0 then
		pose = "Running"
	else
		pose = "Standing"
	end
end

local function moveJump()
	RightShoulder.MaxVelocity = jumpMaxLimbVelocity
	LeftShoulder.MaxVelocity = jumpMaxLimbVelocity
  RightShoulder:SetDesiredAngle(3.14)
	LeftShoulder:SetDesiredAngle(-3.14)
	RightHip:SetDesiredAngle(0)
	LeftHip:SetDesiredAngle(0)
end


-- same as jump for now

local function moveFreeFall()
	RightShoulder.MaxVelocity = jumpMaxLimbVelocity
	LeftShoulder.MaxVelocity = jumpMaxLimbVelocity
	RightShoulder:SetDesiredAngle(3.14)
	LeftShoulder:SetDesiredAngle(-3.14)
	RightHip:SetDesiredAngle(0)
	LeftHip:SetDesiredAngle(0)
end

local function moveSit()
	RightShoulder.MaxVelocity = 0.15
	LeftShoulder.MaxVelocity = 0.15
	RightShoulder:SetDesiredAngle(3.14 /2)
	LeftShoulder:SetDesiredAngle(-3.14 /2)
	RightHip:SetDesiredAngle(3.14 /2)
	LeftHip:SetDesiredAngle(-3.14 /2)
end

local function getTool()	
	for _, kid in ipairs(Figure:GetChildren()) do
		if kid.className == "Tool" then return kid end
	end
	return nil
end

local function getToolAnim(tool)
	for _, c in ipairs(tool:GetChildren()) do
		if c.Name == "toolanim" and c.className == "StringValue" then
			return c
		end
	end
	return nil
end

local function animateTool()
	
	if (toolAnim == "None") then
		RightShoulder:SetDesiredAngle(1.57)
		return
	end

	if (toolAnim == "Slash") then
		RightShoulder.MaxVelocity = 0.5
		RightShoulder:SetDesiredAngle(0)
		return
	end

	if (toolAnim == "Lunge") then
		RightShoulder.MaxVelocity = 0.5
		LeftShoulder.MaxVelocity = 0.5
		RightHip.MaxVelocity = 0.5
		LeftHip.MaxVelocity = 0.5
		RightShoulder:SetDesiredAngle(1.57)
		LeftShoulder:SetDesiredAngle(1.0)
		RightHip:SetDesiredAngle(1.57)
		LeftHip:SetDesiredAngle(1.0)
		return
	end
end

local function move(time)
	local amplitude
	local frequency
  
	if (pose == "Jumping") then
		moveJump()
		return
	end

	if (pose == "FreeFall") then
		moveFreeFall()
		return
	end
 
	if (pose == "Seated") then
		moveSit()
		return
	end

	local climbFudge = 0
	
	if (pose == "Running") then
    if (RightShoulder.CurrentAngle > 1.5 or RightShoulder.CurrentAngle < -1.5) then
			RightShoulder.MaxVelocity = jumpMaxLimbVelocity
		else			
			RightShoulder.MaxVelocity = 0.15
		end
		if (LeftShoulder.CurrentAngle > 1.5 or LeftShoulder.CurrentAngle < -1.5) then
			LeftShoulder.MaxVelocity = jumpMaxLimbVelocity
		else			
			LeftShoulder.MaxVelocity = 0.15
		end
		amplitude = 1
		frequency = 9
	elseif (pose == "Climbing") then
		RightShoulder.MaxVelocity = 0.5 
		LeftShoulder.MaxVelocity = 0.5
		amplitude = 1
		frequency = 9
		climbFudge = 3.14
	else
		amplitude = 0.1
		frequency = 1
	end

	desiredAngle = amplitude * math.sin(time*frequency)

	RightShoulder:SetDesiredAngle(desiredAngle + climbFudge)
	LeftShoulder:SetDesiredAngle(desiredAngle - climbFudge)
	RightHip:SetDesiredAngle(-desiredAngle)
	LeftHip:SetDesiredAngle(-desiredAngle)


	local tool = getTool()

	if tool then
	
		animStringValueObject = getToolAnim(tool)

		if animStringValueObject then
			toolAnim = animStringValueObject.Value
			-- message recieved, delete StringValue
			animStringValueObject.Parent = nil
			toolAnimTime = time + .3
		end

		if time > toolAnimTime then
			toolAnimTime = 0
			toolAnim = "None"
		end

		animateTool()

		
	else
		toolAnim = "None"
		toolAnimTime = 0
	end
end


-- connect events

Humanoid.Died:connect(onDied)
Humanoid.Running:connect(onRunning)
Humanoid.Jumping:connect(onJumping)
Humanoid.Climbing:connect(onClimbing)
Humanoid.GettingUp:connect(onGettingUp)
Humanoid.FreeFalling:connect(onFreeFall)
Humanoid.FallingDown:connect(onFallingDown)
Humanoid.Seated:connect(onSeated)
Humanoid.PlatformStanding:connect(onPlatformStanding)
Humanoid.Swimming:connect(onSwimming)
-- main program

local runService = game:service("RunService");

while Figure.Parent~=nil do
	local _, time = wait(0.1)
local state=Humanoid:GetState()
if state==Enum.HumanoidStateType.Running or state==Enum.HumanoidStateType.RunningNoPhysics or state==Enum.HumanoidStateType.Landed then
    local velocity=Torso.Velocity
    onRunning(Vector3.new(velocity.X,0,velocity.Z).magnitude)
elseif state==Enum.HumanoidStateType.Jumping then onJumping()
elseif state==Enum.HumanoidStateType.Freefall then onFreeFall()
elseif state==Enum.HumanoidStateType.Climbing then onClimbing()
elseif state==Enum.HumanoidStateType.Seated then onSeated()
end
move(time)
end

end
