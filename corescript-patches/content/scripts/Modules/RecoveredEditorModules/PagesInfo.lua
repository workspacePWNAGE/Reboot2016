--


local armsCFrame = CFrame.new(13.0793343, 4.29243708, -25.925909, -0.927571893, 0.164386064, -0.335540771, -0, 0.898020923, 0.439952821, 0.373644739, 0.408087879, -0.832978964)

local pages = {
	{name = 'Recent',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-history.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-history-white.png',
	},
	{name = 'Outfits',			--outfits will include packages in some way
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-bundle.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-bundle-white.png',
		--special = true,
		infiniteScrolling = true,
	},
	{name = 'Hats',
		typeName = 'Hat',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-hat.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-hat-white.png',
		infiniteScrolling = true,
	},
	{name = 'Shirts',
		typeName = 'Shirt',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-tshirt.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-tshirt-white.png',
		infiniteScrolling = true,
	},
	{name = 'Pants',
		typeName = 'Pants',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-pant.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-pant-white.png',
		infiniteScrolling = true,
	},
	{name = 'Faces',
		typeName = 'Face',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-face.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-face-white.png',
		infiniteScrolling = true,
	},
	{name = 'Heads',
		typeName = 'Head',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-head.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-head-white.png',
		infiniteScrolling = true,
	},
	{name = 'Torsos',
		typeName = 'Torso',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-torso.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-torso-white.png',
		infiniteScrolling = true,
	},
	{name = 'Right Arms',
		typeName = 'RightArm',
		cframe = armsCFrame,
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-rightarm.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-rightarm-white.png',
		infiniteScrolling = true,
	},
	{name = 'Left Arms',
		typeName = 'LeftArm',
		cframe = armsCFrame,
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-leftarm.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-leftarm-white.png',
		infiniteScrolling = true,
	},
	{name = 'Right Legs',
		typeName = 'RightLeg',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-rightleg.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-rightleg-white.png',
		infiniteScrolling = true,
	},
	{name = 'Left Legs',
		typeName = 'LeftLeg',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-leftleg.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-leftleg-white.png',
		infiniteScrolling = true,
	},
	{name = 'Gear',
		typeName = 'Gear',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-gear.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-gear-white.png',
		infiniteScrolling = true,
	},
	{name = 'Skin Tone',
		iconImage = 'rbxasset://textures/AvatarEditorIcons/ic-color.png',
		iconImageSelected = 'rbxasset://textures/AvatarEditorIcons/ic-color-filled.png',
		special = true,
	},
}

return pages











