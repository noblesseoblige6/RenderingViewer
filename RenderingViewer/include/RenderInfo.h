#pragma once

using namespace acLib;
using namespace acLib::DX12;
using namespace std;

class RenderInfo
{
public:
    struct ConstructParams
    {
        ConstructParams()
            : clearColor(Vec3f::ONE)
            , clearVal( 1.0f )
        {
            viewport = { 0.0f };
            viewport.MaxDepth = 1.0f;
        }

        D3D12_VIEWPORT viewport;

        shared_ptr<Buffer> renderTarget;
        Vec3f              clearColor;
        D3D12_CPU_DESCRIPTOR_HANDLE hadleRT;

        shared_ptr<Buffer> depthStencil;
        float              clearVal;
        D3D12_CPU_DESCRIPTOR_HANDLE hadleDS;

        D3D12_RESOURCE_STATES targetStateSrc;
        D3D12_RESOURCE_STATES targetStateDst;
    };

public:
    RenderInfo( ID3D12Device* pDevice );
    ~RenderInfo();
    
    void Reset();

    virtual bool CreateDescHeap( ID3D12Device* pDevice) = 0;
    virtual bool CreateRootSinature( ID3D12Device* pDevice ) = 0;
    virtual bool CreatePipelineState( ID3D12Device* pDevice ) = 0;

    shared_ptr<CommandList> GetCommandList() const { return m_pCommandList; }
    shared_ptr<DescriptorHeap> GetDescHeap() const { return m_pDescHeap; }

protected:
    shared_ptr<DescriptorHeap>         m_pDescHeap;

    shared_ptr<RootSignature>          m_pRootSignature;
    shared_ptr<PipelineState>          m_pPipelineState;

    shared_ptr<CommandList>            m_pCommandList;

    shared_ptr<ID3D12Device>           m_pDevice;
};

class RenderInfoShadow : public RenderInfo
{
public:
    RenderInfoShadow( ID3D12Device* pDevice );
    ~RenderInfoShadow();

    bool Construct( const ConstructParams& params, shared_ptr<Model> model );

    virtual bool CreateDescHeap( ID3D12Device* pDevice );
    virtual bool CreateRootSinature( ID3D12Device* pDevice );
    virtual bool CreatePipelineState( ID3D12Device* pDevice );
};
