// ============================================================
// 功能：SenFoToView 新增功能 —— 激光通道选择 filter 实现；
//        RequestData 中按 laser_id 索引使能掩码，Drop 掉禁用通道的点。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-28
// ============================================================

#include "vtkLaserSelectionFilter.h"

#include <vtkCellArray.h>
#include <vtkIdTypeArray.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <vector>

vtkStandardNewMacro(vtkLaserSelectionFilter)

//-----------------------------------------------------------------------------
vtkLaserSelectionFilter::vtkLaserSelectionFilter()
{
  // Default: all channels enabled (pass-through).
  this->LaserSelection->Resize(128);
  for (vtkIdType i = 0; i < 128; ++i)
  {
    this->LaserSelection->InsertTuple1(i, 1);
  }
}

//-----------------------------------------------------------------------------
void vtkLaserSelectionFilter::SetLaserSelection(int index, int value)
{
  if (index < 0)
  {
    return;
  }
  if (index >= this->LaserSelection->GetNumberOfTuples())
  {
    const int oldSize = this->LaserSelection->GetNumberOfTuples();
    this->LaserSelection->Resize(index + 1);
    for (int i = oldSize; i <= index; ++i)
    {
      this->LaserSelection->InsertTuple1(i, 1);
    }
  }
  this->LaserSelection->SetTuple1(index, value ? 1 : 0);
  this->Modified();
}

//-----------------------------------------------------------------------------
vtkIntArray* vtkLaserSelectionFilter::GetLaserSelection()
{
  return this->LaserSelection.GetPointer();
}

//-----------------------------------------------------------------------------
int vtkLaserSelectionFilter::RequestData(
  vtkInformation*, vtkInformationVector** inputVec, vtkInformationVector* outputVec)
{
  vtkPolyData* input = vtkPolyData::GetData(inputVec[0], 0);
  vtkPolyData* output = vtkPolyData::GetData(outputVec, 0);
  if (!input || !output)
  {
    return 0;
  }

  const vtkIdType n = input->GetNumberOfPoints();
  vtkDataArray* laserId = input->GetPointData()->GetArray("laser_id");
  const vtkIdType maskSize = this->LaserSelection->GetNumberOfTuples();

  // Collect the points to keep (by their original index).
  std::vector<vtkIdType> kept;
  kept.reserve(n);
  for (vtkIdType i = 0; i < n; ++i)
  {
    bool selected = true;
    if (laserId)
    {
      const int id = static_cast<int>(laserId->GetTuple1(i));
      if (id >= 0 && id < maskSize)
      {
        selected = this->LaserSelection->GetValue(id) != 0;
      }
    }
    if (selected)
    {
      kept.push_back(i);
    }
  }

  // Nothing filtered out -> pass through unchanged.
  if (kept.size() == static_cast<size_t>(n))
  {
    output->ShallowCopy(input);
    return 1;
  }

  // Copy the kept points and their associated data.
  vtkPoints* inPts = input->GetPoints();
  vtkNew<vtkPoints> outPts;
  if (inPts)
  {
    outPts->SetDataType(inPts->GetDataType());
    outPts->SetNumberOfPoints(static_cast<vtkIdType>(kept.size()));
    for (vtkIdType j = 0; j < static_cast<vtkIdType>(kept.size()); ++j)
    {
      outPts->SetPoint(j, inPts->GetPoint(kept[j]));
    }
  }
  output->SetPoints(outPts.GetPointer());

  vtkPointData* inPD = input->GetPointData();
  vtkPointData* outPD = output->GetPointData();
  outPD->CopyAllocate(inPD, static_cast<vtkIdType>(kept.size()));
  for (vtkIdType j = 0; j < static_cast<vtkIdType>(kept.size()); ++j)
  {
    outPD->CopyData(inPD, kept[j], j);
  }

  // Rebuild the vertex cells so each kept point remains a standalone vertex.
  vtkNew<vtkCellArray> outVerts;
  for (vtkIdType j = 0; j < static_cast<vtkIdType>(kept.size()); ++j)
  {
    outVerts->InsertNextCell(1, &j);
  }
  output->SetVerts(outVerts.GetPointer());

  return 1;
}
