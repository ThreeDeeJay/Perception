/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

OpenVR DirectMode - Open Virtual Reality Direct Mode Rendering Node
Copyright (C) 2016 Denis Reischl

File <OpenVR-DirectMode.cpp> and
Class <OpenVR-DirectMode> :
Copyright (C) 2016 Denis Reischl



Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown
v2.0.4 to v3.0.x 2014-2015 by Grant Bagwell, Simon Brown and Neil Schneider
v4.0.x 2015 by Denis Reischl, Grant Bagwell, Simon Brown, Samuel Austin
and Neil Schneider

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

#include"OpenVR-DirectMode.h"

#define SAFE_RELEASE(a) if (a) { a->Release(); a = nullptr; }
#define DEBUG_UINT(a) { wchar_t buf[128]; wsprintf(buf, L"- %u", a); OutputDebugString(buf); }
#define DEBUG_HEX(a) { wchar_t buf[128]; wsprintf(buf, L"- %x", a); OutputDebugString(buf); }
#define DEBUG_MARKER(a) { wchar_t buf[128]; wsprintf(buf, L"Debug marker %u", a); OutputDebugString(buf); }

#define INTERFACE_IDIRECT3DDEVICE9           8
#define INTERFACE_IDIRECT3DSWAPCHAIN9       15
#define INTERFACE_IDXGISWAPCHAIN            29

#define	METHOD_IDIRECT3DDEVICE9_PRESENT     17
#define	METHOD_IDIRECT3DDEVICE9_ENDSCENE    42
#define	METHOD_IDIRECT3DSWAPCHAIN9_PRESENT   3
#define METHOD_IDXGISWAPCHAIN_PRESENT        8

/**
* Matrix translation helper.
***/
inline vr::HmdMatrix34_t Translate_3x4(vr::HmdVector3_t sV)
{
	vr::HmdMatrix34_t sRet;

	sRet.m[0][0] = 1.0f; sRet.m[1][0] = 0.0f; sRet.m[2][0] = 0.0f;
	sRet.m[0][1] = 0.0f; sRet.m[1][1] = 1.0f; sRet.m[2][1] = 0.0f;
	sRet.m[0][2] = 0.0f; sRet.m[1][2] = 0.0f; sRet.m[2][2] = 1.0f;
	sRet.m[0][3] = sV.v[0]; sRet.m[1][3] = sV.v[1]; sRet.m[2][3] = sV.v[2];

	return sRet;
}

/**
* Little helper.
***/
HRESULT CreateCopyTexture(ID3D11Device* pcDevice, ID3D11Device* pcDeviceTemporary, ID3D11ShaderResourceView* pcSRV, ID3D11Texture2D** ppcDest, ID3D11Texture2D**ppcDestDraw, ID3D11RenderTargetView** ppcDestDrawRTV, ID3D11Texture2D** ppcDestShared)
{
	ID3D11Resource* pcResource = nullptr;
	pcSRV->GetResource(&pcResource);
	if (pcResource)
	{
		// get the description and create the copy texture
		D3D11_TEXTURE2D_DESC sDesc;
		((ID3D11Texture2D*)pcResource)->GetDesc(&sDesc);
		pcResource->Release();

		sDesc.ArraySize = 1;
		sDesc.CPUAccessFlags = 0;
		sDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		sDesc.MipLevels = 1;
		sDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		sDesc.SampleDesc.Count = 1;
		sDesc.SampleDesc.Quality = 0;
		sDesc.Usage = D3D11_USAGE_DEFAULT;
		sDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		// create the copy texture
		if (FAILED(pcDevice->CreateTexture2D(&sDesc, NULL, ppcDest)))
		{
			OutputDebugString(L"OpenVR : Failed to create copy texture !");
			return E_FAIL;
		}

		// create only if needed
		if ((ppcDestDraw) && (ppcDestDrawRTV))
		{
			// create the drawing texture and view
			sDesc.MiscFlags = 0;
			sDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			if (FAILED(pcDevice->CreateTexture2D(&sDesc, NULL, ppcDestDraw)))
			{
				(*ppcDest)->Release(); (*ppcDest) = nullptr;
				OutputDebugString(L"OpenVR : Failed to create overlay texture !");
				return E_FAIL;
			}
			pcDevice->CreateRenderTargetView((ID3D11Resource*)*ppcDestDraw, NULL, ppcDestDrawRTV);
		}

		// get shared handle
		IDXGIResource* pcDXGIResource(NULL);
		(*ppcDest)->QueryInterface(__uuidof(IDXGIResource), (void**)&pcDXGIResource);
		HANDLE sharedHandle;
		if (pcDXGIResource)
		{
			pcDXGIResource->GetSharedHandle(&sharedHandle);
			pcDXGIResource->Release();
		}
		else OutputDebugString(L"Failed to query IDXGIResource.");

		// open the shared handle with the TEMPORARY (!) device
		ID3D11Resource* pcResourceShared;
		pcDeviceTemporary->OpenSharedResource(sharedHandle, __uuidof(ID3D11Resource), (void**)(&pcResourceShared));
		if (pcResourceShared)
		{
			pcResourceShared->QueryInterface(__uuidof(ID3D11Texture2D), (void**)ppcDestShared);
			pcResourceShared->Release();
		}
		else OutputDebugString(L"Could not open shared resource.");
	}
	return S_OK;
}

/**
* Constructor.
***/
OpenVR_DirectMode::OpenVR_DirectMode() : AQU_Nodus(),
m_pcDeviceTemporary(nullptr),
m_pcContextTemporary(nullptr),
m_ulOverlayHandle(0),
m_ulOverlayThumbnailHandle(0),
m_bHotkeySwitch(false),
m_pbZoomOut(nullptr),
m_pcVSGeometry11(nullptr),
m_pcVLGeometry11(nullptr),
m_pcPSGeometry11(nullptr),
m_pcSampler11(nullptr),
m_pcRS(nullptr),
m_pcConstantBufferGeometry(nullptr),
m_asRenderModels(),
m_bRenderModelsCreated(false)
{
	m_ppcTexView11[0] = nullptr;
	m_ppcTexView11[1] = nullptr;
	m_pcTex11Copy[0] = nullptr;
	m_pcTex11Copy[1] = nullptr;
	m_pcTex11CopyHUD = nullptr;
	m_pcTex11Draw[0] = nullptr;
	m_pcTex11Draw[1] = nullptr;
	m_pcTex11DrawRTV[0] = nullptr;
	m_pcTex11DrawRTV[1] = nullptr;
	m_pcTex11Shared[0] = nullptr;
	m_pcTex11Shared[1] = nullptr;
	m_pcTex11SharedHUD = nullptr;

	m_pcVertexShader11 = nullptr;
	m_pcPixelShader11 = nullptr;
	m_pcVertexLayout11 = nullptr;
	m_pcVertexBuffer11 = nullptr;
	m_pcConstantBufferDirect11 = nullptr;
	m_pcSamplerState = nullptr;
	m_pcDSGeometry11[0] = nullptr;
	m_pcDSGeometry11[1] = nullptr;
	m_pcDSVGeometry11[0] = nullptr;
	m_pcDSVGeometry11[1] = nullptr;

	// static fields
	m_bInit = false;
	m_ppHMD = nullptr;
	m_fHorizontalRatioCorrectionLeft = 0.0f;
	m_fHorizontalRatioCorrectionRight = 0.0f;
	m_fHorizontalOffsetCorrectionLeft = 0.0f;
	m_fHorizontalOffsetCorrectionRight = 0.0f;

	//----------------------

	// set default ini settings
	m_bForceInterleavedReprojection = false;

	// set first matrices.. 
	D3DXMatrixIdentity(&m_sView);
	D3DXMatrixIdentity(&m_sProj[0]);
	D3DXMatrixIdentity(&m_sProj[1]);

	// set default overlay properties
	m_sOverlayPropertiesDashboard.sColor.a = 1.0f;
	m_sOverlayPropertiesDashboard.sColor.r = 1.0f;
	m_sOverlayPropertiesDashboard.sColor.g = 1.0f;
	m_sOverlayPropertiesDashboard.sColor.b = 1.0f;
	m_sOverlayPropertiesDashboard.fWidth = 3.0f;

	// create constant shader constants..
	D3DXVECTOR4 sLightDir(-0.7f, -0.6f, -0.02f, 1.0f);
	m_sGeometryConstants.sLightDir = sLightDir;
	m_sGeometryConstants.sLightAmbient = D3DXCOLOR(0.2f, 0.2f, 0.2f, 1.0f);
	m_sGeometryConstants.sLightDiffuse = D3DXCOLOR(1.0f, 0.2f, 0.7f, 1.0f);

	// locate or create the INI file
	char szFilePathINI[1024];
	GetCurrentDirectoryA(1024, szFilePathINI);
	strcat_s(szFilePathINI, "\\VireioPerception.ini");
	bool bFileExists = false;
	if (PathFileExistsA(szFilePathINI)) bFileExists = true;

	// get all ini settings
	if (GetIniFileSetting((DWORD)m_bForceInterleavedReprojection, "OpenVR", "bForceInterleavedReprojection", szFilePathINI, bFileExists)) m_bForceInterleavedReprojection = true; else m_bForceInterleavedReprojection = false;

	m_sOverlayPropertiesDashboard.sColor.a = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.a, "OpenVR", "sOverlayPropertiesDashboard.sColor.a", szFilePathINI, bFileExists);
	m_sOverlayPropertiesDashboard.sColor.r = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.r, "OpenVR", "sOverlayPropertiesDashboard.sColor.r", szFilePathINI, bFileExists);
	m_sOverlayPropertiesDashboard.sColor.g = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.g, "OpenVR", "sOverlayPropertiesDashboard.sColor.g", szFilePathINI, bFileExists);
	m_sOverlayPropertiesDashboard.sColor.b = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.b, "OpenVR", "sOverlayPropertiesDashboard.sColor.b", szFilePathINI, bFileExists);
	m_sOverlayPropertiesDashboard.fWidth = GetIniFileSetting(m_sOverlayPropertiesDashboard.fWidth, "OpenVR", "sOverlayPropertiesDashboard.fWidth", szFilePathINI, bFileExists);

	m_sGeometryConstants.sLightDir.x = GetIniFileSetting(m_sGeometryConstants.sLightDir.x, "OpenVR", "sGeometryConstants.sLightDirection.x", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightDir.y = GetIniFileSetting(m_sGeometryConstants.sLightDir.y, "OpenVR", "sGeometryConstants.sLightDirection.y", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightDir.z = GetIniFileSetting(m_sGeometryConstants.sLightDir.z, "OpenVR", "sGeometryConstants.sLightDirection.z", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightAmbient.a = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.a, "OpenVR", "sGeometryConstants.sColorAmbient.a", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightAmbient.r = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.r, "OpenVR", "sGeometryConstants.sColorAmbient.r", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightAmbient.g = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.g, "OpenVR", "sGeometryConstants.sColorAmbient.g", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightAmbient.b = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.b, "OpenVR", "sGeometryConstants.sColorAmbient.b", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightDiffuse.a = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.a, "OpenVR", "sGeometryConstants.sColorDiffuse.a", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightDiffuse.r = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.r, "OpenVR", "sGeometryConstants.sColorDiffuse.r", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightDiffuse.g = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.g, "OpenVR", "sGeometryConstants.sColorDiffuse.g", szFilePathINI, bFileExists);
	m_sGeometryConstants.sLightDiffuse.b = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.b, "OpenVR", "sGeometryConstants.sColorDiffuse.b", szFilePathINI, bFileExists);

	// normalize light direction
	D3DXVec4Normalize(&m_sGeometryConstants.sLightDir, &m_sGeometryConstants.sLightDir);

	// create the menu
	ZeroMemory(&m_sMenu, sizeof(VireioSubMenu));
	m_sMenu.strSubMenu = "OpenVR DirectMode";
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Force Interleaved Reprojection";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Bool;
		sEntry.pbValue = &m_bForceInterleavedReprojection;
		sEntry.bValue = m_bForceInterleavedReprojection;
		m_sMenu.asEntries.push_back(sEntry);
	}

#pragma region dashboard menu
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Dashboard Color A";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sOverlayPropertiesDashboard.sColor.a;
		sEntry.fValue = m_sOverlayPropertiesDashboard.sColor.a;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Dashboard Color R";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sOverlayPropertiesDashboard.sColor.r;
		sEntry.fValue = m_sOverlayPropertiesDashboard.sColor.r;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Dashboard Color G";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sOverlayPropertiesDashboard.sColor.g;
		sEntry.fValue = m_sOverlayPropertiesDashboard.sColor.g;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Dashboard Color B";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sOverlayPropertiesDashboard.sColor.b;
		sEntry.fValue = m_sOverlayPropertiesDashboard.sColor.b;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Dashboard Width";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 1.0f;
		sEntry.fMaximum = 10.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sOverlayPropertiesDashboard.fWidth;
		sEntry.fValue = m_sOverlayPropertiesDashboard.fWidth;
		m_sMenu.asEntries.push_back(sEntry);
	}
#pragma endregion
#pragma region light menu
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Dir X";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = -1.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightDir.x;
		sEntry.fValue = m_sGeometryConstants.sLightDir.x;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Dir Y";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = -1.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightDir.y;
		sEntry.fValue = m_sGeometryConstants.sLightDir.y;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Dir Z";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = -1.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightDir.z;
		sEntry.fValue = m_sGeometryConstants.sLightDir.z;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Ambient A";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightAmbient.a;
		sEntry.fValue = m_sGeometryConstants.sLightAmbient.a;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Ambient R";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightAmbient.r;
		sEntry.fValue = m_sGeometryConstants.sLightAmbient.r;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Ambient G";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightAmbient.g;
		sEntry.fValue = m_sGeometryConstants.sLightAmbient.g;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Ambient B";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightAmbient.b;
		sEntry.fValue = m_sGeometryConstants.sLightAmbient.b;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Diffuse A";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightDiffuse.a;
		sEntry.fValue = m_sGeometryConstants.sLightDiffuse.a;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Diffuse R";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightDiffuse.r;
		sEntry.fValue = m_sGeometryConstants.sLightDiffuse.r;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Diffuse G";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightDiffuse.g;
		sEntry.fValue = m_sGeometryConstants.sLightDiffuse.g;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Light Diffuse B";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 1.0f;
		sEntry.fChangeSize = 0.01f;
		sEntry.pfValue = &m_sGeometryConstants.sLightDiffuse.b;
		sEntry.fValue = m_sGeometryConstants.sLightDiffuse.b;
		m_sMenu.asEntries.push_back(sEntry);
	}
#pragma endregion
}

/**
* Destructor.
***/
OpenVR_DirectMode::~OpenVR_DirectMode()
{
	for (UINT unI = 0; unI < (UINT)m_asRenderModels.size(); unI++)
	{
		SAFE_RELEASE(m_asRenderModels[unI].pcIndexBuffer);
		SAFE_RELEASE(m_asRenderModels[unI].pcVertexBuffer);
		SAFE_RELEASE(m_asRenderModels[unI].pcTextureSRV);
		SAFE_RELEASE(m_asRenderModels[unI].pcTexture);
	}

	SAFE_RELEASE(m_pcRS);
	SAFE_RELEASE(m_pcSampler11);
	SAFE_RELEASE(m_pcConstantBufferGeometry);
	SAFE_RELEASE(m_pcDSVGeometry11[0]);
	SAFE_RELEASE(m_pcDSVGeometry11[1]);
	SAFE_RELEASE(m_pcDSGeometry11[0]);
	SAFE_RELEASE(m_pcDSGeometry11[1]);
	SAFE_RELEASE(m_pcVLGeometry11);
	SAFE_RELEASE(m_pcVSGeometry11);
	SAFE_RELEASE(m_pcPSGeometry11);

	if (m_pcContextTemporary) m_pcContextTemporary->Release();
	if (m_pcDeviceTemporary) m_pcDeviceTemporary->Release();

	if (m_pcTex11Copy[0]) m_pcTex11Copy[0]->Release();
	if (m_pcTex11Copy[1]) m_pcTex11Copy[1]->Release();
	if (m_pcTex11Draw[0]) m_pcTex11Draw[0]->Release();
	if (m_pcTex11Draw[1]) m_pcTex11Draw[1]->Release();
	if (m_pcTex11DrawRTV[0]) m_pcTex11DrawRTV[0]->Release();
	if (m_pcTex11DrawRTV[1]) m_pcTex11DrawRTV[1]->Release();
	if (m_pcTex11Shared[0]) m_pcTex11Shared[0]->Release();
	if (m_pcTex11Shared[1]) m_pcTex11Shared[1]->Release();
}

/**
* Return the name of the  OpenVR DirectMode node.
***/
const char* OpenVR_DirectMode::GetNodeType()
{
	return "OpenVR DirectMode";
}

/**
* Returns a global unique identifier for the OpenVR DirectMode node.
***/
UINT OpenVR_DirectMode::GetNodeTypeId()
{
#define DEVELOPER_IDENTIFIER 2006
#define MY_PLUGIN_IDENTIFIER 321
	return ((DEVELOPER_IDENTIFIER << 16) + MY_PLUGIN_IDENTIFIER);
}

/**
* Returns the name of the category for the OpenVR DirectMode node.
***/
LPWSTR OpenVR_DirectMode::GetCategory()
{
	return L"Renderer";
}

/**
* Returns a logo to be used for the OpenVR DirectMode node.
***/
HBITMAP OpenVR_DirectMode::GetLogo()
{
	HMODULE hModule = GetModuleHandle(L"OpenVR-DirectMode.dll");
	HBITMAP hBitmap = LoadBitmap(hModule, MAKEINTRESOURCE(IMG_LOGO01));
	return hBitmap;
}

/**
* Returns the updated control for the OpenVR DirectMode node.
* Allways return >nullptr< if there is no update for the control !!
***/
HBITMAP OpenVR_DirectMode::GetControl()
{
	return nullptr;
}

/**
* Provides the name of the requested commander.
***/
LPWSTR OpenVR_DirectMode::GetCommanderName(DWORD dwCommanderIndex)
{
	switch ((OpenVR_Commanders)dwCommanderIndex)
	{
		case VireioMenu:
			return L"Vireio Menu";
	}

	return L"XXX";
}

/**
* Provides the name of the requested decommander.
***/
LPWSTR OpenVR_DirectMode::GetDecommanderName(DWORD dwDecommanderIndex)
{
	switch ((OpenVR_Decommanders)dwDecommanderIndex)
	{
		case OpenVR_Decommanders::LeftTexture11:
			return L"Left Texture DX11";
		case OpenVR_Decommanders::RightTexture11:
			return L"Right Texture DX11";
		case OpenVR_Decommanders::IVRSystem:
			return L"IVRSystem";
		case OpenVR_Decommanders::ZoomOut:
			return L"Zoom Out";
		case HUDTexture11:
			return L"HUD Texture DX11";
		case HUDTexture10:
			break;
		case HUDTexture9:
			break;
	}

	return L"x";
}

/**
* Returns the plug type for the requested commander.
***/
DWORD OpenVR_DirectMode::GetCommanderType(DWORD dwCommanderIndex)
{
	switch ((OpenVR_Commanders)dwCommanderIndex)
	{
		case VireioMenu:
			return NOD_Plugtype::AQU_VOID;
	}

	return 0;
}

/**
* Returns the plug type for the requested decommander.
***/
DWORD OpenVR_DirectMode::GetDecommanderType(DWORD dwDecommanderIndex)
{
	switch ((OpenVR_Decommanders)dwDecommanderIndex)
	{
		case OpenVR_Decommanders::LeftTexture11:
			return NOD_Plugtype::AQU_PNT_ID3D11SHADERRESOURCEVIEW;
		case OpenVR_Decommanders::RightTexture11:
			return NOD_Plugtype::AQU_PNT_ID3D11SHADERRESOURCEVIEW;
		case OpenVR_Decommanders::IVRSystem:
			return NOD_Plugtype::AQU_HANDLE;
		case OpenVR_Decommanders::ZoomOut:
			return NOD_Plugtype::AQU_BOOL;
		case HUDTexture11:
			return NOD_Plugtype::AQU_PNT_ID3D11SHADERRESOURCEVIEW;
		case HUDTexture10:
			break;
		case HUDTexture9:
			break;
	}

	return 0;
}

/**
* Provides the output pointer for the requested commander.
***/
void* OpenVR_DirectMode::GetOutputPointer(DWORD dwCommanderIndex)
{
	switch ((OpenVR_Commanders)dwCommanderIndex)
	{
		case OpenVR_Commanders::VireioMenu:
			return (void*)&m_sMenu;
		default:
			break;
	}

	return nullptr;
}

/**
* Sets the input pointer for the requested decommander.
***/
void OpenVR_DirectMode::SetInputPointer(DWORD dwDecommanderIndex, void* pData)
{
	switch ((OpenVR_Decommanders)dwDecommanderIndex)
	{
		case OpenVR_Decommanders::LeftTexture11:
			m_ppcTexView11[0] = (ID3D11ShaderResourceView**)pData;
			break;
		case OpenVR_Decommanders::RightTexture11:
			m_ppcTexView11[1] = (ID3D11ShaderResourceView**)pData;
			break;
		case OpenVR_Decommanders::IVRSystem:
			m_ppHMD = (vr::IVRSystem**)pData;
			break;
		case OpenVR_Decommanders::ZoomOut:
			m_pbZoomOut = (BOOL*)pData;
			break;
		case HUDTexture11:
			m_ppcTexViewHud11 = (ID3D11ShaderResourceView**)pData;
			break;
		case HUDTexture10:
			break;
		case HUDTexture9:
			break;
	}
}

/**
* DirectMode supports any calls.
***/
bool OpenVR_DirectMode::SupportsD3DMethod(int nD3DVersion, int nD3DInterface, int nD3DMethod)
{
	if ((nD3DInterface == INTERFACE_IDXGISWAPCHAIN) ||
		(nD3DInterface == INTERFACE_IDIRECT3DDEVICE9) ||
		(nD3DInterface == INTERFACE_IDIRECT3DSWAPCHAIN9))
		return true;

	return true;
}

/**
* Handle OpenVR tracking.
***/
void* OpenVR_DirectMode::Provoke(void* pThis, int eD3D, int eD3DInterface, int eD3DMethod, DWORD dwNumberConnected, int& nProvokerIndex)
{
	// submit thread id
	static DWORD unThreadId = 0;

	if ((eD3DInterface == INTERFACE_IDXGISWAPCHAIN) && (eD3DMethod != METHOD_IDXGISWAPCHAIN_PRESENT)) return nullptr;
	if ((eD3DInterface == INTERFACE_IDIRECT3DDEVICE9) && (eD3DMethod != METHOD_IDIRECT3DDEVICE9_PRESENT)) return nullptr;
	if ((eD3DInterface == INTERFACE_IDIRECT3DSWAPCHAIN9) && (eD3DMethod != METHOD_IDIRECT3DSWAPCHAIN9_PRESENT)) return nullptr;

	/*if (!m_bHotkeySwitch)
	{
	if (GetAsyncKeyState(VK_F11))
	{
	m_bHotkeySwitch = true;
	}
	return nullptr;
	}*/

	// save ini file ?
	if (m_nIniFrameCount)
	{
		if (m_nIniFrameCount == 1)
		{
			// get file path
			char szFilePathINI[1024];
			GetCurrentDirectoryA(1024, szFilePathINI);
			strcat_s(szFilePathINI, "\\VireioPerception.ini");
			bool bFileExists = false;

			if (GetIniFileSetting((DWORD)m_bForceInterleavedReprojection, "OpenVR", "bForceInterleavedReprojection", szFilePathINI, bFileExists)) m_bForceInterleavedReprojection = true; else m_bForceInterleavedReprojection = false;

			m_sOverlayPropertiesDashboard.sColor.a = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.a, "OpenVR", "sOverlayPropertiesDashboard.sColor.a", szFilePathINI, bFileExists);
			m_sOverlayPropertiesDashboard.sColor.r = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.r, "OpenVR", "sOverlayPropertiesDashboard.sColor.r", szFilePathINI, bFileExists);
			m_sOverlayPropertiesDashboard.sColor.g = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.g, "OpenVR", "sOverlayPropertiesDashboard.sColor.g", szFilePathINI, bFileExists);
			m_sOverlayPropertiesDashboard.sColor.b = GetIniFileSetting(m_sOverlayPropertiesDashboard.sColor.b, "OpenVR", "sOverlayPropertiesDashboard.sColor.b", szFilePathINI, bFileExists);
			m_sOverlayPropertiesDashboard.fWidth = GetIniFileSetting(m_sOverlayPropertiesDashboard.fWidth, "OpenVR", "sOverlayPropertiesDashboard.fWidth", szFilePathINI, bFileExists);

			m_sGeometryConstants.sLightDir.x = GetIniFileSetting(m_sGeometryConstants.sLightDir.x, "OpenVR", "sGeometryConstants.sLightDirection.x", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightDir.y = GetIniFileSetting(m_sGeometryConstants.sLightDir.y, "OpenVR", "sGeometryConstants.sLightDirection.y", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightDir.z = GetIniFileSetting(m_sGeometryConstants.sLightDir.z, "OpenVR", "sGeometryConstants.sLightDirection.z", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightAmbient.a = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.a, "OpenVR", "sGeometryConstants.sColorAmbient.a", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightAmbient.r = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.r, "OpenVR", "sGeometryConstants.sColorAmbient.r", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightAmbient.g = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.g, "OpenVR", "sGeometryConstants.sColorAmbient.g", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightAmbient.b = GetIniFileSetting(m_sGeometryConstants.sLightAmbient.b, "OpenVR", "sGeometryConstants.sColorAmbient.b", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightDiffuse.a = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.a, "OpenVR", "sGeometryConstants.sColorDiffuse.a", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightDiffuse.r = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.r, "OpenVR", "sGeometryConstants.sColorDiffuse.r", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightDiffuse.g = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.g, "OpenVR", "sGeometryConstants.sColorDiffuse.g", szFilePathINI, bFileExists);
			m_sGeometryConstants.sLightDiffuse.b = GetIniFileSetting(m_sGeometryConstants.sLightDiffuse.b, "OpenVR", "sGeometryConstants.sColorDiffuse.b", szFilePathINI, bFileExists);

		}
		m_nIniFrameCount--;
	}

	// main menu update ?
	if (m_sMenu.bOnChanged)
	{
		// set back event bool, set ini file frame count
		m_sMenu.bOnChanged = false;
		m_nIniFrameCount = 300;

		// loop through entries
		for (size_t nIx = 0; nIx < m_sMenu.asEntries.size(); nIx++)
		{
			// entry index changed ?
			if (m_sMenu.asEntries[nIx].bOnChanged)
			{
				m_sMenu.asEntries[nIx].bOnChanged = false;

			}
		}
	}

	// get device
	ID3D11Device* pcDevice = nullptr;
	switch (eD3DInterface)
	{
		case INTERFACE_IDXGISWAPCHAIN:
			if (pThis)
			{
				// cast swapchain
				IDXGISwapChain* pcSwapChain = (IDXGISwapChain*)pThis;

				// and get the device from the swapchain
				pcSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pcDevice);
				pcSwapChain->Release();
			}
			else
			{
				OutputDebugString(L"[OPENVR] : No swapchain !");
				return nullptr;
			}
			break;
		case INTERFACE_IDIRECT3DDEVICE9:
		case INTERFACE_IDIRECT3DSWAPCHAIN9:
			// get the d3d11 device from the connected tex view
			if (m_ppcTexView11[0])
			{
				if (*(m_ppcTexView11[0]))
					(*(m_ppcTexView11[0]))->GetDevice(&pcDevice);
				// else return nullptr;
			}
			else OutputDebugString(L"[OPENVR] : Connect d3d 11 texture views to obtain d3d 11 device.");
			break;
	}

	if (!pcDevice)
	{
		OutputDebugString(L"[OPENVR] : No d3d 11 device !");
		return nullptr;
	}

	// get context
	ID3D11DeviceContext* pcContext = nullptr;
	pcDevice->GetImmediateContext(&pcContext);
	if (!pcContext)
	{
		OutputDebugString(L"[OPENVR] : No d3d 11 device context !");
		return nullptr;
	}

	if (m_ppHMD)
	{
		if (*m_ppHMD)
		{
			if (!m_bInit)
			{
#pragma region Init
				if (!m_pcDeviceTemporary)
				{
					// create temporary device
					IDXGIFactory * DXGIFactory;
					IDXGIAdapter * Adapter;
					if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)(&DXGIFactory))))
						return(false);
					if (FAILED(DXGIFactory->EnumAdapters(0, &Adapter)))
						return(false);
					if (FAILED(D3D11CreateDevice(Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
						NULL, 0, NULL, 0, D3D11_SDK_VERSION, &m_pcDeviceTemporary, NULL, &m_pcContextTemporary)))
						return(false);
					DXGIFactory->Release();

					// Set max frame latency to 1
					IDXGIDevice1* DXGIDevice1 = NULL;
					HRESULT hr = m_pcDeviceTemporary->QueryInterface(__uuidof(IDXGIDevice1), (void**)&DXGIDevice1);
					if (FAILED(hr) | (DXGIDevice1 == NULL))
					{
						pcDevice->Release();
						pcContext->Release();
						return nullptr;
					}
					DXGIDevice1->SetMaximumFrameLatency(1);
					DXGIDevice1->Release();
				}

				// is the OpenVR compositor present ?
				if (!vr::VRCompositor())
				{
					OutputDebugString(L"OpenVR: Compositor initialization failed.\n");
					return nullptr;
				}

				// create the dashboard overlay
				vr::VROverlayError overlayError = vr::VROverlay()->CreateDashboardOverlay(OPENVR_OVERLAY_NAME, OPENVR_OVERLAY_FRIENDLY_NAME, &m_ulOverlayHandle, &m_ulOverlayThumbnailHandle);
				if (overlayError != vr::VROverlayError_None)
				{
					OutputDebugString(L"OpenVR: Failed to create overlay.");
					return nullptr;
				}

				// set overlay options
				vr::VROverlay()->SetOverlayWidthInMeters(m_ulOverlayHandle, m_sOverlayPropertiesDashboard.fWidth);
				vr::VROverlay()->SetOverlayInputMethod(m_ulOverlayHandle, vr::VROverlayInputMethod_Mouse);
				vr::VROverlay()->SetOverlayAlpha(m_ulOverlayHandle, m_sOverlayPropertiesDashboard.sColor.a);
				vr::VROverlay()->SetOverlayColor(m_ulOverlayHandle, m_sOverlayPropertiesDashboard.sColor.r, m_sOverlayPropertiesDashboard.sColor.g, m_sOverlayPropertiesDashboard.sColor.b);

				m_bInit = true;
#pragma endregion
			}
			else
				if (vr::VRCompositor()->CanRenderScene())
				{
#pragma region render models
					if (!vr::VROverlay()->IsDashboardVisible())
					{
						static UINT unWidthRT = 0, unHeightRT = 0;

						// textures connected ?
						if ((m_ppcTexView11[0]) && (m_ppcTexView11[1]))
						{
							// backup all states
							D3DX11_STATE_BLOCK sStateBlock;
							CreateStateblock(pcContext, &sStateBlock);

							// clear all states, set targets
							ClearContextState(pcContext);

#pragma region set or create
							// set or create a default rasterizer state here
							if (!m_pcRS)
							{
								D3D11_RASTERIZER_DESC sDesc;
								sDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
								sDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_BACK;
								sDesc.FrontCounterClockwise = FALSE;
								sDesc.DepthBias = 0;
								sDesc.SlopeScaledDepthBias = 0.0f;
								sDesc.DepthBiasClamp = 0.0f;
								sDesc.DepthClipEnable = TRUE;
								sDesc.ScissorEnable = FALSE;
								sDesc.MultisampleEnable = FALSE;
								sDesc.AntialiasedLineEnable = FALSE;

								pcDevice->CreateRasterizerState(&sDesc, &m_pcRS);
							}
							pcContext->RSSetState(m_pcRS);

							// set viewport by render target size
							uint32_t unWidth, unHeight;
							(*m_ppHMD)->GetRecommendedRenderTargetSize(&unWidth, &unHeight);
							D3D11_VIEWPORT sViewport = {};
							sViewport.Width = (FLOAT)unWidth;
							sViewport.Height = (FLOAT)unHeight;
							sViewport.MaxDepth = 1.0f;
							pcContext->RSSetViewports(1, &sViewport);

							// create vertex shader
							if (!m_pcVSGeometry11)
							{
								if (FAILED(CreateVertexShaderTechnique(pcDevice, &m_pcVSGeometry11, &m_pcVLGeometry11, VertexShaderTechnique::PosNormUV)))
									OutputDebugString(L"[OPENVR] Failed to create vertex shader. !");
							}

							// create pixel shader
							if (!m_pcPSGeometry11)
							{
								if (FAILED(CreatePixelShaderEffect(pcDevice, &m_pcPSGeometry11, PixelShaderTechnique::GeometryDiffuseTextured)))
									OutputDebugString(L"[OPENVR] Failed to create pixel shader. !");
							}

							// create the depth stencil
							if (!m_pcDSGeometry11[0])
							{
								ID3D11Texture2D* pcResource = nullptr;
								if (m_ppcTexView11[0])
									(*m_ppcTexView11)[0]->GetResource((ID3D11Resource**)&pcResource);

								if (pcResource)
								{
									D3D11_TEXTURE2D_DESC sDesc;
									pcResource->GetDesc(&sDesc);
									pcResource->Release();

									// set static height, width
									unWidthRT = sDesc.Width;
									unHeightRT = sDesc.Height;

									// Create depth stencil textures
									D3D11_TEXTURE2D_DESC descDepth;
									ZeroMemory(&descDepth, sizeof(descDepth));
									descDepth.Width = sDesc.Width;
									descDepth.Height = sDesc.Height;
									descDepth.MipLevels = 1;
									descDepth.ArraySize = 1;
									descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
									descDepth.SampleDesc.Count = 1;
									descDepth.SampleDesc.Quality = 0;
									descDepth.Usage = D3D11_USAGE_DEFAULT;
									descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
									descDepth.CPUAccessFlags = 0;
									descDepth.MiscFlags = 0;
									if (FAILED(pcDevice->CreateTexture2D(&descDepth, NULL, &m_pcDSGeometry11[0])))
										OutputDebugString(L"[OPENVR] Failed to create depth stencil.");
									if (FAILED(pcDevice->CreateTexture2D(&descDepth, NULL, &m_pcDSGeometry11[1])))
										OutputDebugString(L"[OPENVR] Failed to create depth stencil.");

									// Create the depth stencil views
									D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
									ZeroMemory(&descDSV, sizeof(descDSV));
									descDSV.Format = descDepth.Format;
									descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
									descDSV.Texture2D.MipSlice = 0;
									if (FAILED(pcDevice->CreateDepthStencilView(m_pcDSGeometry11[0], &descDSV, &m_pcDSVGeometry11[0])))
										OutputDebugString(L"[OPENVR] Failed to create depth stencil view.");
									if (FAILED(pcDevice->CreateDepthStencilView(m_pcDSGeometry11[1], &descDSV, &m_pcDSVGeometry11[1])))
										OutputDebugString(L"[OPENVR] Failed to create depth stencil view.");

									// create left/right matrices
									for (INT nEye = 0; nEye < 2; nEye++)
									{
										// get the projection matrix for each eye
										m_sProj[nEye] = GetHMDMatrixProjectionEyeLH(*m_ppHMD, (vr::Hmd_Eye)nEye, 0.1f, 30.0f);

										// create eye pose matrix
										m_sToEye[nEye] = GetHMDMatrixPoseEyeLH(*m_ppHMD, (vr::Hmd_Eye)nEye);
									}
								}
							}

							if (!m_pcSampler11)
							{
								// Create the sample state
								D3D11_SAMPLER_DESC sampDesc;
								ZeroMemory(&sampDesc, sizeof(sampDesc));
								sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
								sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
								sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
								sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
								sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
								sampDesc.MinLOD = 0;
								sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
								if (FAILED(pcDevice->CreateSamplerState(&sampDesc, &m_pcSampler11)))
									OutputDebugString(L"[OPENVR] Failed to create sampler.");
							}

							// create constant buffer
							if (!m_pcConstantBufferGeometry)
							{
								if (FAILED(CreateGeometryConstantBuffer(pcDevice, &m_pcConstantBufferGeometry, (UINT)sizeof(GeometryConstantBuffer))))
									OutputDebugString(L"[OPENVR] Failed to create constant buffer.");
							}
#pragma endregion
#pragma region create render models
							// create all models
							if (!m_bRenderModelsCreated)
							{
								for (uint32_t unTrackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1; unTrackedDevice < vr::k_unMaxTrackedDeviceCount; unTrackedDevice++)
								{
									// continue if not connected
									if (!(*m_ppHMD)->IsTrackedDeviceConnected(unTrackedDevice))
										continue;

									// get model name
									uint32_t unRequiredBufferLen = (*m_ppHMD)->GetStringTrackedDeviceProperty(unTrackedDevice, vr::Prop_RenderModelName_String, NULL, 0, NULL);
									char *pchBuffer = new char[unRequiredBufferLen];
									if (unRequiredBufferLen > 0)
										unRequiredBufferLen = (*m_ppHMD)->GetStringTrackedDeviceProperty(unTrackedDevice, vr::Prop_RenderModelName_String, pchBuffer, unRequiredBufferLen, NULL);
									std::string szModelName = pchBuffer;
									delete[] pchBuffer;

									// output model name
									OutputDebugString(L"[OPENVR] Connected model name : ");
									OutputDebugStringA(szModelName.c_str());

									// load model and texture
									vr::RenderModel_t *pModel = NULL;
									vr::RenderModel_TextureMap_t *pTexture = NULL;

									// model
									while (vr::VRRenderModels()->LoadRenderModel_Async(szModelName.c_str(), &pModel) == vr::VRRenderModelError_Loading)
										Sleep(50);
									if (vr::VRRenderModels()->LoadRenderModel_Async(szModelName.c_str(), &pModel) || pModel == NULL)
										OutputDebugString(L"[OPENVR] Unable to load render model");

									// texture
									while (vr::VRRenderModels()->LoadTexture_Async(pModel->diffuseTextureId, &pTexture) == vr::VRRenderModelError_Loading)
										Sleep(50);
									if (vr::VRRenderModels()->LoadTexture_Async(pModel->diffuseTextureId, &pTexture) || pTexture == NULL)
										OutputDebugString(L"[OPENVR] Unable to load render texture");

									// create a D3D render model structure
									RenderModel_D3D sRenderModel = {};

									// translate the model from Right-Handed to Left-Handed (negate z-axis for each vertex)
									vr::RenderModel_Vertex_t* asVertices = new vr::RenderModel_Vertex_t[pModel->unVertexCount];
									memcpy(asVertices, pModel->rVertexData, sizeof(vr::RenderModel_Vertex_t) * pModel->unVertexCount);
									for (UINT unI = 0; unI < pModel->unVertexCount; unI++)
									{
										// negate z axis
										asVertices[unI].vPosition.v[2] *= -1.0f;
										asVertices[unI].vNormal.v[2] *= -1.0f;
									}

									// arrange the indices accordingly to match the new vertex translations
									uint16_t* aunIndices = new uint16_t[pModel->unTriangleCount * 3];
									memcpy(aunIndices, pModel->rIndexData, sizeof(uint16_t)* pModel->unTriangleCount * 3);
									for (UINT unI = 0; unI < pModel->unTriangleCount; unI++)
									{
										// exchange 2nd and 3rd index of the triangle
										uint16_t unIndex3 = aunIndices[unI * 3 + 2];
										aunIndices[unI * 3 + 2] = aunIndices[unI * 3 + 1];
										aunIndices[unI * 3 + 1] = unIndex3;
									}

									// Create vertex buffer
									D3D11_BUFFER_DESC sVertexBufferDesc;
									ZeroMemory(&sVertexBufferDesc, sizeof(sVertexBufferDesc));
									sVertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
									sVertexBufferDesc.ByteWidth = sizeof(TexturedNormalVertex)* pModel->unVertexCount;
									sVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
									sVertexBufferDesc.CPUAccessFlags = 0;
									D3D11_SUBRESOURCE_DATA sInitData;
									ZeroMemory(&sInitData, sizeof(sInitData));
									sInitData.pSysMem = asVertices;
									if (FAILED(pcDevice->CreateBuffer(&sVertexBufferDesc, &sInitData, &sRenderModel.pcVertexBuffer)))
										OutputDebugString(L"[OPENVR] Failed to create vertex buffer.");


									// create index buffer
									D3D11_BUFFER_DESC sIndexBufferDesc;
									ZeroMemory(&sIndexBufferDesc, sizeof(sIndexBufferDesc));
									sIndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
									sIndexBufferDesc.ByteWidth = sizeof(WORD)* pModel->unTriangleCount * 3;
									sIndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
									sIndexBufferDesc.CPUAccessFlags = 0;
									ZeroMemory(&sInitData, sizeof(sInitData));
									sInitData.pSysMem = aunIndices;
									if (FAILED(pcDevice->CreateBuffer(&sIndexBufferDesc, &sInitData, &sRenderModel.pcIndexBuffer)))
										OutputDebugString(L"[OPENVR] Failed to create index buffer.");

									// delete vertex/index translation buffers
									delete[] asVertices;
									delete[] aunIndices;

									// set vertices/triangle count and tracked device index
									sRenderModel.unTriangleCount = pModel->unTriangleCount;
									sRenderModel.unVertexCount = pModel->unVertexCount;
									sRenderModel.unTrackedDeviceIndex = unTrackedDevice;

									// create geometry texture
									D3D11_TEXTURE2D_DESC sDesc;
									ZeroMemory(&sDesc, sizeof(sDesc));
									sDesc.Width = pTexture->unWidth;
									sDesc.Height = pTexture->unHeight;
									sDesc.MipLevels = sDesc.ArraySize = 1;
									sDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
									sDesc.SampleDesc.Count = 1;
									sDesc.Usage = D3D11_USAGE_DEFAULT;
									sDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
									D3D11_SUBRESOURCE_DATA sData;
									ZeroMemory(&sData, sizeof(sData));
									sData.pSysMem = pTexture->rubTextureMapData;
									sData.SysMemPitch = pTexture->unWidth * 4;
									if (FAILED(pcDevice->CreateTexture2D(&sDesc, &sData, &sRenderModel.pcTexture)))
										OutputDebugString(L"[OPENVR] Failed to create model texture.");

									if (sRenderModel.pcTexture)
									{
										// create texture shader resource view
										D3D11_SHADER_RESOURCE_VIEW_DESC sDesc;
										ZeroMemory(&sDesc, sizeof(sDesc));
										sDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
										sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
										sDesc.Texture2D.MostDetailedMip = 0;
										sDesc.Texture2D.MipLevels = 1;

										if ((FAILED(pcDevice->CreateShaderResourceView((ID3D11Resource*)sRenderModel.pcTexture, &sDesc, &sRenderModel.pcTextureSRV))))
											OutputDebugString(L"[OPENVR] Failed to create model texture shader resource view!");
									}

									// and add to vector
									m_asRenderModels.push_back(sRenderModel);

									vr::VRRenderModels()->FreeRenderModel(pModel);
									vr::VRRenderModels()->FreeTexture(pTexture);
								}
								m_bRenderModelsCreated = true;
							}
#pragma endregion
#pragma region init and render

							// get tracking pose... 
							(*m_ppHMD)->GetDeviceToAbsoluteTrackingPose(vr::ETrackingUniverseOrigin::TrackingUniverseStanding, 0, m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount);

							// convert HMD matrix to left handed matrix
							m_rmat4DevicePose[0] = GetLH(m_rTrackedDevicePose[0].mDeviceToAbsoluteTracking);

							// create view matrix by hmd matrix
							D3DXMatrixInverse(&m_sView, 0, &m_rmat4DevicePose[0]);

							// Set the input layout, buffers, sampler
							pcContext->IASetInputLayout(m_pcVLGeometry11);
							UINT stride = sizeof(TexturedNormalVertex);
							UINT offset = 0;

							pcContext->VSSetConstantBuffers(0, 1, &m_pcConstantBufferGeometry);
							pcContext->PSSetConstantBuffers(0, 1, &m_pcConstantBufferGeometry);
							pcContext->PSSetSamplers(0, 1, &m_pcSampler11);

							// Set primitive topology
							pcContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

							// set shaders
							pcContext->VSSetShader(m_pcVSGeometry11, NULL, 0);
							pcContext->PSSetShader(m_pcPSGeometry11, NULL, 0);

							// get render target views for both eyes and clear the depth stencil
							ID3D11RenderTargetView* pcRTV[2];
							pcRTV[0] = nullptr;
							pcRTV[1] = nullptr;
							for (int nEye = 0; nEye < 2; nEye++)
							{
								// get private data interface
								UINT dwSize = sizeof(ID3D11RenderTargetView*);
								(*m_ppcTexView11)[nEye]->GetPrivateData(PDIID_ID3D11TextureXD_RenderTargetView, &dwSize, (void*)&pcRTV[nEye]);

								// clear the depth stencil
								pcContext->ClearDepthStencilView(m_pcDSVGeometry11[nEye], D3D11_CLEAR_DEPTH, 1.0f, 0);
							}

							// loop through available render models, render
							unsigned uHand = 0;
							for (UINT unI = 0; unI < (UINT)m_asRenderModels.size(); unI++)
							{
								// set texture and buffers
								pcContext->PSSetShaderResources(0, 1, &m_asRenderModels[unI].pcTextureSRV);
								pcContext->IASetVertexBuffers(0, 1, &m_asRenderModels[unI].pcVertexBuffer, &stride, &offset);
								pcContext->IASetIndexBuffer(m_asRenderModels[unI].pcIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

								// set world matrix, first get left-handed
								m_rmat4DevicePose[m_asRenderModels[unI].unTrackedDeviceIndex] = GetLH(m_rTrackedDevicePose[m_asRenderModels[unI].unTrackedDeviceIndex].mDeviceToAbsoluteTracking);
								D3DXMatrixTranspose(&m_sGeometryConstants.sWorld, &m_rmat4DevicePose[m_asRenderModels[unI].unTrackedDeviceIndex]);

								// set mouse cursor for first controller device
								vr::TrackedDeviceClass eClass = vr::VRSystem()->GetTrackedDeviceClass((vr::TrackedDeviceIndex_t)unI + 1);
								if (eClass == vr::TrackedDeviceClass::TrackedDeviceClass_Controller)
								{
									// pose available
									m_sMenu.bHandPosesPresent = true;

									// get pose matrix
									UINT unIndex = (UINT)m_asRenderModels[unI].unTrackedDeviceIndex;
									m_sMenu.sPoseMatrix[uHand] = D3DXMATRIX(
										m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[0][0], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[1][0], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[2][0], 0.0,
										m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[0][1], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[1][1], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[2][1], 0.0,
										m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[0][2], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[1][2], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[2][2], 0.0,
										m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[0][3], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[1][3], m_rTrackedDevicePose[unIndex].mDeviceToAbsoluteTracking.m[2][3], 1.0f
										);

									// get position and set matrix translation to zero
									D3DXVECTOR3 sPosition = D3DXVECTOR3(m_sMenu.sPoseMatrix[uHand](3, 0), m_sMenu.sPoseMatrix[uHand](3, 1), m_sMenu.sPoseMatrix[uHand](3, 2));
									m_sMenu.sPosition[uHand].x = sPosition.x;
									m_sMenu.sPosition[uHand].y = sPosition.y;
									m_sMenu.sPosition[uHand].z = sPosition.z;
									m_sMenu.sPoseMatrix[uHand](3, 0) = 0.0f;
									m_sMenu.sPoseMatrix[uHand](3, 1) = 0.0f;
									m_sMenu.sPoseMatrix[uHand](3, 2) = 0.0f;

									uHand++; if (uHand > 1) uHand = 1;
								}

								// left + right
								for (int nEye = 0; nEye < 2; nEye++)
								{
									// set WVP matrix, update constant buffer							
									D3DXMATRIX sWorldViewProjection = m_rmat4DevicePose[m_asRenderModels[unI].unTrackedDeviceIndex] * m_sView * m_sToEye[nEye] * m_sProj[nEye];
									D3DXMatrixTranspose(&m_sGeometryConstants.sWorldViewProjection, &sWorldViewProjection);
									pcContext->UpdateSubresource(m_pcConstantBufferGeometry, 0, NULL, &m_sGeometryConstants, 0, 0);

									// set render target
									pcContext->OMSetRenderTargets(1, &pcRTV[nEye], m_pcDSVGeometry11[nEye]);

									// draw
									pcContext->DrawIndexed(m_asRenderModels[unI].unTriangleCount * 3, 0, 0);
								}
							}

							// release RTVs
							SAFE_RELEASE(pcRTV[0]);
							SAFE_RELEASE(pcRTV[1]);
#pragma endregion

							// set back device
							ApplyStateblock(pcContext, &sStateBlock);
						}
					}
#pragma endregion
#pragma region Render overlay
#pragma region Dashboard overlay
					if (vr::VROverlay() && vr::VROverlay()->IsOverlayVisible(m_ulOverlayHandle))
					{
						// backup all states
						D3DX11_STATE_BLOCK sStateBlock;
						CreateStateblock(pcContext, &sStateBlock);

						// clear all states, set targets
						ClearContextState(pcContext);

						// set viewport by render target size
						uint32_t unWidth, unHeight;
						(*m_ppHMD)->GetRecommendedRenderTargetSize(&unWidth, &unHeight);
						D3D11_VIEWPORT sViewport = {};
						sViewport.Width = (FLOAT)unWidth;
						sViewport.Height = (FLOAT)unHeight;
						sViewport.MaxDepth = 1.0f;
						pcContext->RSSetViewports(1, &sViewport);

						// create all bool
						bool bAllCreated = true;

						// create vertex shader
						if (!m_pcVertexShader11)
						{
							if (FAILED(CreateVertexShaderTechnique(pcDevice, &m_pcVertexShader11, &m_pcVertexLayout11, VertexShaderTechnique::PosUV2D)))
							{
								bAllCreated = false;
							}
						}
						// create pixel shader... 
						if (!m_pcPixelShader11)
						{
							if (FAILED(CreatePixelShaderEffect(pcDevice, &m_pcPixelShader11, PixelShaderTechnique::FullscreenSimple)))
								bAllCreated = false;
						}
						// Create vertex buffer
						if (!m_pcVertexBuffer11)
						{
							if (FAILED(CreateFullScreenVertexBuffer(pcDevice, &m_pcVertexBuffer11)))
								bAllCreated = false;
						}
						// create constant buffer
						if (!m_pcConstantBufferDirect11)
						{
							if (FAILED(CreateMatrixConstantBuffer(pcDevice, &m_pcConstantBufferDirect11)))
								bAllCreated = false;
						}
						// sampler ?
						if (!m_pcSamplerState)
						{
							// Create the sampler state
							D3D11_SAMPLER_DESC sSampDesc;
							ZeroMemory(&sSampDesc, sizeof(sSampDesc));
							sSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
							sSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
							sSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
							sSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
							sSampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
							sSampDesc.MinLOD = 0;
							sSampDesc.MaxLOD = D3D11_FLOAT32_MAX;
							if (FAILED(pcDevice->CreateSamplerState(&sSampDesc, &m_pcSamplerState)))
								bAllCreated = false;
						}

						// texture connected ?
						int nEye = 0;
						if ((m_ppcTexView11[nEye]) && (*m_ppcTexView11[nEye]))
						{
							// Set the input layout
							pcContext->IASetInputLayout(m_pcVertexLayout11);

							// Set vertex buffer
							UINT stride = sizeof(TexturedVertex);
							UINT offset = 0;
							pcContext->IASetVertexBuffers(0, 1, &m_pcVertexBuffer11, &stride, &offset);

							// Set constant buffer, first update it... scale and translate the left and right image
							D3DXMATRIX sProj;
							D3DXMatrixIdentity(&sProj);
							pcContext->UpdateSubresource((ID3D11Resource*)m_pcConstantBufferDirect11, 0, NULL, &sProj, 0, 0);
							pcContext->VSSetConstantBuffers(0, 1, &m_pcConstantBufferDirect11);

							// Set primitive topology
							pcContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

							if (bAllCreated)
							{
								if (!m_pcTex11Copy[nEye])
								{
									if (FAILED(CreateCopyTexture(pcDevice, m_pcDeviceTemporary, *m_ppcTexView11[nEye], &m_pcTex11Copy[nEye], &m_pcTex11Draw[nEye], &m_pcTex11DrawRTV[nEye], &m_pcTex11Shared[nEye])))
										return nullptr;
								}

								// set and clear render target
								FLOAT afColorRgba[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
								pcContext->ClearRenderTargetView(m_pcTex11DrawRTV[nEye], afColorRgba);
								pcContext->OMSetRenderTargets(1, &m_pcTex11DrawRTV[nEye], nullptr);

								// set texture, sampler state
								pcContext->PSSetShaderResources(0, 1, m_ppcTexView11[nEye]);
								pcContext->PSSetSamplers(0, 1, &m_pcSamplerState);

								// set shaders
								pcContext->VSSetShader(m_pcVertexShader11, 0, 0);
								pcContext->PSSetShader(m_pcPixelShader11, 0, 0);

								// Render a triangle
								pcContext->Draw(6, 0);

								// copy to copy texture
								pcContext->CopyResource(m_pcTex11Copy[nEye], m_pcTex11Draw[nEye]);

								// get shared handle.. for the Dashboard Overlay we CANNOT use m_pcTex11Shared for some reason !!
								IDXGIResource* pcDXGIResource(NULL);
								m_pcTex11Copy[nEye]->QueryInterface(__uuidof(IDXGIResource), (void**)&pcDXGIResource);
								HANDLE sharedHandle;
								if (pcDXGIResource)
								{
									pcDXGIResource->GetSharedHandle(&sharedHandle);
									pcDXGIResource->Release();
								}
								else OutputDebugString(L"Failed to query IDXGIResource.");

								ID3D11Resource* pcResourceShared;
								m_pcDeviceTemporary->OpenSharedResource(sharedHandle, __uuidof(ID3D11Resource), (void**)(&pcResourceShared));
								if (pcResourceShared)
								{
									// fill openvr texture struct
									vr::Texture_t sTexture = { (void*)pcResourceShared, vr::TextureType_DirectX, vr::ColorSpace_Gamma };
									vr::VROverlay()->SetOverlayTexture(m_ulOverlayHandle, &sTexture);
									pcResourceShared->Release();
								}

							}
						}

						// set back device
						ApplyStateblock(pcContext, &sStateBlock);
					}
					else
#pragma endregion
#pragma endregion
#pragma region Render
						if (!vr::VROverlay()->IsDashboardVisible())
						{
							// left + right
							for (int nEye = 0; nEye < 2; nEye++)
							{
								// texture connected ?
								if ((m_ppcTexView11[nEye]) && (*m_ppcTexView11[nEye]))
								{
									// are all textures created ?
									if (!m_pcTex11Copy[nEye])
									{
										if (FAILED(CreateCopyTexture(pcDevice, m_pcDeviceTemporary, *m_ppcTexView11[nEye], &m_pcTex11Copy[nEye], &m_pcTex11Draw[nEye], &m_pcTex11DrawRTV[nEye], &m_pcTex11Shared[nEye])))
											return nullptr;
									}

									// get the left game image texture resource
									ID3D11Resource* pcResource = nullptr;
									(*m_ppcTexView11[nEye])->GetResource(&pcResource);
									if (pcResource)
									{
										// copy resource
										pcContext->CopyResource(m_pcTex11Copy[nEye], pcResource);
										pcResource->Release();
									}
								}
							}
						}
#pragma endregion
				}
				else
					OutputDebugString(L"OpenVR: Current process has NOT the scene focus.");
		}

		// submit
		if (m_bInit)
		{
			vr::VRCompositor()->CompositorBringToFront();

			// left + right
			for (int nEye = 0; nEye < 2; nEye++)
			{
				// fill openvr texture struct
				vr::Texture_t sTexture = { (void*)m_pcTex11Shared[nEye], vr::TextureType_DirectX, vr::ColorSpace_Gamma };

				// submit left texture
				vr::VRCompositor()->Submit((vr::EVREye)nEye, &sTexture);
			}

			// force interleaved reprojection ?
			vr::VRCompositor()->ForceInterleavedReprojectionOn(m_bForceInterleavedReprojection);
		}

		// is a swapchain connected to the device ? call present in case
		IDXGISwapChain* pcSwapchain = nullptr;
		UINT unSize = sizeof(pcSwapchain);
		pcDevice->GetPrivateData(PDIID_ID3D11Device_IDXGISwapChain, &unSize, &pcSwapchain);
		if (pcSwapchain) pcSwapchain->Present(0, 0);

		// call post present handoff for better performance here
		vr::VRCompositor()->PostPresentHandoff();
	}

	// release d3d11 device + context... 
	pcContext->Release();
	pcDevice->Release();

	return nullptr;
}