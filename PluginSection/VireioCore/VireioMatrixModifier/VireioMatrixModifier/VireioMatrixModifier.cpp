/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

Vireio Matrix Modifier - Vireio Stereo Matrix Modification Node
Copyright (C) 2015 Denis Reischl

File <VireioMatrixModifier.cpp> and
Class <VireioMatrixModifier> :
Copyright (C) 2015 Denis Reischl

Parts of this class directly derive from Vireio source code originally
authored by Chris Drain (v1.1.x 2013).

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

#define DEBUG_UINT(a) { wchar_t buf[128]; wsprintf(buf, L"%u", a); OutputDebugString(buf); }
#define DEBUG_HEX(a) { wchar_t buf[128]; wsprintf(buf, L"%x", a); OutputDebugString(buf); }

#include"VireioMatrixModifier.h"

#define INTERFACE_ID3D11DEVICE                                               6
#define INTERFACE_ID3D10DEVICE                                               7
#define INTERFACE_ID3D11DEVICECONTEXT                                        11
#define INTERFACE_IDXGISWAPCHAIN                                             29

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
#define METHOD_IDXGISWAPCHAIN_PRESENT                                        8
#define METHOD_ID3D11DEVICE_CREATEBUFFER                                     3
#define METHOD_ID3D11DEVICE_CREATEVERTEXSHADER                               12
#define METHOD_ID3D11DEVICECONTEXT_VSSETCONSTANTBUFFERS                      7
#define METHOD_ID3D11DEVICECONTEXT_PSSETSHADER                               9
#define METHOD_ID3D11DEVICECONTEXT_VSSETSHADER                               11
#define METHOD_ID3D11DEVICECONTEXT_MAP                                       14
#define METHOD_ID3D11DEVICECONTEXT_UNMAP                                     15
#define METHOD_ID3D11DEVICECONTEXT_PSSETCONSTANTBUFFERS                      16
#define METHOD_ID3D11DEVICECONTEXT_GSSETCONSTANTBUFFERS                      22
#define METHOD_ID3D11DEVICECONTEXT_COPYSUBRESOURCEREGION                     46
#define METHOD_ID3D11DEVICECONTEXT_COPYRESOURCE                              47
#define METHOD_ID3D11DEVICECONTEXT_UPDATESUBRESOURCE                         48
#define METHOD_ID3D11DEVICECONTEXT_HSSETCONSTANTBUFFERS                      62
#define METHOD_ID3D11DEVICECONTEXT_DSSETCONSTANTBUFFERS                      66
#define METHOD_ID3D11DEVICECONTEXT_VSGETCONSTANTBUFFERS                      72
#define METHOD_ID3D11DEVICECONTEXT_PSGETCONSTANTBUFFERS                      77
#define METHOD_ID3D10DEVICE_COPYSUBRESOURCEREGION                            32
#define METHOD_ID3D10DEVICE_COPYRESOURCE                                     33
#define METHOD_ID3D10DEVICE_UPDATESUBRESOURCE                                34
#endif

#define SAFE_RELEASE(a) if (a) { a->Release(); a = nullptr; }

#define TO_DO_ADD_BOOL_HERE_TRUE                                           true
#define TO_DO_ADD_BOOL_HERE_FALSE                                         false

/**
* Simple clipboard text helper.
***/
std::string GetClipboardText()
{
	// Try opening the clipboard
	if (!OpenClipboard(nullptr))
		return std::string();

	// Get handle of clipboard object for ANSI text
	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == nullptr)
		return std::string();

	// Lock the handle to get the actual text pointer
	char* pszText = static_cast<char*>(GlobalLock(hData));
	if (pszText == nullptr)
		return std::string();

	// Save text in a string class instance
	std::string text(pszText);

	// Release the lock
	GlobalUnlock(hData);

	// Release the clipboard
	CloseClipboard();

	return text;
}

/**
* Constructor.
***/
MatrixModifier::MatrixModifier() : AQU_Nodus(),
m_aszShaderConstantsA(),
m_aszShaderConstants(),
m_aszVShaderHashCodes(),
m_adwVShaderHashCodes(),
m_aszPShaderHashCodes(),
m_adwPShaderHashCodes(),
m_eDebugOption(Debug_Grab_Options::Debug_ConstantFloat4),
m_bGrabDebug(false),
m_adwPageIDs(0, 0),
m_aunGlobalConstantRuleIndices(),
#if defined(VIREIO_D3D9)
m_asShaderSpecificRuleIndices(),
#endif
m_aasConstantBufferRuleIndices(),
m_dwCurrentChosenShaderHashCode(0),
m_sModifierData{}
{
	// create a new HRESULT pointer
	m_pvReturn = (void*)new HRESULT();

	// init constant rule vector, otherwise pointer to vector may not work
	m_asConstantRules = std::vector<Vireio_Constant_Modification_Rule>();

	// at start, set the view adjustment class to basic config
	ZeroMemory(&m_sGameConfiguration, sizeof(Vireio_GameConfiguration));
	m_sGameConfiguration.fWorldScaleFactor = 0.0f;
	m_sGameConfiguration.fConvergence = DEFAULT_CONVERGENCE;
	m_sGameConfiguration.fIPD = IPD_DEFAULT;
	m_sGameConfiguration.fPFOV = DEFAULT_PFOV;
	m_sGameConfiguration.fAspectMultiplier = DEFAULT_ASPECT_MULTIPLIER;
	m_sGameConfiguration.dwVRboostMinShaderCount = 0;
	m_sGameConfiguration.dwVRboostMaxShaderCount = 99999999;
#ifdef _WIN64
	m_sGameConfiguration.bIs64bit = true;
#else
	m_sGameConfiguration.bIs64bit = false;
#endif
	m_sGameConfiguration.nRollImpl = 0;
	m_sGameConfiguration.bConvergenceEnabled = false;
	m_sGameConfiguration.fYawMultiplier = DEFAULT_YAW_MULTIPLIER;
	m_sGameConfiguration.fPitchMultiplier = DEFAULT_PITCH_MULTIPLIER;
	m_sGameConfiguration.fRollMultiplier = DEFAULT_ROLL_MULTIPLIER;
	m_sGameConfiguration.fPositionMultiplier = 1.0f;
	m_sGameConfiguration.fPositionXMultiplier = DEFAULT_POS_TRACKING_X_MULT;
	m_sGameConfiguration.fPositionYMultiplier = DEFAULT_POS_TRACKING_Y_MULT;
	m_sGameConfiguration.fPositionZMultiplier = DEFAULT_POS_TRACKING_Z_MULT;
	m_sGameConfiguration.bPFOVToggle = false;
	m_pcShaderModificationCalculation = std::make_shared<ModificationCalculation>(&m_sGameConfiguration);

	// clear GUI page structures
	ZeroMemory(&m_sPageDebug, sizeof(PageDebug));
	ZeroMemory(&m_sPageGameSettings, sizeof(PageGameSettings));
	ZeroMemory(&m_sPageShader, sizeof(PageShader));
	ZeroMemory(&m_sPageGameShaderRules, sizeof(PageGameShaderRules));

	// init shader rule page lists
	m_aszShaderRuleIndices = std::vector<std::wstring>();
	m_aszShaderRuleData = std::vector<std::wstring>();
	m_aszShaderRuleGeneralIndices = std::vector<std::wstring>();

	// reg count to 4 on shader page by default (=Matrix), operation by default to 1 (=Simple Translate)
	m_sPageGameShaderRules.m_dwRegCountValue = 4;
	m_sPageGameShaderRules.m_dwOperationToApply = 1;

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	// create buffer vectors ( * 2 for left/right side )
	m_apcVSActiveConstantBuffers11 = std::vector<ID3D11Buffer*>(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT * 2, nullptr);
	m_apcHSActiveConstantBuffers11 = std::vector<ID3D11Buffer*>(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT * 2, nullptr);
	m_apcDSActiveConstantBuffers11 = std::vector<ID3D11Buffer*>(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT * 2, nullptr);
	m_apcGSActiveConstantBuffers11 = std::vector<ID3D11Buffer*>(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT * 2, nullptr);
	m_apcPSActiveConstantBuffers11 = std::vector<ID3D11Buffer*>(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT * 2, nullptr);
	m_apcActiveRenderTargetViews11 = std::vector<ID3D11RenderTargetView*>(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT * 2, nullptr);
	m_apcActiveDepthStencilView11[0] = nullptr;
	m_apcActiveDepthStencilView11[1] = nullptr;

	// create output pointers
	m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_VertexShader] = (void*)&m_apcVSActiveConstantBuffers11[0];
	m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_HullShader] = (void*)&m_apcHSActiveConstantBuffers11[0];
	m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_DomainShader] = (void*)&m_apcDSActiveConstantBuffers11[0];
	m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_GeometryShader] = (void*)&m_apcGSActiveConstantBuffers11[0];
	m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_PixelShader] = (void*)&m_apcPSActiveConstantBuffers11[0];
	m_pvOutput[STS_Commanders::ppActiveRenderTargets_DX11] = (void*)&m_apcActiveRenderTargetViews11[0];
	m_pvOutput[STS_Commanders::ppActiveDepthStencil_DX11] = (void*)&m_apcActiveDepthStencilView11[0];

	// set constant buffer verification at startup (first 30 frames)
	m_dwVerifyConstantBuffers = CONSTANT_BUFFER_VERIFICATION_FRAME_NUMBER;
	m_bConstantBuffersInitialized = false;

	// buffer index sizes debug to false
	m_bBufferIndexDebug = false;

	// init shader vector
	m_asVShaders = std::vector<Vireio_D3D11_Shader>();
	m_asPShaders = std::vector<Vireio_D3D11_Shader>();

	// mapped resource data
	m_asMappedBuffers = std::vector<Vireio_Map_Data>();
	m_dwMappedBuffers = 0;

	// constant rule buffer counter starts with 1 to init a first update
	m_dwConstantRulesUpdateCounter = 1;

	// nullptr for all dx10/11 input fields
	m_ppvShaderBytecode_VertexShader = nullptr;
	m_pnBytecodeLength_VertexShader = nullptr;
	m_ppcClassLinkage_VertexShader = nullptr;
	m_pppcVertexShader_DX10 = nullptr;
	m_ppvShaderBytecode_PixelShader = nullptr;
	m_pnBytecodeLength_PixelShader = nullptr;
	m_ppcClassLinkage_PixelShader = nullptr;
	m_pppcPixelShader_DX10 = nullptr;
	m_ppcVertexShader_10 = nullptr;
	m_ppcVertexShader_11 = nullptr;
	m_ppcPixelShader_10 = nullptr;
	m_ppcPixelShader_11 = nullptr;
	m_ppsDesc_DX10 = nullptr;
	m_ppsInitialData_DX10 = nullptr;
	m_pppcBuffer_DX10 = nullptr;
	m_pdwStartSlot = nullptr;
	m_pdwNumBuffers = nullptr;
	m_pppcConstantBuffers_DX10 = nullptr;
	m_pppcConstantBuffers_DX11 = nullptr;
	m_pdwStartSlot_PixelShader = nullptr;
	m_pdwNumBuffers_PixelShader = nullptr;
	m_pppcConstantBuffers_DX10_PixelShader = nullptr;
	m_pppcConstantBuffers_DX11_PixelShader = nullptr;
	m_ppcDstResource_DX10 = nullptr;
	m_ppcDstResource_DX11 = nullptr;
	m_pdwDstSubresource = nullptr;
	m_ppsDstBox_DX10 = nullptr;
	m_ppsDstBox_DX11 = nullptr;
	m_ppvSrcData = nullptr;
	m_pdwSrcRowPitch = nullptr;
	m_pdwSrcDepthPitch = nullptr;
	m_ppcDstResource_DX10_Copy = nullptr;
	m_ppcSrcResource_DX10_Copy = nullptr;
	m_ppcDstResource_DX11_Copy = nullptr;
	m_ppcSrcResource_DX11_Copy = nullptr;
	m_ppcDstResource_DX10_CopySub = nullptr;
	m_ppcDstResource_DX11_CopySub = nullptr;
	m_pdwDstSubresource_CopySub = nullptr;
	m_pdwDstX = nullptr;
	m_pdwDstY = nullptr;
	m_pdwDstZ = nullptr;
	m_ppcSrcResource_DX10_CopySub = nullptr;
	m_ppcSrcResource_DX11_CopySub = nullptr;
	m_pdwSrcSubresource = nullptr;
	m_ppsSrcBox_DX10 = nullptr;
	m_ppsSrcBox_DX11 = nullptr;
	m_pdwStartSlot = nullptr;
	m_pdwNumBuffers = nullptr;
	m_pppcConstantBuffers_VertexShader = nullptr;
	m_pdwStartSlot_PixelShader = nullptr;
	m_pdwNumBuffers_PixelShader = nullptr;
	m_pppcConstantBuffers_PixelShader = nullptr;
	m_ppcResource_Map = nullptr;
	m_pdwSubresource_Map = nullptr;
	m_psMapType = nullptr;
	m_pdwMapFlags = nullptr;
	m_ppsMappedResource = nullptr;
	m_ppcResource_Unmap = nullptr;
	m_pdwSubresource_Unmap = nullptr;
	m_pcSecondaryRenderTarget10 = nullptr;
	m_pcSecondaryRenderTargetView10 = nullptr;
	m_pcSecondaryRenderTargetSRView10 = nullptr;
#elif defined(VIREIO_D3D9)
	// init shader rule page shader indices list (DX9 only)
	m_aszShaderRuleShaderIndices = std::vector<std::wstring>();

	// init shader constant indices (DX9 only)
	m_sModifierData.m_pasVSConstantRuleIndices = nullptr;
	m_sModifierData.m_pasPSConstantRuleIndices = nullptr;

	// init dx9 classes
	m_pcActiveVertexShader = nullptr;
	m_pcActivePixelShader = nullptr;
	m_bViewTransformSet = false;

	// init proxy registers
	m_afRegistersVertex = std::vector<float>(MAX_DX9_CONSTANT_REGISTERS * VECTOR_LENGTH);
	m_afRegistersPixel = std::vector<float>(MAX_DX9_CONSTANT_REGISTERS * VECTOR_LENGTH);

	// init shader vector
	m_asVShaders = std::vector<Vireio_D3D9_Shader>();
	m_asPShaders = std::vector<Vireio_D3D9_Shader>();

	D3DXMatrixIdentity(&m_sMatView[0]);
	D3DXMatrixIdentity(&m_sMatView[1]);
	D3DXMatrixIdentity(&m_sMatProj[0]);
	D3DXMatrixIdentity(&m_sMatProj[1]);

	m_psMatViewCurrent = &m_sMatView[0];
	m_psMatProjCurrent = &m_sMatProj[0];
#endif
}

/**
* Destructor.
***/
MatrixModifier::~MatrixModifier()
{
	m_pcShaderModificationCalculation.reset();
}

/**
* Return the name of the Matrix Modifier node.
***/
const char* MatrixModifier::GetNodeType()
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	return "Matrix ModifierDx10";
#elif defined(VIREIO_D3D9)
	return "Matrix Modifier";
#endif
}

/**
* Returns a global unique identifier for the Matrix Modifier node.
***/
UINT MatrixModifier::GetNodeTypeId()
{
#define DEVELOPER_IDENTIFIER 2006
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
#define MY_PLUGIN_IDENTIFIER 76
#elif defined(VIREIO_D3D9)
#define MY_PLUGIN_IDENTIFIER 75
#endif
	return ((DEVELOPER_IDENTIFIER << 16) + MY_PLUGIN_IDENTIFIER);
}

/**
* Returns the name of the category for the Matrix Modifier node.
***/
LPWSTR MatrixModifier::GetCategory()
{
	return L"Vireio Core";
}

/**
* Returns a logo to be used for the Matrix Modifier node.
***/
HBITMAP MatrixModifier::GetLogo()
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	HMODULE hModule = GetModuleHandle(L"VireioMatrixModifierDx10.dll");
#elif defined(VIREIO_D3D9)
	HMODULE hModule = GetModuleHandle(L"VireioMatrixModifier.dll");
#endif
	HBITMAP hBitmap = LoadBitmap(hModule, MAKEINTRESOURCE(IMG_LOGO01));
	return hBitmap;
}

/**
* Returns the updated control for the Matrix Modifier node.
* Allways return >nullptr< if there is no update for the control !!
***/
HBITMAP MatrixModifier::GetControl()
{
	return nullptr;
}

/**
* Node data size.
* 1) sizeof(Vireio_GameConfiguration)
* 2) sizeof(UINT) = Number of Rules
* 3) sizeof(Vireio_Constant_Modification_Rule_Normalized) * Number of Rules
* 4) sizeof(UINT) = Number of General Indices
* 5) sizeof(UINT) * Number of General Indices
***/
DWORD MatrixModifier::GetSaveDataSize()
{
	DWORD dwSizeofData = sizeof(Vireio_GameConfiguration);
	dwSizeofData += sizeof(UINT);
	dwSizeofData += (DWORD)m_asConstantRules.size() * sizeof(Vireio_Constant_Modification_Rule_Normalized);
	dwSizeofData += sizeof(UINT);
	dwSizeofData += (DWORD)m_aunGlobalConstantRuleIndices.size() * sizeof(UINT);
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	dwSizeofData += sizeof(UINT);
	dwSizeofData += (DWORD)m_aunFetchedHashCodes.size() * sizeof(UINT);
#elif defined(VIREIO_D3D9)
	dwSizeofData += sizeof(UINT);
	dwSizeofData += (DWORD)m_asShaderSpecificRuleIndices.size() * sizeof(Vireio_Hash_Rule_Index);
#endif
	return dwSizeofData;
}

/**
* Save the data.
***/
char* MatrixModifier::GetSaveData(UINT* pdwSizeOfData)
{
	static std::stringstream acStream;
	acStream = std::stringstream();

	// write game config
	acStream.write((char*)&m_sGameConfiguration, sizeof(Vireio_GameConfiguration));

	// number of rules
	UINT dwNumberOfRules = (UINT)m_asConstantRules.size();
	acStream.write((char*)&dwNumberOfRules, sizeof(UINT));

	// loop through rules, normalize each
	for (UINT dwI = 0; dwI < dwNumberOfRules; dwI++)
	{
		Vireio_Constant_Modification_Rule_Normalized sRule = { 0 };

		// normalize the constant string
		UINT dwStringSize = (UINT)m_asConstantRules[dwI].m_szConstantName.size();
		if (dwStringSize > 63) dwStringSize = 63;
		memcpy(sRule.m_szConstantName, &m_asConstantRules[dwI].m_szConstantName[0], dwStringSize);

		// parse the rest of the data
		sRule.m_dwBufferIndex = m_asConstantRules[dwI].m_dwBufferIndex;
		sRule.m_dwBufferSize = m_asConstantRules[dwI].m_dwBufferSize;
		sRule.m_dwStartRegIndex = m_asConstantRules[dwI].m_dwStartRegIndex;
		sRule.m_bUseName = m_asConstantRules[dwI].m_bUseName;
		sRule.m_bUsePartialNameMatch = m_asConstantRules[dwI].m_bUsePartialNameMatch;
		sRule.m_bUseBufferIndex = m_asConstantRules[dwI].m_bUseBufferIndex;
		sRule.m_bUseBufferSize = m_asConstantRules[dwI].m_bUseBufferSize;
		sRule.m_bUseStartRegIndex = m_asConstantRules[dwI].m_bUseStartRegIndex;
		sRule.m_dwRegisterCount = m_asConstantRules[dwI].m_dwRegisterCount;
		sRule.m_dwOperationToApply = m_asConstantRules[dwI].m_dwOperationToApply;
		sRule.m_bTranspose = m_asConstantRules[dwI].m_bTranspose;

		// write down the normalized rule
		acStream.write((char*)&sRule, sizeof(Vireio_Constant_Modification_Rule_Normalized));
	}

	// general indices
	UINT dwNumberOfIndices = (UINT)m_aunGlobalConstantRuleIndices.size();
	acStream.write((char*)&dwNumberOfIndices, sizeof(UINT));
	if (m_aunGlobalConstantRuleIndices.size() > 0)
		acStream.write((char*)&m_aunGlobalConstantRuleIndices[0], sizeof(UINT) * dwNumberOfIndices);

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	// fetched shader hash codes
	UINT unNumberOfFetched = (UINT)m_aunFetchedHashCodes.size();
	acStream.write((char*)&unNumberOfFetched, sizeof(UINT));
	acStream.write((char*)&m_aunFetchedHashCodes[0], sizeof(UINT) * unNumberOfFetched);
#elif defined(VIREIO_D3D9)
	UINT unNumberOfShaderSpecific = (UINT)m_asShaderSpecificRuleIndices.size();
	acStream.write((char*)&unNumberOfShaderSpecific, sizeof(UINT));
	if (m_asShaderSpecificRuleIndices.size() > 0)
		acStream.write((char*)&m_asShaderSpecificRuleIndices[0], sizeof(Vireio_Hash_Rule_Index) * unNumberOfShaderSpecific);
#endif

	// set data size
	UINT unDataSize = (UINT)acStream.str().size();
	if (unDataSize > MAX_DATA_SIZE) unDataSize = 0;
	*pdwSizeOfData = unDataSize;

	// copy data
	memcpy(&m_acData[0], (void*)&acStream.str()[0], (size_t)unDataSize);

	return (char*)&m_acData[0];
}

/**
* Get node data from the profile file.
***/
void MatrixModifier::InitNodeData(char* pData, UINT dwSizeOfData)
{
	UINT dwDataOffset = 0;
	if (dwSizeOfData >= sizeof(Vireio_GameConfiguration))
	{
		// copy the game configuration data
		memcpy(&m_sGameConfiguration, pData, sizeof(Vireio_GameConfiguration));
		dwDataOffset += sizeof(Vireio_GameConfiguration);

		// get the size of the rules data block
		UINT dwSizeOfRules = dwSizeOfData - sizeof(Vireio_GameConfiguration);

		if (dwSizeOfRules >= sizeof(UINT))
		{
			// get the number of rules
			UINT dwNumRules;
			memcpy(&dwNumRules, pData + dwDataOffset, sizeof(UINT));
			dwDataOffset += sizeof(UINT);

			// data size correct ?
			dwSizeOfRules -= sizeof(UINT);
			if (dwSizeOfRules >= (sizeof(Vireio_Constant_Modification_Rule_Normalized) * dwNumRules))
			{
				// create the rules
				for (UINT dwI = 0; dwI < dwNumRules; dwI++)
				{
					// read normalized data block
					Vireio_Constant_Modification_Rule_Normalized sRuleNormalized;
					memcpy(&sRuleNormalized, pData + dwDataOffset, sizeof(Vireio_Constant_Modification_Rule_Normalized));
					dwDataOffset += sizeof(Vireio_Constant_Modification_Rule_Normalized);

					// create rule, create modification and push back
					Vireio_Constant_Modification_Rule sRule = Vireio_Constant_Modification_Rule(&sRuleNormalized);
					{ wchar_t buf[128]; wsprintf(buf, L"[Vireio] Mod : %u Reg : %u RegCount : %u", sRule.m_dwOperationToApply, sRule.m_dwStartRegIndex, sRule.m_dwRegisterCount); OutputDebugString(buf); }
					if (sRule.m_dwRegisterCount == 1)
						sRule.m_pcModification = CreateVector4Modification(sRule.m_dwOperationToApply, m_pcShaderModificationCalculation);
					else if (sRule.m_dwRegisterCount == 4)
						sRule.m_pcModification = CreateMatrixModification(sRule.m_dwOperationToApply, m_pcShaderModificationCalculation, sRule.m_bTranspose);
					else sRule.m_pcModification = nullptr;
					m_asConstantRules.push_back(sRule);

					dwSizeOfRules -= sizeof(Vireio_Constant_Modification_Rule_Normalized);
				}
			}
		}

		// indices
		if (dwSizeOfRules >= sizeof(UINT))
		{
			// get the number of rules
			UINT dwNumIndices;
			memcpy(&dwNumIndices, pData + dwDataOffset, sizeof(UINT));
			dwDataOffset += sizeof(UINT);

			// data size correct ?
			dwSizeOfRules -= sizeof(UINT);
			if (dwSizeOfRules >= (sizeof(UINT) * dwNumIndices))
			{
				// create the indices
				for (UINT dwI = 0; dwI < dwNumIndices; dwI++)
				{
					// read normalized data block
					UINT dwIndex;
					memcpy(&dwIndex, pData + dwDataOffset, sizeof(UINT));
					dwDataOffset += sizeof(UINT);

					// add index
					m_aunGlobalConstantRuleIndices.push_back(dwIndex);

					dwSizeOfRules -= sizeof(UINT);
				}
			}
		}

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
		// fetched hash codes
		if (dwSizeOfRules >= sizeof(UINT))
		{
			// get the number of rules
			UINT unNumberOfFetched;
			memcpy(&unNumberOfFetched, pData + dwDataOffset, sizeof(UINT));
			dwDataOffset += sizeof(UINT);

			// data size correct ?
			dwSizeOfRules -= sizeof(UINT);
			if (dwSizeOfRules >= (sizeof(UINT) * unNumberOfFetched))
			{
				// create the indices
				for (UINT dwI = 0; dwI < unNumberOfFetched; dwI++)
				{
					// read normalized data block
					UINT unHash;
					memcpy(&unHash, pData + dwDataOffset, sizeof(UINT));
					dwDataOffset += sizeof(UINT);

					// add hash
					m_aunFetchedHashCodes.push_back(unHash);

					dwSizeOfRules -= sizeof(UINT);
				}
			}
		}
#elif defined(VIREIO_D3D9)
		// shader specific rules
		if (dwSizeOfRules >= sizeof(UINT))
		{
			// get the number of rules
			UINT unNumberOfShaderSpecific;
			memcpy(&unNumberOfShaderSpecific, pData + dwDataOffset, sizeof(UINT));
			dwDataOffset += sizeof(UINT);

			// data size correct ?
			dwSizeOfRules -= sizeof(UINT);
			if (dwSizeOfRules >= (sizeof(Vireio_Hash_Rule_Index) * unNumberOfShaderSpecific))
			{
				// create the indices
				for (UINT dwI = 0; dwI < unNumberOfShaderSpecific; dwI++)
				{
					// read normalized data block
					Vireio_Hash_Rule_Index unHashRuleIndex;
					memcpy(&unHashRuleIndex, pData + dwDataOffset, sizeof(Vireio_Hash_Rule_Index));
					dwDataOffset += sizeof(Vireio_Hash_Rule_Index);

					// add rule index
					m_asShaderSpecificRuleIndices.push_back(unHashRuleIndex);

					dwSizeOfRules -= sizeof(Vireio_Hash_Rule_Index);
				}
			}
		}
#endif

		// fill the string list
		FillShaderRuleIndices();
		FillShaderRuleGeneralIndices();
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
		FillFetchedHashCodeList();
#elif defined(VIREIO_D3D9)
		FillShaderRuleShaderIndices();
#endif
	}
	else
	{
		// set to ipd using vireio presenter.... // TODO !! currently set ipd to default
		m_sGameConfiguration.fConvergence = 3.0f;
		m_sGameConfiguration.fIPD = IPD_DEFAULT;
		m_sGameConfiguration.fWorldScaleFactor = 0.0f;
		m_sGameConfiguration.fPFOV = 110.0f;
	}

	// locate or create the INI file
	char szFilePathINI[1024];
	GetCurrentDirectoryA(1024, szFilePathINI);
	strcat_s(szFilePathINI, "\\VireioPerception.ini");
	bool bFileExists = false;
	if (PathFileExistsA(szFilePathINI)) bFileExists = true;

	// get ini file settings
	m_sGameConfiguration.fWorldScaleFactor = GetIniFileSetting(m_sGameConfiguration.fWorldScaleFactor, "MatrixModifier", "fWorldScaleFactor", szFilePathINI, bFileExists);
	m_sGameConfiguration.fConvergence = GetIniFileSetting(m_sGameConfiguration.fConvergence, "MatrixModifier", "fConvergence", szFilePathINI, bFileExists);
	m_sGameConfiguration.fIPD = GetIniFileSetting(m_sGameConfiguration.fIPD, "MatrixModifier", "fIPD", szFilePathINI, bFileExists);
	m_sGameConfiguration.fPFOV = GetIniFileSetting(m_sGameConfiguration.fPFOV, "MatrixModifier", "fPFOV", szFilePathINI, bFileExists);
	DWORD uDummy = 0;
	uDummy = GetIniFileSetting((DWORD)m_sGameConfiguration.bConvergenceEnabled, "MatrixModifier", "bConvergenceEnabled", szFilePathINI, bFileExists);
	if (uDummy) m_sGameConfiguration.bConvergenceEnabled = true; else m_sGameConfiguration.bConvergenceEnabled = false;
	uDummy = GetIniFileSetting((DWORD)m_sGameConfiguration.bPFOVToggle, "MatrixModifier", "bPFOVToggle", szFilePathINI, bFileExists);
	if (uDummy) m_sGameConfiguration.bPFOVToggle = true; else m_sGameConfiguration.bPFOVToggle = false;

	// create the menu
	ZeroMemory(&m_sMenu, sizeof(VireioSubMenu));
	m_sMenu.strSubMenu = "Matrix Modifier";

	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "World Scale Factor";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.pfValue = &m_sGameConfiguration.fWorldScaleFactor;
		sEntry.fValue = m_sGameConfiguration.fWorldScaleFactor;
		sEntry.fMinimum = -100000.0f;
		sEntry.fMaximum = 100000.0f;
		sEntry.fChangeSize = 0.01f;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Convergence";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.pfValue = &m_sGameConfiguration.fConvergence;
		sEntry.fValue = m_sGameConfiguration.fConvergence;
		sEntry.fMinimum = 1.0f;
		sEntry.fMaximum = 10.0f;
		sEntry.fChangeSize = 0.01f;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Inter-Pupillary Distance";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.pfValue = &m_sGameConfiguration.fIPD;
		sEntry.fValue = m_sGameConfiguration.fIPD;
		sEntry.fMinimum = 0.0510f;
		sEntry.fMaximum = 0.0770f;
		sEntry.fChangeSize = 0.001f;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Aspect Multiplier";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.pfValue = &m_sGameConfiguration.fAspectMultiplier;
		sEntry.fValue = m_sGameConfiguration.fAspectMultiplier;
		sEntry.fMinimum = 0.0f;
		sEntry.fMaximum = 5.0f;
		sEntry.fChangeSize = 0.01f;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Convergence Toggle";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Bool;
		sEntry.pbValue = &m_sGameConfiguration.bConvergenceEnabled;
		sEntry.bValue = m_sGameConfiguration.bConvergenceEnabled;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Projection FOV";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Float;
		sEntry.pfValue = &m_sGameConfiguration.fPFOV;
		sEntry.fValue = m_sGameConfiguration.fPFOV;
		m_sMenu.asEntries.push_back(sEntry);
	}
	{
		VireioMenuEntry sEntry = {};
		sEntry.strEntry = "Projection FOV Toggle";
		sEntry.bIsActive = true;
		sEntry.eType = VireioMenuEntry::EntryType::Entry_Bool;
		sEntry.pbValue = &m_sGameConfiguration.bPFOVToggle;
		sEntry.bValue = m_sGameConfiguration.bPFOVToggle;
		m_sMenu.asEntries.push_back(sEntry);
	}

	// TODO !!! Shader element calculation class !!
	/*m_pcShaderModificationCalculation->Load(m_sGameConfiguration);
	m_pcShaderModificationCalculation->UpdateProjectionMatrices((float)1920.0f / (float)1080.0f, m_sGameConfiguration.fPFOV);
	m_pcShaderModificationCalculation->ComputeViewTransforms();*/
}

/**
* Provides the name of the requested commander.
***/
LPWSTR MatrixModifier::GetCommanderName(DWORD dwCommanderIndex)
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	switch ((STS_Commanders)dwCommanderIndex)
	{
	case eDrawingSide:
		return L"Stereo Drawing Side";
	case ppActiveConstantBuffers_DX10_VertexShader:
		return L"ppConstantBuffers_DX10_VS";
	case ppActiveConstantBuffers_DX11_VertexShader:
		return L"ppConstantBuffers_DX11_VS";
	case ppActiveConstantBuffers_DX10_PixelShader:
		return L"ppConstantBuffers_DX10_PS";
	case ppActiveConstantBuffers_DX11_PixelShader:
		return L"ppConstantBuffers_DX11_PS";
	case ppActiveConstantBuffers_DX10_GeometryShader:
		return L"ppConstantBuffers_DX10_GS";
	case ppActiveConstantBuffers_DX11_GeometryShader:
		return L"ppConstantBuffers_DX11_GS";
	case ppActiveConstantBuffers_DX11_HullShader:
		return L"ppConstantBuffers_DX11_HS";
	case ppActiveConstantBuffers_DX11_DomainShader:
		return L"ppConstantBuffers_DX11_DS";
	case dwVerifyConstantBuffers:
		return L"Verify Constant Buffers";
	case asVShaderData:
		return L"Vertex Shader Data Array";
	case asPShaderData:
		return L"Pixel Shader Data Array";
	case ViewAdjustments:
		return L"View Adjustments";
	case SwitchRenderTarget:
		return L"Switch Render Target";
	case RESERVED00:
		return L"RESERVED00";
	case SecondaryRenderTarget_DX10:
		return L"SecondaryRenderTarget_DX10";
	case SecondaryRenderTarget_DX11:
		return L"SecondaryRenderTarget_DX11";
	case ppActiveRenderTargets_DX10:
		return L"ppActiveRenderTargets_DX10";
	case ppActiveRenderTargets_DX11:
		return L"ppActiveRenderTargets_DX11";
	case ppActiveDepthStencil_DX10:
		return L"ppActiveDepthStencil_DX10";
	case ppActiveDepthStencil_DX11:
		return L"ppActiveDepthStencil_DX11";
	case VireioMenu:
		return L"Vireio Menu";
	default:
		break;
	}
#elif defined(VIREIO_D3D9)
	switch ((STS_Commanders)dwCommanderIndex)
	{
	case Modifier:
		return VLink::Name(VLink::_L::ModifierData);
	default:
		break;
	}
#endif

	return L"UNTITLED";
}

/**
* Provides the name of the requested decommander.
***/
LPWSTR MatrixModifier::GetDecommanderName(DWORD dwDecommanderIndex)
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	switch ((STS_Decommanders)dwDecommanderIndex)
	{
	case pShaderBytecode_VertexShader:
		return L"pShaderBytecode_VS";
	case BytecodeLength_VertexShader:
		return L"BytecodeLength_VS";
	case pClassLinkage_VertexShader:
		return L"pClassLinkage_VertexShader";
	case ppVertexShader_DX10:
		return L"ppVertexShader_DX10";
	case pShaderBytecode_PixelShader:
		return L"pShaderBytecode_PS";
	case BytecodeLength_PixelShader:
		return L"BytecodeLength_PS";
	case pClassLinkage_PixelShader:
		return L"pClassLinkage_PixelShader";
	case ppPixelShader_DX10:
		return L"ppPixelShader_DX10";
	case pVertexShader_10:
		return L"pVertexShader_10";
	case pVertexShader_11:
		return L"pVertexShader_11";
	case pPixelShader_10:
		return L"pPixelShader_10";
	case pPixelShader_11:
		return L"pPixelShader_11";
	case pDesc_DX10:
		return L"pDesc_DX10";
	case pInitialData_DX10:
		return L"pInitialData_DX10";
	case ppBuffer_DX10:
		return L"ppBuffer_DX10";
	case StartSlot_VertexShader:
		return L"StartSlot_VS";
	case NumBuffers_VertexShader:
		return L"NumBuffers_VS";
	case ppConstantBuffers_DX10_VertexShader:
		return L"ppConstantBuffers_DX10_VS";
	case ppConstantBuffers_DX11_VertexShader:
		return L"ppConstantBuffers_DX11_VS";
	case pDstResource_DX10:
		return L"pDstResource_DX10";
	case pDstResource_DX11:
		return L"pDstResource_DX11";
	case DstSubresource:
		return L"DstSubresource";
	case pDstBox_DX10:
		return L"pDstBox_DX10";
	case pDstBox_DX11:
		return L"pDstBox_DX11";
	case pSrcData:
		return L"pSrcData";
	case SrcRowPitch:
		return L"SrcRowPitch";
	case SrcDepthPitch:
		return L"SrcDepthPitch";
	case pDstResource_DX10_Copy:
		return L"pDstResource_DX10_Copy";
	case pSrcResource_DX10_Copy:
		return L"pSrcResource_DX10_Copy";
	case pDstResource_DX11_Copy:
		return L"pDstResource_DX11_Copy";
	case pSrcResource_DX11_Copy:
		return L"pSrcResource_DX11_Copy";
	case pDstResource_DX10_CopySub:
		return L"pDstResource_DX10_CopySub";
	case pDstResource_DX11_CopySub:
		return L"pDstResource_DX11_CopySub";
	case DstSubresource_CopySub:
		return L"DstSubresource_CopySub";
	case DstX:
		return L"DstX";
	case DstY:
		return L"DstY";
	case DstZ:
		return L"DstZ";
	case pSrcResource_DX10_CopySub:
		return L"pSrcResource_DX10_CopySub";
	case pSrcResource_DX11_CopySub:
		return L"pSrcResource_DX11_CopySub";
	case SrcSubresource:
		return L"SrcSubresource";
	case pSrcBox_DX10:
		return L"pSrcBox_DX10";
	case pSrcBox_DX11:
		return L"pSrcBox_DX11";
	case StartSlot_Get_VertexShader:
		return L"StartSlot_Get_VertexShader";
	case NumBuffers_Get_VertexShader:
		return L"NumBuffers_Get_VertexShader";
	case ppConstantBuffers_DX10_Get_VertexShader:
		return L"ppConstantBuffers_DX10_Get_VS";
	case ppConstantBuffers_DX11_Get_VertexShader:
		return L"ppConstantBuffers_DX11_Get_VS";
	case pResource:
		return L"pResource";
	case Subresource:
		return L"Subresource";
	case MapType:
		return L"MapType";
	case MapFlags:
		return L"MapFlags";
	case pMappedResource:
		return L"pMappedResource";
	case pResource_Unmap:
		return L"pResource_Unmap";
	case Subresource_Unmap:
		return L"Subresource_Unmap";
	default:
		return L"UNTITLED";
	}
#elif defined(VIREIO_D3D9)
	switch ((STS_Decommanders)dwDecommanderIndex)
	{
	case SetVertexShader:
		return L"SetVertexShader";
	case SetPixelShader:
		return L"SetPixelShader";
	case SetTransform:
		return L"SetTransform";
	case MultiplyTransform:
		return L"MultiplyTransform";
	case SetVertexShaderConstantF:
		return L"SetVertexShaderConstantF";
	case GetVertexShaderConstantF:
		return L"GetVertexShaderConstantF";
	case SetVertexShaderConstantI:
		return L"SetVertexShaderConstantI";
	case GetVertexShaderConstantI:
		return L"GetVertexShaderConstantI";
	case SetVertexShaderConstantB:
		return L"SetVertexShaderConstantB";
	case GetVertexShaderConstantB:
		return L"GetVertexShaderConstantB";
	case SetPixelShaderConstantF:
		return L"SetPixelShaderConstantF";
	case GetPixelShaderConstantF:
		return L"GetPixelShaderConstantF";
	case SetPixelShaderConstantI:
		return L"SetPixelShaderConstantI";
	case GetPixelShaderConstantI:
		return L"GetPixelShaderConstantI";
	case SetPixelShaderConstantB:
		return L"SetPixelShaderConstantB";
	case GetPixelShaderConstantB:
		return L"GetPixelShaderConstantB";
	case SetStreamSource:
		return L"SetStreamSource";
	case GetStreamSource:
		return L"GetStreamSource";
	case CreateVertexShader:
		return L"CreateVertexShader";
	case CreatePixelShader:
		return L"CreatePixelShader";
	case VB_Apply:
		return L"VB_Apply";
	default:
		break;
	}
#endif
	return L"UNTITLED";
}

/**
* Returns the plug type for the requested commander.
***/
DWORD MatrixModifier::GetCommanderType(DWORD dwCommanderIndex)
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	switch ((STS_Commanders)dwCommanderIndex)
	{
	case eDrawingSide:
		return NOD_Plugtype::AQU_INT;
	case ppActiveConstantBuffers_DX10_VertexShader:
	case ppActiveConstantBuffers_DX10_GeometryShader:
	case ppActiveConstantBuffers_DX10_PixelShader:
		return NOD_Plugtype::AQU_PPNT_ID3D10BUFFER;
	case ppActiveConstantBuffers_DX11_VertexShader:
	case ppActiveConstantBuffers_DX11_HullShader:
	case ppActiveConstantBuffers_DX11_DomainShader:
	case ppActiveConstantBuffers_DX11_GeometryShader:
	case ppActiveConstantBuffers_DX11_PixelShader:
		return NOD_Plugtype::AQU_PPNT_ID3D11BUFFER;
	case dwVerifyConstantBuffers:
		return NOD_Plugtype::AQU_UINT;
	case asVShaderData:
		return NOD_Plugtype::AQU_VOID;
	case asPShaderData:
		return NOD_Plugtype::AQU_VOID;
	case ViewAdjustments:
		return NOD_Plugtype::AQU_INT;
	case SwitchRenderTarget:
		return NOD_Plugtype::AQU_INT;
	case RESERVED00:
		return NOD_Plugtype::AQU_SIZE_T;
	case SecondaryRenderTarget_DX10:
		return NOD_Plugtype::AQU_PNT_ID3D10SHADERRESOURCEVIEW;
	case SecondaryRenderTarget_DX11:
		return NOD_Plugtype::AQU_PNT_ID3D11SHADERRESOURCEVIEW;
	case ppActiveRenderTargets_DX10:
		return NOD_Plugtype::AQU_PPNT_ID3D10RENDERTARGETVIEW;
	case ppActiveRenderTargets_DX11:
		return NOD_Plugtype::AQU_PPNT_ID3D11RENDERTARGETVIEW;
	case ppActiveDepthStencil_DX10:
		return NOD_Plugtype::AQU_PPNT_ID3D10DEPTHSTENCILVIEW;
	case ppActiveDepthStencil_DX11:
		return NOD_Plugtype::AQU_PPNT_ID3D11DEPTHSTENCILVIEW;
	default:
		break;
	}
#elif defined(VIREIO_D3D9)
	switch ((STS_Commanders)dwCommanderIndex)
	{
	case Modifier:
		return VLink::Link(VLink::_L::ModifierData);
	default:
		break;
	}
#endif

	return NULL;
}

/**
* Returns the plug type for the requested decommander.
***/
DWORD MatrixModifier::GetDecommanderType(DWORD dwDecommanderIndex)
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	switch ((STS_Decommanders)dwDecommanderIndex)
	{
	case pShaderBytecode_VertexShader:
		return NOD_Plugtype::AQU_PNT_VOID;
	case BytecodeLength_VertexShader:
		return NOD_Plugtype::AQU_SIZE_T;
	case pClassLinkage_VertexShader:
		return NOD_Plugtype::AQU_PNT_ID3D11CLASSLINKAGE;
	case ppVertexShader_DX10:
		return NOD_Plugtype::AQU_PPNT_ID3D10VERTEXSHADER;
	case pShaderBytecode_PixelShader:
		return NOD_Plugtype::AQU_PNT_VOID;
	case BytecodeLength_PixelShader:
		return NOD_Plugtype::AQU_SIZE_T;
	case pClassLinkage_PixelShader:
		return NOD_Plugtype::AQU_PNT_ID3D11CLASSLINKAGE;
	case ppPixelShader_DX10:
		return NOD_Plugtype::AQU_PPNT_ID3D10PIXELSHADER;
	case pVertexShader_10:
		return NOD_Plugtype::AQU_PNT_ID3D10VERTEXSHADER;
	case pVertexShader_11:
		return NOD_Plugtype::AQU_PNT_ID3D11VERTEXSHADER;
	case pPixelShader_10:
		return NOD_Plugtype::AQU_PNT_ID3D10PIXELSHADER;
	case pPixelShader_11:
		return NOD_Plugtype::AQU_PNT_ID3D11PIXELSHADER;
	case pDesc_DX10:
		return NOD_Plugtype::AQU_PNT_D3D10_BUFFER_DESC;
	case pInitialData_DX10:
		return NOD_Plugtype::AQU_PNT_D3D10_SUBRESOURCE_DATA;
	case ppBuffer_DX10:
		return NOD_Plugtype::AQU_PNT_ID3D10BUFFER;
	case StartSlot_VertexShader:
		return NOD_Plugtype::AQU_UINT;
	case NumBuffers_VertexShader:
		return NOD_Plugtype::AQU_UINT;
	case ppConstantBuffers_DX10_VertexShader:
		return NOD_Plugtype::AQU_PPNT_ID3D10BUFFER;
	case ppConstantBuffers_DX11_VertexShader:
		return NOD_Plugtype::AQU_PPNT_ID3D11BUFFER;
	case pDstResource_DX10:
		return NOD_Plugtype::AQU_PNT_ID3D10RESOURCE;
	case pDstResource_DX11:
		return NOD_Plugtype::AQU_PNT_ID3D11RESOURCE;
	case DstSubresource:
		return NOD_Plugtype::AQU_UINT;
	case pDstBox_DX10:
		return NOD_Plugtype::AQU_PNT_D3D10_BOX;
	case pDstBox_DX11:
		return NOD_Plugtype::AQU_PNT_D3D11_BOX;
	case pSrcData:
		return NOD_Plugtype::AQU_PNT_VOID;
	case SrcRowPitch:
		return NOD_Plugtype::AQU_UINT;
	case SrcDepthPitch:
		return NOD_Plugtype::AQU_UINT;
	case pDstResource_DX10_Copy:
		return NOD_Plugtype::AQU_PNT_ID3D10RESOURCE;
	case pSrcResource_DX10_Copy:
		return NOD_Plugtype::AQU_PNT_ID3D10RESOURCE;
	case pDstResource_DX11_Copy:
		return NOD_Plugtype::AQU_PNT_ID3D11RESOURCE;
	case pSrcResource_DX11_Copy:
		return NOD_Plugtype::AQU_PNT_ID3D11RESOURCE;
	case pDstResource_DX10_CopySub:
		return NOD_Plugtype::AQU_PNT_ID3D10RESOURCE;
	case pDstResource_DX11_CopySub:
		return NOD_Plugtype::AQU_PNT_ID3D11RESOURCE;
	case DstSubresource_CopySub:
		return NOD_Plugtype::AQU_UINT;
	case DstX:
		return NOD_Plugtype::AQU_UINT;
	case DstY:
		return NOD_Plugtype::AQU_UINT;
	case DstZ:
		return NOD_Plugtype::AQU_UINT;
	case pSrcResource_DX10_CopySub:
		return NOD_Plugtype::AQU_PNT_ID3D10RESOURCE;
	case pSrcResource_DX11_CopySub:
		return NOD_Plugtype::AQU_PNT_ID3D11RESOURCE;
	case SrcSubresource:
		return NOD_Plugtype::AQU_UINT;
	case pSrcBox_DX10:
		return NOD_Plugtype::AQU_PNT_D3D10_BOX;
	case pSrcBox_DX11:
		return NOD_Plugtype::AQU_PNT_D3D11_BOX;
	case StartSlot_Get_VertexShader:
		return NOD_Plugtype::AQU_UINT;
	case NumBuffers_Get_VertexShader:
		return NOD_Plugtype::AQU_UINT;
	case ppConstantBuffers_DX10_Get_VertexShader:
		return NOD_Plugtype::AQU_PPNT_ID3D10BUFFER;
	case ppConstantBuffers_DX11_Get_VertexShader:
		return NOD_Plugtype::AQU_PPNT_ID3D11BUFFER;
	case pResource:
		return NOD_Plugtype::AQU_PNT_ID3D11RESOURCE;
	case Subresource:
		return NOD_Plugtype::AQU_UINT;
	case MapType:
		return NOD_Plugtype::AQU_D3D11_MAP;
	case MapFlags:
		return NOD_Plugtype::AQU_UINT;
	case pMappedResource:
		return NOD_Plugtype::AQU_PNT_D3D11_MAPPED_SUBRESOURCE;
	case pResource_Unmap:
		return NOD_Plugtype::AQU_PNT_ID3D11RESOURCE;
	case Subresource_Unmap:
		return NOD_Plugtype::AQU_UINT;
	default:
		break;
	}
#elif defined(VIREIO_D3D9)
	switch ((STS_Decommanders)dwDecommanderIndex)
	{
	case SetVertexShader:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetVertexShader);
	case SetPixelShader:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetPixelShader);
	case SetTransform:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetTransform);
	case MultiplyTransform:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::MultiplyTransform);
	case SetVertexShaderConstantF:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantF);
	case GetVertexShaderConstantF:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantF);
	case SetVertexShaderConstantI:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantI);
	case GetVertexShaderConstantI:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantI);
	case SetVertexShaderConstantB:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantB);
	case GetVertexShaderConstantB:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantB);
	case SetPixelShaderConstantF:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantF);
	case GetPixelShaderConstantF:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantF);
	case SetPixelShaderConstantI:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantI);
	case GetPixelShaderConstantI:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantI);
	case SetPixelShaderConstantB:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantB);
	case GetPixelShaderConstantB:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantB);
	case SetStreamSource:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::SetStreamSource);
	case GetStreamSource:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::GetStreamSource);
	case CreateVertexShader:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::CreateVertexShader);
	case CreatePixelShader:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9, (int)VMT_IDIRECT3DDEVICE9::CreatePixelShader);
	case VB_Apply:
		return NOD_Plugtype::WireCable((int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DStateBlock9, (int)VMT_IDIRECT3DSTATEBLOCK9::Apply);
	default:
		break;
	}
#endif
	return 0;
}

/**
* Provides the output pointer for the requested commander.
***/
void* MatrixModifier::GetOutputPointer(DWORD dwCommanderIndex)
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	switch ((STS_Commanders)dwCommanderIndex)
	{
	case eDrawingSide:
		return (void*)&m_sModifierData.m_eCurrentRenderingSide;
	case ppActiveConstantBuffers_DX10_VertexShader:
		//return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX10_VertexShader];
		break;
	case ppActiveConstantBuffers_DX10_GeometryShader:
		//return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX10_GeometryShader];
		break;
	case ppActiveConstantBuffers_DX10_PixelShader:
		//return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX10_PixelShader];
		break;
	case ppActiveConstantBuffers_DX11_VertexShader:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_VertexShader];
	case ppActiveConstantBuffers_DX11_HullShader:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_HullShader];
	case ppActiveConstantBuffers_DX11_DomainShader:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_DomainShader];
	case ppActiveConstantBuffers_DX11_GeometryShader:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_GeometryShader];
	case ppActiveConstantBuffers_DX11_PixelShader:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveConstantBuffers_DX11_PixelShader];
	case dwVerifyConstantBuffers:
		return (void*)&m_dwVerifyConstantBuffers;
	case asVShaderData:
		return (void*)&m_asVShaders;
	case asPShaderData:
		return (void*)&m_asPShaders;
	case ViewAdjustments:
		return (void*)&m_pcShaderViewAdjustment;
	case SwitchRenderTarget:
		return (void*)&m_bSwitchRenderTarget;
	case RESERVED00:
		return nullptr; // TODO !! RESERVED
	case SecondaryRenderTarget_DX10:
		return (void*)&m_pcSecondaryRenderTargetSRView10;
	case SecondaryRenderTarget_DX11:
		return (void*)&m_pcSecondaryRenderTargetSRView11;
	case ppActiveRenderTargets_DX10:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveRenderTargets_DX10];
	case ppActiveRenderTargets_DX11:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveRenderTargets_DX11];
	case ppActiveDepthStencil_DX10:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveDepthStencil_DX10];
	case ppActiveDepthStencil_DX11:
		return (void*)&m_pvOutput[STS_Commanders::ppActiveDepthStencil_DX11];
	case VireioMenu:
		return (void*)&m_sMenu;
	default:
		break;
	}
#elif defined(VIREIO_D3D9)
	switch ((STS_Commanders)dwCommanderIndex)
	{
	case Modifier:
		return (void*)&m_sModifierData;
	default:
		break;
	}
#endif

	return nullptr;
}

/**
* Sets the input pointer for the requested decommander.
***/
void MatrixModifier::SetInputPointer(DWORD dwDecommanderIndex, void* pData)
{
	if (dwDecommanderIndex < NUMBER_OF_DECOMMANDERS)
		m_ppInput[dwDecommanderIndex] = (pData);
}

/**
* Matrix Modifier supports various D3D10 + D3D11 calls.
***/
bool MatrixModifier::SupportsD3DMethod(int nD3DVersion, int nD3DInterface, int nD3DMethod)
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	if ((nD3DVersion >= (int)AQU_DirectXVersion::DirectX_10) &&
		(nD3DVersion <= (int)AQU_DirectXVersion::DirectX_10_1))
	{
		if (nD3DInterface == INTERFACE_ID3D10DEVICE)
		{

		}
		else if (nD3DInterface == INTERFACE_IDXGISWAPCHAIN)
		{

		}
	}
	else if ((nD3DVersion >= (int)AQU_DirectXVersion::DirectX_11) &&
		(nD3DVersion <= (int)AQU_DirectXVersion::DirectX_11_2))
	{
		if (nD3DInterface == INTERFACE_ID3D11DEVICECONTEXT)
		{
			if ((nD3DMethod == METHOD_ID3D11DEVICECONTEXT_VSSETSHADER) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_PSSETSHADER) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_VSSETCONSTANTBUFFERS) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_MAP) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_UNMAP) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_COPYSUBRESOURCEREGION) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_COPYRESOURCE) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_UPDATESUBRESOURCE) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_VSGETCONSTANTBUFFERS) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_PSGETCONSTANTBUFFERS) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_HSSETCONSTANTBUFFERS) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_DSSETCONSTANTBUFFERS) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_GSSETCONSTANTBUFFERS) ||
				(nD3DMethod == METHOD_ID3D11DEVICECONTEXT_PSSETCONSTANTBUFFERS))
				return true;
		}
		else if (nD3DInterface == INTERFACE_IDXGISWAPCHAIN)
		{

		}
	}
#elif defined(VIREIO_D3D9)
	if ((nD3DVersion >= (int)AQU_DirectXVersion::DirectX_9_0) &&
		(nD3DVersion <= (int)AQU_DirectXVersion::DirectX_9_29))
	{
		if (nD3DInterface == (int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9)
		{
			if ((nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetVertexShader) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetPixelShader) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetTransform) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::MultiplyTransform) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantF) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantF) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantI) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantI) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantB) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantB) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantF) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantF) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantI) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantI) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantB) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantB) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::SetStreamSource) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::GetStreamSource) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::CreateVertexShader) ||
				(nD3DMethod == (int)VMT_IDIRECT3DDEVICE9::CreatePixelShader))
				return true;
		}
		else if (nD3DInterface == (int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DStateBlock9)
		{
			if (nD3DMethod == (int)VMT_IDIRECT3DSTATEBLOCK9::Apply) return true;
		}
	}

#endif
	return false;
}

/**
* Handle Stereo Render Targets (+Depth Buffers).
* Main entry point.
***/
void* MatrixModifier::Provoke(void* pThis, int eD3D, int eD3DInterface, int eD3DMethod, DWORD dwNumberConnected, int& nProvokerIndex)
{
	static HRESULT nHr = S_OK;

	//#define _DEBUG_MAM
#ifdef _DEBUG_MAM
	{ wchar_t buf[128]; wsprintf(buf, L"[MAM] ifc %u mtd %u", eD3DInterface, eD3DMethod); OutputDebugString(buf); }
#endif

	// save ini file ?
	if (m_nIniFrameCount)
	{
		if (m_nIniFrameCount == 1)
		{
			// save ini settings.. get file path
			char szFilePathINI[1024];
			GetCurrentDirectoryA(1024, szFilePathINI);
			strcat_s(szFilePathINI, "\\VireioPerception.ini");
			bool bFileExists = false;

			// get ini file settings
			m_sGameConfiguration.fWorldScaleFactor = GetIniFileSetting(m_sGameConfiguration.fWorldScaleFactor, "MatrixModifier", "fWorldScaleFactor", szFilePathINI, bFileExists);
			m_sGameConfiguration.fConvergence = GetIniFileSetting(m_sGameConfiguration.fConvergence, "MatrixModifier", "fConvergence", szFilePathINI, bFileExists);
			m_sGameConfiguration.fIPD = GetIniFileSetting(m_sGameConfiguration.fIPD, "MatrixModifier", "fIPD", szFilePathINI, bFileExists);
			m_sGameConfiguration.fPFOV = GetIniFileSetting(m_sGameConfiguration.fPFOV, "MatrixModifier", "fPFOV", szFilePathINI, bFileExists);
			DWORD uDummy = 0;
			uDummy = GetIniFileSetting((DWORD)m_sGameConfiguration.bConvergenceEnabled, "MatrixModifier", "bConvergenceEnabled", szFilePathINI, bFileExists);
			if (uDummy) m_sGameConfiguration.bConvergenceEnabled = true; else m_sGameConfiguration.bConvergenceEnabled = false;
			uDummy = GetIniFileSetting((DWORD)m_sGameConfiguration.bPFOVToggle, "MatrixModifier", "bPFOVToggle", szFilePathINI, bFileExists);
			if (uDummy) m_sGameConfiguration.bPFOVToggle = true; else m_sGameConfiguration.bPFOVToggle = false;
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
		// for (size_t nIx = 0; nIx < m_sMenu.asEntries.size(); nIx++)
		{
			// update view transform ..... TODO 
			/*m_pcShaderModificationCalculation->Load(m_sGameConfiguration);
			m_pcShaderModificationCalculation->UpdateProjectionMatrices(((float)1920.0f / (float)1080.0f) * m_sGameConfiguration.fAspectMultiplier, m_sGameConfiguration.fPFOV);
			m_pcShaderModificationCalculation->ComputeViewTransforms();*/
		}
	}

	// head roll ? TODO !! Tracker input !!
	static float s_fRoll = 0.0f;
	/*if (m_apfTrackerInput[2])
	if ((*m_apfTrackerInput[2]) != s_fRoll)
	{
		s_fRoll = *m_apfTrackerInput[2];
		// TODO !!
		// m_pcShaderModificationCalculation->UpdateRoll(-s_fRoll);
		// m_pcShaderModificationCalculation->ComputeViewTransforms();
	}*/

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	switch (eD3DInterface)
	{
	case INTERFACE_ID3D11DEVICECONTEXT:
		switch (eD3DMethod)
		{
#pragma region ID3D11DeviceContext::UpdateSubresource
		case METHOD_ID3D11DEVICECONTEXT_UPDATESUBRESOURCE:
			// verify pointers
			if (!m_ppcDstResource_DX11) return nullptr;
			if (!m_pdwDstSubresource) return nullptr;
			if (!m_ppsDstBox_DX11) return nullptr;
			if (!m_ppvSrcData) return nullptr;
			if (!m_pdwSrcRowPitch) return nullptr;
			if (!m_pdwSrcDepthPitch) return nullptr;
			if (!*m_ppcDstResource_DX11) return nullptr;
			if (!*m_ppvSrcData) return nullptr;

			// is this a buffer ?
			D3D11_RESOURCE_DIMENSION eDimension;
			(*m_ppcDstResource_DX11)->GetType(&eDimension);
			if (eDimension == D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_BUFFER)
			{
				// is this a constant buffer ?
				D3D11_BUFFER_DESC sDesc;
				((ID3D11Buffer*)*m_ppcDstResource_DX11)->GetDesc(&sDesc);
				if ((sDesc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) == D3D11_BIND_CONSTANT_BUFFER)
				{
					// get shader rules index
					Vireio_Buffer_Rules_Index sRulesIndex;
					sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;
					sRulesIndex.m_dwUpdateCounter = 0;
					UINT dwDataSizeRulesIndex = sizeof(Vireio_Buffer_Rules_Index);
					((ID3D11Buffer*)*m_ppcDstResource_DX11)->GetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, &dwDataSizeRulesIndex, &sRulesIndex);

					// do modification and update right buffer only if shader rule assigned !!
					if ((dwDataSizeRulesIndex) && (sRulesIndex.m_nRulesIndex >= 0))
					{
						// get the private data interface
						ID3D11Buffer* pcBuffer = nullptr;
						UINT dwSize = sizeof(pcBuffer);
						((ID3D11Buffer*)*m_ppcDstResource_DX11)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcBuffer);

						if (pcBuffer)
						{
							// do the modification, first copy to buffers
							memcpy(m_pchBuffer11Left, *m_ppvSrcData, sDesc.ByteWidth);
							memcpy(m_pchBuffer11Right, *m_ppvSrcData, sDesc.ByteWidth);
							DoBufferModification(sRulesIndex.m_nRulesIndex, (UINT_PTR)m_pchBuffer11Left, (UINT_PTR)m_pchBuffer11Right, sDesc.ByteWidth);

							// update left + right buffer
							((ID3D11DeviceContext*)pThis)->UpdateSubresource(*m_ppcDstResource_DX11, *m_pdwDstSubresource, *m_ppsDstBox_DX11, m_pchBuffer11Left, *m_pdwSrcRowPitch, *m_pdwSrcDepthPitch);
							((ID3D11DeviceContext*)pThis)->UpdateSubresource((ID3D11Resource*)pcBuffer, *m_pdwDstSubresource, *m_ppsDstBox_DX11, m_pchBuffer11Right, *m_pdwSrcRowPitch, *m_pdwSrcDepthPitch);

							pcBuffer->Release();

							// method replaced, immediately return
							nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
						}
					}
				}
				else
				{
					// get the private data interface
					ID3D11Buffer* pcBuffer = nullptr;
					UINT dwSize = sizeof(pcBuffer);
					((ID3D11Buffer*)*m_ppcDstResource_DX11)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcBuffer);

					if (pcBuffer)
					{
						// update right buffer
						((ID3D11DeviceContext*)pThis)->UpdateSubresource((ID3D11Resource*)pcBuffer, *m_pdwDstSubresource, *m_ppsDstBox_DX11, *m_ppvSrcData, *m_pdwSrcRowPitch, *m_pdwSrcDepthPitch);
						pcBuffer->Release();
					}

				}
			}
			else if (eDimension <= D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_TEXTURE3D)
			{
				// get the stereo twin of the resource (texture)
				ID3D11Resource* pcResourceTwin = nullptr;
				UINT dwSize = sizeof(pcResourceTwin);
				((ID3D11Resource*)*m_ppcDstResource_DX11)->GetPrivateData(PDIID_ID3D11TextureXD_Stereo_Twin, &dwSize, (void*)&pcResourceTwin);
				if (pcResourceTwin)
				{
					// update stereo twin
					((ID3D11DeviceContext*)pThis)->UpdateSubresource(pcResourceTwin, *m_pdwDstSubresource, *m_ppsDstBox_DX11, *m_ppvSrcData, *m_pdwSrcRowPitch, *m_pdwSrcDepthPitch);
					pcResourceTwin->Release();
				}

			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::CopySubresourceRegion
		case METHOD_ID3D11DEVICECONTEXT_COPYSUBRESOURCEREGION:
			if (!m_ppcDstResource_DX11_CopySub) return nullptr;
			if (!m_pdwDstSubresource_CopySub) return nullptr;
			if (!m_pdwDstX) return nullptr;
			if (!m_pdwDstY) return nullptr;
			if (!m_pdwDstZ) return nullptr;
			if (!m_ppcSrcResource_DX11_CopySub) return nullptr;
			if (!m_pdwSrcSubresource) return nullptr;
			if (!m_ppsSrcBox_DX11) return nullptr;
			if (!*m_ppcDstResource_DX11_CopySub) return nullptr;
			if (!*m_ppcSrcResource_DX11_CopySub) return nullptr;
			{
				// get destination resource type
				D3D11_RESOURCE_DIMENSION eDimension;
				(*m_ppcDstResource_DX11_CopySub)->GetType(&eDimension);

				// if buffer, get desc
				if (eDimension == D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_BUFFER)
				{
					if (!*m_ppsSrcBox_DX11) return nullptr;

					D3D11_BUFFER_DESC sDescDst;
					((ID3D11Buffer*)*m_ppcDstResource_DX11_CopySub)->GetDesc(&sDescDst);

					// set the same shader rules index (if present) for the destination as for the source
					Vireio_Buffer_Rules_Index sRulesIndex;
					sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;
					sRulesIndex.m_dwUpdateCounter = 0;
					UINT dwDataSizeRulesIndex = sizeof(Vireio_Buffer_Rules_Index);
					((ID3D11Resource*)*m_ppcSrcResource_DX11_CopySub)->GetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, &dwDataSizeRulesIndex, &sRulesIndex);
					if ((dwDataSizeRulesIndex) && (sRulesIndex.m_nRulesIndex >= 0))
					{
						((ID3D11Resource*)*m_ppcDstResource_DX11_CopySub)->SetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, sizeof(Vireio_Buffer_Rules_Index), &sRulesIndex);
					}

					// copy to both sides, if source is a mono buffer set source to stereo buffer
					ID3D11Resource* pcResourceTwinSrc = nullptr;
					UINT dwSize = sizeof(pcResourceTwinSrc);
					((ID3D11Resource*)*m_ppcSrcResource_DX11_CopySub)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcResourceTwinSrc);
					if (pcResourceTwinSrc)
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_CopySub)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopySubresourceRegion(pcResourceTwinDst,
								*m_pdwDstSubresource_CopySub,
								*m_pdwDstX,
								*m_pdwDstY,
								*m_pdwDstZ,
								pcResourceTwinSrc,
								*m_pdwSrcSubresource,
								*m_ppsSrcBox_DX11);

							pcResourceTwinDst->Release();
						}
						pcResourceTwinSrc->Release();
					}
					else
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_CopySub)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopySubresourceRegion(pcResourceTwinDst,
								*m_pdwDstSubresource_CopySub,
								*m_pdwDstX,
								*m_pdwDstY,
								*m_pdwDstZ,
								*m_ppcSrcResource_DX11_CopySub,
								*m_pdwSrcSubresource,
								*m_ppsSrcBox_DX11);

							pcResourceTwinDst->Release();
						}
					}
				}
				// is this a texture ?
				else if (eDimension <= D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_TEXTURE3D)
				{
					// get the stereo twin of the resource (texture)
					ID3D11Resource* pcResourceTwinSrc = nullptr;
					UINT dwSize = sizeof(pcResourceTwinSrc);
					((ID3D11Resource*)*m_ppcSrcResource_DX11_CopySub)->GetPrivateData(PDIID_ID3D11TextureXD_Stereo_Twin, &dwSize, (void*)&pcResourceTwinSrc);
					if (pcResourceTwinSrc)
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_CopySub)->GetPrivateData(PDIID_ID3D11TextureXD_Stereo_Twin, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopySubresourceRegion(pcResourceTwinDst,
								*m_pdwDstSubresource_CopySub,
								*m_pdwDstX,
								*m_pdwDstY,
								*m_pdwDstZ,
								pcResourceTwinSrc,
								*m_pdwSrcSubresource,
								*m_ppsSrcBox_DX11);

							pcResourceTwinDst->Release();
						}
						else
						{
							// set private data "create new" BOOL
							BOOL bTrue = TRUE;
							((ID3D11Resource*)*m_ppcDstResource_DX11_CopySub)->SetPrivateData(PDID_ID3D11TextureXD_ShaderResouceView_Create_New, sizeof(BOOL), &bTrue);
						}
						pcResourceTwinSrc->Release();
					}
					else
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_CopySub)->GetPrivateData(PDIID_ID3D11TextureXD_Stereo_Twin, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopySubresourceRegion(pcResourceTwinDst,
								*m_pdwDstSubresource_CopySub,
								*m_pdwDstX,
								*m_pdwDstY,
								*m_pdwDstZ,
								*m_ppcSrcResource_DX11_CopySub,
								*m_pdwSrcSubresource,
								*m_ppsSrcBox_DX11);

							pcResourceTwinDst->Release();
						}
					}
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::CopyResource
		case METHOD_ID3D11DEVICECONTEXT_COPYRESOURCE:
			if (!m_ppcDstResource_DX11_Copy) return nullptr;
			if (!m_ppcSrcResource_DX11_Copy) return nullptr;
			if (!*m_ppcDstResource_DX11_Copy) return nullptr;
			if (!*m_ppcSrcResource_DX11_Copy) return nullptr;
			{
				// get destination resource type
				D3D11_RESOURCE_DIMENSION eDimension;
				(*m_ppcDstResource_DX11_Copy)->GetType(&eDimension);

				// if buffer, get desc
				if (eDimension == D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_BUFFER)
				{
					D3D11_BUFFER_DESC sDescDst;
					((ID3D11Buffer*)*m_ppcDstResource_DX11_Copy)->GetDesc(&sDescDst);

					// set the same shader rules index (if present) for the destination as for the source
					Vireio_Buffer_Rules_Index sRulesIndex;
					sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;
					sRulesIndex.m_dwUpdateCounter = 0;
					UINT dwDataSizeRulesIndex = sizeof(Vireio_Buffer_Rules_Index);
					((ID3D11Resource*)*m_ppcSrcResource_DX11_Copy)->GetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, &dwDataSizeRulesIndex, &sRulesIndex);
					if ((dwDataSizeRulesIndex) && (sRulesIndex.m_nRulesIndex >= 0))
					{
						((ID3D11Resource*)*m_ppcDstResource_DX11_Copy)->SetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, sizeof(Vireio_Buffer_Rules_Index), &sRulesIndex);
					}

					// copy to both sides, if source is a mono buffer set source to stereo buffer
					ID3D11Resource* pcResourceTwinSrc = nullptr;
					UINT dwSize = sizeof(pcResourceTwinSrc);
					((ID3D11Resource*)*m_ppcSrcResource_DX11_Copy)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcResourceTwinSrc);
					if (pcResourceTwinSrc)
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_Copy)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopyResource(pcResourceTwinDst, pcResourceTwinSrc);
							pcResourceTwinDst->Release();
						}
						pcResourceTwinSrc->Release();
					}
					else
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_Copy)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopyResource(pcResourceTwinDst, *m_ppcSrcResource_DX11_Copy);
							pcResourceTwinDst->Release();
						}
					}
				}
				// is this a texture ?
				else if (eDimension <= D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_TEXTURE3D)
				{
					// get the stereo twin of the resource (texture)
					ID3D11Resource* pcResourceTwinSrc = nullptr;
					UINT dwSize = sizeof(pcResourceTwinSrc);
					((ID3D11Resource*)*m_ppcSrcResource_DX11_Copy)->GetPrivateData(PDIID_ID3D11TextureXD_Stereo_Twin, &dwSize, (void*)&pcResourceTwinSrc);
					if (pcResourceTwinSrc)
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_Copy)->GetPrivateData(PDIID_ID3D11TextureXD_Stereo_Twin, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopyResource(pcResourceTwinDst, pcResourceTwinSrc);
							pcResourceTwinDst->Release();
						}
						else
						{
							// set private data "create new" BOOL
							BOOL bTrue = TRUE;
							((ID3D11Resource*)*m_ppcDstResource_DX11_Copy)->SetPrivateData(PDID_ID3D11TextureXD_ShaderResouceView_Create_New, sizeof(BOOL), &bTrue);
						}
						pcResourceTwinSrc->Release();
					}
					else
					{
						// get the stereo twin of the destination
						ID3D11Resource* pcResourceTwinDst = nullptr;
						dwSize = sizeof(pcResourceTwinDst);
						((ID3D11Resource*)*m_ppcDstResource_DX11_Copy)->GetPrivateData(PDIID_ID3D11TextureXD_Stereo_Twin, &dwSize, (void*)&pcResourceTwinDst);
						if (pcResourceTwinDst)
						{
							// do the copy call on the stereo twins of these textures
							((ID3D11DeviceContext*)pThis)->CopyResource(pcResourceTwinDst, *m_ppcSrcResource_DX11_Copy);
							pcResourceTwinDst->Release();
						}
					}
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::VSSetShader
		case METHOD_ID3D11DEVICECONTEXT_VSSETSHADER:
			if (!m_ppcVertexShader_11)
			{
				m_pcActiveVertexShader11 = nullptr;
				return nullptr;
			}
			if (!*m_ppcVertexShader_11)
			{
				m_pcActiveVertexShader11 = nullptr;
				return nullptr;
			}
			else
			{
				// set the active vertex shader
				m_pcActiveVertexShader11 = *m_ppcVertexShader_11;

				// loop through active constant buffers, get private data and update them accordingly to the new shader
				for (UINT dwIndex = 0; dwIndex < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; dwIndex++)
					if (m_apcVSActiveConstantBuffers11[dwIndex])
					{
						// verify buffer
						VerifyConstantBuffer(m_apcVSActiveConstantBuffers11[dwIndex], dwIndex, Vireio_Supported_Shaders::VertexShader);
					}

				// get the current shader hash
				Vireio_Shader_Private_Data sPrivateData;
				UINT dwDataSize = sizeof(sPrivateData);
				if (m_pcActiveVertexShader11)
					m_pcActiveVertexShader11->GetPrivateData(PDID_ID3D11VertexShader_Vireio_Data, &dwDataSize, (void*)&sPrivateData);

				// render target switched ? set back for new shader
				/*if (m_bSwitchRenderTarget)
				{
					// restore render targets by backup
					if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
						((ID3D11DeviceContext*)pThis)->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, (ID3D11RenderTargetView**)&m_apcActiveRenderTargetViews11[0], (ID3D11DepthStencilView*)m_apcActiveDepthStencilView11[0]);
					else
						((ID3D11DeviceContext*)pThis)->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, (ID3D11RenderTargetView**)&m_apcActiveRenderTargetViews11[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT], (ID3D11DepthStencilView*)m_apcActiveDepthStencilView11[1]);

					m_bSwitchRenderTarget = false;
				}*/

				// currently chosen ?
				if ((m_dwCurrentChosenShaderHashCode) && (m_eChosenShaderType == Vireio_Supported_Shaders::VertexShader))
				{
					// set null shader if hash matches
					if (m_dwCurrentChosenShaderHashCode == sPrivateData.dwHash)
					{
						// output shader debug data
						static UINT dwChosenOld = 0;
						if (dwChosenOld != m_dwCurrentChosenShaderHashCode)
						{
							std::wstringstream strStream;
							strStream << L"Hash:" << sPrivateData.dwHash;
							m_aszDebugTrace.push_back(strStream.str().c_str());
							for (UINT dwI = 0; dwI < (UINT)m_asVShaders[sPrivateData.dwIndex].asBuffers.size(); dwI++)
							{
								strStream = std::wstringstream();
								strStream << L"Name:" << m_asVShaders[sPrivateData.dwIndex].asBuffers[dwI].szName << L"::Size:" << m_asVShaders[sPrivateData.dwIndex].asBuffers[dwI].dwSize;
								m_aszDebugTrace.push_back(strStream.str().c_str());
							}
							for (UINT dwI = 0; dwI < (UINT)m_asVShaders[sPrivateData.dwIndex].asBuffersUnaccounted.size(); dwI++)
							{
								strStream = std::wstringstream();
								strStream << L"Reg:" << m_asVShaders[sPrivateData.dwIndex].asBuffersUnaccounted[dwI].dwRegister << L"::Size:" << m_asVShaders[sPrivateData.dwIndex].asBuffersUnaccounted[dwI].dwSize;
								m_aszDebugTrace.push_back(strStream.str().c_str());
							}

							dwChosenOld = m_dwCurrentChosenShaderHashCode;

							for (UINT dwI = 0; dwI < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; dwI++)
								if (m_apcVSActiveConstantBuffers11[dwI])
								{
									// get shader rules index
									Vireio_Buffer_Rules_Index sRulesIndex;
									sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;
									sRulesIndex.m_dwUpdateCounter = 0;
									UINT dwDataSizeRulesIndex = sizeof(Vireio_Buffer_Rules_Index);
									m_apcVSActiveConstantBuffers11[dwI]->GetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, &dwDataSizeRulesIndex, &sRulesIndex);

									D3D11_BUFFER_DESC sDesc;
									m_apcVSActiveConstantBuffers11[dwI]->GetDesc(&sDesc);
									strStream = std::wstringstream();
									strStream << L"Buffer Size [" << dwI << L"]:" << sDesc.ByteWidth << L" RuleInd:" << sRulesIndex.m_nRulesIndex;
									m_aszDebugTrace.push_back(strStream.str().c_str());
								}
						}

						// secondary render target present ?
						if (!m_pcSecondaryRenderTarget11)
						{
							// get device
							ID3D11Device* pcDevice = nullptr;
							ID3D11DeviceContext* pcContext = (ID3D11DeviceContext*)pThis;
							pcContext->GetDevice(&pcDevice);
							if (pcDevice)
							{
								// get the viewport
								UINT unNumViewports = 1;
								D3D11_VIEWPORT sViewport;
								pcContext->RSGetViewports(&unNumViewports, &sViewport);

								if (unNumViewports)
								{
									// fill the description
									D3D11_TEXTURE2D_DESC sDescTex;
									sDescTex.Width = (UINT)sViewport.Width;
									sDescTex.Height = (UINT)sViewport.Height;
									sDescTex.MipLevels = 1;
									sDescTex.ArraySize = 1;
									sDescTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
									sDescTex.SampleDesc.Count = 1;
									sDescTex.SampleDesc.Quality = 0;
									sDescTex.Usage = D3D11_USAGE_DEFAULT;
									sDescTex.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
									sDescTex.CPUAccessFlags = 0;
									sDescTex.MiscFlags = 0;

									// create secondary render target
									pcDevice->CreateTexture2D(&sDescTex, NULL, &m_pcSecondaryRenderTarget11);
									if (m_pcSecondaryRenderTarget11)
									{
										// create render target view
										if (FAILED(pcDevice->CreateRenderTargetView(m_pcSecondaryRenderTarget11, NULL, &m_pcSecondaryRenderTargetView11)))
											OutputDebugString(L"MAM: Failed to create secondary render target view.");

										// create shader resource view
										if (FAILED(pcDevice->CreateShaderResourceView(m_pcSecondaryRenderTarget11, NULL, &m_pcSecondaryRenderTargetSRView11)))
											OutputDebugString(L"MAM: Failed to create secondary render target shader resource view.");
									}
									else OutputDebugString(L"MAM: Failed to create secondary render target !");
								}
								else OutputDebugString(L"MAM: No Viewport present !");

								pcDevice->Release();
							}
						}

						// switch render targets to temporary
						// m_bSwitchRenderTarget = true;

						// set secondary render target
						((ID3D11DeviceContext*)pThis)->OMSetRenderTargets(1, &m_pcSecondaryRenderTargetView11, nullptr);
					}
				}



				// loop through fetched hash codes
				for (UINT unI = 0; unI < (UINT)m_aunFetchedHashCodes.size(); unI++)
				{
					// switch render targets
					if ((sPrivateData.dwHash == m_aunFetchedHashCodes[unI]))
					{
						// secondary render target present ?
						if (!m_pcSecondaryRenderTarget11)
						{
							// get device
							ID3D11Device* pcDevice = nullptr;
							ID3D11DeviceContext* pcContext = (ID3D11DeviceContext*)pThis;
							pcContext->GetDevice(&pcDevice);
							if (pcDevice)
							{
								// get the viewport
								UINT unNumViewports = 1;
								D3D11_VIEWPORT sViewport;
								pcContext->RSGetViewports(&unNumViewports, &sViewport);

								if (unNumViewports)
								{
									// fill the description
									D3D11_TEXTURE2D_DESC sDescTex;
									sDescTex.Width = (UINT)sViewport.Width;
									sDescTex.Height = (UINT)sViewport.Height;
									sDescTex.MipLevels = 1;
									sDescTex.ArraySize = 1;
									sDescTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
									sDescTex.SampleDesc.Count = 1;
									sDescTex.SampleDesc.Quality = 0;
									sDescTex.Usage = D3D11_USAGE_DEFAULT;
									sDescTex.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
									sDescTex.CPUAccessFlags = 0;
									sDescTex.MiscFlags = 0;

									// create secondary render target
									pcDevice->CreateTexture2D(&sDescTex, NULL, &m_pcSecondaryRenderTarget11);
									if (m_pcSecondaryRenderTarget11)
									{
										// create render target view
										if (FAILED(pcDevice->CreateRenderTargetView(m_pcSecondaryRenderTarget11, NULL, &m_pcSecondaryRenderTargetView11)))
											OutputDebugString(L"MAM: Failed to create secondary render target view.");

										// create shader resource view
										if (FAILED(pcDevice->CreateShaderResourceView(m_pcSecondaryRenderTarget11, NULL, &m_pcSecondaryRenderTargetSRView11)))
											OutputDebugString(L"MAM: Failed to create secondary render target shader resource view.");
									}
									else OutputDebugString(L"MAM: Failed to create secondary render target !");
								}
								else OutputDebugString(L"MAM: No Viewport present !");

								pcDevice->Release();
							}
						}

						// switch render targets to temporary
						// m_bSwitchRenderTarget = true;

						// set secondary render target
						if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
							((ID3D11DeviceContext*)pThis)->OMSetRenderTargets(1, &m_pcSecondaryRenderTargetView11, (ID3D11DepthStencilView*)m_apcActiveDepthStencilView11[0]);
						else
							((ID3D11DeviceContext*)pThis)->OMSetRenderTargets(1, &m_pcSecondaryRenderTargetView11, (ID3D11DepthStencilView*)m_apcActiveDepthStencilView11[1]);
					}
				}

				// sort shader list ?
				if (m_bSortShaderList)
				{
					// get the current shader hash
					Vireio_Shader_Private_Data sPrivateData;
					UINT dwDataSize = sizeof(sPrivateData);
					if (m_pcActiveVertexShader11)
						m_pcActiveVertexShader11->GetPrivateData(PDID_ID3D11VertexShader_Vireio_Data, &dwDataSize, (void*)&sPrivateData);

					// get shader index
					for (UINT dwI = 1; dwI < (UINT)m_adwVShaderHashCodes.size(); dwI++)
					{
						if (sPrivateData.dwHash == m_adwVShaderHashCodes[dwI])
						{
							// move one forward
							std::wstring szBuf = m_aszVShaderHashCodes[dwI - 1];
							m_aszVShaderHashCodes[dwI - 1] = m_aszVShaderHashCodes[dwI];
							m_aszVShaderHashCodes[dwI] = szBuf;

							UINT dwBuf = m_adwVShaderHashCodes[dwI - 1];
							m_adwVShaderHashCodes[dwI - 1] = m_adwVShaderHashCodes[dwI];
							m_adwVShaderHashCodes[dwI] = dwBuf;

							// end search;
							dwI = (UINT)m_adwVShaderHashCodes.size();
						}
					}
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::PSSetShader
		case METHOD_ID3D11DEVICECONTEXT_PSSETSHADER:
			if (!m_ppcPixelShader_11)
			{
				//m_pcActivePixelShader11 = nullptr;
				return nullptr;
			}
			if (!*m_ppcPixelShader_11)
			{
				//m_pcActivePixelShader11 = nullptr;
				return nullptr;
			}
			else
			{
				// loop through active constant buffers, get private data and update them accordingly to the new shader... TODO !! VERIFY ALL SHADERS
				/*for (UINT dwIndex = 0; dwIndex < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; dwIndex++)
				if (m_apcPSActiveConstantBuffers11[dwIndex])
				{
				// verify buffer
				VerifyConstantBuffer(m_apcPSActiveConstantBuffers11[dwIndex], dwIndex);
				}*/

				// currently chosen ?
				if ((m_dwCurrentChosenShaderHashCode) && (m_eChosenShaderType == Vireio_Supported_Shaders::PixelShader))
				{
					// get the current shader hash
					Vireio_Shader_Private_Data sPrivateData;
					UINT dwDataSize = sizeof(sPrivateData);
					(*m_ppcPixelShader_11)->GetPrivateData(PDID_ID3D11VertexShader_Vireio_Data, &dwDataSize, (void*)&sPrivateData);

					// set null shader if hash matches
					if (m_dwCurrentChosenShaderHashCode == sPrivateData.dwHash)
					{
						// call super method
						((ID3D11DeviceContext*)pThis)->PSSetShader(nullptr, nullptr, NULL);

						// method replaced, immediately return
						nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
					}
				}

				// sort shader list ?
				if (m_bSortShaderList)
				{
					// get the current shader hash
					Vireio_Shader_Private_Data sPrivateData;
					UINT dwDataSize = sizeof(sPrivateData);
					(*m_ppcPixelShader_11)->GetPrivateData(PDID_ID3D11VertexShader_Vireio_Data, &dwDataSize, (void*)&sPrivateData);

					// get shader index
					for (UINT dwI = 1; dwI < (UINT)m_adwPShaderHashCodes.size(); dwI++)
					{
						if (sPrivateData.dwHash == m_adwPShaderHashCodes[dwI])
						{
							// move one forward
							std::wstring szBuf = m_aszPShaderHashCodes[dwI - 1];
							m_aszPShaderHashCodes[dwI - 1] = m_aszPShaderHashCodes[dwI];
							m_aszPShaderHashCodes[dwI] = szBuf;

							UINT dwBuf = m_adwPShaderHashCodes[dwI - 1];
							m_adwPShaderHashCodes[dwI - 1] = m_adwPShaderHashCodes[dwI];
							m_adwPShaderHashCodes[dwI] = dwBuf;

							// end search;
							dwI = (UINT)m_adwPShaderHashCodes.size();
						}
					}
				}
			}
			break;
#pragma endregion
#pragma region ID3D11DeviceContext::VSSetConstantBuffers
		case METHOD_ID3D11DEVICECONTEXT_VSSETCONSTANTBUFFERS:
			if (!m_pdwStartSlot) return nullptr;
			if (!m_pdwNumBuffers) return nullptr;
			if (!m_pppcConstantBuffers_DX11) return nullptr;
			if (!*m_pppcConstantBuffers_DX11) return nullptr;
			if (!**m_pppcConstantBuffers_DX11) return nullptr;
			{
				// get current shader hash code if there is a shader chosen
				static Vireio_Shader_Private_Data sPrivateData = { 0 };
				static UINT dwChosenOld = 0;
				if ((m_dwCurrentChosenShaderHashCode) || (m_bBufferIndexDebug))
				{
					if (m_dwCurrentChosenShaderHashCode)
					{
						// get the current shader hash
						UINT dwDataSize = sizeof(sPrivateData);
						if (m_pcActiveVertexShader11)
							m_pcActiveVertexShader11->GetPrivateData(PDID_ID3D11VertexShader_Vireio_Data, &dwDataSize, (void*)&sPrivateData);
						else ZeroMemory(&sPrivateData, sizeof(Vireio_Shader_Private_Data));

						// output shader debug data as long as the chosen shader is set
						if (dwChosenOld == 0) dwChosenOld = m_dwCurrentChosenShaderHashCode;
						if ((dwChosenOld == m_dwCurrentChosenShaderHashCode) && (m_dwCurrentChosenShaderHashCode == sPrivateData.dwHash))
						{
							std::wstringstream strStream = std::wstringstream();
							strStream << L"VSSetConstantBuffers : " << sPrivateData.dwHash;
							m_aszDebugTrace.push_back(strStream.str().c_str());
						}

						// move up/down on list if "W" or "S" key is pressed and released
						static bool s_bW = false, s_bS = false;
						if (GetAsyncKeyState(0x57))
							s_bW = true;
						else
						{
							if (s_bW)
							{
								// send a virtual windows event to the control in the gui
								WindowsEvent(VIRTUAL_EVENT_CONTROL_UP, (WPARAM)m_sPageShader.m_dwHashCodes, 0);
							}
							s_bW = false;
						}
						if (GetAsyncKeyState(0x53))
							s_bS = true;
						else
						{
							if (s_bS)
							{
								// send a virtual windows event to the control in the gui
								WindowsEvent(VIRTUAL_EVENT_CONTROL_DOWN, (WPARAM)m_sPageShader.m_dwHashCodes, 0);
							}
							s_bS = false;
						}
					}
					if (m_bBufferIndexDebug)
					{
						// get buffer index
						UINT dwBufferIndex = 0;
						std::wstringstream sz = std::wstringstream(m_sPageGameShaderRules.m_szBufferIndex);
						sz >> dwBufferIndex;

						// is this index set ?
						if ((dwBufferIndex >= *m_pdwStartSlot) && (dwBufferIndex < (*m_pdwStartSlot + *m_pdwNumBuffers)))
						{
							UINT dwIndex = dwBufferIndex - *m_pdwStartSlot;
							if ((*m_pppcConstantBuffers_DX11)[dwIndex])
							{
								// get the buffer description
								D3D11_BUFFER_DESC sDesc;
								((*m_pppcConstantBuffers_DX11)[dwIndex])->GetDesc(&sDesc);

								// already enlisted ? else enlist and output on debug trace
								auto it = std::find(m_aunBufferIndexSizesDebug.begin(), m_aunBufferIndexSizesDebug.end(), sDesc.ByteWidth);
								if (it == m_aunBufferIndexSizesDebug.end())
								{
									// output to debug trace
									std::wstringstream strStream = std::wstringstream();
									strStream << L"Buffer Index: " << dwBufferIndex << " Size:" << sDesc.ByteWidth;
									m_aszDebugTrace.push_back(strStream.str().c_str());

									// add to vector 
									m_aunBufferIndexSizesDebug.push_back(sDesc.ByteWidth);
								}
							}
						}
					}
				}
				else dwChosenOld = 0;

				// call base method
				XSSetConstantBuffers((ID3D11DeviceContext*)pThis, m_apcVSActiveConstantBuffers11, *m_pdwStartSlot, *m_pdwNumBuffers, *m_pppcConstantBuffers_DX11, Vireio_Supported_Shaders::VertexShader);

				// call super method
				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					((ID3D11DeviceContext*)pThis)->VSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcVSActiveConstantBuffers11[*m_pdwStartSlot]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
				else
				{
					((ID3D11DeviceContext*)pThis)->VSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcVSActiveConstantBuffers11[(*m_pdwStartSlot) + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::HSSetConstantBuffers
		case METHOD_ID3D11DEVICECONTEXT_HSSETCONSTANTBUFFERS:
			if (!m_pdwStartSlot) return nullptr;
			if (!m_pdwNumBuffers) return nullptr;
			if (!m_pppcConstantBuffers_DX11) return nullptr;
			if (!*m_pppcConstantBuffers_DX11) return nullptr;
			if (!**m_pppcConstantBuffers_DX11) return nullptr;
			{
				// call base method
				XSSetConstantBuffers((ID3D11DeviceContext*)pThis, m_apcHSActiveConstantBuffers11, *m_pdwStartSlot, *m_pdwNumBuffers, *m_pppcConstantBuffers_DX11, Vireio_Supported_Shaders::HullShader);

				// call super method
				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					((ID3D11DeviceContext*)pThis)->HSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcHSActiveConstantBuffers11[*m_pdwStartSlot]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
				else
				{
					((ID3D11DeviceContext*)pThis)->HSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcHSActiveConstantBuffers11[(*m_pdwStartSlot) + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::DSSetConstantBuffers
		case METHOD_ID3D11DEVICECONTEXT_DSSETCONSTANTBUFFERS:
			if (!m_pdwStartSlot) return nullptr;
			if (!m_pdwNumBuffers) return nullptr;
			if (!m_pppcConstantBuffers_DX11) return nullptr;
			if (!*m_pppcConstantBuffers_DX11) return nullptr;
			if (!**m_pppcConstantBuffers_DX11) return nullptr;
			{
				// call base method
				XSSetConstantBuffers((ID3D11DeviceContext*)pThis, m_apcDSActiveConstantBuffers11, *m_pdwStartSlot, *m_pdwNumBuffers, *m_pppcConstantBuffers_DX11, Vireio_Supported_Shaders::DomainShader);

				// call super method
				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					((ID3D11DeviceContext*)pThis)->DSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcDSActiveConstantBuffers11[*m_pdwStartSlot]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
				else
				{
					((ID3D11DeviceContext*)pThis)->DSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcDSActiveConstantBuffers11[(*m_pdwStartSlot) + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::GSSetConstantBuffers
		case METHOD_ID3D11DEVICECONTEXT_GSSETCONSTANTBUFFERS:
			if (!m_pdwStartSlot) return nullptr;
			if (!m_pdwNumBuffers) return nullptr;
			if (!m_pppcConstantBuffers_DX11) return nullptr;
			if (!*m_pppcConstantBuffers_DX11) return nullptr;
			if (!**m_pppcConstantBuffers_DX11) return nullptr;
			{
				// call base method
				XSSetConstantBuffers((ID3D11DeviceContext*)pThis, m_apcGSActiveConstantBuffers11, *m_pdwStartSlot, *m_pdwNumBuffers, *m_pppcConstantBuffers_DX11, Vireio_Supported_Shaders::GeometryShader);

				// call super method
				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					((ID3D11DeviceContext*)pThis)->GSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcGSActiveConstantBuffers11[*m_pdwStartSlot]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
				else
				{
					((ID3D11DeviceContext*)pThis)->GSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcGSActiveConstantBuffers11[(*m_pdwStartSlot) + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::PSSetConstantBuffers
		case METHOD_ID3D11DEVICECONTEXT_PSSETCONSTANTBUFFERS:
			if (!m_pdwStartSlot) return nullptr;
			if (!m_pdwNumBuffers) return nullptr;
			if (!m_pppcConstantBuffers_DX11) return nullptr;
			if (!*m_pppcConstantBuffers_DX11) return nullptr;
			if (!**m_pppcConstantBuffers_DX11) return nullptr;
			{
				// call base method
				XSSetConstantBuffers((ID3D11DeviceContext*)pThis, m_apcPSActiveConstantBuffers11, *m_pdwStartSlot, *m_pdwNumBuffers, *m_pppcConstantBuffers_DX11, Vireio_Supported_Shaders::PixelShader);

				// call super method
				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					((ID3D11DeviceContext*)pThis)->PSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcPSActiveConstantBuffers11[*m_pdwStartSlot]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
				else
				{
					((ID3D11DeviceContext*)pThis)->PSSetConstantBuffers(*m_pdwStartSlot,
						*m_pdwNumBuffers,
						(ID3D11Buffer**)&m_apcPSActiveConstantBuffers11[(*m_pdwStartSlot) + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				}
			}
			return nullptr;
#pragma endregion
#pragma region ID3D11DeviceContext::VSGetConstantBuffers
		case METHOD_ID3D11DEVICECONTEXT_VSGETCONSTANTBUFFERS:
			// currently, we set the main buffers to avoid that the game gets the 
			// stereo buffers assioziated with the main buffers as private data interfaces.
			// if there is a game that flickers (should not be) we need to replace the whole
			// method (using AQU_PluginFlags::ImmediateReturnFlag)
			((ID3D11DeviceContext*)pThis)->VSSetConstantBuffers(0,
				D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
				(ID3D11Buffer**)&m_apcVSActiveConstantBuffers11[0]);
			break;
#pragma endregion
#pragma region ID3D11DeviceContext::PSGetConstantBuffers
		case METHOD_ID3D11DEVICECONTEXT_PSGETCONSTANTBUFFERS:
			// currently, we set the main buffers to avoid that the game gets the 
			// stereo buffers assioziated with the main buffers as private data interfaces.
			// if there is a game that flickers (should not be) we need to replace the whole
			// method (using AQU_PluginFlags::ImmediateReturnFlag)
			((ID3D11DeviceContext*)pThis)->PSSetConstantBuffers(0,
				D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,
				(ID3D11Buffer**)&m_apcPSActiveConstantBuffers11[0]);
			break;
#pragma endregion
#pragma region ID3D11DeviceContext::Map
		case METHOD_ID3D11DEVICECONTEXT_MAP:
			if (!m_ppcResource_Map) return nullptr;
			if (!m_pdwSubresource_Map) return nullptr;
			if (!m_psMapType) return nullptr;
			if (!m_pdwMapFlags) return nullptr;
			if (!m_ppsMappedResource) return nullptr;
			if (!*m_ppcResource_Map) return nullptr;
			if (!*m_ppsMappedResource) return nullptr;
			{
				// get destination resource type
				D3D11_RESOURCE_DIMENSION eDimension;
				(*m_ppcResource_Map)->GetType(&eDimension);

				// if buffer, get desc
				if (eDimension == D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_BUFFER)
				{
					D3D11_BUFFER_DESC sDescDst;
					((ID3D11Buffer*)*m_ppcResource_Map)->GetDesc(&sDescDst);

					// if constant buffer, continue
					if ((sDescDst.BindFlags & D3D11_BIND_CONSTANT_BUFFER) == D3D11_BIND_CONSTANT_BUFFER)
					{
						// get an index which is zero 
						UINT dwIndex = m_dwMappedBuffers;
						bool bFound = false;
						for (UINT dwI = 0; dwI < (UINT)m_asMappedBuffers.size(); dwI++)
						{
							// empty resource index ?
							if ((!m_asMappedBuffers[dwI].m_pcMappedResource) && (!bFound))
							{
								dwIndex = dwI;
								bFound = true;
							}
						}

						// get private data rule index from buffer
						Vireio_Buffer_Rules_Index sRulesIndex;
						sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;
						sRulesIndex.m_dwUpdateCounter = 0;
						UINT dwDataSizeRulesIndex = sizeof(Vireio_Buffer_Rules_Index);
						((ID3D11Buffer*)*m_ppcResource_Map)->GetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, &dwDataSizeRulesIndex, &sRulesIndex);

						// no private data ? rules not addressed
						if (!dwDataSizeRulesIndex) sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;

						// do the map call..
						*(HRESULT*)m_pvReturn = ((ID3D11DeviceContext*)pThis)->Map(*m_ppcResource_Map, *m_pdwSubresource_Map, *m_psMapType, *m_pdwMapFlags, *m_ppsMappedResource);

						// succeeded ?
						if (*(HRESULT*)m_pvReturn == S_OK)
						{
							// resize the vector if necessary
							if (m_dwMappedBuffers >= (UINT)m_asMappedBuffers.size())
								m_asMappedBuffers.resize(m_dwMappedBuffers + 1);

							// set the mapped fields
							m_asMappedBuffers[dwIndex].m_pcMappedResource = *m_ppcResource_Map;
							m_asMappedBuffers[dwIndex].m_psMappedResource = *m_ppsMappedResource;
							m_asMappedBuffers[dwIndex].m_pMappedResourceData = (**m_ppsMappedResource).pData;
							m_asMappedBuffers[dwIndex].m_dwMappedResourceDataSize = sDescDst.ByteWidth;
							m_asMappedBuffers[dwIndex].m_eMapType = *m_psMapType;
							m_asMappedBuffers[dwIndex].m_dwMapFlags = *m_pdwMapFlags;
							m_asMappedBuffers[dwIndex].m_nMapRulesIndex = sRulesIndex.m_nRulesIndex;

							// make buffer address homogenous
							UINT_PTR dwAddress = (UINT_PTR)m_asMappedBuffers[dwIndex].m_pchBuffer11;
							dwAddress |= 0xff; dwAddress++;
							(**m_ppsMappedResource).pData = (LPVOID)dwAddress;

							// update number of mapped Buffers
							m_dwMappedBuffers++;
						}

						// method replaced, immediately return
						nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
						return m_pvReturn;
					}
				}
			}
			break;
#pragma endregion
#pragma region ID3D11DeviceContext::Unmap
		case METHOD_ID3D11DEVICECONTEXT_UNMAP:
			if (!m_ppcResource_Unmap) return nullptr;
			if (!m_pdwSubresource_Unmap) return nullptr;
			if (!*m_ppcResource_Unmap) return nullptr;
			{
				// buffers mapped actually ?
				if (m_dwMappedBuffers)
				{
					// loop through all possibly mapped constant buffers
					for (UINT dwI = 0; dwI < (UINT)m_asMappedBuffers.size(); dwI++)
					{
						// continue only if mapped resource data present and the resource pointer matches the stored pointer
						if ((m_asMappedBuffers[dwI].m_pcMappedResource) && (*m_ppcResource_Unmap == m_asMappedBuffers[dwI].m_pcMappedResource))
						{
							// get homogenous address
							UINT_PTR dwAddress = (UINT_PTR)m_asMappedBuffers[dwI].m_pchBuffer11;
							dwAddress |= 0xff; dwAddress++;

							// do modification only if shader rule assigned !!
							if ((m_asMappedBuffers[dwI].m_nMapRulesIndex >= 0))
							{
								// do modification, first copy to right buffer
								memcpy(m_pchBuffer11Right, (LPVOID)dwAddress, m_asMappedBuffers[dwI].m_dwMappedResourceDataSize);
								DoBufferModification(m_asMappedBuffers[dwI].m_nMapRulesIndex, dwAddress, (UINT_PTR)m_pchBuffer11Right, m_asMappedBuffers[dwI].m_dwMappedResourceDataSize);
							}

							// copy the stored data... 
							memcpy(m_asMappedBuffers[dwI].m_pMappedResourceData, (LPVOID)dwAddress, m_asMappedBuffers[dwI].m_dwMappedResourceDataSize);

							// do the unmap call..
							((ID3D11DeviceContext*)pThis)->Unmap(*m_ppcResource_Unmap, *m_pdwSubresource_Unmap);

							// update right buffer only if shader rule assigned !!
							if ((m_asMappedBuffers[dwI].m_nMapRulesIndex >= 0))
							{
								// get the private data interface
								ID3D11Buffer* pcBuffer = nullptr;
								UINT dwSize = sizeof(pcBuffer);
								(*m_ppcResource_Unmap)->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcBuffer);

								if (pcBuffer)
								{
									// update right buffer
									D3D11_MAPPED_SUBRESOURCE sMapped;
									if (SUCCEEDED(((ID3D11DeviceContext*)pThis)->Map((ID3D11Resource*)pcBuffer, *m_pdwSubresource_Unmap, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, NULL, &sMapped)))
									{
										memcpy(sMapped.pData, (LPVOID)m_pchBuffer11Right, m_asMappedBuffers[dwI].m_dwMappedResourceDataSize);
										((ID3D11DeviceContext*)pThis)->Unmap((ID3D11Resource*)pcBuffer, *m_pdwSubresource_Unmap);
									}

									pcBuffer->Release();
								}
							}

							// set mapped resource data to zero
							m_asMappedBuffers[dwI].m_pcMappedResource = nullptr;
							m_asMappedBuffers[dwI].m_pMappedResourceData = nullptr;
							m_asMappedBuffers[dwI].m_dwMappedResourceDataSize = 0;
							m_asMappedBuffers[dwI].m_psMappedResource = nullptr;

							// update number of mapped Buffers
							m_dwMappedBuffers--;

							// method replaced, immediately return
							nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
							return nullptr;
						}
					}
				}
			}
			break;
#pragma endregion
		}
		return nullptr;
#pragma region ID3D10Device::VSGetConstantBuffers
		// ID3D10Device::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers);
#pragma endregion
#pragma region ID3D10Device::PSGetConstantBuffers
			// ID3D10Device::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers);
#pragma endregion
	}
#elif defined(VIREIO_D3D9)
	switch (eD3DInterface)
	{
	case (int)ITA_D3D9INTERFACES::ITA_D3D9Interfaces::IDirect3DDevice9:
		switch (eD3DMethod)
		{
#pragma region SetVertexShader
		case (int)VMT_IDIRECT3DDEVICE9::SetVertexShader:
			/*if (!m_ppcShader_Vertex) return nullptr;
			//if (m_pcActiveVertexShader == *m_ppcShader_Vertex) return nullptr;
			if (!*m_ppcShader_Vertex)
			{
				m_pcActiveVertexShader = nullptr;
				m_sModifierData.m_pasVSConstantRuleIndices = nullptr;
				nHr = ((IDirect3DDevice9*)pThis)->SetVertexShader(nullptr);
			}
			else
			{
				// set back modified constants... TODO !! make this optionally... shouldn't need that maybe
				// if (m_pcActiveVertexShader)
				// m_pcActiveVertexShader->SetShaderOld((IDirect3DDevice9*)pThis, &m_afRegistersVertex[0]);

				// set new active shader
				m_pcActiveVertexShader = static_cast<IDirect3DManagedStereoShader9<IDirect3DVertexShader9>*>(*m_ppcShader_Vertex);

				// set constant rule indices pointer for stereo splitter
				m_sModifierData.m_pasVSConstantRuleIndices = &m_pcActiveVertexShader->m_asConstantRuleIndices;

				// replace call, set actual shader
				nHr = ((IDirect3DDevice9*)pThis)->SetVertexShader(m_pcActiveVertexShader->GetActualShader());

				// update shader constants for active side
				m_pcActiveVertexShader->SetShader(&m_afRegistersVertex[0]);

				// set modified constants
				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					for (std::vector<Vireio_Constant_Rule_Index_DX9>::size_type nI = 0; nI < m_sModifierData.m_pasVSConstantRuleIndices->size(); nI++)
						((IDirect3DDevice9*)pThis)->SetVertexShaderConstantF((*m_sModifierData.m_pasVSConstantRuleIndices)[nI].m_dwConstantRuleRegister, (*m_sModifierData.m_pasVSConstantRuleIndices)[nI].m_afConstantDataLeft, (*m_sModifierData.m_pasVSConstantRuleIndices)[nI].m_dwConstantRuleRegisterCount);
				}
				else
				{
					for (std::vector<Vireio_Constant_Rule_Index_DX9>::size_type nI = 0; nI < m_sModifierData.m_pasVSConstantRuleIndices->size(); nI++)
						((IDirect3DDevice9*)pThis)->SetVertexShaderConstantF((*m_sModifierData.m_pasVSConstantRuleIndices)[nI].m_dwConstantRuleRegister, (*m_sModifierData.m_pasVSConstantRuleIndices)[nI].m_afConstantDataRight, (*m_sModifierData.m_pasVSConstantRuleIndices)[nI].m_dwConstantRuleRegisterCount);
				}
			}

			// method replaced, immediately return
			nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
			return (void*)&nHr;*/
#pragma endregion
#pragma region SetPixelShader
		case (int)VMT_IDIRECT3DDEVICE9::SetPixelShader:
			/*if (!m_ppcShader_Pixel) return nullptr;
			//if (m_pcActivePixelShader == *m_ppcShader_Pixel) return nullptr;
			if (!*m_ppcShader_Pixel)
			{
				m_pcActivePixelShader = nullptr;
				m_sModifierData.m_pasPSConstantRuleIndices = nullptr;
				nHr = ((IDirect3DDevice9*)pThis)->SetPixelShader(nullptr);
			}
			else
			{
				// set new active shader
				m_pcActivePixelShader = static_cast<IDirect3DManagedStereoShader9<IDirect3DPixelShader9>*>(*m_ppcShader_Pixel);

				// set constant rule indices pointer for stereo splitter
				m_sModifierData.m_pasPSConstantRuleIndices = &m_pcActivePixelShader->m_asConstantRuleIndices;

				// replace call, set actual shader
				nHr = ((IDirect3DDevice9*)pThis)->SetPixelShader(m_pcActivePixelShader->GetActualShader());

				// update shader constants for active side
				m_pcActivePixelShader->SetShader(&m_afRegistersPixel[0]);

				// set modified constants
				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					for (std::vector<Vireio_Constant_Rule_Index_DX9>::size_type nI = 0; nI < m_sModifierData.m_pasPSConstantRuleIndices->size(); nI++)
						((IDirect3DDevice9*)pThis)->SetPixelShaderConstantF((*m_sModifierData.m_pasPSConstantRuleIndices)[nI].m_dwConstantRuleRegister, (*m_sModifierData.m_pasPSConstantRuleIndices)[nI].m_afConstantDataLeft, (*m_sModifierData.m_pasPSConstantRuleIndices)[nI].m_dwConstantRuleRegisterCount);
				}
				else
				{
					for (std::vector<Vireio_Constant_Rule_Index_DX9>::size_type nI = 0; nI < m_sModifierData.m_pasPSConstantRuleIndices->size(); nI++)
						((IDirect3DDevice9*)pThis)->SetPixelShaderConstantF((*m_sModifierData.m_pasPSConstantRuleIndices)[nI].m_dwConstantRuleRegister, (*m_sModifierData.m_pasPSConstantRuleIndices)[nI].m_afConstantDataRight, (*m_sModifierData.m_pasPSConstantRuleIndices)[nI].m_dwConstantRuleRegisterCount);
				}
			}

			// method replaced, immediately return
			nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
			return (void*)&nHr;*/
#pragma endregion
#pragma region SetTransform
		case (int)VMT_IDIRECT3DDEVICE9::SetTransform:
			/*if (!m_psState) return nullptr;
			if (!m_ppsMatrix) return nullptr;

			if ((*m_psState) == D3DTS_VIEW)
			{
				D3DXMATRIX sMatTempLeft;
				D3DXMATRIX sMatTempRight;
				D3DXMATRIX* psMatViewToSet = NULL;
				bool bIsTransformSetTemp = false;

				if (!(*m_ppsMatrix))
				{
					D3DXMatrixIdentity(&sMatTempLeft);
					D3DXMatrixIdentity(&sMatTempRight);
				}
				else
				{
					D3DXMATRIX sMatSource(*(*m_ppsMatrix));

					// If the view is set to the identity then we don't need to perform any adjustments
					if (D3DXMatrixIsIdentity(&sMatSource))
					{
						D3DXMatrixIdentity(&sMatTempLeft);
						D3DXMatrixIdentity(&sMatTempRight);
					}
					else
					{
						// If the view matrix is modified we need to apply left/right adjustments (for stereo rendering)
						sMatTempLeft = sMatSource * m_pcShaderModificationCalculation->LeftViewTransform();
						sMatTempRight = sMatSource * m_pcShaderModificationCalculation->RightViewTransform();

						bIsTransformSetTemp = true;
					}
				}

				// update proxy device
				m_bViewTransformSet = bIsTransformSetTemp;
				m_sMatView[0] = sMatTempLeft;
				m_sMatView[1] = sMatTempRight;

				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					m_psMatViewCurrent = &m_sMatView[0];
				}
				else
				{
					m_psMatViewCurrent = &m_sMatView[1];
				}

				psMatViewToSet = m_psMatViewCurrent;

				// method replaced, immediately return
				nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;

				nHr = ((IDirect3DDevice9*)pThis)->SetTransform((*m_psState), psMatViewToSet);
				return (void*)&nHr;
			}
			else if ((*m_psState) == D3DTS_PROJECTION)
			{
				D3DXMATRIX sMatTempLeft;
				D3DXMATRIX sMatTempRight;
				D3DXMATRIX* psMatProjToSet = NULL;
				bool bIsTransformSetTemp = false;

				if (!(*m_ppsMatrix))
				{
					D3DXMatrixIdentity(&sMatTempLeft);
					D3DXMatrixIdentity(&sMatTempRight);
				}
				else
				{
					D3DXMATRIX sMatSource(*(*m_ppsMatrix));

					// If the view is set to the identity then we don't need to perform any adjustments
					if (D3DXMatrixIsIdentity(&sMatSource))
					{
						D3DXMatrixIdentity(&sMatTempLeft);
						D3DXMatrixIdentity(&sMatTempRight);
					}
					else
					{
						// TODO !! LEFT/RIGHT PROJECTION MATRIX ??
						sMatTempLeft = sMatSource;
						sMatTempRight = sMatSource;

						bIsTransformSetTemp = true;
					}
				}

				// update proxy device
				m_bProjectionTransformSet = bIsTransformSetTemp;
				m_sMatProj[0] = sMatTempLeft;
				m_sMatProj[1] = sMatTempRight;

				if (m_sModifierData.m_eCurrentRenderingSide == RenderPosition::Left)
				{
					m_psMatProjCurrent = &m_sMatProj[0];
				}
				else
				{
					m_psMatProjCurrent = &m_sMatProj[1];
				}

				psMatProjToSet = m_psMatProjCurrent;

				// method replaced, immediately return
				nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;

				nHr = ((IDirect3DDevice9*)pThis)->SetTransform((*m_psState), psMatProjToSet);
				return (void*)&nHr;
			}
			break;*/
#pragma endregion
#pragma region MultiplyTransform
		case (int)VMT_IDIRECT3DDEVICE9::MultiplyTransform:
			//if (!m_psState_Multiply) return nullptr;
			//if (!m_ppsMatrix_Multiply) return nullptr;

			break;
#pragma endregion
#pragma region SetVertexShaderConstantF
		case (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantF:
			/*if (!m_pdwStartRegister) return nullptr;
			if (!m_ppfConstantData) return nullptr;
			if (!m_pdwVector4fCount) return nullptr;

			// ->D3DERR_INVALIDCALL
			if (((*m_pdwStartRegister) >= MAX_DX9_CONSTANT_REGISTERS) || (((*m_pdwStartRegister) + (*m_pdwVector4fCount)) >= MAX_DX9_CONSTANT_REGISTERS))
				return nullptr;

			// Set proxy registers
			memcpy(&m_afRegistersVertex[RegisterIndex(*m_pdwStartRegister)], *m_ppfConstantData, (*m_pdwVector4fCount) * 4 * sizeof(float));

			if (m_pcActiveVertexShader)
			{
				// check proxy shader wether the data gets modified or not
				bool bModified = false;
				m_pcActiveVertexShader->SetShaderConstantF(*m_pdwStartRegister, *m_ppfConstantData, *m_pdwVector4fCount, bModified, m_sModifierData.m_eCurrentRenderingSide, &m_afRegistersVertex[0]);

				// was the data modified for stereo ?
				if (bModified)
				{
					// set modified data
					nHr = ((IDirect3DDevice9*)pThis)->SetVertexShaderConstantF(*m_pdwStartRegister, &m_pcActiveVertexShader->m_afRegisterBuffer[0], *m_pdwVector4fCount);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
					return (void*)&nHr;
				}
			}*/
			break;
#pragma endregion
#pragma region GetVertexShaderConstantF
		case (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantF:
			/*if (!m_pdwStartRegister) return nullptr;
			if (!m_ppfConstantData) return nullptr;
			if (!m_pdwVector4fCount) return nullptr;

			if (m_pcActiveVertexShader)
			{
				// out of range ?
				if (((*m_pdwStartRegister) >= MAX_DX9_CONSTANT_REGISTERS) || (((*m_pdwStartRegister) + (*m_pdwVector4fCount)) >= MAX_DX9_CONSTANT_REGISTERS))
					nHr = D3DERR_INVALIDCALL;
				else
				{
					// get data from proxy register
					*m_ppfConstantData = &m_afRegistersVertex[RegisterIndex((*m_pdwStartRegister))];
					nHr = D3D_OK;
				}

				// method replaced, immediately return
				nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				return (void*)&nHr;
			}*/
			break;
#pragma endregion
#pragma region SetVertexShaderConstantI
		case (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantI:
			/*if (!m_pdwStartRegister) return nullptr;
			if (!m_ppnConstantData) return nullptr;
			if (!m_pdwVector4fCount) return nullptr;*/

			break;
#pragma endregion
#pragma region GetVertexShaderConstantI
		case (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantI:
			break;
#pragma endregion
#pragma region SetVertexShaderConstantB
		case (int)VMT_IDIRECT3DDEVICE9::SetVertexShaderConstantB:
			/*if (!m_pdwStartRegister) return nullptr;
			if (!m_ppbConstantData) return nullptr;
			if (!m_pdwVector4fCount) return nullptr;*/

			break;
#pragma endregion
#pragma region GetVertexShaderConstantB
		case (int)VMT_IDIRECT3DDEVICE9::GetVertexShaderConstantB:
			break;
#pragma endregion
#pragma region SetPixelShaderConstantF
		case (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantF:
			/*if (!m_pdwStartRegister) return nullptr;
			if (!m_ppfConstantData) return nullptr;
			if (!m_pdwVector4fCount) return nullptr;

			// ->D3DERR_INVALIDCALL
			if (((*m_pdwStartRegister) >= MAX_DX9_CONSTANT_REGISTERS) || (((*m_pdwStartRegister) + (*m_pdwVector4fCount)) >= MAX_DX9_CONSTANT_REGISTERS))
				return nullptr;

			// Set proxy registers
			memcpy(&m_afRegistersPixel[RegisterIndex(*m_pdwStartRegister)], *m_ppfConstantData, (*m_pdwVector4fCount) * 4 * sizeof(float));

			if (m_pcActivePixelShader)
			{
				// check proxy shader wether the data gets modified or not
				bool bModified = false;
				m_pcActivePixelShader->SetShaderConstantF(*m_pdwStartRegister, *m_ppfConstantData, *m_pdwVector4fCount, bModified, m_sModifierData.m_eCurrentRenderingSide, &m_afRegistersPixel[0]);

				// was the data modified for stereo ?
				if (bModified)
				{
					// set modified data
					nHr = ((IDirect3DDevice9*)pThis)->SetPixelShaderConstantF(*m_pdwStartRegister, &m_pcActivePixelShader->m_afRegisterBuffer[0], *m_pdwVector4fCount);

					// method replaced, immediately return
					nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
					return (void*)&nHr;
				}
			}*/
			break;
#pragma endregion
#pragma region GetPixelShaderConstantF
		case (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantF:
			/*if (!m_pdwStartRegister) return nullptr;
			if (!m_ppfConstantData) return nullptr;
			if (!m_pdwVector4fCount) return nullptr;

			if (m_pcActivePixelShader)
			{
				// out of range ?
				if (((*m_pdwStartRegister) >= MAX_DX9_CONSTANT_REGISTERS) || (((*m_pdwStartRegister) + (*m_pdwVector4fCount)) >= MAX_DX9_CONSTANT_REGISTERS))
					nHr = D3DERR_INVALIDCALL;
				else
				{
					// get data from proxy register
					*m_ppfConstantData = &m_afRegistersPixel[RegisterIndex((*m_pdwStartRegister))];
					nHr = D3D_OK;
				}

				// method replaced, immediately return
				nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
				return (void*)&nHr;
			}*/
			break;
#pragma endregion
#pragma region SetPixelShaderConstantI
		case (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantI:
			break;
#pragma endregion
#pragma region GetPixelShaderConstantI
		case (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantI:
			break;
#pragma endregion
#pragma region SetPixelShaderConstantB
		case (int)VMT_IDIRECT3DDEVICE9::SetPixelShaderConstantB:
			break;
#pragma endregion
#pragma region GetPixelShaderConstantB
		case (int)VMT_IDIRECT3DDEVICE9::GetPixelShaderConstantB:
			break;
#pragma endregion
#pragma region SetStreamSource
		case (int)VMT_IDIRECT3DDEVICE9::SetStreamSource:
			/*if (!m_punStreamNumber) return nullptr;
			if (!m_ppcStreamData) return nullptr;
			if (!m_punOffsetInBytes) return nullptr;
			if (!m_punStride) return nullptr;
			*/
			break;
#pragma endregion
#pragma region GetStreamSource
		case (int)VMT_IDIRECT3DDEVICE9::GetStreamSource:
			/*if (!m_punStreamNumber_Get) return nullptr;
			if (!m_pppcStreamData_Get) return nullptr;
			if (!m_ppunOffsetInBytes_Get) return nullptr;
			if (!m_ppunStride_Get) return nullptr;
			*/
			break;
#pragma endregion
#pragma region CreateVertexShader
		case (int)VMT_IDIRECT3DDEVICE9::CreateVertexShader:
			/*if (!m_ppunFunction) return nullptr;
			if (!m_pppcShader_Vertex) return nullptr;
			if (!*m_pppcShader_Vertex) return nullptr;
			{
				// create the actual shader
				IDirect3DVertexShader9* pcActualVShader = NULL;
				nHr = ((IDirect3DDevice9*)pThis)->CreateVertexShader(*m_ppunFunction, &pcActualVShader);

				// create the proxy shader
				if (SUCCEEDED(nHr))
				{
					**m_pppcShader_Vertex = new IDirect3DManagedStereoShader9<IDirect3DVertexShader9>(pcActualVShader, (IDirect3DDevice9*)pThis, &m_asConstantRules, &m_aunGlobalConstantRuleIndices, &m_asShaderSpecificRuleIndices, Vireio_Supported_Shaders::VertexShader);
				}
				else
				{
					OutputDebugString(L"[MAM] Failed to create the vertex shader!");
				}

				if (**m_pppcShader_Vertex)
				{
					// get the hash
					UINT unHash = ((IDirect3DManagedStereoShader9<IDirect3DVertexShader9>*)**m_pppcShader_Vertex)->GetShaderHash();

					// loop through shader list wether hash is present
					bool bPresent = false;
					for (UINT unI = 0; unI < (UINT)m_asVShaders.size(); unI++)
					{
						if (m_asVShaders[unI].dwHashCode == unHash)
							bPresent = true;
					}

					// add to shader list
					if (!bPresent)
					{
						Vireio_D3D9_Shader sShaderDesc = {};
						sShaderDesc.dwHashCode = unHash;

						// get the constant descriptions from that shader
						std::vector<SAFE_D3DXCONSTANT_DESC>* pasConstants = ((IDirect3DManagedStereoShader9<IDirect3DVertexShader9>*)**m_pppcShader_Vertex)->GetConstantDescriptions();
						sShaderDesc.asConstantDescriptions = std::vector<SAFE_D3DXCONSTANT_DESC>(pasConstants->begin(), pasConstants->end());

						// copy the name (LPCSTR) to a new string
						for (UINT unI = 0; unI < (UINT)pasConstants->size(); unI++)
						{
							// copy name string
							sShaderDesc.asConstantDescriptions[unI].Name = (*pasConstants)[unI].Name;
						}
						m_asVShaders.push_back(sShaderDesc);
					}

				}

				// method replaced, immediately return
				nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
			}
			return (void*)&nHr;*/
#pragma endregion
#pragma region CreatePixelShader
		case (int)VMT_IDIRECT3DDEVICE9::CreatePixelShader:
			/*if (!m_ppunFunction) return nullptr;
			if (!m_pppcShader_Pixel) return nullptr;
			if (!*m_pppcShader_Pixel) return nullptr;
			{
				// create the actual shader
				IDirect3DPixelShader9* pcActualPShader = NULL;
				nHr = ((IDirect3DDevice9*)pThis)->CreatePixelShader(*m_ppunFunction, &pcActualPShader);

				// create the proxy shader
				if (SUCCEEDED(nHr))
				{
					**m_pppcShader_Pixel = new IDirect3DManagedStereoShader9<IDirect3DPixelShader9>(pcActualPShader, (IDirect3DDevice9*)pThis, &m_asConstantRules, &m_aunGlobalConstantRuleIndices, &m_asShaderSpecificRuleIndices, Vireio_Supported_Shaders::PixelShader);
				}
				else
				{
					OutputDebugString(L"[MAM] Failed to create the pixel shader!");
				}

				if (**m_pppcShader_Pixel)
				{
					// get the hash
					UINT unHash = ((IDirect3DManagedStereoShader9<IDirect3DPixelShader9>*)**m_pppcShader_Pixel)->GetShaderHash();

					// loop through shader list wether hash is present
					bool bPresent = false;
					for (UINT unI = 0; unI < (UINT)m_asPShaders.size(); unI++)
					{
						if (m_asPShaders[unI].dwHashCode == unHash)
							bPresent = true;
					}

					// add to shader list
					if (!bPresent)
					{
						Vireio_D3D9_Shader sShaderDesc = {};
						sShaderDesc.dwHashCode = unHash;

						// get the constant descriptions from that shader
						std::vector<SAFE_D3DXCONSTANT_DESC>* pasConstants = ((IDirect3DManagedStereoShader9<IDirect3DPixelShader9>*)**m_pppcShader_Pixel)->GetConstantDescriptions();
						sShaderDesc.asConstantDescriptions = std::vector<SAFE_D3DXCONSTANT_DESC>(pasConstants->begin(), pasConstants->end());

						// copy the name (LPCSTR) to a new string
						for (UINT unI = 0; unI < (UINT)pasConstants->size(); unI++)
						{
							// copy name string
							sShaderDesc.asConstantDescriptions[unI].Name = (*pasConstants)[unI].Name;
						}
						m_asPShaders.push_back(sShaderDesc);
					}

				}

				// method replaced, immediately return
				nProvokerIndex |= AQU_PluginFlags::ImmediateReturnFlag;
			}
			return (void*)&nHr;*/
			break;
#pragma endregion
#pragma region Apply
#pragma endregion
		}
	}
#endif

	return nullptr;
}

/**
* There's some windows event on our node.
***/
void MatrixModifier::WindowsEvent(UINT msg, WPARAM wParam, LPARAM lParam)
{
	//	// multiply mouse coords by 4 due to Aquilinus workspace architecture
	//	Vireio_GUI_Event sEvent = m_pcVireioGUI->WindowsEvent(msg, wParam, lParam, 4);
	//	switch (sEvent.eType)
	//	{
	//		case NoEvent:
	//			break;
	//		case ChangedToNext:
	//		case ChangedToPrevious:
	//			if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::ShadersPage])
	//			{
	//				if (sEvent.dwIndexOfControl == m_sPageShader.m_dwShaderType)
	//				{
	//					// unselect all list boxes
	//					m_pcVireioGUI->UnselectCurrentSelection(m_sPageShader.m_dwHashCodes);
	//					m_pcVireioGUI->UnselectCurrentSelection(m_sPageShader.m_dwCurrentConstants);
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//					m_pcVireioGUI->UnselectCurrentSelection(m_sPageShader.m_dwCurrentBuffersizes);
	//#endif
	//
	//					// set new value and the text list
	//					m_eChosenShaderType = (Vireio_Supported_Shaders)sEvent.dwNewValue;
	//					if (m_eChosenShaderType == Vireio_Supported_Shaders::VertexShader)
	//					{
	//						m_pcVireioGUI->SetNewTextList(m_sPageShader.m_dwHashCodes, &m_aszVShaderHashCodes);
	//					}
	//					else if (m_eChosenShaderType == Vireio_Supported_Shaders::PixelShader)
	//					{
	//						m_pcVireioGUI->SetNewTextList(m_sPageShader.m_dwHashCodes, &m_aszPShaderHashCodes);
	//					}
	//				}
	//			}
	//			else if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::ShaderRulesPage])
	//			{
	//				if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwRegisterCount)
	//				{
	//					if (sEvent.dwNewValue == 0)
	//						m_sPageGameShaderRules.m_dwRegCountValue = 1;
	//					else if (sEvent.dwNewValue == 1)
	//						m_sPageGameShaderRules.m_dwRegCountValue = 2;
	//					else if (sEvent.dwNewValue == 2)
	//						m_sPageGameShaderRules.m_dwRegCountValue = 4;
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwOperationToApply)
	//				{
	//					m_sPageGameShaderRules.m_dwOperationValue = sEvent.dwNewValue;
	//				}
	//			}
	//			else if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::GameSettingsPage])
	//			{
	//				if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwRollImpl)
	//					m_sGameConfiguration.nRollImpl = sEvent.nNewValue;
	//
	//				// update view transform
	//				m_pcShaderViewAdjustment->Load(m_sGameConfiguration);
	//				m_pcShaderViewAdjustment->UpdateProjectionMatrices(((float)1920.0f / (float)1080.0f) * m_sGameConfiguration.fAspectMultiplier, m_sGameConfiguration.fPFOV);
	//				m_pcShaderViewAdjustment->ComputeViewTransforms();
	//			}
	//			break;
	//		case ChangedToValue:
	//#pragma region ChangedToValue
	//			// "Game Settings" page
	//			if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::GameSettingsPage])
	//			{
	//				// get new value from the control
	//				if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwGameSeparation)
	//					m_sGameConfiguration.fWorldScaleFactor = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwConvergence)
	//					m_sGameConfiguration.fConvergence = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwAspectMultiplier)
	//					m_sGameConfiguration.fAspectMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwVRboostMinShaderCount)
	//					m_sGameConfiguration.dwVRboostMinShaderCount = sEvent.dwNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwVRboostMaxShaderCount)
	//					m_sGameConfiguration.dwVRboostMaxShaderCount = sEvent.dwNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwConvergenceEnabled)
	//					m_sGameConfiguration.bConvergenceEnabled = sEvent.bNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwYawMultiplier)
	//					m_sGameConfiguration.fYawMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwPitchMultiplier)
	//					m_sGameConfiguration.fPitchMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwRollMultiplier)
	//					m_sGameConfiguration.fRollMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwPositionMultiplier)
	//					m_sGameConfiguration.fPositionMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwPositionXMultiplier)
	//					m_sGameConfiguration.fPositionXMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwPositionYMultiplier)
	//					m_sGameConfiguration.fPositionYMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwPositionZMultiplier)
	//					m_sGameConfiguration.fPositionZMultiplier = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwPFOV)
	//					m_sGameConfiguration.fPFOV = sEvent.fNewValue;
	//				else if (sEvent.dwIndexOfControl == m_sPageGameSettings.m_dwPFOVToggle)
	//					m_sGameConfiguration.bPFOVToggle = sEvent.bNewValue;
	//
	//				// update view transform
	//				m_pcShaderViewAdjustment->Load(m_sGameConfiguration);
	//				m_pcShaderViewAdjustment->UpdateProjectionMatrices(((float)1920.0f / (float)1080.0f) * m_sGameConfiguration.fAspectMultiplier, m_sGameConfiguration.fPFOV);
	//				m_pcShaderViewAdjustment->ComputeViewTransforms();
	//			}
	//			else if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::ShadersPage])
	//			{
	//				// which shader is chosen ?
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//				std::vector<Vireio_D3D11_Shader>* pasShaders;
	//#elif defined(VIREIO_D3D9)
	//				std::vector<Vireio_D3D9_Shader>* pasShaders;
	//#endif
	//				std::vector<std::wstring>* pasShaderHashCodes;
	//				std::vector<UINT>* padwShaderHashCodes;
	//				if (m_eChosenShaderType == Vireio_Supported_Shaders::VertexShader)
	//				{
	//					pasShaders = &m_asVShaders;
	//					pasShaderHashCodes = &m_aszVShaderHashCodes;
	//					padwShaderHashCodes = &m_adwVShaderHashCodes;
	//				}
	//				else if (m_eChosenShaderType == Vireio_Supported_Shaders::PixelShader)
	//				{
	//					pasShaders = &m_asPShaders;
	//					pasShaderHashCodes = &m_aszPShaderHashCodes;
	//					padwShaderHashCodes = &m_adwPShaderHashCodes;
	//				}
	//				// "Sort Shader List" button
	//				if (sEvent.dwIndexOfControl == m_sPageShader.m_dwSort)
	//				{
	//					// set sorting bool
	//					m_bSortShaderList = sEvent.bNewValue;
	//				}
	//				// "Hash Codes" - List
	//				else if (sEvent.dwIndexOfControl == m_sPageShader.m_dwHashCodes)
	//				{
	//					INT nSelection = 0;
	//
	//					// get current selection of the shader constant list
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageShader.m_dwHashCodes);
	//
	//					if ((nSelection >= 0) && (nSelection < (INT)(*padwShaderHashCodes).size()))
	//						m_dwCurrentChosenShaderHashCode = (*padwShaderHashCodes)[nSelection];
	//					else
	//						m_dwCurrentChosenShaderHashCode = 0;
	//
	//					// fill current shader data lists, first unselect a possible selection
	//					m_pcVireioGUI->UnselectCurrentSelection(m_sPageShader.m_dwCurrentConstants);
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//					m_pcVireioGUI->UnselectCurrentSelection(m_sPageShader.m_dwCurrentBuffersizes);
	//#endif
	//					m_aszShaderConstantsCurrent = std::vector<std::wstring>();
	//					m_aszShaderBuffersizes = std::vector<std::wstring>();
	//
	//					if (m_dwCurrentChosenShaderHashCode)
	//					{
	//						// find the hash code in the shader list
	//						UINT dwIndex = 0;
	//						for (UINT dwI = 0; dwI < (UINT)(*pasShaders).size(); dwI++)
	//						{
	//							if (m_dwCurrentChosenShaderHashCode == (*pasShaders)[dwI].dwHashCode)
	//							{
	//								dwIndex = dwI;
	//								dwI = (UINT)(*pasShaders).size();
	//							}
	//						}
	//
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//						// add all constant names to the current shader constant list
	//						for (UINT dwJ = 0; dwJ < (UINT)(*pasShaders)[dwIndex].asBuffers.size(); dwJ++)
	//						{
	//							std::wstringstream szSize; szSize << (*pasShaders)[dwIndex].asBuffers[dwJ].dwSize;
	//							m_aszShaderBuffersizes.push_back(szSize.str());
	//
	//							for (UINT dwK = 0; dwK < (UINT)(*pasShaders)[dwIndex].asBuffers[dwJ].asVariables.size(); dwK++)
	//							{
	//								// convert to wstring
	//								std::string szNameA = std::string((*pasShaders)[dwIndex].asBuffers[dwJ].asVariables[dwK].szName);
	//								std::wstring szName(szNameA.begin(), szNameA.end());
	//
	//								m_aszShaderConstantsCurrent.push_back(szName);
	//							}
	//						}
	//
	//						m_aszShaderBuffersizes.push_back(std::wstring(L">-----unaccounted:"));
	//
	//						// add all unaccounted buffer sizes to the buffer sizes list
	//						for (UINT dwI = 0; dwI < (UINT)(*pasShaders)[dwIndex].asBuffersUnaccounted.size(); dwI++)
	//						{
	//							std::wstringstream szReg; szReg << (*pasShaders)[dwIndex].asBuffersUnaccounted[dwI].dwRegister;
	//							m_aszShaderBuffersizes.push_back(szReg.str());
	//							std::wstringstream szSize; szSize << (*pasShaders)[dwIndex].asBuffersUnaccounted[dwI].dwSize;
	//							m_aszShaderBuffersizes.push_back(szSize.str());
	//						}
	//#elif defined(VIREIO_D3D9)
	//						// add all constant names to the current shader constant list
	//						for (UINT dwK = 0; dwK < (UINT)(*pasShaders)[dwIndex].asConstantDescriptions.size(); dwK++)
	//						{
	//							// convert to wstring
	//							std::string szNameA = std::string((*pasShaders)[dwIndex].asConstantDescriptions[dwK].Name);
	//							std::wstring szName(szNameA.begin(), szNameA.end());
	//
	//							m_aszShaderConstantsCurrent.push_back(szName);
	//						}
	//#endif
	//					}
	//				}
	//				// "Shader Constants" - List
	//				else if (sEvent.dwIndexOfControl == m_sPageShader.m_dwCurrentConstants)
	//				{
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//					INT nSelection = 0;
	//
	//					// get current selection of the shader constant list
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageShader.m_dwCurrentConstants);
	//
	//					if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderConstantsCurrent.size()))
	//					{
	//						// unselect and clear buffer size list
	//						m_pcVireioGUI->UnselectCurrentSelection(m_sPageShader.m_dwCurrentBuffersizes);
	//						m_aszShaderBuffersizes = std::vector<std::wstring>();
	//
	//						// get a number vector
	//						std::vector<UINT> adwBufferSizes;
	//
	//						// loop through buffers, compare all entries
	//						for (UINT dwI = 0; dwI < (UINT)(*pasShaders).size(); dwI++)
	//						{
	//							for (UINT dwJ = 0; dwJ < (UINT)(*pasShaders)[dwI].asBuffers.size(); dwJ++)
	//							{
	//								if (nSelection < (INT)(*pasShaders)[dwI].asBuffers[dwJ].asVariables.size())
	//								{
	//									// compare the two strings, first convert to wstring
	//									std::string szNameA = std::string((*pasShaders)[dwI].asBuffers[dwJ].asVariables[nSelection].szName);
	//									std::wstring szName(szNameA.begin(), szNameA.end());
	//									if (szName.compare(m_aszShaderConstantsCurrent[nSelection]) == 0)
	//									{
	//										UINT dwSize = (*pasShaders)[dwI].asBuffers[dwJ].dwSize;
	//										// add buffer size to list if not present
	//										if (std::find(adwBufferSizes.begin(), adwBufferSizes.end(), dwSize) == adwBufferSizes.end())
	//											adwBufferSizes.push_back(dwSize);
	//									}
	//								}
	//							}
	//						}
	//
	//						// fill the buffer sizes entries new
	//						for (UINT dwJ = 0; dwJ < (UINT)adwBufferSizes.size(); dwJ++)
	//						{
	//							std::wstringstream szSize; szSize << adwBufferSizes[dwJ];
	//							m_aszShaderBuffersizes.push_back(szSize.str());
	//						}
	//					}
	//#endif
	//				}
	//			}
	//			else if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::ShaderRulesPage])
	//			{
	//				// "Rule Indices" list
	//				if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwRuleIndices)
	//				{
	//					INT nSelection = 0;
	//
	//					// get current selection of the shader constant list
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageGameShaderRules.m_dwRuleIndices);
	//
	//					if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderRuleIndices.size()))
	//					{
	//						// call the method to fill the rule data
	//						FillShaderRuleData((UINT)nSelection);
	//					}
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwConstantName)
	//				{
	//					m_sPageGameShaderRules.m_bConstantName = sEvent.bNewValue;
	//
	//					// if true -> get string from clipboard
	//					if (sEvent.bNewValue)
	//					{
	//						std::string szClipboard = GetClipboardText();
	//						m_sPageGameShaderRules.m_szConstantName = std::wstring(szClipboard.begin(), szClipboard.end());
	//					}
	//					// if false -> delete current value
	//					else
	//					{
	//						m_sPageGameShaderRules.m_szConstantName = std::wstring();
	//					}
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwPartialName)
	//				{
	//					m_sPageGameShaderRules.m_bPartialName = sEvent.bNewValue;
	//
	//					// if true -> get string from clipboard
	//					if (sEvent.bNewValue)
	//					{
	//						std::string szClipboard = GetClipboardText();
	//						m_sPageGameShaderRules.m_szPartialName = std::wstring(szClipboard.begin(), szClipboard.end());
	//					}
	//					// if false -> delete current value
	//					else
	//					{
	//						m_sPageGameShaderRules.m_szPartialName = std::wstring();
	//					}
	//				}
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwBufferIndex)
	//				{
	//					// set new value and delete the buffer index sizes debug vector
	//					m_sPageGameShaderRules.m_bBufferIndex = sEvent.bNewValue;
	//					m_aunBufferIndexSizesDebug = std::vector<UINT>();
	//
	//					// if true -> try to parse a number from the clipboard
	//					if (sEvent.bNewValue)
	//					{
	//						// get the clipboard text as a stream
	//						std::wstringstream szClipboard;
	//						szClipboard << GetClipboardText().c_str();
	//
	//						// parse to a number
	//						UINT dwValue = 0;
	//						szClipboard >> dwValue;
	//
	//						// delete the stream, parse the number back to the stream
	//						szClipboard = std::wstringstream();
	//						szClipboard << dwValue;
	//
	//						// and set the text
	//						m_sPageGameShaderRules.m_szBufferIndex = szClipboard.str();
	//					}
	//					// if false -> delete current value
	//					else
	//					{
	//						m_sPageGameShaderRules.m_szBufferIndex = std::wstring();
	//					}
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwBufferSize)
	//				{
	//					m_sPageGameShaderRules.m_bBufferSize = sEvent.bNewValue;
	//
	//					// if true -> try to parse a number from the clipboard
	//					if (sEvent.bNewValue)
	//					{
	//						// get the clipboard text as a stream
	//						std::wstringstream szClipboard;
	//						szClipboard << GetClipboardText().c_str();
	//
	//						// parse to a number
	//						UINT dwValue = 0;
	//						szClipboard >> dwValue;
	//
	//						// delete the stream, parse the number back to the stream
	//						szClipboard = std::wstringstream();
	//						szClipboard << dwValue;
	//
	//						// and set the text
	//						m_sPageGameShaderRules.m_szBufferSize = szClipboard.str();
	//					}
	//					// if false -> delete current value
	//					else
	//					{
	//						m_sPageGameShaderRules.m_szBufferSize = std::wstring();
	//					}
	//				}
	//#endif
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwStartRegIndex)
	//				{
	//					// set new value
	//					m_sPageGameShaderRules.m_bStartRegIndex = sEvent.bNewValue;
	//
	//					// if true -> try to parse a number from the clipboard
	//					if (sEvent.bNewValue)
	//					{
	//						// get the clipboard text as a stream
	//						std::wstringstream szClipboard;
	//						szClipboard << GetClipboardText().c_str();
	//
	//						// parse to a number
	//						UINT dwValue = 0;
	//						szClipboard >> dwValue;
	//
	//						// delete the stream, parse the number back to the stream
	//						szClipboard = std::wstringstream();
	//						szClipboard << dwValue;
	//
	//						// and set the text
	//						m_sPageGameShaderRules.m_szStartRegIndex = szClipboard.str();
	//					}
	//					// if false -> delete current value
	//					else
	//					{
	//						m_sPageGameShaderRules.m_szStartRegIndex = std::wstring();
	//					}
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwTranspose)
	//					m_sPageGameShaderRules.m_bTranspose = sEvent.bNewValue;
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwBufferIndexDebug)
	//				{
	//					// set new value
	//					m_bBufferIndexDebug = sEvent.bNewValue;
	//				}
	//#endif
	//			}
	//			break;
	//#pragma endregion
	//		case ChangedToText:
	//			break;
	//		case Pressed:
	//#pragma region Pressed:
	//			// "Debug" page
	//			if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::DebugPage])
	//			{
	//				if (sEvent.dwIndexOfControl == m_sPageDebug.m_dwGrab)
	//				{
	//					// set the grab debug bool to true
	//					m_bGrabDebug = true;
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageDebug.m_dwClear)
	//				{
	//					// clear debug string list
	//					m_aszDebugTrace = std::vector<std::wstring>();
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageDebug.m_dwOptions)
	//				{
	//					// new debug option chosen
	//					m_eDebugOption = (Debug_Grab_Options)sEvent.dwNewValue;
	//				}
	//			}
	//			// "Shaders" page
	//			else if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::ShadersPage])
	//			{
	//				// which shader is chosen ?
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//				std::vector<Vireio_D3D11_Shader>* pasShaders;
	//#elif defined(VIREIO_D3D9)
	//				std::vector<Vireio_D3D9_Shader>* pasShaders;
	//#endif
	//				std::vector<std::wstring>* pasShaderHashCodes;
	//				std::vector<UINT>* padwShaderHashCodes;
	//				if (m_eChosenShaderType == Vireio_Supported_Shaders::VertexShader)
	//				{
	//					pasShaders = &m_asVShaders;
	//					pasShaderHashCodes = &m_aszVShaderHashCodes;
	//					padwShaderHashCodes = &m_adwVShaderHashCodes;
	//				}
	//				else if (m_eChosenShaderType == Vireio_Supported_Shaders::PixelShader)
	//				{
	//					pasShaders = &m_asPShaders;
	//					pasShaderHashCodes = &m_aszPShaderHashCodes;
	//					padwShaderHashCodes = &m_adwPShaderHashCodes;
	//				}
	//
	//				// "Update Shader Hash Code List" - Button
	//				if (sEvent.dwIndexOfControl == m_sPageShader.m_dwUpdate)
	//				{
	//					// test the shader constant list for updates...
	//					if ((*pasShaderHashCodes).size() < (*pasShaders).size())
	//					{
	//						// loop through new shaders
	//						for (UINT dwI = (UINT)(*pasShaderHashCodes).size(); dwI < (UINT)(*pasShaders).size(); dwI++)
	//						{
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//							// loop through shader buffers
	//							for (UINT dwJ = 0; dwJ < (UINT)(*pasShaders)[dwI].asBuffers.size(); dwJ++)
	//							{
	//								for (UINT dwK = 0; dwK < (UINT)(*pasShaders)[dwI].asBuffers[dwJ].asVariables.size(); dwK++)
	//								{
	//									// convert to wstring
	//									std::string szNameA = std::string((*pasShaders)[dwI].asBuffers[dwJ].asVariables[dwK].szName);
	//									std::wstring szName(szNameA.begin(), szNameA.end());
	//
	//									// constant available in list ?
	//									if (std::find(m_aszShaderConstants.begin(), m_aszShaderConstants.end(), szName) == m_aszShaderConstants.end())
	//									{
	//										m_aszShaderConstants.push_back(szName);
	//										m_aszShaderConstantsA.push_back(szNameA);
	//									}
	//								}
	//							}
	//#elif defined(VIREIO_D3D9)
	//							// loop through constant descriptions for that shader
	//							for (UINT dwK = 0; dwK < (UINT)(*pasShaders)[dwI].asConstantDescriptions.size(); dwK++)
	//							{
	//								// convert to wstring
	//								std::string szNameA = std::string((*pasShaders)[dwI].asConstantDescriptions[dwK].Name);
	//								std::wstring szName(szNameA.begin(), szNameA.end());
	//
	//								// constant available in list ?
	//								if (std::find(m_aszShaderConstants.begin(), m_aszShaderConstants.end(), szName) == m_aszShaderConstants.end())
	//								{
	//									m_aszShaderConstants.push_back(szName);
	//									m_aszShaderConstantsA.push_back(szNameA);
	//								}
	//							}
	//#endif
	//							// add shader hash to lists
	//							std::wstring szHash = std::to_wstring((*pasShaders)[dwI].dwHashCode);
	//							(*pasShaderHashCodes).push_back(szHash);
	//							(*padwShaderHashCodes).push_back((*pasShaders)[dwI].dwHashCode);
	//						}
	//					}
	//				}
	//				// "-> To Name" - Button
	//				else if (sEvent.dwIndexOfControl == m_sPageShader.m_dwToName)
	//				{
	//					INT nSelection = 0;
	//
	//					// get current selection of the shader constant list
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageShader.m_dwCurrentConstants);
	//
	//					if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderConstantsCurrent.size()))
	//					{
	//						// set the name to the name control of the shader rules page
	//						m_sPageGameShaderRules.m_szConstantName = m_aszShaderConstantsCurrent[nSelection];
	//					}
	//				}
	//				// "-> To Register" - Button
	//				else if (sEvent.dwIndexOfControl == m_sPageShader.m_dwToRegister)
	//				{
	//					INT nSelection = 0;
	//
	//					// get current selection of the shader constant list
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageShader.m_dwHashCodes);
	//
	//					if ((nSelection >= 0) && (nSelection < (INT)(*padwShaderHashCodes).size()))
	//						m_dwCurrentChosenShaderHashCode = (*padwShaderHashCodes)[nSelection];
	//					else
	//						m_dwCurrentChosenShaderHashCode = 0;
	//
	//					if (m_dwCurrentChosenShaderHashCode)
	//					{
	//						// find the hash code in the shader list
	//						UINT dwIndex = 0;
	//						for (UINT dwI = 0; dwI < (UINT)(*pasShaders).size(); dwI++)
	//						{
	//							if (m_dwCurrentChosenShaderHashCode == (*pasShaders)[dwI].dwHashCode)
	//							{
	//								dwIndex = dwI;
	//								dwI = (UINT)(*pasShaders).size();
	//							}
	//						}
	//
	//						INT nSelection = 0;
	//
	//						// get current selection of the shader constant list
	//						nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageShader.m_dwCurrentConstants);
	//
	//						if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderConstantsCurrent.size()))
	//						{
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//							if ((*pasShaders)[dwIndex].asBuffers.size())
	//							{
	//								UINT dwBufferIndex = 0;
	//
	//								// get the buffer index
	//								while (dwBufferIndex < (UINT)(*pasShaders)[dwIndex].asBuffers.size())
	//								{
	//									if (nSelection >= (INT)(*pasShaders)[dwIndex].asBuffers[dwBufferIndex].asVariables.size())
	//									{
	//										nSelection -= (INT)(*pasShaders)[dwIndex].asBuffers[dwBufferIndex].asVariables.size();
	//										dwBufferIndex++;
	//									}
	//									else break;
	//								}
	//
	//								// set data based on selection
	//								if (nSelection < (INT)(*pasShaders)[dwIndex].asBuffers[dwBufferIndex].asVariables.size())
	//								{
	//									// set new start register string on the shader rules page (=offset/16)
	//									std::wstringstream szStartOffset; szStartOffset << ((*pasShaders)[dwIndex].asBuffers[dwBufferIndex].asVariables[nSelection].dwStartOffset >> 4);
	//									m_sPageGameShaderRules.m_szStartRegIndex = szStartOffset.str();
	//
	//									// also set the buffer index if present
	//									if (dwBufferIndex < (UINT)(*pasShaders)[dwIndex].asBuffersUnaccounted.size())
	//									{
	//										// set the new register
	//										std::wstringstream szStartRegister; szStartRegister << (*pasShaders)[dwIndex].asBuffersUnaccounted[dwBufferIndex].dwRegister;
	//										m_sPageGameShaderRules.m_szBufferIndex = szStartRegister.str();
	//									}
	//								}
	//							}
	//#endif
	//						}
	//					}
	//				}
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//				// "-> To Buffer Size" - Button
	//				else if (sEvent.dwIndexOfControl == m_sPageShader.m_dwToBufferSize)
	//				{
	//					INT nSelection = 0;
	//
	//					// get current selection of the shader constant list
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageShader.m_dwCurrentBuffersizes);
	//
	//					if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderBuffersizes.size()))
	//					{
	//						// set the name to the name control of the shader rules page
	//						m_sPageGameShaderRules.m_szBufferSize = m_aszShaderBuffersizes[nSelection];
	//					}
	//				}
	//				// "-> To Buffer Index" - Button
	//				else if (sEvent.dwIndexOfControl == m_sPageShader.m_dwToBufferIndex)
	//				{
	//					INT nSelection = 0;
	//
	//					// get current selection of the shader constant list
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageShader.m_dwCurrentBuffersizes);
	//
	//					if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderBuffersizes.size()))
	//					{
	//						// set the name to the name control of the shader rules page
	//						m_sPageGameShaderRules.m_szBufferIndex = m_aszShaderBuffersizes[nSelection];
	//					}
	//				}
	//				// "-> To Fetched Hash" - Button
	//				else if (sEvent.dwIndexOfControl == m_sPageShader.m_dwToFetchedList)
	//				{
	//					if (m_dwCurrentChosenShaderHashCode)
	//					{
	//						// add to hash code list
	//						m_aunFetchedHashCodes.push_back(m_dwCurrentChosenShaderHashCode);
	//
	//						// fill the string list
	//						FillFetchedHashCodeList();
	//					}
	//				}
	//#endif				
	//			}
	//			// "Shader Rules" page
	//			else if (sEvent.dwIndexOfPage == m_adwPageIDs[GUI_Pages::ShaderRulesPage])
	//			{
	//				if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwCreate)
	//				{
	//					// create rule, first parse input data
	//					Vireio_Constant_Modification_Rule sRule = Vireio_Constant_Modification_Rule();
	//
	//					// full name
	//					if (m_sPageGameShaderRules.m_bConstantName)
	//					{
	//						std::string sz(m_sPageGameShaderRules.m_szConstantName.begin(), m_sPageGameShaderRules.m_szConstantName.end());
	//						sRule.m_szConstantName = sz;
	//						sRule.m_bUseName = true;
	//					}
	//
	//					// partial name
	//					if (m_sPageGameShaderRules.m_bPartialName)
	//					{
	//						std::string sz(m_sPageGameShaderRules.m_szPartialName.begin(), m_sPageGameShaderRules.m_szPartialName.end());
	//						sRule.m_szConstantName = sz;
	//						sRule.m_bUsePartialNameMatch = true;
	//					}
	//
	//					// start register
	//					if (m_sPageGameShaderRules.m_bStartRegIndex)
	//					{
	//						std::wstringstream sz = std::wstringstream(m_sPageGameShaderRules.m_szStartRegIndex);
	//						sz >> sRule.m_dwStartRegIndex;
	//						sRule.m_bUseStartRegIndex = true;
	//					}
	//
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//					// buffer size
	//					if (m_sPageGameShaderRules.m_bBufferSize)
	//					{
	//						std::wstringstream sz = std::wstringstream(m_sPageGameShaderRules.m_szBufferSize);
	//						sz >> sRule.m_dwBufferSize;
	//						sRule.m_bUseBufferSize = true;
	//					}
	//
	//					// buffer index
	//					if (m_sPageGameShaderRules.m_bBufferIndex)
	//					{
	//						std::wstringstream sz = std::wstringstream(m_sPageGameShaderRules.m_szBufferIndex);
	//						sz >> sRule.m_dwBufferIndex;
	//						sRule.m_bUseBufferIndex = true;
	//					}
	//#endif
	//
	//					// operation
	//					sRule.m_dwOperationToApply = m_sPageGameShaderRules.m_dwOperationValue;
	//
	//					// register count
	//					sRule.m_dwRegisterCount = m_sPageGameShaderRules.m_dwRegCountValue;
	//
	//					// transpose
	//					sRule.m_bTranspose = m_sPageGameShaderRules.m_bTranspose;
	//
	//					// create modification
	//					if (sRule.m_dwRegisterCount == 1)
	//						sRule.m_pcModification = ShaderConstantModificationFactory::CreateVector4Modification(sRule.m_dwOperationToApply, m_pcShaderViewAdjustment);
	//					else if (sRule.m_dwRegisterCount == 4)
	//						sRule.m_pcModification = ShaderConstantModificationFactory::CreateMatrixModification(sRule.m_dwOperationToApply, m_pcShaderViewAdjustment, sRule.m_bTranspose);
	//					else sRule.m_pcModification = nullptr;
	//
	//					// add the rule
	//					m_asConstantRules.push_back(sRule);
	//
	//					// update the control string list
	//					FillShaderRuleIndices();
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwAddGeneral)
	//				{
	//					// increase the update counter to reverify the constant buffers for rules
	//					m_dwConstantRulesUpdateCounter++;
	//
	//					// get current selection of the rule indices list
	//					INT nSelection = 0;
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageGameShaderRules.m_dwRuleIndices);
	//					if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderRuleIndices.size()))
	//					{
	//						// add to general rules
	//						m_aunGlobalConstantRuleIndices.push_back((UINT)nSelection);
	//					}
	//
	//					// update the list
	//					FillShaderRuleGeneralIndices();
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwDeleteGeneral)
	//				{
	//					// increase the update counter to reverify the constant buffers for rules
	//					m_dwConstantRulesUpdateCounter++;
	//
	//					// get current selection of the general indices constant list
	//					INT nSelection = 0;
	//					nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageGameShaderRules.m_dwGeneralIndices);
	//					if ((nSelection >= 0) && (nSelection < (INT)m_aszShaderRuleGeneralIndices.size()))
	//					{
	//						// erase
	//						m_aunGlobalConstantRuleIndices.erase(m_aunGlobalConstantRuleIndices.begin() + nSelection);
	//					}
	//
	//					// update the list
	//					FillShaderRuleGeneralIndices();
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwDeleteLatest)
	//				{
	//					// increase the update counter to reverify the constant buffers for rules
	//					m_dwConstantRulesUpdateCounter++;
	//
	//					if (m_asConstantRules.size())
	//					{
	//						// get last index of the shader rule list
	//						INT nSelection = (INT)m_asConstantRules.size() - 1;
	//
	//						{
	//							// delete all entries for that index in the general list
	//							auto it = m_aunGlobalConstantRuleIndices.begin();
	//							while (it < m_aunGlobalConstantRuleIndices.end())
	//							{
	//								if ((INT)*it == nSelection)
	//								{
	//									m_aunGlobalConstantRuleIndices.erase(it);
	//								}
	//								it++;
	//							}
	//						}
	//
	//						// erase latest
	//						auto it = m_asConstantRules.end();
	//						it--;
	//						m_asConstantRules.erase(it);
	//
	//						// update the control string lists
	//						FillShaderRuleIndices();
	//						FillShaderRuleGeneralIndices();
	//					}
	//				}
	//				else if (sEvent.dwIndexOfControl == m_sPageGameShaderRules.m_dwImportXML)
	//				{
	//					OPENFILENAME ofn;
	//					wchar_t szFileName[MAX_PATH] = L"";
	//
	//					// get filename using the OPENFILENAME structure and GetOpenFileName()
	//					ZeroMemory(&ofn, sizeof(ofn));
	//
	//					ofn.lStructSize = sizeof(ofn);
	//					ofn.hwndOwner = NULL;
	//					ofn.lpstrFilter = (LPCWSTR)L"Vireio v3 Shader Rule XML file (*.xml)\0*.xml\0";
	//					ofn.lpstrFile = (LPWSTR)szFileName;
	//					ofn.nMaxFile = MAX_PATH;
	//					ofn.lpstrDefExt = (LPCWSTR)L"xml";
	//					ofn.lpstrTitle = L"Import Vireio v3 Shader Rule XML file";
	//					ofn.Flags = OFN_FILEMUSTEXIST;
	//
	//					if (GetOpenFileName(&ofn))
	//					{
	//						// and import
	//						std::wstring szW = std::wstring(ofn.lpstrFile);
	//						std::string szFilePath = std::string(szW.begin(), szW.end());
	//						ImportXMLRules(szFilePath);
	//					}
	//
	//					// fill the string list
	//					FillShaderRuleIndices();
	//					FillShaderRuleGeneralIndices();
	//#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	//					FillFetchedHashCodeList();
	//#elif defined(VIREIO_D3D9)
	//					FillShaderRuleShaderIndices();
	//#endif
	//				}
	//			}
	//			break;
	//#pragma endregion
	//		default:
	//			break;
	//	}
}

#if (defined(VIREIO_D3D11) || defined(VIREIO_D3D10))
/**
* Handles all constant buffer setting methods for all DX11 shaders.
* Called by VSSetConstantBuffers, GSSetConstantBuffers, HSSetConstantBuffers, DSSetConstantBuffers and PSSetConstantBuffers.
***/
void MatrixModifier::XSSetConstantBuffers(ID3D11DeviceContext* pcContext, std::vector<ID3D11Buffer*>& apcActiveConstantBuffers, UINT dwStartSlot, UINT dwNumBuffers, ID3D11Buffer* const* ppcConstantBuffers, Vireio_Supported_Shaders eShaderType)
{
	// loop through the new buffers
	for (UINT dwIndex = 0; dwIndex < dwNumBuffers; dwIndex++)
	{
		// get internal index
		UINT dwInternalIndex = dwIndex + dwStartSlot;

		// in range ? 
		if (dwInternalIndex < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
		{
			// set buffer internally 
			apcActiveConstantBuffers[dwInternalIndex] = ppcConstantBuffers[dwIndex];

			if (apcActiveConstantBuffers[dwInternalIndex])
			{
				// get private rule index from buffer
				Vireio_Buffer_Rules_Index sRulesIndex;
				sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;
				sRulesIndex.m_dwUpdateCounter = 0;
				UINT dwDataSizeRulesIndex = sizeof(Vireio_Buffer_Rules_Index);
				apcActiveConstantBuffers[dwInternalIndex]->GetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, &dwDataSizeRulesIndex, &sRulesIndex);

				// set twin for right side, first get the private data interface
				ID3D11Buffer* pcBuffer = nullptr;
				UINT dwSize = sizeof(pcBuffer);
				apcActiveConstantBuffers[dwInternalIndex]->GetPrivateData(PDIID_ID3D11Buffer_Constant_Buffer_Right, &dwSize, (void*)&pcBuffer);

				// stereo buffer and rules index present ?
				if ((pcBuffer) && (dwDataSizeRulesIndex) && (sRulesIndex.m_nRulesIndex >= 0))
				{
					// set right buffer as active buffer
					apcActiveConstantBuffers[dwInternalIndex + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = pcBuffer;
				}
				else
				{
					// no buffer or no shader rules assigned ? left = right side -> right = left side
					apcActiveConstantBuffers[dwInternalIndex + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = apcActiveConstantBuffers[dwInternalIndex];

					// verify buffer
					if ((pcBuffer) && (sRulesIndex.m_nRulesIndex == VIREIO_CONSTANT_RULES_NOT_ADDRESSED))
						VerifyConstantBuffer(apcActiveConstantBuffers[dwInternalIndex], dwInternalIndex, eShaderType);
				}

				// no stereo buffer present ?
				if (!pcBuffer)
				{
					D3D11_BUFFER_DESC sDesc;
					apcActiveConstantBuffers[dwInternalIndex]->GetDesc(&sDesc);

					// ignore immutable buffers
					if (sDesc.Usage != D3D11_USAGE_IMMUTABLE)
					{
						// create stereo constant buffer, first get device
						ID3D11Device* pcDevice = nullptr;
						apcActiveConstantBuffers[dwInternalIndex]->GetDevice(&pcDevice);
						if (pcDevice)
						{
							CreateStereoBuffer(pcDevice, pcContext, (ID3D11Buffer*)apcActiveConstantBuffers[dwInternalIndex], &sDesc, NULL, true);
							pcDevice->Release();
						}
					}
				}
				else
					pcBuffer->Release();
			}
			else
				apcActiveConstantBuffers[dwInternalIndex + D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = nullptr;
		}
	}
}

/**
* Verifies a stereo constant buffer for shader rules and assigns them in case.
* @param pcBuffer The constant buffer to be verified.
***/
void MatrixModifier::VerifyConstantBuffer(ID3D11Buffer* pcBuffer, UINT dwBufferIndex, Vireio_Supported_Shaders eShaderType)
{
	// buffer already verified ?
	Vireio_Buffer_Rules_Index sRulesIndex;
	sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;
	sRulesIndex.m_dwUpdateCounter = 0;
	UINT dwDataSizeRulesIndex = sizeof(Vireio_Buffer_Rules_Index);
	pcBuffer->GetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, &dwDataSizeRulesIndex, &sRulesIndex);
	if (dwDataSizeRulesIndex)
	{
		// if the update counter has increased set to "NOT ADDRESSED"
		if (sRulesIndex.m_dwUpdateCounter < m_dwConstantRulesUpdateCounter)
			sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_ADDRESSED;

		// continue only if constant rules not addressed
		if (sRulesIndex.m_nRulesIndex != VIREIO_CONSTANT_RULES_NOT_ADDRESSED)
			return;
	}

	// get buffer size by description
	D3D11_BUFFER_DESC sDesc;
	pcBuffer->GetDesc(&sDesc);
	UINT dwBufferSize = sDesc.ByteWidth;

	// get the register size of the buffer
	UINT dwBufferRegisterSize = dwBufferSize >> 5;

	// not addressed ? address
	if (sRulesIndex.m_nRulesIndex == VIREIO_CONSTANT_RULES_NOT_ADDRESSED)
	{
		// create a vector for this constant buffer
		std::vector<Vireio_Constant_Rule_Index> asConstantBufferRules = std::vector<Vireio_Constant_Rule_Index>();

		// loop through all global rules
		for (UINT dwI = 0; dwI < (UINT)m_aunGlobalConstantRuleIndices.size(); dwI++)
		{
			// get a bool for each register index, set to false
			std::vector<bool> abRegistersMatching = std::vector<bool>(dwBufferRegisterSize, false);

			if ((m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUseName) || (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUsePartialNameMatch))
			{
				Vireio_Shader_Private_Data sPrivateData;
				UINT dwDataSize = sizeof(sPrivateData);
				pcBuffer->GetPrivateData(PDID_ID3D11VertexShader_Vireio_Data, &dwDataSize, (void*)&sPrivateData);
				if (dwDataSize)
				{
					// use name ?
					if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUseName)
					{
						switch (eShaderType)
						{
						case VertexShader:
							// shader has this buffer index ?
							if (dwBufferIndex < m_asVShaders[sPrivateData.dwIndex].asBuffers.size())
							{
								// loop through the shader constants
								for (UINT nConstant = 0; nConstant < (UINT)m_asVShaders[sPrivateData.dwIndex].asBuffers[dwBufferIndex].asVariables.size(); nConstant++)
								{
									// test full name matching
									if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_szConstantName.compare(m_asVShaders[sPrivateData.dwIndex].asBuffers[dwBufferIndex].asVariables[nConstant].szName) == 0)
									{
										// set this register to 'false' if not matching
										UINT dwRegister = m_asVShaders[sPrivateData.dwIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset >> 5;
										if (dwRegister <= dwBufferRegisterSize)
											abRegistersMatching[m_asVShaders[sPrivateData.dwIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset >> 5] = true;
									}
								}
							}
							break;
						case PixelShader:
							break;
						case GeometryShader:
							break;
						case HullShader:
							break;
						case DomainShader:
							break;
						default:
							break;
						}
					}

					// use partial name ?
					if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUsePartialNameMatch)
					{
						switch (eShaderType)
						{
						case VertexShader:
							// shader has this buffer index ?
							if (dwBufferIndex < m_asVShaders[sPrivateData.dwIndex].asBuffers.size())
							{
								// loop through the shader constants
								for (UINT nConstant = 0; nConstant < (UINT)m_asVShaders[sPrivateData.dwIndex].asBuffers[dwBufferIndex].asVariables.size(); nConstant++)
								{
									// test partial name matching
									if (std::strstr(m_asVShaders[sPrivateData.dwIndex].asBuffers[dwBufferIndex].asVariables[nConstant].szName, m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_szConstantName.c_str()))
									{
										// set this register to 'false' if not matching
										UINT dwRegister = m_asVShaders[sPrivateData.dwIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset >> 5;
										if (dwRegister <= dwBufferRegisterSize)
											abRegistersMatching[dwRegister] = true;
									}
								}
							}
							break;
						case PixelShader:
							break;
						case GeometryShader:
							break;
						case HullShader:
							break;
						case DomainShader:
							break;
						default:
							break;
						}
					}
				}
			}

			// use start reg index ?
			if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUseStartRegIndex)
			{
				UINT dwRegister = m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_dwStartRegIndex;
				if ((dwBufferRegisterSize) && (dwRegister <= dwBufferRegisterSize))
				{
					bool bOld = abRegistersMatching[dwRegister];
					abRegistersMatching = std::vector<bool>(dwBufferRegisterSize, false);
					abRegistersMatching[dwRegister] = bOld;

					// set to true if no naming convention 
					if ((!m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUseName) && (!m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUsePartialNameMatch))
						abRegistersMatching[dwRegister] = true;
				}
				else
					abRegistersMatching = std::vector<bool>(dwBufferRegisterSize, false);
			}

			// use buffer index
			if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUseBufferIndex)
			{
				switch (eShaderType)
				{
				case VertexShader:
					if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_dwBufferIndex != dwBufferIndex)
						abRegistersMatching = std::vector<bool>(dwBufferRegisterSize, false);
					break;
				case PixelShader:
					break;
				case GeometryShader:
					break;
				case HullShader:
					break;
				case DomainShader:
					break;
				default:
					break;
				}
			}

			// use buffer size
			if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_bUseBufferSize)
			{
				if (m_asConstantRules[m_aunGlobalConstantRuleIndices[dwI]].m_dwBufferSize != dwBufferSize)
					abRegistersMatching = std::vector<bool>(dwBufferRegisterSize, false);
			}

			// loop through registers and create the rules
			for (UINT dwJ = 0; dwJ < dwBufferRegisterSize; dwJ++)
			{
				// register matches the rule ?
				if (abRegistersMatching[dwJ])
				{
					Vireio_Constant_Rule_Index sIndex;
					sIndex.m_dwIndex = m_aunGlobalConstantRuleIndices[dwI];
					sIndex.m_dwConstantRuleRegister = dwJ;
					asConstantBufferRules.push_back(sIndex);
				}
			}
		}

		// look wether these rules are already present, otherwise add
		bool bPresent = false;
		for (UINT dwI = 0; dwI < (UINT)m_aasConstantBufferRuleIndices.size(); dwI++)
		{
			// size equals ?
			if (m_aasConstantBufferRuleIndices[dwI].size() == asConstantBufferRules.size())
			{
				// test all constants
				UINT dwCount = 0;
				for (UINT dwJ = 0; dwJ < (UINT)m_aasConstantBufferRuleIndices[dwI].size(); dwJ++)
				{
					if ((m_aasConstantBufferRuleIndices[dwI][dwJ].m_dwConstantRuleRegister == asConstantBufferRules[dwJ].m_dwConstantRuleRegister) &&
						(m_aasConstantBufferRuleIndices[dwI][dwJ].m_dwIndex == asConstantBufferRules[dwJ].m_dwIndex))
						dwCount++;
				}
				if ((dwCount) && (dwCount == (UINT)asConstantBufferRules.size()))
				{
					// set existing constant rule indices
					sRulesIndex.m_nRulesIndex = (INT)dwI;
					bPresent = true;
				}
			}
		}

		// no rules found ? set to unavailable
		if (!asConstantBufferRules.size())
		{
			sRulesIndex.m_nRulesIndex = VIREIO_CONSTANT_RULES_NOT_AVAILABLE;
		}
		// rules not present ?
		else if (!bPresent)
		{
			// set index... current vector size
			sRulesIndex.m_nRulesIndex = (INT)m_aasConstantBufferRuleIndices.size();

			// and add
			m_aasConstantBufferRuleIndices.push_back(asConstantBufferRules);
		}
	}

	// set the rules index as private data to the constant buffer, first update the update counter
	sRulesIndex.m_dwUpdateCounter = m_dwConstantRulesUpdateCounter;
	pcBuffer->SetPrivateData(PDID_ID3D11Buffer_Vireio_Rules_Data, sizeof(Vireio_Buffer_Rules_Index), &sRulesIndex);
}

/**
* Modifies left and right constant data using specified shader rules.
* @param nRulesIndex The index of the shader rules this constant buffer is modified with.
* @param pdwLeft The buffer data for the left constant buffer. Data must match right buffer data.
* @param pdwRight The buffer data for the right constant buffer. Data must match left buffer data.
* @param dwBufferSize The size of this buffer, in bytes.
***/
void MatrixModifier::DoBufferModification(INT nRulesIndex, UINT_PTR pdwLeft, UINT_PTR pdwRight, UINT dwBufferSize)
{
	// do modifications
	if (nRulesIndex >= 0)
	{
		// loop through rules for that constant buffer
		for (UINT dwI = 0; dwI < (UINT)m_aasConstantBufferRuleIndices[nRulesIndex].size(); dwI++)
		{
			UINT dwIndex = m_aasConstantBufferRuleIndices[nRulesIndex][dwI].m_dwIndex;
			UINT dwRegister = m_aasConstantBufferRuleIndices[nRulesIndex][dwI].m_dwConstantRuleRegister;

			// TODO !! VECTOR MODIFICATION, MATRIX2x4 MODIFICATION

			// is this modification in range ?
			if ((dwBufferSize >= dwRegister * 4 * sizeof(float) + sizeof(D3DMATRIX)))
			{
				// get pointers to the matrix (left+right)
				UINT_PTR pvLeft = (UINT_PTR)pdwLeft + dwRegister * 4 * sizeof(float);
				UINT_PTR pvRight = (UINT_PTR)pdwRight + dwRegister * 4 * sizeof(float);

				// get the matrix
				D3DXMATRIX sMatrix = D3DXMATRIX((CONST FLOAT*)pvLeft);

				// do matrix modification
				if (m_asConstantRules[dwIndex].m_pcModification)
				{
					// matrix to be transposed ?
					if (m_asConstantRules[dwIndex].m_bTranspose)
					{
						D3DXMatrixTranspose(&sMatrix, &sMatrix);
					}

					D3DXMATRIX sMatrixLeft, sMatrixRight;

					// do modification
					((ShaderMatrixModification*)m_asConstantRules[dwIndex].m_pcModification.get())->DoMatrixModification(sMatrix, sMatrixLeft, sMatrixRight);

					// transpose back
					if (m_asConstantRules[dwIndex].m_bTranspose)
					{
						D3DXMatrixTranspose(&sMatrixLeft, &sMatrixLeft);
						D3DXMatrixTranspose(&sMatrixRight, &sMatrixRight);
					}

					memcpy((void*)pvLeft, &sMatrixLeft, sizeof(D3DXMATRIX));
					memcpy((void*)pvRight, &sMatrixRight, sizeof(D3DXMATRIX));
				}
			}
		}
	}
}
#endif

#if defined(VIREIO_D3D9)
#endif

/**
* Handles the debug trace.
***/
void MatrixModifier::DebugOutput(const void* pvSrcData, UINT dwShaderIndex, UINT dwBufferIndex, UINT dwBufferSize)
{
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	// which shader is chosen ?
	std::vector<Vireio_D3D11_Shader>* pasShaders;
	std::vector<std::wstring>* pasShaderHashCodes;
	std::vector<UINT>* padwShaderHashCodes;
	if (m_eChosenShaderType == Vireio_Supported_Shaders::VertexShader)
	{
		pasShaders = &m_asVShaders;
		pasShaderHashCodes = &m_aszVShaderHashCodes;
		padwShaderHashCodes = &m_adwVShaderHashCodes;
	}
	else if (m_eChosenShaderType == Vireio_Supported_Shaders::PixelShader)
	{
		pasShaders = &m_asPShaders;
		pasShaderHashCodes = &m_aszPShaderHashCodes;
		padwShaderHashCodes = &m_adwPShaderHashCodes;
	}

	// get current selection of the shader constant list
	INT nSelection = 0;
	nSelection = m_pcVireioGUI->GetCurrentSelection(m_sPageDebug.m_dwShaderConstants);
	if ((nSelection < 0) || (nSelection >= (INT)m_aszShaderConstantsA.size()))
	{
		m_bGrabDebug = false;
		return;
	}

	// loop through the shader constants
	for (size_t nConstant = 0; nConstant < (*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables.size(); nConstant++)
	{
		// add the debug data by type
		switch (m_eDebugOption)
		{
		case Debug_ConstantFloat4:
			// test the name of the constant
			if (std::strstr((*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].szName, m_aszShaderConstantsA[nSelection].c_str()))
			{
				UINT dwSize = sizeof(float) * 4;

				// is this  in range ?
				if (dwBufferSize >= ((*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset + dwSize))
				{
					// get pointers to the data
					UINT_PTR pv = (UINT_PTR)pvSrcData + (*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset;
					D3DXVECTOR4 sVector4 = D3DXVECTOR4((CONST FLOAT*)pv);

					std::wstringstream strStream;
					strStream << L"X:" << sVector4.x << L"::Y:" << sVector4.y << L"::Z:" << sVector4.z << L"::W:" << sVector4.w;
					m_aszDebugTrace.push_back(strStream.str().c_str());
				}

				m_bGrabDebug = false;
				return;
			}
			break;
		case Debug_ConstantFloat8:
			// test the name of the constant
			if (std::strstr((*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].szName, m_aszShaderConstantsA[nSelection].c_str()))
			{
				UINT dwSize = sizeof(float) * 4 * 2;

				// is this  in range ?
				if (dwBufferSize >= ((*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset + dwSize))
				{
					// get pointers to the data
					UINT_PTR pv = (UINT_PTR)pvSrcData + (*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset;
					D3DXVECTOR4 sVector4 = D3DXVECTOR4((CONST FLOAT*)pv);
					pv = (UINT_PTR)pvSrcData + (*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset + sizeof(D3DVECTOR);
					D3DXVECTOR4 sVector4_1 = D3DXVECTOR4((CONST FLOAT*)pv);

					std::wstringstream strStream;
					strStream << L"11:" << sVector4.x << L"::12:" << sVector4.y << L"::13:" << sVector4.z << L"::14:" << sVector4.w;
					m_aszDebugTrace.push_back(strStream.str().c_str()); strStream = std::wstringstream();
					strStream << L"21:" << sVector4_1.x << L"::22:" << sVector4_1.y << L"::23:" << sVector4_1.z << L"::24:" << sVector4_1.w;
					m_aszDebugTrace.push_back(strStream.str().c_str());
				}

				m_bGrabDebug = false;
				return;
			}
			break;
		case Debug_ConstantFloat16:
			// test the name of the constant
			if (std::strstr((*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].szName, m_aszShaderConstantsA[nSelection].c_str()))
			{
				UINT dwSize = sizeof(float) * 4 * 4;

				// is this  in range ?
				if (dwBufferSize >= ((*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset + dwSize))
				{
					// get pointers to the data
					UINT_PTR pv = (UINT_PTR)pvSrcData + (*pasShaders)[dwShaderIndex].asBuffers[dwBufferIndex].asVariables[nConstant].dwStartOffset;
					D3DXMATRIX sMatrix = D3DXMATRIX((CONST FLOAT*)pv);

					std::wstringstream strStream;
					strStream << L"11:" << sMatrix._11 << L"::12:" << sMatrix._12 << L"::13:" << sMatrix._13 << L"::14:" << sMatrix._14;
					m_aszDebugTrace.push_back(strStream.str().c_str()); strStream = std::wstringstream();
					strStream << L"21:" << sMatrix._21 << L"::22:" << sMatrix._22 << L"::23:" << sMatrix._23 << L"::24:" << sMatrix._24;
					m_aszDebugTrace.push_back(strStream.str().c_str()); strStream = std::wstringstream();
					strStream << L"31:" << sMatrix._31 << L"::32:" << sMatrix._32 << L"::33:" << sMatrix._33 << L"::34:" << sMatrix._34;
					m_aszDebugTrace.push_back(strStream.str().c_str()); strStream = std::wstringstream();
					strStream << L"41:" << sMatrix._41 << L"::42:" << sMatrix._42 << L"::43:" << sMatrix._43 << L"::44:" << sMatrix._44;
					m_aszDebugTrace.push_back(strStream.str().c_str());
				}

				m_bGrabDebug = false;
				return;
			}
			break;
		case Debug_ConstantFloat32:
			break;
		case Debug_ConstantFloat64:
			break;
		default:
			break;
		}
	}
#endif
}

/**
* Creates the Graphical User Interface for this node.
***/
//void MatrixModifier::CreateGUI()
//{
	/*SIZE sSizeOfThis;
	sSizeOfThis.cx = GUI_WIDTH; sSizeOfThis.cy = GUI_HEIGHT;
	m_pcVireioGUI = new Vireio_GUI(sSizeOfThis, L"Cambria", TRUE, GUI_CONTROL_FONTSIZE, RGB(110, 105, 95), RGB(254, 249, 240));

	// first, add all pages
	for (int i = 0; i < (int)GUI_Pages::NumberOfPages; i++)
	{
		UINT dwPage = m_pcVireioGUI->AddPage();
		m_adwPageIDs.push_back(dwPage);
	}

	// control structure
	Vireio_Control sControl;

#pragma region Static Text
	// game configuration
	static std::wstring szGameSeparationText = std::wstring(L"World Scale Factor");
	static std::wstring szConvergence = std::wstring(L"Convergence");
	static std::wstring szAspectMultiplier = std::wstring(L"Aspect Multiplier");
	static std::wstring szYawMultiplier = std::wstring(L"Yaw Multiplier");
	static std::wstring szPitchMultiplier = std::wstring(L"Pitch Multiplier");
	static std::wstring szRollMultiplier = std::wstring(L"Roll Multiplier");
	static std::wstring szPositionMultiplier = std::wstring(L"Position Multiplier");
	static std::wstring szPositionXMultiplier = std::wstring(L"Position X Multiplier");
	static std::wstring szPositionYMultiplier = std::wstring(L"Position Y Multiplier");
	static std::wstring szPositionZMultiplier = std::wstring(L"Position Z Multiplier");
	static std::wstring szPFOV = std::wstring(L"Projection FOV");
	static std::wstring szConvergenceToggle = std::wstring(L"Convergence");
	static std::wstring szPFOVToggle = std::wstring(L"ProjectionFOV");
	std::wstring sz64bit;
	if (m_sGameConfiguration.bIs64bit)
		sz64bit = std::wstring(L"64 Bit");
	else
		sz64bit = std::wstring(L"32 Bit");
	static std::vector<std::wstring> aszEntries64bit; aszEntries64bit.push_back(sz64bit);
	std::wstring szNoRoll = std::wstring(L"0-No Roll");
	std::wstring szRollMtx = std::wstring(L"1-Matrix Roll");
	std::wstring szRollPSh = std::wstring(L"2-Pixel S.Roll");
	static std::vector<std::wstring> aszRollImpl;
	aszRollImpl.push_back(szNoRoll);
	aszRollImpl.push_back(szRollMtx);
	aszRollImpl.push_back(szRollPSh);

	// vertex shader page
	static std::wstring szSort = std::wstring(L"Sort");
	static std::wstring szToName = std::wstring(L"-> Name");
	static std::wstring szToRegister = std::wstring(L"-> Register");
	static std::wstring szToBufferSize = std::wstring(L"-> Buffer Size");
	static std::wstring szToBufferIndex = std::wstring(L"-> Buffer Index");
	static std::wstring szToFetchedHash = std::wstring(L"-> Fetched Hash");
	std::wstring szVertexShader = std::wstring(L"Vertex Shader");
	std::wstring szPixelShader = std::wstring(L"Pixel Shader");
	static std::vector<std::wstring> aszShaderTypes;
	aszShaderTypes.push_back(szVertexShader);
	aszShaderTypes.push_back(szPixelShader);

	// shader rules page
	static std::wstring szConstantName = std::wstring(L"Constant Name");
	static std::wstring szPartialNameMatch = std::wstring(L"Partial Name");
	static std::wstring szStartRegIndex = std::wstring(L"Start Register");
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	static std::wstring szBufferSize = std::wstring(L"Buffer Size");
	static std::wstring szBufferIndex = std::wstring(L"Buffer Index");
#endif
	static std::wstring szTranspose = std::wstring(L"Transpose");
	static std::vector<std::wstring> aszEntriesShaderRulePage;
	aszEntriesShaderRulePage.push_back(szConstantName); aszEntriesShaderRulePage.push_back(std::wstring()); aszEntriesShaderRulePage.push_back(std::wstring());
	aszEntriesShaderRulePage.push_back(szPartialNameMatch); aszEntriesShaderRulePage.push_back(std::wstring()); aszEntriesShaderRulePage.push_back(std::wstring());
	aszEntriesShaderRulePage.push_back(szStartRegIndex); aszEntriesShaderRulePage.push_back(std::wstring()); aszEntriesShaderRulePage.push_back(std::wstring());
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	aszEntriesShaderRulePage.push_back(szBufferSize); aszEntriesShaderRulePage.push_back(std::wstring()); aszEntriesShaderRulePage.push_back(std::wstring());
	aszEntriesShaderRulePage.push_back(szBufferIndex); aszEntriesShaderRulePage.push_back(std::wstring()); aszEntriesShaderRulePage.push_back(std::wstring());
#endif
	static std::wstring szCreate = std::wstring(L"Create");
	static std::wstring szDeleteLast = std::wstring(L"Delete Last");
	static std::wstring szToGeneral = std::wstring(L"To General");
	static std::wstring szDelete = std::wstring(L"Delete");
	static std::wstring szImport = std::wstring(L"Import XML");
	static std::wstring szBufferIndexDebug = std::wstring(L"Debug Index");

	static std::vector<std::wstring> aszOperations;
	for (UINT nMod = 0; nMod < ShaderConstantModificationFactory::m_unMatrixModificationNumber; nMod++)
	{
		aszOperations.push_back(ShaderConstantModificationFactory::GetMatrixModificationName(nMod));
	}

	std::wstring sz1 = std::wstring(L"1 - Vector 4f");
	std::wstring sz2 = std::wstring(L"2 - Matrix 2x4f");
	std::wstring sz4 = std::wstring(L"4 - Matrix 4x4f");
	static std::vector<std::wstring> aszRegCounts;
	aszRegCounts.push_back(sz1);
	aszRegCounts.push_back(sz2);
	aszRegCounts.push_back(sz4);
#pragma endregion

#pragma region Main Page
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	static std::vector<std::wstring> sEntriesCommanders;
	static std::vector<std::wstring> sEntriesDecommanders;

	// create the decommanders list
	sControl.m_eControlType = Vireio_Control_Type::StaticListBox;
	sControl.m_sPosition.x = GUI_CONTROL_FONTBORDER;
	sControl.m_sPosition.y = 0;
	sControl.m_sSize.cx = GUI_WIDTH - GUI_CONTROL_BORDER;
	sControl.m_sSize.cy = NUMBER_OF_DECOMMANDERS * 64;
	sControl.m_sStaticListBox.m_bSelectable = false;
	sControl.m_sStaticListBox.m_paszEntries = &sEntriesDecommanders;
	UINT dwDecommandersList = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::MainPage], sControl);

	// create the commanders list out of the decommanders list
	sControl.m_sPosition.y += NUMBER_OF_DECOMMANDERS * 64;
	sControl.m_sPosition.x = GUI_WIDTH >> 2;
	sControl.m_sSize.cy = NUMBER_OF_COMMANDERS * 64;
	sControl.m_sStaticListBox.m_paszEntries = &sEntriesCommanders;
	UINT dwCommandersList = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::MainPage], sControl);

	// and add all entries
	for (int i = 0; i < NUMBER_OF_DECOMMANDERS; i++)
		m_pcVireioGUI->AddEntry(dwDecommandersList, this->GetDecommanderName(i));
	for (int i = 0; i < NUMBER_OF_COMMANDERS; i++)
		m_pcVireioGUI->AddEntry(dwCommandersList, this->GetCommanderName(i));
#pragma endregion

#pragma region Game setting page
	// game separation float value
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::FloatInput;
	sControl.m_sPosition.x = GUI_CONTROL_BORDER;
	sControl.m_sPosition.y = GUI_CONTROL_BORDER;
	sControl.m_sSize.cx = GUI_CONTROL_FONTSIZE * 12;                     /// Standard for float controls
	sControl.m_sSize.cy = GUI_CONTROL_FONTSIZE * 3;                      /// Standard for float controls
	sControl.m_sFloat.m_fValue = m_sGameConfiguration.fWorldScaleFactor;
	sControl.m_sFloat.m_pszText = &szGameSeparationText;

	// game configuration
	m_sPageGameSettings.m_dwGameSeparation = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::GameSettingsPage], sControl);
	m_sPageGameSettings.m_dwConvergence = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szConvergence, m_sGameConfiguration.fConvergence, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 3);
	m_sPageGameSettings.m_dwAspectMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szAspectMultiplier, m_sGameConfiguration.fAspectMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 6);
	m_sPageGameSettings.m_dwYawMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szYawMultiplier, m_sGameConfiguration.fYawMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 9);
	m_sPageGameSettings.m_dwPitchMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szPitchMultiplier, m_sGameConfiguration.fPitchMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 12);
	m_sPageGameSettings.m_dwRollMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szRollMultiplier, m_sGameConfiguration.fRollMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 15);
	m_sPageGameSettings.m_dwPositionMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szPositionMultiplier, m_sGameConfiguration.fPositionMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 18);
	m_sPageGameSettings.m_dwPositionXMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szPositionXMultiplier, m_sGameConfiguration.fPositionXMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 21);
	m_sPageGameSettings.m_dwPositionYMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szPositionYMultiplier, m_sGameConfiguration.fPositionYMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 24);
	m_sPageGameSettings.m_dwPositionZMultiplier = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szPositionZMultiplier, m_sGameConfiguration.fPositionZMultiplier, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 27);
	m_sPageGameSettings.m_dwPFOV = CreateFloatControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szPFOV, m_sGameConfiguration.fPFOV, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 30);
	m_sPageGameSettings.m_dwConvergenceEnabled = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szConvergenceToggle, m_sGameConfiguration.bConvergenceEnabled, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 33, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameSettings.m_dwPFOVToggle = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &szPFOVToggle, m_sGameConfiguration.bPFOVToggle, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER + (GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER) * 33 + GUI_CONTROL_LINE, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameSettings.m_dwRollImpl = CreateSpinControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &aszRollImpl, m_sGameConfiguration.nRollImpl, GUI_CONTROL_BORDER, GUI_HEIGHT - GUI_CONTROL_FONTSIZE * 24, GUI_CONTROL_BUTTONSIZE);

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	// technical options
#endif

	m_sPageGameSettings.m_dwIs64bit = CreateStaticListControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::GameSettingsPage], &aszEntries64bit, GUI_CONTROL_BORDER, GUI_HEIGHT - GUI_CONTROL_BORDER - GUI_CONTROL_FONTSIZE - GUI_CONTROL_FONTSIZE * 4, GUI_CONTROL_BUTTONSIZE);

#pragma endregion

#pragma region Shader Page
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	// shader constant buffer size list (lists always first from bottom to top!)
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::ListBox;
	sControl.m_sPosition.x = GUI_CONTROL_FONTBORDER;
	sControl.m_sPosition.y = (GUI_HEIGHT >> 2) + (GUI_HEIGHT >> 1) + (GUI_CONTROL_BORDER >> 1);
	sControl.m_sSize.cx = GUI_WIDTH - GUI_CONTROL_BORDER;
	sControl.m_sSize.cy = ((GUI_HEIGHT - GUI_CONTROL_FONTSIZE * 4) >> 3) - GUI_CONTROL_LINE;
	sControl.m_sListBox.m_paszEntries = &m_aszShaderBuffersizes;
	sControl.m_sListBox.m_bSelectable = true;
	m_sPageShader.m_dwCurrentBuffersizes = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::ShadersPage], sControl);
#endif
	// shader constant list
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::ListBox;
	sControl.m_sPosition.x = GUI_CONTROL_FONTBORDER;
	sControl.m_sPosition.y = (GUI_HEIGHT >> 1) + (GUI_CONTROL_BORDER >> 1);
	sControl.m_sSize.cx = GUI_WIDTH - GUI_CONTROL_BORDER;
	sControl.m_sSize.cy = ((GUI_HEIGHT - GUI_CONTROL_FONTSIZE * 4) >> 2) - GUI_CONTROL_LINE;
	sControl.m_sListBox.m_paszEntries = &m_aszShaderConstantsCurrent;
	sControl.m_sListBox.m_bSelectable = true;
	m_sPageShader.m_dwCurrentConstants = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::ShadersPage], sControl);

	// create the shader list
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::ListBox;
	sControl.m_sPosition.x = GUI_CONTROL_FONTBORDER;
	sControl.m_sPosition.y = GUI_CONTROL_BORDER >> 1;
	sControl.m_sSize.cx = GUI_WIDTH - GUI_CONTROL_BORDER;
	sControl.m_sSize.cy = (GUI_HEIGHT >> 1) - GUI_CONTROL_BORDER - GUI_CONTROL_LINE;
	sControl.m_sListBox.m_bSelectable = true;
	sControl.m_sListBox.m_nCurrentSelection = -1;                                                   /// Set to -1 means 'No Selection' at startup
	sControl.m_sListBox.m_paszEntries = &m_aszVShaderHashCodes;                                     /// At start we set the vertex shader hash code list here.
	m_sPageShader.m_dwHashCodes = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::ShadersPage], sControl);

	// "update shader data" button
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::Button;
	sControl.m_sPosition.x = GUI_CONTROL_FONTBORDER;
	sControl.m_sPosition.y = (GUI_HEIGHT >> 1) - GUI_CONTROL_LINE;
	sControl.m_sSize.cx = GUI_CONTROL_BUTTONSIZE;
	sControl.m_sSize.cy = GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER;
	static std::wstring szButtonUpdateShaderDataText = std::wstring(L"Update Data");
	sControl.m_sButton.m_pszText = &szButtonUpdateShaderDataText;
	m_sPageShader.m_dwUpdate = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::ShadersPage], sControl);
	m_sPageShader.m_dwSort = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShadersPage], &szSort, m_bSortShaderList, GUI_CONTROL_FONTBORDER + GUI_CONTROL_BUTTONSIZE, (GUI_HEIGHT >> 1) - GUI_CONTROL_LINE, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);

	m_sPageShader.m_dwToName = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShadersPage], &szToName, GUI_CONTROL_FONTBORDER, (GUI_HEIGHT >> 2) + (GUI_HEIGHT >> 1) - GUI_CONTROL_BORDER - GUI_CONTROL_FONTBORDER, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageShader.m_dwToRegister = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShadersPage], &szToRegister, GUI_CONTROL_FONTBORDER + GUI_CONTROL_BUTTONSIZE, (GUI_HEIGHT >> 2) + (GUI_HEIGHT >> 1) - GUI_CONTROL_BORDER - GUI_CONTROL_FONTBORDER, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageShader.m_dwShaderType = CreateSpinControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShadersPage], &aszShaderTypes, 0, GUI_CONTROL_FONTBORDER, GUI_HEIGHT - GUI_CONTROL_FONTSIZE * 7, GUI_CONTROL_BUTTONSIZE);
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	m_sPageShader.m_dwToBufferSize = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShadersPage], &szToBufferSize, GUI_CONTROL_FONTBORDER, GUI_HEIGHT - GUI_CONTROL_FONTSIZE * 9, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageShader.m_dwToBufferIndex = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShadersPage], &szToBufferIndex, GUI_CONTROL_FONTBORDER + GUI_CONTROL_BUTTONSIZE, GUI_HEIGHT - GUI_CONTROL_FONTSIZE * 9, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);

	m_sPageShader.m_dwToFetchedList = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShadersPage], &szToFetchedHash, GUI_CONTROL_FONTBORDER + GUI_CONTROL_BUTTONSIZE, GUI_HEIGHT - GUI_CONTROL_FONTSIZE * 7, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
#endif
#pragma endregion

#pragma region Shader Rule Page
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	m_sPageGameShaderRules.m_dwFetchedShaderHashcodes = CreateListControlSelectable(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_aszFetchedHashCodes, GUI_CONTROL_FONTBORDER,
		(GUI_HEIGHT >> 1) + (GUI_CONTROL_BORDER << 2) + (GUI_HEIGHT >> 3) + (GUI_CONTROL_FONTSIZE << 3),
		GUI_WIDTH - GUI_CONTROL_BORDER, GUI_HEIGHT >> 3);
#elif defined(VIREIO_D3D9)
	m_sPageGameShaderRules.m_dwShaderIndices = CreateListControlSelectable(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_aszShaderRuleShaderIndices, GUI_CONTROL_FONTBORDER,
		(GUI_HEIGHT >> 2) + (GUI_CONTROL_BORDER << 2) + ((GUI_HEIGHT >> 3) + (GUI_CONTROL_FONTSIZE << 1)) * 3,
		GUI_WIDTH - GUI_CONTROL_BORDER, GUI_HEIGHT >> 3);
#endif
	m_sPageGameShaderRules.m_dwGeneralIndices = CreateListControlSelectable(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_aszShaderRuleGeneralIndices, GUI_CONTROL_FONTBORDER,
		(GUI_HEIGHT >> 2) + (GUI_CONTROL_BORDER << 2) + (GUI_HEIGHT >> 2) + (GUI_CONTROL_FONTSIZE << 2),
		GUI_WIDTH - GUI_CONTROL_BORDER, GUI_HEIGHT >> 3);
	m_sPageGameShaderRules.m_dwRuleData = CreateListControlSelectable(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_aszShaderRuleData, GUI_CONTROL_FONTBORDER,
		(GUI_HEIGHT >> 2) + (GUI_CONTROL_BORDER << 2) + (GUI_HEIGHT >> 3) + (GUI_CONTROL_FONTSIZE << 1),
		GUI_WIDTH - GUI_CONTROL_BORDER, GUI_HEIGHT >> 3);
	m_sPageGameShaderRules.m_dwRuleIndices = CreateListControlSelectable(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_aszShaderRuleIndices, GUI_CONTROL_FONTBORDER,
		(GUI_HEIGHT >> 2) + (GUI_CONTROL_BORDER << 2),
		GUI_WIDTH - GUI_CONTROL_BORDER, GUI_HEIGHT >> 3);

	m_sPageGameShaderRules.m_dwTextlist = CreateStaticListControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &aszEntriesShaderRulePage, GUI_CONTROL_BORDER, GUI_CONTROL_BORDER >> 1, GUI_CONTROL_BUTTONSIZE);

	m_sPageGameShaderRules.m_dwConstantName = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_sPageGameShaderRules.m_szConstantName, false, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE, (GUI_CONTROL_BUTTONSIZE << 1) - (GUI_CONTROL_BUTTONSIZE >> 1), GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameShaderRules.m_dwPartialName = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_sPageGameShaderRules.m_szPartialName, false, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 4, (GUI_CONTROL_BUTTONSIZE << 1) - (GUI_CONTROL_BUTTONSIZE >> 1), GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameShaderRules.m_dwStartRegIndex = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_sPageGameShaderRules.m_szStartRegIndex, false, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 7, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	m_sPageGameShaderRules.m_dwBufferSize = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_sPageGameShaderRules.m_szBufferSize, false, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 10, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameShaderRules.m_dwBufferIndex = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &m_sPageGameShaderRules.m_szBufferIndex, false, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 13, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
#endif
	m_sPageGameShaderRules.m_dwTranspose = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &szTranspose, false, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 16, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameShaderRules.m_dwOperationToApply = CreateSpinControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &aszOperations, 0, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 18, (GUI_CONTROL_BUTTONSIZE << 1) - (GUI_CONTROL_BUTTONSIZE >> 1));
	m_sPageGameShaderRules.m_dwRegisterCount = CreateSpinControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &aszRegCounts, 2, GUI_CONTROL_BORDER, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 20, GUI_CONTROL_BUTTONSIZE);

	m_sPageGameShaderRules.m_dwCreate = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &szCreate, GUI_CONTROL_BORDER + GUI_CONTROL_BUTTONSIZE, (GUI_CONTROL_BORDER >> 1) + GUI_CONTROL_FONTSIZE * 20, (GUI_CONTROL_BUTTONSIZE >> 1), GUI_CONTROL_FONTSIZE << 1);
	m_sPageGameShaderRules.m_dwDeleteLatest = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &szDeleteLast, GUI_CONTROL_FONTBORDER, (GUI_HEIGHT >> 2) + (GUI_CONTROL_BORDER << 2) + (GUI_HEIGHT >> 3) + GUI_CONTROL_FONTBORDER, GUI_CONTROL_BUTTONSIZE - GUI_CONTROL_FONTBORDER, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameShaderRules.m_dwAddGeneral = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &szToGeneral, GUI_CONTROL_FONTBORDER + GUI_CONTROL_BUTTONSIZE, (GUI_HEIGHT >> 2) + (GUI_CONTROL_BORDER << 2) + (GUI_HEIGHT >> 3) + GUI_CONTROL_FONTBORDER, GUI_CONTROL_BUTTONSIZE - GUI_CONTROL_FONTBORDER, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameShaderRules.m_dwDeleteGeneral = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &szDelete, GUI_CONTROL_FONTBORDER, (GUI_HEIGHT >> 2) + (GUI_CONTROL_FONTSIZE << 1) + GUI_CONTROL_FONTBORDER + ((GUI_HEIGHT >> 3) + (GUI_CONTROL_FONTSIZE << 1)) * 3, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
	m_sPageGameShaderRules.m_dwImportXML = CreateButtonControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &szImport, GUI_CONTROL_FONTBORDER + GUI_CONTROL_BUTTONSIZE, (GUI_HEIGHT >> 2) + (GUI_CONTROL_FONTSIZE << 1) + GUI_CONTROL_FONTBORDER + ((GUI_HEIGHT >> 3) + (GUI_CONTROL_FONTSIZE << 1)) * 3, GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	m_sPageGameShaderRules.m_dwBufferIndexDebug = CreateSwitchControl(m_pcVireioGUI, m_adwPageIDs[GUI_Pages::ShaderRulesPage], &szBufferIndexDebug, m_bBufferIndexDebug, GUI_CONTROL_FONTBORDER + GUI_CONTROL_BUTTONSIZE, (GUI_HEIGHT >> 2) + (GUI_CONTROL_FONTSIZE << 1) + GUI_CONTROL_FONTBORDER + ((GUI_HEIGHT >> 3) + (GUI_CONTROL_FONTSIZE << 1)) * 3 + (GUI_CONTROL_FONTSIZE << 1), GUI_CONTROL_BUTTONSIZE, GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER);
#endif
#pragma endregion

#pragma region Description Page
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	static std::vector<std::wstring> sEntriesDescription;

	// create the description list
	sControl.m_eControlType = Vireio_Control_Type::StaticListBox;
	sControl.m_sPosition.x = GUI_CONTROL_FONTBORDER;
	sControl.m_sPosition.y = 0;
	sControl.m_sSize.cx = GUI_WIDTH - GUI_CONTROL_BORDER;
	sControl.m_sSize.cy = (NUMBER_OF_DECOMMANDERS + NUMBER_OF_COMMANDERS) * 64;
	sControl.m_sStaticListBox.m_bSelectable = false;
	sControl.m_sStaticListBox.m_paszEntries = &sEntriesDescription;
	UINT dwDescriptionList = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::DescriptionPage], sControl);

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	// and add all entries
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10Device::CreateVertexShader");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10Device::CreatePixelShader");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::VSSetShader");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::PSSetShader");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10Device::CreateBuffer");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::VSSetConstantBuffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::PSSetConstantBuffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::UpdateSubresource");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::CopyResource");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::CopySubresourceRegion");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::VSGetConstantBuffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D10/ID3D11::PSGetConstantBuffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D11DeviceContext::Map");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L".");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"ID3D11DeviceContext::Unmap");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"--------------------------------------------");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"Splitter switches Left/Right per draw call");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"D3D10 VS constant buffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"D3D11 VS constant buffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"D3D10 PS constant buffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"D3D11 PS constant buffers");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"Splitter verifies constant buffers if needed");
	m_pcVireioGUI->AddEntry(dwDescriptionList, L"D3D11 Vireio Constructor Data");
#elif defined(VIREIO_D3D9)
#endif
#pragma endregion

#pragma region Debug Page
	// add debug options, first the debug trace (bottom list ALWAYS first for each page)
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::ListBox;
	sControl.m_sPosition.x = GUI_CONTROL_BORDER;
	sControl.m_sPosition.y = GUI_CONTROL_BORDER + GUI_CONTROL_LINE * 4 + (GUI_HEIGHT >> 1);
	sControl.m_sSize.cx = GUI_WIDTH - GUI_CONTROL_BORDER - GUI_CONTROL_LINE;
	sControl.m_sSize.cy = GUI_HEIGHT - sControl.m_sPosition.y - GUI_CONTROL_FONTSIZE * 4; // (GUI_CONTROL_FONTSIZE * 4) = size of the bottom arrows of the page
	sControl.m_sListBox.m_paszEntries = &m_aszDebugTrace;
	sControl.m_sListBox.m_bSelectable = false;
	m_sPageDebug.m_dwTrace = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::DebugPage], sControl);

	// shader constant debug list (lists always first from bottom to top!)
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::ListBox;
	sControl.m_sPosition.x = GUI_CONTROL_BORDER;
	sControl.m_sPosition.y = GUI_CONTROL_BORDER + GUI_CONTROL_LINE * 3;
	sControl.m_sSize.cx = GUI_WIDTH - GUI_CONTROL_BORDER - GUI_CONTROL_LINE;
	sControl.m_sSize.cy = GUI_HEIGHT >> 1;
	sControl.m_sListBox.m_paszEntries = &m_aszShaderConstants;
	sControl.m_sListBox.m_bSelectable = true;
	m_sPageDebug.m_dwShaderConstants = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::DebugPage], sControl);

	// debug type spin control
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	m_aszDebugOptions.push_back(L"Constant Float 4");
	m_aszDebugOptions.push_back(L"Constant Float 8");
	m_aszDebugOptions.push_back(L"Constant Float 16");
	m_aszDebugOptions.push_back(L"Constant Float 32");
	m_aszDebugOptions.push_back(L"Constant Float 64");
	sControl.m_eControlType = Vireio_Control_Type::SpinControl;
	sControl.m_sPosition.x = GUI_CONTROL_BORDER;
	sControl.m_sPosition.y = GUI_CONTROL_BORDER;
	sControl.m_sSize.cx = GUI_CONTROL_BUTTONSIZE;
	sControl.m_sSize.cy = GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER;
	sControl.m_sSpinControl.m_dwCurrentSelection = (DWORD)m_eDebugOption;
	sControl.m_sSpinControl.m_paszEntries = &m_aszDebugOptions;
	m_sPageDebug.m_dwOptions = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::DebugPage], sControl);

	// debug "grab" button
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::Button;
	sControl.m_sPosition.x = GUI_CONTROL_BORDER;
	sControl.m_sPosition.y = GUI_CONTROL_BORDER + GUI_CONTROL_LINE;
	sControl.m_sSize.cx = GUI_CONTROL_BUTTONSIZE;
	sControl.m_sSize.cy = GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER;
	static std::wstring szButtonText = std::wstring(L"Grab Debug Data");
	sControl.m_sButton.m_pszText = &szButtonText;
	m_sPageDebug.m_dwGrab = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::DebugPage], sControl);

	// debug "clear" button
	ZeroMemory(&sControl, sizeof(Vireio_Control));
	sControl.m_eControlType = Vireio_Control_Type::Button;
	sControl.m_sPosition.x = GUI_CONTROL_BORDER;
	sControl.m_sPosition.y = GUI_CONTROL_BORDER + GUI_CONTROL_LINE * 2;
	sControl.m_sSize.cx = GUI_CONTROL_BUTTONSIZE;
	sControl.m_sSize.cy = GUI_CONTROL_FONTSIZE + GUI_CONTROL_FONTBORDER;
	static std::wstring szButtonClear = std::wstring(L"Clear Debug Trace");
	sControl.m_sButton.m_pszText = &szButtonClear;
	m_sPageDebug.m_dwClear = m_pcVireioGUI->AddControl(m_adwPageIDs[GUI_Pages::DebugPage], sControl);
#pragma endregion*/
//}

/**
*
***/
void MatrixModifier::FillShaderRuleIndices()
{
	m_aszShaderRuleIndices = std::vector<std::wstring>();
	for (UINT dwI = 0; dwI < (UINT)m_asConstantRules.size(); dwI++)
	{
		// we use just the index as identifier
		std::wstringstream szIndex; szIndex << L"Rule : " << dwI;
		m_aszShaderRuleIndices.push_back(szIndex.str());
	}
}

/**
*
***/
void MatrixModifier::FillShaderRuleData(UINT dwRuleIndex)
{
	if (dwRuleIndex >= (UINT)m_asConstantRules.size())
		return;

	m_aszShaderRuleData = std::vector<std::wstring>();

	// add the data strings
	std::wstringstream szIndex;
	szIndex << L"Rule : " << dwRuleIndex;
	m_aszShaderRuleData.push_back(szIndex.str());

	// name ?
	if (m_asConstantRules[dwRuleIndex].m_bUseName)
	{
		std::wstringstream szName;
		szName << L"Constant Name : " << m_asConstantRules[dwRuleIndex].m_szConstantName.c_str();
		m_aszShaderRuleData.push_back(szName.str());
	}

	// partial name ?
	if (m_asConstantRules[dwRuleIndex].m_bUsePartialNameMatch)
	{
		std::wstringstream szName;
		szName << L"Partial Name : " << m_asConstantRules[dwRuleIndex].m_szConstantName.c_str();
		m_aszShaderRuleData.push_back(szName.str());
	}

	// start reg index ?
	if (m_asConstantRules[dwRuleIndex].m_bUseStartRegIndex)
	{
		std::wstringstream szRegIndex;
		szRegIndex << L"Register : " << m_asConstantRules[dwRuleIndex].m_dwStartRegIndex;
		m_aszShaderRuleData.push_back(szRegIndex.str());
	}

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
	// buffer index ?
	if (m_asConstantRules[dwRuleIndex].m_bUseBufferIndex)
	{
		std::wstringstream szBufferIndex;
		szBufferIndex << L"Buffer Index : " << m_asConstantRules[dwRuleIndex].m_dwBufferIndex;
		m_aszShaderRuleData.push_back(szBufferIndex.str());
	}

	// buffer size ?
	if (m_asConstantRules[dwRuleIndex].m_bUseBufferSize)
	{
		std::wstringstream szBufferSize;
		szBufferSize << L"Buffer Size : " << m_asConstantRules[dwRuleIndex].m_dwBufferSize;
		m_aszShaderRuleData.push_back(szBufferSize.str());
	}
#endif

	// reg count ?
	{
		std::wstringstream szRegCount;
		szRegCount << L"Reg Count : " << m_asConstantRules[dwRuleIndex].m_dwRegisterCount;

		if (m_asConstantRules[dwRuleIndex].m_dwRegisterCount == 1)
			szRegCount << " (Vector4f)";
		else if (m_asConstantRules[dwRuleIndex].m_dwRegisterCount == 4)
			szRegCount << " (Matrix4x4f)";

		m_aszShaderRuleData.push_back(szRegCount.str());
	}

	// transpose ?
	{
		std::wstringstream szTranspose;
		if (m_asConstantRules[dwRuleIndex].m_bTranspose)
			szTranspose << L"Transpose : TRUE";
		else
			szTranspose << L"Transpose : FALSE";
		m_aszShaderRuleData.push_back(szTranspose.str());
	}

	// operation ? TODO !!
	/*{
		if (m_asConstantRules[dwRuleIndex].m_dwRegisterCount == 1)
			m_aszShaderRuleData.push_back(ShaderConstantModificationFactory::GetVector4ModificationName(m_asConstantRules[dwRuleIndex].m_dwOperationToApply));
		else if (m_asConstantRules[dwRuleIndex].m_dwRegisterCount == 4)
			m_aszShaderRuleData.push_back(ShaderConstantModificationFactory::GetMatrixModificationName(m_asConstantRules[dwRuleIndex].m_dwOperationToApply));
	}*/
}

/**
*
***/
void MatrixModifier::FillShaderRuleGeneralIndices()
{
	m_aszShaderRuleGeneralIndices = std::vector<std::wstring>();
	for (UINT dwI = 0; dwI < (UINT)m_aunGlobalConstantRuleIndices.size(); dwI++)
	{
		// add text
		std::wstringstream szIndex; szIndex << L"Rule : " << m_aunGlobalConstantRuleIndices[dwI];
		m_aszShaderRuleGeneralIndices.push_back(szIndex.str());
	}
}

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
/**
*
***/
void MatrixModifier::FillFetchedHashCodeList()
{
	// first, unselect and clear
	if (m_pcVireioGUI)
		m_pcVireioGUI->UnselectCurrentSelection(m_sPageGameShaderRules.m_dwFetchedShaderHashcodes);
	m_aszFetchedHashCodes = std::vector<std::wstring>();
	for (UINT dwI = 0; dwI < (UINT)m_aunFetchedHashCodes.size(); dwI++)
	{
		// add text
		std::wstringstream szIndex; szIndex << L"Hash : " << m_aunFetchedHashCodes[dwI];
		m_aszFetchedHashCodes.push_back(szIndex.str());
	}
}

#elif defined(VIREIO_D3D9)
/**
*
***/
void MatrixModifier::FillShaderRuleShaderIndices()
{
	m_aszShaderRuleShaderIndices = std::vector<std::wstring>();
	for (UINT dwI = 0; dwI < (UINT)m_asShaderSpecificRuleIndices.size(); dwI++)
	{
		// add text
		std::wstringstream szIndex; szIndex << L"Hash : " << m_asShaderSpecificRuleIndices[dwI].unHash << " Rule Ix: " << m_asShaderSpecificRuleIndices[dwI].unRuleIndex;
		m_aszShaderRuleShaderIndices.push_back(szIndex.str());
	}
}
#endif

/**
* Imports (v3) shader modification rules.
* True if load succeeds, false otherwise.
* (pugi::xml_document)
* @param szRulesPath Rules path as defined in game configuration.
***/
bool MatrixModifier::ImportXMLRules(std::string szRulesPath)
{
	// helper to convert IDs to indices (Vireio v4 does use indices instead of IDs)
	/*std::vector<UINT> aIDIndices = std::vector<UINT>();

	m_asConstantRules.clear();
	m_aunGlobalConstantRuleIndices.clear();
#if defined(VIREIO_D3D9)
	m_asShaderSpecificRuleIndices.clear();
#endif

	pugi::xml_document cRulesFile;
	pugi::xml_parse_result sResultProfiles = cRulesFile.load_file(szRulesPath.c_str());

	if (sResultProfiles.status != pugi::status_ok)
	{
		OutputDebugString(L"[MAM] Parsing of shader rules file failed. No rules loaded.\n");
		OutputDebugStringA(szRulesPath.c_str());
		return false;
	}

	// load from file
	pugi::xml_node cXmlShaderConfig = cRulesFile.child("shaderConfig");
	if (!cXmlShaderConfig)
	{
		OutputDebugString(L"[MAM] 'shaderConfig' node missing, malformed shader rules doc.\n");
		return false;
	}

	pugi::xml_node cXmlRules = cXmlShaderConfig.child("rules");
	if (!cXmlRules)
	{
		OutputDebugString(L"[MAM] No 'rules' node found, malformed shader rules doc.\n");
		return false;
	}
	else
	{
		for (pugi::xml_node cRule = cXmlRules.child("rule"); cRule; cRule = cRule.next_sibling("rule"))
		{
			// NOTE : no pattern search supported in v4 !!
			static Vireio_Constant_Modification_Rule sNewRule;

			// get constant name
			sNewRule.m_szConstantName = cRule.attribute("constantName").as_string();
			sNewRule.m_bUseName = (bool)(sNewRule.m_szConstantName.length() > 0);

			// parse type
			std::string szType = cRule.attribute("constantType").as_string();
			if (szType.compare("MatrixC") == 0)
			{
				sNewRule.m_dwRegisterCount = 4;
			}
			else if (szType.compare("MatrixR") == 0)
			{
				sNewRule.m_dwRegisterCount = 4;
			}
			else if (szType.compare("Vector") == 0)
			{
				sNewRule.m_dwRegisterCount = 1;
			}
			else
			{
				OutputDebugString(L"Unknown or unsupported constant type: ");
				OutputDebugStringA(szType.c_str());
				OutputDebugString(L"\n");

				sNewRule.m_dwRegisterCount = 1;
			}

			// get the ID as index, to convert later
			UINT unID = cRule.attribute("id").as_uint();
			aIDIndices.push_back(unID);

			sNewRule.m_dwOperationToApply = cRule.attribute("modToApply").as_uint();
			sNewRule.m_dwStartRegIndex = cRule.attribute("startReg").as_uint(UINT_MAX);
			sNewRule.m_bUsePartialNameMatch = cRule.attribute("partialName").as_bool(false);
			sNewRule.m_bTranspose = cRule.attribute("transpose").as_bool(false);

			// and add to vector
			m_asConstantRules.push_back(sNewRule);
		}

		// default rules (these are optional but will most likely exist for 99% of profiles)
		pugi::xml_node cDefaultRules = cXmlShaderConfig.child("defaultRuleIDs");
		if (cDefaultRules)
		{
			for (pugi::xml_node cRuleId = cDefaultRules.child("ruleID"); cRuleId; cRuleId = cRuleId.next_sibling("ruleID"))
			{
				// get ID
				UINT unID = cRuleId.attribute("id").as_uint();

				// get index
				UINT unIndex = 0;
				bool bFoundID = false;
				for (UINT unI = 0; unI < (UINT)aIDIndices.size(); unI++)
				{
					if (unID == aIDIndices[unI])
					{
						unIndex = unI;
						bFoundID = true;
					}
				}

				// add to indices if ID is present
				if (bFoundID)
				{
					m_aunGlobalConstantRuleIndices.push_back(unIndex);
				}
			}
		}
		else
		{
			OutputDebugString(L"[MAM] No default rules found, did you do this intentionally?\n");
		}

#if defined(VIREIO_D3D9)
		// Shader specific rules (optional)
		for (pugi::xml_node cShaderSpecificIDs = cXmlShaderConfig.child("shaderSpecificRuleIDs"); cShaderSpecificIDs; cShaderSpecificIDs = cShaderSpecificIDs.next_sibling("shaderSpecificRuleIDs"))
		{
			UINT unHash = cShaderSpecificIDs.attribute("shaderHash").as_uint(0);

			if (unHash == 0)
			{
				OutputDebugString(L"[MAM] Shader specific rule with invalid/no hash. Skipping rule.\n");
				continue;
			}

			// TODO !! VIEWPORT SQUISH ?? OBJECT TYPE ??
			std::vector<UINT> aunShaderRuleIDs;

			for (pugi::xml_node cRuleId = cShaderSpecificIDs.child("ruleID"); cRuleId; cRuleId = cRuleId.next_sibling("ruleID"))
			{
				aunShaderRuleIDs.push_back(cRuleId.attribute("id").as_uint());
			}

			if (aunShaderRuleIDs.size())
			{
				for (UINT unI = 0; unI < (UINT)aunShaderRuleIDs.size(); unI++)
				{
					// get ID
					UINT unID = aunShaderRuleIDs[unI];

					// get index
					UINT unIndex = 0;
					bool bFoundID = false;
					for (UINT unJ = 0; unJ < (UINT)aIDIndices.size(); unJ++)
					{
						if (unID == aIDIndices[unJ])
						{
							unIndex = unJ;
							bFoundID = true;
						}
					}

					// add to indices if ID is present
					if (bFoundID)
					{
						static Vireio_Hash_Rule_Index sIndex;
						sIndex.unHash = unHash;
						sIndex.unRuleIndex = unIndex;

						m_asShaderSpecificRuleIndices.push_back(sIndex);
					}

				}
			}
		}
#endif
	}

	return true;*/
}
