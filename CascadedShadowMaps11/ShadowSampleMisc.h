#pragma once

#include <d3dcommon.h>
#include <DirectXMath.h>

#define MAX_CASCADES 8
#define MAX_CASCADE_COUNT_IN_4 ((MAX_CASCADES+1) / 4)
#define MAX_CASCADE_COUNT_MORE  ((MAX_CASCADES / 4+1)*4)

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

enum FIT_LIGHT_VIEW_FRUSTRUM
{
	FIT_LIGHT_VIEW_FRUSTRUM_TO_CASCADE_INTERVALS,
	FIT_LIGHT_VIEW_FRUSTRUM_TO_SCENE
};

enum FIT_NEAR_FAR
{
	FIT_NEAR_FAR_DEFAULT_ZERO_ONE,
	FIT_NEAR_FAR_ONLY_SCENE_AABB,
	FIT_NEAR_FAR_SCENE_AABB_AND_ORTHO_BOUND,
	FIT_NEAR_FAR_PANCAKING,
};

enum CASCADE_SELECTION_MODE
{
	CASCADE_SELECTION_MAP,//jingz 根据3D坐标变化至不同层级对应的纹理空间，迭代对比当前像素和shadowMapTexture的层级，合理范围内的像素则记录为当前shadowMapTexture的层级
	CASCADE_SELECTION_INTERVAL,//根据深度z值和层级划分的depth interval[深度区间]来对比像素所属层级
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
	INT m_nUsingCascadeLevelsCount;
	SHADOW_TEXTURE_FORMAT m_ShadowBufferFormat;
	INT m_iLengthOfShadowBufferSquare;
};

struct CB_ALL_SHADOW_DATA
{
	DirectX::XMMATRIX m_WorldViewProj;
	DirectX::XMMATRIX m_World;
	DirectX::XMMATRIX m_WorldView;
	DirectX::XMMATRIX m_ShadowView;
	DirectX::XMVECTOR m_vOffsetFactorFromOrthoProjToTexureCoord[8];
	DirectX::XMVECTOR m_vScaleFactorFromOrthoProjToTexureCoord[8];

	INT m_nCascadeLeves; // number of Cascades
	INT m_iIsVisualizeCascade; // 1 is to visualize the cascades in different 
	INT m_iPCFBlurForLoopStart; // For loop begin value.For a 5x5 kernel this would be -2.
	INT m_iPCFBlurForLoopEnd;// For loop end value,For a 5x5 kernel this would be 3.


	// For Map based selection scheme, this keeps the pixels inside of the valid range.
	// When there is no boarder, these values are 0 and 1 respectively
	FLOAT m_fMinBorderPaddingInShadowUV;
	FLOAT m_fMaxBorderPaddingInShadowUV;
	FLOAT m_fPCFShadowDepthBiaFromGUI; // A shadow map offset to deal with self shadow artifacts.
								// There artifacts are aggrabated by PCF.

	FLOAT m_fWidthPerShadowTextureLevel_InU;
	FLOAT m_fMaxBlendRatioBetweenCascadeLevel;// Amount to overlap when blending between cascades.
	FLOAT m_fLogicStepPerTexel;// Shadow map texel size.逻辑上每个Texel对应的Shadow步长(以UV单位制度1作为总长度)
	FLOAT m_fNativeCascadedShadowMapTexelStepInX;// Texel size in native map(texture are packed)//实际上被打包至一块的所有纹理分配到的UV单位步长
	FLOAT m_fPaddingForCB3;//Padding variables CBs must be a multiple of 16 bytes.

	DirectX::XMVECTOR m_vLightDir;

	DirectX::XMFLOAT4 m_fCascadePartitionDepthsInEyeSpace_InFloat4[MAX_CASCADE_COUNT_IN_4];//The values along Z that separate the cascade.

	//index = 0 被0.0f作为下限占用
	DirectX::XMFLOAT4 m_fCascadePartitionDepthsInEyeSpace_OnlyX[MAX_CASCADE_COUNT_MORE]; // the values along Z that separate the cascade.
																// Wastefully stored in float4 so they are array indexable
};