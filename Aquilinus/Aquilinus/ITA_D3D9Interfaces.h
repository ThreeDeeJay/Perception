/********************************************************************
Vireio Perception : Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

Aquilinus : Vireio Perception 3D Modification Studio 
Copyright � 2014 Denis Reischl

Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown
v2.0.4 to v3.0.x 2014-2015 by Grant Bagwell, Simon Brown and Neil Schneider
v4.0.x 2015 by Denis Reischl, Grant Bagwell, Simon Brown and Neil Schneider

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
#ifndef ITA_D3D9INTERFACES_ENUM
#define ITA_D3D9INTERFACES_ENUM

namespace ITA_D3D9INTERFACES
{
	/**
	* Direct3D 9.0 interfaces enumeration.
	* The following interfaces are used with Direct3D 9.
	***/
	enum ITA_D3D9Interfaces
	{
		/*** Direct3D9 interfaces ***/
		ID3DXFile,
		ID3DXFileData,
		ID3DXFileEnumObject,
		ID3DXFileSaveData,
		ID3DXFileSaveObject,
		IDirect3D9,
		IDirect3DBaseTexture9,
		IDirect3DCubeTexture9,
		IDirect3DDevice9,
		IDirect3DIndexBuffer9,
		IDirect3DPixelShader9,
		IDirect3DQuery9,
		IDirect3DResource9,
		IDirect3DStateBlock9,
		IDirect3DSurface9,
		IDirect3DSwapChain9,
		IDirect3DTexture9,
		IDirect3DVertexBuffer9,
		IDirect3DVertexDeclaration9,
		IDirect3DVertexShader9,
		IDirect3DVolume9,
		IDirect3DVolumeTexture9,
		/*** Direct3D9Ex interfaces ***/
		IDirect3D9Ex,
		IDirect3D9ExOverlayExtension,
		IDirect3DAuthenticatedChannel9,
		IDirect3DCryptoSession9,
		IDirect3DDevice9Ex,
		IDirect3DDevice9Video,
		IDirect3DSwapChain9Ex,
		/*** D3DX ****/
		D3DX9,
	};

}
#endif