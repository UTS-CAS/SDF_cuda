// Copyright 2017-2019, Nicholas Sharp and the Polyscope contributors. http://polyscope.run.
#include "polyscope/point_cloud_scalar_quantity.h"

#include "polyscope/gl/materials/materials.h"
#include "polyscope/gl/shaders.h"
#include "polyscope/gl/shaders/sphere_shaders.h"
#include "polyscope/polyscope.h"

#include "imgui.h"

namespace polyscope {


PointCloudScalarQuantity::PointCloudScalarQuantity(std::string name, const std::vector<double>& values_,
                                                   PointCloud& pointCloud_, DataType dataType_)
    : PointCloudQuantity(name, pointCloud_, true), dataType(dataType_),
      cMap(uniquePrefix() + "#cmap", defaultColorMap(dataType))

{
  if (values_.size() != parent.points.size()) {
    polyscope::error("Point cloud scalar quantity " + name + " does not have same number of values (" +
                     std::to_string(values_.size()) + ") as point cloud size (" + std::to_string(parent.points.size()) +
                     ")");
  }

  // Copy the raw data
  values = values_;

  hist.updateColormap(cMap.get());
  hist.buildHistogram(values);

  dataRange = robustMinMax(values, 1e-5);
  resetMapRange();
}

void PointCloudScalarQuantity::draw() {
  if (!isEnabled()) return;

  // Make the program if we don't have one already
  if (pointProgram == nullptr) {
    createPointProgram();
  }

  // Set uniforms
  parent.setTransformUniforms(*pointProgram);
  parent.setPointCloudUniforms(*pointProgram);
  pointProgram->setUniform("u_rangeLow", vizRange.first);
  pointProgram->setUniform("u_rangeHigh", vizRange.second);

  pointProgram->draw();
}

PointCloudScalarQuantity* PointCloudScalarQuantity::resetMapRange() {
  switch (dataType) {
  case DataType::STANDARD:
    vizRange = dataRange;
    break;
  case DataType::SYMMETRIC: {
    double absRange = std::max(std::abs(dataRange.first), std::abs(dataRange.second));
    vizRange = std::make_pair(-absRange, absRange);
  } break;
  case DataType::MAGNITUDE:
    vizRange = std::make_pair(0., dataRange.second);
    break;
  }

  requestRedraw();
  return this;
}

void PointCloudScalarQuantity::buildCustomUI() {
  ImGui::SameLine();

  // == Options popup
  if (ImGui::Button("Options")) {
    ImGui::OpenPopup("OptionsPopup");
  }
  if (ImGui::BeginPopup("OptionsPopup")) {

    if (ImGui::MenuItem("Reset colormap range")) resetMapRange();

    ImGui::EndPopup();
  }
  if (buildColormapSelector(cMap.get())) {
    pointProgram.reset();
    hist.updateColormap(cMap.get());
    setColorMap(getColorMap());
  }

  // Reset button
  ImGui::SameLine();
  if (ImGui::Button("Reset")) {
    resetMapRange();
  }

  // Draw the histogram of values
  hist.colormapRange = vizRange;
  hist.buildUI();

  // Data range
  // Note: %g specifiers are generally nicer than %e, but here we don't acutally have a choice. ImGui (for somewhat
  // valid reasons) links the resolution of the slider to the decimal width of the formatted number. When %g formats a
  // number with few decimal places, sliders can break. There is no way to set a minimum number of decimal places with
  // %g, unfortunately.
  {
    switch (dataType) {
    case DataType::STANDARD:
      ImGui::DragFloatRange2("", &vizRange.first, &vizRange.second, (dataRange.second - dataRange.first) / 100.,
                             dataRange.first, dataRange.second, "Min: %.3e", "Max: %.3e");
      break;
    case DataType::SYMMETRIC: {
      float absRange = std::max(std::abs(dataRange.first), std::abs(dataRange.second));
      ImGui::DragFloatRange2("##range_symmetric", &vizRange.first, &vizRange.second, absRange / 100., -absRange,
                             absRange, "Min: %.3e", "Max: %.3e");
    } break;
    case DataType::MAGNITUDE: {
      ImGui::DragFloatRange2("##range_mag", &vizRange.first, &vizRange.second, vizRange.second / 100., 0.0,
                             dataRange.second, "Min: %.3e", "Max: %.3e");
    } break;
    }
  }
}


void PointCloudScalarQuantity::createPointProgram() {
  // Create the program to draw this quantity

  pointProgram.reset(new gl::GLProgram(&gl::SPHERE_VALUE_VERT_SHADER, &gl::SPHERE_VALUE_BILLBOARD_GEOM_SHADER,
                                       &gl::SPHERE_VALUE_BILLBOARD_FRAG_SHADER, gl::DrawMode::Points));

  // Fill buffers
  pointProgram->setAttribute("a_position", parent.points);
  pointProgram->setAttribute("a_value", values);
  pointProgram->setTextureFromColormap("t_colormap", gl::getColorMap(cMap.get()));

  setMaterialForProgram(*pointProgram, "wax");
}

void PointCloudScalarQuantity::geometryChanged() { pointProgram.reset(); }

void PointCloudScalarQuantity::buildPickUI(size_t ind) {
  ImGui::TextUnformatted(name.c_str());
  ImGui::NextColumn();
  ImGui::Text("%g", values[ind]);
  ImGui::NextColumn();
}

PointCloudScalarQuantity* PointCloudScalarQuantity::setColorMap(gl::ColorMapID val) {
  cMap = val;
  hist.updateColormap(cMap.get());
  requestRedraw();
  return this;
}
gl::ColorMapID PointCloudScalarQuantity::getColorMap() { return cMap.get(); }
PointCloudScalarQuantity* PointCloudScalarQuantity::setMapRange(std::pair<double, double> val) {
  vizRange = val;
  requestRedraw();
  return this;
}
std::pair<double, double> PointCloudScalarQuantity::getMapRange() { return vizRange; }

std::string PointCloudScalarQuantity::niceName() { return name + " (scalar)"; }

} // namespace polyscope
