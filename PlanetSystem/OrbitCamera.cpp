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

// При движении мышью меняем углы yaw/pitch
void OrbitCamera::Rotate(float deltaX, float deltaY)
{
    float rotationSpeed = 0.005f; // подберите под чувствительность
    m_yaw += deltaX * rotationSpeed;
    m_pitch += deltaY * rotationSpeed;

    // Ограничим pitch, чтобы камера «не переворачивалась»
    float limit = DirectX::XM_PIDIV2 - 0.1f;
    m_pitch = std::clamp(m_pitch, -limit, limit);
}

// Приближение/отдаление (колёсико мыши)
void OrbitCamera::Zoom(float deltaDist)
{
    float zoomSpeed = 1.0f; // подберите чувствительность
    m_distance -= deltaDist * zoomSpeed;
    if (m_distance < 1.0f) {
        m_distance = 1.0f; // минимальная дистанция
    }
    // Можно и верхний лимит поставить, если нужно
}

// Смена цели (например, пользователь кликнул по другой планете)
void OrbitCamera::SetTarget(const Vector3& newTarget)
{
    m_target = newTarget;
}

// Генерируем матрицу вида
Matrix OrbitCamera::GetViewMatrix() const
{
    // Исходная точка на сфере в координатах камеры:
    // При yaw=0,pitch=0 мы «смотрим» вдоль -Z.
    // Можем вычислить позиции в сферических координатах
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    // Направление камеры из target
    Vector3 cameraPos;
    cameraPos.x = m_target.x + m_distance * cosPitch * sinYaw; // (R * cosPitch * sinYaw)
    cameraPos.y = m_target.y + m_distance * sinPitch;          // (R * sinPitch)
    cameraPos.z = m_target.z + m_distance * cosPitch * cosYaw; // (R * cosPitch * cosYaw)

    Vector3 g_Up(0.0f, 1.0f, 0.0f);
    // Камера смотрит на target, «вверх» – всегда Y
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
