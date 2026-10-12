import json
import unicodedata
from pathlib import Path
from typing import Annotated

from pydantic import BaseModel, ConfigDict, Field, SecretStr, field_validator, model_validator

PALETTE = frozenset(json.loads(Path(__file__).with_name('brick_colors.json').read_text()))
AssetId = Annotated[int, Field(strict=True, gt=0, le=9007199254740991)]
Name = Annotated[str, Field(strict=True, min_length=1, max_length=64)]


class StrictModel(BaseModel):
    model_config = ConfigDict(extra='forbid', strict=True)


class GameLaunch(StrictModel):
    library_id: Annotated[str, Field(pattern=r'^[a-f0-9]{32}$')]
    game_id: Annotated[int, Field(gt=0, le=2147483647)]
    title: Annotated[str, Field(min_length=1, max_length=128)]
    launch_id: Annotated[str, Field(pattern=r'^[a-f0-9]{32}$')]

    @field_validator('title')
    @classmethod
    def clean_title(cls, value):
        if any(ord(c) < 32 for c in value):
            raise ValueError('invalid title')
        return value


class GamePreference(StrictModel):
    vote: Annotated[int, Field(ge=-1, le=1)] | None = None
    favourite: bool | None = None

    @model_validator(mode='after')
    def valid_operation(self):
        if len(self.model_fields_set) != 1:
            raise ValueError('provide exactly one preference')
        if 'vote' in self.model_fields_set and self.vote == 0:
            raise ValueError('vote must be -1, 1, or null')
        if 'favourite' in self.model_fields_set and self.favourite is None:
            raise ValueError('favourite must be a boolean')
        return self


class Credentials(StrictModel):
    username: Annotated[str, Field(min_length=3, max_length=32, pattern=r'^[A-Za-z0-9_]+$')]
    password: SecretStr

    @field_validator('password')
    @classmethod
    def password_length(cls, value):
        if not 12 <= len(value.get_secret_value()) <= 128:
            raise ValueError('password must contain 12 to 128 characters')
        return value


class BodyColors(StrictModel):
    head: int = 226
    torso: int = 28
    left_arm: int = 226
    right_arm: int = 226
    left_leg: int = 23
    right_leg: int = 23

    @field_validator('*')
    @classmethod
    def valid_brick_color(cls, value):
        if value not in PALETTE:
            raise ValueError('unknown 2016 BrickColor number')
        return value


class Clothing(StrictModel):
    shirt: AssetId | None = None
    pants: AssetId | None = None
    tshirt: AssetId | None = None


class Appearance(StrictModel):
    rig_type: Annotated[str, Field(pattern=r'^R6$')] = 'R6'
    body_colors: BodyColors = Field(default_factory=BodyColors)
    body_package: AssetId | None = None
    clothing: Clothing = Field(default_factory=Clothing)
    accessories: list[AssetId] = Field(default_factory=list, max_length=10)
    equipped_assets: list[AssetId] = Field(default_factory=list, max_length=32)

    @field_validator('accessories', 'equipped_assets')
    @classmethod
    def no_duplicates(cls, value):
        if len(value) != len(set(value)):
            raise ValueError('duplicate asset IDs')
        return value


class Settings(StrictModel):
    master_volume: Annotated[float, Field(ge=0, le=1)] = 1.0
    camera_sensitivity: Annotated[float, Field(ge=0.1, le=5)] = 1.0
    invert_camera_y: bool = False


class ProfilePatch(StrictModel):
    profile_version: Annotated[int, Field(ge=1)]
    display_name: Name | None = None
    avatar_name: Name | None = None
    appearance: Appearance | None = None
    settings: Settings | None = None

    @field_validator('display_name', 'avatar_name')
    @classmethod
    def safe_name(cls, value):
        if value is None:
            return value
        if value != value.strip() or any(unicodedata.category(c).startswith('C') for c in value):
            raise ValueError('name cannot contain surrounding whitespace or control characters')
        return value

    @model_validator(mode='after')
    def reject_explicit_null(self):
        for key in self.model_fields_set - {'profile_version'}:
            if getattr(self, key) is None:
                raise ValueError('profile fields cannot be null')
        return self


class OutfitCreate(StrictModel):
    name: Name
    appearance: Appearance

    @field_validator('name')
    @classmethod
    def clean(cls, value):
        return ProfilePatch.safe_name(value)


class OutfitRename(StrictModel):
    name: Name

    @field_validator('name')
    @classmethod
    def clean(cls, value):
        return ProfilePatch.safe_name(value)


class OutfitEquip(StrictModel):
    profile_version: Annotated[int, Field(ge=1)]
    default_on_login: bool = False
