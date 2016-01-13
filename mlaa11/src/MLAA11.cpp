//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: MLAA11.cpp
//
// This sample implements Morphological Anti-Aliasing (MLAA) using DirectX 11 as a post
// processing operation.
//--------------------------------------------------------------------------------------

// DXUT now sits one directory up
#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "..\\..\\DXUT\\Core\\DXUTmisc.h"
#include "..\\..\\DXUT\\Optional\\DXUTgui.h"
#include "..\\..\\DXUT\\Optional\\DXUTCamera.h"
#include "..\\..\\DXUT\\Optional\\DXUTSettingsDlg.h"
#include "..\\..\\DXUT\\Optional\\SDKmisc.h"
#include "..\\..\\DXUT\\Optional\\SDKmesh.h"

// AMD SDK also sits one directory up
#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

// Project includes
#include "resource.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

using namespace DirectX;

#define OFFSCREENFORMAT		DXGI_FORMAT_R8G8B8A8_UNORM	

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CFirstPersonCamera          g_Camera;					// A first person camera
CDXUTDialogResourceManager  g_DialogResourceManager;	// manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;				// Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = NULL;

bool						g_bShowMLAA = true;
bool						g_bUseFiltering = false;
float						gEdgeDetectionThreshold = 12.0f;
bool						g_bShowEdges = false;
bool						g_bUseStencilBuffer = true;
float						g_Width = 1920.0f;
float						g_Height = 1080.0f;
int							g_MSAACount = 1;

// The mesh
CDXUTSDKMesh				g_SceneMesh;

// Direct3D 11 resources
ID3D11InputLayout*          g_pLayout11			= NULL;
ID3D11VertexShader*         g_pVertexShader11	= NULL;
ID3D11PixelShader*          g_pPixelShader11	= NULL;
ID3D11SamplerState*         g_pSamLinear		= NULL;
ID3D11SamplerState*         g_pSceneColorSam	= NULL;
ID3D11BlendState*			g_pBS				= NULL;    
ID3D11BlendState*			g_pNoBlendingBS		= NULL;    

ID3D11InputLayout*          g_pScreenQuadLayout	= NULL;
ID3D11VertexShader*         g_pScreenQuadVS		= NULL;
ID3D11Buffer*				g_pScreenQuadVB		= NULL;
ID3D11PixelShader*          g_pSeparateEdgePS	= NULL;
ID3D11PixelShader*          g_pSeparateEdgePSStencilPS = NULL;
ID3D11PixelShader*          g_pComputeEdgePS	= NULL;
ID3D11PixelShader*          g_pBlendColorPS		= NULL;
ID3D11PixelShader*          g_pShowEdgesPS		= NULL;
ID3D11PixelShader*          g_pClearPS			= NULL;

ID3D11Texture2D*			g_SceneColor			= NULL; 
ID3D11RenderTargetView*		g_SceneColorRTV			= NULL; 
ID3D11ShaderResourceView*	g_SceneColorSRV			= NULL; 
ID3D11Texture2D*			g_ResolvedSceneColor	= NULL; 
ID3D11ShaderResourceView*	g_ResolvedSceneColorSRV = NULL; 

ID3D11Texture2D*			g_EdgeMask		= NULL; 
ID3D11RenderTargetView*		g_EdgeMaskRTV	= NULL; 
ID3D11ShaderResourceView*	g_EdgeMaskSRV	= NULL; 

ID3D11Texture2D*			g_EdgeCount		= NULL; 
ID3D11RenderTargetView*		g_EdgeCountRTV	= NULL; 
ID3D11ShaderResourceView*	g_EdgeCountSRV	= NULL; 

ID3D11DepthStencilState*	g_SceneDepthStencilState = NULL;
ID3D11DepthStencilState*	g_DepthStencilState = NULL;
ID3D11DepthStencilState*	g_ScreenQuadDepthStencilState = NULL;
ID3D11Texture2D*			g_DepthStencil		= NULL; 
ID3D11DepthStencilView*		g_DepthStencilView = NULL;

//--------------------------------------------------------------------------------------
// AMD helper classes defined here
//--------------------------------------------------------------------------------------
static AMD::MagnifyTool     g_MagnifyTool;
static AMD::HUD             g_HUD;

// Global boolean for HUD rendering
bool                        g_bRenderHUD = true;

//--------------------------------------------------------------------------------------
// Timing data
//--------------------------------------------------------------------------------------
float						gPass1Time = 0.0f;
float						gPass2Time = 0.0f;
float						gPass3Time = 0.0f;
float						gTotalTime = 0.0f;

//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------
#pragma pack(push,1)
struct CB_VS_PER_OBJECT
{
    XMMATRIX  m_mWorldViewProjection;
    XMMATRIX  m_mWorld;
    XMVECTOR m_MaterialAmbientColor;
    XMVECTOR m_MaterialDiffuseColor;
};

struct CB_VS_PER_FRAME
{
    XMVECTOR m_vLightDirAndTime;
    XMVECTOR m_LightDiffuse;
};

struct CB_MLAA
{
	// (x, y)	-> Render target size
	// (z)		-> Edge detection threshold
    XMVECTOR m_Param; 
};
#pragma pack(pop)

ID3D11Buffer*				g_pcbVSPerObject11	= NULL;
ID3D11Buffer*               g_pcbVSPerFrame11	= NULL;
ID3D11Buffer*               g_pcbMLAA11			= NULL;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum 
{
    IDC_TOGGLEFULLSCREEN = 1,
    IDC_TOGGLEREF,
    IDC_CHANGEDEVICE,
    IDC_MLAA,
    IDC_USE_STENCIL,
    IDC_THRESHOLD,
    IDC_THRESHOLD_STATIC,
    IDC_SHOWEDGE,
    IDC_NUM_CONTROL_IDS
};

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
	
	// Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"MLAA11 v2.2" );

    // Only require 10-level hardware, change to D3D_FEATURE_LEVEL_11_0 to require 11-class hardware
    // Switch to D3D_FEATURE_LEVEL_9_x for 10level9 hardware
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, (int)g_Width, (int)g_Height );

    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
	WCHAR szTemp[256];

	D3DCOLOR DlgColor = 0x88888888; // Semi-transparent background for the dialog

    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.m_GUI.Init( &g_DialogResourceManager );
    g_HUD.m_GUI.SetBackgroundColors( DlgColor );
    g_HUD.m_GUI.SetCallback( OnGUIEvent );

    // Don't allow MSAA, since this sample does fullscreen AA
    // as a post process
    g_SettingsDlg.GetDialogControl()->GetControl( DXUTSETTINGSDLG_D3D11_MULTISAMPLE_COUNT )->SetEnabled( false );
    g_SettingsDlg.GetDialogControl()->GetControl( DXUTSETTINGSDLG_D3D11_MULTISAMPLE_QUALITY )->SetEnabled( false );

    int iY = AMD::HUD::iElementDelta;

    g_HUD.m_GUI.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );
    g_HUD.m_GUI.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F3 );
    g_HUD.m_GUI.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F2 );

    iY += AMD::HUD::iGroupDelta;	

	g_HUD.m_GUI.AddCheckBox( IDC_MLAA, L"Enable MLAA (M)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 170, 22, g_bShowMLAA, 'M' );	
	g_HUD.m_GUI.AddCheckBox( IDC_USE_STENCIL, L"Use Stencil Test", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 170, 22, g_bUseStencilBuffer, 'J' );
	swprintf_s( szTemp, L"Edge Detect Threshold:%d", (int)gEdgeDetectionThreshold);	
	g_HUD.m_GUI.AddStatic( IDC_THRESHOLD_STATIC, szTemp, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 170, 22 );
	g_HUD.m_GUI.AddSlider( IDC_THRESHOLD, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 170, 22, 1, 64, (int)gEdgeDetectionThreshold);
	g_HUD.m_GUI.AddCheckBox( IDC_SHOWEDGE, L"Show MLAA Edges", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 170, 22, g_bShowEdges );	    

    iY += AMD::HUD::iGroupDelta;

    // Add the magnify tool UI to our HUD
    g_MagnifyTool.InitApp( &g_HUD.m_GUI, iY );
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{	
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( XMVectorSet( 0.90f, 0.90f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );	    

	WCHAR szTemp[256];
	swprintf_s( szTemp, L"Effect cost in milliseconds (Detect Edge = %.2f, Compute Edge Length = %.2f, Blend Color = %.2f, Total = %.2f)", gPass1Time, gPass2Time, gPass3Time, gTotalTime);
	g_pTxtHelper->DrawTextLine( szTemp );

	g_pTxtHelper->SetInsertionPos( 5, DXUTGetDXGIBackBufferSurfaceDesc()->Height - AMD::HUD::iElementDelta );
	g_pTxtHelper->DrawTextLine( L"Toggle GUI    : F1" );

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}
//--------------------------------------------------------------------------------------
// Create render targets for MLAA post processing
//--------------------------------------------------------------------------------------
HRESULT CreateMLAARenderTargets(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	HRESULT hr;

	SAFE_RELEASE( g_SceneColor );
    SAFE_RELEASE( g_SceneColorRTV );
    SAFE_RELEASE( g_SceneColorSRV );
	SAFE_RELEASE( g_ResolvedSceneColor );
	SAFE_RELEASE( g_ResolvedSceneColorSRV );

	SAFE_RELEASE( g_EdgeMask );
    SAFE_RELEASE( g_EdgeMaskRTV );
    SAFE_RELEASE( g_EdgeMaskSRV );    

	SAFE_RELEASE( g_EdgeCount );
    SAFE_RELEASE( g_EdgeCountRTV );
    SAFE_RELEASE( g_EdgeCountSRV );    

	SAFE_RELEASE( g_SceneDepthStencilState);
	SAFE_RELEASE( g_DepthStencilState);
	SAFE_RELEASE( g_ScreenQuadDepthStencilState);
	SAFE_RELEASE( g_DepthStencil);	
	SAFE_RELEASE( g_DepthStencilView); 


	g_Width = (float)pBackBufferSurfaceDesc->Width;
	g_Height = (float)pBackBufferSurfaceDesc->Height;

	// Create other render resources here	
    D3D11_TEXTURE2D_DESC td;
	D3D11_RENDER_TARGET_VIEW_DESC rtvd;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvd;

	// Create offscreen render target
    memset(&td, 0, sizeof(td));
    td.ArraySize = 1;
    td.Format = OFFSCREENFORMAT;
	td.Height = pBackBufferSurfaceDesc->Height;
	td.Width = pBackBufferSurfaceDesc->Width;
    td.CPUAccessFlags = 0;
    td.MipLevels = 1;
    td.MiscFlags = 0;
	td.SampleDesc = pBackBufferSurfaceDesc->SampleDesc;    
    td.Usage = D3D11_USAGE_DEFAULT;
	if (g_MSAACount <= 1)
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;    
	else
		td.BindFlags = D3D11_BIND_RENDER_TARGET;    
    V_RETURN(pd3dDevice->CreateTexture2D(&td, NULL, &g_SceneColor));
    
    memset(&rtvd, 0, sizeof(rtvd));
    rtvd.Format = OFFSCREENFORMAT;
	if (g_MSAACount <= 1)
		rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	else
		rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
    rtvd.Texture2D.MipSlice = 0;
    V_RETURN(pd3dDevice->CreateRenderTargetView(g_SceneColor, &rtvd, &g_SceneColorRTV));
    
	if (g_MSAACount <= 1)
	{
		memset(&srvd, 0, sizeof(srvd));
		srvd.Format = OFFSCREENFORMAT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MostDetailedMip = 0;
		srvd.Texture2D.MipLevels = 1;
		V_RETURN(pd3dDevice->CreateShaderResourceView(g_SceneColor, &srvd, &g_SceneColorSRV));
	}

	if (g_MSAACount > 1)
	{
		// Create resolved offscreen render target when MSAA is enabled
		memset(&td, 0, sizeof(td));
		td.ArraySize = 1;
		td.Format = OFFSCREENFORMAT;
		td.Height = pBackBufferSurfaceDesc->Height;
		td.Width = pBackBufferSurfaceDesc->Width;
		td.CPUAccessFlags = 0;
		td.MipLevels = 1;
		td.MiscFlags = 0;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;    
		V_RETURN(pd3dDevice->CreateTexture2D(&td, NULL, &g_ResolvedSceneColor));
	    
		memset(&srvd, 0, sizeof(srvd));
		srvd.Format = OFFSCREENFORMAT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MostDetailedMip = 0;
		srvd.Texture2D.MipLevels = 1;
		V_RETURN(pd3dDevice->CreateShaderResourceView(g_ResolvedSceneColor, &srvd, &g_ResolvedSceneColorSRV));
	}

	// Create edge mask render target
    memset(&td, 0, sizeof(td));
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8_TYPELESS;
	td.Height = pBackBufferSurfaceDesc->Height;
	td.Width = pBackBufferSurfaceDesc->Width;
    td.CPUAccessFlags = 0;
    td.MipLevels = 1;
    td.MiscFlags = 0;
	td.SampleDesc.Count = 1;
	td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;    
    V_RETURN(pd3dDevice->CreateTexture2D(&td, NULL, &g_EdgeMask));
    
    memset(&rtvd, 0, sizeof(rtvd));
    rtvd.Format = DXGI_FORMAT_R8_UINT;
    rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvd.Texture2D.MipSlice = 0;
    V_RETURN(pd3dDevice->CreateRenderTargetView(g_EdgeMask, &rtvd, &g_EdgeMaskRTV));
    
    memset(&srvd, 0, sizeof(srvd));
    srvd.Format = DXGI_FORMAT_R8_UINT;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MostDetailedMip = 0;
    srvd.Texture2D.MipLevels = 1;
    V_RETURN(pd3dDevice->CreateShaderResourceView(g_EdgeMask, &srvd, &g_EdgeMaskSRV));

	// Create edge count render target
    memset(&td, 0, sizeof(td));
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8_TYPELESS;
	td.Height = pBackBufferSurfaceDesc->Height;
	td.Width = pBackBufferSurfaceDesc->Width;
    td.CPUAccessFlags = 0;
    td.MipLevels = 1;
    td.MiscFlags = 0;
	td.SampleDesc.Count = 1;
	td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;    
    V_RETURN(pd3dDevice->CreateTexture2D(&td, NULL, &g_EdgeCount));
    
    memset(&rtvd, 0, sizeof(rtvd));
    rtvd.Format = DXGI_FORMAT_R8G8_UINT;
    rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvd.Texture2D.MipSlice = 0;
    V_RETURN(pd3dDevice->CreateRenderTargetView(g_EdgeCount, &rtvd, &g_EdgeCountRTV));
    
    memset(&srvd, 0, sizeof(srvd));
    srvd.Format = DXGI_FORMAT_R8G8_UINT;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MostDetailedMip = 0;
    srvd.Texture2D.MipLevels = 1;
    V_RETURN(pd3dDevice->CreateShaderResourceView(g_EdgeCount, &srvd, &g_EdgeCountSRV));


	D3D11_DEPTH_STENCIL_DESC desc;
    desc.DepthEnable = FALSE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = TRUE;
    desc.StencilReadMask = 0xff;
    desc.StencilWriteMask = 0xff;
    desc.BackFace.StencilFunc         = D3D11_COMPARISON_NOT_EQUAL;
    desc.BackFace.StencilDepthFailOp  = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilPassOp       = D3D11_STENCIL_OP_INCR_SAT;
    desc.FrontFace.StencilFunc        = D3D11_COMPARISON_NOT_EQUAL;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_INCR_SAT;

    pd3dDevice->CreateDepthStencilState(&desc, &g_DepthStencilState);

	desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_LESS;//D3D11_COMPARISON_LESS_EQUAL;
    desc.StencilEnable = FALSE;
    pd3dDevice->CreateDepthStencilState(&desc, &g_SceneDepthStencilState);

	desc.DepthEnable = FALSE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = FALSE;
    pd3dDevice->CreateDepthStencilState(&desc, &g_ScreenQuadDepthStencilState);	

	memset(&td, 0, sizeof(td));
	td.ArraySize = 1;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.Height = pBackBufferSurfaceDesc->Height;
    td.Width = pBackBufferSurfaceDesc->Width;
    td.CPUAccessFlags = 0;
    td.MipLevels = 1;
    td.MiscFlags = 0;
    td.SampleDesc = pBackBufferSurfaceDesc->SampleDesc;
    td.Usage = D3D11_USAGE_DEFAULT;
      
    V_RETURN(pd3dDevice->CreateTexture2D(&td, NULL, &g_DepthStencil));
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
    memset(&dsvd, 0, sizeof(dsvd));

    dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvd.Flags = 0;
	dsvd.ViewDimension = (pBackBufferSurfaceDesc->SampleDesc.Count > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvd.Texture2D.MipSlice = 0;
    V_RETURN(pd3dDevice->CreateDepthStencilView(g_DepthStencil, &dsvd, &g_DepthStencilView));
            
	return S_OK;
}
//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

	g_Width = (float)pBackBufferSurfaceDesc->Width;
	g_Height = (float)pBackBufferSurfaceDesc->Height;
	g_MSAACount = pBackBufferSurfaceDesc->SampleDesc.Count;

	// Hooks to various AMD helper classes
    g_MagnifyTool.OnCreateDevice( pd3dDevice );
    g_HUD.OnCreateDevice( pd3dDevice );

    // Read the HLSL file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\src\\Shaders\\Scene.hlsl" ) );

    // Load the scene mesh	
	g_SceneMesh.Create( pd3dDevice, L"powerplant\\powerplant.sdkmesh", false );

    // Compile the shaders
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    // You should use the lowest possible shader profile for your shader to enable various feature levels. These
    // shaders are simple enough to work well within the lowest possible profile, and will run on all feature levels
	ID3DBlob* pErrorBuffer = NULL;
    ID3DBlob* pVertexShaderBuffer = NULL;
    V_RETURN( D3DCompileFromFile( str, NULL, NULL, "RenderSceneVS", "vs_4_0", dwShaderFlags, 0, 
                                  &pVertexShaderBuffer, &pErrorBuffer ) );	
    ID3DBlob* pPixelShaderBuffer = NULL;
    V_RETURN( D3DCompileFromFile( str, NULL, NULL, "RenderScenePS", "ps_4_0", dwShaderFlags, 0, 
                                  &pPixelShaderBuffer, &pErrorBuffer ) );

    // Create the shaders for scene rendering
    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
                                              pVertexShaderBuffer->GetBufferSize(), NULL, &g_pVertexShader11 ) );
    DXUT_SetDebugName( g_pVertexShader11, "RenderSceneVS" );

    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pPixelShader11 ) );
    DXUT_SetDebugName( g_pPixelShader11, "RenderScenePS" );

    // Create a layout for the object data
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pVertexShaderBuffer->GetBufferPointer(),
                                             pVertexShaderBuffer->GetBufferSize(), &g_pLayout11 ) );
    DXUT_SetDebugName( g_pLayout11, "Primary" );
    
    // Create state objects
    D3D11_SAMPLER_DESC samDesc;
    ZeroMemory( &samDesc, sizeof(samDesc) );
    samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samDesc.MaxAnisotropy = 1;
    samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &samDesc, &g_pSamLinear ) );
    DXUT_SetDebugName( g_pSamLinear, "Linear" );

    ZeroMemory( &samDesc, sizeof(samDesc) );
    samDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;//D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samDesc.MaxAnisotropy = 1;
    samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samDesc.MaxLOD = 0;
    V_RETURN( pd3dDevice->CreateSamplerState( &samDesc, &g_pSceneColorSam ) );
    DXUT_SetDebugName( g_pSceneColorSam, "Scene color sampler" );	

    // Create constant buffers
    D3D11_BUFFER_DESC cbDesc;
    ZeroMemory( &cbDesc, sizeof(cbDesc) );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbDesc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &cbDesc, NULL, &g_pcbVSPerObject11 ) );
    DXUT_SetDebugName( g_pcbVSPerObject11, "CB_VS_PER_OBJECT" );

    cbDesc.ByteWidth = sizeof( CB_VS_PER_FRAME );
    V_RETURN( pd3dDevice->CreateBuffer( &cbDesc, NULL, &g_pcbVSPerFrame11 ) );
    DXUT_SetDebugName( g_pcbVSPerFrame11, "CB_VS_PER_FRAME" );

	// Create other render resources here	
	CreateMLAARenderTargets(pd3dDevice, pBackBufferSurfaceDesc);

	// Create a layout for the screen quad vertex
    const D3D11_INPUT_ELEMENT_DESC ScreenQuadLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },        
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\src\\Shaders\\MLAA11.hlsl" ) );
    V_RETURN( D3DCompileFromFile( str, NULL, NULL, "ScreenQuadVS", "vs_4_0", dwShaderFlags, 0, 
                                  &pVertexShaderBuffer, &pErrorBuffer ) );   
	//CHAR* Error = (CHAR*)(pErrorBuffer->GetBufferPointer());

	// Create the shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
                                              pVertexShaderBuffer->GetBufferSize(), NULL, &g_pScreenQuadVS ) );
    DXUT_SetDebugName( g_pScreenQuadVS, "ScreenQuadVS" );
	
	// create screen quad vertex buffer
	V_RETURN( pd3dDevice->CreateInputLayout( ScreenQuadLayout, ARRAYSIZE( ScreenQuadLayout ), pVertexShaderBuffer->GetBufferPointer(), pVertexShaderBuffer->GetBufferSize(), &g_pScreenQuadLayout ) );	    
	D3D11_BUFFER_DESC bd;
	D3D11_SUBRESOURCE_DATA initData;
	float ScreenQuadVertex[] = { -1, -1, 1, 1, 0, 1, 
		                         -1,  1, 1, 1, 0, 0,
								  1, -1, 1, 1, 1, 1,
								  1,  1, 1, 1, 1, 0};

	// create shaders for MLAA passes
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = 4*sizeof(float)*6;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	initData.pSysMem = ScreenQuadVertex;
	V_RETURN( pd3dDevice->CreateBuffer(&bd, &initData, &g_pScreenQuadVB) ); 	

	D3D10_SHADER_MACRO ShaderMacros[4];

	// create first pass pixel shader which detects the edges.
	V_RETURN( D3DCompileFromFile( str, NULL, NULL, "MLAA_SeperatingLines_PS", "ps_5_0", dwShaderFlags, 0, 
                                  &pPixelShaderBuffer, NULL ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pSeparateEdgePS ) );	
    DXUT_SetDebugName( g_pSeparateEdgePS, "g_pSeparateEdgePS" );

	ShaderMacros[0].Name = "USE_STENCIL";
    ShaderMacros[0].Definition = "1";
	ShaderMacros[1].Name = NULL;
    ShaderMacros[1].Definition = "1";
	V_RETURN( D3DCompileFromFile( str, ShaderMacros, NULL, "MLAA_SeperatingLines_PS", "ps_5_0", dwShaderFlags, 0, 
                                  &pPixelShaderBuffer, NULL ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pSeparateEdgePSStencilPS ) );	
    DXUT_SetDebugName( g_pSeparateEdgePSStencilPS, "g_pComputeEdgeUsingStencilPS" );	


	// create second pass pixel shader which conpute the length of edges.
	V_RETURN( D3DCompileFromFile( str, NULL, NULL, "MLAA_ComputeLineLength_PS", "ps_4_0", dwShaderFlags, 0, 
                                  &pPixelShaderBuffer, NULL ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pComputeEdgePS ) );	
    DXUT_SetDebugName( g_pComputeEdgePS, "g_pComputeEdgePS" );		

	// create third pass pixel shader which blend pixel color according to the edge length and shape.
	V_RETURN( D3DCompileFromFile( str, NULL, NULL, "MLAA_BlendColor_PS", "ps_4_0", dwShaderFlags, 0, 
                                  &pPixelShaderBuffer, NULL ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pBlendColorPS ) );	
    DXUT_SetDebugName( g_pBlendColorPS, "g_pBlendColorPS" );		    
	
	// create color blending pixel shader for showing edges.
	ShaderMacros[0].Name = "SHOW_EDGES";
    ShaderMacros[0].Definition = "1";
	ShaderMacros[1].Name = NULL;
    ShaderMacros[1].Definition = "1";
    
	V_RETURN( D3DCompileFromFile( str, ShaderMacros, NULL, "MLAA_BlendColor_PS", "ps_4_0", dwShaderFlags, 0, 
                                  &pPixelShaderBuffer, NULL ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pShowEdgesPS ) );	
    DXUT_SetDebugName( g_pShowEdgesPS, "g_pShowEdgesPS" );			

	V_RETURN( D3DCompileFromFile( str, ShaderMacros, NULL, "MLAA_Clear_PS", "ps_4_0", dwShaderFlags, 0, 
                                  &pPixelShaderBuffer, NULL ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), NULL, &g_pClearPS ) );	
    DXUT_SetDebugName( g_pClearPS, "MLAA_Clear_PS" );			


	// No longer need the shader blobs
    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );

	// create blend state object for scene and MLAA rendering
	D3D11_BLEND_DESC BlendDesc;
	memset(&BlendDesc, 0, sizeof(D3D11_BLEND_DESC));

    BlendDesc.AlphaToCoverageEnable  = FALSE;
    BlendDesc.IndependentBlendEnable = TRUE;
    for (UINT i = 0; i < 3; i++)
    {
        BlendDesc.RenderTarget[i].BlendEnable           = true;
        BlendDesc.RenderTarget[i].SrcBlend              = D3D11_BLEND_ONE;
        BlendDesc.RenderTarget[i].DestBlend             = D3D11_BLEND_ZERO;
        BlendDesc.RenderTarget[i].BlendOp               = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[i].SrcBlendAlpha         = D3D11_BLEND_ONE;
        BlendDesc.RenderTarget[i].DestBlendAlpha        = D3D11_BLEND_ZERO;
        BlendDesc.RenderTarget[i].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
	V_RETURN( pd3dDevice->CreateBlendState( &BlendDesc, &g_pBS ) );

	// No color blending for MLAA rendering
	BlendDesc.AlphaToCoverageEnable  = FALSE;
    BlendDesc.IndependentBlendEnable = FALSE;
	for (UINT i = 0; i < 2; i++)
    {
        BlendDesc.RenderTarget[i].BlendEnable = FALSE;        
    }
	V_RETURN( pd3dDevice->CreateBlendState( &BlendDesc, &g_pNoBlendingBS ) );
	
	// create constant buffer for MLAA
	cbDesc.ByteWidth = sizeof( CB_MLAA );
    V_RETURN( pd3dDevice->CreateBuffer( &cbDesc, NULL, &g_pcbMLAA11 ) );
    DXUT_SetDebugName( g_pcbMLAA11, "CB_MLAA" );

    static bool bFirstPass = true;

    // One-time setup
    if( bFirstPass )
    {
        // Setup the camera's view parameters
        g_Camera.SetRotateButtons( true, false, false );
        g_Camera.SetEnablePositionMovement( true );
        g_Camera.SetViewParams( XMVectorSet( -93.6121f, 7.82762f, 46.3031f, 1.0f ), XMVectorSet( -92.6522f, 7.77217f, 46.0284f, 1.0f ) );
        g_Camera.SetScalers( 0.005f, 100.0f );

		bFirstPass = false;
    }

	TIMER_Init(pd3dDevice);

    return S_OK;
}
//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

	V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 500.0f );
    
    // Set the location and size of the AMD standard HUD
    g_HUD.m_GUI.SetLocation( pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0 );
    g_HUD.m_GUI.SetSize( AMD::HUD::iDialogWidth, pBackBufferSurfaceDesc->Height );
 
	// Magnify tool will capture from the color buffer
    g_MagnifyTool.OnResizedSwapChain( pd3dDevice, pSwapChain, pBackBufferSurfaceDesc, pUserContext, 
        pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0 );
    D3D11_RENDER_TARGET_VIEW_DESC RTDesc;
    ID3D11Resource* pTempRTResource;
    DXUTGetD3D11RenderTargetView()->GetResource( &pTempRTResource );
    DXUTGetD3D11RenderTargetView()->GetDesc( &RTDesc );
    g_MagnifyTool.SetSourceResources( pTempRTResource, RTDesc.Format, 
                DXUTGetDXGIBackBufferSurfaceDesc()->Width, DXUTGetDXGIBackBufferSurfaceDesc()->Height,
                DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count );
    g_MagnifyTool.SetPixelRegion( 128 );
    g_MagnifyTool.SetScale( 5 );
    SAFE_RELEASE( pTempRTResource );

	// AMD HUD hook
    g_HUD.OnResizedSwapChain( pBackBufferSurfaceDesc );

	CreateMLAARenderTargets(pd3dDevice, pBackBufferSurfaceDesc);

    return S_OK;
}
//--------------------------------------------------------------------------------------
// Render the scene
//--------------------------------------------------------------------------------------
void RenderScene(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime)
{
	const float BlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	pd3dImmediateContext->OMSetBlendState(g_pBS, BlendFactor, 0xffffffff);
	
	TIMER_Reset();

	ID3D11RenderTargetView* RTVArray[3] = {NULL, NULL, NULL};
	if (g_bShowMLAA)
	{
		RTVArray[0] = g_SceneColorRTV;		
	}
	else
	{
		RTVArray[0] = DXUTGetD3D11RenderTargetView();
	}
	pd3dImmediateContext->OMSetRenderTargets(1, RTVArray, g_DepthStencilView);	
	pd3dImmediateContext->OMSetDepthStencilState(g_SceneDepthStencilState, 0);

	XMFLOAT4 vClearColor;
    vClearColor.x = 0.25f;
    vClearColor.y = 0.25f;
    vClearColor.z = 0.5f;
    // Put clear color in Gamma space
    vClearColor.x = sqrt(vClearColor.x);
    vClearColor.y = sqrt(vClearColor.y);
    vClearColor.z = sqrt(vClearColor.z);
	vClearColor.w = vClearColor.x*0.30f + vClearColor.x*0.59f + vClearColor.x*0.11f; // Luma from gamma-space color
    float ClearColor[4] = { vClearColor.x, vClearColor.y, vClearColor.z, vClearColor.w };
    pd3dImmediateContext->ClearRenderTargetView( RTVArray[0], ClearColor );

	// Clear the depth stencil        
	pd3dImmediateContext->ClearDepthStencilView( g_DepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0, 0 );			
	
    // Get the projection & view matrix from the camera class
    XMMATRIX mWorld = XMMatrixIdentity();
    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mWorldViewProjection = mWorld * mView * mProj;

    // Set the constant buffers
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pcbVSPerFrame11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_VS_PER_FRAME* pVSPerFrame = ( CB_VS_PER_FRAME* )MappedResource.pData;
    pVSPerFrame->m_vLightDirAndTime = XMVectorSet( 0,0.707f,-0.707f, (float)fTime );
    pVSPerFrame->m_LightDiffuse = XMVectorSet( 1.f, 1.f, 1.f, 1.f );
    pd3dImmediateContext->Unmap( g_pcbVSPerFrame11, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 1, 1, &g_pcbVSPerFrame11 );

    V( pd3dImmediateContext->Map( g_pcbVSPerObject11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_VS_PER_OBJECT* pVSPerObject = ( CB_VS_PER_OBJECT* )MappedResource.pData;
    pVSPerObject->m_mWorldViewProjection = XMMatrixTranspose( mWorldViewProjection );
    pVSPerObject->m_mWorld = XMMatrixTranspose( mWorld );
    pVSPerObject->m_MaterialAmbientColor = XMVectorSet( 0.4f, 0.4f, 0.4f, 1.0f );
    pVSPerObject->m_MaterialDiffuseColor = XMVectorSet( 0.6f, 0.6f, 0.6f, 1.0f );
    pd3dImmediateContext->Unmap( g_pcbVSPerObject11, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pcbVSPerObject11 );

    // Set render resources
    pd3dImmediateContext->IASetInputLayout( g_pLayout11 );
    pd3dImmediateContext->VSSetShader( g_pVertexShader11, NULL, 0 );
    pd3dImmediateContext->PSSetShader( g_pPixelShader11, NULL, 0 );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSamLinear );	

    // Render the scene mesh 
	g_SceneMesh.Render( pd3dImmediateContext, 0 );	
	
	// Resolve the offscreen if MSAA is enabled
	if (g_MSAACount > 1)
	{
		pd3dImmediateContext->ResolveSubresource(g_ResolvedSceneColor, 0, g_SceneColor, 0, OFFSCREENFORMAT);
	}	
}
//--------------------------------------------------------------------------------------
// Render MLAA post processing 
//--------------------------------------------------------------------------------------
void RenderMLAA(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext)
{	
	static int Count = 0;
	static float T = 0.0f, T1 = 0.0f, T2 = 0.0f, T3 = 0.0f;	
	static float ZeroColor[4] = {0, 0, 0, 0};
	
	if (g_bShowMLAA)
	{	
		ID3D11ShaderResourceView* SRVArray[3] = {NULL, NULL, NULL};

		// Upload constants
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map( g_pcbMLAA11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		CB_MLAA* pMLAAParam = ( CB_MLAA* )MappedResource.pData;
		pMLAAParam->m_Param = XMVectorSet( g_Width, g_Height, 1.0f/gEdgeDetectionThreshold, 0.0f);
		pd3dImmediateContext->Unmap( g_pcbMLAA11, 0 );
		pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &g_pcbMLAA11 );
		const float BlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		pd3dImmediateContext->OMSetBlendState(g_pNoBlendingBS, BlendFactor, 0xffffffff);		
	
		TIMER_Begin(0, L"Pass1");
			// 1st pass, detect edges ---------------------------------------------------------------------			
			// Set render resources
			pd3dImmediateContext->IASetInputLayout( g_pScreenQuadLayout );
			UINT Offset[1] = {0};		
			UINT Stride[1] = {sizeof(float)*6};
			pd3dImmediateContext->IASetVertexBuffers(0, 1, &g_pScreenQuadVB, Stride, Offset);
			pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pd3dImmediateContext->VSSetShader( g_pScreenQuadVS, NULL, 0 );						
			
			ID3D11RenderTargetView* pRTV = g_EdgeMaskRTV; 
			if (g_bUseStencilBuffer)
			{				
				pd3dImmediateContext->PSSetShader( g_pSeparateEdgePSStencilPS, NULL, 0 );
				pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, g_DepthStencilView);	       			
				pd3dImmediateContext->ClearRenderTargetView( g_EdgeMaskRTV, ZeroColor );	
				pd3dImmediateContext->OMSetDepthStencilState(g_DepthStencilState, 1);
			}
			else
			{		
				pd3dImmediateContext->PSSetShader( g_pSeparateEdgePS, NULL, 0 );				
				pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, NULL);	       					
			}						
			
			if (g_MSAACount > 1)
				SRVArray[0] = g_ResolvedSceneColorSRV;			
			else
				SRVArray[0] = g_SceneColorSRV;			
			pd3dImmediateContext->PSSetShaderResources(0, 3, SRVArray);
            pd3dImmediateContext->PSSetSamplers( 1, 1, &g_pSceneColorSam );
			pd3dImmediateContext->Draw(4, 0);
		TIMER_End( );		

		TIMER_Begin(0, L"Pass2");
			// 2nd pass, compute the length of edges --------------------------------------------------------		
			pRTV = g_EdgeCountRTV;
			if (g_bUseStencilBuffer)
			{
				pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, g_DepthStencilView);				
				pd3dImmediateContext->ClearRenderTargetView( g_EdgeCountRTV, ZeroColor );	
				pd3dImmediateContext->OMSetDepthStencilState(g_DepthStencilState, 0);				
			}
			else
			{
				pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, NULL);				
			}
			// Set render resources
			pd3dImmediateContext->IASetInputLayout( g_pScreenQuadLayout );	
			pd3dImmediateContext->IASetVertexBuffers(0, 1, &g_pScreenQuadVB, Stride, Offset);
			pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pd3dImmediateContext->VSSetShader( g_pScreenQuadVS, NULL, 0 );
			pd3dImmediateContext->PSSetShader( g_pComputeEdgePS, NULL, 0 );
			SRVArray[1] = g_EdgeMaskSRV;
			pd3dImmediateContext->PSSetShaderResources(0, 3, SRVArray);
			pd3dImmediateContext->Draw(4, 0);
		TIMER_End( );

		TIMER_Begin(0, L"Pass3");
			// 3rd pass, blend colors according to the edge shape and length ----------------------------------
			pRTV = DXUTGetD3D11RenderTargetView();
			pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, NULL);		
			
			// Set render resources
			pd3dImmediateContext->IASetInputLayout( g_pScreenQuadLayout );	
			pd3dImmediateContext->IASetVertexBuffers(0, 1, &g_pScreenQuadVB, Stride, Offset);
			pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pd3dImmediateContext->VSSetShader( g_pScreenQuadVS, NULL, 0 );
			if (g_bShowEdges)
				pd3dImmediateContext->PSSetShader( g_pShowEdgesPS, NULL, 0 );				
			else
			{
				pd3dImmediateContext->PSSetShader( g_pBlendColorPS, NULL, 0 );
			}
			SRVArray[1] = NULL;
			SRVArray[2] = g_EdgeCountSRV;
			pd3dImmediateContext->PSSetShaderResources(0, 3, SRVArray);		
			pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSceneColorSam );
			pd3dImmediateContext->Draw(4, 0);
		TIMER_End( );			

		SRVArray[0] = NULL;
		SRVArray[1] = NULL;
		SRVArray[2] = NULL;
		pd3dImmediateContext->PSSetShaderResources(0, 3, SRVArray);
		pd3dImmediateContext->VSSetShader( NULL, NULL, 0 );
		pd3dImmediateContext->PSSetShader( NULL, NULL, 0 );
		Count++;

		T1 += (float)TIMER_GetTime(Gpu, L"Pass1") * 1000.0f;
		T2 += (float)TIMER_GetTime(Gpu, L"Pass2") * 1000.0f;
		T3 += (float)TIMER_GetTime(Gpu, L"Pass3") * 1000.0f;
		
		if (Count == 100)
		{
			gPass1Time = T1 / (float)Count;
			gPass2Time = T2 / (float)Count;
			gPass3Time = T3 / (float)Count;
			gTotalTime = gPass1Time + gPass2Time + gPass3Time;

			T1 = T2 = T3 = 0.0f;
			Count = 0;		
		}
	}
	else
	{
		gTotalTime = gPass1Time = gPass2Time = gPass3Time = 0.0f;
	}
}
//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }       

	RenderScene(pd3dDevice, pd3dImmediateContext, fTime);
	RenderMLAA(pd3dDevice, pd3dImmediateContext);

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

		if ( g_bRenderHUD )
		{
			g_MagnifyTool.Render();
			g_HUD.OnRender( fElapsedTime );
		}
		RenderText();

    DXUT_EndPerfEvent();

    static DWORD dwTimefirst = GetTickCount();
    if ( GetTickCount() - dwTimefirst > 5000 )
    {    
        OutputDebugString( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
        OutputDebugString( L"\n" );
        dwTimefirst = GetTickCount();
    }
}
//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}
//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

	g_MagnifyTool.OnDestroyDevice();    
    g_HUD.OnDestroyDevice();

	SAFE_RELEASE( g_pLayout11 );
    SAFE_RELEASE( g_pVertexShader11 );
    SAFE_RELEASE( g_pPixelShader11 );    
    SAFE_RELEASE( g_pSamLinear );
	SAFE_RELEASE( g_pSceneColorSam );	
	SAFE_RELEASE( g_pBS );
	SAFE_RELEASE( g_pNoBlendingBS );
	
	SAFE_RELEASE( g_pScreenQuadLayout );
    SAFE_RELEASE( g_pScreenQuadVS );
	SAFE_RELEASE( g_pScreenQuadVB );
    SAFE_RELEASE( g_pSeparateEdgePS );
	SAFE_RELEASE( g_pSeparateEdgePSStencilPS );
    SAFE_RELEASE( g_pComputeEdgePS );
	SAFE_RELEASE( g_pBlendColorPS );	
	SAFE_RELEASE( g_pShowEdgesPS );
	SAFE_RELEASE( g_pClearPS );

	SAFE_RELEASE( g_SceneColor );
    SAFE_RELEASE( g_SceneColorRTV );
    SAFE_RELEASE( g_SceneColorSRV );
	SAFE_RELEASE( g_ResolvedSceneColor );
	SAFE_RELEASE( g_ResolvedSceneColorSRV );

	SAFE_RELEASE( g_EdgeMask );
    SAFE_RELEASE( g_EdgeMaskRTV );
    SAFE_RELEASE( g_EdgeMaskSRV );    

	SAFE_RELEASE( g_EdgeCount );
    SAFE_RELEASE( g_EdgeCountRTV );
    SAFE_RELEASE( g_EdgeCountSRV );    

	SAFE_RELEASE( g_SceneDepthStencilState);
	SAFE_RELEASE( g_DepthStencilState);
	SAFE_RELEASE( g_ScreenQuadDepthStencilState);
	SAFE_RELEASE( g_DepthStencil);	
	SAFE_RELEASE( g_DepthStencilView);  

    // Delete additional render resources here...
    g_SceneMesh.Destroy();

    SAFE_RELEASE( g_pcbVSPerObject11 );
    SAFE_RELEASE( g_pcbVSPerFrame11 );
	SAFE_RELEASE( g_pcbMLAA11 );

	TIMER_Destroy();
}
//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE )
        {
            DXUTDisplaySwitchingToREFWarning();
        }

        // Start with vsync disabled
        pDeviceSettings->d3d11.SyncInterval = 0;
    }

    // Don't allow MSAA, since this sample does fullscreen AA
    // as a post process
    pDeviceSettings->d3d11.sd.SampleDesc.Count = 1;

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
     // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.m_GUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );


    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if ( bKeyDown )
    {
        switch( nChar )
        {
			case VK_F1:
				g_bRenderHUD = !g_bRenderHUD;
				break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	WCHAR szTemp[256];

	switch( nControlID )
	{
		case IDC_TOGGLEFULLSCREEN:
			DXUTToggleFullScreen();
			break;
		
        case IDC_TOGGLEREF:
			DXUTToggleREF();
			break;
		
        case IDC_CHANGEDEVICE:
			g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
			break;
		
        case IDC_MLAA:
			g_bShowMLAA = !g_bShowMLAA;			
			if (g_bShowMLAA)			
			{
				g_HUD.m_GUI.GetCheckBox(IDC_USE_STENCIL)->SetEnabled(TRUE);
				g_HUD.m_GUI.GetCheckBox(IDC_SHOWEDGE)->SetEnabled(TRUE);
				g_HUD.m_GUI.GetSlider(IDC_THRESHOLD)->SetEnabled(TRUE);
			}
			else
			{
				g_HUD.m_GUI.GetCheckBox(IDC_USE_STENCIL)->SetEnabled(FALSE);
				g_HUD.m_GUI.GetCheckBox(IDC_SHOWEDGE)->SetEnabled(FALSE);
				g_HUD.m_GUI.GetSlider(IDC_THRESHOLD)->SetEnabled(FALSE);
			}
			break;
		
        case IDC_USE_STENCIL:
			g_bUseStencilBuffer = !g_bUseStencilBuffer;
			break;
		
        case IDC_SHOWEDGE :
			g_bShowEdges = !g_bShowEdges;			
			break;
		
        case IDC_THRESHOLD:          
			gEdgeDetectionThreshold = (float)(((CDXUTSlider*)pControl)->GetValue());
			swprintf_s( szTemp, L"Edge detection threshold:%d", (int)gEdgeDetectionThreshold);
			g_HUD.m_GUI.GetStatic( IDC_THRESHOLD_STATIC )->SetText( szTemp );                    
			break;
	}
	// Call the MagnifyTool gui event handler
	g_MagnifyTool.OnGUIEvent( nEvent, nControlID, pControl, pUserContext );	
}

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
