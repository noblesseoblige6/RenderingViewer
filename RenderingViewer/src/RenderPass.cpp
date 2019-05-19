RenderPass::RenderPass( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
}

RenderPass::~RenderPass()
{
}

void RenderPass::SetScene( shared_ptr<Scene> pScene )
{
    m_pScene = pScene;
}

void RenderPass::BindResource( ID3D12Device* pDevice, shared_ptr<Buffer> pResource, Buffer::BUFFER_VIEW_TYPE type )
{
    for (auto pRenderInfo : m_pRenderContexts)
    {
        pResource->CreateBufferView( pDevice, pRenderInfo->GetDescHeap(), type );
    }
}

void RenderPass::Construct( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
    m_pRenderContexts.clear();
}

void RenderPass::Draw( const RenderContext::ConstructParams& params )
{
    for (auto pRenderInfo : m_pRenderContexts)
    {
        pRenderInfo->Draw( params );
    }
}

void RenderPass::Render( ID3D12CommandQueue* pCommadnQueue )
{
    ID3D12CommandList* cmdList[1024];

    for (int i = 0; i < m_pRenderContexts.size(); ++i)
    {
        const auto& pCommand = m_pRenderContexts[i]->GetCommandList();
        cmdList[i] = pCommand->GetCommandList();
    }

    pCommadnQueue->ExecuteCommandLists( (UINT)m_pRenderContexts.size(), cmdList );
}

void RenderPass::Reset()
{
    for (auto pRenderContext : m_pRenderContexts)
    {
        pRenderContext->Reset();
    }
}
