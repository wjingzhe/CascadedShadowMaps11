//-----------------------------------
// File: RenderCascadeShadow.hlsl
//
// Copyright (c) Microsoft Corporation.All right reserved.
//--------------------------------------

//--------------
// Globals
//-------------
cbuffer cbPerObject:register(b0)
{
	matrix g_mViewProjection:packoffset(c0);
};

//----------------
// Input / Output structure
// ----------------
struct VS_INPUT
{
	float4 vPositionW:POSITION;
};

struct VS_OUTPUT
{
	float4 vPosition:SV_POSITION;
};

//--------------------------------
// Vertex Shader
//----------------------------
VS_OUTPUT VSMain(VS_INPUT Input)
{
	VS_OUTPUT Output;
	
	// There is nothing special here,just transform and write out the depth.
	Output.vPosition = mul(Input.vPositionW,g_mViewProjection);
	
	return Output;
}

VS_OUTPUT VSMainPancake(VS_INPUT Input)
{
	VS_OUTPUT Output;
	
	//after transform move clipped geometry to near plane
	Output.vPosition = mul(Input.vPositionW,g_mViewProjection);
	//Output.vPosition.z = max(Output.vPosition.z,0.0f);
	return Output;
}