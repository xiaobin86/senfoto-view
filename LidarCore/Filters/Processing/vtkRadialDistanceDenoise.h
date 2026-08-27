#ifndef vtkRadialDistanceDenoise_h
#define vtkRadialDistanceDenoise_h

#include "lvFiltersProcessingModule.h"
#include <unordered_map>
#include <vtkPolyData.h>
#include <vtkPolyDataAlgorithm.h>

class LVFILTERSPROCESSING_EXPORT vtkRadialDistanceDenoise : public vtkPolyDataAlgorithm
{
public:
  static vtkRadialDistanceDenoise* New();
  vtkTypeMacro(vtkRadialDistanceDenoise, vtkPolyDataAlgorithm)

  vtkSetMacro(Level1Enabled, bool);
  vtkGetMacro(Level1Enabled, bool);
  vtkSetMacro(Level1Threshold, double);
  vtkGetMacro(Level1Threshold, double);
  vtkSetMacro(Level2Enabled, bool);
  vtkGetMacro(Level2Enabled, bool);
  vtkSetMacro(Level2Threshold, double);
  vtkGetMacro(Level2Threshold, double);
  vtkSetMacro(NumberOfLasers, int);
  vtkGetMacro(NumberOfLasers, int);
  vtkSetMacro(AzimuthBinSize, double);
  vtkGetMacro(AzimuthBinSize, double);
  vtkSetStringMacro(DistanceArrayName);
  vtkGetStringMacro(DistanceArrayName);
  vtkSetStringMacro(LaserIdArrayName);
  vtkGetStringMacro(LaserIdArrayName);
  vtkSetStringMacro(AzimuthArrayName);
  vtkGetStringMacro(AzimuthArrayName);

protected:
  vtkRadialDistanceDenoise();
  ~vtkRadialDistanceDenoise() override
  {
    delete[] this->DistanceArrayName;
    delete[] this->LaserIdArrayName;
    delete[] this->AzimuthArrayName;
  }
  int FillInputPortInformation(int port, vtkInformation* info) override;
  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) override;

private:
  vtkRadialDistanceDenoise(const vtkRadialDistanceDenoise&) = delete;
  void operator=(const vtkRadialDistanceDenoise&) = delete;

  char* DistanceArrayName = nullptr;
  char* LaserIdArrayName = nullptr;
  char* AzimuthArrayName = nullptr;
  bool Level1Enabled = true;
  double Level1Threshold = 10.24;
  bool Level2Enabled = true;
  double Level2Threshold = 10.0;
  int NumberOfLasers = 96;
  double AzimuthBinSize = 0.1;
  std::unordered_map<long long, double> PrevRange;
};
#endif
