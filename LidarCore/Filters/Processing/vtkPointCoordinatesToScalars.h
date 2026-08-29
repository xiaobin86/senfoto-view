// ============================================================
// 功能：SenFoToView 新增功能 —— 将点云坐标 (x,y,z) 转为标量数组 X/Y/Z，
//        供染色工具栏按坐标染色。
// 作者：acelan
// 新建时间：2026-08-29
// 修改时间：2026-08-29
// ============================================================

#ifndef vtkPointCoordinatesToScalars_h
#define vtkPointCoordinatesToScalars_h

#include "lvFiltersProcessingModule.h"
#include <vtkPolyDataAlgorithm.h>

class LVFILTERSPROCESSING_EXPORT vtkPointCoordinatesToScalars : public vtkPolyDataAlgorithm
{
public:
  static vtkPointCoordinatesToScalars* New();
  vtkTypeMacro(vtkPointCoordinatesToScalars, vtkPolyDataAlgorithm)

  vtkSetMacro(GenerateX, bool);
  vtkGetMacro(GenerateX, bool);
  vtkSetMacro(GenerateY, bool);
  vtkGetMacro(GenerateY, bool);
  vtkSetMacro(GenerateZ, bool);
  vtkGetMacro(GenerateZ, bool);

protected:
  vtkPointCoordinatesToScalars();
  ~vtkPointCoordinatesToScalars() override = default;

  int FillInputPortInformation(int port, vtkInformation* info) override;
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
  vtkPointCoordinatesToScalars(const vtkPointCoordinatesToScalars&) = delete;
  void operator=(const vtkPointCoordinatesToScalars&) = delete;

  bool GenerateX = true;
  bool GenerateY = true;
  bool GenerateZ = true;
};

#endif
