// ============================================================
// 功能：SenFoToView 新增功能 —— 激光通道选择（LaserSelection）filter 类声明；
//        按逐点 laser_id 数组 + 使能掩码，剔除被禁用的激光通道。
// 作者：acelan
// 新建时间：2026-08-28
// 修改时间：2026-08-28
// ============================================================

#ifndef vtkLaserSelectionFilter_h
#define vtkLaserSelectionFilter_h

#include "lvFiltersProcessingModule.h"
#include <vtkPolyDataAlgorithm.h>

class LVFILTERSPROCESSING_EXPORT vtkLaserSelectionFilter : public vtkPolyDataAlgorithm
{
public:
  static vtkLaserSelectionFilter* New();
  vtkTypeMacro(vtkLaserSelectionFilter, vtkPolyDataAlgorithm)

  /**
   * Per-laser enable/disable mask, indexed by the channel id (the value of the
   * per-point "laser_id" array). 1 = displayed, 0 = filtered out of the output.
   */
  void SetLaserSelection(int index, int value);
  vtkIntArray* GetLaserSelection();

protected:
  vtkLaserSelectionFilter();
  ~vtkLaserSelectionFilter() override = default;

  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
  vtkLaserSelectionFilter(const vtkLaserSelectionFilter&) = delete;
  void operator=(const vtkLaserSelectionFilter&) = delete;

  vtkNew<vtkIntArray> LaserSelection;
};

#endif
