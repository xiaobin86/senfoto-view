/*=========================================================================

  Program: LidarView
  Module:  vtkNormalizeExternalSensorData.cxx

  Copyright (c) Kitware Inc.
  All rights reserved.
  See LICENSE or http://www.apache.org/licenses/LICENSE-2.0 for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkNormalizeExternalSensorData.h"

#include <vtkAbstractArray.h>
#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkLVUtilities.h>
#include <vtkLogger.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObject.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkTransform.h>

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkNormalizeExternalSensorData);
vtkCxxSetObjectMacro(vtkNormalizeExternalSensorData, SensorTransform, vtkTransform);
vtkCxxSetObjectMacro(vtkNormalizeExternalSensorData, IMUTransform, vtkTransform);
vtkCxxSetObjectMacro(vtkNormalizeExternalSensorData, OdometryTransform, vtkTransform);
vtkCxxSetObjectMacro(vtkNormalizeExternalSensorData, PoseTransform, vtkTransform);

namespace
{
//-----------------------------------------------------------------------------
inline std::string GetColumnNameIfExists(vtkTable* inTable,
  const std::string& column,
  const std::string& defaultName)
{
  if (inTable && inTable->GetColumnByName(column.c_str()))
  {
    return column;
  }
  if (!column.empty())
  {
    return defaultName;
  }
  return "";
}

//-----------------------------------------------------------------------------
double ScaleFromAccelUnit(vtkNormalizeExternalSensorData::AccelUnit u)
{
  switch (u)
  {
    case vtkNormalizeExternalSensorData::AccelUnit::MPS2:
      return 1.0;
    case vtkNormalizeExternalSensorData::AccelUnit::G:
      return 9.80665;
    case vtkNormalizeExternalSensorData::AccelUnit::MG:
      return 9.80665e-3;
    default:
      return 1.0;
  }
}

//-----------------------------------------------------------------------------
double ScaleFromGyroUnit(vtkNormalizeExternalSensorData::GyroUnit u)
{
  switch (u)
  {
    case vtkNormalizeExternalSensorData::GyroUnit::RAD_S:
      return 1.0;
    case vtkNormalizeExternalSensorData::GyroUnit::DEG_S:
      return vtkMath::Pi() / 180.0;
    default:
      return 1.0;
  }
}

//-----------------------------------------------------------------------------
double ScaleFromDistUnit(vtkNormalizeExternalSensorData::DistanceUnit u)
{
  switch (u)
  {
    case vtkNormalizeExternalSensorData::DistanceUnit::M:
      return 1.0;
    case vtkNormalizeExternalSensorData::DistanceUnit::CM:
      return 0.01;
    case vtkNormalizeExternalSensorData::DistanceUnit::MM:
      return 0.001;
    default:
      return 1.0;
  }
}

//-----------------------------------------------------------------------------
double ScaleFromAngleUnit(vtkNormalizeExternalSensorData::AngleUnit u)
{
  switch (u)
  {
    case vtkNormalizeExternalSensorData::AngleUnit::RAD:
      return 1.0;
    case vtkNormalizeExternalSensorData::AngleUnit::DEG:
      return vtkMath::Pi() / 180.0;
    default:
      return 1.0;
  }
}

//-----------------------------------------------------------------------------
// Converts a numeric source column into one or more normalized scalar columns.
// If dstNames contains one name, only the first component is copied.
// If dstNames contains N names, the source must have N components and is split
// into N scalar columns.
bool ConvertColumns(vtkTable* dst,
  vtkTable* src,
  const std::string& srcName,
  const std::vector<std::string>& dstNames,
  double scale)
{
  if (!src || srcName.empty() || dstNames.empty())
  {
    return false;
  }

  vtkAbstractArray* aa = src->GetColumnByName(srcName.c_str());
  if (!aa)
  {
    return false;
  }
  auto* dataArray = vtkDataArray::SafeDownCast(aa);
  if (!dataArray)
  {
    vtkLog(WARNING,
      << "NormalizeExternalSensorData: column '" << srcName << "' is not a numeric array.");
    return false;
  }

  const int numOutputs = static_cast<int>(dstNames.size());
  if (numOutputs != 1 && numOutputs != dataArray->GetNumberOfComponents())
  {
    vtkLog(WARNING,
      << "NormalizeExternalSensorData: column '" << srcName << "' has "
      << dataArray->GetNumberOfComponents() << " components but " << numOutputs
      << " output names were requested.");
    return false;
  }

  // If scaling factor is 1.0 (pure copy), or already float/double type,
  // preserve the original numeric type.
  const int dtype = dataArray->GetDataType();
  const bool preserveType = scale == 1.0 || dtype == VTK_FLOAT || dtype == VTK_DOUBLE;
  const int outType = preserveType ? dtype : VTK_DOUBLE;

  // Pre-allocate all output arrays
  std::vector<vtkSmartPointer<vtkDataArray>> outArrays(numOutputs);
  const vtkIdType numTuples = dataArray->GetNumberOfTuples();
  for (int idOutput = 0; idOutput < numOutputs; ++idOutput)
  {
    outArrays[idOutput].TakeReference(vtkDataArray::CreateDataArray(outType));
    outArrays[idOutput]->SetName(dstNames[idOutput].c_str());
    outArrays[idOutput]->SetNumberOfComponents(1);
    outArrays[idOutput]->SetNumberOfTuples(numTuples);
  }

  std::vector<double> tuple(numOutputs);
  for (vtkIdType i = 0; i < numTuples; ++i)
  {
    dataArray->GetTuple(i, tuple.data());
    for (int idOutput = 0; idOutput < numOutputs; ++idOutput)
    {
      outArrays[idOutput]->SetTuple1(i, tuple[idOutput] * scale);
    }
  }

  for (auto& arr : outArrays)
  {
    dst->AddColumn(arr);
  }
  return true;
}

//-----------------------------------------------------------------------------
bool ConvertSensorChannels(vtkTable* dst,
  vtkTable* src,
  const std::string& vectorColumn,
  const std::vector<std::string>& scalarColumns,
  const std::vector<std::string>& dstNames,
  double scale)
{
  // If the input composes by a column of vector
  if (!vectorColumn.empty())
  {
    return ::ConvertColumns(dst, src, vectorColumn, dstNames, scale);
  }

  // If the input composes by individual scalar columns.
  if (scalarColumns.size() != dstNames.size())
  {
    return false;
  }

  bool converted = false;
  for (size_t i = 0; i < scalarColumns.size(); ++i)
  {
    converted &= ::ConvertColumns(dst, src, scalarColumns[i], { dstNames[i] }, scale);
  }

  return converted;
}

//-----------------------------------------------------------------------------
void AddColumnAndScale(vtkTable* out, const std::string& columnName, double value, double scale)
{
  // Check if array already exist
  vtkAbstractArray* array = out->GetColumnByName(columnName.c_str());
  if (array)
  {
    // If column is a numeric array (vtkDataArray or subclass)
    if (auto dataArray = vtkDataArray::SafeDownCast(array))
    {
      dataArray->InsertNextTuple1(value * scale);
    }
    else
    {
      vtkLog(ERROR, << "Trying to append data on non-numeric array!");
    }
  }
  else
  {
    vtkSmartPointer<vtkDoubleArray> newArray = vtkSmartPointer<vtkDoubleArray>::New();
    newArray->SetNumberOfComponents(0);
    newArray->SetNumberOfValues(1);
    newArray->SetValue(0, value * scale);
    newArray->SetName(columnName.c_str());
    out->AddColumn(newArray);
  }
}
} // namespace

//-----------------------------------------------------------------------------
vtkNormalizeExternalSensorData::~vtkNormalizeExternalSensorData()
{
  this->SetSensorTransform(nullptr);
  this->SetIMUTransform(nullptr);
  this->SetOdometryTransform(nullptr);
  this->SetPoseTransform(nullptr);
}

//-----------------------------------------------------------------------------
int vtkNormalizeExternalSensorData::FillInputPortInformation(int vtkNotUsed(port),
  vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkTable");
  // Set input optional here to avoid warning, in vtkExternalSensorReader,
  // but this is not really optional.
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//-----------------------------------------------------------------------------
vtkStringArray* vtkNormalizeExternalSensorData::GetHeaderColumns()
{
  if (!this->HeaderColumnsCacheArray)
  {
    this->HeaderColumnsCacheArray = vtkSmartPointer<vtkStringArray>::New();
  }
  vtkStringArray* arr = this->HeaderColumnsCacheArray;
  arr->Initialize();

  vtkTable* inTable = vtkTable::SafeDownCast(this->GetInputDataObject(0, 0));
  if (!inTable)
  {
    return arr;
  }

  const int ncols = inTable->GetNumberOfColumns();
  for (int i = 0; i < ncols; ++i)
  {
    vtkAbstractArray* aa = inTable->GetColumn(i);
    const char* name = aa ? aa->GetName() : nullptr;
    if (name && *name)
    {
      arr->InsertNextValue(name);
    }
  }
  return arr;
}

//-----------------------------------------------------------------------------
int vtkNormalizeExternalSensorData::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkTable* inTable = vtkTable::GetData(inputVector[0]->GetInformationObject(0));
  vtkTable* output = vtkTable::GetData(outputVector->GetInformationObject(0));
  output->Initialize();

  if (!inTable || inTable->GetNumberOfRows() == 0)
  {
    vtkWarningMacro(<< "Empty or invalid input table");
    return 1;
  }

  if (this->UseIMU)
  {
    // Acceleration (to m/s^2)
    auto accelUnit =
      static_cast<vtkNormalizeExternalSensorData::AccelUnit>(this->IMUAccelerationUnits);
    double accelScale = ::ScaleFromAccelUnit(accelUnit);

    std::string ax = ::GetColumnNameIfExists(inTable, this->IMUAccXColumn, IMU_ACC_X_ARRAY_NAME());
    std::string ay = ::GetColumnNameIfExists(inTable, this->IMUAccYColumn, IMU_ACC_Y_ARRAY_NAME());
    std::string az = ::GetColumnNameIfExists(inTable, this->IMUAccZColumn, IMU_ACC_Z_ARRAY_NAME());
    std::string aVec = ::GetColumnNameIfExists(inTable, this->IMUAccVectorColumn, "");

    ::ConvertSensorChannels(output,
      inTable,
      aVec,
      { ax, ay, az },
      { IMU_ACC_X_ARRAY_NAME(), IMU_ACC_Y_ARRAY_NAME(), IMU_ACC_Z_ARRAY_NAME() },
      accelScale);

    // Gyro (to rad/s)
    auto gyroUnit = static_cast<vtkNormalizeExternalSensorData::GyroUnit>(this->IMUGyroUnits);
    double gyroScale = ::ScaleFromGyroUnit(gyroUnit);

    auto wx = ::GetColumnNameIfExists(inTable, this->IMUGyroXColumn, IMU_GYRO_X_ARRAY_NAME());
    auto wy = ::GetColumnNameIfExists(inTable, this->IMUGyroYColumn, IMU_GYRO_Y_ARRAY_NAME());
    auto wz = ::GetColumnNameIfExists(inTable, this->IMUGyroZColumn, IMU_GYRO_Z_ARRAY_NAME());
    auto wVec = ::GetColumnNameIfExists(inTable, this->IMUGyroVectorColumn, "");

    ::ConvertSensorChannels(output,
      inTable,
      wVec,
      { wx, wy, wz },
      { IMU_GYRO_X_ARRAY_NAME(), IMU_GYRO_Y_ARRAY_NAME(), IMU_GYRO_Z_ARRAY_NAME() },
      gyroScale);
  }

  if (this->UseGNSS)
  {
    // Position only (to meters)
    auto posUnit =
      static_cast<vtkNormalizeExternalSensorData::DistanceUnit>(this->GNSSPositionUnits);
    double posScale = ScaleFromDistUnit(posUnit);

    std::string px = ::GetColumnNameIfExists(inTable, this->GNSSXColumn, GNSS_POS_X_ARRAY_NAME());
    std::string py = ::GetColumnNameIfExists(inTable, this->GNSSYColumn, GNSS_POS_Y_ARRAY_NAME());
    std::string pz = ::GetColumnNameIfExists(inTable, this->GNSSZColumn, GNSS_POS_Z_ARRAY_NAME());
    std::string pVec = ::GetColumnNameIfExists(inTable, this->GNSSVectorColumn, "");

    ::ConvertSensorChannels(output,
      inTable,
      pVec,
      { px, py, pz },
      { GNSS_POS_X_ARRAY_NAME(), GNSS_POS_Y_ARRAY_NAME(), GNSS_POS_Z_ARRAY_NAME() },
      posScale);

    if (this->UseConfidenceError)
    {
      std::string pxError =
        ::GetColumnNameIfExists(inTable, this->GNSSXErrorColumn, GNSS_POS_X_ERROR_ARRAY_NAME());
      std::string pyError =
        ::GetColumnNameIfExists(inTable, this->GNSSYErrorColumn, GNSS_POS_Y_ERROR_ARRAY_NAME());
      std::string pzError =
        ::GetColumnNameIfExists(inTable, this->GNSSZErrorColumn, GNSS_POS_Z_ERROR_ARRAY_NAME());

      ::ConvertSensorChannels(output,
        inTable,
        "",
        { pxError, pyError, pzError },
        { GNSS_POS_X_ERROR_ARRAY_NAME(),
          GNSS_POS_Y_ERROR_ARRAY_NAME(),
          GNSS_POS_Z_ERROR_ARRAY_NAME() },
        1);
    }
  }

  if (this->UseINS)
  {
    // Position (to meters)
    auto posUnit =
      static_cast<vtkNormalizeExternalSensorData::DistanceUnit>(this->GNSSPositionUnits);
    double posScale = ::ScaleFromDistUnit(posUnit);

    std::string px = ::GetColumnNameIfExists(inTable, this->GNSSXColumn, GNSS_POS_X_ARRAY_NAME());
    std::string py = ::GetColumnNameIfExists(inTable, this->GNSSYColumn, GNSS_POS_Y_ARRAY_NAME());
    std::string pz = ::GetColumnNameIfExists(inTable, this->GNSSZColumn, GNSS_POS_Z_ARRAY_NAME());
    std::string pVec = ::GetColumnNameIfExists(inTable, this->GNSSVectorColumn, "");

    ::ConvertSensorChannels(output,
      inTable,
      pVec,
      { px, py, pz },
      { GNSS_POS_X_ARRAY_NAME(), GNSS_POS_Y_ARRAY_NAME(), GNSS_POS_Z_ARRAY_NAME() },
      posScale);

    // Euler angles (to radians)
    auto angUnit = static_cast<vtkNormalizeExternalSensorData::AngleUnit>(this->GNSSEulerUnits);
    double angScale = ::ScaleFromAngleUnit(angUnit);

    std::string rx = ::GetColumnNameIfExists(inTable, this->RollColumn, INS_ANGLE_RX_ARRAY_NAME());
    std::string ry = ::GetColumnNameIfExists(inTable, this->PitchColumn, INS_ANGLE_RY_ARRAY_NAME());
    std::string rz = ::GetColumnNameIfExists(inTable, this->YawColumn, INS_ANGLE_RZ_ARRAY_NAME());
    std::string rVec = ::GetColumnNameIfExists(inTable, this->OrientationVectorColumn, "");

    ::ConvertSensorChannels(output,
      inTable,
      rVec,
      { rx, ry, rz },
      { INS_ANGLE_RX_ARRAY_NAME(), INS_ANGLE_RY_ARRAY_NAME(), INS_ANGLE_RZ_ARRAY_NAME() },
      angScale);

    if (this->UseConfidenceError)
    {
      std::string pxError =
        ::GetColumnNameIfExists(inTable, this->GNSSXErrorColumn, GNSS_POS_X_ERROR_ARRAY_NAME());
      std::string pyError =
        ::GetColumnNameIfExists(inTable, this->GNSSYErrorColumn, GNSS_POS_Y_ERROR_ARRAY_NAME());
      std::string pzError =
        ::GetColumnNameIfExists(inTable, this->GNSSZErrorColumn, GNSS_POS_Z_ERROR_ARRAY_NAME());

      ::ConvertSensorChannels(output,
        inTable,
        "",
        { pxError, pyError, pzError },
        { GNSS_POS_X_ERROR_ARRAY_NAME(),
          GNSS_POS_Y_ERROR_ARRAY_NAME(),
          GNSS_POS_Z_ERROR_ARRAY_NAME() },
        1);

      std::string rxError =
        ::GetColumnNameIfExists(inTable, this->RollErrorColumn, INS_ANGLE_RX_ERROR_ARRAY_NAME());
      std::string ryError =
        ::GetColumnNameIfExists(inTable, this->PitchErrorColumn, INS_ANGLE_RY_ERROR_ARRAY_NAME());
      std::string rzError =
        ::GetColumnNameIfExists(inTable, this->YawErrorColumn, INS_ANGLE_RZ_ERROR_ARRAY_NAME());

      ::ConvertSensorChannels(output,
        inTable,
        "",
        { rxError, ryError, rzError },
        { INS_ANGLE_RX_ERROR_ARRAY_NAME(),
          INS_ANGLE_RY_ERROR_ARRAY_NAME(),
          INS_ANGLE_RZ_ERROR_ARRAY_NAME() },
        1);
    }
  }

  if (this->UseOdometry)
  {
    // Distance (to meters)
    auto odUnit = static_cast<vtkNormalizeExternalSensorData::DistanceUnit>(this->OdometryUnits);
    double odScale = ::ScaleFromDistUnit(odUnit);

    std::string od =
      ::GetColumnNameIfExists(inTable, this->OdometryColumn, ODOMETRY_DISTANCE_ARRAY_NAME());

    ::ConvertColumns(output, inTable, od, { ODOMETRY_DISTANCE_ARRAY_NAME() }, odScale);
  }

  // Time column pass-through (no units conversion).
  {
    std::string tname = ::GetColumnNameIfExists(inTable, this->TimeColumn, TIME_ARRAY_NAME());

    ::ConvertColumns(output, inTable, tname, { TIME_ARRAY_NAME() }, 1.0);
  }

  if (output->GetNumberOfColumns() == 0)
  {
    vtkWarningMacro(<< "No matching columns found or no sensors enabled; output table is empty.");
  }

  // Propagate field data if any (e.g., calibration matrices from upstream)
  if (inTable && inTable->GetFieldData())
  {
    output->GetFieldData()->ShallowCopy(inTable->GetFieldData());
  }

  // Store selected sensors' calibration transforms into FieldData (4x4 each),
  // matching reader behavior (IMU/GNSS/Odometry use per-sensor transform or fallback to
  // SensorTransform)
  if (this->UseIMU)
  {
    vtkTransform* t = this->IMUTransform ? this->IMUTransform : this->SensorTransform;
    vtkNormalizeExternalSensorData::SetTransformInFieldData(output, t, CALIBRATION_IMU_NAME());
  }
  if (this->UseGNSS)
  {
    vtkTransform* t = this->PoseTransform ? this->PoseTransform : this->SensorTransform;
    vtkNormalizeExternalSensorData::SetTransformInFieldData(output, t, CALIBRATION_GNSS_NAME());
  }
  if (this->UseINS)
  {
    vtkTransform* t = this->PoseTransform ? this->PoseTransform : this->SensorTransform;
    vtkNormalizeExternalSensorData::SetTransformInFieldData(output, t, CALIBRATION_INS_NAME());
  }
  if (this->UseOdometry)
  {
    vtkTransform* t = this->OdometryTransform ? this->OdometryTransform : this->SensorTransform;
    vtkNormalizeExternalSensorData::SetTransformInFieldData(output, t, CALIBRATION_ODOMETRY_NAME());
  }

  return 1;
}

//-----------------------------------------------------------------------------
vtkMTimeType vtkNormalizeExternalSensorData::GetMTime()
{
  vtkMTimeType mt = this->Superclass::GetMTime();
  if (this->SensorTransform)
  {
    mt = std::max(mt, this->SensorTransform->GetMTime());
  }
  if (this->IMUTransform)
  {
    mt = std::max(mt, this->IMUTransform->GetMTime());
  }
  if (this->PoseTransform)
  {
    mt = std::max(mt, this->PoseTransform->GetMTime());
  }
  if (this->OdometryTransform)
  {
    mt = std::max(mt, this->OdometryTransform->GetMTime());
  }
  return mt;
}

//-----------------------------------------------------------------------------
void vtkNormalizeExternalSensorData::AppendIMUDataToTable(vtkTable* out,
  const vtkVector3d& acc,
  const vtkVector3d& gyro,
  AccelUnit accUnit,
  GyroUnit gyroUnit)
{
  double accelScale = ::ScaleFromAccelUnit(accUnit);
  ::AddColumnAndScale(out, IMU_ACC_X_ARRAY_NAME(), acc.GetX(), accelScale);
  ::AddColumnAndScale(out, IMU_ACC_Y_ARRAY_NAME(), acc.GetY(), accelScale);
  ::AddColumnAndScale(out, IMU_ACC_Z_ARRAY_NAME(), acc.GetZ(), accelScale);

  double gyroScale = ::ScaleFromGyroUnit(gyroUnit);
  ::AddColumnAndScale(out, IMU_GYRO_X_ARRAY_NAME(), gyro.GetX(), gyroScale);
  ::AddColumnAndScale(out, IMU_GYRO_Y_ARRAY_NAME(), gyro.GetY(), gyroScale);
  ::AddColumnAndScale(out, IMU_GYRO_Z_ARRAY_NAME(), gyro.GetZ(), gyroScale);
}

//-----------------------------------------------------------------------------
void vtkNormalizeExternalSensorData::AppendGNSSDataToTable(vtkTable* out,
  const vtkVector3d& position,
  DistanceUnit positionUnit)
{
  double positionScale = ::ScaleFromDistUnit(positionUnit);
  ::AddColumnAndScale(out, GNSS_POS_X_ARRAY_NAME(), position.GetX(), positionScale);
  ::AddColumnAndScale(out, GNSS_POS_Y_ARRAY_NAME(), position.GetY(), positionScale);
  ::AddColumnAndScale(out, GNSS_POS_Z_ARRAY_NAME(), position.GetZ(), positionScale);
}

//-----------------------------------------------------------------------------
void vtkNormalizeExternalSensorData::AppendINSDataToTable(vtkTable* out,
  const vtkVector3d& position,
  const vtkVector3d& angles,
  DistanceUnit positionUnit,
  AngleUnit angleUnit)
{
  vtkNormalizeExternalSensorData::AppendGNSSDataToTable(out, position, positionUnit);

  double angleScale = ::ScaleFromAngleUnit(angleUnit);
  ::AddColumnAndScale(out, INS_ANGLE_RX_ARRAY_NAME(), angles.GetX(), angleScale);
  ::AddColumnAndScale(out, INS_ANGLE_RY_ARRAY_NAME(), angles.GetY(), angleScale);
  ::AddColumnAndScale(out, INS_ANGLE_RZ_ARRAY_NAME(), angles.GetZ(), angleScale);
}

//-----------------------------------------------------------------------------
void vtkNormalizeExternalSensorData::AppendOdometryDataToTable(vtkTable* out,
  double distance,
  DistanceUnit distanecUnit)
{
  double distanceScale = ::ScaleFromDistUnit(distanecUnit);
  ::AddColumnAndScale(out, ODOMETRY_DISTANCE_ARRAY_NAME(), distance, distanceScale);
}

//-----------------------------------------------------------------------------
void vtkNormalizeExternalSensorData::AppendTimeDataToTable(vtkTable* out, double timestamp)
{
  ::AddColumnAndScale(out, TIME_ARRAY_NAME(), timestamp, 1.);
}

//-----------------------------------------------------------------------------
void vtkNormalizeExternalSensorData::SetTransformInFieldData(vtkTable* out,
  vtkTransform* transform,
  const std::string& name)
{
  vtkFieldData* fd = out->GetFieldData();
  vtkLVUtilities::SetTransformInFieldData(fd, transform, name);
}
