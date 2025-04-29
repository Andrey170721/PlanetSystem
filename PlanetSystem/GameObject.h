#pragma once
#include "D3D11_Framework.h"
#include <directxmath.h>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <iostream>
#include <vector>

using namespace DirectX::SimpleMath;

struct MeshGPU     // ссылки на VB/IB, их хватит для рисования
{
    ID3D11Buffer* vb = nullptr;
    ID3D11Buffer* ib = nullptr;
    UINT indexCount = 0;
    float bsRadius = 1.0f;
};

struct GameObject
{
    MeshGPU mesh;               // геометрия
    Matrix  local;              // собственное преобразование
    DirectX::BoundingSphere bs;          // локальная (!) сфера
    bool    attached = false;   // уже присоединили к шару?
};

struct KatamariBall
{
    DirectX::BoundingSphere bs;          // сфера-коллайдер
    Matrix         world;       // мировой transform (движение + масштаб)
    float          growRate = 0.1f;   // во сколько радиус растёт при “поедании”
    float          visualRadius = 0.8f; // фикс. визуальный радиус
    Quaternion     orientation = Quaternion::Identity;
};