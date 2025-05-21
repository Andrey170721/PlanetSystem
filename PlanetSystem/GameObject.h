#pragma once
#include "D3D11_Framework.h"
#include <directxmath.h>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <iostream>
#include <vector>

using namespace DirectX::SimpleMath;

struct GameObject
{
    MeshGPU mesh;               // ���������
    Matrix  local;              // ����������� ��������������
    DirectX::BoundingSphere bs;          // ��������� (!) �����
    bool    attached = false;   // ��� ������������ � ����?
};

struct KatamariBall
{
    DirectX::BoundingSphere bs;          // �����-���������
    Matrix         world;       // ������� transform (�������� + �������)
    float          growRate = 0.1f;   // �� ������� ������ ����� ��� ���������
    float          visualRadius = 0.8f; // ����. ���������� ������
    Quaternion     orientation = Quaternion::Identity;
};