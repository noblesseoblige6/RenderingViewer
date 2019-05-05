#pragma once

using namespace acLib;
using namespace acLib::DX12;
using namespace std;

class Model : public Node
{
public:
    struct ResMaterialData
    {
        Vec4f ka;
        Vec4f kd;
        Vec4f ks; // Alpha: Shininess

        DWORD size;
    };

    struct BoundingBox
    {
        BoundingBox()
            : hi( Vec3f( FLT_MIN ) )
            , lo( Vec3f( FLT_MAX ) )
        {
        }

        Vec3f hi;
        Vec3f lo;
    };

public:
    Model( ID3D12Device* pDevice );
    ~Model();
    
    void Release();

    bool BindAsset( ID3D12Device* pDevice, const string& sourcePath );
    virtual bool BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap);
    
    int GetIndexCount() const { return m_indexCount; }

    shared_ptr<VertexBuffer> GetVertexBuffer() const { return m_pVertexBuffer; }
    shared_ptr<IndexBuffer>  GetIndexBuffer() const { return m_pIndexBuffer; }

protected:
    void CreateVertexBuffer( ID3D12Device* pDevice, const acObjLoader& loader);
    void CreateIndexBuffer( ID3D12Device* pDevice, const acObjLoader& loader );
    void CreateBoundingBox( const acObjLoader& loader );
    void CreateMaterial( ID3D12Device* pDevice );

private:
    shared_ptr<VertexBuffer>    m_pVertexBuffer;
    shared_ptr<IndexBuffer>     m_pIndexBuffer;
    int                         m_indexCount;

    shared_ptr<ConstantBuffer>    m_pMaterialCB;
    ResMaterialData               m_materialData;

    string m_sourcePath;
    BoundingBox     m_boundingBox;
};

