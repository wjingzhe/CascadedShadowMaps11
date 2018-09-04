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
	m_iBlurBetweenCascades(0),
	m_fBlurBetweenCascadesAmount(0.005f),
	m_RenderOneTileVP(m_RenderVP[0]),
	m_pcbGlobalConstantBuffer(nullptr),
	m_prsScene(nullptr),
	m_prsShadow(nullptr),
	m_prsShadowPancake(nullptr),
	m_iPCFBlurSize(3),
	m_fPCFOffset(0.002f),
	m_iDerivativeBaseOffset(0),
	m_pvsRenderOrthoShadowBlob(nullptr)
{
	sprintf_s(m_cvsMode, "vs_5_0");
	sprintf_s(m_cpsMode, "ps_5_0");
	sprintf_s(m_cgsMode, "gs_5_0");


	for (INT index = 0;index < MAX_CASCADES;++index)
	{
		m_RenderVP[index].Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
		m_RenderVP[index].Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
		m_RenderVP[index].MaxDepth = 1.0f;
		m_RenderVP[index].MinDepth = 0.0f;
		m_RenderVP[index].TopLeftX = 0;
		m_RenderVP[index].TopLeftY = 0;
		m_pvsRenderSceneBlob[index] = nullptr;

		for (int x1 = 0;x1<2;++x1)
		{
			for (int x2 = 0;x2<2;++x2)
			{
				for (int x3 = 0; x3 < 2; ++x3)
				{
					m_ppsRenderSceneAllShadersBlob[index][x1][x2][x3] = nullptr;
				}
			}
		}

	}//for


}
CascadedShadowsManager::~CascadedShadowsManager()
{
	DestroyAndDeallocateShadowResources();
	SAFE_RELEASE(m_pvsRenderOrthoShadowBlob);

	for (int i = 0;i<MAX_CASCADES;++i)
	{
		SAFE_RELEASE(m_pvsRenderSceneBlob[i]);


		for (int x1 = 0;x1<2;++x1)
		{
			for (int x2 = 0;x2<2;++x2)
			{
				for (int x3 = 0;x3<2;++x3)
				{
					SAFE_RELEASE(m_ppsRenderSceneAllShadersBlob[i][x1][x2][x3]);
				}
			}
		}

	}

}


HRESULT CascadedShadowsManager::DestroyAndDeallocateShadowResources()
{
	SAFE_RELEASE(m_pMeshVertexLayout);
	SAFE_RELEASE(m_pvsRenderOrthoShadow);


	SAFE_RELEASE(m_pCascadedShadowMapTexture);
	SAFE_RELEASE(m_pCascadedShadowMapDSV);
	SAFE_RELEASE(m_pCascadedShadowMapSRV);

	SAFE_RELEASE(m_pcbGlobalConstantBuffer);

	SAFE_RELEASE(m_prsScene);
	SAFE_RELEASE(m_prsShadow);
	SAFE_RELEASE(m_prsShadowPancake);

	SAFE_RELEASE(m_pSamLinear);
	SAFE_RELEASE(m_pSamShadowPoint);
	SAFE_RELEASE(m_pSamShadowPCF);

	for (INT iCascadeIndex = 0;iCascadeIndex<MAX_CASCADES;++iCascadeIndex)
	{
		SAFE_RELEASE(m_pvsRenderScene[iCascadeIndex]);

		for (INT iDerivativeIndex = 0; iDerivativeIndex<2;++iDerivativeIndex)
		{
			for (INT iBlendIndex = 0;iBlendIndex<2;++iBlendIndex)
			{
				for (INT iIntervalIndex = 0;iIntervalIndex<2;++iIntervalIndex)
				{
					SAFE_RELEASE(m_ppsRenderSceneAllShaders[iCascadeIndex][iDerivativeIndex][iBlendIndex][iIntervalIndex]);
				}
			}
		}
	}


	return E_NOTIMPL;
}
HRESULT CascadedShadowsManager::InitFrame(ID3D11Device * pD3dDevice, CDXUTSDKMesh * mesh)
{

	ReleaseAndAllocateNewShadowResources(pD3dDevice);

	// Copy D3DX matrices into XNA Math Math matrices
	DirectX::XMMATRIX ViewCameraProjection = m_pViewerCamera->GetProjMatrix();
	DirectX::XMMATRIX ViewCameraView = m_pViewerCamera->GetViewMatrix();
	DirectX::XMMATRIX LightView = m_pLightCamera->GetViewMatrix();

	XMVECTOR det;
	XMMATRIX InverseViewCamera = XMMatrixInverse(&det, ViewCameraView);


	// Convert from min max representation to center extents representation
	// This will make it easier to pull the points out of the transformation.
	XMVECTOR vSceneCenter = m_vSceneAABBMin + m_vSceneAABBMax;
	vSceneCenter *= g_vHalfVector;
	XMVECTOR vSceneExtends = m_vSceneAABBMax - m_vSceneAABBMin;
	vSceneExtends *= g_vHalfVector;

	XMVECTOR vSceneAABBPointsLightSpace[8];
	//This function simply converts the center and extends of an AABB into 8 points
	CreateAABBPoints(vSceneAABBPointsLightSpace, vSceneCenter, vSceneExtends);
	//Transform the scene AABB to Light space.
	for (int index = 0;index<8;++index)
	{
		vSceneAABBPointsLightSpace[index] = XMVector4Transform(vSceneAABBPointsLightSpace[index], ViewCameraView);
	}

	FLOAT fFrustumIntervalBegin, fFrustumIntervalEnd;
	XMVECTOR vLightCameraOrthographicMin; //light space frustum aabb
	XMVECTOR vLightCameraOrthographicMax;
	FLOAT fCameraNearFarRange = m_pViewerCamera->GetFarClip() - m_pViewerCamera->GetNearClip();

	XMVECTOR vWorldUnitePerTexel = g_vZero;

	// we loop over the cascade to calculate the orthographic projection for each cascade.
	for (INT iCascadeIndex = 0;iCascadeIndex<m_CopyOfCascadeConfig.m_nCascadeLevels;++iCascadeIndex)
	{
		// Calculate the interval to the View Frustum that this cascade covers.We measure the interval
		// the cascade covers as a Min and Max Distance along the Z Axis
		if (m_eSelectedCascadesFit == FIT_TO_CASCADES)
		{
			// Because we want to fit the orthograpic projection tightly around the Cascade,we set the Minimum cascade
			// value to the previous Frustum and Interval
			if (iCascadeIndex == 0)
			{
				fFrustumIntervalBegin = 0.0f;
			}
			else
			{
				fFrustumIntervalBegin = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex - 1];
			}
		}
		else
		{
			// In the FIT_TO_SCENE technique the Cascade overlap each other.
			//In other words,interval 1 is covered by cascades 1 to 8,interval 2 is covered by cascade 2 to 8 and so forth.
			fFrustumIntervalBegin = 0.0f;
		}


		// Scale the intervals between 0 and 1,They are now percentages that we can scale with.
		fFrustumIntervalEnd = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex];
		fFrustumIntervalBegin /= (FLOAT)m_iCascadePartitionMax;
		fFrustumIntervalEnd /= (FLOAT)m_iCascadePartitionMax;
		fFrustumIntervalBegin = fFrustumIntervalBegin* fCameraNearFarRange;
		fFrustumIntervalEnd = fFrustumIntervalEnd*fCameraNearFarRange;

		XMVECTOR vFrustumPoints[8];

		//This functun takes the begin and end intervals along with the projection matrix and returns the 8 points
		// That represented the cascade Interval
		CreateFrustumPointFromCascadeInterval(fFrustumIntervalBegin, fFrustumIntervalEnd, ViewCameraProjection, vFrustumPoints);

		vLightCameraOrthographicMin = g_vFLTMAX;
		vLightCameraOrthographicMax = g_vFLTMIN;

		XMVECTOR vTempTranslatedCornerPoint;

		//This next section of code calculates the min and max values for the orthographic projection.
		for (int icpIndex = 0;icpIndex < 8;++icpIndex)
		{
			//Transform the frustum from camera view space to world space.
			vFrustumPoints[icpIndex] = XMVector4Transform(vFrustumPoints[icpIndex], InverseViewCamera);
			//Transform the point from world space to LightCamera Space.
			vTempTranslatedCornerPoint = XMVector4Transform(vFrustumPoints[icpIndex], LightView);
			// Find the closest point.
			vLightCameraOrthographicMin = XMVectorMin(vTempTranslatedCornerPoint, vLightCameraOrthographicMin);
			vLightCameraOrthographicMax = XMVectorMax(vTempTranslatedCornerPoint, vLightCameraOrthographicMax);

		}

		//This code removes the shimmering effect along the edges of shadow due to
		// the light changing to fit the camera.
		if (m_eSelectedCascadesFit == FIT_TO_SCENE)
		{
			// Fit the ortho projection to the cascades far plane and a near plane of zero.
			// Pad the projection to be the size of the diagonal of the Frustum partition.
			
			// To do this, we pad the ortho transform so that it is always big enough to cover
			// the entire camera view frustum.
			XMVECTOR vDiagonal = vFrustumPoints[0] - vFrustumPoints[6];
			vDiagonal = XMVector3Length(vDiagonal);

			// The bound is the length of the diagonal of the frustum interval.
			FLOAT fCascadeBound = XMVectorGetX(vDiagonal);

			//The offset calculated will pad the ortho projection so that it is always the same size
			// and big enough to cover the entire cascade interval
			XMVECTOR vBoarderoffset = (vDiagonal - (vLightCameraOrthographicMax - vLightCameraOrthographicMin))* g_vHalfVector;

			//Set the Z and W component to zero
			vBoarderoffset *= g_vMultiplySetzwZero;

			//Add the offsets to the projection.
			vLightCameraOrthographicMax += vBoarderoffset;
			vLightCameraOrthographicMin -= vBoarderoffset;

			//The world units per texel are used to snap the shadow the orthographic projection
			// to texel sized increments.This keeps the edges of the shadows from shimmering.
			FLOAT fWorldUnitsperTexel = fCascadeBound / (float)m_CopyOfCascadeConfig.m_iBufferSize;
			vWorldUnitePerTexel = XMVectorSet(fWorldUnitsperTexel, fWorldUnitsperTexel, 0.0f, 0.0f);

		}
		else if (m_eSelectedCascadesFit == FIT_TO_CASCADES)
		{
			// We calculate a looser bound based on the size of the PCF blur. This ensures us that we're
			// Sampling within the correct map.
			float fScaleDuetoBluredAMT = ((float)(m_iPCFBlurSize * 2 + 1) / (float)m_CopyOfCascadeConfig.m_iBufferSize);
			XMVECTORF32 vScaleDueToBluredAMT = { fScaleDuetoBluredAMT, fScaleDuetoBluredAMT, 0.0f, 0.0f };

			float fNormalizeByBufferSize = ((1.0f) / (float)m_CopyOfCascadeConfig.m_iBufferSize);
			XMVECTOR vNormalizeByBufferSize = XMVectorSet(fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f);

			//We calculate the offsets as a percentage of the bound.
			XMVECTOR vBoarderOffset = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
			vBoarderOffset *= g_vHalfVector;
			vBoarderOffset *= vScaleDueToBluredAMT;
			vLightCameraOrthographicMax += vBoarderOffset;
			vLightCameraOrthographicMin -= vBoarderOffset;


			// The world units per texel are used to snap the orthographic projection
			// to texel sized increments
			// Because we're fitting tightly to the cascade,the shimmering shadow edges will still be present when
			// the camera rotates.However when zooming in or strafing the shadow edge will not shimmer.
			vWorldUnitePerTexel = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
			vWorldUnitePerTexel *= vNormalizeByBufferSize;

		}

		float fLightCameraOrthographicMinZ = XMVectorGetZ(vLightCameraOrthographicMin);

		if (m_bMoveLightTexelSize)
		{
			// we snape the camera to 1 pixel increments so that moving the camera does not cause the shadow to jitter(抖动)
			// This is a matter of integer dividing by the world space size of texel
			vLightCameraOrthographicMin /= vWorldUnitePerTexel;
			vLightCameraOrthographicMin = XMVectorFloor(vLightCameraOrthographicMin);
			vLightCameraOrthographicMin *= vWorldUnitePerTexel;


			vLightCameraOrthographicMax /= vWorldUnitePerTexel;
			vLightCameraOrthographicMax = XMVectorFloor(vLightCameraOrthographicMax);
			vLightCameraOrthographicMax *= vWorldUnitePerTexel;

		}

		// These are the unconfigured near and far plane values. They are purposely awful to show
		// how important calculating accurate near and far plane is.
		FLOAT fNearPlane = 0.0f;
		FLOAT fFarPlane = 10000.0f;

		if (m_eSelectedNearFarFit == FIT_NEAR_FAR_AABB)
		{
			XMVECTOR vLightSpaceSceneAABBminValue = g_vFLTMAX; //World space scene aabb
			XMVECTOR vLightSpaceSceneAABBmaxValue = g_vFLTMIN;

			// We calculate the min and max vectors of the scene in the light space.The min and max "Z" values of the light space AABB
			// can be used for the near and far plane.This is easier than intersecting the scene with the AABB
			// and in some cases provides similar results
			for (int index = 0;index<8;++index)
			{
				vLightSpaceSceneAABBminValue = XMVectorMin(vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBminValue); 
				vLightSpaceSceneAABBmaxValue = XMVectorMax(vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBmaxValue);
			}

			//The min and max z values are the near and far planes.
			fNearPlane = XMVectorGetZ(vLightSpaceSceneAABBminValue);
			fFarPlane = XMVectorGetZ(vLightSpaceSceneAABBmaxValue);


		}
		else if(m_eSelectedNearFarFit == FIT_NEAR_FAR_SCENE_AABB || m_eSelectedNearFarFit == FIT_NEAR_FAR_PANCAKING)
		{
			//By intersecting the light frustum with the scene AABB we can get a tighter bound on the near and far plane.
			ComputeNearAndFar(fNearPlane, fFarPlane, vLightCameraOrthographicMin, vLightCameraOrthographicMax, vSceneAABBPointsLightSpace);

			if (m_eSelectedNearFarFit == FIT_NEAR_FAR_PANCAKING)
			{
				if (fLightCameraOrthographicMinZ > fNearPlane)
				{
					fNearPlane = fLightCameraOrthographicMinZ;
				}
			}
		}
		else
		{
			//todo
		}

		//Create the orthographic projection for this cacscade.
		m_matShadowProj[iCascadeIndex] = DirectX::XMMatrixOrthographicOffCenterLH(XMVectorGetX(vLightCameraOrthographicMin), XMVectorGetX(vLightCameraOrthographicMax),
			XMVectorGetY(vLightCameraOrthographicMin), XMVectorGetY(vLightCameraOrthographicMax),
			fNearPlane, fFarPlane);

		m_fCascadePartitionsFrustum[iCascadeIndex] = fFrustumIntervalEnd;

	}

	m_matShadowView = m_pLightCamera->GetViewMatrix();

	return S_OK;
}


// Render the cascades into a texture atlas.
HRESULT CascadedShadowsManager::RenderShadowForAllCascades(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dDeviceContext, CDXUTSDKMesh * pMesh)
{
	HRESULT hr = S_OK;

	XMMATRIX WorldViewProjection;
	XMMATRIX World;

	pD3dDeviceContext->ClearDepthStencilView(m_pCascadedShadowMapDSV, D3D11_CLEAR_DEPTH, 1.0, 0);
	ID3D11RenderTargetView* pNullView = nullptr;

	//Set a null render target so as not to render color.
	pD3dDeviceContext->OMSetRenderTargets(1, &pNullView, m_pCascadedShadowMapDSV);

	if (m_eSelectedNearFarFit == FIT_NEAR_FAR_PANCAKING)
	{
		pD3dDeviceContext->RSSetState(m_prsShadowPancake);
	}
	else
	{
		pD3dDeviceContext->RSSetState(m_prsShadow);
	}

	//Iterate over cascades and render shadows;
	for (INT currentCascade = 0;currentCascade<m_CopyOfCascadeConfig.m_nCascadeLevels;++currentCascade)
	{
		// Each cascade has its own viewport because we're storing all the cascades in on large texture
		pD3dDeviceContext->RSSetViewports(1, &m_RenderVP[currentCascade]);
		World = m_pLightCamera->GetWorldMatrix();

		// We calculate the matrices in th Init function.
		WorldViewProjection = m_matShadowView* m_matShadowProj[currentCascade];


		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V(pD3dDeviceContext->Map(m_pcbGlobalConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_ALL_SHADOW_DATA* pcbAllShadowConstants = (CB_ALL_SHADOW_DATA*)MappedResource.pData;

		pcbAllShadowConstants->m_WorldViewProj = DirectX::XMMatrixTranspose(WorldViewProjection);

		//XMMATRIX Identity = XMMatrixIdentity();
		
		//The model was exported in world space ,so we can pass the identity up as the world transform.
		pcbAllShadowConstants->m_World = DirectX::XMMatrixIdentity();

		pD3dDeviceContext->Unmap(m_pcbGlobalConstantBuffer, 0);
		pD3dDeviceContext->IASetInputLayout(m_pMeshVertexLayout);


		//No pixel shader is bound as we're only writing out depth.
		pD3dDeviceContext->VSSetShader(m_pvsRenderOrthoShadow, nullptr, 0);
		pD3dDeviceContext->PSSetShader(nullptr, nullptr, 0);
		pD3dDeviceContext->GSSetShader(nullptr, nullptr, 0);

		pD3dDeviceContext->VSSetConstantBuffers(0, 1, &m_pcbGlobalConstantBuffer);

		pMesh->Render(pD3dDeviceContext, 0, 1);
	}

	pD3dDeviceContext->RSSetState(nullptr);

	pD3dDeviceContext->OMSetRenderTargets(1, &pNullView, nullptr);

	return hr;
}
HRESULT CascadedShadowsManager::RenderScene(ID3D11DeviceContext * pD3dDeviceContext, ID3D11RenderTargetView * pRtvBackBuffer, ID3D11DepthStencilView * pDsvBackBuffer,
	CDXUTSDKMesh * pMesh, CFirstPersonCamera * pActiveCamera, D3D11_VIEWPORT * pViewPort, BOOL bVisualize)
{
	HRESULT hr = S_OK;

	D3D11_MAPPED_SUBRESOURCE MappedResource;

	//we have a separate render state for the actual rasterization because of different depth biases and Cull modes.
	pD3dDeviceContext->RSSetState(m_prsScene);

	pD3dDeviceContext->OMSetRenderTargets(1, &pRtvBackBuffer, pDsvBackBuffer);
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
		CameraProj = m_matShadowProj[(int)m_eSelectedCamera - 2];
		CameraView = m_matShadowView;
	}

	XMMATRIX WorldViewProjection = CameraView*CameraProj;

	V(pD3dDeviceContext->Map(m_pcbGlobalConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));

	CB_ALL_SHADOW_DATA* pcbAllShadowConstants = (CB_ALL_SHADOW_DATA*)MappedResource.pData;

	pcbAllShadowConstants->m_WorldViewProj = XMMatrixTranspose(WorldViewProjection);
	pcbAllShadowConstants->m_WorldView = XMMatrixTranspose(CameraView);

	//There are the for loop begin end values.
	pcbAllShadowConstants->m_iPCFBlurForLoopEnd = m_iPCFBlurSize / 2 + 1;
	pcbAllShadowConstants->m_iPCFBlurForLoopStart = m_iPCFBlurSize / -2;

	//This is a floating point number that is used as percentage to blur between maps
	pcbAllShadowConstants->m_fCascadeBlendArea = m_fBlurBetweenCascadesAmount;
	pcbAllShadowConstants->m_fTexelSize = 1.0f / (float)m_CopyOfCascadeConfig.m_iBufferSize;
	pcbAllShadowConstants->m_fNativeTexelSizeInX = pcbAllShadowConstants->m_fTexelSize / m_CopyOfCascadeConfig.m_nCascadeLevels;
	pcbAllShadowConstants->m_World = XMMatrixIdentity();

	XMMATRIX TextureScale = XMMatrixScaling(0.5f, -0.5f, 1.0f);
	XMMATRIX TextureTranslation = XMMatrixTranslation(0.5f,0.5f,0.0f);
	XMMATRIX ScaleToTile = XMMatrixScaling(1.0f / (float)m_pCascadeConfig->m_nCascadeLevels, 1.0f, 1.0f);

	pcbAllShadowConstants->m_fShadowBiasFromGUI = m_fPCFOffset;
	pcbAllShadowConstants->m_fShadowPartitionSize = 1.0f / (float)m_CopyOfCascadeConfig.m_nCascadeLevels;

	pcbAllShadowConstants->m_Shadow = XMMatrixTranspose(m_matShadowView);

	for (int index = 0;index<m_CopyOfCascadeConfig.m_nCascadeLevels;++index)
	{
		XMMATRIX mShadowTexture = m_matShadowProj[index] * TextureScale*TextureTranslation;
		XMFLOAT4X4 ShadowTexture;
		
		DirectX::XMStoreFloat4x4(&ShadowTexture, mShadowTexture);
		pcbAllShadowConstants->m_vCascadeScale[index] = XMVectorSet(ShadowTexture._11, ShadowTexture._22, ShadowTexture._33, 1);
		pcbAllShadowConstants->m_vCascadeOffset[index] = XMVectorSet(ShadowTexture._41, ShadowTexture._42, ShadowTexture._43, 0);
	
	}

	//Copy intervals for the depth interval selection method.
	memcpy(pcbAllShadowConstants->m_fCascadeFrustumEyeSpaceDepths, m_fCascadePartitionsFrustum, MAX_CASCADES * 4);
	for (int index = 0;index < MAX_CASCADES;++index)
	{
		pcbAllShadowConstants->m_fCascadeFrustumEyeSpaceDepthsFloat4[index].x = m_fCascadePartitionsFrustum[index];
	}

	//The border padding values keep the pixel shader from reading the borders during PCF filtering.
	pcbAllShadowConstants->m_fMaxBorderPadding = (float)(m_pCascadeConfig->m_iBufferSize - 1.0f) / (float)m_pCascadeConfig->m_iBufferSize;
	pcbAllShadowConstants->m_fCascadeBlendArea = (float)(1.0f) / (float)m_pCascadeConfig->m_iBufferSize;

	XMFLOAT3 ep;
	XMStoreFloat3(&ep, XMVector3Normalize(m_pLightCamera->GetEyePt() - m_pLightCamera->GetLookAtPt()));
	
	pcbAllShadowConstants->m_vLightDir = XMVectorSet(ep.x, ep.y, ep.z, 1.0f);
	pcbAllShadowConstants->m_nCascadeLeves = m_CopyOfCascadeConfig.m_nCascadeLevels;
	pcbAllShadowConstants->m_iVisualizeCascades = bVisualize;//jingz todo
	pD3dDeviceContext->Unmap(m_pcbGlobalConstantBuffer, 0);

	pD3dDeviceContext->PSSetSamplers(0, 1, &m_pSamLinear);
	pD3dDeviceContext->PSSetSamplers(1, 1, &m_pSamLinear);

	pD3dDeviceContext->PSSetSamplers(5, 1, &m_pSamShadowPCF);
	pD3dDeviceContext->GSSetShader(nullptr, nullptr, 0);

	pD3dDeviceContext->VSSetShader(m_pvsRenderScene[m_CopyOfCascadeConfig.m_nCascadeLevels - 1], nullptr, 0);

	//There are up to 8 cascades,possible derivative based offsets,blur between cascades,
	// and two cascade selection maps. This is total of 64permutations of the shader.
	pD3dDeviceContext->PSSetShader(m_ppsRenderSceneAllShaders[m_CopyOfCascadeConfig.m_nCascadeLevels - 1][m_iDerivativeBaseOffset][m_iBlurBetweenCascades][m_eSelectedCascadeSelection],
		nullptr, 0);


	pD3dDeviceContext->PSSetShaderResources(5, 1, &m_pCascadedShadowMapSRV);

	pD3dDeviceContext->VSSetConstantBuffers(0, 1, &m_pcbGlobalConstantBuffer);
	pD3dDeviceContext->PSSetConstantBuffers(0, 1, &m_pcbGlobalConstantBuffer);

	pMesh->Render(pD3dDeviceContext, 0, 1);

	ID3D11ShaderResourceView* nv[] = { nullptr,nullptr, nullptr, nullptr, nullptr, nullptr,nullptr, nullptr };
	pD3dDeviceContext->PSSetShaderResources(5, 8, nv);

	return hr;
}

struct Triangle
{
	XMVECTOR point[3];
	BOOL culled;
};

// Computing an accurate near and far plane will decrease surface ance and Peter-panning
// Surface acne is the term for erroneous self shadowing.Peter-panning is the effect where
// shadows disappear near the base of an object.
// As offsets are generally used with PCF filtering due self shadowing issues,computing the
// This concept is not complicated,but the intersection code is
void CascadedShadowsManager::ComputeNearAndFar(FLOAT & fNearPlane, FLOAT & fFarPlane, DirectX::FXMVECTOR vLightCameraOrthographicMin, DirectX::FXMVECTOR vLightCameraOrthographicMax, DirectX::XMVECTOR * pvPointInCameraView)
{
	// Initialize the near and far plane
	fNearPlane = FLT_MAX;
	fFarPlane = -FLT_MAX;

	Triangle triangleList[16];//jingz 每个frustum和三角形做面线裁减，至多可能生成多少个三角形？？
	INT iTriangleCount = 1;

	triangleList[0].point[0] = pvPointInCameraView[0];
	triangleList[0].point[1] = pvPointInCameraView[1];
	triangleList[0].point[2] = pvPointInCameraView[2];
	triangleList[0].culled = false;

	// These are the indices uesed to tesselate an AABB into a list of triangles.
	static const INT iAABBTriIndexes[] =
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
	// 1. Iterate over all 12 triangles of the AABB
	// 2. Clip the triangles against each plane,Create new triangle as needed
	// 3. Find the min and max z values as the near and far plane.
	
	//This is easier because the triangles are in camera spacing making the collisions tests sample comparisions.

	float fLightCameraOrthographicMinX = XMVectorGetX(vLightCameraOrthographicMin);
	float fLightCameraOrthographicMaxX = XMVectorGetX(vLightCameraOrthographicMax);
	float fLightCameraOrthographicMinY = XMVectorGetY(vLightCameraOrthographicMin);
	float fLightCameraOrthographicMaxY = XMVectorGetY(vLightCameraOrthographicMax);

	for (INT AABBTriIter = 0;AABBTriIter<12;++AABBTriIter)
	{
		triangleList[0].point[0] = pvPointInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 0]];
		triangleList[0].point[1] = pvPointInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 1]];
		triangleList[0].point[2] = pvPointInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 2]];

		iTriangleCount = 1;
		triangleList[0].culled = FALSE;

		//Clip each individual triangle against the 4 frustums.When ever a triangle is clipped into new triangles,
		// add them to the list.
		for (INT frustumPlaneIter = 0;frustumPlaneIter<4;++frustumPlaneIter)
		{
			FLOAT fEdge;
			INT iComponent;
			if (frustumPlaneIter == 0)
			{
				fEdge = fLightCameraOrthographicMinX;//todo make float temp
				iComponent = 0;
			}
			else if (frustumPlaneIter == 1)
			{
				fEdge = fLightCameraOrthographicMaxX;
				iComponent = 0;
			}
			else if (frustumPlaneIter == 2)
			{
				fEdge = fLightCameraOrthographicMinY;
				iComponent = 1;
			}
			else
			{
				fEdge = fLightCameraOrthographicMaxY;
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

					if (frustumPlaneIter ==0)//左平面
					{
						for (INT i = 0;i<3;++i)
						{
							//todo jingz 1和0 分别代表什么意义？？

							if (XMVectorGetX(triangleList[triIter].point[i]) > XMVectorGetX(vLightCameraOrthographicMin))
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
					else if (frustumPlaneIter == 1)//右平面
					{
						for (INT i = 0;i<3;++i)
						{
							if (XMVectorGetX(triangleList[triIter].point[i]) < XMVectorGetX(vLightCameraOrthographicMax))
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
					else if (frustumPlaneIter ==2)//jingz 下平面
					{
						for (INT i = 0; i < 3; ++i)
						{
							if (XMVectorGetY(triangleList[triIter].point[i]) > XMVectorGetY(vLightCameraOrthographicMin))
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
					else //上平面
					{
						for (INT i = 0; i < 3; ++i)
						{
							if (XMVectorGetY(triangleList[triIter].point[i]) < XMVectorGetY(vLightCameraOrthographicMax))
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
						tempPoint = triangleList[triIter].point[0];
						triangleList[triIter].point[0] = triangleList[triIter].point[1];
						triangleList[triIter].point[1] = tempPoint;

						bPointPassesCollision[0] = true;
						bPointPassesCollision[1] = false;

					}
					if (bPointPassesCollision[2] && !bPointPassesCollision[1])
					{
						tempPoint = triangleList[triIter].point[1];
						triangleList[triIter].point[1] = triangleList[triIter].point[2];
						triangleList[triIter].point[2] = tempPoint;
						bPointPassesCollision[1] = true;
						bPointPassesCollision[2] = false;
					}
					if (bPointPassesCollision[1]&&!bPointPassesCollision[0])
					{
						tempPoint = triangleList[triIter].point[0];
						triangleList[triIter].point[0] = triangleList[triIter].point[1];
						triangleList[triIter].point[1] = tempPoint;

						bPointPassesCollision[0] = true;
						bPointPassesCollision[1] = false;
					}

					if (iInsideVertCount == 0)
					{
						//All points failed.We're done
						triangleList[triIter].culled = false;
					}
					else if(iInsideVertCount == 1)
					{//One point passed.Clip the triangle against the frustum plane
						triangleList[triIter].culled = false;

						//
						XMVECTOR vVert0ToVert1 = triangleList[triIter].point[1] - triangleList[triIter].point[0];
						XMVECTOR vVert0ToVert2 = triangleList[triIter].point[2] - triangleList[triIter].point[0];

						// Find the collision ratio
						FLOAT fHitPointTimeTRatio = fEdge - XMVectorGetByIndex(triangleList[triIter].point[0],iComponent);

						//Calculate the distance along the vector as ratio of the hit ratio to the component
						FLOAT fRatioAlongVert01 = fHitPointTimeTRatio / XMVectorGetByIndex(vVert0ToVert1, iComponent);
						FLOAT fRatioAlongVert02 = fHitPointTimeTRatio / XMVectorGetByIndex(vVert0ToVert2, iComponent);

						//Add the point plus a percentage of the vector
						vVert0ToVert1 *= fRatioAlongVert01;
						vVert0ToVert1 += triangleList[triIter].point[0];
						vVert0ToVert2 *= fRatioAlongVert02;
						vVert0ToVert2 += triangleList[triIter].point[0];

						triangleList[triIter].point[1] = vVert0ToVert2;
						triangleList[triIter].point[2] = vVert0ToVert1;
					}
					else if(iInsideVertCount == 2)
					{
						// 2 in 
						// tessellate into 2 triangles

						//Copy the triangle(if is exists) after the current triangle out of
						// the way so we can override it with the new triangle we're inserting.
						triangleList[iTriangleCount] = triangleList[triIter + 1];

						triangleList[triIter].culled = false;
						triangleList[triIter + 1].culled = false;

						// Get the vector from the outside point into 2 inside points
						XMVECTOR vVert2ToVert0 = triangleList[triIter].point[0] - triangleList[triIter].point[2];
						XMVECTOR vVert2ToVert1 = triangleList[triIter].point[1] - triangleList[triIter].point[2];

						// Get the hit point ratio
						FLOAT fHitPointTime_2_0 = fEdge - XMVectorGetByIndex(triangleList[triIter].point[2], iComponent);
						FLOAT fRatioAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex(vVert2ToVert0, iComponent);
						//Calculate the new vertex by adding the percentage of the vector plus point 2
						vVert2ToVert0 *= fRatioAlongVector_2_0;
						vVert2ToVert0 += triangleList[triIter].point[2];
						
						// Add new triangle.
						triangleList[triIter + 1].point[0] = triangleList[triIter].point[0];
						triangleList[triIter + 1].point[1] = triangleList[triIter].point[1];
						triangleList[triIter + 1].point[2] = vVert2ToVert0;

						triangleList[triIter + 1].point[0] = triangleList[triIter].point[1];
						triangleList[triIter + 1].point[1] = triangleList[triIter].point[2];
						triangleList[triIter + 1].point[2] = vVert2ToVert0;

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
					float fTriangleCoordZ = XMVectorGetZ(triangleList[j].point[j]);

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
void CascadedShadowsManager::CreateFrustumPointFromCascadeInterval(FLOAT fCascadeIntervalBegin, FLOAT fCascadeIntervalEnd, DirectX::XMMATRIX & vProjection, DirectX::XMVECTOR * pvCornerPointsWorld)
{
	XNA::Frustum vViewFrustum;
	XNA::ComputeFrustumFromProjection(&vViewFrustum, &vProjection);

	vViewFrustum.Near = fCascadeIntervalBegin;
	vViewFrustum.Far = fCascadeIntervalEnd;


	static const XMVECTORU32 vGrabY = { 0x00000000,0xFFFFFFFF,0x00000000,0x00000000 };
	static const XMVECTORU32 vGrabX = { 0xFFFFFFFF,0x00000000,0x00000000,0x00000000 };

	XMVECTORF32 vRightTop = { vViewFrustum.RightSlope,vViewFrustum.TopSlope,1.0f,1.0f };
	XMVECTORF32 vLeftBottom = { vViewFrustum.LeftSlope,vViewFrustum.BottomSlope,1.0f,1.0f };
	XMVECTORF32 vNearFactor = { vViewFrustum.Near,vViewFrustum.Near,vViewFrustum.Near,1.0f };
	XMVECTORF32 vFarFactor = { vViewFrustum.Far,vViewFrustum.Far,vViewFrustum.Far,1.0f };
	XMVECTOR vRightTopNear = DirectX::XMVectorMultiply(vRightTop, vNearFactor);
	XMVECTOR vRightTopFar = DirectX::XMVectorMultiply(vRightTop, vFarFactor);
	XMVECTOR vLeftBottomNear = DirectX::XMVectorMultiply(vLeftBottom, vNearFactor);
	XMVECTOR vLeftBottomFar = DirectX::XMVectorMultiply(vLeftBottom, vFarFactor);

	pvCornerPointsWorld[0] = vRightTopNear;
	pvCornerPointsWorld[1] = DirectX::XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabX);
	pvCornerPointsWorld[2] = vLeftBottomNear;
	pvCornerPointsWorld[3] = DirectX::XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabY);

	pvCornerPointsWorld[4] = vRightTopFar;
	pvCornerPointsWorld[5] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabX);
	pvCornerPointsWorld[6] = vLeftBottomFar;
	pvCornerPointsWorld[7] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabY);

}

void CascadedShadowsManager::CreateAABBPoints(DirectX::XMVECTOR * vAABBPoints, DirectX::XMVECTOR vCenter, DirectX::FXMVECTOR vExtends)
{
	//This map enables us to use a for loop and do vector math.
	static const XMVECTORF32 vExtentsMap[]=
	{
		{ 1.0f,1.0f,-1.0f,1.0f },
		{ -1.0f,1.0f,-1.0f,1.0f },
		{ 1.0f,-1.0f,-1.0f,1.0f },
		{ -1.0f,-1.0f,-1.0f,1.0f },
		{ 1.0f,1.0f,1.0f,1.0f },
		{ -1.0f,1.0f,1.0f,1.0f },
		{ 1.0f,-1.0f,1.0f,1.0f },
		{ -1.0f,-1.0f,1.0f,1.0f }
	};

	for (INT i = 0;i <8;++i)
	{
		vAABBPoints[i] = XMVectorMultiplyAdd(vExtentsMap[i], vExtends, vCenter);
	}

}
HRESULT CascadedShadowsManager::ReleaseAndAllocateNewShadowResources(ID3D11Device * pD3dDevice)
{
	HRESULT hr = S_OK;

	//if any of the these 3 parameters was changed ,we must reallocate the D3D resources.
	if (m_CopyOfCascadeConfig.m_nCascadeLevels != m_pCascadeConfig->m_nCascadeLevels
		|| m_CopyOfCascadeConfig.m_ShadowBufferFormat != m_pCascadeConfig->m_ShadowBufferFormat
		|| m_CopyOfCascadeConfig.m_iBufferSize != m_pCascadeConfig->m_iBufferSize)
	{
		m_CopyOfCascadeConfig = *m_pCascadeConfig;

		SAFE_RELEASE(m_pSamLinear);
		SAFE_RELEASE(m_pSamShadowPCF);
		SAFE_RELEASE(m_pSamShadowPoint);

		D3D11_SAMPLER_DESC SamDesc;
		SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		SamDesc.AddressU = SamDesc.AddressV  = SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamDesc.MipLODBias = 0.0f;
		SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
		SamDesc.MinLOD = 0;
		SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamDesc, &m_pSamLinear));
		DXUT_SetDebugName(m_pSamLinear, "CSM Linear");

		D3D11_SAMPLER_DESC SamDescShadow;
		SamDescShadow.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		SamDescShadow.AddressU = SamDescShadow.AddressV = SamDescShadow.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamDescShadow.MipLODBias = 0.0f;
		SamDescShadow.MaxAnisotropy = 0;
		SamDescShadow.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
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

		for (INT i = 0;i<m_CopyOfCascadeConfig.m_nCascadeLevels;++i)
		{
			m_RenderVP[i].Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
			m_RenderVP[i].Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
			m_RenderVP[i].MaxDepth = 1.0f;
			m_RenderVP[i].MinDepth = 0.0f;
			m_RenderVP[i].TopLeftX = (FLOAT)(m_CopyOfCascadeConfig.m_iBufferSize*i);
			m_RenderVP[i].TopLeftY = 0;

		}

		m_RenderOneTileVP.Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
		m_RenderOneTileVP.Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
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


		D3D11_TEXTURE2D_DESC textureDesc;
		textureDesc.Width = m_CopyOfCascadeConfig.m_iBufferSize*m_CopyOfCascadeConfig.m_nCascadeLevels;
		textureDesc.Height = m_CopyOfCascadeConfig.m_iBufferSize;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = textureFormat;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		V_RETURN(pD3dDevice->CreateTexture2D(&textureDesc, NULL, &m_pCascadedShadowMapTexture));
		DXUT_SetDebugName(m_pCascadedShadowMapTexture, "CSM ShadowMap");

		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
		depthStencilViewDesc.Format = DsvFormat;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Flags = 0;

		V_RETURN(pD3dDevice->CreateDepthStencilView(m_pCascadedShadowMapTexture, &depthStencilViewDesc, &m_pCascadedShadowMapDSV));
		DXUT_SetDebugName(m_pCascadedShadowMapDSV,"CSM ShadowMap DSV");

		D3D11_SHADER_RESOURCE_VIEW_DESC depthStencilSrvDesc;
		depthStencilSrvDesc.Format = SrvFormat;
		depthStencilSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		depthStencilSrvDesc.Texture2D.MostDetailedMip = 0;
		depthStencilSrvDesc.Texture2D.MipLevels = 1;

		V_RETURN(pD3dDevice->CreateShaderResourceView(m_pCascadedShadowMapTexture, &depthStencilSrvDesc, &m_pCascadedShadowMapSRV));

		DXUT_SetDebugName(m_pCascadedShadowMapSRV, "CSM ShadowMap SRV");

	}


	return hr;
}
;


