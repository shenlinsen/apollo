/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#include "modules/perception/common/io/io_util.h"

#include <fcntl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <Eigen/Geometry>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <vector>

#include "cyber/common/log.h"
#include "modules/perception/base/camera.h"
#include "modules/perception/lib/io/file_util.h"
#include "yaml-cpp/yaml.h"

namespace apollo {
namespace perception {
namespace common {

bool ReadPoseFile(const std::string &filename, Eigen::Affine3d *pose,
                  int *frame_id, double *time_stamp) {
  if (pose == nullptr || frame_id == nullptr || time_stamp == nullptr) {
    AERROR << "Nullptr error.";
    return false;
  }

  std::ifstream fin(filename.c_str());
  if (!fin.is_open()) {
    AERROR << "Failed to open file " << filename;
    return false;
  }

  Eigen::Vector3d translation;
  Eigen::Quaterniond quat;
  fin >> *frame_id >> *time_stamp >> translation(0) >> translation(1) >>
      translation(2) >> quat.x() >> quat.y() >> quat.z() >> quat.w();

  *pose = Eigen::Affine3d::Identity();
  pose->prerotate(quat);
  pose->pretranslate(translation);

  fin.close();
  return true;
}

bool LoadBrownCameraIntrinsic(const std::string &yaml_file,
                              base::BrownCameraDistortionModel *model) {
  if (model == nullptr) {
    return false;
  }

  lib::FileUtil file_util;
  if (!file_util.Exists(yaml_file)) {
    return false;
  }

  YAML::Node node = YAML::LoadFile(yaml_file);
  if (node.IsNull()) {
    AINFO << "Load " << yaml_file << " failed! please check!";
    return false;
  }

  float camera_width = 0;
  float camera_height = 0;
  Eigen::VectorXf params(9 + 5);
  try {
    camera_width = node["width"].as<float>();
    camera_height = node["height"].as<float>();
    for (size_t i = 0; i < 9; ++i) {
      params(i) = node["K"][i].as<float>();
    }
    for (size_t i = 0; i < 5; ++i) {
      params(9 + i) = node["D"][i].as<float>();
    }

    model->set_params(camera_width, camera_height, params);
  } catch (YAML::Exception &e) {
    AERROR << "load camera intrisic file " << yaml_file
              << " with error, YAML exception: " << e.what();
    return false;
  }

  return true;
}

bool LoadOmnidirectionalCameraIntrinsics(
    const std::string &yaml_file,
    base::OmnidirectionalCameraDistortionModel *model) {
  if (model == nullptr) {
    return false;
  }

  lib::FileUtil file_util;
  if (!file_util.Exists(yaml_file)) {
    return false;
  }

  YAML::Node node = YAML::LoadFile(yaml_file);
  if (node.IsNull()) {
    AINFO << "Load " << yaml_file << " failed! please check!";
    return false;
  }

  if (!node["width"].IsDefined() || !node["height"].IsDefined() ||
      !node["center"].IsDefined() || !node["affine"].IsDefined() ||
      !node["cam2world"].IsDefined() || !node["world2cam"].IsDefined() ||
      !node["focallength"].IsDefined() || !node["principalpoint"].IsDefined()) {
    AINFO << "Invalid intrinsics file for an omnidirectional camera.";
    return false;
  }

  try {
    int camera_width = 0;
    int camera_height = 0;

    std::vector<float> params;  // center|affine|f|p|i,cam2world|j,world2cam

    camera_width = node["width"].as<int>();
    camera_height = node["height"].as<int>();

    params.push_back(node["center"]["x"].as<float>());
    params.push_back(node["center"]["y"].as<float>());

    params.push_back(node["affine"]["c"].as<float>());
    params.push_back(node["affine"]["d"].as<float>());
    params.push_back(node["affine"]["e"].as<float>());

    params.push_back(node["focallength"].as<float>());
    params.push_back(node["principalpoint"]["x"].as<float>());
    params.push_back(node["principalpoint"]["y"].as<float>());

    params.push_back(node["cam2world"].size());

    for (size_t i = 0; i < node["cam2world"].size(); ++i) {
      params.push_back(node["cam2world"][i].as<float>());
    }

    params.push_back(node["world2cam"].size());

    for (size_t i = 0; i < node["world2cam"].size(); ++i) {
      params.push_back(node["world2cam"][i].as<float>());
    }

    Eigen::VectorXf eigen_params(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
      eigen_params(i) = params[i];
    }

    model->set_params(camera_width, camera_height, eigen_params);
  } catch (YAML::Exception &e) {
    AERROR << "load camera intrisic file " << yaml_file
              << " with error, YAML exception: " << e.what();
    return false;
  }

  return true;
}

}  // namespace common
}  // namespace perception
}  // namespace apollo