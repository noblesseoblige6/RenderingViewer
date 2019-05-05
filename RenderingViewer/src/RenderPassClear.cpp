RenderPassClear::RenderPassClear( ID3D12Device* pDevice )
    : RenderPass( pDevice )
{
}

RenderPassClear::~RenderPassClear()
{
}

void RenderPassClear::Construct( ID3D12Device* pDevice )
{
    shared_ptr<RenderContext> pContext = make_shared<RenderContext>( pDevice );
    m_pRenderContexts.push_back( pContext );
}

void RenderPassClear::Clear( const RenderContext::ConstructParams& params )
{
    for (auto pRenderInfo : m_pRenderContexts)
    {
        pRenderInfo->Clear( params );
    }
}

shared_ptr<DescriptorHeap> RenderPassClear::CreateDescHeap( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
    return nullptr;
}

shared_ptr<RootSignature> RenderPassClear::CreateRootSinature( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
    return nullptr;
}

shared_ptr<PipelineState> RenderPassClear::CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature )
{
    AC_USE_VAR( pDevice );
    return nullptr;
}