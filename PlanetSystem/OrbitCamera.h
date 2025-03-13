#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class OrbitCamera
{
public:
    OrbitCamera();

    // ������������� ���������� ������
    void Init(const Vector3& target, float distance);

    // ��������� �������� �����
    // deltaX, deltaY � �������� ������� (��������, �������)
    void Rotate(float deltaX, float deltaY);

    // �����������/��������� (������� ����, ��������)
    void Zoom(float deltaDist);

    // ��������� ����� ����� (������������ � ������ �������)
    void SetTarget(const Vector3& newTarget);

    // �������� ������� ���� ��� �������
    Matrix GetViewMatrix() const;

private:
    Vector3 m_target;   // ���������� ������ ��������
    float   m_distance; // ������� ��������� �� ������ �� target
    float   m_yaw;      // ���� �������� ������ Y
    float   m_pitch;    // ���� �������
};