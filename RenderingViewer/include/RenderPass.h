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

    void AddModel( shared_ptr<Model> pModel );
    void AddResource( Buffer::BUFFER_VIEW_TYPE type, shared_ptr<Buffer> pResource);

    virtual void BindResources( ID3D12Device* pDevice );
    virtual void Construct( const RenderInfo::ConstructParams& params );

    void Render( ID3D12CommandQueue* pCommadnQueue );

    void Reset();

protected:
    vector<shared_ptr<Model> >         m_pModels;
    vector<shared_ptr<RenderInfo> >     m_pRenderInfoList;
    map<shared_ptr<Model>, shared_ptr<RenderInfo> >  m_modelToRenderInfoMap;

    vector<vector<shared_ptr<Buffer> > >            m_pResources;

    ComPtr<ID3DBlob>            m_pVSBlob;
    ComPtr<ID3DBlob>            m_pPSBlob;
    PipelineState::ShaderCode   m_shader;

    PipelineState::InputElement m_element;
};

class RenderPassForward : public RenderPass
{
public:
    RenderPassForward( ID3D12Device* pDevice );
    ~RenderPassForward();

    virtual shared_ptr<DescriptorHeap> CreateDescHeap( ID3D12Device* pDevice );
    virtual shared_ptr<RootSignature> CreateRootSinature( ID3D12Device* pDevice );
    virtual shared_ptr<PipelineState> CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature );
};

class RenderPassShadow : public RenderPass
{
public:
    RenderPassShadow( ID3D12Device* pDevice );
    ~RenderPassShadow();

    virtual shared_ptr<DescriptorHeap> CreateDescHeap( ID3D12Device* pDevice );
    virtual shared_ptr<RootSignature> CreateRootSinature( ID3D12Device* pDevice );
    virtual shared_ptr<PipelineState> CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature );
};

class RenderPassClear : public RenderPass
{
public:
    RenderPassClear( ID3D12Device* pDevice );
    ~RenderPassClear();

public:
    virtual void BindResources( ID3D12Device* pDevice );
    virtual void Construct( const RenderInfo::ConstructParams& params );

    virtual shared_ptr<DescriptorHeap> CreateDescHeap( ID3D12Device* pDevice );
    virtual shared_ptr<RootSignature> CreateRootSinature( ID3D12Device* pDevice );
    virtual shared_ptr<PipelineState> CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature );
};