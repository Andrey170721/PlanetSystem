#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class FpsCamera
{
public:
    FpsCamera();

    // ������������� ������: ������� � ���� �������� (yaw, pitch)
    void Init(const Vector3& startPos, float startYaw = 0.0f, float startPitch = 0.0f);

    // ��������� �������� ����� (dx, dy - �������� �� X, Y)
    void Rotate(float dx, float dy);

    // �������� ����� (forward), ���� (strafe) � ����������� (upDown)
    // ��������, forward>0 -> �����, <0 -> �����
    void Move(float forward, float strafe, float upDown);

    // �������� ������� ���� (ViewMatrix)
    Matrix GetViewMatrix() const;

    // �������/������� ��� �������������
    Vector3 GetPosition() const { return m_position; }
    void    SetPosition(const Vector3& pos) { m_position = pos; }

private:
    Vector3 m_position;  // ������� ������ � ����
    float   m_yaw;       // �������� ������ ��� Y (� ��������)
    float   m_pitch;     // ���� ������� ������ (� ��������)
};
