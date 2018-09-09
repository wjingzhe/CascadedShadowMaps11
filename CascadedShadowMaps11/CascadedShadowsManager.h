#pragma once

#include "ShadowSampleMisc.h"
#include <d3d11.h>

class CFirstPersonCamera;
class CDXUTSDKMesh;

#pragma warning(push)
#pragma warning(disable:4324)

__declspec(align(16)) class CascadedShadowsManager
{
public:
	CascadedShadowsManager();
	~CascadedShadowsManager();

	//This runs when the application is initialized
	HRESULT Init(ID3D11Device* pD3DDevice, ID3D11DeviceContext* pD3DImmediateContext,
		CDXUTSDKMesh* pMesh, CFirstPersonCamera* pViewerCamera, CFirstPersonCamera* pLightCamera, CascadeConfig* pCascadeConfig);
	
	HRESULT DestroyAndDeallocateShadowResources();

	//This runs per frame.This data could be cached when the cameras do not move.
	HRESULT InitFrame(ID3D11Device* pD3dDevice, CDXUTSDKMesh* mesh);

	HRESULT RenderShadowForAllCascades(ID3D11Device* pD3dDevice, ID3D11DeviceContext* pD3dDeviceContext, CDXUTSDKMesh* pMesh);

	HRESULT RenderScene(ID3D11DeviceContext* pD3dDeviceContext,
		ID3D11RenderTargetView* pRtvBackBuffer,
		ID3D11DepthStencilView* pDsvBackBuffer,
		CDXUTSDKMesh* pMesh,
		CFirstPersonCamera* pActiveCamera,
		D3D11_VIEWPORT* pViewPort,
		BOOL bVisualize);

	DirectX::XMVECTOR GetScenAABBMin()
	{
		return m_vSceneAABBMin;
	}

	DirectX::XMVECTOR GetSceneAABBMax()
	{
		return m_vSceneAABBMax;
	}

	INT m_iCascadePartitionMax;
	FLOAT m_fCascadePartitionsFrustum[MAX_CASCADES]; // Values are between near and far
	INT m_iCascadePartitionsZeroToOne[MAX_CASCADES]; // Values are 0 to 100 and represent of the frstum
	INT m_iPCFBlurSize;
	FLOAT m_fPCFOffset;
	INT m_iDerivativeBaseOffset;
	INT m_iBlurBetweenCascades;
	FLOAT m_fBlurBetweenCascadesAmount;

	BOOL m_bMoveLightTexelSize;
	CAMERA_SELECTION m_eSelectedCamera;
	FIT_PROJECTION_TO_CASCADES m_eSelectedCascadesFit;
	FIT_TO_NEAR_FAR m_eSelectedNearFarFit;
	CASCADE_SELECTION m_eSelectedCascadeSelection;


private:
	//Compute the near far plane by interesting an Ortho Projection with the Scenes AABB
	void ComputeNearAndFar(FLOAT& fNearPlane, FLOAT& fFarPlane, DirectX::FXMVECTOR vLightCameraOrthographicMin, DirectX::FXMVECTOR vLightCameraOrthographicMax, DirectX::XMVECTOR* pvPointInCameraView);

	void CreateFrustumPointFromCascadeInterval(FLOAT fCascadeIntervalBegin, FLOAT fCascadeIntervalEnd, DirectX::XMMATRIX& vProjection, DirectX::XMVECTOR* pvCornerPointsWorld);

	void CreateAABBPoints(DirectX::XMVECTOR* vAABBPoints, DirectX::XMVECTOR vCenter, DirectX::FXMVECTOR vExtends);

	HRESULT ReleaseAndAllocateNewShadowResources(ID3D11Device* pD3dDevice); // This is called when cascade config changes

	DirectX::XMVECTOR m_vSceneAABBMin;
	DirectX::XMVECTOR m_vSceneAABBMax;

	// For example:when the shadow buffer size changes.
	char m_cvsMode[32];
	char m_cpsMode[32];
	char m_cgsMode[32];
	DirectX::XMMATRIX m_matShadowProj[MAX_CASCADES];
	DirectX::XMMATRIX m_matShadowView;
	CascadeConfig m_CopyOfCascadeConfig; // this copy is used to determine when setting change.
										// Some of these settings require new buffer allocations
	CascadeConfig* m_pCascadeConfig;	//Pointer to the most recent setting.

	// D3D11 variables
	ID3D11InputLayout* m_pMeshVertexLayout;
	ID3D11VertexShader* m_pvsRenderOrthoShadow;
	ID3DBlob* m_pvsRenderOrthoShadowBlob;
	ID3D11VertexShader* m_pvsRenderScene[MAX_CASCADES];
	ID3DBlob* m_pvsRenderSceneBlob[MAX_CASCADES];
	ID3D11PixelShader* m_ppsRenderSceneAllShaders[MAX_CASCADES][2][2][2];
	ID3DBlob* m_ppsRenderSceneAllShadersBlob[MAX_CASCADES][2][2][2];

	ID3D11Texture2D* m_pCascadedShadowMapTexture;
	ID3D11DepthStencilView* m_pCascadedShadowMapDSV;
	ID3D11ShaderResourceView* m_pCascadedShadowMapSRV;

	ID3D11Buffer* m_pcbGlobalConstantBuffer;// All VS and PS Contants are in the same buffer.
											// An actual title would break this up into multiple
											// buffers updated based on frequency of variable changes

	ID3D11RasterizerState* m_prsScene;
	ID3D11RasterizerState* m_prsShadow;
	ID3D11RasterizerState* m_prsShadowPancake;

	D3D11_VIEWPORT m_RenderVP[MAX_CASCADES];
	D3D11_VIEWPORT m_RenderOneTileVP;

	CFirstPersonCamera* m_pViewerCamera;
	CFirstPersonCamera* m_pLightCamera;

	ID3D11SamplerState* m_pSamLinear;
	ID3D11SamplerState* m_pSamShadowPCF;
	ID3D11SamplerState* m_pSamShadowPoint;


protected:
private:
};

#pragma warning(pop)