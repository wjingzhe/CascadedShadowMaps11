//--------------------------------------------------------------------------------------
// File: RenderCascadeScene.hlsl
//
// This is the main shader file.This shader is compiled with several different flags
// to provide different customizations based on user controls.
// 
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------


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
//Map模式为0，Depth_INTERVAL为1
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
	float4 m_vScaleFactorFromOrthoProjToTexureCoord[CASCADE_COUNT_FLAG]:packoffset(c24);
	int m_nCascadeLevels_Unused : packoffset(c32.x);//Number of Cascades
	int m_iIsVisualizeCascades : packoffset(c32.y);//1 is to visualize the cascades in different colors. 0 is to just draw the scene
	int m_iPCFBlurForLoopStart : packoffset(c32.z);// For loop begin value.For a 5x5 kernel this would be -2.
	int m_iPCFBlurForLoopEnd : packoffset(c32.w);// For loop end value.For a 5x5 kernel this would be 3
	
	//For Map based selection scheme,this keeps the pixels inside of the valid range.
	// When there is no boarder,these values are 0 and 1 respectively.
	float m_fMinBorderPaddingInShadowUV : packoffset(c33.x);
	float m_fMaxBorderPaddingInShadowUV : packoffset(c33.y);
	float m_fPCFShadowDepthBiaFromGUI : packoffset(c33.z); // A shadow map offset to deal with self shadow artifacts.// These artifacts are aggravated by PCF.
	float m_fWidthPerShadowTextureLevel_InU : packoffset(c33.w);

	float m_fMaxBlendRatioBetweenCascadeLevel : packoffset(c34.x);// Amount to overlap when blending between cascades.
	float m_fLogicTexelSizeInX : packoffset(c34.y);
	float m_fCascadedShadowMapTexelSizeInX : packoffset(c34.z);
//	float m_fPaddingForCB3 : packoffset(c34.w);//Padding variables exist because ConstBuffers must be a multiple of 16 bytes.
	
	float3 m_vLightDir : packoffset(c35);

	float4 m_fCascadePartitionDepthsInView_InFloat4[MAX_CASCADE_COUNT_IN_4]: packoffset(c36);//The values along Z that separate the cascades.

	float4 m_fCascadePartitionDepthsInView_OnlyX[MAX_CASCADE_COUNT_MORE]: packoffset(c38);//The values along Z that separate the cascades.//init from pixel shader
};



//--------------------------------------------------------------------------------------
// Textures and Samplers
//--------------------------------------------------------------------------------------
Texture2D g_txDiffuse:register(t0);
Texture2D<float> g_txShadow:register(t5);

SamplerState g_SamLinear:register(s0);
SamplerComparisonState g_SamplerComparisonState:register(s5);

//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
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
	float fDepthInWorldView:TEXCOORD3;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VSMain( VS_INPUT Input )
{
	VS_OUTPUT Output;
	
	Output.vPosition  = mul(Input.vPositionL,m_mWorldViewProjection);
	Output.vNormal = mul(Input.vNormal,(float3x3)m_mWorld);
	Output.vTexCoord = Input.vTexCoord;
	Output.vInterpPos = Input.vPositionL;
	Output.fDepthInWorldView = mul(Input.vPositionL,m_mWorldView).z;
	
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

void TranformShadowToTexture3D(in float4 vPosInShadowView, in int iCascadeIndex, out float4 vShadowMap_InTargetTextureCoord3D)
{
	vShadowMap_InTargetTextureCoord3D = vPosInShadowView * m_vScaleFactorFromOrthoProjToTexureCoord[iCascadeIndex];
	vShadowMap_InTargetTextureCoord3D += m_vOffsetFactorFromShadowViewToTexure[iCascadeIndex];
}



/*
（1）注意这里有N个层级，理论上有对应的N张ShadowMap，但是合成了一张真实的ShadowTexture,
也就是说如果ShadowMap的分辨率是1024X1024，则以x为平铺方向递增，合成ShadowMap分辨率为(N*1024)X1024，
因此可以看到计算ShadowMap采样坐标的代
*/
void TransformLogicU_ToNativeU(in int iCascadeIndex, in out float4 vShadowTexCoord)
{
	//换算至当前Cascade对应的真实ShadowTexure X轴
	// x*i+x/length
	//下面的运算就是为了性能把公式拆写开了

	vShadowTexCoord.x *= m_fWidthPerShadowTextureLevel_InU;// precomputed (float) iCascadeIndex/(float)CASCADE_COUNT_FLAG
													   //累加iCascadeIndex 所在的偏移基址：m_fWidthPerShadowTextureLevel_InU*(float)iCascadeIndex
	vShadowTexCoord.x += (m_fWidthPerShadowTextureLevel_InU*(float)iCascadeIndex);
}


//--------------------------------------------------------------------------------------
// This function calculates the screen space depth for shadow space texels
//--------------------------------------------------------------------------------------
void CalculateRightAndUpTexelDepthDeltas(in float3 vOrthoTexDDX, in float3 vOrthoTexDDY,
										out float fUpTexDepthWeight,out float fRightTexDepthWeight)
{
	//vOrthoTexDDX 偏导数是基于Pos3InShadowView的变化率，而ShadowView的坐标经过m_vScaleFactorFromOrthoProjToTexureCoord变化，经历了正交proj和到Texture空间的变化，变成正交投影纹理的变化率

		//we use derivatives in X and Y to create a transformation matrix.Because these derivatives give us the 
	// transformation from screen space to shadow space,we need the inverse matrix to take us from shadow space
	// to screen space.This new matrix will allow us to map shadow map texels to screen space. This will allow
	// us to find the screen space depth of a corresponding depth pixel.
	// This is not a perfect solution as it assumes the underlying geometry of the scene is a plane.A more
	// accurate way of finding the actual depth would be to do a deferred rendering approach and actually sample the depth
	
	// Using an offset,or using variance shadow maps is a better approach to reducing these artifacts in most cases.
	
	float2x2 matScreenToShadowOrtho = float2x2(vOrthoTexDDX.xy,vOrthoTexDDY.xy);
	float fDeterminant = determinant(matScreenToShadowOrtho);
	
	float fInvDeterminant = 1.0f/fDeterminant;
	
	float2x2 matShadowOrthoToScreen = float2x2(
		matScreenToShadowOrtho._22 * fInvDeterminant,matScreenToShadowOrtho._12*-fInvDeterminant,
		matScreenToShadowOrtho._21 * -fInvDeterminant,matScreenToShadowOrtho._11*fInvDeterminant); 
	
	float2 vRightShadowTexelLocation = float2(m_fLogicTexelSizeInX,0.0f);
	float2 vUpShadowTexelLocation = float2(0.0f,m_fLogicTexelSizeInX);
	
	//Transform the right pixel by the shadow space to screen matrix
	float2 vRightTexelDepthRatio = mul(vRightShadowTexelLocation,matShadowOrthoToScreen);
	float2 vUpTexelDepthRatio = mul(vUpShadowTexelLocation,matShadowOrthoToScreen);
	
	//We can now calculate how much depth changes when you move up or right in the shadow map.
	// We use the ratio of change in x and y times the derivative in X and Y of the screen space
	// depth to calculate this change
	fUpTexDepthWeight = vUpTexelDepthRatio.x*vOrthoTexDDX.z+vUpTexelDepthRatio.y*vOrthoTexDDY.z;
	fRightTexDepthWeight = vRightTexelDepthRatio.x* vOrthoTexDDX.z + vRightTexelDepthRatio.y*vOrthoTexDDY.z;
	
	
};


//--------------------------------------------------------------------------------------
// Use PCF to sample the depth map and return a percent lit value.
//--------------------------------------------------------------------------------------
void CalculatePCFPercentLit(in float4 vShadowTexCoord,
in float fRightTexelDepthDelta,
in float fUpTexelDepthDelta,
						in float fBlurRowSize,out float fPercentLit)
{
    fPercentLit = 0.0f;//jingz 阴影系数，计算满足阴影深度值的点累计平均的系数，最后用来做全阴影和全光照之间插值的系数

    // This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
    // This would be performance improvement.
    for( int x = m_iPCFBlurForLoopStart; x < m_iPCFBlurForLoopEnd; ++x ) 
    {
        for( int y = m_iPCFBlurForLoopStart; y < m_iPCFBlurForLoopEnd; ++y ) 
        {
            float depthCompare = vShadowTexCoord.z;
            // A very simple solution to the depth bias problems of PCF is to use an offset.
            // Unfortunately, too much offset can lead to Peter-panning (shadows near the base of object disappear )
            // Too little offset can lead to shadow acne ( objects that should not be in shadow are partially self shadowed ).
            depthCompare -= m_fPCFShadowDepthBiaFromGUI;

            if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
            {
               //Add in derivative computed depth scale based on the x and y pixel/
               depthCompare += fRightTexelDepthDelta * ((float)x) + fUpTexelDepthDelta * ((float)y);
            }

            // Compare the transformed pixel depth to the depth read from the map.
            float2 uv = float2(vShadowTexCoord.x + ((float)x)*m_fCascadedShadowMapTexelSizeInX,vShadowTexCoord.y + ((float)y)*m_fLogicTexelSizeInX);
            
            fPercentLit += g_txShadow.SampleCmpLevelZero( g_SamplerComparisonState, uv, depthCompare );
        }
    }
    fPercentLit /= (float)fBlurRowSize;
}

//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
//计算一个像素在区间的比例，为什么弄得这么复杂是为什么
//影子区间是从上一个下标到当前iCurrentCascadeIndex为当前区间
void CalculateBlendAmountForInterval(in int iCurrentCascadeIndex,
	in float fPixelDepthInWorldView, in out float fRatioCurrentPixelsBlendBandLocationToUpInterval,in out float fBlendRatioBetweenCascadeLevel)
{
	int temp_iCurrentCascadeIndex = iCurrentCascadeIndex + 1;

	int fBlendIntervalBelowIndex = min(1, temp_iCurrentCascadeIndex - 1);
	float fDiffPixelDepthInWorldView = fPixelDepthInWorldView - m_fCascadePartitionDepthsInView_OnlyX[fBlendIntervalBelowIndex].x;

	float fBlendInterval = m_fCascadePartitionDepthsInView_OnlyX[temp_iCurrentCascadeIndex].x - m_fCascadePartitionDepthsInView_OnlyX[fBlendIntervalBelowIndex].x;;

	fRatioCurrentPixelsBlendBandLocationToUpInterval = 1.0f - fDiffPixelDepthInWorldView / fBlendInterval;

	fBlendRatioBetweenCascadeLevel = fRatioCurrentPixelsBlendBandLocationToUpInterval / m_fMaxBlendRatioBetweenCascadeLevel;
}

//void CalculateBlendAmountForInterval(in int iCurrentCascadeIndex,
//	in float fPixelDepthInWorldView, in out int iPreCascadeIndex, in out int iNextCascadeIndex, in out float fBlendRatioBetweenCascadeLevel)
//{
//	int temp_iCurrentCascadeIndex = iCurrentCascadeIndex + 1;
//	iPreCascadeIndex = temp_iCurrentCascadeIndex - 1;
//	iNextCascadeIndex = min(CASCADE_COUNT_FLAG, temp_iCurrentCascadeIndex + 1);
//
//
//	float fDifferCurPixelDepthToBelowInterval = fPixelDepthInWorldView - m_fCascadePartitionDepthsInView_OnlyX[iPreCascadeIndex].x;
//	float fDepthIntervalSize = m_fCascadePartitionDepthsInView_OnlyX[temp_iCurrentCascadeIndex].x - m_fCascadePartitionDepthsInView_OnlyX[iPreCascadeIndex].x;
//
//	float fRatioCurrentPixelsBlendBandLocationToPrevInterval = fDifferCurPixelDepthToBelowInterval / fDepthIntervalSize;
//
//
//	iPreCascadeIndex = max(0, iPreCascadeIndex - 1);
//	iNextCascadeIndex = iNextCascadeIndex - 1;
//
//	fBlendRatioBetweenCascadeLevel = fRatioCurrentPixelsBlendBandLocationToPrevInterval / m_fMaxBlendRatioBetweenCascadeLevel;
//}

//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForMap ( in float4 vShadowMap_TargetTextureCoord, 
                                  in out float fCurrentPixelsBlendRatioBandLocation,
                                  out float fBlendRatioBetweenCascadeLevel ) 
{
    // Calcaulte the blend band for the map based selection.
    float2 distanceToOne = float2 ( 1.0f - vShadowMap_TargetTextureCoord.x, 1.0f - vShadowMap_TargetTextureCoord.y );
    fCurrentPixelsBlendRatioBandLocation = min( vShadowMap_TargetTextureCoord.x, vShadowMap_TargetTextureCoord.y );
    float fCurrentPixelsBlendRatioBandLocation2 = min( distanceToOne.x, distanceToOne.y );
    fCurrentPixelsBlendRatioBandLocation = 
        min( fCurrentPixelsBlendRatioBandLocation, fCurrentPixelsBlendRatioBandLocation2 );
	fBlendRatioBetweenCascadeLevel = fCurrentPixelsBlendRatioBandLocation/m_fMaxBlendRatioBetweenCascadeLevel;
}

//--------------------------------------------------------------------------------------
// Calculate the shadow based on several options and render the scene.
//--------------------------------------------------------------------------------------
float4 PSMain( VS_OUTPUT Input ) : SV_TARGET
{
	float4 vDiffuse = g_txDiffuse.Sample(g_SamLinear,Input.vTexCoord);
	//UVW,W表示当前UV对应的深度值
	float4 vShadowMap_InTargetTextureCoord3D = float4(0.0f,0.0f,0.0f,0.0f);
	float4 vShadowMap_InTargetTextureCoord3D_NextLevel = float4(0.0f,0.f,0.f,0.f);

	float fPercentLit_CurLevel = 0.0f;
	float fPercentLit_NextLevel = 1.0f;

	float fUpTexDepthWeight = 0;
	float fRightTexDepthWeight = 0;

	int iBlurRowSize = m_iPCFBlurForLoopEnd-m_iPCFBlurForLoopStart;
	float fBlurRowSize = (float)(iBlurRowSize*iBlurRowSize);

	// The interval based selection technique compares the pixel's depth against the frustum's cascade divisions.
	float fCurrentPixelDepthInWorldView = Input.fDepthInWorldView;

	// This for loop is not necessary when the frustum is uniformly divided and interval based selection is used.
	// In this case fCurrentPixelDepthInWorldView could be used as an array lookup into the correct frustum.
	int iCurrentCascadeIndex = 0;
	int iNextCascadeIndex = 1;

	//
	//寻找当前像素的合适阴影层级下标

	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		if(CASCADE_COUNT_FLAG > 1)
		{
			float4 vCurrentPixelDepth = float4(Input.fDepthInWorldView, Input.fDepthInWorldView, Input.fDepthInWorldView, Input.fDepthInWorldView);

			//jingz 满足大下标层级肯定满足深度数值一定大于近处低下标层级的深度值
			//以递进的形式累计层级下标，dot用来实现不同分量对应的层级控制效果。参考上面的说法，一旦高层级阴影是开启且符合深度最小要求，则必累计1x1=1下标

			float4 fComparison = (vCurrentPixelDepth > m_fCascadePartitionDepthsInView_InFloat4[0]);
			float4 fComparison2 = (vCurrentPixelDepth > m_fCascadePartitionDepthsInView_InFloat4[1]);
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
	else
	{
		int bCascadeFound = 0;
		for(int iCascadeIndex = 0;iCascadeIndex<CASCADE_COUNT_FLAG&& bCascadeFound==0;++iCascadeIndex)
		{
			TranformShadowToTexture3D(Input.vPosInShadowView, iCascadeIndex, vShadowMap_InTargetTextureCoord3D);
				
			if( min(vShadowMap_InTargetTextureCoord3D.x,vShadowMap_InTargetTextureCoord3D.y) > m_fMinBorderPaddingInShadowUV
				&& max(vShadowMap_InTargetTextureCoord3D.x,vShadowMap_InTargetTextureCoord3D.y) < m_fMaxBorderPaddingInShadowUV)
			{
				iCurrentCascadeIndex = iCascadeIndex;//jingz 最先找到的满足于层级xy裁剪的下标
				bCascadeFound = 1;
			}
		}
	}

	float fCurrentPixelsBlendRatioBandLocation = 1.0f;//band是带，归档的意思。将当前像素和高区间的差值归一化至最大混合比例上
	float fBlendRatioBetweenCascadeLevel = 1.0f;

	



	if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
	{
		// Repeat text coord calculations for the next cascade. 
		// The next cascade index is used for blurring between maps.
		iNextCascadeIndex = min(CASCADE_COUNT_FLAG - 1, iCurrentCascadeIndex + 1);

		if (SELECT_CASCADE_BY_INTERVAL_FLAG&CASCADE_COUNT_FLAG > 1)
		{
			CalculateBlendAmountForInterval(iCurrentCascadeIndex,
				fCurrentPixelDepthInWorldView, fCurrentPixelsBlendRatioBandLocation, fBlendRatioBetweenCascadeLevel);
		}
		else
		{
			CalculateBlendAmountForMap(vShadowMap_InTargetTextureCoord3D, fCurrentPixelsBlendRatioBandLocation, fBlendRatioBetweenCascadeLevel);
		}
	}
	
	float3 vOrthoUV_InTexCoordDDX;
	float3 vOrthoUV_InTexCoordDDY;
	// The derivative are used to find the slope of the current plane
	// The derivative calculation has to be inside of the loop in order to prevent divergent flow control artifacts.
	if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
	{
		float tempOrtho3D_DDX = ddx(Input.vPosInShadowView);
		float tempOrtho3D_DDY = ddy(Input.vPosInShadowView);
		
		vOrthoUV_InTexCoordDDX = tempOrtho3D_DDX * m_vScaleFactorFromOrthoProjToTexureCoord[iCurrentCascadeIndex];
		vOrthoUV_InTexCoordDDY = tempOrtho3D_DDY * m_vScaleFactorFromOrthoProjToTexureCoord[iCurrentCascadeIndex];

		CalculateRightAndUpTexelDepthDeltas(vOrthoUV_InTexCoordDDX, vOrthoUV_InTexCoordDDY, fUpTexDepthWeight, fRightTexDepthWeight);
	}

	//     Now that we know the correct map, we can transform the world space position of the current fragment                
	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		TranformShadowToTexture3D(Input.vPosInShadowView, iCurrentCascadeIndex, vShadowMap_InTargetTextureCoord3D);
	}

	TransformLogicU_ToNativeU(iCurrentCascadeIndex, vShadowMap_InTargetTextureCoord3D);

	
	//jingz 拿到深度buffer混合成的shadowMap的UVW坐标，采样对比w（即深度），用来计算类似于AO的遮挡比例
	CalculatePCFPercentLit(vShadowMap_InTargetTextureCoord3D,fRightTexDepthWeight,fUpTexDepthWeight,fBlurRowSize,fPercentLit_CurLevel);
	
	if(BLEND_BETWEEN_CASCADE_LAYERS_FLAG && CASCADE_COUNT_FLAG > 1)
	{
		if (fCurrentPixelsBlendRatioBandLocation < m_fMaxBlendRatioBetweenCascadeLevel)
		{
			//Next
			TranformShadowToTexture3D(Input.vPosInShadowView, iNextCascadeIndex, vShadowMap_InTargetTextureCoord3D_NextLevel);
			TransformLogicU_ToNativeU(iNextCascadeIndex, vShadowMap_InTargetTextureCoord3D_NextLevel);
			CalculatePCFPercentLit(saturate(vShadowMap_InTargetTextureCoord3D_NextLevel), fRightTexDepthWeight, fUpTexDepthWeight, fBlurRowSize, fPercentLit_NextLevel);
					
			fPercentLit_CurLevel = lerp(fPercentLit_NextLevel, fPercentLit_CurLevel, fBlendRatioBetweenCascadeLevel);
		}

	}
	

	float4 vVisualizeCascadeColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
	if (m_iIsVisualizeCascades != 0)
	{
		vVisualizeCascadeColor = vCascadeColors_DEBUG[iCurrentCascadeIndex];
	}
	else
	{
		vVisualizeCascadeColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
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
	fLighting = lerp(vShadowLighting,fLighting,fPercentLit_CurLevel);
	
	return fLighting* vVisualizeCascadeColor*vDiffuse;
};


