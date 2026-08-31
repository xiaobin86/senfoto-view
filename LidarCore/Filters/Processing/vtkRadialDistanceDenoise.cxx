// ============================================================
// 功能：SenFoToView 新增功能 —— 径向距离去噪 filter 实现；
//        RequestData 中执行二级尖峰检测 + 一级跨帧范围缓存 + Drop 输出。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-31
// ============================================================

#include "vtkRadialDistanceDenoise.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkDoubleArray.h>
#include <vtkIdTypeArray.h>
#include <vtkInformation.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkRemovePolyData.h>
#include <vtkUnsignedCharArray.h>

constexpr int POINTS_INPUT_PORT = 0;
constexpr int OUTPUT_PORT = 0;

vtkStandardNewMacro(vtkRadialDistanceDenoise)

vtkRadialDistanceDenoise::vtkRadialDistanceDenoise()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->SetDistanceArrayName("distance_m");
  this->SetLaserIdArrayName("laser_id");
  this->SetAzimuthArrayName("azimuth");
}

int vtkRadialDistanceDenoise::FillInputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  return 1;
}

int vtkRadialDistanceDenoise::RequestData(vtkInformation*,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkPolyData* input = vtkPolyData::GetData(inputVector[POINTS_INPUT_PORT], 0);
  vtkPolyData* output = vtkPolyData::GetData(outputVector, OUTPUT_PORT);
  if (!input)
    return 0;

  if (!this->DistanceArrayName || !this->LaserIdArrayName || !this->AzimuthArrayName)
  {
    output->ShallowCopy(input);
    return 1;
  }

  vtkDataArray* distArr = input->GetPointData()->GetArray(this->DistanceArrayName);
  vtkDataArray* laserArr = input->GetPointData()->GetArray(this->LaserIdArrayName);
  vtkDataArray* azimArr = input->GetPointData()->GetArray(this->AzimuthArrayName);
  if (!distArr || !laserArr || !azimArr)
  {
    output->ShallowCopy(input); // 缺必需数组则原样透传
    return 1;
  }

  const vtkIdType n = input->GetNumberOfPoints();
  std::vector<char> drop(n, 0);

  // ===== 二级：同帧、同 laser_id、按方位角排序后左-中-右插值 =====
  if (this->Level2Enabled)
  {
    int maxLaser = this->NumberOfLasers - 1;
    for (vtkIdType i = 0; i < n; ++i)
      maxLaser = std::max(maxLaser, static_cast<int>(laserArr->GetTuple1(i)));
    std::vector<std::vector<vtkIdType>> byLaser(maxLaser + 1);
    for (vtkIdType i = 0; i < n; ++i)
      byLaser[static_cast<int>(laserArr->GetTuple1(i))].push_back(i);
    for (int lid = 0; lid <= maxLaser; ++lid)
    {
      auto& idx = byLaser[lid];
      std::sort(idx.begin(),
        idx.end(),
        [&](vtkIdType a, vtkIdType b) { return azimArr->GetTuple1(a) < azimArr->GetTuple1(b); });
      for (size_t k = 1; k + 1 < idx.size(); ++k)
      {
        double dMid = distArr->GetTuple1(idx[k]);
        double dLeft = distArr->GetTuple1(idx[k - 1]);
        double dRight = distArr->GetTuple1(idx[k + 1]);
        double expected = 0.5 * (dLeft + dRight);
        if (std::fabs(dMid - expected) > this->Level2Threshold)
          drop[idx[k]] = 1;
      }
    }
  }

  // ===== 一级：跨帧、keyed by (laser_id, 方位角分箱) =====
  std::map<std::pair<int, int>, double> newPrev;
  if (this->Level1Enabled)
  {
    double binSize = this->AzimuthBinSize > 0.0 ? this->AzimuthBinSize : 0.1;
    for (vtkIdType i = 0; i < n; ++i)
    {
      int lid = static_cast<int>(laserArr->GetTuple1(i));
      int bin = static_cast<int>(std::floor(azimArr->GetTuple1(i) / binSize));
      auto key = std::make_pair(lid, bin);
      double cur = distArr->GetTuple1(i);
      auto it = this->PrevRange.find(key);
      if (it != this->PrevRange.end() && std::fabs(it->second - cur) > this->Level1Threshold)
        drop[i] = 1;
      newPrev[key] = cur; // 双回波取末次；新帧覆盖旧基准
    }
    this->PrevRange.swap(newPrev);
  }
  else
  {
    this->PrevRange.clear();
  }

  // ===== 输出：剔除 Drop 点 =====
  vtkNew<vtkIdTypeArray> idsToRemove;
  for (vtkIdType i = 0; i < n; ++i)
    if (drop[i])
      idsToRemove->InsertNextValue(i);

  if (idsToRemove->GetNumberOfTuples() == 0)
  {
    output->ShallowCopy(input);
    return 1;
  }
  vtkNew<vtkRemovePolyData> remove;
  remove->SetInputData(input);
  remove->SetPointIds(idsToRemove);
  vtkNew<vtkCleanPolyData> clean;
  clean->SetInputConnection(remove->GetOutputPort());
  clean->PointMergingOff();
  clean->Update();
  output->ShallowCopy(clean->GetOutput());
  return 1;
}
