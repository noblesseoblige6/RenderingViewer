#pragma once

using namespace acLib;
using namespace acLib::DX12;
using namespace std;

class Model
{
public:
    struct BoundingBox
    {
        BoundingBox()
            : maxPoint( Vec3f( FLT_MIN ) )
            , minPoint( Vec3f( FLT_MAX ) )
        {
        }

        Vec3f maxPoint;
        Vec3f minPoint;
    };

public:
    Model();
    ~Model();
    
    void Release();

    bool BindAsset( ID3D12Device* pDevice, const string& sourcePath );

    int GetIndexCount() const { return m_indexCount; }

    shared_ptr<VertexBuffer> GetVertexBuffer() const { return m_pVertexBuffer; }
    shared_ptr<IndexBuffer>  GetIndexBuffer() const { return m_pIndexBuffer; }

protected:
    void CreateVertexBuffer( ID3D12Device* pDevice, const acObjLoader& loader);
    void CreateIndexBuffer( ID3D12Device* pDevice, const acObjLoader& loader );
    void CreateBoundingBox( const acObjLoader& loader );

private:
    shared_ptr<VertexBuffer>    m_pVertexBuffer;
    shared_ptr<IndexBuffer>     m_pIndexBuffer;
    int                         m_indexCount;

    string m_sourcePath;
    BoundingBox     m_boundingBox;
};

