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
#ifndef VMT_ID3D11DEVICECONTEXT1_ENUM
#define VMT_ID3D11DEVICECONTEXT1_ENUM

#include"VMT_IUnknown.h"
#include"VMT_ID3D11DeviceContext.h"

namespace VMT_ID3D11DEVICECONTEXT1
{
	/**
	* Virtual Methods Table for ID3D11DeviceContext1
	***/
	enum VMT_ID3D11DeviceContext1
	{
		/*** ID3D11DeviceContext1 methods ***/
		CopySubresourceRegion1 = 111,
		UpdateSubresource1 = 112,
		DiscardResource = 113,
		DiscardView = 114,
		VSSetConstantBuffers1 = 115,
		HSSetConstantBuffers1 = 116,
		DSSetConstantBuffers1 = 117,
		GSSetConstantBuffers1 = 118,
		PSSetConstantBuffers1 = 119,
		CSSetConstantBuffers1 = 120,
		VSGetConstantBuffers1 = 121,
		HSGetConstantBuffers1 = 122,
		DSGetConstantBuffers1 = 123,
		GSGetConstantBuffers1 = 124,
		PSGetConstantBuffers1 = 125,
		CSGetConstantBuffers1 = 126,
		SwapDeviceContextState = 127,
		ClearView = 128,
		DiscardView1 = 129,
	};
}
#endif
