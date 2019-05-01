RenderInfo::RenderInfo( ID3D12Device* pDevice )
{
    m_pCommandList = make_shared<CommandList>( pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT );
}

RenderInfo::~RenderInfo()
{
}

void RenderInfo::Reset()
{
    m_pCommandList->Reset( m_pPipelineState);
}

RenderInfoForward::RenderInfoForward( ID3D12Device* pDevice )
    : RenderInfo( pDevice )
{
    CreateDescHeap( pDevice );
    CreateRootSinature( pDevice );
    CreatePipelineState( pDevice );
}

RenderInfoForward::~RenderInfoForward()
{
}

bool RenderInfoForward::Construct( const ConstructParams& params, shared_ptr<Model> model )
{
    m_pCommandList->SetRootSignature( m_pRootSignature );
    m_pCommandList->SetDescriptorHeaps( 1, m_pDescHeap );
    m_pCommandList->SetPipelineState( m_pPipelineState );

    m_pCommandList->SetViewport( params.viewport );

    {
        m_pCommandList->Begin( params.renderTarget, params.targetStateSrc, params.targetStateDst );
        {
            auto handleRTV = params.hadleRT;
            auto handleDSV = params.hadleDS;

            m_pCommandList->SetTargets( &handleRTV, &handleDSV );

            float clearColor[] = { params.clearColor.x, params.clearColor.y, params.clearColor.z, 1.0f };
            m_pCommandList->ClearTargets( &handleRTV, clearColor, &handleDSV, D3D12_CLEAR_FLAG_DEPTH, params.clearVal );

            m_pCommandList->Draw( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, model->GetVertexBuffer(), model->GetIndexBuffer(), model->GetIndexCount() );
        }
        m_pCommandList->End();
    }

    return true;
}

bool RenderInfoForward::CreateDescHeap( ID3D12Device* pDevice )
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = 4;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    m_pDescHeap = make_shared<DescriptorHeap>();
    m_pDescHeap->Create( pDevice, desc );

    return true;
}

bool RenderInfoForward::CreateRootSinature( ID3D12Device* pDevice )
{
    // ディスクリプタレンジの設定.
    D3D12_DESCRIPTOR_RANGE ranges[2];
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 3;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートパラメータの設定.
    D3D12_ROOT_PARAMETER params[1];
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[0].DescriptorTable.NumDescriptorRanges = _countof( ranges );
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];

    // 静的サンプラーの設定.
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ルートシグニチャの設定.
    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.NumParameters = _countof( params );
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;;

    m_pRootSignature = make_shared<RootSignature>();
    
    return m_pRootSignature->Create( pDevice, desc );
}


bool RenderInfoForward::CreatePipelineState( ID3D12Device* pDevice )
{
    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;
    if (!Shader::CompileShader( L"ForwardShading.hlsl", pVSBlob, pPSBlob ))
    {
        return false;
    }

    PipelineState::ShaderCode shader;
    shader.vs = { reinterpret_cast<UINT8*>(pVSBlob->GetBufferPointer()), pVSBlob->GetBufferSize() };
    shader.ps = { reinterpret_cast<UINT8*>(pPSBlob->GetBufferPointer()), pPSBlob->GetBufferSize() };

    // 入力レイアウトの設定.
    PipelineState::InputElement element;
    element.elements = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "VTX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    m_pPipelineState = make_shared<PipelineState>( element, shader, m_pRootSignature );

    m_pPipelineState->GetDesc().NumRenderTargets = 1;
    m_pPipelineState->GetDesc().RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    return m_pPipelineState->Create( pDevice );
}



RenderInfoShadow::RenderInfoShadow( ID3D12Device* pDevice )
    : RenderInfo( pDevice )
{
    CreateDescHeap( pDevice );
    CreateRootSinature( pDevice );
    CreatePipelineState( pDevice );
}

RenderInfoShadow::~RenderInfoShadow()
{
}

bool RenderInfoShadow::Construct( const ConstructParams& params, shared_ptr<Model> model )
{
    m_pCommandList->SetRootSignature( m_pRootSignature );
    m_pCommandList->SetDescriptorHeaps( 1, m_pDescHeap );
    m_pCommandList->SetPipelineState( m_pPipelineState );

    m_pCommandList->SetViewport( params.viewport );

    m_pCommandList->Begin( params.depthStencil, params.targetStateSrc, params.targetStateDst );
    {
        auto hadleDS = params.hadleDS;

        m_pCommandList->SetTargets( nullptr, &hadleDS );

        m_pCommandList->ClearTargets( nullptr, nullptr, &hadleDS, D3D12_CLEAR_FLAG_DEPTH, params.clearVal );

        m_pCommandList->Draw( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, model->GetVertexBuffer(), model->GetIndexBuffer(), model->GetIndexCount() );
    }
    m_pCommandList->End();

    return true;
}

bool RenderInfoShadow::CreateDescHeap( ID3D12Device* pDevice )
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = 1;
    desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    m_pDescHeap = make_shared<DescriptorHeap>();
    m_pDescHeap->Create( pDevice, desc );

    return true;
}

bool RenderInfoShadow::CreateRootSinature( ID3D12Device* pDevice )
{
    // ディスクリプタレンジの設定.
    D3D12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors     = 1;
    ranges[0].BaseShaderRegister = 1;
    ranges[0].RegisterSpace      = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートパラメータの設定.
    D3D12_ROOT_PARAMETER params[1];
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[0].DescriptorTable.NumDescriptorRanges = _countof( ranges );
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];

    // ルートシグニチャの設定.
    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.NumParameters = _countof( params );
    desc.pParameters = params;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    m_pRootSignature = make_shared<RootSignature>();
    return m_pRootSignature->Create( pDevice, desc );
}


bool RenderInfoShadow::CreatePipelineState( ID3D12Device* pDevice )
{
    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;
    if (!Shader::CompileShader( L"Shadow.hlsl", pVSBlob, pPSBlob ))
    {
        return false;
    }

    PipelineState::ShaderCode shader;
    shader.vs = { reinterpret_cast<UINT8*>(pVSBlob->GetBufferPointer()), pVSBlob->GetBufferSize() };
    shader.ps = { reinterpret_cast<UINT8*>(pPSBlob->GetBufferPointer()), pPSBlob->GetBufferSize() };


    // 入力レイアウトの設定
    PipelineState::InputElement element;
    element.elements = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "VTX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    m_pPipelineState = make_shared<PipelineState>( element, shader, m_pRootSignature );

    return m_pPipelineState->Create( pDevice );
}

