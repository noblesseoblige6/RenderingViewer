#pragma once

using namespace acLib;
using namespace acLib::DX12;
using namespace std;

class RenderPassClear : public RenderPass
{
public:
    RenderPassClear( ID3D12Device* pDevice );
    ~RenderPassClear();

public:
    virtual void Construct( ID3D12Device* pDevice );
    virtual void Clear( const RenderContext::ConstructParams& params );

    virtual shared_ptr<DescriptorHeap> CreateDescHeap( ID3D12Device* pDevice );
    virtual shared_ptr<RootSignature> CreateRootSinature( ID3D12Device* pDevice );
    virtual shared_ptr<PipelineState> CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature );
};