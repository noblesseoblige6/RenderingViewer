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
