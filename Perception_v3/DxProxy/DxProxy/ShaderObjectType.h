/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

File <D3D9ProxyVertexShader.h> and
Class <D3D9ProxyVertexShader> :
Copyright (C) 2013 Chris Drain

Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

#ifndef SHADEROBJECTTYPE_H_INCLUDED
#define SHADEROBJECTTYPE_H_INCLUDED

#include <string>

enum ShaderObjectType
{
	ShaderObjectTypeUnknown, //The default, it is undefined and doesn't matter
	ShaderObjectTypeDoNotDraw, //Specifically always avoid drawing this shader
	ShaderObjectTypeReticule, //Allow user to prevent drawing of aiming reticule for games that support it
	ShaderObjectTypePlayer,  //This is used for players arms body etc, so we can use a different projection FOV
	ShaderObjectTypeSky, // For any shader that is part of the skybox, including clouds, sun, moon etc
	ShaderObjectTypeShadows, // Shadows, so user can elect to turn shadows on or off themselves (they might have a mod that makes shadows look ok)
	ShaderObjectTypeFog, // Fog, so issues with fog or other particles can be turned off
	ShaderObjectTypeClothes, // Clothes, so user can elect to turn clothes off (naughty user!)
	ShaderObjectType_Count // Not used, records number of items in enum
};

//Utility methods for conversion
std::string GetShaderObjectTypeStrng(ShaderObjectType objectType);
ShaderObjectType GetShaderObjectTypeEnum(std::string objectType);

#endif