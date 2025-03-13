#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class FpsCamera
{
public:
    FpsCamera();

    // Инициализация камеры: позиция и углы вращения (yaw, pitch)
    void Init(const Vector3& startPos, float startYaw = 0.0f, float startPitch = 0.0f);

    // Обработка вращения мышью (dx, dy - смещения по X, Y)
    void Rotate(float dx, float dy);

    // Движение вперёд (forward), вбок (strafe) и вертикально (upDown)
    // Например, forward>0 -> вперёд, <0 -> назад
    void Move(float forward, float strafe, float upDown);

    // Получить матрицу вида (ViewMatrix)
    Matrix GetViewMatrix() const;

    // Геттеры/сеттеры при необходимости
    Vector3 GetPosition() const { return m_position; }
    void    SetPosition(const Vector3& pos) { m_position = pos; }

private:
    Vector3 m_position;  // Позиция камеры в мире
    float   m_yaw;       // Вращение вокруг оси Y (в радианах)
    float   m_pitch;     // Угол наклона камеры (в радианах)
};
