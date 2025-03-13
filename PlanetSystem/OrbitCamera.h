#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class OrbitCamera
{
public:
    OrbitCamera();

    // Инициализация параметров камеры
    void Init(const Vector3& target, float distance);

    // Обработка вращения мышью
    // deltaX, deltaY – смещения курсора (например, пиксели)
    void Rotate(float deltaX, float deltaY);

    // Приближение/отдаление (колёсико мыши, например)
    void Zoom(float deltaDist);

    // Установка новой «цели» (прикрепиться к другой планете)
    void SetTarget(const Vector3& newTarget);

    // Получить матрицу вида для рендера
    Matrix GetViewMatrix() const;

private:
    Vector3 m_target;   // Координаты центра «орбиты»
    float   m_distance; // Текущая дистанция от камеры до target
    float   m_yaw;      // Угол поворота вокруг Y
    float   m_pitch;    // Угол наклона
};