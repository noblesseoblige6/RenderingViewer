#pragma once

using namespace acLib;
using namespace acLib::DX12;
using namespace std;

class RenderPass
{
public:
    RenderPass( ID3D12Device* pDevice );
    ~RenderPass();
    
public:
    virtual shared_ptr<DescriptorHeap> CreateDescHeap( ID3D12Device* pDevice ) = 0;
    virtual shared_ptr<RootSignature>  CreateRootSinature( ID3D12Device* pDevice ) = 0;
    virtual shared_ptr<PipelineState> CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature ) = 0;

    void SetScene( shared_ptr<Scene> pScene );

    virtual void BindResource( ID3D12Device* pDevice, shared_ptr<Buffer> pResource, Buffer::BUFFER_VIEW_TYPE type );

    virtual void Construct( ID3D12Device* pDevice );
    virtual void Draw( const RenderContext::ConstructParams& params );

    void Render( ID3D12CommandQueue* pCommadnQueue );

    void Reset();

protected:
    shared_ptr<Scene>                   m_pScene;
    vector<shared_ptr<RenderContext> >     m_pRenderContexts;

    ComPtr<ID3DBlob>            m_pVSBlob;
    ComPtr<ID3DBlob>            m_pPSBlob;
    PipelineState::ShaderCode   m_shader;

    PipelineState::InputElement m_element;
};
