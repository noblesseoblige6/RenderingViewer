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
            , bDSOnly(false)
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

        bool bDSOnly;
    };

public:
    RenderInfo( ID3D12Device* pDevice );
    ~RenderInfo();
    
public:
    bool Clear( const ConstructParams& params );
    bool Construct( const ConstructParams& params, shared_ptr<Model> model );

    void Reset();

    shared_ptr<DescriptorHeap> GetDescHeap() const { return m_pDescHeap; }
    void SetDescHeap( shared_ptr<DescriptorHeap> pDescHeap ) { m_pDescHeap = pDescHeap; }

    shared_ptr<RootSignature> GetRootSignature() const { return m_pRootSignature; }
    void SetRootSinature( shared_ptr<RootSignature> pRootSignature ) { m_pRootSignature = pRootSignature; }

    shared_ptr<PipelineState> GetPipelineState() const { return m_pPipelineState; }
    void SetPipelineState( shared_ptr<PipelineState> pPipelineState ){ m_pPipelineState = pPipelineState; }

    shared_ptr<CommandList> GetCommandList() const { return m_pCommandList; }

protected:
    shared_ptr<DescriptorHeap>         m_pDescHeap;

    shared_ptr<RootSignature>          m_pRootSignature;
    shared_ptr<PipelineState>          m_pPipelineState;

    shared_ptr<CommandList>            m_pCommandList;
};
