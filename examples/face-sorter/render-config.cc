#include "render-config.h"

#include "picojson.h"

#include <fstream>
#include <istream>

namespace example {

bool LoadRenderConfig(example::RenderConfig* config, const char* filename) {
  std::ifstream is(filename);
  if (is.fail()) {
    std::cerr << "Cannot open " << filename << std::endl;
    return false;
  }

  std::istream_iterator<char> input(is);
  std::string err;
  picojson::value v;
  input = picojson::parse(v, input, std::istream_iterator<char>(), &err);
  if (!err.empty()) {
    std::cerr << err << std::endl;
  }

  if (!v.is<picojson::object>()) {
    std::cerr << "Not a JSON object" << std::endl;
    return false;
  }

  picojson::object o = v.get<picojson::object>();

  if (o.find("obj_filename") != o.end()) {
    if (o["obj_filename"].is<std::string>()) {
      config->obj_filename = o["obj_filename"].get<std::string>();
    }
  }

  config->scene_scale = 1.0f;
  if (o.find("scene_scale") != o.end()) {
    if (o["scene_scale"].is<double>()) {
      config->scene_scale = static_cast<float>(o["scene_scale"].get<double>());
    }
  }

  config->eye[0] = 0.0f;
  config->eye[1] = 0.0f;
  config->eye[2] = 5.0f;
  if (o.find("eye") != o.end()) {
    if (o["eye"].is<picojson::array>()) {
      picojson::array arr = o["eye"].get<picojson::array>();
      if (arr.size() == 3) {
        config->eye[0] = arr[0].get<double>();
        config->eye[1] = arr[1].get<double>();
        config->eye[2] = arr[2].get<double>();
      }
    }
  }

  config->up[0] = 0.0f;
  config->up[1] = 1.0f;
  config->up[2] = 0.0f;
  if (o.find("up") != o.end()) {
    if (o["up"].is<picojson::array>()) {
      picojson::array arr = o["up"].get<picojson::array>();
      if (arr.size() == 3) {
        config->up[0] = arr[0].get<double>();
        config->up[1] = arr[1].get<double>();
        config->up[2] = arr[2].get<double>();
      }
    }
  }

  config->look_at[0] = 0.0f;
  config->look_at[1] = 0.0f;
  config->look_at[2] = 0.0f;
  if (o.find("look_at") != o.end()) {
    if (o["look_at"].is<picojson::array>()) {
      picojson::array arr = o["look_at"].get<picojson::array>();
      if (arr.size() == 3) {
        config->look_at[0] = arr[0].get<double>();
        config->look_at[1] = arr[1].get<double>();
        config->look_at[2] = arr[2].get<double>();
      }
    }
  }

  config->fov = 45.0f;
  if (o.find("fov") != o.end()) {
    if (o["width"].is<double>()) {
      config->fov = static_cast<int>(o["fov"].get<double>());
    }
  }

  config->width = 512;
  if (o.find("width") != o.end()) {
    if (o["width"].is<double>()) {
      config->width = static_cast<int>(o["width"].get<double>());
    }
  }

  config->height = 512;
  if (o.find("height") != o.end()) {
    if (o["height"].is<double>()) {
      config->height = static_cast<int>(o["height"].get<double>());
    }
  }

  config->shape_id = 512;
  if (o.find("shape_id") != o.end()) {
    if (o["shape_id"].is<double>()) {
      config->shape_id = static_cast<int>(o["shape_id"].get<double>());
    }
  }

  config->ray_org[0] = 0.0f;
  config->ray_org[1] = 0.0f;
  config->ray_org[2] = 100.0f;
  if (o.find("ray_org") != o.end()) {
    if (o["ray_org"].is<picojson::array>()) {
      picojson::array arr = o["ray_org"].get<picojson::array>();
      if (arr.size() == 3) {
        config->ray_org[0] = arr[0].get<double>();
        config->ray_org[1] = arr[1].get<double>();
        config->ray_org[2] = arr[2].get<double>();
      }
    }
  }

  config->ray_dir[0] = 0.0f;
  config->ray_dir[1] = 0.0f;
  config->ray_dir[2] = -1.0f;
  if (o.find("ray_dir") != o.end()) {
    if (o["ray_dir"].is<picojson::array>()) {
      picojson::array arr = o["ray_dir"].get<picojson::array>();
      if (arr.size() == 3) {
        config->ray_dir[0] = arr[0].get<double>();
        config->ray_dir[1] = arr[1].get<double>();
        config->ray_dir[2] = arr[2].get<double>();
      }
    }
  }

  return true;
}
}
