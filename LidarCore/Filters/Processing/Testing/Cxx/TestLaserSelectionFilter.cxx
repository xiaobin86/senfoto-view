// ============================================================
// 功能：SenFoToView 新增功能 —— vtkLaserSelectionFilter 单元测试
//        （按 laser_id 掩码剔除禁用通道）。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-28
// ============================================================

#include "vtkLaserSelectionFilter.h"

#include <vtkCellArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

int TestLaserSelectionFilter(int, char*[])
{
  // 5 points carrying laser_id [0,1,2,0,1].
  vtkNew<vtkPolyData> pd;
  vtkNew<vtkPoints> pts;
  pts->SetNumberOfPoints(5);
  pd->SetPoints(pts.GetPointer());
  vtkNew<vtkIntArray> laserId;
  laserId->SetName("laser_id");
  laserId->SetNumberOfTuples(5);
  int ids[5] = { 0, 1, 2, 0, 1 };
  for (int i = 0; i < 5; ++i)
  {
    laserId->SetTuple1(i, ids[i]);
  }
  pd->GetPointData()->AddArray(laserId.GetPointer());
  vtkNew<vtkCellArray> verts;
  for (vtkIdType i = 0; i < 5; ++i)
  {
    verts->InsertNextCell(1, &i);
  }
  pd->SetVerts(verts.GetPointer());

  vtkNew<vtkLaserSelectionFilter> filter;
  // Disable channel 1 only.
  filter->SetLaserSelection(1, 0);
  filter->SetInputData(pd.GetPointer());
  filter->Update();

  vtkPolyData* out = filter->GetOutput();
  vtkIntArray* outId = vtkIntArray::SafeDownCast(out->GetPointData()->GetArray("laser_id"));
  if (!outId)
  {
    return 1;
  }
  // Two points (indices 1 and 4) carry channel 1 and must be removed -> 3 left.
  if (outId->GetNumberOfTuples() != 3)
  {
    return 1;
  }
  for (vtkIdType i = 0; i < outId->GetNumberOfTuples(); ++i)
  {
    if (outId->GetValue(i) == 1)
    {
      return 1; // channel 1 must be gone
    }
  }
  return 0;
}
