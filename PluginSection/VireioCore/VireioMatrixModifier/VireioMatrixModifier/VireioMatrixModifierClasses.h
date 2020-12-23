/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

Vireio Matrix Modifier - Vireio Stereo Matrix Modification Node
Copyright (C) 2015 Denis Reischl

File <VireioMatrixModifierClasses.h> and
Class <VireioMatrixModifierClasses> :
Copyright (C) 2020 Denis Reischl

Parts of included classes directly derive from Vireio source code originally
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

#include<stdio.h>
#include<vector>
#include<array>
#include<string>
#include"..\..\..\Include\Vireio_GameConfig.h"

#include<d3d11_1.h>
#include<d3d11.h>
#include<d3d10_1.h>
#include<d3d10.h>
#include<d3d9.h>
#include<d3dx9.h>

// constants
constexpr float fLEFT_CONSTANT = -1.f;
constexpr float fRIGHT_CONSTANT = 1.f;

#define IPD_DEFAULT 0.064f /**< 64mm in meters */

#define DEFAULT_CONVERGENCE 3.0f
#define DEFAULT_PFOV 110.0f
#define DEFAULT_ASPECT_MULTIPLIER 1.0f

#define DEFAULT_YAW_MULTIPLIER 25.0f
#define DEFAULT_PITCH_MULTIPLIER 25.0f
#define DEFAULT_ROLL_MULTIPLIER 1.0f

#define DEFAULT_POS_TRACKING_X_MULT 2.0f
#define DEFAULT_POS_TRACKING_Y_MULT 2.5f
#define DEFAULT_POS_TRACKING_Z_MULT 0.5f

#pragma region => Modification calculation class
/// <summary>
/// All float fields enumeration used
/// to compute any mathematical operation.
/// </summary>
enum class MathFloatFields : size_t
{
	Roll,                     /*< Amount of actual roll (in radians) */
	IPD,                      /*< Interpupillary distance. */
	Convergence,              /*< Convergence. Left/Rigth offset adjustment. In millimeters. */
	Squash,                   /*< The amount of squashing GUI shader constants. */
	GUI_Depth,                /*< The 3d depth of the GUI. */
	HUD_Distance,             /*< The distance of the HUD. */
	HUD_Depth,                /*< The 3d depth of the HUD. */
	AspectMultiplier,         /*< Aspect multiplier for projection matrices */
	FOV_H,                    /*< FOV, horizontal */
	FOV_V,                    /*< FOV, vertical*/
	Separation_World,         /*< Separation, in World Units */
	Separation_IPDAdjustment, /*< Separation IPD adjustment being used for GUI and HUD matrices */
	Frustum_Asymmetry,        /*< Frustum Asymmetry, in meters. For convergence computation */
	Physical_Screensize,      /*< Physical (Monitor) Screen Size, in meters. For convergence computation */
	LensXCenterOffset,        /*< HMD lens center horizontal offset */

	Math_FloatFields_Size

};
constexpr size_t Math_FloatFields_Size = (size_t)MathFloatFields::Math_FloatFields_Size;

/// <summary>
/// All mathematical registers enumeration.
/// Indices to all mathematical registers (vectors, 
/// matrices), both input and output (meanwhile
/// stored).
/// 1 Register  =  4 * float = Vector4
/// 4 Registers = 16 * float = Matix4x4
/// </summary>
enum class MathRegisters : size_t
{
	VEC_PositionTransform = 0,

	MAT_BasicProjection = VEC_PositionTransform + 4,                       /*< Projection matrix - basic with no PFOV */
	MAT_ProjectionInv = MAT_BasicProjection + 4,                           /*< Projection inverse matrix */
	MAT_ProjectionFOV = MAT_ProjectionInv + 4,                             /*< The projection with adjusted FOV */
	MAT_ProjectionConvL = MAT_ProjectionFOV + 4,                           /*< The projection with left eye convergence */
	MAT_ProjectionConvR = MAT_ProjectionConvL + 4,                         /*< The projection with right eye convergence */
	MAT_Roll = MAT_ProjectionConvR + 4,                                    /*< The head roll matrix */
	MAT_RollNegative = MAT_Roll + 4,                                       /*< The head roll matrix. (negative) */
	MAT_RollHalf = MAT_RollNegative + 4,                                   /*< The head roll matrix. (half roll) */
	MAT_TransformL = MAT_RollHalf + 4,                                     /*< Left matrix used to roll (if roll enabled) and shift view for ipd. */
	MAT_TransformR = MAT_TransformL + 4,                                   /*< Right matrix used to roll (if roll enabled) and shift view for ipd. */
	MAT_ViewProjectionL = MAT_TransformR + 4,                              /*< Left view projection matrix */
	MAT_ViewProjectionR = MAT_ViewProjectionL + 4,                         /*< Right view projection matrix */
	MAT_ViewProjectionTransL = MAT_ViewProjectionR + 4,                    /*< Left view projection transform matrix */
	MAT_ViewProjectionTransR = MAT_ViewProjectionTransL + 4,               /*< Right view projection transform matrix */
	MAT_ViewProjectionTransNoRollL = MAT_ViewProjectionTransR + 4,         /*< Left view projection transform matrix (no roll) */
	MAT_ViewProjectionTransNoRollR = MAT_ViewProjectionTransNoRollL + 4,   /*< Right view projection transform matrix (no roll) */
	MAT_Position = MAT_ViewProjectionTransNoRollR + 4,                     /*< Positional translation matrix */
	MAT_GatheredL = MAT_Position + 4,                                      /*< Gathered matrix to be used in gathered modifications */
	MAT_GatheredR = MAT_GatheredL + 4,                                     /*< Gathered matrix to be used in gathered modifications */
	MAT_HudL = MAT_GatheredR + 4,                                          /*< Left HUD matrix. */
	MAT_HudR = MAT_HudL + 4,                                               /*< Right HUD matrix */
	MAT_GuiL = MAT_HudR + 4,                                               /*< Left GUI matrix. */
	MAT_GuiR = MAT_GuiL + 4,                                               /*< Right GUI matrix. */
	MAT_Squash = MAT_GuiR + 4,                                             /*< Squash scaling matrix, to be used in HUD/GUI scaling matrices. */
	MAT_HudDistance = MAT_Squash + 4,                                      /*< HUD distance matrix, to be used in HUD scaling matrices. */
	MAT_Hud3dDepthL = MAT_HudDistance + 4,                                 /*< HUD 3d depth matrix, to be used in HUD separation matrices. */
	MAT_Hud3dDepthR = MAT_Hud3dDepthL + 4,                                 /*< HUD 3d depth matrix, to be used in HUD separation matrices. */
	MAT_Hud3dDepthShiftL = MAT_Hud3dDepthR + 4,                            /*< HUD 3d depth matrix (shifted), to be used in HUD separation matrices. */
	MAT_Hud3dDepthShiftR = MAT_Hud3dDepthShiftL + 4,                       /*< HUD 3d depth matrix (shifted), to be used in HUD separation matrices. */
	MAT_Gui3dDepthL = MAT_Hud3dDepthShiftR + 4,                            /*< GUI 3d depth matrix, to be used in GUI separation matrices. */
	MAT_Gui3dDepthR = MAT_Gui3dDepthL + 4,                                 /*< GUI 3d depth matrix, to be used in GUI separation matrices. */
	MAT_ConvergenceOffL = MAT_Gui3dDepthR + 4,                             /*< Convergence offset left. (only translation) */
	MAT_ConvergenceOffR = MAT_ConvergenceOffL + 4,                         /*< Convergence offset right. (only translation) */

	Math_Registers_Size = MAT_GatheredR + 4     // not used
};
constexpr size_t Math_Registers_Size = (size_t)MathRegisters::Math_Registers_Size;

#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
#elif defined(VIREIO_D3D9)
typedef D3DXVECTOR4 REGISTER4F;
#endif

/// <summary>
/// Class for any matrix/vector calculation.
/// Calculates different matrices and vertices (shader registers) for different nodes.
/// Compression of Class >ViewAdjustment< 2013 by Chris Drain
/// 
/// TODO !! PROJECTION FOV !! PIXEL SHADER ROLL !! CONVERGENCE FOR MONITOR STEREO !! MATRIX POSITION TRACKING !! HMD LENS X CENTER !!
/// </summary>
class ModificationCalculation
{
public:
	ModificationCalculation(Vireio_GameConfiguration* psConfig) : m_psConfig(psConfig)
	{
		SetFloat(MathFloatFields::AspectMultiplier, psConfig->fAspectMultiplier);
		SetFloat(MathFloatFields::FOV_H, 110.0f);
		SetFloat(MathFloatFields::Roll, 0.f);
		SetFloat(MathFloatFields::Convergence, psConfig->fConvergence);
		SetFloat(MathFloatFields::IPD, psConfig->fIPD);
		SetFloat(MathFloatFields::Separation_World, (psConfig->fIPD / 2.f) * psConfig->fWorldScaleFactor);
		SetFloat(MathFloatFields::Separation_IPDAdjustment, ((psConfig->fIPD - IPD_DEFAULT) / 2.f) * psConfig->fWorldScaleFactor);
		SetFloat(MathFloatFields::Frustum_Asymmetry, 0.f);   // ignore convergence settings for now
		SetFloat(MathFloatFields::Physical_Screensize, 1.f); // set to 1 to avoid division by zero
		SetFloat(MathFloatFields::LensXCenterOffset, 0.f);
		ComputeViewTransforms();
	}
	virtual ~ModificationCalculation() {}

	/// <summary>
	/// Loads basic fields from config.
	/// </summary>
	/// <param name="psConfig"></param>
	void Load(Vireio_GameConfiguration* psConfig)
	{
		m_psConfig = psConfig;
		SetFloat(MathFloatFields::Convergence, psConfig->fConvergence);
		SetFloat(MathFloatFields::IPD, psConfig->fIPD);
		SetFloat(MathFloatFields::Separation_World, (psConfig->fIPD / 2.f) * psConfig->fWorldScaleFactor);
		SetFloat(MathFloatFields::Separation_IPDAdjustment, ((psConfig->fIPD - IPD_DEFAULT) / 2.f) * psConfig->fWorldScaleFactor);

		// TODO !! PROJECTION FOV !
	}

	/// <summary>
	/// Computes all necessary view matrices for modification/transform.
	/// Do not change the succession of the computiations here !!
	/// </summary>
	void ComputeViewTransforms()
	{
		Compute(MathRegisters::MAT_BasicProjection);
		Compute(MathRegisters::MAT_ProjectionFOV);
		Compute(MathRegisters::MAT_ProjectionConvL);
		Compute(MathRegisters::MAT_Roll);
		Compute(MathRegisters::MAT_TransformL);
		Compute(MathRegisters::MAT_ViewProjectionL);
		Compute(MathRegisters::MAT_ViewProjectionTransL);

		// TODO !! POSITIONAL TRACKING !
	}

	/// <summary>
	/// Set float field.
	/// </summary>
	/// <param name="eIndex">Index based on enumeration</param>
	/// <param name="fValue">Value to be applied</param>
	void SetFloat(MathFloatFields eIndex, float fValue)
	{
		m_aMathFloat[(size_t)eIndex] = fValue;
	}

	/// <summary>
	/// Set register field.
	/// </summary>
	/// <param name="eIndex">Index based on enumeration</param>
	/// <param name="fValue">Value to be applied</param>
	void SetRegister(MathRegisters eIndex, REGISTER4F fValue)
	{
		m_aMathRegisters[(size_t)eIndex] = fValue;
	}

	/// <summary>
	/// Any computation (update) done here.
	/// </summary>
	/// <param name="eRegister">The register to be updated</param>
	void Compute(MathRegisters eRegister)
	{
		// Minimum (near) Z-value
		const float n = 0.1f;
		// Maximum (far) Z-value
		const float f = 10.0f;
		// Minimum (left) X-value
		const float l = -0.5f;
		// Maximum (right) X-value
		const float r = 0.5f;

		switch (eRegister)
		{
		case MathRegisters::VEC_PositionTransform:
			break;
		case MathRegisters::MAT_BasicProjection:
		case MathRegisters::MAT_ProjectionInv:
		{
			float fAspect = m_aMathFloat[(size_t)MathFloatFields::AspectMultiplier];
			// Maximum (top) y-value of the view volume
			float t = 0.5f / fAspect;
			// Minimum (bottom) y-value of the volume
			float b = -0.5f / fAspect;
			// Calculate basic projection
			D3DXMatrixPerspectiveOffCenterLH((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_BasicProjection], l, r, b, t, n, f);
			D3DXMatrixInverse((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv], 0, (D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_BasicProjection]);
		}
		break;
		case MathRegisters::MAT_ProjectionFOV:
		{
			float fAspect = m_aMathFloat[(size_t)MathFloatFields::AspectMultiplier];
			float fFOV_horiz = m_aMathFloat[(size_t)MathFloatFields::FOV_H];

			// TODO !! NO HMD ?? see ViewAdjustment::UpdateProjectionMatrices()

			// Calculate vertical fov from provided horizontal
			float fFOV_vert = m_aMathFloat[(size_t)MathFloatFields::FOV_V] = 2.0f * atan(tan(D3DXToRadian(fFOV_horiz) / 2.0f) * fAspect);

			// And left and right (identical in this case)
			D3DXMatrixPerspectiveFovLH((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionFOV], fFOV_vert, fAspect, n, f);
		}
		break;
		case MathRegisters::MAT_ProjectionConvL:
		case MathRegisters::MAT_ProjectionConvR:
		case MathRegisters::MAT_ConvergenceOffL:
		case MathRegisters::MAT_ConvergenceOffR:
		{
			// TODO !! CONVERGENCE FOR MONITOR STEREO

			float frustumAsymmetryInMeters = m_aMathFloat[(size_t)MathFloatFields::Frustum_Asymmetry];
			float physicalScreenSizeInMeters = m_aMathFloat[(size_t)MathFloatFields::Physical_Screensize];
			float fAspect = m_aMathFloat[(size_t)MathFloatFields::AspectMultiplier];

			// Maximum (top) y-value of the view volume
			float t = 0.5f / fAspect;
			// Minimum (bottom) y-value of the volume
			float b = -0.5f / fAspect;

			// divide the frustum asymmetry by the assumed physical size of the physical screen
			float frustumAsymmetryLeftInMeters = (frustumAsymmetryInMeters * fLEFT_CONSTANT) / physicalScreenSizeInMeters;
			float frustumAsymmetryRightInMeters = (frustumAsymmetryInMeters * fRIGHT_CONSTANT) / physicalScreenSizeInMeters;

			// get the horizontal screen space size and compute screen space adjustment
			float screenSpaceXSize = abs(l) + abs(r);
			float multiplier = screenSpaceXSize / 1; // = 1 meter
			float frustumAsymmetryLeft = frustumAsymmetryLeftInMeters * multiplier;
			float frustumAsymmetryRight = frustumAsymmetryRightInMeters * multiplier;

			// now, create the re-projection matrices for both eyes using this frustum asymmetry
			D3DXMatrixPerspectiveOffCenterLH((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvL], l + frustumAsymmetryLeft, r + frustumAsymmetryLeft, b, t, n, f);
			D3DXMatrixPerspectiveOffCenterLH((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvR], l + frustumAsymmetryRight, r + frustumAsymmetryRight, b, t, n, f);

			// create convergence offset matrices without projection
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_ConvergenceOffL], frustumAsymmetryLeftInMeters * m_psConfig->fWorldScaleFactor, 0, 0);
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_ConvergenceOffR], frustumAsymmetryRightInMeters * m_psConfig->fWorldScaleFactor, 0, 0);
		}
		break;
		case MathRegisters::MAT_Roll:
		case MathRegisters::MAT_RollNegative:
		case MathRegisters::MAT_RollHalf:
			D3DXMatrixRotationZ((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Roll], m_aMathFloat[(size_t)MathFloatFields::Roll]);
			D3DXMatrixRotationZ((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_RollNegative], -m_aMathFloat[(size_t)MathFloatFields::Roll]);
			D3DXMatrixRotationZ((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_RollHalf], m_aMathFloat[(size_t)MathFloatFields::Roll] * .5f);
			break;
		case MathRegisters::MAT_TransformL:
		case MathRegisters::MAT_TransformR:
		case MathRegisters::MAT_ViewProjectionTransNoRollL:
		case MathRegisters::MAT_ViewProjectionTransNoRollR:
		{
			float fSeparation_World = m_aMathFloat[(size_t)MathFloatFields::Separation_World];

			// separation settings are overall (HMD and desktop), since they are based on physical IPD
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_TransformL], fSeparation_World * fLEFT_CONSTANT, 0, 0);
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_TransformR], fSeparation_World * fRIGHT_CONSTANT, 0, 0);

			// update "no-roll" matrices here before roll is applied eventually
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionTransNoRollL] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_TransformL] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvL];
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionTransNoRollR] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_TransformR] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvR];

			// head roll - only if using translation implementation
			if (m_psConfig->nRollImpl == 1)
			{
				D3DXMatrixMultiply((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_TransformL], (D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Roll], (D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_TransformL]);
				D3DXMatrixMultiply((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_TransformR], (D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Roll], (D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_TransformR]);
			}
		}
		break;
		case MathRegisters::MAT_ViewProjectionL:
		case MathRegisters::MAT_ViewProjectionR:
			// head roll - only if using translation implementation
			if (m_psConfig->nRollImpl == 1)
			{
				// projection l/r
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionL] =
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Roll] *
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvL];
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionR] =
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Roll] *
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvR];
			}
			else
			{
				// projection l/r
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionL] =
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvL];
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionR] =
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
					(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvR];
			}
			break;
		case MathRegisters::MAT_ViewProjectionTransL:
		case MathRegisters::MAT_ViewProjectionTransR:
			// projection l/r
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionTransL] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_TransformL] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvL];
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ViewProjectionTransR] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_TransformR] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionConvR];
			break;
		case MathRegisters::MAT_Position:
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Position],
				m_aMathRegisters[(size_t)MathRegisters::VEC_PositionTransform].x,
				m_aMathRegisters[(size_t)MathRegisters::VEC_PositionTransform].y,
				m_aMathRegisters[(size_t)MathRegisters::VEC_PositionTransform].z);
			break;
		case MathRegisters::MAT_GatheredL:
		case MathRegisters::MAT_GatheredR:
			// TODO !! MATRIX GATHER METHOD !!
			break;
		case MathRegisters::MAT_HudL:
		case MathRegisters::MAT_HudR:
		case MathRegisters::MAT_GuiL:
		case MathRegisters::MAT_GuiR:
		case MathRegisters::MAT_Squash:
		case MathRegisters::MAT_HudDistance:
		case MathRegisters::MAT_Hud3dDepthL:
		case MathRegisters::MAT_Hud3dDepthR:
		case MathRegisters::MAT_Hud3dDepthShiftL:
		case MathRegisters::MAT_Hud3dDepthShiftR:
		case MathRegisters::MAT_Gui3dDepthL:
		case MathRegisters::MAT_Gui3dDepthR:
		{
			// squash
			float fSquash = m_aMathFloat[(size_t)MathFloatFields::Squash];
			D3DXMatrixScaling((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Squash], fSquash, fSquash, 1);

			// hudDistance
			float fHudDistance = m_aMathFloat[(size_t)MathFloatFields::HUD_Distance];
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_HudDistance], 0, 0, fHudDistance);

			// hud3DDepth
			float fHud3DDepth = m_aMathFloat[(size_t)MathFloatFields::HUD_Depth];
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Hud3dDepthL], fHud3DDepth, 0, 0);
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Hud3dDepthR], -fHud3DDepth, 0, 0);
			float fAdditionalSeparation = (1.5f - fHudDistance) * m_aMathFloat[(size_t)MathFloatFields::LensXCenterOffset];
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Hud3dDepthShiftL], fHud3DDepth + fAdditionalSeparation, 0, 0);
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Hud3dDepthShiftR], -fHud3DDepth - fAdditionalSeparation, 0, 0);

			// gui3DDepth
			float fGui3DDepth = m_aMathFloat[(size_t)MathFloatFields::GUI_Depth] + m_aMathFloat[(size_t)MathFloatFields::Separation_IPDAdjustment];
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Gui3dDepthL], fGui3DDepth, 0, 0);
			D3DXMatrixTranslation((D3DXMATRIX*)&m_aMathRegisters[(size_t)MathRegisters::MAT_Gui3dDepthR], -fGui3DDepth, 0, 0);

			// gui/hud matrices - Just use the default projection not the PFOV
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_HudL] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Hud3dDepthL] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_TransformL] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_HudDistance] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_BasicProjection];
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_HudR] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Hud3dDepthR] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_TransformR] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_HudDistance] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_BasicProjection];
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_GuiL] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Gui3dDepthL] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Squash] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_BasicProjection];
			(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_GuiL] =
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_ProjectionInv] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Gui3dDepthR] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_Squash] *
				(D3DXMATRIX)m_aMathRegisters[(size_t)MathRegisters::MAT_BasicProjection];
		}
		break;
		default:
			break;
		}
	}

	/// <summary>
	/// Retrieves an internal float value.
	/// </summary>
	/// <param name="eIndex">Index based on (MathFloatFields enum)</param>
	/// <returns>Current value of specified index</returns>
	float Get(MathFloatFields eIndex)
	{
		return m_aMathFloat[(size_t)eIndex];
	}

	/// <summary>
	/// Retrieves an internal vector value.
	/// </summary>
	/// <param name="eRegister">Register index (MathRegisters enum)</param>
	/// <returns>Current value (field) of specified index</returns>
	D3DXVECTOR4 Get(MathRegisters eRegister)
	{
		return D3DXVECTOR4((float*)&m_aMathRegisters[(size_t)eRegister]);
	}

	/// <summary>
	/// Retrieves an internal matrix value.
	/// </summary>
	/// <param name="eRegister">Register index (MathRegisters enum)</param>
	/// <returns>Current value (field) of specified index</returns>
	D3DXMATRIX Get(MathRegisters eRegister, const unsigned uSize = 4)
	{
		// out of range ?
		if ((uSize != 4) || ((uSize + 4) > (unsigned)Math_Registers_Size)) return D3DXMATRIX();
		return D3DXMATRIX((float*)&m_aMathRegisters[(size_t)eRegister]);
	}

private:
	/// <summary> All mathematical registers needed to compute any modification. </summary>
	std::array<REGISTER4F, Math_Registers_Size> m_aMathRegisters;
	/// <summary> All float fields needed to compute any modification. </summary>
	std::array<float, Math_FloatFields_Size> m_aMathFloat;
	/// <summary>Vireio v4.x game configuration.</summary>
	Vireio_GameConfiguration* m_psConfig;
};
#pragma endregion

#pragma region => Shader constant modification
/// <summary>
/// Abstract class acts as the skeleton class for Shader Modification Classes,
/// contains no shader modification logic simply keeps track of info.
/// Must implement ApplyModification method.
/// Prototype for any shader register modification.
/// 
/// Original class by Chris Drain (c) 2013
/// </summary>
/// <typeparam name="T"> The type of parameter to use for applying modifications, default is float.</typeparam>
template <class T = float>
class ShaderConstantModification
{
public:
	/// <summary> Constructor/Desctructor </summary>
	/// <param name="modID">Identifier of the modification. Identifier enumerations defined in ShaderConstantModificationFactory</param>
	ShaderConstantModification(UINT modID, std::shared_ptr<ModificationCalculation> pcCalculaion) : m_ModificationID(modID), m_pcCalculation(pcCalculaion) {}
	virtual ~ShaderConstantModification() {}

	/// <summary> Pure virtual method, should apply the modification to produce left and right versions </summary>
	virtual void ApplyModification(const T* inData, std::vector<T>* outLeft, std::vector<T>* outRight) = 0;

	/// <summary> Simply a way to identify this modification. Useful for comparing shadermodification equality </summary>
	UINT m_ModificationID;

	/// <summary> Calculation class. </summary>
	std::shared_ptr<ModificationCalculation> m_pcCalculation;
};

/// <summary>
/// Constant modification rule (v4+) normalized.
/// </summary>
struct Vireio_Constant_Modification_Rule_Normalized
{
	char m_szConstantName[64];
	UINT m_dwBufferIndex;
	UINT m_dwBufferSize;
	UINT m_dwStartRegIndex;
	bool m_bUseName;
	bool m_bUsePartialNameMatch;
	bool m_bUseBufferIndex;
	bool m_bUseBufferSize;
	bool m_bUseStartRegIndex;
	UINT m_dwRegisterCount;
	UINT m_dwOperationToApply;
	bool m_bTranspose;
};

/// <summary>
/// Constant modification rule (v4+).
/// Stores all data necessary for a
/// Original class >ConstantModificationRule< 2013 by Chris Drain.
/// </summary>
struct Vireio_Constant_Modification_Rule
{
	/// <summary>
	/// Constructor.
	/// Creates empty shader constant modification rule.
	/// </summary>
	Vireio_Constant_Modification_Rule() :
		m_szConstantName("ThisWontMatchAnything"),
		m_dwBufferIndex(999999),
		m_dwBufferSize(0),
		m_dwStartRegIndex(0),
		m_bUseName(false),
		m_bUsePartialNameMatch(false),
		m_bUseBufferIndex(false),
		m_bUseBufferSize(false),
		m_bUseStartRegIndex(false),
		m_dwRegisterCount(0),
		m_dwOperationToApply(0),
		m_bTranspose(false)
	{};

	/// <summary>
	/// Constructor.
	/// Creates rule out of a normalized one.
	/// </summary>
	Vireio_Constant_Modification_Rule(Vireio_Constant_Modification_Rule_Normalized* pRuleNormalized) :
		m_szConstantName(pRuleNormalized->m_szConstantName),
		m_dwBufferIndex(pRuleNormalized->m_dwBufferIndex),
		m_dwBufferSize(pRuleNormalized->m_dwBufferSize),
		m_dwStartRegIndex(pRuleNormalized->m_dwStartRegIndex),
		m_bUseName(pRuleNormalized->m_bUseName),
		m_bUsePartialNameMatch(pRuleNormalized->m_bUsePartialNameMatch),
		m_bUseBufferIndex(pRuleNormalized->m_bUseBufferIndex),
		m_bUseBufferSize(pRuleNormalized->m_bUseBufferSize),
		m_bUseStartRegIndex(pRuleNormalized->m_bUseStartRegIndex),
		m_dwRegisterCount(pRuleNormalized->m_dwRegisterCount),
		m_dwOperationToApply(pRuleNormalized->m_dwOperationToApply),
		m_bTranspose(pRuleNormalized->m_bTranspose)
	{};

	/// <summary>
	/// Constructor.
	/// Creates shader constant modification rule by specified data.
	/// @param szConstantName Constant string name.
	/// @param dwBufferIndex The index of the constant buffer (as set with ->?SSetConstantBuffers()).
	/// @param dwBufferSize The size of the constant buffer.
	/// @param dwStartRegIndex Shader start register of that rule.
	/// @param bUseName True to use the constant name to identify the constant.
	/// @param bUsePartialNameMatch True to use partial string name matches to identify the constant.
	/// @param bUseBufferIndex True to use the buffer index to identify the constant.
	/// @param bUseBufferSize True to use the buffer size to identify the constant.
	/// @param bUseStartRegIndex True to use the constant start register to identify the constant.
	/// @param dwRegisterCount Constant size (in shader registers = vectors = 4*sizeof(float)).
	/// @param dwOperationToApply Modification identifier.
	/// @param bTranspose True if input matrix should be bTransposed before modifying (and bTransposed back after).
	/// </summary>
	Vireio_Constant_Modification_Rule(std::string szConstantName, UINT dwBufferIndex, UINT dwBufferSize, UINT dwStartRegIndex, bool bUseName, bool bUsePartialNameMatch, bool bUseBufferIndex, bool bUseBufferSize, bool bUseStartRegIndex,
		UINT dwRegisterCount, UINT dwOperationToApply, bool bTranspose) :
		m_szConstantName(szConstantName),
		m_dwBufferIndex(dwBufferIndex),
		m_dwBufferSize(dwBufferSize),
		m_dwStartRegIndex(dwStartRegIndex),
		m_bUseName(bUseName),
		m_bUsePartialNameMatch(bUsePartialNameMatch),
		m_bUseBufferIndex(bUseBufferIndex),
		m_bUseBufferSize(bUseBufferSize),
		m_bUseStartRegIndex(bUseStartRegIndex),
		m_dwRegisterCount(dwRegisterCount),
		m_dwOperationToApply(dwOperationToApply),
		m_bTranspose(bTranspose)
	{};

	/// <summary>
	/// Constant string name.
	/// Empty string is "no constant".
	/// </summary>
	std::string m_szConstantName;
	/// <summary>
	/// If the shader has no constant table, we need the constant buffer index to identify the constant.
	/// </summary>
	UINT m_dwBufferIndex;
	/// <summary>
	/// If the shader has no constant table, we need the constant buffer size to identify the constant.
	/// </summary>
	UINT m_dwBufferSize;
	/// <summary>
	/// Shader start register of that rule.
	/// </summary>
	UINT m_dwStartRegIndex;
	/// <summary>
	/// If true the full constant string name will be used to identify the constant.
	/// </summary>
	bool m_bUseName;
	/// <summary>
	/// If true the partial constant string name will be used to identify the constant.
	/// </summary>
	bool m_bUsePartialNameMatch;
	/// <summary>
	/// If true the buffer index will be used to identify the constant.
	/// </summary>
	bool m_bUseBufferIndex;
	/// <summary>
	/// If true the buffer size will be used to identify the constant.
	/// </summary>
	bool m_bUseBufferSize;
	/// <summary>
	/// If true the start register index will be used to identify the constant.
	/// </summary>
	bool m_bUseStartRegIndex;
	/// <summary>
	/// Register count, 4 for Matrix, 1 for Vector, 2 for half-size matrices.
	/// For v4+ rules this count is used instead of D3DXPARAMETER_CLASS (used in v1.1->v3.x of the Driver).
	/// For this value there is no option (bool) to Use/NotUse to identify since this value is ALW
	/// </summary>
	UINT m_dwRegisterCount;
	/// <summary>
	/// Modification identifier.
	/// Identifier (int that maps to a m_dwRegisterCount type indexed class) of the operation to apply.
	/// </summary>
	UINT m_dwOperationToApply;
	/// <summary>
	/// True if input matrix should be transposed before modifying (and transposed back after).
	/// </summary>
	bool m_bTranspose;
	/// <summary>
	/// The eventual modification class. (created by m_dwOperationToApply index)
	/// </summary>
	std::shared_ptr<ShaderConstantModification<>> m_pcModification;
};

/// <summary>
/// Dummy class for now... TODO !!!
/// </summary>
class Vector4SimpleTranslate : public ShaderConstantModification<float>
{
public:
	Vector4SimpleTranslate(UINT uModID, std::shared_ptr<ModificationCalculation> pcCalculation) : ShaderConstantModification(uModID, pcCalculation) {};

	virtual void ApplyModification(const float* inData, std::vector<float>* outLeft, std::vector<float>* outRight)
	{
		D3DXVECTOR4 tempLeft(inData);
		D3DXVECTOR4 tempRight(inData);

		outLeft->assign(&tempLeft[0], &tempLeft[0] + outLeft->size());
		outRight->assign(&tempRight[0], &tempRight[0] + outRight->size());
	}
};

/// <summary>
/// TODO !! create different modifications
/// </summary>
/// <param name="uModID">Modification Identifier</param>
/// <param name="pcCalculation">Elements Calculation class</param>
/// <returns></returns>
static std::shared_ptr<ShaderConstantModification<>> CreateVector4Modification(UINT uModID, std::shared_ptr<ModificationCalculation> pcCalculation)
{
	return std::make_shared<Vector4SimpleTranslate>(uModID, pcCalculation);
}

/// <summary>
/// TODO !! create different modifications
/// </summary>
/// <param name="uModID">Modification Identifier</param>
/// <param name="pcCalculation">Elements Calculation class</param>
/// <returns></returns>
static std::shared_ptr<ShaderConstantModification<>> CreateMatrixModification(UINT uModID, std::shared_ptr<ModificationCalculation> pcCalculation, bool bTranspose)
{
	UNREFERENCED_PARAMETER(bTranspose);

	return std::make_shared<Vector4SimpleTranslate>(uModID, pcCalculation);
}
#pragma endregion

#pragma region => Managed shader class
#if defined(VIREIO_D3D11) || defined(VIREIO_D3D10)
#elif defined(VIREIO_D3D9)
/// <summary>
/// Structure only used for shader specific rules.
/// Contains hash code and rule index.
/// </summary>
struct Vireio_Hash_Rule_Index
{
	unsigned __int32 unHash;
	unsigned __int32 unRuleIndex;
};
/// <summary>
/// Simple enumeration of supported Shaders.
/// </summary>
enum class Vireio_Supported_Shaders : int
{
	VertexShader,
	PixelShader,
};

/// <summary>
/// Simple copy of the d3d x constant structure.
/// With a std::string name and without a default value pointer.
/// </summary>
typedef struct _SAFE_D3DXCONSTANT_DESC
{
	std::string Name;                   /**< Constant name ***/

	D3DXREGISTER_SET RegisterSet;       /**< Register set ***/
	UINT RegisterIndex;                 /**< Register index ***/
	UINT RegisterCount;                 /**< Number of registers occupied ***/

	D3DXPARAMETER_CLASS Class;          /**<  Class ***/
	D3DXPARAMETER_TYPE Type;            /**<  Component type ***/

	UINT Rows;                          /**<  Number of rows ***/
	UINT Columns;                       /**<  Number of columns ***/
	UINT Elements;                      /**<  Number of array elements ***/
	UINT StructMembers;                 /**<  Number of structure member sub-parameters ***/

	UINT Bytes;                         /**<  Data size, in bytes ***/
} SAFE_D3DXCONSTANT_DESC;

/// <summary>
/// Managed proxy shader class.
/// Contains left and right shader constants.
/// </summary>
template <class T = float>
class IDirect3DManagedStereoShader9 : public T
{
public:
	/// <summary>
	/// Constructor.
	/// </summary>
	IDirect3DManagedStereoShader9(T* pcActualVertexShader, IDirect3DDevice9* pcOwningDevice, std::vector<Vireio_Constant_Modification_Rule>* pasConstantRules, std::vector<UINT>* paunGeneralIndices, std::vector<Vireio_Hash_Rule_Index>* pasShaderIndices, Vireio_Supported_Shaders eShaderType) :
		m_pcActualShader(pcActualVertexShader),
		m_pcOwningDevice(pcOwningDevice),
		m_pasConstantRules(pasConstantRules),
		m_paunGeneralIndices(paunGeneralIndices),
		m_pasShaderIndices(pasShaderIndices),
		m_unRefCount(1),
		m_eShaderType(eShaderType),
		m_bOutputShaderCode(false)
	{
		assert(pcActualVertexShader != NULL);
		assert(pcOwningDevice != NULL);
		assert(pasConstantRules != NULL);

		pcOwningDevice->AddRef();

		// get shader hash
		m_unShaderHash = ShaderHash(pcActualVertexShader);

		// create managed registers
		m_unMaxShaderConstantRegs = MAX_DX9_CONSTANT_REGISTERS; // TODO !! COUNT MAX CONSTANT REG NUMBER FOR THIS SHADER

		// ... and the buffer for the last SetShaderDataF() call
		m_afRegisterBuffer = std::vector<float>(m_unMaxShaderConstantRegs * VECTOR_LENGTH);

		// init the shader rules, first clear constants vector
		m_asConstantDesc = std::vector<SAFE_D3DXCONSTANT_DESC>();
		InitShaderRules();
	}
	/// <summary>
	/// Destructor.
	/// </summary>
	virtual ~IDirect3DManagedStereoShader9()
	{
		if (m_pcActualShader)
			m_pcActualShader->Release();

		if (m_pcOwningDevice)
			m_pcOwningDevice->Release();
	}

	/*** IUnknown methods ***/
	virtual HRESULT WINAPI QueryInterface(REFIID riid, LPVOID* ppv)
	{
		return m_pcActualShader->QueryInterface(riid, ppv);
	}
	virtual ULONG   WINAPI AddRef()
	{
		return ++m_unRefCount;
	}
	virtual ULONG   WINAPI Release()
	{
		if (--m_unRefCount == 0)
		{
			delete this;
			return 0;
		}

		return m_unRefCount;
	}

	/*** IDirect3DXShader9 methods ***/

	/// <summary>
	/// IDirect3DXShader9->GetDevice() call.
	/// </summary>
	virtual HRESULT WINAPI GetDevice(IDirect3DDevice9** ppcDevice)
	{
		if (!m_pcOwningDevice)
			return D3DERR_INVALIDCALL;
		else
		{
			*ppcDevice = m_pcOwningDevice;
			//m_pOwningDevice->AddRef(); //TODO Test this. Docs don't have the notice that is usually there about a refcount increase
			return D3D_OK;
		}
	}
	/// <summary>
	/// IDirect3DXShader9->GetFunction() call.
	/// </summary>
	virtual HRESULT WINAPI GetFunction(void* pDate, UINT* punSizeOfData)
	{
		return m_pcActualShader->GetFunction(pDate, punSizeOfData);
	}

	/*** IDirect3DManagedStereoShader9 methods ***/

	/// <summary>
	/// Inits the shader rules based on the constant table of the shader.
	/// </summary>
	void InitShaderRules()
	{
		// @see ShaderModificationRepository::GetModifiedConstantsF from Vireio < v3

		// clear constant rule vector
		m_asConstantRuleIndices = std::vector<Vireio_Constant_Rule_Index_DX9>();

		// clear register indices to max int
		FillMemory(m_aunRegisterModificationIndex, MAX_DX9_CONSTANT_REGISTERS * sizeof(UINT), 0xFF);

		// get shader function
		BYTE* pData = NULL;
		UINT pSizeOfData;

		m_pcActualShader->GetFunction(NULL, &pSizeOfData);
		pData = new BYTE[pSizeOfData];
		m_pcActualShader->GetFunction(pData, &pSizeOfData);

		// Load the constant descriptions for this shader and create StereoShaderConstants as the applicable rules require them.
		LPD3DXCONSTANTTABLE pConstantTable = NULL;
		D3DXGetShaderConstantTable(reinterpret_cast<DWORD*>(pData), &pConstantTable);

		if (pConstantTable)
		{
			// set to defaults
			IDirect3DDevice9* pcDevice = nullptr;
			m_pcActualShader->GetDevice(&pcDevice);
			if (pcDevice)
			{
				pConstantTable->SetDefaults(pcDevice);
				pcDevice->Release();
			}

			// get constant table description
			D3DXCONSTANTTABLE_DESC pDesc;
			pConstantTable->GetDesc(&pDesc);

			D3DXCONSTANT_DESC pConstantDesc[64];

			// loop throught constants
			for (UINT unI = 0; unI < pDesc.Constants; unI++)
			{
				D3DXHANDLE handle = pConstantTable->GetConstant(NULL, unI);
				if (handle == NULL) continue;

				UINT unConstantNum = 64;
				pConstantTable->GetConstantDesc(handle, pConstantDesc, &unConstantNum);
				if (unConstantNum >= 64)
				{
					OutputDebugString(L"[MAM] Need larger constant description buffer");
					unConstantNum = 63;
				}

				for (UINT unJ = 0; unJ < unConstantNum; unJ++)
				{
					// add to constant vector
					SAFE_D3DXCONSTANT_DESC sDesc = {};
					sDesc.Name = std::string(pConstantDesc[unJ].Name);
					sDesc.RegisterSet = pConstantDesc[unJ].RegisterSet;
					sDesc.RegisterIndex = pConstantDesc[unJ].RegisterIndex;
					sDesc.RegisterCount = pConstantDesc[unJ].RegisterCount;
					sDesc.Class = pConstantDesc[unJ].Class;
					sDesc.Type = pConstantDesc[unJ].Type;
					sDesc.Rows = pConstantDesc[unJ].Rows;
					sDesc.Columns = pConstantDesc[unJ].Columns;
					sDesc.Elements = pConstantDesc[unJ].Elements;
					sDesc.StructMembers = pConstantDesc[unJ].StructMembers;
					sDesc.Bytes = pConstantDesc[unJ].Bytes;
					m_asConstantDesc.push_back(sDesc);

					// register count 1 (= vector) and 4 (= matrix) supported
					if (((pConstantDesc[unJ].Class == D3DXPC_VECTOR) && (pConstantDesc[unJ].RegisterCount == 1))
						|| (((pConstantDesc[unJ].Class == D3DXPC_MATRIX_ROWS) || (pConstantDesc[unJ].Class == D3DXPC_MATRIX_COLUMNS)) && (pConstantDesc[unJ].RegisterCount == 4)))
					{
						// Check if any rules match this constant, first check shader specific indices
						bool bShaderSpecificRulePresent = false;
						for (UINT unK = 0; unK < (UINT)(*m_pasShaderIndices).size(); unK++)
						{
							// same hash code ?
							if ((*m_pasShaderIndices)[unK].unHash == m_unShaderHash)
							{
								// compare rule->description, break in case of success
								if (SUCCEEDED(VerifyConstantDescriptionForRule(&((*m_pasConstantRules)[(*m_pasShaderIndices)[unK].unRuleIndex]), &(pConstantDesc[unJ]), (*m_pasShaderIndices)[unK].unRuleIndex, (void*)pConstantDesc[unJ].DefaultValue)))
								{
									bShaderSpecificRulePresent = true;
									break;
								}
							}
						}

						// now check general indices if no shader specific rule got applied
						if (!bShaderSpecificRulePresent)
						{
							for (UINT unK = 0; unK < (UINT)(*m_paunGeneralIndices).size(); unK++)
							{
								// compare rule->description, break in case of success
								if (SUCCEEDED(VerifyConstantDescriptionForRule(&((*m_pasConstantRules)[(*m_paunGeneralIndices)[unK]]), &(pConstantDesc[unJ]), (*m_paunGeneralIndices)[unK], (void*)pConstantDesc[unJ].DefaultValue))) break;
							}
						}
					}
				}
			}
		}

		if (pConstantTable) pConstantTable->Release();
		if (pData) delete[] pData;
	}
	/// <summary>
	/// This shader is set new, applies all modifications.
	/// </summary>
	void SetShader(float* afRegisters)
	{
		UINT unInd = 0;

		// more data, loop through modified constants for this shader
		auto it = m_asConstantRuleIndices.begin();
		while (it != m_asConstantRuleIndices.end())
		{
			// apply to left and right data
			UINT unIndex = unInd;
			UINT unStartRegisterConstant = (*it).m_dwConstantRuleRegister;

			// get the matrix
			D3DXMATRIX sMatrix(&afRegisters[RegisterIndex(unStartRegisterConstant)]);
			{
				// matrix to be transposed ?
				bool bTranspose = (*m_pasConstantRules)[m_asConstantRuleIndices[unIndex].m_dwIndex].m_bTranspose;
				if (bTranspose)
				{
					D3DXMatrixTranspose(&sMatrix, &sMatrix);
				}

				// do modification
				D3DXMATRIX sMatrixLeft, sMatrixRight;
				((ShaderMatrixModification*)(*m_pasConstantRules)[m_asConstantRuleIndices[unIndex].m_dwIndex].m_pcModification.get())->DoMatrixModification(sMatrix, sMatrixLeft, sMatrixRight);

				// transpose back
				if (bTranspose)
				{
					D3DXMatrixTranspose(&sMatrixLeft, &sMatrixLeft);
					D3DXMatrixTranspose(&sMatrixRight, &sMatrixRight);
				}

				m_asConstantRuleIndices[unIndex].m_asConstantDataLeft = (D3DMATRIX)sMatrixLeft;
				m_asConstantRuleIndices[unIndex].m_asConstantDataRight = (D3DMATRIX)sMatrixRight;
			}
			it++; unInd++;
		}
	}
	/// <summary>
	/// This shader is set old, applies unmodified data.
	/// </summary>
	void SetShaderOld(IDirect3DDevice9* pcDevice, float* afRegisters)
	{
		UINT unInd = 0;

		// more data, loop through modified constants for this shader
		auto it = m_asConstantRuleIndices.begin();
		while (it != m_asConstantRuleIndices.end())
		{
			// apply to left and right data
			UINT unIndex = unInd;
			UINT unStartRegisterConstant = (*it).m_dwConstantRuleRegister;

			// set back modification
			pcDevice->SetVertexShaderConstantF(unStartRegisterConstant, &afRegisters[RegisterIndex(unStartRegisterConstant)], 4);

			it++; unInd++;
		}
	}
	/// <summary>
	/// Override IDirect3DDevice9->SetShaderConstantF() here.
	/// </summary>
	HRESULT SetShaderConstantF(UINT unStartRegister, const float* pfConstantData, UINT unVector4fCount, bool& bModified, RenderPosition eRenderSide, float* afRegisters)
	{
		// no rules present ? return
		if (!m_asConstantRuleIndices.size()) return S_OK;

		// set buffer
		memcpy(&m_afRegisterBuffer[0], pfConstantData, unVector4fCount * 4 * sizeof(float));

		// modification present for this index ?
		if ((m_aunRegisterModificationIndex[unStartRegister] < (UINT)m_asConstantRuleIndices.size()) && (unVector4fCount == 4))
		{
			// apply to left and right data
			UINT unIndex = m_aunRegisterModificationIndex[unStartRegister];
			bModified = true;

			// get the matrix
			D3DXMATRIX sMatrix(&afRegisters[RegisterIndex(unStartRegister)]);
			{
				// matrix to be transposed ?
				bool bTranspose = (*m_pasConstantRules)[m_asConstantRuleIndices[unIndex].m_dwIndex].m_bTranspose;
				if (bTranspose)
				{
					D3DXMatrixTranspose(&sMatrix, &sMatrix);
				}

				// do modification
				D3DXMATRIX sMatrixLeft, sMatrixRight;
				((ShaderMatrixModification*)(*m_pasConstantRules)[m_asConstantRuleIndices[unIndex].m_dwIndex].m_pcModification.get())->DoMatrixModification(sMatrix, sMatrixLeft, sMatrixRight);

				// transpose back
				if (bTranspose)
				{
					D3DXMatrixTranspose(&sMatrixLeft, &sMatrixLeft);
					D3DXMatrixTranspose(&sMatrixRight, &sMatrixRight);
				}

				m_asConstantRuleIndices[unIndex].m_asConstantDataLeft = (D3DMATRIX)sMatrixLeft;
				m_asConstantRuleIndices[unIndex].m_asConstantDataRight = (D3DMATRIX)sMatrixRight;

				// copy modified data to buffer
				if (eRenderSide == RenderPosition::Left)
					memcpy(&m_afRegisterBuffer[0], &sMatrixLeft, sizeof(D3DMATRIX));
				else
					memcpy(&m_afRegisterBuffer[0], &sMatrixRight, sizeof(D3DMATRIX));

			}
		}
		else
		{
			UINT unInd = 0;

			// more data, loop through modified constants for this shader
			auto it = m_asConstantRuleIndices.begin();
			while (it != m_asConstantRuleIndices.end())
			{
				// register in range ?
				if ((unStartRegister < ((*it).m_dwConstantRuleRegister + (*it).m_dwConstantRuleRegisterCount)) && ((unStartRegister + unVector4fCount) > (*it).m_dwConstantRuleRegister))
				{
					// apply to left and right data
					bModified = true;
					UINT unIndex = unInd;
					UINT unStartRegisterConstant = (*it).m_dwConstantRuleRegister;

					// get the matrix
					D3DXMATRIX sMatrix(&afRegisters[RegisterIndex(unStartRegisterConstant)]);
					{
						// matrix to be transposed ?
						bool bTranspose = (*m_pasConstantRules)[m_asConstantRuleIndices[unIndex].m_dwIndex].m_bTranspose;
						if (bTranspose)
						{
							D3DXMatrixTranspose(&sMatrix, &sMatrix);
						}

						// do modification
						D3DXMATRIX sMatrixLeft, sMatrixRight;
						((ShaderMatrixModification*)(*m_pasConstantRules)[m_asConstantRuleIndices[unIndex].m_dwIndex].m_pcModification.get())->DoMatrixModification(sMatrix, sMatrixLeft, sMatrixRight);

						// transpose back
						if (bTranspose)
						{
							D3DXMatrixTranspose(&sMatrixLeft, &sMatrixLeft);
							D3DXMatrixTranspose(&sMatrixRight, &sMatrixRight);
						}

						m_asConstantRuleIndices[unIndex].m_asConstantDataLeft = (D3DMATRIX)sMatrixLeft;
						m_asConstantRuleIndices[unIndex].m_asConstantDataRight = (D3DMATRIX)sMatrixRight;

						// copy modified data to buffer
						if (eRenderSide == RenderPosition::Left)
						{
							if (unStartRegister <= unStartRegisterConstant)
								memcpy(&m_afRegisterBuffer[unStartRegisterConstant - unStartRegister], &sMatrixLeft, sizeof(D3DMATRIX));
							else
								OutputDebugString(L"TODO: Handle partially changed matrices.");
						}
						else
						{
							if (unStartRegister <= unStartRegisterConstant)
								memcpy(&m_afRegisterBuffer[unStartRegisterConstant - unStartRegister], &sMatrixRight, sizeof(D3DMATRIX));
							else
								OutputDebugString(L"TODO: Handle partially changed matrices.");
						}
					}
				}
				it++; unInd++;
			}
		}

		return D3D_OK;
	}
	/// <summary>
	/// The hash code of the shader.
	/// </summary>
	uint32_t GetShaderHash() { return m_unShaderHash; }
	/// <summary>
	/// The actual shader.
	/// </summary>
	T* GetActualShader() { return m_pcActualShader; }
	/// <summary>
	/// The constant list.
	/// </summary>
	std::vector<SAFE_D3DXCONSTANT_DESC>* GetConstantDescriptions() { return &m_asConstantDesc; }

	/// <summary>
	/// The indices of the shader rules assigned to that shader.
	/// For DX9 these indices also contain the left/right modified constant data.
	/// </summary>
	std::vector<Vireio_Constant_Rule_Index_DX9> m_asConstantRuleIndices;
	/// <summary>
	/// Shader register buffer. Currently modified shader constant data.
	/// 4 floats == 1 register (defined in VECTOR_LENGTH).
	/// Filled with the last SetShaderConstantF update.
	/// </summary>
	std::vector<float> m_afRegisterBuffer;

protected:
	/// <summary>
	/// The actual vertex shader embedded.
	/// </summary>
	T* const m_pcActualShader;
	/// <summary>
	/// The type of this class.
	/// </summary>
	Vireio_Supported_Shaders m_eShaderType;
	/// <summary>
	/// Pointer to the D3D device that owns the shader.
	/// </summary>
	IDirect3DDevice9* m_pcOwningDevice;
	/// <summary>
	/// Internal reference counter.
	/// </summary>
	ULONG m_unRefCount;
	/// <summary>
	/// Maximum shader constant registers.
	/// </summary>
	UINT m_unMaxShaderConstantRegs;
	/// <summary>
	/// The shader hash code.
	/// </summary>
	uint32_t m_unShaderHash;
	/// <summary>
	/// Pointer to all available constant rules.
	/// </summary>
	std::vector<Vireio_Constant_Modification_Rule>* m_pasConstantRules;
	/// <summary>
	/// Pointer to all general rule indices.
	/// </summary>
	std::vector<UINT>* m_paunGeneralIndices;
	/// <summary>
	/// Pointer to all shader specfic rule indices.
	/// </summary>
	std::vector<Vireio_Hash_Rule_Index>* m_pasShaderIndices;
	/// <summary>
	/// Index of modification for the specified register.
	/// </summary>
	UINT m_aunRegisterModificationIndex[MAX_DX9_CONSTANT_REGISTERS];
	/// <summary>
	/// The constant descriptions for that shader.
	/// </summary>
	std::vector<SAFE_D3DXCONSTANT_DESC> m_asConstantDesc;

private:
	/// <summary>
	/// Simple helper to get the hash of a shader.
	/// @param pShader The input vertex shader.
	/// @return The hash code of the shader.
	/// </summary>
	uint32_t ShaderHash(T* pcShader)
	{
		if (!pcShader) return 0;

		BYTE* pnData = NULL;
		UINT unSizeOfData = 0;
		pcShader->GetFunction(NULL, &unSizeOfData);

		pnData = new BYTE[unSizeOfData];
		pcShader->GetFunction(pnData, &unSizeOfData);

		uint32_t unHash = GetHashCode(pnData, (int32_t)unSizeOfData, VIREIO_SEED);

		// output shader code ?
		if (m_bOutputShaderCode)
		{
			// optionally, output shader code to "?S(hash).txt"
			char buf[32]; ZeroMemory(&buf[0], 32);
			switch (m_eShaderType)
			{
			case VertexShader:
				sprintf_s(buf, "VS%u.txt", unHash);
				break;
			case PixelShader:
				sprintf_s(buf, "PS%u.txt", unHash);
				break;
			default:
				sprintf_s(buf, "UNKNOWN%u.txt", unHash);
				break;
			}
			std::ofstream oLogFile(buf, std::ios::ate);

			if (oLogFile.is_open())
			{
				LPD3DXBUFFER pcBuffer;
				D3DXDisassembleShader(reinterpret_cast<DWORD*>(pnData), NULL, NULL, &pcBuffer);
				oLogFile << static_cast<char*>(pcBuffer->GetBufferPointer()) << std::endl;
				oLogFile << std::endl << std::endl;
				oLogFile << "// Shader Hash   : " << unHash << std::endl;
			}
		}

		delete[] pnData;
		return unHash;
	}
	/// <summary>
	/// Compares a constant description and a shader rule, if succeedes it adds the index to the shader constant rule indices.
	/// </summary>
	HRESULT VerifyConstantDescriptionForRule(Vireio_Constant_Modification_Rule* psRule, D3DXCONSTANT_DESC* psDescription, UINT unRuleIndex, void* pDefaultValue)
	{
		// Type match
		if (psRule->m_dwRegisterCount == psDescription->RegisterCount)
		{
			// name match required
			if (psRule->m_szConstantName.size() > 0)
			{
				bool nameMatch = false;
				if (psRule->m_bUsePartialNameMatch)
				{
					nameMatch = std::strstr(psDescription->Name, psRule->m_szConstantName.c_str()) != NULL;
				}
				else
				{
					nameMatch = psRule->m_szConstantName.compare(psDescription->Name) == 0;
				}

				if (!nameMatch)
				{
					// no match
					return E_NO_MATCH;
				}
			}

			// register match required
			if (psRule->m_dwStartRegIndex != UINT_MAX)
			{
				if (psRule->m_dwStartRegIndex != psDescription->RegisterIndex)
				{
					// no match
					return E_NO_MATCH;
				}
			}

#ifdef _DEBUG
			// output shader constant + index 
			switch (psDescription->Class)
			{
			case D3DXPC_VECTOR:
				OutputDebugString(L"VS: D3DXPC_VECTOR");
				break;
			case D3DXPC_MATRIX_ROWS:
				OutputDebugString(L"VS: D3DXPC_MATRIX_ROWS");
				break;
			case D3DXPC_MATRIX_COLUMNS:
				OutputDebugString(L"VS: D3DXPC_MATRIX_COLUMNS");
				break;
			default:
				OutputDebugString(L"VS: UNKNOWN_CONSTANT");
				break;
			}
			debugf("Register Index: %d", psDescription->RegisterIndex);
#endif 
			// set register index
			m_aunRegisterModificationIndex[psDescription->RegisterIndex] = (UINT)m_asConstantRuleIndices.size();

			// set constant rule index
			Vireio_Constant_Rule_Index_DX9 sConstantRuleIndex;
			sConstantRuleIndex.m_dwIndex = unRuleIndex;
			sConstantRuleIndex.m_dwConstantRuleRegister = psDescription->RegisterIndex;
			sConstantRuleIndex.m_dwConstantRuleRegisterCount = psDescription->RegisterCount;

			// init data if default value present
			if (pDefaultValue)
			{
				memcpy(&sConstantRuleIndex.m_afConstantDataLeft[0], pDefaultValue, psDescription->RegisterCount * sizeof(float) * 4);
				memcpy(&sConstantRuleIndex.m_afConstantDataRight[0], pDefaultValue, psDescription->RegisterCount * sizeof(float) * 4);
			};

			m_asConstantRuleIndices.push_back(sConstantRuleIndex);

			// only the first matching rule is applied to a constant
			return S_OK;
		}
		else return E_NO_MATCH;
	}

	/// <summary>
	/// True if shader code should be saved in file.
	/// </summary>
	bool m_bOutputShaderCode;
};
#endif
#pragma endregion