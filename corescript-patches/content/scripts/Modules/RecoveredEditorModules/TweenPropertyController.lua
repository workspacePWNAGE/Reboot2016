local module = {}

local runService = game:GetService("RunService")
local min = math.min

function swait(a)
	if a and a>.0333 then
		wait(a)
	else
		runService.RenderStepped:wait()
	end
end

local tweens = {}
function tweenProperty(thing, property, start, desired, tweenTime, style, easeDirection)
	local thisTween = 0
	local startTime = tick()
	local start = start or thing[property]
	local tweenTime = tweenTime or 2
	local difference = desired - start
	local style = style or function(t) return t end

	if not tweens[thing] then
		tweens[thing] = {}
	end
	if tweens[thing][property] then
		thisTween = tweens[thing][property] + 1
	end
	tweens[thing][property] = thisTween

	while thing and tweens[thing] and tweens[thing][property] == thisTween do
		local percent = min(1,(tick()-startTime)/tweenTime)
		local t = easeDirection and easeDirection(percent,style) or style(percent)
		thing[property] = start + difference*t
		if percent == 1 then
			thing[property] = desired
			tweens[thing][property] = nil
			local stillTweens = false
			for _,v in pairs(tweens[thing]) do
				stillTweens = true
				break
			end
			if not stillTweens then
				tweens[thing] = nil
			end
			return true
		end
		swait()
	end

	return false
end

module.tweens = tweens
module.tweenProperty = tweenProperty


return module
