// CascadedShadowMaps11.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "Resource.h"
#include "ShadowSampleMisc.h"
#include "CascadedShadowsManager.h"
#include "CascadedShadowMaps11.h"
#include <commdlg.h>
#include "WaitDlg.h"

using namespace DirectX;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name



CascadedShadowsManager g_CascadedShadow;

CDXUTDialogResourceManager g_DialogReourceManager; // Manager for shared resources of dialogs
CFirstPersonCamera g_ViewerCamera;
CFirstPersonCamera g_LightCamera;
CFirstPersonCamera* g_pActiveCamera = &g_ViewerCamera;

CascadeConfig g_CascadeConfig;
CDXUTSDKMesh g_MeshPowerPlant;
CDXUTSDKMesh g_MeshTestScene;
CDXUTSDKMesh* g_pSelectedMesh;

// This enum is used to allow the user to select the number of cascades in the scene.
enum CASCADE_LEVELS
{
	L1COMBO,
	L2COMBO,
	L3COMBO,
	L4COMBO,
	L5COMBO,
	L6COMBO,
	L7COMBO,
	L8COMBO
};

// DXUT GUI stuff
CDXUTComboBox* g_DepthBufferFormatCombo;
CDXUTComboBox* g_ShadowBufferTypeCombo;
CDXUTComboBox* g_CascadeLevelsCombo;
CDXUTComboBox* g_CameraSelectCombo;
CDXUTComboBox* g_SceneSelectCombo;
CDXUTComboBox* g_FitToCascadesCombo;
CDXUTComboBox* g_FitToNearFarCombo;
CDXUTComboBox* g_CascadeSelectionCombo;
CD3DSettingsDlg g_D3DSettingDlg;//Device setting dialog
CDXUTDialog g_HUD; //manages the 3D
CDXUTDialog g_SampleUI;// dialog for sample specific controls
CDXUTTextHelper* g_pTextHelper = nullptr;


DirectX::XMMATRIX g_mCenterMesh;
INT g_nNumActiveLights;
INT g_nActiveLight;
bool g_bShowHelp = FALSE; // if true,it renders the UI control text
bool g_bVisualizeCascades = FALSE;
bool g_bMoveLightTexelSize = TRUE;
FLOAT g_fApsectRatio = 1.0f;
FLOAT g_fDepthMin;
FLOAT g_fDepthMax;
FLOAT g_fDepthScale;

//-----------
// UI control IDs
//-----------------

enum IDC_ID
{
	IDC_TOGGLE_FULL_SCREEN = 1,
	IDC_TOGGLE_WARP = 2,
	IDC_CHANGE_DEVICE = 3,

	IDC_TOGGLE_VISUALIZE_CASCADES = 4,
	IDC_DEPTH_BUFFER_FORMAT = 5,

	IDC_BUFFER_SIZE = 6,
	IDC_BUFFER_SIZE_TEXT = 7,
	IDC_SELECTED_CAMERA = 8,
	IDC_SELECTED_SCENE = 9,

	IDC_CASCADE_LEVELS = 10,
	IDC_CASCADE_LEVEL1 = 11,
	IDC_CASCADE_LEVEL2 = 12,
	IDC_CASCADE_LEVEL3 = 13,
	IDC_CASCADE_LEVEL4 = 14,
	IDC_CASCADE_LEVEL5 = 15,
	IDC_CASCADE_LEVEL6 = 16,
	IDC_CASCADE_LEVEL7 = 17,
	IDC_CASCADE_LEVEL8 = 18,

	IDC_CASCADE_LEVEL1TEXT = 19,
	IDC_CASCADE_LEVEL2TEXT = 20,
	IDC_CASCADE_LEVEL3TEXT = 21,
	IDC_CASCADE_LEVEL4TEXT = 22,
	IDC_CASCADE_LEVEL5TEXT = 23,
	IDC_CASCADE_LEVEL6TEXT = 24,
	IDC_CASCADE_LEVEL7TEXT = 25,
	IDC_CASCADE_LEVEL8TEXT = 26,

	IDC_MOVE_LIGHT_IN_TEXEL_INC = 27,
	IDC_FIT_TO_CASCADE = 28,
	IDC_FIT_TO_NEAR_FAR = 29,
	IDC_CASCADE_SELECT = 30,
	IDC_PCF_SIZE = 31,
	IDC_PCF_SIZETEXT = 32,
	IDC_TOGGLE_DERIVATIVE_OFFSET = 33,
	IDC_PCF_OFFSET_SIZE = 34,
	IDC_PCF_OFFSET_SIZE_TEXT = 35,

	IDC_BLEND_BETWEEN_MAPS_CHECK = 36,
	IDC_BLEND_MAPS_SLIDER = 37,
};

//#define IDC_TOGGLE_FULL_SCREEN 1
//#define IDC_TOGGLE_WARP 2
//#define IDC_CHANGE_DEVICE 3
//
//#define IDC_TOGGLE_VISUALIZE_CASCADES 4
//#define IDC_DEPTH_BUFFER_FORMAT 5
//
//#define IDC_BUFFER_SIZE 6
//#define IDC_BUFFER_SIZE_TEXT 7
//#define IDC_SELECTED_CAMERA 8
//#define	IDC_SELECTED_SCENE 9
//
//#define IDC_CASCADE_LEVELS 10
//#define IDC_CASCADE_LEVEL1 11
//#define IDC_CASCADE_LEVEL2 12
//#define IDC_CASCADE_LEVEL3 13
//#define IDC_CASCADE_LEVEL4 14
//#define IDC_CASCADE_LEVEL5 15
//#define IDC_CASCADE_LEVEL6 16
//#define IDC_CASCADE_LEVEL7 17
//#define IDC_CASCADE_LEVEL8 18
//
//#define IDC_CASCADE_LEVEL1TEXT 19
//#define IDC_CASCADE_LEVEL2TEXT 20
//#define IDC_CASCADE_LEVEL3TEXT 21
//#define IDC_CASCADE_LEVEL4TEXT 22
//#define IDC_CASCADE_LEVEL5TEXT 23
//#define IDC_CASCADE_LEVEL6TEXT 24
//#define IDC_CASCADE_LEVEL7TEXT 25
//#define IDC_CASCADE_LEVEL8TEXT 26
//
//#define IDC_MOVE_LIGHT_IN_TEXEL_INC 27
//#define IDC_FIT_TO_CASCADE 28
//#define IDC_FIT_TO_NEAR_FAR 29
//#define IDC_CASCADE_SELECT 30
//#define IDC_PCF_SIZE 31
//#define IDC_PCF_SIZETEXT 32
//#define IDC_TOGGLE_DERIVATIVE_OFFSET 33
//#define IDC_PCF_OFFSET_SIZE 34
//#define IDC_PCF_OFFSET_SIZE_TEXT 35
//
//#define IDC_BLEND_BETWEEN_MAPS_CHECK 36
//#define IDC_BLEND_MAPS_SLIDER 37

//--------------
// Forward declarations of functions included in this code module:
//-------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);
void CALLBACK OnFrameMove(double fTime, FLOAT fElapsedTime, void* pUserContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext);
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool AltDown, void* pUserContext);
void CALLBACK OnGUIEvent(UINT EVENT, INT nControlID, CDXUTControl* pControl, void* pUserContext);
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdpaterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device*pD3DDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pD3DDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);

void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);
void CALLBACK OnD3D11DestroyDevice(void* pUserContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* pD3DDevice, ID3D11DeviceContext* pD3DImmediateContext, double fTime, FLOAT fElapsedTime, void* pUserContext);

void InitApp();
void RenderText();
HRESULT DestroyD3DComponents();
HRESULT CreateD3DComponents(ID3D11Device* pD3DDevice);
void UpdateViewerCameraNearFar();


ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CASCADEDSHADOWMAPS11, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

   

	// Set DXUT callbacks
	DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
	DXUTSetCallbackMsgProc(MsgProc);
	DXUTSetCallbackKeyboard(OnKeyboard);
	DXUTSetCallbackFrameMove(OnFrameMove);

	DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
	DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
	DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
	DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
	InitApp();

	DXUTInit(true, true, nullptr);//Parse the command line ,show messageBoxes on error,no extra command line params

	DXUTSetCursorSettings(true, true);// Show the cursor and clip it when in full screen
	DXUTCreateWindow(szTitle, hInstance);
	CWaitDlg CompilingShadersDlg;
	if (DXUT_EnsureD3D11APIs())
	{
		CompilingShadersDlg.ShowDialog(L"Compiling Shaders and loading models.");
	}

	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 800, 600);

	CompilingShadersDlg.DestroyDialog();

	DXUTMainLoop();

    return DXUTGetExitCode();
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CASCADEDSHADOWMAPS11));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CASCADEDSHADOWMAPS11);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//Called right before creating a D3D11 device ,allowing the app to modify the device settings as needed
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings * pDeviceSettings, void * pUserContext)
{
	//For the first device created if its a REF device,optionally display a warning dialog box
	static BOOL s_bFirstTime = true;

	if (s_bFirstTime)
	{
		s_bFirstTime = FALSE;
		if (pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE)
		{
			DXUTDisplaySwitchingToREFWarning();
		}

	}

	return true;
}

void CALLBACK OnFrameMove(double fTime, FLOAT fElapsedTime, void * pUserContext)
{
	g_LightCamera.FrameMove(fElapsedTime);
	g_ViewerCamera.FrameMove(fElapsedTime);
}

LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool * pbNoFurtherProcessing, void * pUserContext)
{
	// Pass message to dialog resource manager calls so GUI state is updated correctly
	*pbNoFurtherProcessing = g_DialogReourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
	if (*pbNoFurtherProcessing)
	{
		return 0;
	}

	// Pass message to settings dialog if its active
	if (g_D3DSettingDlg.IsActive())
	{
		g_D3DSettingDlg.MsgProc(hWnd, uMsg, wParam, lParam);
		return 0;
	}

	// Give the dialogs a change to handle the message first
	*pbNoFurtherProcessing = g_HUD.MsgProc(hWnd, uMsg, wParam, lParam);
	if (*pbNoFurtherProcessing)
	{
		return 0;
	}

	*pbNoFurtherProcessing = g_SampleUI.MsgProc(hWnd, uMsg, wParam, lParam);
	if (*pbNoFurtherProcessing)
	{
		return 0;
	}

	// pass all remaining windows messages to camera so it can respond to user input
	g_pActiveCamera->HandleMessages(hWnd, uMsg, wParam, lParam);

	return 0;
}

void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool AltDown, void * pUserContext)
{
	if (bKeyDown)
	{
		switch (nChar)
		{
		case VK_F1:
			g_bShowHelp = !g_bShowHelp;
			break;
		}
	}
}

void CALLBACK OnGUIEvent(UINT EVENT, INT nControlID, CDXUTControl * pControl, void * pUserContext)
{
	IDC_ID ControlID = (IDC_ID)nControlID;
	switch (ControlID)
	{
	case IDC_TOGGLE_FULL_SCREEN:
		DXUTToggleFullScreen();
		break;
	case IDC_TOGGLE_WARP:
		DXUTToggleWARP();
		break;
	case IDC_CHANGE_DEVICE:
		g_D3DSettingDlg.SetActive(!g_D3DSettingDlg.IsActive());
		break;
	case IDC_TOGGLE_VISUALIZE_CASCADES:
		g_bVisualizeCascades = !g_bVisualizeCascades;
		break;
	case IDC_DEPTH_BUFFER_FORMAT:
	{
		SHADOW_TEXTURE_FORMAT temp = (SHADOW_TEXTURE_FORMAT)PtrToUlong(g_DepthBufferFormatCombo->GetSelectedData());
		g_CascadeConfig.m_ShadowBufferFormat = temp;
	}
		break;
	case IDC_BUFFER_SIZE:
	{
		INT value = 32 * g_HUD.GetSlider(nControlID)->GetValue();
		INT max = 8192 / g_CascadeConfig.m_nCascadeLevels;
		if (value > max)
		{
			value = max;
			g_HUD.GetSlider(nControlID)->SetValue(value / 32);
		}

		WCHAR desc[256];
		swprintf_s(desc, L"Texture Size: %d", value);
		g_HUD.GetStatic(IDC_BUFFER_SIZE_TEXT)->SetText(desc);

		//Only tell the app to recreate buffers once the user is through moving slider.
		if (EVENT == EVENT_SLIDER_VALUE_CHANGED_UP)
		{
			g_CascadeConfig.m_iBufferSize = value;
		}
	}
		break;
	case IDC_BUFFER_SIZE_TEXT:
		break;
	case IDC_SELECTED_CAMERA:
	{
		g_CascadedShadow.m_eSelectedCamera = (CAMERA_SELECTION)(g_CascadeSelectionCombo->GetSelectedIndex());

		if (g_CascadedShadow.m_eSelectedCamera< LIGHT_CAMERA)
		{
			g_pActiveCamera = &g_ViewerCamera;
		}
		else
		{
			g_pActiveCamera = &g_LightCamera;
		}
	}
		break;
	case IDC_SELECTED_SCENE:
	{
		SCENE_SELECTION ss = (SCENE_SELECTION)PtrToUlong(g_SceneSelectCombo->GetSelectedData());
		if (ss == POWER_PLANT_SCENE)
		{
			g_pSelectedMesh = &g_MeshPowerPlant;
		}
		else if ( ss == TEST_SCENE)
		{
			g_pSelectedMesh = &g_MeshTestScene;
		}

		DestroyD3DComponents();
		CreateD3DComponents(DXUTGetD3D11Device());
		UpdateViewerCameraNearFar();

	}
		break;
	case IDC_CASCADE_LEVELS:
	{
		INT selectedIndex = 1 + g_CascadeSelectionCombo->GetSelectedIndex();
		g_CascadeConfig.m_nCascadeLevels = selectedIndex;
		for (int i = 0;i<selectedIndex;++selectedIndex)
		{
			g_HUD.GetStatic(IDC_CASCADE_LEVEL1TEXT + i)->SetVisible(true);
			g_HUD.GetSlider(IDC_CASCADE_LEVEL1 + i)->SetVisible(true);
		}
		for (int i = selectedIndex;i<MAX_CASCADES;++i)
		{
			g_HUD.GetStatic(IDC_CASCADE_LEVEL1TEXT + i)->SetVisible(false);
			g_HUD.GetSlider(IDC_CASCADE_LEVEL1 + i)->SetVisible(false);
		}
		INT value = 32 * g_HUD.GetSlider(IDC_BUFFER_SIZE)->GetValue();
		INT max = 8192 / g_CascadeConfig.m_nCascadeLevels;
		if (value>max)
		{
			value = max;

			WCHAR desc[256];

			swprintf_s(desc, L"Texture Size: %d", value);
			g_HUD.GetStatic(IDC_BUFFER_SIZE_TEXT)->SetText(desc);
			g_HUD.GetSlider(IDC_BUFFER_SIZE)->SetValue(value / 32);
			g_CascadeConfig.m_iBufferSize = value;
		}

		// update the selected camera based on these changes.
		INT newSelectedIndex = g_CameraSelectCombo->GetSelectedIndex();
		WCHAR desc[60];
		g_CameraSelectCombo->RemoveAllItems();
		swprintf_s(desc, L"Eye Camera %d", EYE_CAMERA + 1);
		g_CameraSelectCombo->AddItem(desc, UlongToPtr(EYE_CAMERA));
		swprintf_s(desc, L"Light Camera %d", LIGHT_CAMERA + 1);
		g_CameraSelectCombo->AddItem(desc, UlongToPtr(LIGHT_CAMERA));
		for (int i = 0;i<g_CascadeConfig.m_nCascadeLevels;++i)
		{
			swprintf_s(desc, L"Cascade Cam %d", i + 1);
			g_CascadeSelectionCombo->AddItem(desc, UlongToPtr(ORTHO_CAMERA1 + i));
		}
		if (newSelectedIndex - 1 >=selectedIndex)
		{
			newSelectedIndex = selectedIndex + 1;
		}
		g_CameraSelectCombo->SetSelectedByIndex(newSelectedIndex);

		g_CascadedShadow.m_eSelectedCamera = (CAMERA_SELECTION)(g_CascadeSelectionCombo->GetSelectedIndex());

		if (g_CascadedShadow.m_eSelectedCamera < 1)
		{
			g_pActiveCamera = &g_ViewerCamera;
		}
		else
		{
			g_pActiveCamera = &g_LightCamera;
		}
			
	}
		break;
	case IDC_CASCADE_LEVEL1:
	case IDC_CASCADE_LEVEL2:
	case IDC_CASCADE_LEVEL3:
	case IDC_CASCADE_LEVEL4:
	case IDC_CASCADE_LEVEL5:
	case IDC_CASCADE_LEVEL6:
	case IDC_CASCADE_LEVEL7:
	case IDC_CASCADE_LEVEL8:
	{
		INT index = nControlID - IDC_CASCADE_LEVEL1;
		INT move = g_HUD.GetSlider(nControlID)->GetValue();
		CDXUTSlider* selectedSlider;
		CDXUTStatic* selectedStatic;
		WCHAR label[16];
		for (int i = 0;i<index;++i)
		{
			selectedSlider = g_HUD.GetSlider(IDC_CASCADE_LEVEL1 + i);
			INT sVal = selectedSlider->GetValue();
			if (move <sVal)
			{
				selectedSlider->SetValue(move);
				selectedStatic = g_HUD.GetStatic(IDC_CASCADE_LEVEL1 + i);
				swprintf_s(label, L"L%d: %d", i + 1, move);
				selectedStatic->SetText(label);
				g_CascadedShadow.m_iCascadePartitionsZeroToOne[i] = move;
			}
		}

		for (int i = index;i<MAX_CASCADES;++i)
		{
			selectedSlider = g_HUD.GetSlider(IDC_CASCADE_LEVEL1 + i);
			INT sVal = selectedSlider->GetValue();
			if (move >= sVal)
			{
				selectedSlider->SetValue(move);
				selectedStatic = g_HUD.GetStatic(IDC_CASCADE_LEVEL1TEXT + i);
				swprintf_s(label, L"L%d: %d", i + 1, move);
				selectedStatic->SetText(label);
				g_CascadedShadow.m_iCascadePartitionsZeroToOne[i] = move;
			}
		}

	}
		break;
	case IDC_CASCADE_LEVEL1TEXT:
		break;
	case IDC_CASCADE_LEVEL2TEXT:
		break;
	case IDC_CASCADE_LEVEL3TEXT:
		break;
	case IDC_CASCADE_LEVEL4TEXT:
		break;
	case IDC_CASCADE_LEVEL5TEXT:
		break;
	case IDC_CASCADE_LEVEL6TEXT:
		break;
	case IDC_CASCADE_LEVEL7TEXT:
		break;
	case IDC_CASCADE_LEVEL8TEXT:
		break;
	case IDC_MOVE_LIGHT_IN_TEXEL_INC:
		g_bMoveLightTexelSize = !g_bMoveLightTexelSize;
		g_CascadedShadow.m_bMoveLightTexelSize = g_bMoveLightTexelSize;
		break;
	case IDC_FIT_TO_CASCADE:
		g_CascadedShadow.m_eSelectedCascadesFit = (FIT_PROJECTION_TO_CASCADES)PtrToUlong(g_FitToCascadesCombo->GetSelectedData());
		break;
	case IDC_FIT_TO_NEAR_FAR:
	{
		g_CascadedShadow.m_eSelectedNearFarFit = (FIT_TO_NEAR_FAR)PtrToUlong(g_FitToNearFarCombo->GetSelectedData());

		if (g_CascadedShadow.m_eSelectedNearFarFit == FIT_NEAR_FAR_PANCAKING)
		{
			g_CascadedShadow.m_eSelectedCascadeSelection = CASCADE_SELECTION_INTERVAL;
			g_CascadeSelectionCombo->SetSelectedByData(UlongToPtr(CASCADE_SELECTION_INTERVAL));
		}
	}

		break;
	case IDC_CASCADE_SELECT:
	{
		static int iSaveLastCascadeValue = 100;
		if ((CASCADE_SELECTION)PtrToUlong(g_CascadeSelectionCombo->GetSelectedData()) == CASCADE_SELECTION_MAP)
		{
			if ((FIT_TO_NEAR_FAR)PtrToUlong(g_FitToNearFarCombo->GetSelectedData()) == FIT_NEAR_FAR_PANCAKING)
			{
				g_FitToCascadesCombo->SetSelectedByData(UlongToPtr(FIT_NEAR_FAR_SCENE_AABB));
				g_CascadedShadow.m_eSelectedNearFarFit = FIT_NEAR_FAR_SCENE_AABB;
			}

			g_CascadedShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels - 1] = iSaveLastCascadeValue;

		}
		else
		{
			iSaveLastCascadeValue = g_CascadedShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels - 1];
			g_CascadedShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels - 1] = 100;
		}

		g_CascadedShadow.m_eSelectedCascadeSelection = (CASCADE_SELECTION)PtrToUlong(g_CascadeSelectionCombo->GetSelectedData());

		auto tempSlider = g_HUD.GetSlider(IDC_CASCADE_LEVEL1 + g_CascadeConfig.m_nCascadeLevels - 1);
		tempSlider->SetValue(g_CascadedShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels - 1]);

		WCHAR label[6];

		swprintf_s(label, L"L%d: %d", g_CascadeConfig.m_nCascadeLevels, g_CascadedShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels - 1]);

		g_HUD.GetStatic(IDC_CASCADE_LEVEL1TEXT + g_CascadeConfig.m_nCascadeLevels - 1)->SetText(label);
	}
		break;
	case IDC_PCF_SIZE:
	{
		INT PCFSize = g_HUD.GetSlider(IDC_PCF_SIZE)->GetValue();
		PCFSize *= 2;
		PCFSize -= 1;

		WCHAR desc[256];

		swprintf_s(desc, L"PCF Blur: %d", PCFSize);
		g_HUD.GetStatic(IDC_PCF_OFFSET_SIZE_TEXT)->SetText(desc);

		g_CascadedShadow.m_iPCFBlurSize = PCFSize;
	}
	break;
		break;
	case IDC_PCF_SIZETEXT:
		break;
	case IDC_TOGGLE_DERIVATIVE_OFFSET:
	{
		if (g_CascadedShadow.m_iDerivativeBaseOffset == 0)
		{
			g_CascadedShadow.m_iDerivativeBaseOffset = 1;
		}
		else
		{
			g_CascadedShadow.m_iDerivativeBaseOffset = 0;
		}
	}
		break;
	case IDC_PCF_OFFSET_SIZE:
	{
		INT offset = g_HUD.GetSlider(IDC_PCF_OFFSET_SIZE)->GetValue();
		FLOAT useOffset = (FLOAT)offset*0.001f;

		WCHAR desc[256];

		swprintf_s(desc, L"Offset: %0.03f", useOffset);
		g_HUD.GetStatic(IDC_PCF_OFFSET_SIZE_TEXT)->SetText(desc);
		g_CascadedShadow.m_fPCFOffset = useOffset;
	}

		break;
	case IDC_PCF_OFFSET_SIZE_TEXT:
		break;
	case IDC_BLEND_BETWEEN_MAPS_CHECK:
	{
		if (g_HUD.GetCheckBox(nControlID)->GetChecked())
		{
			g_CascadedShadow.m_iBlurBetweenCascades = 1;
		}
		else
		{
			g_CascadedShadow.m_iBlurBetweenCascades = 0;
		}
	}
		break;
	case IDC_BLEND_MAPS_SLIDER:
	{
		INT val = g_HUD.GetSlider(nControlID)->GetValue();
		g_CascadedShadow.m_fBlurBetweenCascadesAmount = (float)val*0.005f;
		WCHAR desc[256];
		swprintf_s(desc, L"Cascade Blur: %0.03f", g_CascadedShadow.m_fBlurBetweenCascadesAmount);
		g_HUD.GetStatic(IDC_PCF_OFFSET_SIZE_TEXT)->SetText(desc);
	}
		break;

	default:
		break;
	}


	
}


// Reject any D3D11 devices that aren't acceptable by returning false
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo * AdpaterInfo, UINT Output, const CD3D11EnumDeviceInfo * DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void * pUserContext)
{
	return true;
}

HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device * pD3DDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{
	HRESULT hr = S_OK;
	V_RETURN(g_MeshPowerPlant.Create(pD3DDevice, L"powerplant\\powerplant.sdkmesh"));
	V_RETURN(g_MeshTestScene.Create(pD3DDevice, L"ShadowColumns\\testscene.sdkmesh"));

	g_pSelectedMesh = &g_MeshPowerPlant;

	return CreateD3DComponents(pD3DDevice);
}

// Create any D3D11 resources that depend on the back buffer
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device * pD3DDevice, IDXGISwapChain * pSwapChain, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{
	HRESULT hr;
	V_RETURN(g_DialogReourceManager.OnD3D11ResizedSwapChain(pD3DDevice, pBackBufferSurfaceDesc));
	V_RETURN(g_D3DSettingDlg.OnD3D11ResizedSwapChain(pD3DDevice, pBackBufferSurfaceDesc));

	g_fApsectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;

	UpdateViewerCameraNearFar();

	g_HUD.SetLocation(pBackBufferSurfaceDesc->Width - 170, 0);
	g_HUD.SetSize(170, 170);
	g_SampleUI.SetLocation(pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300);
	g_SampleUI.SetSize(170, 300);

	return S_OK;
}

void CALLBACK OnD3D11ReleasingSwapChain(void * pUserContext)
{
	g_DialogReourceManager.OnD3D11ReleasingSwapChain();
}

void CALLBACK OnD3D11DestroyDevice(void * pUserContext)
{
	g_MeshPowerPlant.Destroy();
	g_MeshTestScene.Destroy();
	DestroyD3DComponents();
}

void CALLBACK OnD3D11FrameRender(ID3D11Device * pD3DDevice, ID3D11DeviceContext * pD3DImmediateContext, double fTime, FLOAT fElapsedTime, void * pUserContext)
{
	if (g_D3DSettingDlg.IsActive())
	{
		g_D3DSettingDlg.OnRender(fElapsedTime);
		return;
	}

	FLOAT ClearColor[4] = { 0.0f,0.25f,0.25f,0.55f };
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
	pD3DImmediateContext->ClearRenderTargetView(pRTV, ClearColor);
	pD3DImmediateContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	g_CascadedShadow.InitFrame(pD3DDevice, g_pSelectedMesh);

	g_CascadedShadow.RenderShadowForAllCascades(pD3DDevice, pD3DImmediateContext, g_pSelectedMesh);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
	vp.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	g_CascadedShadow.RenderScene(pD3DImmediateContext, pRTV, pDSV, g_pSelectedMesh, g_pActiveCamera, &vp, g_bVisualizeCascades);
	
	pD3DImmediateContext->RSSetViewports(1, &vp);
	pD3DImmediateContext->OMSetRenderTargets(1, &pRTV, pDSV);

	DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD/Stats");

	g_HUD.OnRender(fElapsedTime);
	g_SampleUI.OnRender(fElapsedTime);
	RenderText();

	DXUT_EndPerfEvent();

}

// Initialize the app
void InitApp()
{
	g_CascadeConfig.m_nCascadeLevels = 3;
	g_CascadeConfig.m_iBufferSize = 1024;

	g_CascadedShadow.m_iCascadePartitionsZeroToOne[0] = 5;
	g_CascadedShadow.m_iCascadePartitionsZeroToOne[1] = 15;
	g_CascadedShadow.m_iCascadePartitionsZeroToOne[2] = 60;
	g_CascadedShadow.m_iCascadePartitionsZeroToOne[3] = 100;
	g_CascadedShadow.m_iCascadePartitionsZeroToOne[4] = 100;
	g_CascadedShadow.m_iCascadePartitionsZeroToOne[5] = 100;
	g_CascadedShadow.m_iCascadePartitionsZeroToOne[6] = 100;
	g_CascadedShadow.m_iCascadePartitionsZeroToOne[7] = 100;

	//Pick some arbitrary intervals for the Cascade Maps
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[0] = 2;
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[1] = 4;
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[2] = 6;
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[3] = 9;
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[4] = 13;
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[5] = 26;
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[6] = 36;
	//g_CascadedShadow.m_iCascadePartitionsZeroToOne[7] = 70;

	g_CascadedShadow.m_iCascadePartitionMax = 100;

	//Initialize dialogs
	g_D3DSettingDlg.Init(&g_DialogReourceManager);
	g_HUD.Init(&g_DialogReourceManager);
	g_SampleUI.Init(&g_DialogReourceManager);

	g_HUD.SetCallback(OnGUIEvent);
	INT iY = 10;

	//Add tons of GUI stuff
	g_HUD.AddButton(IDC_TOGGLE_FULL_SCREEN, L"Toggle full screen", 0, iY, 170, 23);
	g_HUD.AddButton(IDC_TOGGLE_WARP, L"Toggle WARP (F3)", 0, iY += 26, 170, 23, VK_F3);
	g_HUD.AddButton(IDC_CHANGE_DEVICE, L"Change device(F2)", 0, iY += 26, 170, 23, VK_F2);
	g_HUD.AddCheckBox(IDC_TOGGLE_VISUALIZE_CASCADES, L"Visualize Cascades", 0, iY += 26, 170, 23, g_bVisualizeCascades, VK_F8);

	g_HUD.AddComboBox(IDC_DEPTH_BUFFER_FORMAT, 0, iY += 26, 170, 23, VK_F10, false, &g_DepthBufferFormatCombo);
	g_DepthBufferFormatCombo->AddItem(L"32 bit Buffer", UlongToPtr(CASCADE_DXGI_FORMAT_R32_TYPELESS));
	g_DepthBufferFormatCombo->AddItem(L"16 bit Buffer", UlongToPtr(CASCADE_DXGI_FORMAT_R16_TYPELESS));
	g_DepthBufferFormatCombo->AddItem(L"24 bit Buffer", UlongToPtr(CASCADE_DXGI_FORMAT_R24G8_TYPELESS));

	SHADOW_TEXTURE_FORMAT sbt = (SHADOW_TEXTURE_FORMAT)PtrToUlong(g_DepthBufferFormatCombo->GetSelectedData());
	g_CascadeConfig.m_ShadowBufferFormat = sbt;

	WCHAR desc[256];
	swprintf_s(desc, L"Texture Size: %d", g_CascadeConfig.m_iBufferSize);

	g_HUD.AddStatic(IDC_BUFFER_SIZE_TEXT, desc, 0, iY + 26, 30, 10);
	g_HUD.AddSlider(IDC_BUFFER_SIZE, 0, iY += 46, 128, 15, 1, 128, g_CascadeConfig.m_iBufferSize / 32);

	g_HUD.AddStatic(IDC_PCF_SIZETEXT, L"PCF Blur: 3", 0, iY + 16, 30, 10);
	g_HUD.AddSlider(IDC_PCF_SIZE, 90, iY += 20, 64, 15, 1, 16, g_CascadedShadow.m_iPCFBlurSize / 2 + 1);

	swprintf_s(desc, L" Offset: %0.03f", g_CascadedShadow.m_fPCFOffset);
	g_HUD.AddStatic(IDC_PCF_OFFSET_SIZE_TEXT, desc, 0, iY + 16, 30, 10);
	g_HUD.AddSlider(IDC_PCF_OFFSET_SIZE, 115, iY += 20, 50, 15, 0, 50, (INT)g_CascadedShadow.m_fPCFOffset*1000.0f);

	swprintf_s(desc, L"Cascade Blur % 0.03f", g_CascadedShadow.m_fBlurBetweenCascadesAmount);
	bool bValue;
	if (g_CascadedShadow.m_fBlurBetweenCascadesAmount*2000.0f)
	{
		bValue = false;
	}
	else
	{
		bValue = true;
	}

	g_HUD.AddCheckBox(IDC_BLEND_BETWEEN_MAPS_CHECK, desc, 0, iY + 15, 170, 23, bValue);
	g_HUD.AddSlider(IDC_BLEND_MAPS_SLIDER, 40, iY + 33, 100, 15, 0, 100, (INT)g_CascadedShadow.m_fBlurBetweenCascadesAmount*2000.0f);
	iY += 26;

	if (g_CascadedShadow.m_iDerivativeBaseOffset == 0)
	{
		bValue = false;
	}
	else
	{
		bValue = true;
	}
	g_HUD.AddCheckBox(IDC_TOGGLE_DERIVATIVE_OFFSET, L"DDX,DDY offset", 0, iY += 26, 170, 23, bValue);


	WCHAR data[60];

	g_HUD.AddComboBox(IDC_SELECTED_SCENE, 0, iY += 26, 170, 23, VK_F8, false, &g_SceneSelectCombo);
	g_SceneSelectCombo->AddItem(L"Power Plant", UlongToPtr(POWER_PLANT_SCENE));
	g_SceneSelectCombo->AddItem(L"Test Scene", UlongToPtr(TEST_SCENE));

	g_HUD.AddComboBox(IDC_SELECTED_CAMERA, 0, iY += 26, 170, 23, VK_F9, false, &g_CameraSelectCombo);
	swprintf_s(data, L"Eye Camera", EYE_CAMERA + 1);
	g_CameraSelectCombo->AddItem(data, UlongToPtr(LIGHT_CAMERA));
	swprintf_s(data, L"Light Camera", LIGHT_CAMERA + 1);
	g_CameraSelectCombo->AddItem(data, UlongToPtr(LIGHT_CAMERA));
	for (int index = 0;index < g_CascadeConfig.m_nCascadeLevels;++index)
	{
		swprintf_s(data, L"Cascade Cam %d", index + 1);
		g_CameraSelectCombo->AddItem(data, UlongToPtr(ORTHO_CAMERA1 + index));
	}

	g_HUD.AddCheckBox(IDC_MOVE_LIGHT_IN_TEXEL_INC, L"Fit Light to Texels", 0, iY += 26, 170, 23, g_bMoveLightTexelSize, VK_F8);
	g_CascadedShadow.m_bMoveLightTexelSize = g_bMoveLightTexelSize;
	g_HUD.AddComboBox(IDC_FIT_TO_CASCADE, 0, iY += 26, 170, 23, VK_F9, false, &g_FitToCascadesCombo);
	g_FitToCascadesCombo->AddItem(L"Fit Scene", UlongToPtr(FIT_TO_SCENE));
	g_FitToCascadesCombo->AddItem(L"Fit Cascades", UlongToPtr(FIT_TO_CASCADES));
	g_CascadedShadow.m_eSelectedCascadesFit = FIT_TO_SCENE;

	g_HUD.AddComboBox(IDC_FIT_TO_NEAR_FAR, 0, iY += 26, 170, 23, VK_F9, false, &g_FitToNearFarCombo);
	g_FitToNearFarCombo->AddItem(L"AABB/Scene NearFar", UlongToPtr(FIT_NEAR_FAR_SCENE_AABB));
	g_FitToNearFarCombo->AddItem(L"Pancaking", UlongToPtr(FIT_NEAR_FAR_PANCAKING));
	g_FitToNearFarCombo->AddItem(L"0:1 NearFar", UlongToPtr(FIT_NEAR_FAR_ZERO_ONE));
	g_FitToNearFarCombo->AddItem(L"AABB NearFar", UlongToPtr(FIT_NEAR_FAR_AABB));
	g_CascadedShadow.m_eSelectedNearFarFit = FIT_NEAR_FAR_SCENE_AABB;

	g_HUD.AddComboBox(IDC_CASCADE_SELECT, 0, iY += 26, 170, 23, VK_F9, false, &g_CascadeSelectionCombo);
	g_CascadeSelectionCombo->AddItem(L"Map Selection", ULongToPtr(CASCADE_SELECTION_MAP));
	g_CascadeSelectionCombo->AddItem(L"Interval Selection", ULongToPtr(CASCADE_SELECTION_INTERVAL));

	g_CascadedShadow.m_eSelectedCascadeSelection = CASCADE_SELECTION_MAP;

	g_HUD.AddComboBox(IDC_CASCADE_LEVELS, 0, iY += 26, 170, 23, VK_F11, false, &g_CascadeLevelsCombo);
	swprintf_s(data, L"%d Level", 1);
	g_CascadeLevelsCombo->AddItem(data, UlongToPtr(L1COMBO + 1));
	for (INT index = 1;index < MAX_CASCADES;++index)
	{
		swprintf_s(data, L"%d Levels", index + 1);
		g_CascadeLevelsCombo->AddItem(data, ULongToPtr(L1COMBO + index));
	}

	g_CascadeLevelsCombo->SetSelectedByIndex(g_CascadeConfig.m_nCascadeLevels - 1);

	INT sp = 12;
	iY += 20;
	WCHAR label[16];
	//Color the cascade labels similar to the visualization.
	D3DCOLOR tempColors[] =
	{
		0xFFFF0000,
		0xFF00FF00,
		0xFF0000FF,
		0xFFFF00FF,
		0xFFFFFF00,
		0xFFFFFFFF,
		0xFF00AAFF,
		0xFFAAFFAA
	};

	for (INT index = 0;index < MAX_CASCADES;++index)
	{
		swprintf_s(label, L"L%d: %d", (index + 1), g_CascadedShadow.m_iCascadePartitionsZeroToOne[index]);
		g_HUD.AddStatic(index + IDC_CASCADE_LEVEL1TEXT, label, 0, iY + sp, 30, 10);
		g_HUD.GetStatic(index + IDC_CASCADE_LEVEL1TEXT)->SetTextColor(tempColors[index]);
		g_HUD.AddSlider(index + IDC_CASCADE_LEVEL1, 50, iY += 15, 100, 15, 0, 100, g_CascadedShadow.m_iCascadePartitionsZeroToOne[index]);
	}

	for (INT index = 0;index<g_CascadeConfig.m_nCascadeLevels;++index)
	{
		g_HUD.GetStatic(IDC_CASCADE_LEVEL1TEXT + index)->SetVisible(true);
		g_HUD.GetSlider(IDC_CASCADE_LEVEL1 + index)->SetVisible(true);
	}

	for (int index = g_CascadeConfig.m_nCascadeLevels; index < MAX_CASCADES; ++index)
	{
		g_HUD.GetStatic(IDC_CASCADE_LEVEL1TEXT + index)->SetVisible(false);
		g_HUD.GetSlider(IDC_CASCADE_LEVEL1 + index)->SetVisible(false);
	}

	g_SampleUI.SetCallback(OnGUIEvent); iY = 10;

}

void RenderText()
{
	UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;

	g_pTextHelper->Begin();
	g_pTextHelper->SetInsertionPos(2, 0);
	g_pTextHelper->SetForegroundColor(XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));
	g_pTextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
	g_pTextHelper->DrawTextLine(DXUTGetDeviceStats());

	//Draw help
	if (g_bShowHelp)
	{
		g_pTextHelper->SetInsertionPos(2, nBackBufferHeight - 20 * 6);
		g_pTextHelper->SetForegroundColor(XMFLOAT4(1.0f, 0.75f, 0.0f, 1.0f));
		g_pTextHelper->DrawTextLine(L"Controls:");

		g_pTextHelper->SetInsertionPos(20, nBackBufferHeight - 20 * 5);
		g_pTextHelper->DrawTextLine(L"Move forward and backward with 'E' and 'D'\n"
			L"Move left and right with 'S' and 'D' \n"
			L"Click the mouse button to rotate the camera\n");

		g_pTextHelper->SetInsertionPos(350, nBackBufferHeight - 20 * 5);
		g_pTextHelper->DrawTextLine(L"Hide help:F1\n"
			L"Quit:ESC\n");

	}
	else
	{
		g_pTextHelper->SetForegroundColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		g_pTextHelper->DrawTextLine(L"Press F1 for help");
	}

	g_pTextHelper->End();
}

HRESULT DestroyD3DComponents()
{
	g_DialogReourceManager.OnD3D11DestroyDevice();
	g_D3DSettingDlg.OnD3D11DestroyDevice();
	DXUTGetGlobalResourceCache().OnDestroyDevice();
	SAFE_DELETE(g_pTextHelper);

	g_CascadedShadow.DestroyAndDeallocateShadowResources();

	return S_OK;
}

// When the user change scene,recreate these components as they are scene dependent.
HRESULT CreateD3DComponents(ID3D11Device * pD3DDevice)
{
	HRESULT hr;

	ID3D11DeviceContext* pD3DImmediateContext = DXUTGetD3D11DeviceContext();
	V_RETURN(g_DialogReourceManager.OnD3D11CreateDevice(pD3DDevice, pD3DImmediateContext));
	V_RETURN(g_D3DSettingDlg.OnD3D11CreateDevice(pD3DDevice));
	g_pTextHelper = new CDXUTTextHelper(pD3DDevice, pD3DImmediateContext, &g_DialogReourceManager, 15);

	XMVECTOR eyePos = DirectX::XMVectorSet(100.0f,5.0f,5.0f,0.0f);
	XMVECTOR lookAt = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT3 vMin = XMFLOAT3(-1000.0f, -1000.0f, -1000.0f);
	XMFLOAT3 vMax = XMFLOAT3(1000.0f, 1000.0f, 1000.0f);

	g_ViewerCamera.SetViewParams(eyePos, lookAt);
	g_ViewerCamera.SetRotateButtons(TRUE, FALSE, FALSE);
	g_ViewerCamera.SetScalers(0.01f, 10.0f);
	g_ViewerCamera.SetDrag(true);
	g_ViewerCamera.SetEnableYAxisMovement(true);
	g_ViewerCamera.SetClipToBoundary(TRUE, &vMin, &vMax);
	g_ViewerCamera.FrameMove(0);

	eyePos = DirectX::XMVectorSet(-320.0f, 300.0f, -220.3f,0.0f);
	lookAt = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	g_LightCamera.SetViewParams(eyePos, lookAt);
	g_LightCamera.SetRotateButtons(TRUE, FALSE, FALSE);
	g_LightCamera.SetScalers(0.01f, 50.f);
	g_LightCamera.SetDrag(true);
	g_LightCamera.SetEnableYAxisMovement(true);
	g_LightCamera.SetClipToBoundary(TRUE, &vMin, &vMax);
	g_LightCamera.SetProjParams(DirectX::XM_PI / 4, 1.0f, 0.1f, 1000.0f);
	g_LightCamera.FrameMove(0);

	g_CascadedShadow.Init(pD3DDevice, pD3DImmediateContext, g_pSelectedMesh, &g_ViewerCamera, &g_LightCamera, &g_CascadeConfig);

	return S_OK;
}

//-------
// Calculate the camera based on size of the current scene
void UpdateViewerCameraNearFar()
{
	XMVECTOR vMeshExtents = g_CascadedShadow.GetSceneAABBMax() - g_CascadedShadow.GetScenAABBMin();
	XMVECTOR vMeshLength = XMVector3Length(vMeshExtents);
	FLOAT fMeshLength = XMVectorGetByIndex(vMeshLength, 0);
	g_ViewerCamera.SetProjParams(XM_PI / 4, g_fApsectRatio, 0.05f, fMeshLength);
}
