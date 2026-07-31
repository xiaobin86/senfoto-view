#include "vtkHelper.h"

#include <vtkPolyLine.h>
#include <vtkVersion.h>

vtkSmartPointer<vtkPolyLine> CreatePolyLineFromPoints(const vtkSmartPointer<vtkPoints>& points)
{
  // Create new poly line with all points contains in the PolyData
  vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
  vtkIdType nbTotalPoints = points->GetNumberOfPoints();
  vtkIdList* polyIds = polyLine->GetPointIds();
#if VTK_VERSION_NUMBER >= VTK_VERSION_CHECK(9, 7, 0)
  polyIds->Reserve(nbTotalPoints);
#else
  polyIds->Allocate(nbTotalPoints);
#endif
  for (vtkIdType i = 0; i < nbTotalPoints; i++)
  {
    polyIds->InsertNextId(i);
  }
  return polyLine;
}
