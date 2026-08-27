#include "vtkLidarPacketInterpreter.h"

#include "vtkNew.h"
#include "vtkPolyData.h"
#include "vtkIntArray.h"
#include "vtkPoints.h"

// Minimal concrete subclass so we can exercise the base filter.
class TestInterpreter : public vtkLidarPacketInterpreter
{
public:
  vtkTypeMacro(TestInterpreter, vtkLidarPacketInterpreter);
  static TestInterpreter* New();
  bool PreProcessPacket(const unsigned char*, unsigned int, double&) override { return false; }
  bool IsLidarPacket(const unsigned char*, unsigned int) override { return false; }
  vtkSmartPointer<vtkPolyData> CreateNewEmptyFrame(vtkIdType, vtkIdType) override
  {
    return vtkSmartPointer<vtkPolyData>::New();
  }
  void ProcessPacket(const unsigned char*, unsigned int) override {}
  void ProcessPacketWrapped(const unsigned char*, unsigned int, double) override {}
};
vtkStandardNewMacro(TestInterpreter);

int TestLaserSelection(int, char*[])
{
  vtkNew<TestInterpreter> interp;

  // Build a polydata with 5 points carrying laser_id [0,1,2,0,1].
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

  // Disable channel 1 only.
  interp->SetLaserSelection(1, 0);

  interp->ApplyLaserSelection(pd.GetPointer());

  vtkIntArray* out = vtkIntArray::SafeDownCast(pd->GetPointData()->GetArray("laser_id"));
  if (!out)
  {
    return 1;
  }
  // Two points (indices 1 and 4) carry channel 1 and must be removed -> 3 left.
  if (out->GetNumberOfTuples() != 3)
  {
    return 1;
  }
  for (vtkIdType i = 0; i < out->GetNumberOfTuples(); ++i)
  {
    if (out->GetValue(i) == 1)
    {
      return 1; // channel 1 must be gone
    }
  }
  return 0;
}
