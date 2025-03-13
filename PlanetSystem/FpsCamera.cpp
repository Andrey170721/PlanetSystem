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

// При вращении мышью меняем углы yaw/pitch
void FpsCamera::Rotate(float dx, float dy)
{
    // Чувствительность
    float rotationSpeed = 0.005f;

    m_yaw += dx * rotationSpeed;
    m_pitch += dy * rotationSpeed;

    // Ограничим pitch, чтобы не «выворачиваться»
    float limit = DirectX::XM_PIDIV2 - 0.1f; // чуть меньше 90°
    m_pitch = std::clamp(m_pitch, -limit, limit);
}

// Двигаемся вперёд/назад, влево/вправо и вверх/вниз
void FpsCamera::Move(float forward, float strafe, float upDown)
{
    // Скорость движения
    // Можно умножать ещё на deltaTime, если хотите независимость от FPS
    float moveSpeed = 0.1f;

    // Направление «вперёд» для камеры с учётом yaw/pitch
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);

    // Forward-вектор (направление, куда «смотрит» камера)
    Vector3 forwardVec;
    forwardVec.x = sinYaw * cosPitch;
    forwardVec.y = sinPitch;
    forwardVec.z = cosYaw * cosPitch;

    // Right-вектор (перпендикулярен forward и оси Y примерно)
    // Можно упрощённо получить, игнорируя pitch:
    Vector3 rightVec;
    rightVec.x = cosYaw;
    rightVec.y = 0.0f;
    rightVec.z = -sinYaw;

    // Up-вектор (зависит от того, хотите ли вы полный полёт
    //  или просто вертикаль по Y). Обычно UP = (0,1,0) фиксирован.
    Vector3 upVec(0.0f, 1.0f, 0.0f);

    // Суммарное движение
    Vector3 moveDir = forward * forwardVec + strafe * rightVec + upDown * upVec;
    moveDir *= moveSpeed;

    // Обновляем позицию
    m_position += moveDir;
}

// Матрица вида
Matrix FpsCamera::GetViewMatrix() const
{
    // Позиция, куда мы «смотрим»:
    // положение камеры + forward-вектор
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
    // При небольших углах pitch это нормально. 
    // Если хотите точный up, можно вычислять кросс-продукт right/forward.

    return DirectX::XMMatrixLookAtLH(m_position, lookAt, up);
}
