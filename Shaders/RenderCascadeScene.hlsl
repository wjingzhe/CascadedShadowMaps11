//------------------
// File: RenderCascadeScene.hlsl
//
// This is the main shader file.This shader is compiled with several different flags
// to provide different customizations based on user controls.
// 
// Copyright(c) Microsoft Corporation. All rights reserved

//-----------------------------
// Globals
//-------------------------------


//The number of cascades
#ifndef CASCADE_COUNT_FLAG
#define CASCADE_COUNT_FLAG 3
#endif

//This flag uses the derivate information to map the texels in a shadow map to the
// view space plane of the primitive being rendered. This depth is then used as the 
// comparison depth and reduces self shadowing aliases.This technique is expensive
// and is only valid when objects are planer(such as a ground plane).
#ifndef USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG
#define USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG 0
#endif

// This flag enables the shadow to blend between cascades. This is most useful when
// the shadow maps are small and artifacts can be seen between the various cascade layers.
#ifndef BLEND_BETWEEN_CASCADE_LAYERS_FLAG
#define BLEND_BETWEEN_CASCADE_LAYERS_FLAG 0
#endif

//There are two methods for selecting the proper cascade a fragment lies in. Interval selection
// compares the depth of the fragment against the frustum's depth partition.
// Map based selection compares the texture coordinates against the actual cascade maps.
// Map based selection gives better coverage.
// Interval based selection is easier to extend and understand
//MapģʽΪ0��Depth_INTERVALΪ1
#ifndef SELECT_CASCADE_BY_INTERVAL_FLAG
#define SELECT_CASCADE_BY_INTERVAL_FLAG 0
#endif

#define MAP_FLAG 0
#define INTERVAL_FLAG 1

#define MAX_CASCADE_COUNT 8
#define MAX_CASCADE_COUNT_IN_4 ceil(MAX_CASCADE_COUNT / 4)
#define MAX_CASCADE_COUNT_MORE (ceil((MAX_CASCADE_COUNT+1) / 4)*4)

// Most tittles will find that 3-4 cascades with 
// BLEND_BETWEEN_CASCADE_LAYERS_FLAG, is good for lower and PCs.
// High end PCs will be able to handle more cascades,and larger blur bands.
// In some cases such as when large PCF kernels are used,derivate based depth offsets could be used
// with larger PCF blur kernels on high end PCs for the ground plane.

cbuffer cbAllShadowData:register(b0)
{
	matrix m_mWorldViewProjection:packoffset(c0);
	matrix m_mWorld:packoffset(c4);
	matrix m_mWorldView:packoffset(c8);
	matrix m_mShadowView:packoffset(c12);
	float4 m_vOffsetFactorFromShadowViewToTexure[CASCADE_COUNT_FLAG]:packoffset(c16);
	float4 m_vScaleFactorFromShadowViewToTexure[CASCADE_COUNT_FLAG]:packoffset(c24);
	int m_nCascadeLevels_Unused : packoffset(c32.x);//Number of Cascades
	bool m_bIsVisualizeCascades : packoffset(c32.y);//1 is to visualize the cascades in different colors. 0 is to just draw the scene
	int m_iPCFBlurForLoopStart : packoffset(c32.z);// For loop begin value.For a 5x5 kernel this would be -2.
	int m_iPCFBlurForLoopEnd : packoffset(c32.w);// For loop end value.For a 5x5 kernel this would be 3
	
	//For Map based selection scheme,this keeps the pixels inside of the valid range.
	// When there is no boarder,these values are 0 and 1 respectively.
	float m_fMinBorderPaddingInShadowUV : packoffset(c33.x);
	float m_fMaxBorderPaddingInShadowUV : packoffset(c33.y);
	float m_fPCFShadowDepthBiaFromGUI : packoffset(c33.z); // A shadow map offset to deal with self shadow artifacts.// These artifacts are aggravated by PCF.
	float m_fShadowTexPartitionPerLevel : packoffset(c33.w);

	float m_fMaxBlendRatioBetweenCascadeLevel : packoffset(c34.x);// Amount to overlap when blending between cascades.
	float m_fLogicTexelSizeInX : packoffset(c34.y);
	float m_fCascadedShadowMapTexelSizeInX : packoffset(c34.z);
//	float m_fPaddingForCB3 : packoffset(c34.w);//Padding variables exist because ConstBuffers must be a multiple of 16 bytes.
	
	float3 m_vLightDir : packoffset(c35);

	float4 m_fCascadePartitionDepthsInEyeSpace_InFloat4[MAX_CASCADE_COUNT_IN_4]: packoffset(c36);//The values along Z that separate the cascades.

	float4 m_fCascadePartitionDepthsInEyeSpace_OnlyX[MAX_CASCADE_COUNT_MORE]: packoffset(c38);//The values along Z that separate the cascades.//init from pixel shader
};



//-------------------------------------------
//Textures and Samplers
//---------------------------------------
Texture2D g_txDiffuse:register(t0);
Texture2D<float> g_txShadow:register(t5);

SamplerState g_SamLinear:register(s0);
SamplerComparisonState g_SamplerComparisonState:register(s5);

//-----------------------------------------
// Input/Output structures
//-----------------------------------------
struct VS_INPUT
{
	float4 vPositionL:POSITION;
	float3 vNormal:NORMAL;
	float2 vTexCoord:TEXCOORD0;
};

struct VS_OUTPUT
{
	float3 vNormal:NORMAL;
	float2 vTexCoord:TEXCOORD0;
	float4 vPosInShadowView:TEXCOORD1;
	float4 vPosition:SV_POSITION;
	float4 vInterpPos:TEXCOORD2;
	float fDepthInView:TEXCOORD3;
};

//Vertex Shader
VS_OUTPUT VSMain(VS_INPUT Input)
{
	VS_OUTPUT Output;
	
	Output.vPosition  = mul(Input.vPositionL,m_mWorldViewProjection);
	Output.vNormal = mul(Input.vNormal,(float3x3)m_mWorld);
	Output.vTexCoord = Input.vTexCoord;
	Output.vInterpPos = Input.vPositionL;
	Output.fDepthInView = mul(Input.vPositionL,m_mWorldView).z;
	
	//Transform the shadow texture coordinates for all the cascades.
	Output.vPosInShadowView = mul(Input.vPositionL,m_mShadowView);
	return Output;
};


static const float4 vCascadeColors_DEBUG[8] =
{
	float4(1.5f,0.0f,0.0f,1.0f),
	float4(0.0f, 1.5f, 0.0f, 1.0f),
	float4(0.0f, 0.0f, 5.5f, 1.0f),
	float4(1.5f, 0.0f, 5.5f, 1.0f),
	float4(1.5f, 1.5f, 0.0f, 1.0f),
	float4(1.0f, 1.0f, 1.0f, 1.0f),
	float4(0.0f, 1.0f, 5.5f, 1.0f),
	float4(0.5f, 3.5f, 0.75f, 1.0f)
};



void TranformShadowToTexture3D(in float4 vPosInShadowView, in int iCascadeIndex, out float4 vShadowUV_InTexCoord)
{
	vShadowUV_InTexCoord = vPosInShadowView * m_vScaleFactorFromShadowViewToTexure[iCascadeIndex];
	vShadowUV_InTexCoord += m_vOffsetFactorFromShadowViewToTexure[iCascadeIndex];
}



/*
��1��ע��������N���㼶���������ж�Ӧ��N��ShadowMap�����Ǻϳ���һ����ʵ��ShadowTexture,
Ҳ����˵���ShadowMap�ķֱ�����1024X1024������xΪƽ�̷���������ϳ�ShadowMap�ֱ���Ϊ(N*1024)X1024��
��˿��Կ�������ShadowMap��������Ĵ�
*/
void TransformLogicU_ToNativeU(in int iCascadeIndex, in out float4 vShadowTexCoord)
{
	//��������ǰCascade��Ӧ����ʵShadowTexure X��
	// x*i+x/length
	//������������Ϊ�����ܰѹ�ʽ��д����

	vShadowTexCoord.x *= m_fShadowTexPartitionPerLevel;// precomputed (float) iCascadeIndex/(float)CASCADE_COUNT_FLAG
													   //�ۼ�iCascadeIndex ���ڵ�ƫ�ƻ�ַ��m_fShadowTexPartitionPerLevel*(float)iCascadeIndex
	vShadowTexCoord.x += (m_fShadowTexPartitionPerLevel*(float)iCascadeIndex);
}


//----------------------------
// This function calculates the screen space depth for shadow space texels
// -------
void CalculateRightAndUpTexelDepthDeltas(in float3 vShadowTexDDX, in float3 vShadowTexDDY,
										out float fUpTexDepthWeight,out float fRightTexDepthWeight)
{
	//we use derivatives in X and Y to create a transformation matrix.Because these derivatives give us the 
	// transformation from screen space to shadow space,we need the inverse matrix to take us from shadow space
	// to screen space.This new matrix will allow us to map shadow map texels to screen space. This will allow
	// us to find the screen space depth of a corresponding depth pixel.
	// This is not a perfect solution as it assumes the underlying geometry of the scene is a plane.A more
	// accurate way of finding the actual depth would be to do a deferred rendering approach and actually sample the depth
	
	// Using an offset,or using variance shadow maps is a better approach to reducing these artifacts in most cases.
	
	float2x2 matScreenToShadow = float2x2(vShadowTexDDX.xy,vShadowTexDDY.xy);
	float fDeterminant = determinant(matScreenToShadow);
	
	float fInvDeterminant = 1.0f/fDeterminant;
	
	float2x2 matShadowToScreen = float2x2(
		matScreenToShadow._22 * fInvDeterminant,matScreenToShadow._12*-fInvDeterminant,
		matScreenToShadow._21 * -fInvDeterminant,matScreenToShadow._11*fInvDeterminant); 
	
	float2 vRightShadowTexelLocation = float2(m_fLogicTexelSizeInX,0.0f);
	float2 vUpShadowTexelLocation = float2(0.0f,m_fLogicTexelSizeInX);
	
	//Transform the right pixel by the shadow space to screen matrix
	float2 vRightTexelDepthRatio = mul(vRightShadowTexelLocation,matShadowToScreen);
	float2 vUpTexelDepthRatio = mul(vUpShadowTexelLocation,matShadowToScreen);
	
	//We can now calculate how much depth changes when you move up or right in the shadow map.
	// We use the ratio of change in x and y times the derivative in X and Y of the screen space
	// depth to calculate this change
	fUpTexDepthWeight = vUpTexelDepthRatio.x*vShadowTexDDX.z+vUpTexelDepthRatio.y*vShadowTexDDY.z;
	fRightTexDepthWeight = vRightTexelDepthRatio.x* vShadowTexDDX.z + vRightTexelDepthRatio.y*vShadowTexDDY.z;
	
	
};


//
// Use PCF to sample the depth map and return a percent lit value
//
void CalculatePCFPercentLit(in float4 vShadowTexCoord,in float fRightTexelDepthDelta,in float fUpTexelDepthDelta,
						in float fBlurRowSize,out float fPercentLit)
{
	fPercentLit = 0.0f;//jingz ��Ӱϵ��������������Ӱ���ֵ�ĵ��ۼ�ƽ����ϵ�������������ȫ��Ӱ��ȫ����֮���ֵ��ϵ��

	//This loop could be unrolled,and texture immediate offsets could be used if the kernel size were fixed/
	// This would be performance improvement.
	for(int x = m_iPCFBlurForLoopStart;x<m_iPCFBlurForLoopEnd;++x)
	{




		for(int y = m_iPCFBlurForLoopStart;y<m_iPCFBlurForLoopEnd;++y)
		{
			// A very simple solution to the depth bias problems of PCF is to use an offset.
			// Unfortunately,too much offset can lead to Peter-panning(Shadows near the base of object disappear)
			// Too little offset can lead to shadow acne(objects that should not be in shadow are partially self shadowed).
			float depthCompare = vShadowTexCoord.z;
		

			if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
			{
				//Add in derivative computed depth scale based on the x and y pixel/
				depthCompare += fRightTexelDepthDelta * ((float)x) + fUpTexelDepthDelta * ((float)y);
			}
			//// Compare the transformed pixel depth to the depth read from the map.
			float2 uv = float2(vShadowTexCoord.x + ((float)x)*m_fCascadedShadowMapTexelSizeInX,
				vShadowTexCoord.y + ((float)y)*m_fLogicTexelSizeInX);

			float temp = g_txShadow.SampleCmpLevelZero(g_SamplerComparisonState, uv, depthCompare);

			fPercentLit += temp;

		}
		
	}//for
	
	fPercentLit /= (float)fBlurRowSize;

}


//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
//����һ������������ı�����ΪʲôŪ����ô������Ϊʲô
//Ӱ�������Ǵ���һ���±굽��ǰiCurrentCascadeIndexΪ��ǰ����
//void CalculateBlendAmountForInterval(in int iCurrentCascadeIndex,
//	in float fCurPixelDepth, in out float fRatioCurrentPixelsBlendBandLocationToUpInterval, out float fBlendRatioBetweenCascadeLevel)
//{
//	int fBlendIntervalBelowIndex = min(0, iCurrentCascadeIndex - 1);
//	float fPixelDepth = fCurPixelDepth - m_fCascadePartitionDepthsInEyeSpace_OnlyX[fBlendIntervalBelowIndex].x;
//
//	float fBlendInterval = m_fCascadePartitionDepthsInEyeSpace_OnlyX[iCurrentCascadeIndex].x - m_fCascadePartitionDepthsInEyeSpace_OnlyX[fBlendIntervalBelowIndex].x;;
//
//	fRatioCurrentPixelsBlendBandLocationToUpInterval = 1.0f - fPixelDepth / fBlendInterval;
//
//	fBlendRatioBetweenCascadeLevel = fRatioCurrentPixelsBlendBandLocationToUpInterval / m_fMaxBlendRatioBetweenCascadeLevel;
//}

void CalculateBlendAmountForInterval(in int iCurrentCascadeIndex,
	in float fPixelDepth, in out int iClosestIndex_Next, in out float fBlendRatioBetweenCascadeLevel)
{
	int temp_iCurrentCascadeIndex = iCurrentCascadeIndex + 1;
	int iPreCascadeIndex = temp_iCurrentCascadeIndex - 1;
	int iNextCascadeIndex = min(CASCADE_COUNT_FLAG, temp_iCurrentCascadeIndex + 1);


	//We need to calculate the band of the current shadow map where it will be fade into the next cascade.
	// We can then early out of the expensive PCF for loop
	//

	float fRatioCurrentPixelsBlendBandLocationToClosestInterval = 1.0f;

	float fDifferCurPixelDepthToBelowInterval = fPixelDepth - m_fCascadePartitionDepthsInEyeSpace_OnlyX[iPreCascadeIndex].x;
	float fDepthIntervalSize = m_fCascadePartitionDepthsInEyeSpace_OnlyX[iCurrentCascadeIndex].x - m_fCascadePartitionDepthsInEyeSpace_OnlyX[iPreCascadeIndex].x;

	// The current pixel's blend band location will be used to determine when we need to blend and by how much.
	float fRatioCurrentPixelsBlendBandLocationToPrevInterval = fDifferCurPixelDepthToBelowInterval / fDepthIntervalSize;

	float fDifferCurPixelDepthToNextInterval = m_fCascadePartitionDepthsInEyeSpace_OnlyX[iNextCascadeIndex].x - fPixelDepth;
	float fDepthIntervalSizeToNext = m_fCascadePartitionDepthsInEyeSpace_OnlyX[iNextCascadeIndex].x - m_fCascadePartitionDepthsInEyeSpace_OnlyX[iCurrentCascadeIndex].x;
	// The current pixel's blend band location will be used to determine when we need to blend and by how much.
	float fRatioCurrentPixelsBlendBandLocationToNextInterval = fDifferCurPixelDepthToNextInterval / fDepthIntervalSizeToNext;


	if (fRatioCurrentPixelsBlendBandLocationToPrevInterval<fRatioCurrentPixelsBlendBandLocationToNextInterval)
	{
		fRatioCurrentPixelsBlendBandLocationToClosestInterval = fRatioCurrentPixelsBlendBandLocationToPrevInterval;
		iClosestIndex_Next = iPreCascadeIndex;
	}
	else
	{
		fRatioCurrentPixelsBlendBandLocationToClosestInterval = fRatioCurrentPixelsBlendBandLocationToNextInterval;
		iClosestIndex_Next = iNextCascadeIndex;
	}
	// The fBlendRatioBetweenCascadeLevel is our location in the blend band.
	fBlendRatioBetweenCascadeLevel = fRatioCurrentPixelsBlendBandLocationToClosestInterval / m_fMaxBlendRatioBetweenCascadeLevel;
}

//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForMap(in float4 vShadowUV_InTexCoord,in out float fCurrentPixelsBlendRatioBandLocation,out float fBlendRatioBetweenCascadeLevel)
{
	//Calculate the blend band for the map based selection.
	float2 distanceToOne = float2(1.0f-vShadowUV_InTexCoord.x,1.0f-vShadowUV_InTexCoord.y);
	fCurrentPixelsBlendRatioBandLocation = min(vShadowUV_InTexCoord.x,vShadowUV_InTexCoord.y);
	float fCurrentPixelsBlendRatioBandLocation2 = min(distanceToOne.x,distanceToOne.y);
	fCurrentPixelsBlendRatioBandLocation = min(fCurrentPixelsBlendRatioBandLocation,fCurrentPixelsBlendRatioBandLocation2);
	fBlendRatioBetweenCascadeLevel = fCurrentPixelsBlendRatioBandLocation/m_fMaxBlendRatioBetweenCascadeLevel;
}


//---------------------------------
// Calculate the Shadow based on several options and render the scene
//------------------------------------------------
float4 PSMain(VS_OUTPUT Input):SV_TARGET
{
	float4 vDiffuse = g_txDiffuse.Sample(g_SamLinear,Input.vTexCoord);
	
	float4 vShadowUV_InTexCoord = float4(0.0f,0.0f,0.0f,0.0f);
	float4 vShadowUV_InTexCoord_Next = float4(0.0f,0.f,0.f,0.f);
	
	float fPercentLit = 0.0f;
	float fPercentLit_NextCascadedLevel = 0.0f;
	
	float fUpTexDepthWeight = 0;
	float fRightTexDepthWeight = 0;
	float fUpTexDepthWeight_Next = 0;
	float fRightTexDepthWeight_Next = 0;
	
	int iBlurRowSize = m_iPCFBlurForLoopEnd-m_iPCFBlurForLoopStart;

	float fBlurRowSize = (float)(iBlurRowSize*iBlurRowSize);

	
	//The interval based selection technique compares the pixel's depth against the frustum's cascade divisions.
	float fCurrentPixelDepthInView = Input.fDepthInView;
	
	
	// This for loop is not necessary when the frustum is uniformly divided and interval based selection is used.
	// In this case fCurrentPixelDepthInView could be used as an array lookup into the correct frustum.
	int iCurrentCascadeIndex = 0;


	int iClosestIndex_Next = 0;

	//
	//Ѱ�ҵ�ǰ���صĺ�����Ӱ�㼶�±�

	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		if(CASCADE_COUNT_FLAG > 1)
		{
			float4 vCurrentPixelDepth = float4(Input.fDepthInView, Input.fDepthInView, Input.fDepthInView, Input.fDepthInView);

			//jingz ������±�㼶�϶����������ֵһ�����ڽ������±�㼶�����ֵ
			//�Եݽ�����ʽ�ۼƲ㼶�±꣬dot����ʵ�ֲ�ͬ������Ӧ�Ĳ㼶����Ч�����ο������˵����һ���߲㼶��Ӱ�ǿ����ҷ��������СҪ������ۼ�1x1=1�±�

			float4 fComparison = (vCurrentPixelDepth > m_fCascadePartitionDepthsInEyeSpace_InFloat4[0]);
			float4 fComparison2 = (vCurrentPixelDepth > m_fCascadePartitionDepthsInEyeSpace_InFloat4[1]);
			float fIndex = dot(
							float4(CASCADE_COUNT_FLAG>0,CASCADE_COUNT_FLAG>1,CASCADE_COUNT_FLAG>2,CASCADE_COUNT_FLAG>3),
							fComparison)
							+dot(
							float4(CASCADE_COUNT_FLAG > 4,CASCADE_COUNT_FLAG > 5,CASCADE_COUNT_FLAG>6,CASCADE_COUNT_FLAG>7),
							fComparison2
							);
							
			fIndex = min(fIndex,CASCADE_COUNT_FLAG-1);
			iCurrentCascadeIndex = (int)fIndex;
		}
	}
	else// Ĭ�ϵ�Mapģʽ
	{
		bool bCascadeFound = false;

		for(int iCascadeIndex = 0;iCascadeIndex<CASCADE_COUNT_FLAG&& !bCascadeFound;++iCascadeIndex)
		{
			TranformShadowToTexture3D(Input.vPosInShadowView, iCascadeIndex, vShadowUV_InTexCoord);
				
			if( min(vShadowUV_InTexCoord.x,vShadowUV_InTexCoord.y) > m_fMinBorderPaddingInShadowUV
				&& max(vShadowUV_InTexCoord.x,vShadowUV_InTexCoord.y) < m_fMaxBorderPaddingInShadowUV)
			{
				iCurrentCascadeIndex = iCascadeIndex;//jingz �����ҵ��������ڲ㼶xy�ü����±�
				bCascadeFound = true;
			}
		}
	}

	float fBlendRatioBetweenCascadeLevel = 1.0f;
	float fCurrentPixelsBlendRatioBandLocation=1.0f;//band�Ǵ����鵵����˼������ǰ���غ͸�����Ĳ�ֵ��һ��������ϱ�����
	



	if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
	{
		if (SELECT_CASCADE_BY_INTERVAL_FLAG&CASCADE_COUNT_FLAG > 1)
		{
			CalculateBlendAmountForInterval(iCurrentCascadeIndex,
				fCurrentPixelDepthInView, iClosestIndex_Next, fBlendRatioBetweenCascadeLevel);
		}
		else
		{
			CalculateBlendAmountForMap(Input.vPosInShadowView, fCurrentPixelsBlendRatioBandLocation, fBlendRatioBetweenCascadeLevel);
		}
	}
	
	float3 vShadowUV_InTexCoordDDX;
	float3 vShadowUV_InTexCoordDDY;
	// The derivative are used to find the slope of the current plane
	// The derivative calculation has to be inside of the loop in order to prevent divergent flow control artifacts.
	if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
	{
		vShadowUV_InTexCoordDDX = ddx(Input.vPosInShadowView);
		vShadowUV_InTexCoordDDY = ddy(Input.vPosInShadowView);
		
		vShadowUV_InTexCoordDDX *= m_vScaleFactorFromShadowViewToTexure[iCurrentCascadeIndex];
		vShadowUV_InTexCoordDDY *= m_vScaleFactorFromShadowViewToTexure[iCurrentCascadeIndex];
	}

	//     Now that we know the correct map, we can transform the world space position of the current fragment                
	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		TranformShadowToTexture3D(Input.vPosInShadowView, iCurrentCascadeIndex, vShadowUV_InTexCoord);
	}

	TransformLogicU_ToNativeU(iCurrentCascadeIndex, vShadowUV_InTexCoord);
	
	float4 vVisualizeCascadeColor = float4(0.0f,0.0f,0.0f,1.0f);
	if (m_bIsVisualizeCascades)
	{
		vVisualizeCascadeColor = vCascadeColors_DEBUG[iCurrentCascadeIndex];
	}
	else
	{
		vVisualizeCascadeColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	
	if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
	{
		CalculateRightAndUpTexelDepthDeltas(vShadowUV_InTexCoordDDX,vShadowUV_InTexCoordDDY,fUpTexDepthWeight,fRightTexDepthWeight);
	}
	
	CalculatePCFPercentLit(vShadowUV_InTexCoord,fRightTexDepthWeight,fUpTexDepthWeight,fBlurRowSize,fPercentLit);
	
	if(BLEND_BETWEEN_CASCADE_LAYERS_FLAG && CASCADE_COUNT_FLAG > 1)
	{
		if(fCurrentPixelsBlendRatioBandLocation < m_fMaxBlendRatioBetweenCascadeLevel)
		{
			//the current pixel is within the blend band.
			
			//Repeat Texture coordinate calculations for the next cascade.
			// The next cascade index is used for blurring between maps.
			
			TranformShadowToTexture3D(Input.vPosInShadowView, iClosestIndex_Next, vShadowUV_InTexCoord_Next);

			TransformLogicU_ToNativeU(iClosestIndex_Next, vShadowUV_InTexCoord_Next);

			
			// We repeat the calculation for the next cascade layer,when blending between maps.
			if(fCurrentPixelsBlendRatioBandLocation < m_fMaxBlendRatioBetweenCascadeLevel)
			{
				// The current pixel is within the blend band.
				if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
				{
					CalculateRightAndUpTexelDepthDeltas(vShadowUV_InTexCoordDDX,vShadowUV_InTexCoordDDY,fUpTexDepthWeight_Next,fRightTexDepthWeight_Next);
				}
				

				CalculatePCFPercentLit(saturate(vShadowUV_InTexCoord_Next), fRightTexDepthWeight_Next, fUpTexDepthWeight_Next, fBlurRowSize, fPercentLit_NextCascadedLevel);
				// Blend the two calculated shadows bu the blend amount

				fPercentLit = lerp(fPercentLit_NextCascadedLevel,fPercentLit,fBlendRatioBetweenCascadeLevel);
				
			}
			
		}
	}
	

	
	float3 vLightDir1 = float3(-1.0f,1.0f,-1.0f);
	float3 vLightDir2 = float3(1.0f,1.0f,-1.0f);
	float3 vLightDir3 = float3(0.0f,-1.0f,0.0f);
	float3 vLightDir4 = float3(1.0f,1.0f,1.0f);
	
	// Some ambient-like lighting.
	float fLighting = 
		saturate( dot(vLightDir1,Input.vNormal))*0.05f+
		saturate( dot(vLightDir2,Input.vNormal))*0.05f+
		saturate( dot(vLightDir3,Input.vNormal))*0.05f+
		saturate( dot(vLightDir4,Input.vNormal))*0.05f;
	
	
	float4 vShadowLighting = fLighting * 0.5f;
	fLighting += saturate(dot(m_vLightDir,Input.vNormal));
	fLighting = lerp(vShadowLighting,fLighting,fPercentLit);
	
	return fLighting* vVisualizeCascadeColor*vDiffuse;
	//return vVisualizeCascadeColor*vDiffuse;
};


