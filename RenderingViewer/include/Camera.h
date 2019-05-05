#pragma once

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
