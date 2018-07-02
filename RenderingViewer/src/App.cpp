#include "stdafx.h"
#include "App.h"
#include "Vertex.h"

using namespace std;

App::App( HWND hWnd )
    : m_isInit( false )
{
    m_hWnd = hWnd;

    Initialize();
}

App::~App()
{
    Terminate();
}

bool App::Run()
{
    if (!IsInitialized())
    {
        cerr << "Failed to run app" << endl;

        return false;
    }

    OnFrameRender();

    return true;
}


bool App::Initialize()
{
    if (!InitD3D12())
    {
        cerr << "Failed to initialize D3D12" << endl;
        return false;
    }

    if (!InitApp())
    {
        cerr << "Failed to initialize App" << endl;
        return false;
    }

    SetIsInitialized( true );

    return true;
}

void App::Terminate()
{
    TermD3D12();

    // COMライブラリの終了処理
    CoUninitialize();
}

bool App::InitD3D12()
{
    HRESULT hr = S_OK;

    // ウィンドウ幅を取得.
    RECT rc;
    GetClientRect( m_hWnd, &rc );
    m_width = rc.right - rc.left;
    m_height = rc.bottom - rc.top;

    UINT flags = 0;

#if defined(DEBUG) || defined(_DEBUG)
    ID3D12Debug* pDebug;
    D3D12GetDebugInterface( IID_ID3D12Debug, (void**)&pDebug );
    if (pDebug)
    {
        pDebug->EnableDebugLayer();
        pDebug->Release();
    }
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    hr = CreateDXGIFactory2( flags, IID_IDXGIFactory4, (void**)(m_pFactory.ReleaseAndGetAddressOf()) );
    if (FAILED( hr ))
    {
        std::cerr << "Error : CreateDXGIFactory() Failed." << std::endl;
        return false;
    }

    hr = m_pFactory->EnumAdapters( 0, m_pAdapter.GetAddressOf() );
    if (FAILED( hr ))
    {
        std::cerr << "Error : IDXGIFactory::EnumAdapters() Failed." << std::endl;
        return false;
    }

    // デバイス生成.
    hr = D3D12CreateDevice( m_pAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_ID3D12Device,
        (void**)(m_pDevice.ReleaseAndGetAddressOf()) );

    // 生成チェック.
    if (FAILED( hr ))
    {
        // Warpアダプターで再トライ.
        m_pAdapter.Reset();
        m_pDevice.Reset();

        hr = m_pFactory->EnumWarpAdapter( IID_PPV_ARGS( m_pAdapter.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            std::cerr << "Error : IDXGIFactory::EnumWarpAdapter() Failed." << std::endl;
            return false;
        }

        // デバイス生成.
        hr = D3D12CreateDevice( m_pAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_ID3D12Device,
            (void**)(m_pDevice.ReleaseAndGetAddressOf()) );
        if (FAILED( hr ))
        {
            std::cerr << "Error: D3D12CreateDevice() Failed." << std::endl;
            return false;
        }
    }

    // create command allocator
    hr = m_pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_ID3D12CommandAllocator,
        (void**)(m_pCommandAllocator.ReleaseAndGetAddressOf()) );
    if (FAILED( hr ))
    {
        return false;
    }

    // create command queue
    {
        D3D12_COMMAND_QUEUE_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );

        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = 0;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        hr = m_pDevice->CreateCommandQueue( &desc,
                                            IID_ID3D12CommandQueue,
                                            (void**)(m_pCommandQueue.
                                            ReleaseAndGetAddressOf()) );
        if (FAILED( hr ))
        {
            return false;
        }
    }

    // create command list
    {
        hr = m_pDevice->CreateCommandList( 1,
                                           D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           m_pCommandAllocator.Get(),
                                           nullptr,
                                           IID_ID3D12GraphicsCommandList,
                                           (void**)m_pCommandList.ReleaseAndGetAddressOf() );
        if (FAILED( hr ))
        {
            return false;
        }
    }

    // create swap chain
    {
        hr = CreateDXGIFactory( IID_IDXGIFactory,
                                (void**)m_pFactory.ReleaseAndGetAddressOf() );
        if (FAILED( hr ))
        {
            return false;
        }

        DXGI_SWAP_CHAIN_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );

        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.Width = m_width;
        desc.BufferDesc.Height = m_height;
        desc.BufferDesc.RefreshRate.Numerator = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
        desc.OutputWindow = m_hWnd;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Windowed = true;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        hr = m_pFactory->CreateSwapChain( m_pCommandQueue.Get(), &desc, m_pSwapChain.ReleaseAndGetAddressOf() );
        if (FAILED( hr ))
        {
            return false;
        }

        m_swapChainCount = 0;
    }

    // create descripter heap for render target
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );

        desc.NumDescriptors = 2;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = m_pDevice->CreateDescriptorHeap( &desc,
                                              IID_ID3D12DescriptorHeap,
                                              (void**)(m_pDescHeapRenderTarget.ReleaseAndGetAddressOf()) );

        m_DescHeapRenderTargetSize = m_pDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
        if (FAILED( hr ))
        {
            return false;
        }

        desc.NumDescriptors = 1;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        
        
        hr = m_pDevice->CreateDescriptorHeap( &desc,
                                              IID_ID3D12DescriptorHeap,
                                              (void**)(m_pDescHeapDepthStencil.ReleaseAndGetAddressOf()) );

        m_DescHeapDepthStencilSize = m_pDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
        if (FAILED( hr ))
        {
            return false;
        }
    }

    // create render targets from back buffers
    {
        auto handle = m_pDescHeapRenderTarget->GetCPUDescriptorHandleForHeapStart();

        D3D12_RENDER_TARGET_VIEW_DESC desc;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        desc.Texture2D.PlaneSlice = 0;

        for (int i = 0; i < 2; ++i)
        {
            // Resources come from back buffers
            hr = m_pSwapChain->GetBuffer( i, IID_ID3D12Resource, (void**)m_pRenderTarget[i].ReleaseAndGetAddressOf() );
            if (FAILED( hr ))
            {
                return false;
            }

            m_pDevice->CreateRenderTargetView( m_pRenderTarget[i].Get(), &desc, handle );
            m_renderTargetHandle[i] = handle;

            // advance hadle pointer
            handle.ptr += m_DescHeapRenderTargetSize;
        }
    }

    // create depth stencil view
    {
     // create resource
     // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop;
        prop.Type = D3D12_HEAP_TYPE_DEFAULT;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        // リソースの設定.
        D3D12_RESOURCE_DESC desc;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 0;
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        // クリア値の設定.
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        // リソースを生成.
        hr = m_pDevice->CreateCommittedResource( &prop,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &desc,
                                                 D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                 &clearValue,
                                                 IID_PPV_ARGS( m_pDepthStencil.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            std::cerr << "Error : ID3D12Device::CreateCommittedResource() Failed." << std::endl;
            return false;
        }
            // create depth stencil view
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

            m_pDevice->CreateDepthStencilView( m_pDepthStencil.Get(),
                                               &dsvDesc,
                                               m_pDescHeapDepthStencil->GetCPUDescriptorHandleForHeapStart() );
        }

    // create fence
    m_fenceEvent = CreateEvent( 0, false, false, 0 );

    hr = m_pDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)m_pFence.ReleaseAndGetAddressOf() );
    if (FAILED( hr ))
    {
        return false;
    }

    // viewport config
    {
        m_viewport.TopLeftX = 0;
        m_viewport.TopLeftY = 0;
        m_viewport.Width = static_cast<float>(m_width);
        m_viewport.Height = static_cast<float>(m_height);
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;
    }

    return true;
}

bool App::InitApp()
{
    HRESULT hr = S_OK;

    // create root sinature
    {
        // ディスクリプタレンジの設定.
        D3D12_DESCRIPTOR_RANGE range;
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // ルートパラメータの設定.
        D3D12_ROOT_PARAMETER param;
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &range;

        // ルートシグニチャの設定.
        D3D12_ROOT_SIGNATURE_DESC desc;
        desc.NumParameters = 1;
        desc.pParameters = &param;
        desc.NumStaticSamplers = 0;
        desc.pStaticSamplers = nullptr;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        ComPtr<ID3DBlob> pSignature;
        ComPtr<ID3DBlob> pError;

        // シリアライズする.
        hr = D3D12SerializeRootSignature( &desc,
                                          D3D_ROOT_SIGNATURE_VERSION_1,
                                          pSignature.GetAddressOf(),
                                          pError.GetAddressOf() );
        if (FAILED( hr ))
        {
            std::cerr << "Error : D3D12SerializeRootSignataure() Failed." << std::endl;
            return false;
        }

        // ルートシグニチャを生成.
        hr = m_pDevice->CreateRootSignature( 0,
                                             pSignature->GetBufferPointer(),
                                             pSignature->GetBufferSize(),
                                             IID_PPV_ARGS( m_pRootSignature.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            std::cerr <<  "Error : ID3D12Device::CreateRootSignature() Failed." << std::endl;
            return false;
        }
    }

    // create pipe-line state
    {
        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;

        // 頂点シェーダのファイルパスを検索.
        std::wstring path;
        if (!SearchFilePath( L"SimpleVS.cso", path ))
        {
            std::cerr <<  "Error : File Not Found." << std::endl;
            return false;
        }

        // コンパイル済み頂点シェーダを読み込む.
        hr = D3DReadFileToBlob( path.c_str(), pVSBlob.GetAddressOf() );
        if (FAILED( hr ))
        {
            std::cerr <<  "Error : D3DReadFileToBlob() Failed." << std::endl;
            return false;
        }

        // ピクセルシェーダのファイルパスを検索.
        if (!SearchFilePath( L"SimplePS.cso", path ))
        {
            std::cerr << "Error : File Not Found." << std::endl;
            return false;
        }

        // コンパイル済みピクセルシェーダを読み込む.
        hr = D3DReadFileToBlob( path.c_str(), pPSBlob.GetAddressOf() );
        if (FAILED( hr ))
        {
            std::cerr <<  "Error : D3DReadFileToBlob() Failed." << std::endl;
            return false;
        }

        // 入力レイアウトの設定.
        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "VTX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // ラスタライザーステートの設定.
        D3D12_RASTERIZER_DESC descRS;
        descRS.FillMode = D3D12_FILL_MODE_SOLID;
        descRS.CullMode = D3D12_CULL_MODE_NONE;
        descRS.FrontCounterClockwise = FALSE;
        descRS.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        descRS.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        descRS.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        descRS.DepthClipEnable = TRUE;
        descRS.MultisampleEnable = FALSE;
        descRS.AntialiasedLineEnable = FALSE;
        descRS.ForcedSampleCount = 0;
        descRS.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // レンダーターゲットのブレンド設定.
        D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };

        // ブレンドステートの設定.
        D3D12_BLEND_DESC descBS;
        descBS.AlphaToCoverageEnable = FALSE;
        descBS.IndependentBlendEnable = FALSE;
        for (UINT i = 0; i<D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            descBS.RenderTarget[i] = descRTBS;
        }

        // パイプラインステートの設定.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.InputLayout = { inputElements, _countof( inputElements ) };
        desc.pRootSignature = m_pRootSignature.Get();
        desc.VS = { reinterpret_cast<UINT8*>(pVSBlob->GetBufferPointer()), pVSBlob->GetBufferSize() };
        desc.PS = { reinterpret_cast<UINT8*>(pPSBlob->GetBufferPointer()), pPSBlob->GetBufferSize() };
        desc.RasterizerState = descRS;
        desc.BlendState = descBS;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.StencilEnable = FALSE;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;

        // パイプラインステートを生成.
        hr = m_pDevice->CreateGraphicsPipelineState( &desc, IID_PPV_ARGS( m_pPipelineState.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            std::cerr <<  "Error : ID3D12Device::CreateGraphicsPipelineState() Failed." << std::endl;
            return false;
        }
    }

    // create vertex buffer
    {
        Vertex vertices[] =
        {
            Vertex( Vec3( 0.0f,  1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec2(0.0f, 1.0f), Vec4(1.0f, 0.0f, 0.0f, 1.0f) ),
            Vertex( Vec3( 1.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec2(1.0f, 0.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f) ),
            Vertex( Vec3(-1.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec2(0.0f, 0.0f), Vec4(0.0f, 0.0f, 1.0f, 1.0f) ),
        };

        // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop;
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        // リソースの設定.
        D3D12_RESOURCE_DESC desc;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof( vertices );
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = m_pDevice->CreateCommittedResource( &prop,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &desc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr,
                                                 IID_PPV_ARGS(m_pVertexBuffer.ReleaseAndGetAddressOf()) );
        if (FAILED( hr ))
        {
            std::cerr << "Error : ID3D12Device::CreateCommittedResource() Failed." << std::endl;
            return false;
        }

        // mapping data on CPU memory to GPU memory
        UINT8* pData;
        hr = m_pVertexBuffer->Map( 0, nullptr, reinterpret_cast<void**>(&pData) );
        if (FAILED( hr ))
        {
            std::cerr << "Error : ID3D12Resource::Map() Failed."  << std::endl;
            return false;
        }

        memcpy( pData, vertices, sizeof( vertices ) );

        m_pVertexBuffer->Unmap( 0, nullptr );

        // 頂点バッファビューの設定.
        m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
        m_VertexBufferView.StrideInBytes = sizeof( Vertex );
        m_VertexBufferView.SizeInBytes = sizeof( vertices );
    }

    // create descriptor heap for constant buffer
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        hr = m_pDevice->CreateDescriptorHeap( &desc, IID_ID3D12DescriptorHeap, (void**)( m_pDescHeapConstant.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            std::cerr <<  "Error : ID3D12Device::CreateDescriptorHeap() Failed." << std::endl;
            return false;
        }
    }

    // create constant buffer
    {
        // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        // リソースの設定.
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        // 定数バッファは256バイトでアラインメントされている必要がある
        desc.Width = 512;// sizeof( ResConstantBuffer );
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // リソースを生成.
        hr = m_pDevice->CreateCommittedResource( &prop,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &desc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr,
                                                 IID_PPV_ARGS( m_pConstantBuffer.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            std::cerr <<  "Error : ID3D12Device::CreateCommittedResource() Failed."  << std::endl;
            return false;
        }

        // 定数バッファビューの設定.
        D3D12_CONSTANT_BUFFER_VIEW_DESC bufferDesc = {};
        bufferDesc.BufferLocation = m_pConstantBuffer->GetGPUVirtualAddress();
        // 定数バッファは256バイトでアラインメントされている必要がある
        //bufferDesc.SizeInBytes = sizeof( ResConstantBuffer );
        bufferDesc.SizeInBytes = 512;

        // 定数バッファビューを生成.
        m_pDevice->CreateConstantBufferView( &bufferDesc, m_pDescHeapConstant->GetCPUDescriptorHandleForHeapStart() );

        // マップする. アプリケーション終了まで Unmap しない.
        hr = m_pConstantBuffer->Map( 0, nullptr, reinterpret_cast<void**>(&m_pCbvDataBegin) );
        if (FAILED( hr ))
        {
            std::cerr <<  "Error : ID3D12Resource::Map() Failed."  << std::endl;
            return false;
        }

        // アスペクト比算出.
        auto aspectRatio = static_cast<FLOAT>(m_width) / static_cast<FLOAT>(m_height);

        // 定数バッファデータの設定.
        m_constantBufferData.world      = Mat44::IDENTITY;
        m_constantBufferData.view       = Mat44::CreateLookAt( Vec3( 0.0f, 0.0f, 5.0f ), Vec3( 0.0f, 0.0f, 0.0f ), Vec3( 0.0f, 1.0f, 0.0f ) );
        m_constantBufferData.projection = Mat44::CreatePerspectiveFieldOfView( kPI*0.25, aspectRatio, 1.0f, 1000.0f );

        memcpy( m_pCbvDataBegin, &m_constantBufferData, sizeof( m_constantBufferData ) );
    }

    return true;
}

bool App::TermD3D12()
{
    WaitDrawCommandDone();

    if (!CloseHandle( m_fenceEvent ))
    {
        cerr << "Failed to terminate app" << endl;
        return false;
    }
    m_fenceEvent = nullptr;

    for (UINT i = 0; i < 2; ++i)
    {
        m_pRenderTarget[i].Reset();
    }

    m_pDepthStencil.Reset();

    m_pSwapChain.Reset();
    m_pFence.Reset();
    m_pCommandList.Reset();
    m_pCommandAllocator.Reset();
    m_pCommandQueue.Reset();
    m_pDevice.Reset();

    return true;
}

bool App::TermApp()
{
    m_pConstantBuffer->Unmap( 0, nullptr );

    m_pConstantBuffer.Reset();
    m_pDescHeapConstant.Reset();

    m_pVertexBuffer.Reset();
    m_VertexBufferView.BufferLocation = 0;
    m_VertexBufferView.SizeInBytes = 0;
    m_VertexBufferView.StrideInBytes = 0;

    m_pPipelineState.Reset();
    m_pRootSignature.Reset();

    return true;
}

bool App::SearchFilePath( const wchar_t* filePath, std::wstring& result )
{
    if (filePath == nullptr)
    {
        return false;
    }

    if (wcscmp( filePath, L" " ) == 0 || wcscmp( filePath, L"" ) == 0)
    {
        return false;
    }

    wchar_t exePath[520] = { 0 };
    GetModuleFileNameW( nullptr, exePath, 520 );
    exePath[519] = 0; // null終端化.
    PathRemoveFileSpecW( exePath );

    wchar_t dstPath[520] = { 0 };

    wcscpy_s( dstPath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"..\\%s", filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"..\\..\\%s", filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"\\res\\%s", filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\%s", exePath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\..\\%s", exePath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\..\\..\\%s", exePath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\res\\%s", exePath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    return false;
}

void App::SetResourceBarrier( ID3D12GraphicsCommandList* pCmdList,
                              ID3D12Resource* pResource,
                              D3D12_RESOURCE_STATES stateBefore,
                              D3D12_RESOURCE_STATES stateAfter )
{
    D3D12_RESOURCE_BARRIER desc = {};
    desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    desc.Transition.pResource = pResource;
    desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    desc.Transition.StateBefore = stateBefore;
    desc.Transition.StateAfter = stateAfter;

    pCmdList->ResourceBarrier( 1, &desc );
}

void App::Present( unsigned int syncInterval )
{
    BeginDrawCommand();

    WaitDrawCommandDone();

    m_pSwapChain->Present( syncInterval, 0 );
}

void App::OnFrameRender()
{
    memcpy( m_pCbvDataBegin, &m_constantBufferData, 512 );

    m_pCommandList->SetDescriptorHeaps( 1, m_pDescHeapConstant.GetAddressOf() );
    m_pCommandList->SetGraphicsRootSignature( m_pRootSignature.Get() );
    m_pCommandList->SetGraphicsRootDescriptorTable( 0, m_pDescHeapConstant->GetGPUDescriptorHandleForHeapStart() );

    m_pCommandList->RSSetViewports( 1, &m_viewport );
    
    {
        SetResourceBarrier( m_pCommandList.Get(), m_pRenderTarget[m_swapChainCount].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );

        // レンダーターゲットのハンドルを取得.
        auto handleRTV = m_pDescHeapRenderTarget->GetCPUDescriptorHandleForHeapStart();
        auto handleDSV = m_pDescHeapDepthStencil->GetCPUDescriptorHandleForHeapStart();
        handleRTV.ptr += (m_swapChainCount * m_DescHeapRenderTargetSize);

        // レンダーターゲットの設定.
        m_pCommandList->OMSetRenderTargets( 1, &handleRTV, FALSE, &handleDSV );

        float clearColor[] = { 0.5f, 0.8f, 0.5f, 1.0f };
        m_pCommandList->ClearRenderTargetView( m_renderTargetHandle[m_swapChainCount], clearColor, 0, nullptr );
        m_pCommandList->ClearDepthStencilView( handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

        // プリミティブトポロジーの設定.
        m_pCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        // 頂点バッファビューを設定.
        m_pCommandList->IASetVertexBuffers( 0, 1, &m_VertexBufferView );
        // 描画コマンドを生成.
        m_pCommandList->DrawInstanced( 3, 1, 0, 0 );

        SetResourceBarrier( m_pCommandList.Get(), m_pRenderTarget[m_swapChainCount].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
    }

    Present( 0 );

    if (++m_swapChainCount >= 2)
        m_swapChainCount = 0;

    ResetFrame();
}

void App::ResetFrame()
{
    // コマンドリストとコマンドアロケータをリセットする.
    m_pCommandAllocator->Reset();
    m_pCommandList->Reset( m_pCommandAllocator.Get(), m_pPipelineState.Get() );
}

void App::BeginDrawCommand()
{
    // コマンドリストへの記録を終了し，コマンド実行.
    ID3D12CommandList* cmdList = m_pCommandList.Get();

    m_pCommandList->Close();
    m_pCommandQueue->ExecuteCommandLists( 1, &cmdList );

    // set target fence value 
    m_pCommandQueue->Signal( m_pFence.Get(), m_fenceValue );
    m_fenceValue++;
}

void App::WaitDrawCommandDone()
{
    const UINT64 fenceValue = m_fenceValue-1;
    if (m_pFence->GetCompletedValue() >= fenceValue)
        return;

    // ignite the event when fence value reached target value
    m_pFence->SetEventOnCompletion( fenceValue, m_fenceEvent );
    WaitForSingleObject( m_fenceEvent, INFINITE );
}
