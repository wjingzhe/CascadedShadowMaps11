#include "../DXUT/Core/DXUT.h"
#include "ShadowSampleMisc.h"
#include "../DXUT/Optional/SDKmisc.h"
#include <d3d10misc.h>
#include <d3d11.h>


// Find and compile the specified shader
HRESULT CompileShaderFromFile(WCHAR* szFileName, D3D_SHADER_MACRO * macros, LPCSTR szEntryPoint,
	LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	//find the file
	WCHAR str[MAX_PATH];
	V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, szFileName));

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG) || defined(DEBUG)
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience,but still allows
	// the shaders to be optimized and to run exactly the way they will run in
	// the release configutation of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DCompileFromFile(str, macros, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);

	if (FAILED(hr))
	{
		if (pErrorBlob!=nullptr)
		{
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		}
		SAFE_RELEASE(pErrorBlob);
		return hr;
	}

	SAFE_RELEASE(pErrorBlob);

	return S_OK;
}
