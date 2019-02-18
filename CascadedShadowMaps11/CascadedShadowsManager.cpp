#include "DXUT.h"

#include "CascadedShadowsManager.h"
#include "DXUTcamera.h"
#include "SDKmesh.h"
#include "xnacollision.h"
#include "SDKmisc.h"
#include "Resource.h"

using namespace DirectX;

static const XMVECTORF32 g_vFLTMAX = { FLT_MAX,FLT_MAX, FLT_MAX, FLT_MAX };
static const XMVECTORF32 g_vFLTMIN = { -FLT_MAX,-FLT_MAX, -FLT_MAX, -FLT_MAX };
static const XMVECTORF32 g_vHalfVector = { 0.5f,0.5f,0.5f,0.5f };
static const XMVECTORF32 g_vMultiplySetzwZero = { 1.0f,1.0f,0.0f,0.0f };
static const XMVECTORF32 g_vZero = { 0.0f,0.0f,0.0f,0.0f };

//------------------------------------------------------------------------
// Initialize the Manager. The manager performs all the work of calculating the render
// parameters of the shadow,creating the D3D resources,rendering the shadow,and rendering
// the actual scene.
//-----------------------------------------------------------------------
CascadedShadowsManager::CascadedShadowsManager()
	:m_pMeshVertexLayout(nullptr),
	m_pSamLinear(nullptr),
	m_pSamShadowPCF(nullptr),
	m_pSamShadowPoint(nullptr),
	m_pCascadedShadowMapTexture(nullptr),
	m_pCascadedShadowMapDSV(nullptr),
	m_pCascadedShadowMapSRV(nullptr),
	m_bIsBlurBetweenCascades(false),
	m_fMaxBlendRatioBetweenCascadeLevel(10.0f),
	m_RenderOneTileVP(m_RenderViewPort[0]),
	m_pDepthStencilStateLess(nullptr),
	m_pGlobalConstantBuffer(nullptr),
	m_pRasterizerStateScene(nullptr),
	m_pRasterizerStateShadow(nullptr),
	m_pRasterizerStateShadowPancake(nullptr),
	m_iPCFBlurSize(3),
	m_fPCFShadowDepthBia(0.002f),
	m_bIsDerivativeBaseOffset(false),
	m_pRenderOrthoShadowVertexShaderBlob(nullptr)
{
	sprintf_s(m_cVertexShaderMode, "vs_5_0");
	sprintf_s(m_cPixelShaderMode, "ps_5_0");
	sprintf_s(m_cGeometryShaderMode, "gs_5_0");


	for (INT index = 0;index < MAX_CASCADES;++index)
	{
		m_RenderViewPort[index].Height = (FLOAT)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
		m_RenderViewPort[index].Width = (FLOAT)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
		m_RenderViewPort[index].MaxDepth = 1.0f;
		m_RenderViewPort[index].MinDepth = 0.0f;
		m_RenderViewPort[index].TopLeftX = 0;
		m_RenderViewPort[index].TopLeftY = 0;
		m_pRenderSceneVertexShaderBlob[index] = nullptr;

		for (int x1 = 0;x1<2;++x1)
		{
			for (int x2 = 0;x2<2;++x2)
			{
				for (int x3 = 0; x3 < 2; ++x3)
				{
					m_pRenderSceneAllPixelShaderBlobs[index][x1][x2][x3] = nullptr;
				}
			}
		}

	}//for


}
CascadedShadowsManager::~CascadedShadowsManager()
{
	DestroyAndDeallocateShadowResources();
	SAFE_RELEASE(m_pRenderOrthoShadowVertexShaderBlob);

	for (int i = 0;i<MAX_CASCADES;++i)
	{
		SAFE_RELEASE(m_pRenderSceneVertexShaderBlob[i]);


		for (int x1 = 0;x1<2;++x1)
		{
			for (int x2 = 0;x2<2;++x2)
			{
				for (int x3 = 0;x3<2;++x3)
				{
					SAFE_RELEASE(m_pRenderSceneAllPixelShaderBlobs[i][x1][x2][x3]);
				}
			}
		}

	}

}


HRESULT CascadedShadowsManager::Init(ID3D11Device * pD3DDevice, ID3D11DeviceContext * pD3DImmediateContext, CDXUTSDKMesh * pMesh, CFirstPersonCamera * pViewerCamera, CFirstPersonCamera * pLightCamera, CascadeConfig * pCascadeConfig)
{
	HRESULT hr = S_OK;

	m_CopyOfCascadeConfig = *pCascadeConfig;

	//Initialize m_iBufferSize to 0 to trigger a reallocate on the first frame.
	m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare = 0;
	//Save a pointer to cascade config.Each frame we check our copy against the pointer.
	m_pCascadeConfig = pCascadeConfig;

	XMVECTOR vMeshMin;
	XMVECTOR vMeshMax;

	m_vSceneAABBMin = g_vFLTMAX;
	m_vSceneAABBMax = g_vFLTMIN;

	// Calculate the AABB for the scene by iterating through all the meshes in the SDKMesh file.
	for (UINT i = 0;i<pMesh->GetNumMeshes();++i)
	{
		SDKMESH_MESH* mesh = pMesh->GetMesh(i);
		vMeshMin = XMVectorSet(mesh->BoundingBoxCenter.x - mesh->BoundingBoxExtents.x,
			mesh->BoundingBoxCenter.y - mesh->BoundingBoxExtents.y,
			mesh->BoundingBoxCenter.z - mesh->BoundingBoxExtents.z,
			1.0f);

		vMeshMax = XMVectorSet(mesh->BoundingBoxCenter.x + mesh->BoundingBoxExtents.x, mesh->BoundingBoxCenter.y + mesh->BoundingBoxExtents.y,
			mesh->BoundingBoxCenter.z + mesh->BoundingBoxExtents.z, 1.0f);

		m_vSceneAABBMin = XMVectorMin(vMeshMin, m_vSceneAABBMin);
		m_vSceneAABBMax = XMVectorMax(vMeshMax, m_vSceneAABBMax);
	}

	m_pViewerCamera = pViewerCamera;
	m_pLightCamera = pLightCamera;

	if (m_pRenderOrthoShadowVertexShaderBlob == nullptr)
	{
		V_RETURN(CompileShaderFromFile(L"RenderCascadeShadow.hlsl", nullptr, "VSMain", m_cVertexShaderMode, &m_pRenderOrthoShadowVertexShaderBlob));
	}

	V_RETURN(pD3DDevice->CreateVertexShader(m_pRenderOrthoShadowVertexShaderBlob->GetBufferPointer(), m_pRenderOrthoShadowVertexShaderBlob->GetBufferSize(),
		nullptr, &m_pRenderOrthoShadowVertexShader));
	DXUT_SetDebugName(m_pRenderOrthoShadowVertexShader, "RenderCascadeShadow");

	//In order to compile optimal versions of each shaders,compile out of 64 versions of the same file/
	// the if statements are dependent upon these macros.This enables the compiler to optimize out code
	// that can never be reached.
	//D3D11 Dynamic shader linkage would have this same effect without the need to compile 64 versions of the shader.
	D3D_SHADER_MACRO defines[]
	{
		"CASCADE_COUNT_FLAG","1",
		"USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG","0",
		"BLEND_BETWEEN_CASCADE_LAYERS_FLAG","0",
		"SELECT_CASCADE_BY_INTERVAL_FLAG","0",
		nullptr,nullptr
	}; 

	char cCascadeDefinition[32];
	char cDerivativeDefinition[32];
	char cBlendDefinition[32];
	char cIntervalDefinition[32];

	for (INT iCascadeIndex = 0;iCascadeIndex<MAX_CASCADES;++iCascadeIndex)
	{
		//There is just one vertex shader for the scene.
		sprintf_s(cCascadeDefinition, "%d", iCascadeIndex+1);
		defines[0].Definition = cCascadeDefinition;
		defines[1].Definition = "0";
		defines[2].Definition = "0";
		defines[3].Definition = "0";
		//We don't want to release the last pVertexShaderBuffer until we create the input layout.

		if (m_pRenderSceneVertexShaderBlob[iCascadeIndex] == NULL)
		{
			V_RETURN(CompileShaderFromFile(L"RenderCascadeScene.hlsl", defines, "VSMain",
				m_cVertexShaderMode, &m_pRenderSceneVertexShaderBlob[iCascadeIndex]));
		}

		V_RETURN(pD3DDevice->CreateVertexShader(m_pRenderSceneVertexShaderBlob[iCascadeIndex]->GetBufferPointer(),
			m_pRenderSceneVertexShaderBlob[iCascadeIndex]->GetBufferSize(), nullptr, &m_pRenderSceneVertexShader[iCascadeIndex]));
		DXUT_SetDebugName(m_pRenderSceneVertexShader[iCascadeIndex], "RenderCascadeScene");

		for (INT iDerivativeIndex = 0;iDerivativeIndex<2;++iDerivativeIndex)
		{
			for (INT iBlendIndex = 0;iBlendIndex<2;++iBlendIndex)
			{
				for (INT iCascadeFitMode = 0; iCascadeFitMode < 2; iCascadeFitMode++)
				{
					sprintf_s(cCascadeDefinition, "%d", iCascadeIndex + 1);
					sprintf_s(cDerivativeDefinition, "%d", iDerivativeIndex);
					sprintf_s(cBlendDefinition, "%d", iBlendIndex);
					sprintf_s(cIntervalDefinition, "%d", iCascadeFitMode);


					defines[0].Definition = cCascadeDefinition;
					defines[1].Definition = cDerivativeDefinition;
					defines[2].Definition = cBlendDefinition;
					defines[3].Definition = cIntervalDefinition;

					if (m_pRenderSceneAllPixelShaderBlobs[iCascadeIndex][iDerivativeIndex][iBlendIndex][iCascadeFitMode] == nullptr)
					{
						V_RETURN(CompileShaderFromFile(L"RenderCascadeScene.hlsl", defines, "PSMain",
							m_cPixelShaderMode, &m_pRenderSceneAllPixelShaderBlobs[iCascadeIndex][iDerivativeIndex][iBlendIndex][iCascadeFitMode]));
					}

					V_RETURN(pD3DDevice->CreatePixelShader(m_pRenderSceneAllPixelShaderBlobs[iCascadeIndex][iDerivativeIndex][iBlendIndex][iCascadeFitMode]->GetBufferPointer(),
						m_pRenderSceneAllPixelShaderBlobs[iCascadeIndex][iDerivativeIndex][iBlendIndex][iCascadeFitMode]->GetBufferSize(),
						nullptr,
						&m_pRenderSceneAllPixelShaders[iCascadeIndex][iDerivativeIndex][iBlendIndex][iCascadeFitMode]));

					char temp[64];
					sprintf_s(temp, "RenderCascadeScene_%d_%d_%d_%d", iCascadeIndex + 1, iDerivativeIndex, iBlendIndex, iCascadeFitMode);

					DXUT_SetDebugName(m_pRenderSceneAllPixelShaders[iCascadeIndex][iDerivativeIndex][iBlendIndex][iCascadeFitMode], temp);

				}
			}
		}
	}


	const D3D11_INPUT_ELEMENT_DESC layout_mesh[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
	};

	V_RETURN(pD3DDevice->CreateInputLayout(
		layout_mesh, ARRAYSIZE(layout_mesh),
		m_pRenderSceneVertexShaderBlob[0]->GetBufferPointer(),
		m_pRenderSceneVertexShaderBlob[0]->GetBufferSize(),
		&m_pMeshVertexLayout));
	DXUT_SetDebugName(m_pMeshVertexLayout, "CascadeShadowsManagerInputLayout");


	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = FALSE;

	V_RETURN(pD3DDevice->CreateDepthStencilState(&depthStencilDesc, &m_pDepthStencilStateLess));
	DXUT_SetDebugName(m_pDepthStencilStateLess, "DepthStencil LESS");

	D3D11_RASTERIZER_DESC drd;
	drd.FillMode = D3D11_FILL_SOLID;
	drd.CullMode = D3D11_CULL_NONE;
	drd.FrontCounterClockwise = FALSE;
	drd.DepthBias = 0;
	drd.DepthBiasClamp = 0.0f;
	drd.SlopeScaledDepthBias = 0.0f;
	drd.DepthClipEnable = TRUE;
	drd.ScissorEnable = FALSE;
	drd.MultisampleEnable = TRUE;
	drd.AntialiasedLineEnable = FALSE;

	//D3D11_RASTERIZER_DESC drd =
	//{
	//	D3D11_FILL_SOLID,//D3D11_FILL_MODE FillMode;
	//	D3D11_CULL_NONE,//D3D11_CULL_MODE CullMode;
	//	FALSE,//BOOL FrontCounterClockwise;
	//	0,//INT DepthBias;
	//	0.0,//FLOAT DepthBiasClamp;
	//	0.0,//FLOAT SlopeScaledDepthBias;
	//	TRUE,//BOOL DepthClipEnable;
	//	FALSE,//BOOL ScissorEnable;
	//	TRUE,//BOOL MultisampleEnable;
	//	FALSE//BOOL AntialiasedLineEnable;   
	//};

	pD3DDevice->CreateRasterizerState(&drd, &m_pRasterizerStateScene);
	DXUT_SetDebugName(m_pRasterizerStateScene, "CSM Scene");

	//Setting the slope scale depth bias greatly decreases surface acne and incorrect self shadowing.
	drd.SlopeScaledDepthBias = 1.0;
	pD3DDevice->CreateRasterizerState(&drd, &m_pRasterizerStateShadow);
	DXUT_SetDebugName(m_pRasterizerStateShadow, "CSM Shadow");
	drd.DepthClipEnable = false;
	pD3DDevice->CreateRasterizerState(&drd, &m_pRasterizerStateShadowPancake);
	DXUT_SetDebugName(m_pRasterizerStateShadowPancake, "CSM Pancake");

	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;

	Desc.ByteWidth = sizeof(CB_ALL_SHADOW_DATA);
	V_RETURN(pD3DDevice->CreateBuffer(&Desc, NULL, &m_pGlobalConstantBuffer));
	DXUT_SetDebugName(m_pGlobalConstantBuffer, "CB_ALL_SHADOW_DATACB_ALL_SHADOW_DATA");

	return hr;
}

HRESULT CascadedShadowsManager::DestroyAndDeallocateShadowResources()
{
	SAFE_RELEASE(m_pMeshVertexLayout);
	SAFE_RELEASE(m_pRenderOrthoShadowVertexShader);


	SAFE_RELEASE(m_pCascadedShadowMapTexture);
	SAFE_RELEASE(m_pCascadedShadowMapDSV);
	SAFE_RELEASE(m_pCascadedShadowMapSRV);

	SAFE_RELEASE(m_pGlobalConstantBuffer);

	SAFE_RELEASE(m_pDepthStencilStateLess);

	SAFE_RELEASE(m_pRasterizerStateScene);
	SAFE_RELEASE(m_pRasterizerStateShadow);
	SAFE_RELEASE(m_pRasterizerStateShadowPancake);

	SAFE_RELEASE(m_pSamLinear);
	SAFE_RELEASE(m_pSamShadowPoint);
	SAFE_RELEASE(m_pSamShadowPCF);

	for (INT iCascadeIndex = 0;iCascadeIndex<MAX_CASCADES;++iCascadeIndex)
	{
		SAFE_RELEASE(m_pRenderSceneVertexShader[iCascadeIndex]);

		for (INT iDerivativeIndex = 0; iDerivativeIndex<2;++iDerivativeIndex)
		{
			for (INT iBlendIndex = 0;iBlendIndex<2;++iBlendIndex)
			{
				for (INT iIntervalIndex = 0;iIntervalIndex<2;++iIntervalIndex)
				{
					SAFE_RELEASE(m_pRenderSceneAllPixelShaders[iCascadeIndex][iDerivativeIndex][iBlendIndex][iIntervalIndex]);
				}
			}
		}
	}


	return E_NOTIMPL;
}
HRESULT CascadedShadowsManager::InitPerFrame(ID3D11Device * pD3dDevice, CDXUTSDKMesh * mesh)
{

	ReleaseOldAndAllocateNewShadowResources(pD3dDevice);

	// Copy D3DX matrices into XNA Math Math matrices
	DirectX::XMMATRIX ViewerCameraProjection = m_pViewerCamera->GetProjMatrix();
	DirectX::XMMATRIX ViewerCameraView = m_pViewerCamera->GetViewMatrix();
	DirectX::XMMATRIX LightCameraView = m_pLightCamera->GetViewMatrix();

	XMVECTOR det;
	XMMATRIX InverseViewCamera = XMMatrixInverse(&det, ViewerCameraView);


	// Convert from min max representation to center extents representation
	// This will make it easier to pull the points out of the transformation.
	XMVECTOR vSceneCenter = m_vSceneAABBMin + m_vSceneAABBMax;
	vSceneCenter *= g_vHalfVector;
	XMVECTOR vSceneExtends = m_vSceneAABBMax - m_vSceneAABBMin;
	vSceneExtends *= g_vHalfVector;

	XMVECTOR vSceneAABBPointsInLightView[8];
	//This function simply converts the center and extends of an AABB into 8 points
	CreateAABBPoints(vSceneAABBPointsInLightView, vSceneCenter, vSceneExtends);
	//Transform the scene AABB to Light space.
	for (int index = 0;index<8;++index)
	{
		vSceneAABBPointsInLightView[index] = XMVector4Transform(vSceneAABBPointsInLightView[index], LightCameraView);
	}

	FLOAT fFrustumPartitionBeginDepth, fFrustumPartitionEndDepth;
	XMVECTOR vOrthographicMinInLightView; //light space frustum aabb
	XMVECTOR vOrthographicMaxInLightView;
	FLOAT fCameraNearFarRange = m_pViewerCamera->GetFarClip() - m_pViewerCamera->GetNearClip();

	XMVECTOR vViewSpaceUnitsPerTexel = g_vZero;

	// we loop over the cascade to calculate the orthographic projection for each cascade.
	for (INT iCascadeIndex = 0;iCascadeIndex<m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;++iCascadeIndex)
	{
		// Calculate the interval to the View Frustum that this cascade covers.We measure the interval
		// the cascade covers as a Min and Max Distance along the Z Axis
		if (m_eLightViewFrustumFitMode == FIT_LIGHT_VIEW_FRUSTRUM_TO_CASCADE_INTERVALS)
		{
			// Because we want to fit the orthograpic projection tightly around the Cascade,we set the Minimum cascade
			// value to the previous Frustum and Interval
			if (iCascadeIndex == 0)
			{
				fFrustumPartitionBeginDepth = 0.0f;
			}
			else
			{
				fFrustumPartitionBeginDepth = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex - 1];
			}
		}
		else
		{
			// In the FIT_TO_SCENE technique the Cascade overlap each other.
			//In other words,interval 1 is covered by cascades 1 to 8,interval 2 is covered by cascade 2 to 8 and so forth.
			fFrustumPartitionBeginDepth = 0.0f;
		}


		// Scale the intervals between 0 and 1,They are now percentages that we can scale with.
		fFrustumPartitionBeginDepth = fFrustumPartitionBeginDepth/(FLOAT)m_iCascadePartitionMax*fCameraNearFarRange;
		fFrustumPartitionEndDepth = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex] /(FLOAT)m_iCascadePartitionMax*fCameraNearFarRange;

		XMVECTOR vFrustumPointsInCameraView[8];
		XMVECTOR vFrustumPointsInWorld[8];
		XMVECTOR vTempTranslatedCornerPointInLightView;

		//This function takes the begin and end intervals along with the projection matrix and returns the 8 points
		// That represented the cascade Interval
		CreateFrustumPointsFromCascadeInterval(fFrustumPartitionBeginDepth, fFrustumPartitionEndDepth, ViewerCameraProjection, vFrustumPointsInCameraView);

		vOrthographicMinInLightView = g_vFLTMAX;
		vOrthographicMaxInLightView = g_vFLTMIN;

		

		//将视截体的八个点从相机空间变换到光照空间，并求其在光照空间的AABB
		//This next section of code calculates the min and max values for the orthographic projection.
		for (int i = 0;i < 8;++i)
		{
			//Transform the frustum from camera view space to world space.
			vFrustumPointsInWorld[i] = XMVector4Transform(vFrustumPointsInCameraView[i], InverseViewCamera);
			//Transform the point from world space to LightCamera Space.
			vTempTranslatedCornerPointInLightView = XMVector4Transform(vFrustumPointsInWorld[i], LightCameraView);
			// Find the closest point.
			vOrthographicMinInLightView = XMVectorMin(vTempTranslatedCornerPointInLightView, vOrthographicMinInLightView);
			vOrthographicMaxInLightView = XMVectorMax(vTempTranslatedCornerPointInLightView, vOrthographicMaxInLightView);

		}

		//This code removes the shimmering effect along the edges of shadow due to
		// the light changing to fit the camera.
		if (m_eLightViewFrustumFitMode == FIT_LIGHT_VIEW_FRUSTRUM_TO_SCENE)
		{
			// Fit the ortho projection to the cascades far plane and a near plane of zero.
			// Pad the projection to be the size of the diagonal of the Frustum partition.
			
			// To do this, we pad the ortho transform so that it is always big enough to cover
			// the entire camera view frustum.
			XMVECTOR vDiagonal = vFrustumPointsInWorld[0] - vFrustumPointsInWorld[6];//斜对角的两个点
			vDiagonal = XMVector3Length(vDiagonal);

			// The bound is the length of the diagonal of the frustum interval.
			FLOAT fCascadeBound = XMVectorGetX(vDiagonal);

			//The offset calculated will pad the ortho projection so that it is always the same size
			// and big enough to cover the entire cascade interval
			XMVECTOR vBoarderoffset = (vDiagonal - (vOrthographicMaxInLightView - vOrthographicMinInLightView))* g_vHalfVector;

			//Set the Z and W component to zero
			vBoarderoffset *= g_vMultiplySetzwZero;

			//Add the offsets to the projection.
			vOrthographicMaxInLightView += vBoarderoffset;
			vOrthographicMinInLightView -= vBoarderoffset;

			//The world units per texel are used to snap the shadow the orthographic projection
			// to texel sized increments.This keeps the edges of the shadows from shimmering.
			FLOAT fViewSpaceUnitsPerTexel = fCascadeBound / (float)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
			vViewSpaceUnitsPerTexel = XMVectorSet(fViewSpaceUnitsPerTexel, fViewSpaceUnitsPerTexel, 0.0f, 0.0f);

		}
		else if (m_eLightViewFrustumFitMode == FIT_LIGHT_VIEW_FRUSTRUM_TO_CASCADE_INTERVALS)
		{
			// We calculate a looser bound based on the size of the PCF blur. This ensures us that we're
			// Sampling within the correct map.
			float fScaleDuetoBluredAMT = ((float)(m_iPCFBlurSize * 2 + 1) / (float)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare);
			XMVECTORF32 vScaleDueToBluredAMT = { fScaleDuetoBluredAMT, fScaleDuetoBluredAMT, 0.0f, 0.0f };

			float fTexelStepeByBufferSize = ((1.0f) / (float)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare);
			XMVECTOR vTexelStepByBufferSize = XMVectorSet(fTexelStepeByBufferSize, fTexelStepeByBufferSize, 0.0f, 0.0f);

			//We calculate the offsets as a percentage of the bound.
			XMVECTOR vBoarderOffset = vOrthographicMaxInLightView - vOrthographicMinInLightView;
			vBoarderOffset *= g_vHalfVector;
			vBoarderOffset *= vScaleDueToBluredAMT;
			vOrthographicMaxInLightView += vBoarderOffset;
			vOrthographicMinInLightView -= vBoarderOffset;


			// The world units per texel are used to snap the orthographic projection
			// to texel sized increments
			// Because we're fitting tightly to the cascade,the shimmering shadow edges will still be present when
			// the camera rotates.However when zooming in or strafing the shadow edge will not shimmer.
			vViewSpaceUnitsPerTexel = vOrthographicMaxInLightView - vOrthographicMinInLightView;
			vViewSpaceUnitsPerTexel *= vTexelStepByBufferSize;

		}

		float fOrthographicMinZInLightView = XMVectorGetZ(vOrthographicMinInLightView);

		if (m_bMoveLightTexelSize)
		{
			//建立Texel对应ViewSpace的数值映射，当出现移动Camera视角被调整后仍是同个像素对应的映射中，则保持投影矩阵不变，可能避免微弱的相机变化导致阴影出现抖动
			// we snape the camera to 1 pixel increments so that moving the camera does not cause the shadow to jitter(抖动)
			// This is a matter of integer dividing by the world space size of texel
			vOrthographicMinInLightView /= vViewSpaceUnitsPerTexel;
			vOrthographicMinInLightView = XMVectorFloor(vOrthographicMinInLightView);
			vOrthographicMinInLightView *= vViewSpaceUnitsPerTexel;


			vOrthographicMaxInLightView /= vViewSpaceUnitsPerTexel;
			vOrthographicMaxInLightView = XMVectorFloor(vOrthographicMaxInLightView);
			vOrthographicMaxInLightView *= vViewSpaceUnitsPerTexel;

		}

		// These are the unconfigured near and far plane values. They are purposely awful to show
		// how important calculating accurate near and far plane is.
		FLOAT fNearPlaneInLightView = 0.0f;
		FLOAT fFarPlaneInLightView = 10000.0f;

		if (m_eSelectedNearFarFit == FIT_NEAR_FAR_ONLY_SCENE_AABB)
		{
			XMVECTOR vLightSpaceSceneAABBminValue = g_vFLTMAX; //World space scene aabb
			XMVECTOR vLightSpaceSceneAABBmaxValue = g_vFLTMIN;

			// We calculate the min and max vectors of the scene in the light space.The min and max "Z" values of the light space AABB
			// can be used for the near and far plane.This is easier than intersecting the scene with the AABB
			// and in some cases provides similar results
			for (int index = 0;index<8;++index)
			{
				vLightSpaceSceneAABBminValue = XMVectorMin(vSceneAABBPointsInLightView[index], vLightSpaceSceneAABBminValue); 
				vLightSpaceSceneAABBmaxValue = XMVectorMax(vSceneAABBPointsInLightView[index], vLightSpaceSceneAABBmaxValue);
			}

			//The min and max z values are the near and far planes.
			fNearPlaneInLightView = XMVectorGetZ(vLightSpaceSceneAABBminValue);
			fFarPlaneInLightView = XMVectorGetZ(vLightSpaceSceneAABBmaxValue);


		}
		else if(m_eSelectedNearFarFit == FIT_NEAR_FAR_SCENE_AABB_AND_ORTHO_BOUND || m_eSelectedNearFarFit == FIT_NEAR_FAR_PANCAKING)
		{
			//By intersecting the light frustum with the scene AABB we can get a tighter bound on the near and far plane.
			//从场景AABB和正交投影包围盒一起计算出最窄的NearFarPlane
			ComputeNearAndFarInViewSpace(fNearPlaneInLightView, fFarPlaneInLightView, vOrthographicMinInLightView, vOrthographicMaxInLightView, vSceneAABBPointsInLightView);

			if (m_eSelectedNearFarFit == FIT_NEAR_FAR_PANCAKING)
			{
				if (fOrthographicMinZInLightView > fNearPlaneInLightView)
				{
					fNearPlaneInLightView = fOrthographicMinZInLightView;
				}
			}
		}
		else
		{
			//todo
		}

		//Create the orthographic projection for this cacscade.
		m_matOrthoProjForCascades[iCascadeIndex] = DirectX::XMMatrixOrthographicOffCenterLH(
			XMVectorGetX(vOrthographicMinInLightView), XMVectorGetX(vOrthographicMaxInLightView),
			XMVectorGetY(vOrthographicMinInLightView), XMVectorGetY(vOrthographicMaxInLightView),
			fNearPlaneInLightView, fFarPlaneInLightView);

		m_fCascadePartitionDepthsInEyeSpace[iCascadeIndex] = fFrustumPartitionEndDepth;

	}

	m_matShadowView = m_pLightCamera->GetViewMatrix();

	return S_OK;
}


// Render the cascades into a texture atlas.
HRESULT CascadedShadowsManager::RenderShadowForAllCascades(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dDeviceContext, CDXUTSDKMesh * pMesh)
{
	HRESULT hr = S_OK;

	XMMATRIX ViewProjection;


	pD3dDeviceContext->ClearDepthStencilView(m_pCascadedShadowMapDSV, D3D11_CLEAR_DEPTH, 1.0, 0);
	ID3D11RenderTargetView* pNullView = nullptr;

	//Set a null render target so as not to render color.
	pD3dDeviceContext->OMSetRenderTargets(1, &pNullView, m_pCascadedShadowMapDSV);

	if (m_eSelectedNearFarFit == FIT_NEAR_FAR_PANCAKING)
	{
		pD3dDeviceContext->RSSetState(m_pRasterizerStateShadowPancake);
	}
	else
	{
		pD3dDeviceContext->RSSetState(m_pRasterizerStateShadow);
	}

	pD3dDeviceContext->OMSetDepthStencilState(m_pDepthStencilStateLess, 1);

	//Iterate over cascades and render shadows;
	for (INT currentCascade = 0;currentCascade<m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;++currentCascade)
	{
		// Each cascade has its own viewport because we're storing all the cascades in on large texture
		pD3dDeviceContext->RSSetViewports(1, &m_RenderViewPort[currentCascade]);

		//原模型就是世界坐标系
		// We calculate the matrices in th Init function.
		ViewProjection = m_matShadowView* m_matOrthoProjForCascades[currentCascade];


		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V(pD3dDeviceContext->Map(m_pGlobalConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_ALL_SHADOW_DATA* pcbAllShadowConstants = (CB_ALL_SHADOW_DATA*)MappedResource.pData;

		pcbAllShadowConstants->m_WorldViewProj = DirectX::XMMatrixTranspose(ViewProjection);//混用ConstantBuffer

		//XMMATRIX Identity = XMMatrixIdentity();
		
		//The model was exported in world space ,so we can pass the identity up as the world transform.
		pcbAllShadowConstants->m_World = DirectX::XMMatrixIdentity();

		pD3dDeviceContext->Unmap(m_pGlobalConstantBuffer, 0);
		pD3dDeviceContext->IASetInputLayout(m_pMeshVertexLayout);


		//No pixel shader is bound as we're only writing out depth.
		pD3dDeviceContext->VSSetShader(m_pRenderOrthoShadowVertexShader, nullptr, 0);
		pD3dDeviceContext->PSSetShader(nullptr, nullptr, 0);
		pD3dDeviceContext->GSSetShader(nullptr, nullptr, 0);

		pD3dDeviceContext->VSSetConstantBuffers(0, 1, &m_pGlobalConstantBuffer);

		pMesh->Render(pD3dDeviceContext, 0, 1);
	}

	pD3dDeviceContext->RSSetState(nullptr);

	pD3dDeviceContext->OMSetRenderTargets(1, &pNullView, nullptr);

	return hr;
}
HRESULT CascadedShadowsManager::RenderScene(ID3D11DeviceContext * pD3dDeviceContext, ID3D11RenderTargetView * pRenderTargetView, ID3D11DepthStencilView * pDepthStencilView,
	CDXUTSDKMesh * pMesh, CFirstPersonCamera * pActiveCamera, D3D11_VIEWPORT * pViewPort, BOOL bVisualize)
{
	HRESULT hr = S_OK;

	D3D11_MAPPED_SUBRESOURCE MappedResource;

	//we have a separate render state for the actual rasterization because of different depth biases and Cull modes.
	pD3dDeviceContext->RSSetState(m_pRasterizerStateScene);

	pD3dDeviceContext->OMSetRenderTargets(1, &pRenderTargetView, pDepthStencilView);
	pD3dDeviceContext->RSSetViewports(1, pViewPort);
	pD3dDeviceContext->IASetInputLayout(m_pMeshVertexLayout);


	XMMATRIX CameraProj = pActiveCamera->GetProjMatrix();
	XMMATRIX CameraView = pActiveCamera->GetViewMatrix();

	//The user has the option to view the ortho shadow cameras.
	if (m_eSelectedCamera >= ORTHO_CAMERA1)
	{
		// In the CAMERA_SELECTION enumeration, value 0 is EYE_CAMERA
		// value 1 is LIGHT_CAMERA and 2 to 10 are the ORTHO_CAMERA values.
		// Subtract to so that we can use the enum to index
		CameraProj = m_matOrthoProjForCascades[(int)m_eSelectedCamera - 2];
		CameraView = m_matShadowView;
	}

	XMMATRIX WorldViewProjection = CameraView*CameraProj;//jingz原模型已经使用世界坐标系导出

	V(pD3dDeviceContext->Map(m_pGlobalConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));

	CB_ALL_SHADOW_DATA* pcbAllShadowConstants = (CB_ALL_SHADOW_DATA*)MappedResource.pData;

	pcbAllShadowConstants->m_WorldViewProj = XMMatrixTranspose(WorldViewProjection);
	pcbAllShadowConstants->m_WorldView = XMMatrixTranspose(CameraView);

	//There are the for loop begin end values.
	pcbAllShadowConstants->m_iPCFBlurForLoopEnd = m_iPCFBlurSize / 2 + 1;
	pcbAllShadowConstants->m_iPCFBlurForLoopStart = m_iPCFBlurSize / -2;

	//This is a floating point number that is used as percentage to blur between maps
	pcbAllShadowConstants->m_fMaxBlendRatioBetweenCascadeLevel = m_fMaxBlendRatioBetweenCascadeLevel;
	pcbAllShadowConstants->m_fLogicStepPerTexel = 1.0f / (float)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
	pcbAllShadowConstants->m_fNativeCascadedShadowMapTexelStepInX = pcbAllShadowConstants->m_fLogicStepPerTexel / m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;
	pcbAllShadowConstants->m_World = XMMatrixIdentity();

	XMMATRIX TextureScale = XMMatrixScaling(0.5f, -0.5f, 1.0f);
	XMMATRIX TextureTranslation = XMMatrixTranslation(0.5f,0.5f,0.0f);

	pcbAllShadowConstants->m_fPCFShadowDepthBiaFromGUI = m_fPCFShadowDepthBia;
	pcbAllShadowConstants->m_fWidthPerShadowTextureLevel_InU = 1.0f / (float)m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;

	pcbAllShadowConstants->m_ShadowView = XMMatrixTranspose(m_matShadowView);

	for (int index = 0;index<m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;++index)
	{
		//jingz TextureScale/TextureTranslation 为NDC到像素空间转化。y取反，xy/2+0.5
		XMMATRIX mShadowProjToTextureCoord = m_matOrthoProjForCascades[index] * TextureScale*TextureTranslation;
		XMFLOAT4X4 ShadowProjToTextureCoord;
		DirectX::XMStoreFloat4x4(&ShadowProjToTextureCoord, mShadowProjToTextureCoord);
		
		//jingz 从shadowView经过proj/ 转化为texture坐标，旋转朝向信息是没用的。
		pcbAllShadowConstants->m_vScaleFactorFromOrthoProjToTexureCoord[index] = XMVectorSet(ShadowProjToTextureCoord._11, ShadowProjToTextureCoord._22, ShadowProjToTextureCoord._33, 1);
		pcbAllShadowConstants->m_vOffsetFactorFromOrthoProjToTexureCoord[index] = XMVectorSet(ShadowProjToTextureCoord._41, ShadowProjToTextureCoord._42, ShadowProjToTextureCoord._43, 0);
	
	}

	//Copy intervals for the depth interval selection method.
	memcpy(pcbAllShadowConstants->m_fCascadePartitionDepthsInEyeSpace_InFloat4, m_fCascadePartitionDepthsInEyeSpace, MAX_CASCADES * 4);

	pcbAllShadowConstants->m_fCascadePartitionDepthsInEyeSpace_OnlyX[0].x = 0.0f;
	for (int index = 0; index < MAX_CASCADES; ++index)
	{
		pcbAllShadowConstants->m_fCascadePartitionDepthsInEyeSpace_OnlyX[index+1].x = m_fCascadePartitionDepthsInEyeSpace[index];
	}

	//The border padding values keep the pixel shader from reading the borders during PCF filtering.
	pcbAllShadowConstants->m_fMaxBorderPaddingInShadowUV = (float)(m_pCascadeConfig->m_iLengthOfShadowBufferSquare - 1.0f) / (float)m_pCascadeConfig->m_iLengthOfShadowBufferSquare;
	pcbAllShadowConstants->m_fMinBorderPaddingInShadowUV = (float)(1.0f) / (float)m_pCascadeConfig->m_iLengthOfShadowBufferSquare;

	XMFLOAT3 ep;
	XMStoreFloat3(&ep, XMVector3Normalize(m_pLightCamera->GetEyePt() - m_pLightCamera->GetLookAtPt()));
	
	pcbAllShadowConstants->m_vLightDir = XMVectorSet(ep.x, ep.y, ep.z, 1.0f);
	pcbAllShadowConstants->m_nCascadeLeves = m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;
	pcbAllShadowConstants->m_iIsVisualizeCascade = bVisualize?1:0;//jingz todo
	pD3dDeviceContext->Unmap(m_pGlobalConstantBuffer, 0);

	pD3dDeviceContext->PSSetSamplers(0, 1, &m_pSamLinear);
	pD3dDeviceContext->PSSetSamplers(1, 1, &m_pSamLinear);

	pD3dDeviceContext->PSSetSamplers(5, 1, &m_pSamShadowPCF);
	pD3dDeviceContext->GSSetShader(nullptr, nullptr, 0);

	pD3dDeviceContext->VSSetShader(m_pRenderSceneVertexShader[m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount - 1], nullptr, 0);

	//There are up to 8 cascades,possible derivative based offsets,blur between cascades,
	// and two cascade selection maps. This is total of 64permutations of the shader.

	int indexBlurShader = m_bIsBlurBetweenCascades ? 1 : 0;

	pD3dDeviceContext->PSSetShader(m_pRenderSceneAllPixelShaders[m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount - 1][m_bIsDerivativeBaseOffset?1:0][indexBlurShader][m_eSelectedCascadeMode],
		nullptr, 0);


	pD3dDeviceContext->PSSetShaderResources(5, 1, &m_pCascadedShadowMapSRV);

	pD3dDeviceContext->VSSetConstantBuffers(0, 1, &m_pGlobalConstantBuffer);
	pD3dDeviceContext->PSSetConstantBuffers(0, 1, &m_pGlobalConstantBuffer);

	pMesh->Render(pD3dDeviceContext, 0, 1);

	ID3D11ShaderResourceView* nv[] = { nullptr,nullptr, nullptr, nullptr, nullptr, nullptr,nullptr, nullptr };
	pD3dDeviceContext->PSSetShaderResources(5, 8, nv);

	return hr;
}

struct Triangle
{
	XMVECTOR points[3];
	BOOL culled;
};

enum FRUSTUM_PLANE_FLAG
{
	FRUSTUM_PLANE_LEFT = 0,
	FRUSTUM_PLANE_RIGHT,
	FRUSTUM_PLANE_BOTTOM,
	FRUSTUM_PLANE_UP,
	FRUSTUM_PLANE_COUNT,
};


// Computing an accurate near and far plane will decrease surface ance and Peter-panning
// Surface acne is the term for erroneous self shadowing.Peter-panning is the effect where
// shadows disappear near the base of an object.
// As offsets are generally used with PCF filtering due self shadowing issues,computing the
// This concept is not complicated,but the intersection code is
void CascadedShadowsManager::ComputeNearAndFarInViewSpace(FLOAT & fNearPlane, FLOAT & fFarPlane, DirectX::FXMVECTOR vOrthographicMin, DirectX::FXMVECTOR vOrthographicMax, DirectX::XMVECTOR * pPointInView)
{
	// Initialize the near and far plane
	fNearPlane = FLT_MAX;
	fFarPlane = -FLT_MAX;

	Triangle triangleList[16];//jingz 每个frustum和三角形做面线裁减，至多可能生成多少个三角形？？
	INT iTriangleCount = 1;

	triangleList[0].points[0] = pPointInView[0];
	triangleList[0].points[1] = pPointInView[1];
	triangleList[0].points[2] = pPointInView[2];
	triangleList[0].culled = false;

	// These are the indices uesed to tesselate an AABB into a list of triangles.
	static const INT iPointIndices[] =
	{
		0,1,2,1,2,3,
		4,5,6,5,6,7,
		0,2,4,2,4,6,
		1,3,5,3,5,7,
		0,1,4,1,4,5,
		2,3,6,3,6,7
	};

	bool bPointPassesCollision[3];

	// At a high level
	// 1. Iterate over all 6*2 triangles of the AABB
	// 2. Clip the triangles against each plane,Create new triangle as needed
	// 3. Find the min and max z values as the near and far plane.
	
	//This is easier because the triangles are in camera spacing making the collisions tests sample comparisions.

	float fLightCameraOrthographicMinX = XMVectorGetX(vOrthographicMin);
	float fLightCameraOrthographicMaxX = XMVectorGetX(vOrthographicMax);
	float fLightCameraOrthographicMinY = XMVectorGetY(vOrthographicMin);
	float fLightCameraOrthographicMaxY = XMVectorGetY(vOrthographicMax);

	for (INT iTriangleIndex = 0;iTriangleIndex<12;++iTriangleIndex)
	{
		triangleList[0].points[0] = pPointInView[iPointIndices[iTriangleIndex * 3 + 0]];
		triangleList[0].points[1] = pPointInView[iPointIndices[iTriangleIndex * 3 + 1]];
		triangleList[0].points[2] = pPointInView[iPointIndices[iTriangleIndex * 3 + 2]];

		iTriangleCount = 1;
		triangleList[0].culled = FALSE;

		//Clip each individual triangle against the 4 frustums.When ever a triangle is clipped into new triangles,
		// add them to the list.
		for (INT frustumPlaneFlag = FRUSTUM_PLANE_LEFT;frustumPlaneFlag<FRUSTUM_PLANE_COUNT;++frustumPlaneFlag)
		{
			FLOAT fEdge;
			INT iComponent;
			if (frustumPlaneFlag == FRUSTUM_PLANE_LEFT)
			{
				fEdge = fLightCameraOrthographicMinX;//Left
				iComponent = 0;
			}
			else if (frustumPlaneFlag == FRUSTUM_PLANE_RIGHT)
			{
				fEdge = fLightCameraOrthographicMaxX;//Right
				iComponent = 0;
			}
			else if (frustumPlaneFlag == FRUSTUM_PLANE_BOTTOM)
			{
				fEdge = fLightCameraOrthographicMinY;//Bottom
				iComponent = 1;
			}
			else//FRUSTUM_PLANE_UP
			{
				fEdge = fLightCameraOrthographicMaxY;//Top
				iComponent = 1;
			}

			for (INT triIter = 0;triIter < iTriangleCount;++triIter)
			{
				//pass 意思是insided
				// we don't delete triangles,so we skip those that have been culled.
				if (!triangleList[triIter].culled)
				{
					INT iInsideVertCount = 0;
					XMVECTOR tempPoint;

					//Test against the correct frustum plane.
					// This could be written more compactly,but it would be harder to understand

					//jingz 1代表在顶点在frusturm内

					if (frustumPlaneFlag == FRUSTUM_PLANE_LEFT)//左平面
					{
						for (INT i = 0;i<3;++i)
						{
							//todo jingz 1和0 分别代表什么意义？？

							if (XMVectorGetX(triangleList[triIter].points[i]) > XMVectorGetX(vOrthographicMin))
							{
								bPointPassesCollision[i] = true;
							}
							else
							{
								bPointPassesCollision[i] = false;
							}
							
							iInsideVertCount += bPointPassesCollision[i] ? 1 : 0;
						}
					}
					else if (frustumPlaneFlag == FRUSTUM_PLANE_RIGHT)//右平面
					{
						for (INT i = 0;i<3;++i)
						{
							if (XMVectorGetX(triangleList[triIter].points[i]) < XMVectorGetX(vOrthographicMax))
							{
								bPointPassesCollision[i] = true;
							}
							else
							{
								bPointPassesCollision[i] = false;
							}

							iInsideVertCount += bPointPassesCollision[i]?1:0;
						}
					}
					else if (frustumPlaneFlag == FRUSTUM_PLANE_BOTTOM)//jingz 下平面
					{
						for (INT i = 0; i < 3; ++i)
						{
							if (XMVectorGetY(triangleList[triIter].points[i]) > XMVectorGetY(vOrthographicMin))
							{
								bPointPassesCollision[i] = true;
							}
							else
							{
								bPointPassesCollision[i] = false;
							}

							iInsideVertCount += bPointPassesCollision[i] ? 1 : 0;
						}
					}
					else // FRUSTUM_PLANE_UP 上平面
					{
						for (INT i = 0; i < 3; ++i)
						{
							if (XMVectorGetY(triangleList[triIter].points[i]) < XMVectorGetY(vOrthographicMax))
							{
								bPointPassesCollision[i] = true;
							}
							else
							{
								bPointPassesCollision[i] = false;
							}

							iInsideVertCount += bPointPassesCollision[i] ? 1 : 0;
						}
					}

					//jingz 两个点确认是否被frustum截断,但是为什么要切换他们位置
					//move the points that pass the frustum test to the beginning of the array.
					if (bPointPassesCollision[1]&& !bPointPassesCollision[0])
					{
						tempPoint = triangleList[triIter].points[0];
						triangleList[triIter].points[0] = triangleList[triIter].points[1];
						triangleList[triIter].points[1] = tempPoint;

						bPointPassesCollision[0] = true;
						bPointPassesCollision[1] = false;

					}
					if (bPointPassesCollision[2] && !bPointPassesCollision[1])
					{
						tempPoint = triangleList[triIter].points[1];
						triangleList[triIter].points[1] = triangleList[triIter].points[2];
						triangleList[triIter].points[2] = tempPoint;
						bPointPassesCollision[1] = true;
						bPointPassesCollision[2] = false;
					}
					if (bPointPassesCollision[1]&&!bPointPassesCollision[0])//1不在，2在，经过上一轮交换测试后要重新测试1和0的关系，有点像堆冒泡排序
					{
						tempPoint = triangleList[triIter].points[0];
						triangleList[triIter].points[0] = triangleList[triIter].points[1];
						triangleList[triIter].points[1] = tempPoint;

						bPointPassesCollision[0] = true;
						bPointPassesCollision[1] = false;
					}

					if (iInsideVertCount == 0)
					{
						//All points failed.We're done
						triangleList[triIter].culled = true;
					}
					else if(iInsideVertCount == 1)//jingz 两个点在截头体外
					{//One point passed.Clip the triangle against the frustum plane
						triangleList[triIter].culled = false;

						//
						XMVECTOR vVert0ToVert1 = triangleList[triIter].points[1] - triangleList[triIter].points[0];
						XMVECTOR vVert0ToVert2 = triangleList[triIter].points[2] - triangleList[triIter].points[0];

						// Find the collision ratio
						FLOAT fHitPointDiff = fEdge - XMVectorGetByIndex(triangleList[triIter].points[0],iComponent);

						//Calculate the distance along the vector as ratio of the hit ratio to the component
						FLOAT fRatioAlongVert01 = fHitPointDiff / XMVectorGetByIndex(vVert0ToVert1, iComponent);// t1
						FLOAT fRatioAlongVert02 = fHitPointDiff / XMVectorGetByIndex(vVert0ToVert2, iComponent);// t2

						//jingz todo 为什么要交换，如何保证环绕方向的正确性
						//Add the point plus a percentage of the vector
						triangleList[triIter].points[1] = triangleList[triIter].points[0] + vVert0ToVert2 * fRatioAlongVert02;
						triangleList[triIter].points[2] = triangleList[triIter].points[0] + vVert0ToVert1 * fRatioAlongVert01;
					}
					else if(iInsideVertCount == 2)//jingz 由一个点截取出两个点，变成四边形，即两个面
					{
						// 2 in 
						// tessellate into 2 triangles

						//Copy the triangle(if is exists) after the current triangle out of
						// the way so we can override it with the new triangle we're inserting.
						triangleList[iTriangleCount] = triangleList[triIter + 1];//jingz 把原先的下一个拓展点移动到最后面，腾空位置给新生成的点

						triangleList[triIter].culled = false;
						triangleList[triIter + 1].culled = false;

						// Get the vector from the outside point into 2 inside points
						XMVECTOR vVert2ToVert0 = triangleList[triIter].points[0] - triangleList[triIter].points[2];
						XMVECTOR vVert2ToVert1 = triangleList[triIter].points[1] - triangleList[triIter].points[2];

						// Get the hit point ratio
						FLOAT fHitPointDiff = fEdge - XMVectorGetByIndex(triangleList[triIter].points[2], iComponent);
						FLOAT fRatioAlongVector_2_0 = fHitPointDiff / XMVectorGetByIndex(vVert2ToVert0, iComponent);
						//Calculate the new vertex by adding the percentage of the vector plus point 2
						vVert2ToVert0 *= fRatioAlongVector_2_0;
						vVert2ToVert0 += triangleList[triIter].points[2];
						
						// Add new triangle.
						triangleList[triIter + 1].points[0] = triangleList[triIter].points[0];
						triangleList[triIter + 1].points[1] = triangleList[triIter].points[1];
						triangleList[triIter + 1].points[2] = vVert2ToVert0;


						// Get the hit point ratio
						FLOAT fRatioAlongVector_2_1 = fHitPointDiff / XMVectorGetByIndex(vVert2ToVert1, iComponent);
						//Calculate the new vertex by adding the percentage of the vector plus point 2
						vVert2ToVert1 *= fRatioAlongVector_2_1;
						vVert2ToVert1 += triangleList[triIter].points[2];

						triangleList[triIter].points[0] = triangleList[triIter+1].points[1];
						triangleList[triIter].points[1] = triangleList[triIter+1].points[2];
						triangleList[triIter].points[2] = vVert2ToVert1;

						//increase triangle count and skip the triangle we just inserted.

						++iTriangleCount;
						++triIter;
					}
					else
					{
						//all in
						triangleList[triIter].culled = false;
					}


				}//if triangle not culled
			}//for_triangle
		}//for_frustumPlane

		for (INT i = 0;i<iTriangleCount;++i)
		{
			if (!triangleList[i].culled)
			{
				//Set the near and far plane and the min and max z values respectively
				for (INT j = 0;j<3;++j)
				{
					float fTriangleCoordZ = XMVectorGetZ(triangleList[i].points[j]);

					if (fNearPlane > fTriangleCoordZ)
					{
						fNearPlane = fTriangleCoordZ;
					}

					if (fFarPlane < fTriangleCoordZ)
					{
						fFarPlane = fTriangleCoordZ;
					}

				}
			}
		}

	}//for_AABB

}
void CascadedShadowsManager::CreateFrustumPointsFromCascadeInterval(FLOAT fCascadeIntervalBegin, FLOAT fCascadeIntervalEnd, DirectX::XMMATRIX & vProjection, DirectX::XMVECTOR * pCornerPointsInView)
{
	XNA::Frustum vViewFrustum;
	XNA::ComputeFrustumFromProjection(&vViewFrustum, &vProjection);

	vViewFrustum.Near = fCascadeIntervalBegin;
	vViewFrustum.Far = fCascadeIntervalEnd;


	static const XMVECTORU32 vGrabY = { 0x00000000,0xFFFFFFFF,0x00000000,0x00000000 };
	static const XMVECTORU32 vGrabX = { 0xFFFFFFFF,0x00000000,0x00000000,0x00000000 };

	XMVECTORF32 vRightTopSlope = { vViewFrustum.RightSlope,vViewFrustum.TopSlope,1.0f,1.0f };
	XMVECTORF32 vLeftBottomSlope = { vViewFrustum.LeftSlope,vViewFrustum.BottomSlope,1.0f,1.0f };
	XMVECTORF32 vNearFactor = { vViewFrustum.Near,vViewFrustum.Near,vViewFrustum.Near,1.0f };
	XMVECTORF32 vFarFactor = { vViewFrustum.Far,vViewFrustum.Far,vViewFrustum.Far,1.0f };
	XMVECTOR vRightTopNear = DirectX::XMVectorMultiply(vRightTopSlope, vNearFactor);
	XMVECTOR vRightTopFar = DirectX::XMVectorMultiply(vRightTopSlope, vFarFactor);
	XMVECTOR vLeftBottomNear = DirectX::XMVectorMultiply(vLeftBottomSlope, vNearFactor);
	XMVECTOR vLeftBottomFar = DirectX::XMVectorMultiply(vLeftBottomSlope, vFarFactor);

	pCornerPointsInView[0] = vRightTopNear;
	pCornerPointsInView[1] = DirectX::XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabX);//RightBottomNear
	pCornerPointsInView[2] = vLeftBottomNear;
	pCornerPointsInView[3] = DirectX::XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabY);//LeftTopNear

	pCornerPointsInView[4] = vRightTopFar;
	pCornerPointsInView[5] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabX);//RightBottomFar
	pCornerPointsInView[6] = vLeftBottomFar;
	pCornerPointsInView[7] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabY);//LeftTopFar

}

void CascadedShadowsManager::CreateAABBPoints(DirectX::XMVECTOR * vAABBPoints, DirectX::XMVECTOR vCenter, DirectX::FXMVECTOR vExtends)
{
	//This map enables us to use a for loop and do vector math.
	static const XMVECTORF32 vExtentsMap[]=
	{
		{ 1.0f,1.0f,-1.0f,1.0f },//右上前
		{ -1.0f,1.0f,-1.0f,1.0f },//左上前
		{ 1.0f,-1.0f,-1.0f,1.0f },//右下前
		{ -1.0f,-1.0f,-1.0f,1.0f },//左下前
		{ 1.0f,1.0f,1.0f,1.0f },//右上后
		{ -1.0f,1.0f,1.0f,1.0f },//左上后
		{ 1.0f,-1.0f,1.0f,1.0f },//右下后
		{ -1.0f,-1.0f,1.0f,1.0f }//左下后
	};

	for (INT i = 0;i <8;++i)
	{
		vAABBPoints[i] = XMVectorMultiplyAdd(vExtentsMap[i], vExtends, vCenter);
	}

}
HRESULT CascadedShadowsManager::ReleaseOldAndAllocateNewShadowResources(ID3D11Device * pD3dDevice)
{
	HRESULT hr = S_OK;

	//if any of the these 3 parameters was changed ,we must reallocate the D3D resources.
	if (m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount != m_pCascadeConfig->m_nUsingCascadeLevelsCount
		|| m_CopyOfCascadeConfig.m_ShadowBufferFormat != m_pCascadeConfig->m_ShadowBufferFormat
		|| m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare != m_pCascadeConfig->m_iLengthOfShadowBufferSquare)
	{
		m_CopyOfCascadeConfig = *m_pCascadeConfig;

		SAFE_RELEASE(m_pSamLinear);
		SAFE_RELEASE(m_pSamShadowPCF);
		SAFE_RELEASE(m_pSamShadowPoint);

		D3D11_SAMPLER_DESC SamDesc;
		SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		SamDesc.AddressU = SamDesc.AddressV  = SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamDesc.MipLODBias = 0.0f;
		SamDesc.MaxAnisotropy = 1;
		SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
		SamDesc.MinLOD = 0;
		SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamDesc, &m_pSamLinear));
		DXUT_SetDebugName(m_pSamLinear, "CSM Linear");

		D3D11_SAMPLER_DESC SamDescShadow;
		SamDescShadow.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		SamDescShadow.AddressU = SamDescShadow.AddressV = SamDescShadow.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		SamDescShadow.MipLODBias = 0.0f;
		SamDescShadow.MaxAnisotropy = 0;
		SamDescShadow.ComparisonFunc = D3D11_COMPARISON_LESS;
		SamDescShadow.BorderColor[0] = SamDescShadow.BorderColor[1] = SamDescShadow.BorderColor[2] = SamDescShadow.BorderColor[3] = 0.0f;
		SamDescShadow.MinLOD = 0;
		SamDescShadow.MaxLOD = 0;
		
		V_RETURN(pD3dDevice->CreateSamplerState(&SamDescShadow, &m_pSamShadowPCF));
		DXUT_SetDebugName(m_pSamShadowPCF, "CSM Shader PCF");

		SamDescShadow.MaxAnisotropy = 15;
		SamDescShadow.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamDescShadow.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamDescShadow.Filter = D3D11_FILTER_ANISOTROPIC;
		SamDescShadow.ComparisonFunc = D3D11_COMPARISON_NEVER;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamDescShadow, &m_pSamShadowPoint));
		DXUT_SetDebugName(m_pSamShadowPoint, "CSM Shadow Point");

		for (INT i = 0;i<m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;++i)
		{
			m_RenderViewPort[i].Height = (FLOAT)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
			m_RenderViewPort[i].Width = (FLOAT)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
			m_RenderViewPort[i].MaxDepth = 1.0f;
			m_RenderViewPort[i].MinDepth = 0.0f;
			m_RenderViewPort[i].TopLeftX = (FLOAT)(m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare*i);
			m_RenderViewPort[i].TopLeftY = 0;

		}

		m_RenderOneTileVP.Height = (FLOAT)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
		m_RenderOneTileVP.Width = (FLOAT)m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
		m_RenderOneTileVP.MaxDepth = 1.0f;
		m_RenderOneTileVP.MinDepth = 0.0f;
		m_RenderOneTileVP.TopLeftX = 0.0f;
		m_RenderOneTileVP.TopLeftY = 0.0f;

		SAFE_RELEASE(m_pCascadedShadowMapSRV);
		SAFE_RELEASE(m_pCascadedShadowMapTexture);
		SAFE_RELEASE(m_pCascadedShadowMapDSV);

		DXGI_FORMAT textureFormat = DXGI_FORMAT_R32_TYPELESS;
		DXGI_FORMAT SrvFormat = DXGI_FORMAT_R32_FLOAT;
		DXGI_FORMAT DsvFormat = DXGI_FORMAT_D32_FLOAT;

		switch (m_CopyOfCascadeConfig.m_ShadowBufferFormat)
		{
		case CASCADE_DXGI_FORMAT_R32_TYPELESS:
			textureFormat = DXGI_FORMAT_R32_TYPELESS;
			SrvFormat = DXGI_FORMAT_R32_FLOAT;
			DsvFormat = DXGI_FORMAT_D32_FLOAT;
			break;
		
		case CASCADE_DXGI_FORMAT_R24G8_TYPELESS:
			textureFormat = DXGI_FORMAT_R24G8_TYPELESS;
			SrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			DsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			break;
		
		case CASCADE_DXGI_FORMAT_R16_TYPELESS:
			textureFormat = DXGI_FORMAT_R16_TYPELESS;
			SrvFormat = DXGI_FORMAT_R16_UNORM;
			DsvFormat = DXGI_FORMAT_D16_UNORM;
			break;

		case CASCADE_DXGI_FORMAT_R8_TYPELESS:
			textureFormat = DXGI_FORMAT_R8_TYPELESS;
			SrvFormat = DXGI_FORMAT_R8_UNORM;
			DsvFormat = DXGI_FORMAT_R8_UNORM;
			break;
		}


		D3D11_TEXTURE2D_DESC ShadowMapTextureDesc;
		ShadowMapTextureDesc.Width = m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare*m_CopyOfCascadeConfig.m_nUsingCascadeLevelsCount;
		ShadowMapTextureDesc.Height = m_CopyOfCascadeConfig.m_iLengthOfShadowBufferSquare;
		ShadowMapTextureDesc.MipLevels = 1;
		ShadowMapTextureDesc.ArraySize = 1;
		ShadowMapTextureDesc.Format = textureFormat;
		ShadowMapTextureDesc.SampleDesc.Count = 1;
		ShadowMapTextureDesc.SampleDesc.Quality = 0;
		ShadowMapTextureDesc.Usage = D3D11_USAGE_DEFAULT;
		ShadowMapTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		ShadowMapTextureDesc.CPUAccessFlags = 0;
		ShadowMapTextureDesc.MiscFlags = 0;

		V_RETURN(pD3dDevice->CreateTexture2D(&ShadowMapTextureDesc, NULL, &m_pCascadedShadowMapTexture));
		DXUT_SetDebugName(m_pCascadedShadowMapTexture, "CSM Texture2D");



		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
		depthStencilViewDesc.Format = DsvFormat;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Flags = 0;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		V_RETURN(pD3dDevice->CreateDepthStencilView(m_pCascadedShadowMapTexture, &depthStencilViewDesc, &m_pCascadedShadowMapDSV));
		DXUT_SetDebugName(m_pCascadedShadowMapDSV,"CSM Texture2D DSV");

		D3D11_SHADER_RESOURCE_VIEW_DESC depthStencilSrvDesc;
		depthStencilSrvDesc.Format = SrvFormat;
		depthStencilSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		depthStencilSrvDesc.Texture2D.MostDetailedMip = 0;
		depthStencilSrvDesc.Texture2D.MipLevels = 1;

		V_RETURN(pD3dDevice->CreateShaderResourceView(m_pCascadedShadowMapTexture, &depthStencilSrvDesc, &m_pCascadedShadowMapSRV));

		DXUT_SetDebugName(m_pCascadedShadowMapSRV, "CSM Texture2D SRV");

	}


	return hr;
}
;


