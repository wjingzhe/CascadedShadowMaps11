#pragma once

#include <d3dcommon.h>
#include <DirectXMath.h>

#define MAX_CASCADES 8

HRESULT CompileShaderFromFile(WCHAR* szFileName, D3D_SHADER_MACRO * macros, LPCSTR szEntryPoint,
	LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

// Used to do selection of the shadow buffer format
enum SHADOW_TEXTURE_FORMAT
{
	CASCADE_DXGI_FORMAT_R32_TYPELESS,
	CASCADE_DXGI_FORMAT_R24G8_TYPELESS,
	CASCADE_DXGI_FORMAT_R16_TYPELESS,
	CASCADE_DXGI_FORMAT_R8_TYPELESS
};

enum SCENE_SELECTION
{
	POWER_PLANT_SCENE,
	TEST_SCENE,
};

enum FIT_PROJECTION_TO_CASCADES
{
	FIT_TO_CASCADES,
	FIT_TO_SCENE
};

enum FIT_TO_NEAR_FAR
{
	FIT_NEAR_FAR_PANCAKING,
	FIT_NEAR_FAR_ZERO_ONE,
	FIT_NEAR_FAR_AABB,
	FIT_NEAR_FAR_SCENE_AABB,
};

enum CASCADE_SELECTION
{
	CASCADE_SELECTION_MAP,
	CASCADE_SELECTION_INTERVAL,
};

enum CAMERA_SELECTION
{
	EYE_CAMERA,
	LIGHT_CAMERA,
	ORTHO_CAMERA1,
	ORTHO_CAMERA2,
	ORTHO_CAMERA3,
	ORTHO_CAMERA4,
	ORTHO_CAMERA5,
	ORTHO_CAMERA6,
	ORTHO_CAMERA7,
	ORTHO_CAMERA8
};

//when these parameters change, we must reallocates the shadow resources.
struct CascadeConfig
{
	INT m_nCascadeLevels;
	SHADOW_TEXTURE_FORMAT m_ShadowBufferFormat;
	INT m_iRenderTargetBufferSizeInX;
};

struct CB_ALL_SHADOW_DATA
{
	DirectX::XMMATRIX m_WorldViewProj;
	DirectX::XMMATRIX m_World;
	DirectX::XMMATRIX m_WorldView;
	DirectX::XMMATRIX m_ShadowView;
	DirectX::XMVECTOR m_vCascadeOffset[8];
	DirectX::XMVECTOR m_vCascadeScale[8];

	INT m_nCascadeLeves; // number of Cascades
	INT m_iVisualizeCascades; // 1 is to visualize the cascades in different 
	INT m_iPCFBlurForLoopStart; // For loop begin value.For a 5x5 kernel this would be -2.
	INT m_iPCFBlurForLoopEnd;// For loop end value,For a 5x5 kernel this would be 3.


	// For Map based selection scheme, this keeps the pixels inside of the valid range.
	// When there is no boarder, these values are 0 and 1 respectively
	FLOAT m_fMinBorderPadding;
	FLOAT m_fMaxBorderPadding;
	FLOAT m_fShadowBiasFromGUI; // A shadow map offset to deal with self shadow artifacts.
								// There artifacts are aggrabated by PCF.

	FLOAT m_fShadowTexPartitionPerLevel;


	FLOAT m_fCascadeBlendArea;// Amount to overlap when blending between cascades.
	FLOAT m_fLogicTexelSizeInX;// Shadow map texel size.
	FLOAT m_fCascadedShadowMapTexelSizeInX;// Texel size in native map(texture are packed)
	FLOAT m_fPaddingForCB3;//Padding variables CBs must be a multiple of 16 bytes.

	DirectX::XMVECTOR m_vLightDir;

	FLOAT m_fCascadeFrustumEyeSpaceDepths[8];//The values along Z that separate the cascade.


};