// ============================================================
// 功能：SenFoToView 新增功能 —— 将点云坐标 (x,y,z) 转为标量数组 X/Y/Z。
// 作者：acelan
// 新建时间：2026-08-29
// 修改时间：2026-08-29
// ============================================================

#include "vtkPointCoordinatesToScalars.h"

#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

vtkStandardNewMacro(vtkPointCoordinatesToScalars)

//-----------------------------------------------------------------------------
vtkPointCoordinatesToScalars::vtkPointCoordinatesToScalars()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

//-----------------------------------------------------------------------------
int vtkPointCoordinatesToScalars::FillInputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
  return 1;
}

//-----------------------------------------------------------------------------
int vtkPointCoordinatesToScalars::RequestData(vtkInformation*,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkPolyData* input = vtkPolyData::GetData(inputVector[0], 0);
  vtkPolyData* output = vtkPolyData::GetData(outputVector, 0);
  if (!input || !output)
  {
    return 0;
  }

  // Start with a shallow copy so all input arrays and cells pass through.
  output->ShallowCopy(input);

  vtkPoints* points = input->GetPoints();
  if (!points)
  {
    return 1;
  }

  const vtkIdType n = points->GetNumberOfPoints();
  if (n == 0)
  {
    return 1;
  }

  vtkPointData* pd = output->GetPointData();

  auto generate = [&](const char* name, int comp) -> void {
    vtkNew<vtkDoubleArray> arr;
    arr->SetName(name);
    arr->SetNumberOfComponents(1);
    arr->SetNumberOfTuples(n);
    for (vtkIdType i = 0; i < n; ++i)
    {
      double p[3];
      points->GetPoint(i, p);
      arr->SetValue(i, p[comp]);
    }
    pd->AddArray(arr.GetPointer());
  };

  if (this->GenerateX)
  {
    generate("X", 0);
  }
  if (this->GenerateY)
  {
    generate("Y", 1);
  }
  if (this->GenerateZ)
  {
    generate("Z", 2);
  }

  return 1;
}
