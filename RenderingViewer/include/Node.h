#pragma once
class Node
{
public:
    enum NODE_TYPE
    {
        NODE_TYPE_NODE = 01,
        NODE_TYPE_CAMERA,
        NODE_TYPE_LIGHT,
        NODE_TYPE_MODEL,
        NODE_TYPE_NUM,
    };

public:
    Node( ID3D12Device* pDevice );
    ~Node();

public:
    NODE_TYPE GetNodeType() const { return m_nodeType; }
    bool IsNodeType( NODE_TYPE type ) const { return type == m_nodeType; }

    const Vec3f& GetPosition() const { return m_position; }
    void SetPosition(const Vec3f& position) { m_position = position; }

    const Vec3f& GetScale() const { return m_scale; }
    void SetScale( const Vec3f& scale ) { m_scale = scale; }

    const Vec3f& GetRotate() const { return m_rotate; }
    void SetRotate( const Vec3f& rotate ) { m_rotate = rotate; }

    shared_ptr<Node> GetParent() const { return m_pParent; }

    shared_ptr<Node> GetChild(int index) const { return m_pChildren[index]; }
    const vector<shared_ptr<Node> >& GetChildren() const { return m_pChildren; }

    void AddChild( shared_ptr<Node> pNode );

    void RemoveChild( shared_ptr<Node> pNode );
    void RemoveChild( int index );

    virtual bool BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap );

    virtual void UpdateGPUBuffer();

protected:
    virtual bool CreateCB();

protected:
    NODE_TYPE m_nodeType;

    Vec3f m_position;
    Vec3f m_scale;
    Vec3f m_rotate;

    shared_ptr<Node>          m_pParent;
    vector<shared_ptr<Node> > m_pChildren;
};

class Camera : public Node
{
public:
    struct ResTransformBuffer
    {
        Mat44f world;
        Mat44f view;
        Mat44f projection;

        DWORD size;
    };

public:
    Camera( ID3D12Device* pDevice );
    ~Camera();

public:
    const Vec3f& GetLookAt() const { return m_lookAt; }
    void SetLookAt( const Vec3f& lookAt ) { m_lookAt = lookAt; }

    const Mat44f& GetViewMatrix() const { return m_viewMatrix; }
    void SetViewMatrix( const Mat44f& viewMatrix ) { m_viewMatrix = viewMatrix; }

    const Mat44f& GetProjectionMatrix() const { return m_projectionMatrix; }
    void SetProjectionMatrix( const Mat44f& projectionMatrix ) { m_projectionMatrix = projectionMatrix; }

    const Mat33f& GetPoseMatrix() const { return m_poseMatrix; }
    void SetPoseMatrix( const Mat33f& poseMatrix ) { m_poseMatrix = poseMatrix; }

    Vec3f GetLookDir() const { return Vec3f::normalize( m_lookAt - m_position ); }

    void CreateViewMatrix();

    virtual bool BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap );

    virtual void UpdateGPUBuffer();

public:
    ResTransformBuffer& GetBufferData() { return m_transformBufferData; }
    const ResTransformBuffer& GetBufferData() const { return m_transformBufferData; }

protected:
    virtual bool CreateCB( ID3D12Device* pDevice );

private:
    ResTransformBuffer            m_transformBufferData;
    shared_ptr<ConstantBuffer>    m_pCameraCB;

    Mat44f m_viewMatrix;
    Mat44f m_projectionMatrix;

    Mat33f m_poseMatrix;
    Vec3f  m_lookAt;
};

class Light : public Node
{
public:
    struct ResLightData
    {
        // Directinal Light
        static const int DIRECTIONAL_LIGHT_NUM = 1;
        Vec4f position[DIRECTIONAL_LIGHT_NUM];
        Vec4f color[DIRECTIONAL_LIGHT_NUM];

        Mat44f view[DIRECTIONAL_LIGHT_NUM];
        Mat44f projection[DIRECTIONAL_LIGHT_NUM];

        Vec3f direction[DIRECTIONAL_LIGHT_NUM];
        Vec3f intensity[DIRECTIONAL_LIGHT_NUM];
        //Mat44f lightVP[DIRECTIONAL_LIGHT_NUM];

        DWORD size;
    };

public:
    Light( ID3D12Device* pDevice );
    ~Light();

public:
    virtual bool BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap );

    virtual void UpdateGPUBuffer();

public:
    ResLightData & GetBufferData() { return m_lightBufferData; }
    const ResLightData& GetBufferData() const { return m_lightBufferData; }

protected:
    virtual bool CreateCB( ID3D12Device* pDevice );

private:
    ResLightData               m_lightBufferData;
    shared_ptr<ConstantBuffer> m_pLightCB;
};
