#include "FpsCamera.h"
#include <algorithm>  // std::clamp

FpsCamera::FpsCamera()
{
    m_position = Vector3(0.0f, 0.0f, 0.0f);
    m_yaw = 0.0f;
    m_pitch = 0.0f;
}

void FpsCamera::Init(const Vector3& startPos, float startYaw, float startPitch)
{
    m_position = startPos;
    m_yaw = startYaw;
    m_pitch = startPitch;
}

// ��� �������� ����� ������ ���� yaw/pitch
void FpsCamera::Rotate(float dx, float dy)
{
    // ����������������
    float rotationSpeed = 0.005f;

    m_yaw += dx * rotationSpeed;
    m_pitch += dy * rotationSpeed;

    // ��������� pitch, ����� �� ����������������
    float limit = DirectX::XM_PIDIV2 - 0.1f; // ���� ������ 90�
    m_pitch = std::clamp(m_pitch, -limit, limit);
}

// ��������� �����/�����, �����/������ � �����/����
void FpsCamera::Move(float forward, float strafe, float upDown)
{
    // �������� ��������
    // ����� �������� ��� �� deltaTime, ���� ������ ������������� �� FPS
    float moveSpeed = 0.1f;

    // ����������� ������ ��� ������ � ������ yaw/pitch
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    // Forward-������ (�����������, ���� �������� ������)
    Vector3 forwardVec;
    forwardVec.x = sinYaw * cosPitch;
    forwardVec.y = sinPitch;
    forwardVec.z = cosYaw * cosPitch;

    // Right-������ (��������������� forward � ��� Y ��������)
    // ����� ��������� ��������, ��������� pitch:
    Vector3 rightVec;
    rightVec.x = cosYaw;
    rightVec.y = 0.0f;
    rightVec.z = -sinYaw;

    // Up-������ (������� �� ����, ������ �� �� ������ ����
    //  ��� ������ ��������� �� Y). ������ UP = (0,1,0) ����������.
    Vector3 upVec(0.0f, 1.0f, 0.0f);

    // ��������� ��������
    Vector3 moveDir = forward * forwardVec + strafe * rightVec + upDown * upVec;
    moveDir *= moveSpeed;

    // ��������� �������
    m_position += moveDir;
}

// ������� ����
Matrix FpsCamera::GetViewMatrix() const
{
    // �������, ���� �� ��������:
    // ��������� ������ + forward-������
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    Vector3 forwardVec(
        sinYaw * cosPitch,
        sinPitch,
        cosYaw * cosPitch
    );
    Vector3 lookAt = m_position + forwardVec;

    Vector3 up(0.0f, 1.0f, 0.0f);
    // ��� ��������� ����� pitch ��� ���������. 
    // ���� ������ ������ up, ����� ��������� �����-������� right/forward.

    return DirectX::XMMatrixLookAtLH(m_position, lookAt, up);
}
