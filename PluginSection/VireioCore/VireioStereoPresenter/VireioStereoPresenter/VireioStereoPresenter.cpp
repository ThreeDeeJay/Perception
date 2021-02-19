/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

Vireio Stereo Presenter - Vireio Perception Stereo Configuration Handler
Copyright (C) 2015 Denis Reischl

File <VireioStereoPresenter.cpp> and
Class <VireioStereoPresenter> :
Copyright (C) 2015 Denis Reischl

Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown
v2.0.4 onwards 2014 by Grant Bagwell, Simon Brown and Neil Schneider
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

#include"VireioStereoPresenter.h"

#define SAFE_RELEASE(a) if (a) { a->Release(); a = nullptr; }
#ifndef _TRACE
#define TRACE_UINT(a) { wchar_t buf[128]; wsprintf(buf, L"%s:%u", L#a, a); OutputDebugString(buf); }
#define TRACE_HEX(a) { wchar_t buf[128]; wsprintf(buf, L"%s:%x", L#a, a); OutputDebugString(buf); }
#define TRACE_LINE { wchar_t buf[128]; wsprintf(buf, L"LINE : %d", __LINE__); OutputDebugString(buf); }
#endif

#define INTERFACE_ID3D11DEVICE                                               6
#define INTERFACE_ID3D10DEVICE                                               7
#define INTERFACE_IDIRECT3DDEVICE9                                           8
#define INTERFACE_IDXGISWAPCHAIN                                             29

#define METHOD_IDXGISWAPCHAIN_PRESENT                                        8
#define	METHOD_IDIRECT3DDEVICE9_PRESENT                                      17

#define ENTRY_FONT 0

#define STEREO_L 0
#define STEREO_R 1

/// <summary>
/// Constructor
/// </summary>
StereoPresenter::StereoPresenter(ImGuiContext * sCtx) :AQU_Nodus(sCtx),
m_psStereoData(nullptr),
m_pcBackBufferView(nullptr),
m_pcVertexShader10(nullptr),
m_pcPixelShader10(nullptr),
m_pcVertexLayout10(nullptr),
m_pcVertexBuffer10(nullptr),
m_pcConstantBufferDirect10(nullptr),
m_pcDSGeometry11(nullptr),
m_pcDSVGeometry11(nullptr),
m_pcSampler11(nullptr),
m_bHotkeySwitch(false),
m_eStereoMode(VireioMonitorStereoModes::Vireio_Mono),
m_bMenu(false),
m_bMenuHotkeySwitch(false),
m_pcFontSegeo128(nullptr),
m_pbCinemaMode(nullptr)
{
	m_strFontName = std::string("PassionOne");
	m_unFontSelection = 0;

	// read or create the INI file
	char szFilePathINI[1024];
	GetCurrentDirectoryA(1024, szFilePathINI);
	strcat_s(szFilePathINI, "\\VireioPerception.ini");
	bool bFileExists = false;
	if (PathFileExistsA(szFilePathINI)) bFileExists = true;

	// read settings
	m_strFontName = GetIniFileSetting(m_strFontName, "Stereo Presenter", "strFontName", szFilePathINI, bFileExists);

	// TODO !! LOOP THROUGH AVAILABLE MENUES, SET SECONDARY VALUE ! (IN FIRST PROVOKING CALL)
	m_asSubMenu = std::vector<VireioSubMenu*>();
	m_asSubMenu.resize(32);
	ZeroMemory(&m_sMenuControl, sizeof(MenuControl));
	m_sMenuControl.nMenuIx = -1; // -1 = main menu
	ZeroMemory(&m_abMenuEvents[0], sizeof(VireioMenuEvent) * (int)VireioMenuEvent::NumberOfEvents);

	// create the main menu
	ZeroMemory(&m_sMainMenu, sizeof(VireioSubMenu));
	m_sMainMenu.strSubMenu = "Vireio Perception Profile Settings";

	// create entries for all 32 possible sub menu connections
	for (UINT unIx = 0; unIx < 32; unIx++)
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Inactive";
		sEntry.bIsActive = false;
		sEntry.eType = VireioMenuEntry::EntryType::Entry;
		m_sMainMenu.asEntries.push_back(sEntry);
	}

	// add an entry for the stereo presenter sub menu
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Stereo Presenter";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry;
		m_sMainMenu.asEntries.push_back(sEntry);
	}

	// create the presenter sub menu
	ZeroMemory(&m_sSubMenu, sizeof(VireioSubMenu));
	m_sSubMenu.strSubMenu = "Stereo Presenter";
#pragma region entry font
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Font";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_UInt;
		sEntry.unMinimum = 0;
		sEntry.unMaximum = 6;
		sEntry.unChangeSize = 1;
		sEntry.bValueEnumeration = true;
		{ std::string strEnum = "PassionOne"; sEntry.astrValueEnumeration.push_back(strEnum); }
		{ std::string strEnum = "ComicSansMS"; sEntry.astrValueEnumeration.push_back(strEnum); }
		{ std::string strEnum = "JacintoSans"; sEntry.astrValueEnumeration.push_back(strEnum); }
		{ std::string strEnum = "bitwise"; sEntry.astrValueEnumeration.push_back(strEnum); }
		{ std::string strEnum = "SegoeUI128"; sEntry.astrValueEnumeration.push_back(strEnum); }
		{ std::string strEnum = "UltimateGameplayer"; sEntry.astrValueEnumeration.push_back(strEnum); }
		{ std::string strEnum = "Videophreak"; sEntry.astrValueEnumeration.push_back(strEnum); }
		sEntry.punValue = &m_unFontSelection;
		sEntry.unValue = *sEntry.punValue;
		m_sSubMenu.asEntries.push_back(sEntry);
	}
#pragma endregion
}

/// <summary>
/// Destructor
/// </summary>
StereoPresenter::~StereoPresenter()
{
	if (m_pcFontSegeo128) delete m_pcFontSegeo128;

	SAFE_RELEASE(m_pcDSVGeometry11);
	SAFE_RELEASE(m_pcDSGeometry11);
	SAFE_RELEASE(m_pcSampler11);
	SAFE_RELEASE(m_pcVertexShader10);
	SAFE_RELEASE(m_pcPixelShader10);
	SAFE_RELEASE(m_pcVertexLayout10);
	SAFE_RELEASE(m_pcVertexBuffer10);
	SAFE_RELEASE(m_pcBackBufferView);
	SAFE_RELEASE(m_pcConstantBufferDirect10);
}

/// <summary>
/// Return the name of the  Stereo Presenter node
/// </summary>
const char* StereoPresenter::GetNodeType()
{
	return "Stereo Presenter";
}

/// <summary>
///Returns a global unique identifier for the Stereo Presenter node.
/// </summary>
UINT StereoPresenter::GetNodeTypeId()
{
#define DEVELOPER_IDENTIFIER 2006
#define MY_PLUGIN_IDENTIFIER 70
	return ((DEVELOPER_IDENTIFIER << 16) + MY_PLUGIN_IDENTIFIER);
}

/// <summary>
///Returns the name of the category for the Stereo Presenter node.
/// </summary>
LPWSTR StereoPresenter::GetCategory()
{
	return L"Vireio Core";
}

/// <summary>
///Returns a logo to be used for the Stereo Presenter node.
/// </summary>
HBITMAP StereoPresenter::GetLogo()
{
	HMODULE hModule = GetModuleHandle(L"VireioStereoPresenter.dll");
	HBITMAP hBitmap = LoadBitmap(hModule, MAKEINTRESOURCE(IMG_LOGO01));
	return hBitmap;
}

/// <summary>
/// Provides the name of the requested commander.
/// </summary>
LPWSTR StereoPresenter::GetCommanderName(DWORD dwCommanderIndex)
{
	return L"UNTITLED";
}

/// <summary>
/// Provides the name of the requested decommander.
/// </summary>
LPWSTR StereoPresenter::GetDecommanderName(DWORD dwDecommanderIndex)
{
	switch ((STP_Decommanders)dwDecommanderIndex)
	{
	case STP_Decommanders::Cinema:
		return VLink::Name(VLink::_L::StereoData);
	}

	return L"";
}

/// <summary>
/// Provides the type of the requested commander.
/// </summary>
DWORD StereoPresenter::GetCommanderType(DWORD dwCommanderIndex)
{
	return 0;
}

/// <summary>
/// Returns the plug type for the requested decommander.
/// </summary>
DWORD StereoPresenter::GetDecommanderType(DWORD dwDecommanderIndex)
{
	switch ((STP_Decommanders)dwDecommanderIndex)
	{
	case STP_Decommanders::Cinema:
		return VLink::Link(VLink::_L::StereoData);
	}

	return 0;
}

/// <summary>
///Provides the output pointer for the requested commander.
/// </summary>
void* StereoPresenter::GetOutputPointer(DWORD dwCommanderIndex)
{
	return nullptr;
}

/// <summary>
///Sets the input pointer for the requested decommander.
/// </summary>
void StereoPresenter::SetInputPointer(DWORD dwDecommanderIndex, void* pData)
{
	switch ((STP_Decommanders)dwDecommanderIndex)
	{
	case STP_Decommanders::Cinema:
		m_psStereoData = (StereoData*)pData;
		break;
	}
}

/// <summary>
///Stereo Presenter supports any call since it is on the end of the line.
///(verifies for supported methods in provoke call)
/// </summary>
bool StereoPresenter::SupportsD3DMethod(int nD3DVersion, int nD3DInterface, int nD3DMethod)
{
	return true;
}

/// <summary>
/// => Provoke
/// Handle Stereo Drawing.
/// </summary>
void* StereoPresenter::Provoke(void* pThis, int eD3D, int eD3DInterface, int eD3DMethod, DWORD dwNumberConnected, int& nProvokerIndex)
{
#ifdef _DEBUG_STP
	{ wchar_t buf[128]; wsprintf(buf, L"[STP] ifc %u mtd %u", eD3DInterface, eD3DMethod); OutputDebugString(buf); }
#endif

	// only present accepted
	bool bValid = false;
	if (((eD3DInterface == INTERFACE_IDXGISWAPCHAIN) && (eD3DMethod == METHOD_IDXGISWAPCHAIN_PRESENT)) ||
		((eD3DInterface == INTERFACE_IDIRECT3DDEVICE9) && (eD3DMethod == METHOD_IDIRECT3DDEVICE9_PRESENT))) bValid = true;
	if (!bValid) return nullptr;

	// update our global time
	static float fGlobalTime = 0.0f;
	static DWORD dwTimeStart = 0;
	DWORD dwTimeCur = GetTickCount();
	if (dwTimeStart == 0)
		dwTimeStart = dwTimeCur;
	fGlobalTime = (dwTimeCur - dwTimeStart) / 1000.0f;

	// menu vector initialized within cinema connection ?
	if (m_psStereoData)
	{
		if (!m_psStereoData->aasMenu)
			m_psStereoData->aasMenu = &m_asSubMenu;
	}

	// clear all previous menu events
	ZeroMemory(&m_abMenuEvents[0], sizeof(VireioMenuEvent) * (int)VireioMenuEvent::NumberOfEvents);

	// main menu update ?
	if ((m_sMainMenu.bOnChanged) && (!m_sMenuControl.eSelectionMovement))
	{
		m_sMainMenu.bOnChanged = false;

		// loop through entries
		for (size_t nIx = 0; nIx < m_sMainMenu.asEntries.size(); nIx++)
		{
			// entry index changed ?
			if (m_sMainMenu.asEntries[nIx].bOnChanged)
			{
				m_sMainMenu.asEntries[nIx].bOnChanged = false;

				// set new menu index.. selection to zero
				m_sMenuControl.nMenuIx = (INT)nIx;
				m_sMenuControl.unSelectionFormer = m_sMenuControl.unSelection = 0;
			}
		}
	}

	// sub menu update ?
	if ((m_sSubMenu.bOnChanged) && (!m_sMenuControl.eSelectionMovement))
	{
		m_sSubMenu.bOnChanged = false;

		// exit ?
		if (m_sSubMenu.bOnExit)
			m_sMenuControl.nMenuIx = -1;

		// loop through entries
		for (size_t nIx = 0; nIx < m_sSubMenu.asEntries.size(); nIx++)
		{
			// entry index changed ?
			if (m_sSubMenu.asEntries[nIx].bOnChanged)
			{
				m_sSubMenu.asEntries[nIx].bOnChanged = false;

				// font ?
				if (nIx == ENTRY_FONT)
				{
					// get device and context
					ID3D11Device* pcDevice = nullptr;
					ID3D11DeviceContext* pcContext = nullptr;
					HRESULT nHr = S_OK;
					if ((eD3DInterface == INTERFACE_IDXGISWAPCHAIN) && (eD3DMethod == METHOD_IDXGISWAPCHAIN_PRESENT))
						nHr = GetDeviceAndContext((IDXGISwapChain*)pThis, &pcDevice, &pcContext);
					else
					{
						if (m_psStereoData)
						{
							if (m_psStereoData->pcTex11DrawSRV[0])
								m_psStereoData->pcTex11DrawSRV[0]->GetDevice(&pcDevice);
							if (pcDevice)
								pcDevice->GetImmediateContext(&pcContext);
							else nHr = E_FAIL;
							if (!pcContext) nHr = E_FAIL;
						}
						else
							nHr = E_FAIL;
					}
					if (SUCCEEDED(nHr))
					{
						HRESULT nHr;
						// get base directory and convert to wchar_t string
						std::wstring strVireioPathW = GetBaseDir();
						std::string strVireioPath;
						for (wchar_t c : strVireioPathW) strVireioPath += (char)c;

						// add file path
						strVireioPath += "..//..//font//";
						strVireioPath += m_sSubMenu.asEntries[nIx].astrValueEnumeration[m_sSubMenu.asEntries[nIx].unValue];
						strVireioPath += ".spritefont";
						OutputDebugStringA(strVireioPath.c_str());

						// create font, make backup
						VireioFont* pcOldFont = m_pcFontSegeo128;
						m_pcFontSegeo128 = new VireioFont(pcDevice, pcContext, strVireioPath.c_str(), 128.0f, 1.0f, nHr, 1);
						if (FAILED(nHr))
						{
							delete m_pcFontSegeo128; m_pcFontSegeo128 = pcOldFont;
						}
						else
						{
							// set new font name
							m_strFontName = m_sSubMenu.asEntries[nIx].astrValueEnumeration[m_sSubMenu.asEntries[nIx].unValue];

							// write to ini file
							char szFilePathINI[1024];
							GetCurrentDirectoryA(1024, szFilePathINI);
							strcat_s(szFilePathINI, "\\VireioPerception.ini");
							WritePrivateProfileStringA("Stereo Presenter", "strFontName", m_strFontName.c_str(), szFilePathINI);
						}
					}
					SAFE_RELEASE(pcDevice);
					SAFE_RELEASE(pcContext);
				}

			}
		}
	}

	/// => Handle Controller 
	XINPUT_STATE sControllerState;
	bool bControllerAttached = false;
	ZeroMemory(&sControllerState, sizeof(XINPUT_STATE));
	if (XInputGetState(0, &sControllerState) == ERROR_SUCCESS)
	{
		bControllerAttached = true;
	}

	if (true)
	{
#pragma region menu hotkeys
		static bool bReleased = true;
		static bool s_bOnMenu = false;

		// keyboard menu on/off event + get hand poses
		UINT uIxHandPoses = 0, uIxPoseRequest = 0;
		s_bOnMenu = GetAsyncKeyState(VK_LCONTROL) && GetAsyncKeyState(0x51);
		for (UINT unIx = 0; unIx < 32; unIx++)
		{
			// set menu bool event
			if (m_asSubMenu[unIx])
			{
				if (m_asSubMenu[unIx]->bOnBack)
				{
					// main menu ? exit
					if ((m_sMenuControl.nMenuIx == -1) && (!m_sMenuControl.eSelectionMovement))
						s_bOnMenu = true;
					else
						m_abMenuEvents[VireioMenuEvent::OnExit] = TRUE;

					m_asSubMenu[unIx]->bOnBack = false;
				}

				// hand poses ?
				if (m_asSubMenu[unIx]->bHandPosesPresent)
					uIxHandPoses = unIx;
				if (m_asSubMenu[unIx]->bHandPosesRequest)
					uIxPoseRequest = unIx;
			}
		}
		if ((m_asSubMenu[uIxHandPoses]) && (m_asSubMenu[uIxPoseRequest]))
		{
			// copy the hand pose data to the request node
			m_asSubMenu[uIxPoseRequest]->sPoseMatrix[0] = m_asSubMenu[uIxHandPoses]->sPoseMatrix[0];
			m_asSubMenu[uIxPoseRequest]->sPoseMatrix[1] = m_asSubMenu[uIxHandPoses]->sPoseMatrix[1];
			m_asSubMenu[uIxPoseRequest]->sPosition[0] = m_asSubMenu[uIxHandPoses]->sPosition[0];
			m_asSubMenu[uIxPoseRequest]->sPosition[1] = m_asSubMenu[uIxHandPoses]->sPosition[1];
		}

		// static hotkeys :  LCTRL+Q - toggle vireio menu
		//                   F11 - toggle full immersive mode
		//                   F12 - toggle stereo output
		if (GetAsyncKeyState(VK_F12))
		{
			m_bHotkeySwitch = true;
		}
		else
			if (s_bOnMenu)
			{
				m_bMenuHotkeySwitch = true;
			}
			else
				if (m_bMenuHotkeySwitch)
				{
					m_bMenuHotkeySwitch = false;
					m_bMenu = !m_bMenu;
					for (UINT unIx = 0; unIx < 32; unIx++)
					{
						// set sub menu active if menu is active
						if (m_asSubMenu[unIx])
							m_asSubMenu[unIx]->bIsActive = m_bMenu;
					}
				}
				else
					if (m_bHotkeySwitch)
					{
						if (m_eStereoMode) m_eStereoMode = VireioMonitorStereoModes::Vireio_Mono; else m_eStereoMode = VireioMonitorStereoModes::Vireio_SideBySide;
						m_bHotkeySwitch = false;
					}
					else
						bReleased = true;

		// handle full immersive switch, first on gamepad
		static bool bFullImmersiveSwitch = false, bFullImmersiveSwitchKb = false;
		if (bControllerAttached)
		{
			if ((sControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) && (sControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB))
			{
				bFullImmersiveSwitch = true;
			}
			else
				if (bFullImmersiveSwitch)
				{
					bFullImmersiveSwitch = false;
					// loop through sub menues
					for (UINT unIx = 0; unIx < 32; unIx++)
					{
						// set bool events
						if (m_asSubMenu[unIx]) m_asSubMenu[unIx]->bOnFullImmersive = true;
					}
				}
		}

		// ...then on keyboard (F11)
		if (GetAsyncKeyState(VK_F12))
		{
			bFullImmersiveSwitchKb = true;
		}
		else
			if (bFullImmersiveSwitchKb)
			{
				bFullImmersiveSwitchKb = false;
				// loop through sub menues
				for (UINT unIx = 0; unIx < 32; unIx++)
				{
					// set bool events
					if (m_asSubMenu[unIx]) m_asSubMenu[unIx]->bOnFullImmersive = true;
				}
			}

#pragma endregion
#pragma region menu events
		// menu is shown ?
		if (m_bMenu)
		{
			// handle controller
			if (bControllerAttached)
			{
				if (sControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
				{
					m_abMenuEvents[VireioMenuEvent::OnExit] = TRUE;
				}
				if (sControllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A)
				{
					m_abMenuEvents[VireioMenuEvent::OnAccept] = TRUE;
				}
				if (sControllerState.Gamepad.sThumbLY > 28000)
					m_abMenuEvents[VireioMenuEvent::OnUp] = TRUE;
				if (sControllerState.Gamepad.sThumbLY < -28000)
					m_abMenuEvents[VireioMenuEvent::OnDown] = TRUE;
				if (sControllerState.Gamepad.sThumbLX > 28000)
					m_abMenuEvents[VireioMenuEvent::OnRight] = TRUE;
				if (sControllerState.Gamepad.sThumbLX < -28000)
					m_abMenuEvents[VireioMenuEvent::OnLeft] = TRUE;

			}

			// loop through sub menues
			for (UINT unIx = 0; unIx < 32; unIx++)
			{
				// set bool events
				if (m_asSubMenu[unIx])
				{
					if (m_asSubMenu[unIx]->bOnUp) m_abMenuEvents[VireioMenuEvent::OnUp] = TRUE;
					if (m_asSubMenu[unIx]->bOnDown) m_abMenuEvents[VireioMenuEvent::OnDown] = TRUE;
					if (m_asSubMenu[unIx]->bOnLeft) m_abMenuEvents[VireioMenuEvent::OnLeft] = TRUE;
					if (m_asSubMenu[unIx]->bOnRight) m_abMenuEvents[VireioMenuEvent::OnRight] = TRUE;
					if (m_asSubMenu[unIx]->bOnAccept) m_abMenuEvents[VireioMenuEvent::OnAccept] = TRUE;

					// clear events
					m_asSubMenu[unIx]->bOnUp = false;
					m_asSubMenu[unIx]->bOnDown = false;
					m_asSubMenu[unIx]->bOnLeft = false;
					m_asSubMenu[unIx]->bOnRight = false;
					m_asSubMenu[unIx]->bOnAccept = false;
					m_asSubMenu[unIx]->bOnBack = false;
				}
			}
#pragma endregion
#pragma region menu update/render

			// update
			UpdateMenu(fGlobalTime);

			// get device and context
			ID3D11Device* pcDevice = nullptr;
			ID3D11DeviceContext* pcContext = nullptr;
			HRESULT nHr = S_OK;
			if ((eD3DInterface == INTERFACE_IDXGISWAPCHAIN) && (eD3DMethod == METHOD_IDXGISWAPCHAIN_PRESENT))
				nHr = GetDeviceAndContext((IDXGISwapChain*)pThis, &pcDevice, &pcContext);
			else
			{
				if (m_psStereoData)
				{
					if (m_psStereoData->pcTex11DrawSRV[0])
						m_psStereoData->pcTex11DrawSRV[0]->GetDevice(&pcDevice);
					if (pcDevice)
						pcDevice->GetImmediateContext(&pcContext);
					else nHr = E_FAIL;
					if (!pcContext) nHr = E_FAIL;
				}
				else
					nHr = E_FAIL;
			}
			if (FAILED(nHr))
			{
				// release frame texture+view
				if (pcDevice) { pcDevice->Release(); pcDevice = nullptr; }
				if (pcContext) { pcContext->Release(); pcContext = nullptr; }
				return nullptr;
			}
			// create the depth stencil... if D3D11
			if ((eD3DInterface == INTERFACE_IDXGISWAPCHAIN) && (eD3DMethod == METHOD_IDXGISWAPCHAIN_PRESENT) && (!m_pcDSGeometry11))
			{
				ID3D11Texture2D* pcBackBuffer = nullptr;
				((IDXGISwapChain*)pThis)->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pcBackBuffer);

				if (pcBackBuffer)
				{
					D3D11_TEXTURE2D_DESC sDesc;
					pcBackBuffer->GetDesc(&sDesc);
					pcBackBuffer->Release();

					// Create depth stencil texture
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
					if (FAILED(pcDevice->CreateTexture2D(&descDepth, NULL, &m_pcDSGeometry11)))
						OutputDebugString(L"[STP] Failed to create depth stencil.");

					// Create the depth stencil view
					D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
					ZeroMemory(&descDSV, sizeof(descDSV));
					descDSV.Format = descDepth.Format;
					descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
					descDSV.Texture2D.MipSlice = 0;
					if (FAILED(pcDevice->CreateDepthStencilView(m_pcDSGeometry11, &descDSV, &m_pcDSVGeometry11)))
						OutputDebugString(L"[STP] Failed to create depth stencil view.");
				}
			}

			// get the viewport
			UINT dwNumViewports = 1;
			D3D11_VIEWPORT psViewport[16];
			pcContext->RSGetViewports(&dwNumViewports, psViewport);

			// backup all states
			D3DX11_STATE_BLOCK sStateBlock;
			CreateStateblock(pcContext, &sStateBlock);

			// clear all states, set targets
			ClearContextState(pcContext);

			// set the menu texture (if present)
			if (m_psStereoData)
			{
				if (m_psStereoData->pcTexMenuRTV)
				{
					// set render target
					pcContext->OMSetRenderTargets(1, &m_psStereoData->pcTexMenuRTV, NULL);

					// set viewport
					D3D11_VIEWPORT sViewport = {};
					sViewport.TopLeftX = 0;
					sViewport.TopLeftY = 0;
					sViewport.Width = 1024;
					sViewport.Height = 1024;
					sViewport.MinDepth = 0.0f;
					sViewport.MaxDepth = 1.0f;
					pcContext->RSSetViewports(1, &sViewport);

					// clear render target...zero alpha
					FLOAT afColorRgba[4] = { 0.5f, 0.4f, 0.2f, 0.4f };
					pcContext->ClearRenderTargetView(m_psStereoData->pcTexMenuRTV, afColorRgba);
				}
			}
			else
				if ((eD3DInterface == INTERFACE_IDXGISWAPCHAIN) && (eD3DMethod == METHOD_IDXGISWAPCHAIN_PRESENT))
				{
					// set first active render target - the stored back buffer - get the stored private data view
					ID3D11Texture2D* pcBackBuffer = nullptr;
					((IDXGISwapChain*)pThis)->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pcBackBuffer);
					ID3D11RenderTargetView* pcView = nullptr;
					UINT dwSize = sizeof(pcView);
					pcBackBuffer->GetPrivateData(PDIID_ID3D11TextureXD_RenderTargetView, &dwSize, (void*)&pcView);
					if (dwSize)
					{
						pcContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&pcView, m_pcDSVGeometry11);
						pcView->Release();
					}
					else
					{
						// create render target view for the back buffer
						ID3D11RenderTargetView* pcRTV = nullptr;
						pcDevice->CreateRenderTargetView(pcBackBuffer, NULL, &pcRTV);
						if (pcRTV)
						{
							pcBackBuffer->SetPrivateDataInterface(PDIID_ID3D11TextureXD_RenderTargetView, pcRTV);
							pcRTV->Release();
						}
					}
					pcContext->RSSetViewports(dwNumViewports, psViewport);
					pcBackBuffer->Release();

					// clear the depth stencil
					pcContext->ClearDepthStencilView(m_pcDSVGeometry11, D3D11_CLEAR_DEPTH, 1.0f, 0);
				}

			// create the font class if not present 
			nHr = S_OK;
			if (!m_pcFontSegeo128)
			{
				// get base directory and convert to wchar_t string
				std::wstring strVireioPathW = GetBaseDir();
				std::string strVireioPath;
				for (wchar_t c : strVireioPathW) strVireioPath += (char)c;

				// add file path
				strVireioPath += "..//..//font//";
				strVireioPath += m_strFontName;
				strVireioPath += ".spritefont";
				OutputDebugStringA(strVireioPath.c_str());

				// create font
				m_pcFontSegeo128 = new VireioFont(pcDevice, pcContext, strVireioPath.c_str(), 128.0f, 1.0f, nHr, 1);
			}
			if (FAILED(nHr)) { delete m_pcFontSegeo128; m_pcFontSegeo128 = nullptr; }

			// render text (if font present)
			if (m_pcFontSegeo128)
			{
				m_pcFontSegeo128->SetTextAttributes(0.0f, 0.2f, 0.0001f);

				// set additional tremble for "accepted" event
				float fDepthTremble = 0.0f;
				if (m_sMenuControl.eSelectionMovement == MenuControl::SelectionMovement::Accepted)
				{
					float fActionTimeElapsed = (fGlobalTime - m_sMenuControl.fActionStartTime) / m_sMenuControl.fActionTime;
					fDepthTremble = sin(fActionTimeElapsed * PI_F) * -3.0f;
				}

				m_pcFontSegeo128->ToRender(pcContext, fGlobalTime, m_sMenuControl.fYOrigin, 30.0f, fDepthTremble);
				RenderMenu(pcDevice, pcContext);
			}
			else OutputDebugString(L"Failed to create font!");

			// set back device
			ApplyStateblock(pcContext, &sStateBlock);

			if (pcDevice) { pcDevice->Release(); pcDevice = nullptr; }
			if (pcContext) { pcContext->Release(); pcContext = nullptr; }
		}
#pragma endregion
#pragma region draw stereo (optionally)

		// draw stereo target to screen (optionally)
		if ((m_eStereoMode) && (eD3DInterface == INTERFACE_IDXGISWAPCHAIN) && (eD3DMethod == METHOD_IDXGISWAPCHAIN_PRESENT))
		{
			// DX 11
			if (m_psStereoData)
			{
				// get device and context
				ID3D11Device* pcDevice = nullptr;
				ID3D11DeviceContext* pcContext = nullptr;
				if (FAILED(GetDeviceAndContext((IDXGISwapChain*)pThis, &pcDevice, &pcContext)))
				{
					// release frame texture+view
					if (pcDevice) { pcDevice->Release(); pcDevice = nullptr; }
					if (pcContext) { pcContext->Release(); pcContext = nullptr; }
					return nullptr;
				}

				// get the viewport
				UINT dwNumViewports = 1;
				D3D11_VIEWPORT psViewport[16];
				pcContext->RSGetViewports(&dwNumViewports, psViewport);

				// backup all states
				D3DX11_STATE_BLOCK sStateBlock;
				CreateStateblock(pcContext, &sStateBlock);

				// clear all states, set targets
				ClearContextState(pcContext);

				// set first active render target - the stored back buffer - get the stored private data view
				ID3D11Texture2D* pcBackBuffer = nullptr;
				((IDXGISwapChain*)pThis)->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pcBackBuffer);
				ID3D11RenderTargetView* pcView = nullptr;
				UINT dwSize = sizeof(pcView);
				pcBackBuffer->GetPrivateData(PDIID_ID3D11TextureXD_RenderTargetView, &dwSize, (void*)&pcView);
				if (dwSize)
				{
					pcContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&pcView, m_pcDSVGeometry11);
					pcView->Release();
				}
				else
				{
					// create render target view for the back buffer
					ID3D11RenderTargetView* pcRTV = nullptr;
					pcDevice->CreateRenderTargetView(pcBackBuffer, NULL, &pcRTV);
					if (pcRTV)
					{
						pcBackBuffer->SetPrivateDataInterface(PDIID_ID3D11TextureXD_RenderTargetView, pcRTV);
						pcRTV->Release();
					}
				}
				pcContext->RSSetViewports(dwNumViewports, psViewport);
				pcBackBuffer->Release();

				// clear the depth stencil
				pcContext->ClearDepthStencilView(m_pcDSVGeometry11, D3D11_CLEAR_DEPTH, 1.0f, 0);

				// create all bool
				bool bAllCreated = true;

				// create vertex shader
				if (!m_pcVertexShader11)
				{
					if (FAILED(CreateVertexShaderTechnique(pcDevice, &m_pcVertexShader11, &m_pcVertexLayout11, VertexShaderTechnique::PosUV2D)))
						bAllCreated = false;
				}
				// create pixel shader... TODO !! add option to switch output
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
					if (FAILED(CreateGeometryConstantBuffer(pcDevice, &m_pcConstantBufferDirect11, (UINT)sizeof(GeometryConstantBuffer))))
						bAllCreated = false;
				}

				if (bAllCreated)
				{
					// left/right eye
					for (const int nEye : {STEREO_L, STEREO_R})
					{
						// Set the input layout
						pcContext->IASetInputLayout(m_pcVertexLayout11);

						// Set vertex buffer
						UINT stride = sizeof(TexturedVertex);
						UINT offset = 0;
						pcContext->IASetVertexBuffers(0, 1, &m_pcVertexBuffer11, &stride, &offset);

						// Set constant buffer, first update it... scale and translate the left and right image
						D3DXMATRIX sScale;
						D3DXMatrixScaling(&sScale, 0.5f, 1.0f, 1.0f);
						D3DXMATRIX sTrans;
						if (nEye == 0)
							D3DXMatrixTranslation(&sTrans, -0.5f, 0.0f, 0.0f);
						else
							D3DXMatrixTranslation(&sTrans, 0.5f, 0.0f, 0.0f);
						D3DXMatrixTranspose(&sTrans, &sTrans);
						D3DXMATRIX sProj;
						D3DXMatrixMultiply(&sProj, &sTrans, &sScale);
						pcContext->UpdateSubresource((ID3D11Resource*)m_pcConstantBufferDirect11, 0, NULL, &sProj, 0, 0);
						pcContext->VSSetConstantBuffers(0, 1, &m_pcConstantBufferDirect11);

						// Set primitive topology
						pcContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

						// set texture
						pcContext->PSSetShaderResources(0, 1, &m_psStereoData->pcTex11DrawSRV[nEye]);

						// set shaders
						pcContext->VSSetShader(m_pcVertexShader11, 0, 0);
						pcContext->PSSetShader(m_pcPixelShader11, 0, 0);

						// Render a triangle
						pcContext->Draw(6, 0);
					}
				}

				// set back device
				ApplyStateblock(pcContext, &sStateBlock);

				if (pcDevice) { pcDevice->Release(); pcDevice = nullptr; }
				if (pcContext) { pcContext->Release(); pcContext = nullptr; }
			}
		}
#pragma endregion
	}

	return nullptr;
}

/// <summary>
/// Menu render method.
/// </summary>
void StereoPresenter::RenderMenu(ID3D11Device* pcDevice, ID3D11DeviceContext* pcContext)
{
	// main menu ?
	if (m_sMenuControl.nMenuIx == -1)
	{
		RenderSubMenu(pcDevice, pcContext, &m_sMainMenu);
	}
	else
		// connected sub menu ?
		if ((m_sMenuControl.nMenuIx >= 0) && (m_sMenuControl.nMenuIx < 32))
		{
			if (!m_asSubMenu[m_sMenuControl.nMenuIx])
				OutputDebugString(L"[STP] Fatal menu error -> wrong menu index !!");
			else
				RenderSubMenu(pcDevice, pcContext, m_asSubMenu[m_sMenuControl.nMenuIx]);
		}
		else
			// stereo presenter sub menu ?
			if (m_sMenuControl.nMenuIx == 32)
				RenderSubMenu(pcDevice, pcContext, &m_sSubMenu);
			else
				OutputDebugString(L"[STP] Fatal menu error -> wrong menu index !!");
}

/// <summary>
/// Renders a sub menu.
/// </summary>
void StereoPresenter::RenderSubMenu(ID3D11Device* pcDevice, ID3D11DeviceContext* pcContext, VireioSubMenu* psSubMenu)
{
	// render title
	m_pcFontSegeo128->RenderTextLine(pcDevice, pcContext, psSubMenu->strSubMenu.c_str());
	m_pcFontSegeo128->Enter();

	// loop through entries
	for (size_t nEntryIx = 0; nEntryIx < psSubMenu->asEntries.size(); nEntryIx++)
	{
		// entry is inactive ?
		if (!psSubMenu->asEntries[nEntryIx].bIsActive) continue;

		// create an output stream based on entry string
		std::stringstream strOutput = std::stringstream();
		strOutput << psSubMenu->asEntries[nEntryIx].strEntry;

		// switch by entry type
		switch (psSubMenu->asEntries[nEntryIx].eType)
		{
		case VireioMenuEntry::Entry_Bool:
			if (psSubMenu->asEntries[nEntryIx].bValue)
				strOutput << " - True ";
			else
				strOutput << " - False ";
			break;
		case VireioMenuEntry::Entry_Int:
			strOutput << " : " << psSubMenu->asEntries[nEntryIx].nValue;
			break;
		case VireioMenuEntry::Entry_UInt:
			if (psSubMenu->asEntries[nEntryIx].bValueEnumeration)
			{
				// render the enumeration string
				UINT unIx = psSubMenu->asEntries[nEntryIx].unValue;
				if (unIx < (UINT)psSubMenu->asEntries[nEntryIx].astrValueEnumeration.size())
					strOutput << " : " << psSubMenu->asEntries[nEntryIx].astrValueEnumeration[unIx];
			}
			else
			{
				strOutput << " : " << psSubMenu->asEntries[nEntryIx].unValue;
			}
			break;
		case VireioMenuEntry::Entry_Float:
			strOutput << " : " << psSubMenu->asEntries[nEntryIx].fValue;
			break;
		case VireioMenuEntry::Entry:
			break;
		default:
			strOutput << "ERROR";
			break;
		}

		// render the text line
		m_pcFontSegeo128->RenderTextLine(pcDevice, pcContext, strOutput.str().c_str());
	}
}

/// <summary>
/// Update sub menu based on menu index.
/// </summary>
void StereoPresenter::UpdateMenu(float fGlobalTime)
{
	// main menu ?
	if (m_sMenuControl.nMenuIx == -1)
	{
		UpdateSubMenu(&m_sMainMenu, fGlobalTime);
	}
	else
		// connected sub menu ?
		if ((m_sMenuControl.nMenuIx >= 0) && (m_sMenuControl.nMenuIx < 32))
		{
			if (!m_asSubMenu[m_sMenuControl.nMenuIx])
				OutputDebugString(L"[STP] Fatal menu error -> wrong menu index !!");
			else
				UpdateSubMenu(m_asSubMenu[m_sMenuControl.nMenuIx], fGlobalTime);
		}
		else
			// stereo presenter sub menu ?
			if (m_sMenuControl.nMenuIx == 32)
				UpdateSubMenu(&m_sSubMenu, fGlobalTime);
			else
				OutputDebugString(L"[STP] Fatal menu error -> wrong menu index !!");
}

/// <summary>
/// Updates a sub menu.
/// </summary>
void StereoPresenter::UpdateSubMenu(VireioSubMenu* psSubMenu, float fGlobalTime)
{
	// update the main menu entries by connected sub menues
	if (m_sMenuControl.nMenuIx == -1)
	{
		for (UINT unIx = 0; unIx < 32; unIx++)
		{
			if ((m_asSubMenu[unIx]) && (!m_sMainMenu.asEntries[unIx].bIsActive))
			{
				m_sMainMenu.asEntries[unIx].bIsActive = true;
				m_sMainMenu.asEntries[unIx].strEntry = m_asSubMenu[unIx]->strSubMenu;
			}
		}
	}

	// update the sub (or main) menu active entries number
	psSubMenu->unActiveEntries = 0;
	for (size_t nIx = 0; nIx < psSubMenu->asEntries.size(); nIx++)
	{
		if (psSubMenu->asEntries[nIx].bIsActive)
			psSubMenu->unActiveEntries++;
	}

	// set the menu y origin
	m_sMenuControl.fYOrigin = -2.8f + (float)m_sMenuControl.unSelection * -1.4f;

	if (m_sMenuControl.eSelectionMovement)
	{
		// how much action time elapsed for the movement ?
		float fActionTimeElapsed = (fGlobalTime - m_sMenuControl.fActionStartTime) / m_sMenuControl.fActionTime;

		// is the movement done ?
		if (fActionTimeElapsed > 1.0f)
		{
			fActionTimeElapsed = 1.0f;
			m_sMenuControl.eSelectionMovement = MenuControl::SelectionMovement::Standing;
		}

		// compute the movement origin
		float fOldOrigin = -2.8f + (float)m_sMenuControl.unSelectionFormer * -1.4f;
		float fYOriginMovement = (fOldOrigin - m_sMenuControl.fYOrigin) * (1.0f - sin(fActionTimeElapsed * PI_F * 0.5f));
		m_sMenuControl.fYOrigin += fYOriginMovement;
	}
	else
	{
		// no selection movement, events possible...up
		if ((m_abMenuEvents[VireioMenuEvent::OnUp]) && (m_sMenuControl.unSelection > 0))
		{
			// set event
			m_sMenuControl.eSelectionMovement = MenuControl::SelectionMovement::MovesUp;
			m_sMenuControl.fActionTime = 0.3f;
			m_sMenuControl.fActionStartTime = fGlobalTime;

			// set selection
			m_sMenuControl.unSelectionFormer = m_sMenuControl.unSelection;
			m_sMenuControl.unSelection--;
		}
		// down
		if ((m_abMenuEvents[VireioMenuEvent::OnDown]) && (m_sMenuControl.unSelection < (UINT)(psSubMenu->unActiveEntries - 1)))
		{
			// set event
			m_sMenuControl.eSelectionMovement = MenuControl::SelectionMovement::MovesDown;
			m_sMenuControl.fActionTime = 0.3f;
			m_sMenuControl.fActionStartTime = fGlobalTime;

			// set selection
			m_sMenuControl.unSelectionFormer = m_sMenuControl.unSelection;
			m_sMenuControl.unSelection++;
		}
		// left
		if (m_abMenuEvents[VireioMenuEvent::OnRight])
		{
			// set event
			m_sMenuControl.eSelectionMovement = MenuControl::SelectionMovement::TriggerRight;
			m_sMenuControl.fActionTime = 0.1f;
			m_sMenuControl.fActionStartTime = fGlobalTime;

			// set former selection to actual
			m_sMenuControl.unSelectionFormer = m_sMenuControl.unSelection;

			// get index
			UINT unIx = GetSelection(psSubMenu, m_sMenuControl.unSelection);

			// type ?
			switch (psSubMenu->asEntries[unIx].eType)
			{
			case VireioMenuEntry::EntryType::Entry_Float:
				// change value, set event
				*psSubMenu->asEntries[unIx].pfValue += psSubMenu->asEntries[unIx].fChangeSize;
				psSubMenu->asEntries[unIx].bOnChanged = true;
				psSubMenu->bOnChanged = true;

				// clamp value
				if (*psSubMenu->asEntries[unIx].pfValue > psSubMenu->asEntries[unIx].fMaximum)
					*psSubMenu->asEntries[unIx].pfValue = psSubMenu->asEntries[unIx].fMaximum;

				// set the menu entry value
				psSubMenu->asEntries[unIx].fValue = *psSubMenu->asEntries[unIx].pfValue;
				break;
			case VireioMenuEntry::EntryType::Entry_UInt:
				if (!psSubMenu->asEntries[unIx].bValueEnumeration)
				{
					// change value, set event
					*psSubMenu->asEntries[unIx].punValue += psSubMenu->asEntries[unIx].unChangeSize;
					psSubMenu->asEntries[unIx].bOnChanged = true;
					psSubMenu->bOnChanged = true;

					// clamp value
					if (*psSubMenu->asEntries[unIx].punValue > psSubMenu->asEntries[unIx].unMaximum)
						*psSubMenu->asEntries[unIx].punValue = psSubMenu->asEntries[unIx].unMaximum;

					// set the menu entry value
					psSubMenu->asEntries[unIx].unValue = *psSubMenu->asEntries[unIx].punValue;
				}
				break;
			case VireioMenuEntry::EntryType::Entry_Int:
				// change value, set event
				*psSubMenu->asEntries[unIx].pnValue += psSubMenu->asEntries[unIx].nChangeSize;
				psSubMenu->asEntries[unIx].bOnChanged = true;
				psSubMenu->bOnChanged = true;

				// clamp value
				if (*psSubMenu->asEntries[unIx].pnValue > psSubMenu->asEntries[unIx].nMaximum)
					*psSubMenu->asEntries[unIx].pnValue = psSubMenu->asEntries[unIx].nMaximum;

				// set the menu entry value
				psSubMenu->asEntries[unIx].nValue = *psSubMenu->asEntries[unIx].pnValue;
				break;
			case VireioMenuEntry::EntryType::Entry_Bool:
				// change value, set event
				*psSubMenu->asEntries[unIx].pbValue = !(*psSubMenu->asEntries[unIx].pbValue);
				psSubMenu->asEntries[unIx].bOnChanged = true;
				psSubMenu->bOnChanged = true;

				// set the menu entry value
				psSubMenu->asEntries[unIx].bValue = *psSubMenu->asEntries[unIx].pbValue;
				break;
			}
		}

		// right
		if (m_abMenuEvents[VireioMenuEvent::OnLeft])
		{
			// set event
			m_sMenuControl.eSelectionMovement = MenuControl::SelectionMovement::TriggerLeft;
			m_sMenuControl.fActionTime = 0.1f;
			m_sMenuControl.fActionStartTime = fGlobalTime;

			// set former selection to actual
			m_sMenuControl.unSelectionFormer = m_sMenuControl.unSelection;

			// get index
			UINT unIx = GetSelection(psSubMenu, m_sMenuControl.unSelection);

			// type ?
			switch (psSubMenu->asEntries[unIx].eType)
			{
			case VireioMenuEntry::EntryType::Entry_Float:
				// change value, set event
				*psSubMenu->asEntries[unIx].pfValue -= psSubMenu->asEntries[unIx].fChangeSize;
				psSubMenu->asEntries[unIx].bOnChanged = true;
				psSubMenu->bOnChanged = true;

				// clamp value
				if (*psSubMenu->asEntries[unIx].pfValue < psSubMenu->asEntries[unIx].fMinimum)
					*psSubMenu->asEntries[unIx].pfValue = psSubMenu->asEntries[unIx].fMinimum;

				// set the menu entry value
				psSubMenu->asEntries[unIx].fValue = *psSubMenu->asEntries[unIx].pfValue;
				break;
			case VireioMenuEntry::EntryType::Entry_UInt:
				if (!psSubMenu->asEntries[unIx].bValueEnumeration)
				{
					// change value, set event
					*psSubMenu->asEntries[unIx].punValue -= psSubMenu->asEntries[unIx].unChangeSize;
					psSubMenu->asEntries[unIx].bOnChanged = true;
					psSubMenu->bOnChanged = true;

					// clamp value
					if (*psSubMenu->asEntries[unIx].punValue > psSubMenu->asEntries[unIx].unMaximum)
						*psSubMenu->asEntries[unIx].punValue = psSubMenu->asEntries[unIx].unMaximum;

					// set the menu entry value
					psSubMenu->asEntries[unIx].unValue = *psSubMenu->asEntries[unIx].punValue;
				}
				break;
			case VireioMenuEntry::EntryType::Entry_Int:
				// change value, set event
				*psSubMenu->asEntries[unIx].pnValue -= psSubMenu->asEntries[unIx].nChangeSize;
				psSubMenu->asEntries[unIx].bOnChanged = true;
				psSubMenu->bOnChanged = true;

				// clamp value
				if (*psSubMenu->asEntries[unIx].pnValue > psSubMenu->asEntries[unIx].nMaximum)
					*psSubMenu->asEntries[unIx].pnValue = psSubMenu->asEntries[unIx].nMaximum;

				// set the menu entry value
				psSubMenu->asEntries[unIx].nValue = *psSubMenu->asEntries[unIx].pnValue;
				break;
			case VireioMenuEntry::EntryType::Entry_Bool:
				// change value, set event
				*psSubMenu->asEntries[unIx].pbValue = !(*psSubMenu->asEntries[unIx].pbValue);
				psSubMenu->asEntries[unIx].bOnChanged = true;
				psSubMenu->bOnChanged = true;

				// set the menu entry value
				psSubMenu->asEntries[unIx].bValue = *psSubMenu->asEntries[unIx].pbValue;
				break;
			}
		}

		// accept
		if (m_abMenuEvents[VireioMenuEvent::OnAccept])
		{
			// set event
			m_sMenuControl.eSelectionMovement = MenuControl::SelectionMovement::Accepted;
			m_sMenuControl.fActionTime = 0.3f;
			m_sMenuControl.fActionStartTime = fGlobalTime;

			// set former selection to actual
			m_sMenuControl.unSelectionFormer = m_sMenuControl.unSelection;

			// update sub menu, first get index and set events
			UINT unIxPrimal = m_sMenuControl.unSelection;
			UINT unIx = 0;
			for (UINT unI = 0; unI < psSubMenu->asEntries.size(); unI++)
			{
				// menu error ?
				if (unIx >= (UINT)psSubMenu->asEntries.size())
				{
					OutputDebugString(L"[STP] Fatal menu structure error ! Empty menu ??");
					m_sMenuControl.unSelection = 0;
					unIx = 0;
					break;
				}

				// set back index runner if inactive entry
				if (!psSubMenu->asEntries[unIx].bIsActive) { unIxPrimal++; }

				// caught selection ?
				if (unIx == unIxPrimal) break;

				// increment index
				unIx++;
			}
			psSubMenu->bOnAccept = true;
			psSubMenu->bOnChanged = true;
			psSubMenu->asEntries[unIx].bOnChanged = true;

			// is this a string selection enumeration entry ?
			if (psSubMenu->asEntries[unIx].bValueEnumeration)
			{
				// before last entry in enumeration list ?
				if ((*psSubMenu->asEntries[unIx].punValue) < (psSubMenu->asEntries[unIx].astrValueEnumeration.size() - 1))
					(*(psSubMenu->asEntries[unIx].punValue))++;
				else
					(*(psSubMenu->asEntries[unIx].punValue)) = 0;
				psSubMenu->asEntries[unIx].unValue = *psSubMenu->asEntries[unIx].punValue;
			}
			else
			{
				// only bool entries accept value change here
				switch (psSubMenu->asEntries[unIx].eType)
				{
				case VireioMenuEntry::EntryType::Entry_Bool:
					psSubMenu->asEntries[unIx].bValue = !psSubMenu->asEntries[unIx].bValue;
					(*(psSubMenu->asEntries[unIx].pbValue)) = psSubMenu->asEntries[unIx].bValue;
					break;
				case VireioMenuEntry::EntryType::Entry_Int:
				case VireioMenuEntry::EntryType::Entry_UInt:
				case VireioMenuEntry::EntryType::Entry_Float:
				default:
					break;
				}
			}
		}

		// exit
		if (m_abMenuEvents[VireioMenuEvent::OnExit])
		{
			// set event
			m_sMenuControl.eSelectionMovement = MenuControl::SelectionMovement::Exit;
			m_sMenuControl.fActionTime = 0.3f;
			m_sMenuControl.fActionStartTime = fGlobalTime;

			// set selection to zero
			m_sMenuControl.unSelectionFormer = m_sMenuControl.unSelection = 0;

			// main menu ? set menu bool to false
			if (m_sMenuControl.nMenuIx == -1)
			{
				m_bMenu = false;
				for (UINT unIx = 0; unIx < 32; unIx++)
				{
					// set sub menu active if menu is active
					if (m_asSubMenu[unIx])
						m_asSubMenu[unIx]->bIsActive = m_bMenu;
				}
			}
			// ...otherwise go back to main menu
			else m_sMenuControl.nMenuIx = -1;
		}
	}
}
