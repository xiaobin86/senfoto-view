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
