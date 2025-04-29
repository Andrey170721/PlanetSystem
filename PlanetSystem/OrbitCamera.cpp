#include "OrbitCamera.h"
#include <algorithm> // std::clamp (C++17)


OrbitCamera::OrbitCamera()
{
    m_target = Vector3(0.0f, 0.0f, 0.0f);
    m_distance = 5.0f;
    m_yaw = 0.0f;
    m_pitch = 0.0f;
}

void OrbitCamera::Init(const Vector3& target, float distance)
{
    m_target = target;
    m_distance = distance;
    m_yaw = 0.0f;
    m_pitch = 0.0f;
}

// ��� �������� ����� ������ ���� yaw/pitch
void OrbitCamera::Rotate(float deltaX, float deltaY)
{
    float rotationSpeed = 0.005f; // ��������� ��� ����������������
    m_yaw += deltaX * rotationSpeed;
    m_pitch += deltaY * rotationSpeed;

    // ��������� pitch, ����� ������ ��� �����������������
    float limit = DirectX::XM_PIDIV2 - 0.1f;
    m_pitch = std::clamp(m_pitch, -limit, limit);
}

// �����������/��������� (������� ����)
void OrbitCamera::Zoom(float deltaDist)
{
    float zoomSpeed = 1.0f; // ��������� ����������������
    m_distance -= deltaDist * zoomSpeed;
    if (m_distance < 1.0f) {
        m_distance = 1.0f; // ����������� ���������
    }
    // ����� � ������� ����� ���������, ���� �����
}

// ����� ���� (��������, ������������ ������� �� ������ �������)
void OrbitCamera::SetTarget(const Vector3& newTarget)
{
    m_target = newTarget;
}

// ���������� ������� ����
Matrix OrbitCamera::GetViewMatrix() const
{
    // �������� ����� �� ����� � ����������� ������:
    // ��� yaw=0,pitch=0 �� �������� ����� -Z.
    // ����� ��������� ������� � ����������� �����������
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    // ����������� ������ �� target
    Vector3 cameraPos;
    cameraPos.x = m_target.x + m_distance * cosPitch * sinYaw; // (R * cosPitch * sinYaw)
    cameraPos.y = m_target.y + m_distance * sinPitch;          // (R * sinPitch)
    cameraPos.z = m_target.z + m_distance * cosPitch * cosYaw; // (R * cosPitch * cosYaw)

    Vector3 g_Up(0.0f, 1.0f, 0.0f);
    // ������ ������� �� target, ������� � ������ Y
    return DirectX::XMMatrixLookAtLH(cameraPos, m_target, g_Up);
}

Vector3 OrbitCamera::GetPosition() const
{
    float cosP = cosf(m_pitch), sinP = sinf(m_pitch);
    float cosY = cosf(m_yaw), sinY = sinf(m_yaw);

    Vector3 pos;
    pos.x = m_target.x + m_distance * cosP * sinY;
    pos.y = m_target.y + m_distance * sinP;
    pos.z = m_target.z + m_distance * cosP * cosY;
    return pos;
}
