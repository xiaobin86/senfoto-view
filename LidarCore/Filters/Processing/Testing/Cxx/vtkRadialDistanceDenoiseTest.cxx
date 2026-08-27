#include "vtkRadialDistanceDenoise.h"
#include <iostream>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>

namespace
{
vtkSmartPointer<vtkPolyData> BuildFrame(vtkDoubleArray* distance,
  vtkUnsignedCharArray* laser,
  vtkDoubleArray* azimuth)
{
  vtkNew<vtkPolyData> pd;
  vtkNew<vtkPoints> pts;
  vtkIdType n = distance->GetNumberOfTuples();
  pts->SetNumberOfPoints(n);
  for (vtkIdType i = 0; i < n; ++i)
    pts->SetPoint(i, distance->GetValue(i), 0.0, 0.0); // 几何不参与去噪，占位即可
  pd->SetPoints(pts);
  pd->GetPointData()->AddArray(distance);
  pd->GetPointData()->AddArray(laser);
  pd->GetPointData()->AddArray(azimuth);
  vtkNew<vtkCellArray> verts;
  for (vtkIdType i = 0; i < n; ++i)
    verts->InsertNextCell(1, &i);
  pd->SetVerts(verts);
  return pd.GetPointer();
}
}

int vtkRadialDistanceDenoiseTest(int, char*[])
{
  // ---- 二级：单激光线上的尖峰应被剔除 ----
  vtkNew<vtkDoubleArray> dist;
  dist->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> laser;
  laser->SetName("laser_id");
  vtkNew<vtkDoubleArray> azim;
  azim->SetName("azimuth");
  double d[] = { 10, 10, 50, 10, 10 };
  double a[] = { 0, 1, 2, 3, 4 };
  unsigned char l[] = { 0, 0, 0, 0, 0 };
  for (int i = 0; i < 5; ++i)
  {
    dist->InsertNextValue(d[i]);
    laser->InsertNextValue(l[i]);
    azim->InsertNextValue(a[i]);
  }
  vtkNew<vtkPolyData> pd;
  pd->ShallowCopy(BuildFrame(dist, laser, azim));
  vtkNew<vtkRadialDistanceDenoise> f;
  f->SetInputData(pd);
  f->SetLevel1Enabled(false);
  f->SetLevel2Threshold(10.0);
  f->Update();
  if (f->GetOutput()->GetNumberOfPoints() != 2)
  {
    std::cerr << "L2 spike FAIL: " << f->GetOutput()->GetNumberOfPoints() << "\n";
    return 1;
  }

  // ---- 二级：线性斜坡中点应保留 ----
  vtkNew<vtkDoubleArray> dist2;
  dist2->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> laser2;
  laser2->SetName("laser_id");
  vtkNew<vtkDoubleArray> azim2;
  azim2->SetName("azimuth");
  double d2[] = { 0, 10, 20 };
  double a2[] = { 0, 1, 2 };
  unsigned char l2[] = { 0, 0, 0 };
  for (int i = 0; i < 3; ++i)
  {
    dist2->InsertNextValue(d2[i]);
    laser2->InsertNextValue(l2[i]);
    azim2->InsertNextValue(a2[i]);
  }
  vtkNew<vtkPolyData> pd2;
  pd2->ShallowCopy(BuildFrame(dist2, laser2, azim2));
  vtkNew<vtkRadialDistanceDenoise> f2;
  f2->SetInputData(pd2);
  f2->SetLevel1Enabled(false);
  f2->Update();
  if (f2->GetOutput()->GetNumberOfPoints() != 3)
  {
    std::cerr << "L2 ramp FAIL\n";
    return 1;
  }

  // ---- 一级：跨帧同 (laser_id,bin) 距差超阈应被剔除 ----
  vtkNew<vtkDoubleArray> dA;
  dA->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> lA;
  lA->SetName("laser_id");
  vtkNew<vtkDoubleArray> aA;
  aA->SetName("azimuth");
  dA->InsertNextValue(100.0);
  lA->InsertNextValue(0);
  aA->InsertNextValue(10.0);
  vtkNew<vtkPolyData> fr1;
  fr1->ShallowCopy(BuildFrame(dA, lA, aA));
  vtkNew<vtkRadialDistanceDenoise> f1;
  f1->SetInputData(fr1);
  f1->SetLevel2Enabled(false);
  f1->SetLevel1Threshold(10.24);
  f1->Update(); // 第一帧：无上一帧，保留 1 点，并写入 PrevRange
  vtkNew<vtkDoubleArray> dB;
  dB->SetName("distance_m");
  vtkNew<vtkUnsignedCharArray> lB;
  lB->SetName("laser_id");
  vtkNew<vtkDoubleArray> aB;
  aB->SetName("azimuth");
  dB->InsertNextValue(200.0);
  lB->InsertNextValue(0);
  aB->InsertNextValue(10.0);
  vtkNew<vtkPolyData> fr2;
  fr2->ShallowCopy(BuildFrame(dB, lB, aB));
  f1->SetInputData(fr2);
  f1->Update(); // 第二帧：与上一帧距差 100 > 10.24 -> 剔除
  if (f1->GetOutput()->GetNumberOfPoints() != 0)
  {
    std::cerr << "L1 FAIL: " << f1->GetOutput()->GetNumberOfPoints() << "\n";
    return 1;
  }

  std::cout << "PASS\n";
  return 0;
}
