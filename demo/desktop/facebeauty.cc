/**
 * facebeauty.cc
 *
 * A simple command-line tool that applies beauty filters to an input image
 * using the GPUPixel library and saves the result to an output path.
 *
 * Usage:
 *   facebeauty <input_image> <output_image> [options]
 *
 * Options:
 *   --smoothing   <0.0-1.0>    Skin smoothing strength   (default: 0.5)
 *   --whitening   <0.0-0.5>    Skin whitening strength   (default: 0.15)
 *   --slim        <0.0-0.05>   Face slimming strength    (default: 0.01)
 *   --eye         <0.0-0.1>    Eye enlarging strength    (default: 0.02)
 *   --lipstick    <0.0-1.0>    Lipstick intensity        (default: 0.0)
 *   --blusher     <0.0-1.0>    Blusher intensity         (default: 0.0)
 */

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#ifdef _WIN32
#include <Shlwapi.h>
#include <windows.h>
#pragma comment(lib, "Shlwapi.lib")
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdlib.h>
#endif

// stb_image for writing PNG output
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "gpupixel/gpupixel.h"

using namespace gpupixel;

// ---------------------------------------------------------------------------
// Beauty parameters with sensible defaults
// (ranges match GPUPixel setter ranges directly – no scaling needed)
// ---------------------------------------------------------------------------
struct BeautyParams {
  float smoothing = 0.5f;    // 0.0 – 1.0   BeautyFaceFilter::SetBlurAlpha
  float whitening = 0.15f;   // 0.0 – 0.5   BeautyFaceFilter::SetWhite
  float slim      = 0.01f;   // 0.0 – 0.05  FaceReshapeFilter::SetFaceSlimLevel
  float eye       = 0.02f;   // 0.0 – 0.1   FaceReshapeFilter::SetEyeZoomLevel
  float lipstick  = 0.0f;    // 0.0 – 1.0   LipstickFilter::SetBlendLevel
  float blusher   = 0.0f;    // 0.0 – 1.0   BlusherFilter::SetBlendLevel
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void PrintUsage(const char* prog) {
  std::cout << "Usage: " << prog
            << " <input_image> <output_image> [options]\n\n"
            << "Options:\n"
            << "  --smoothing <0.0-1.0>    Skin smoothing  (default: 0.5)\n"
            << "  --whitening <0.0-0.5>    Skin whitening  (default: 0.15)\n"
            << "  --slim      <0.0-0.05>   Face slimming   (default: 0.01)\n"
            << "  --eye       <0.0-0.1>    Eye enlarging   (default: 0.02)\n"
            << "  --lipstick  <0.0-1.0>    Lipstick        (default: 0.0)\n"
            << "  --blusher   <0.0-1.0>    Blusher         (default: 0.0)\n";
}

static std::string GetExecutableDir() {
  std::string path;
#ifdef _WIN32
  char buf[MAX_PATH];
  GetModuleFileNameA(NULL, buf, MAX_PATH);
  PathRemoveFileSpecA(buf);
  path = buf;
#elif defined(__APPLE__)
  char buf[PATH_MAX];
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) == 0) {
    char real[PATH_MAX];
    if (realpath(buf, real)) {
      path = real;
      auto pos = path.find_last_of("/\\");
      if (pos != std::string::npos) path = path.substr(0, pos);
    }
  }
#elif defined(__linux__)
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, PATH_MAX);
  if (n != -1) {
    buf[n] = '\0';
    path = buf;
    auto pos = path.find_last_of("/\\");
    if (pos != std::string::npos) path = path.substr(0, pos);
  }
#endif
  return path;
}

// Parse a float argument following a named flag
static bool ParseFloat(int argc, char* argv[], int& i, float& out) {
  if (i + 1 >= argc) return false;
  try {
    out = std::stof(argv[++i]);
    return true;
  } catch (...) {
    return false;
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  if (argc < 3) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string input_path  = argv[1];
  std::string output_path = argv[2];
  BeautyParams params;

  // Parse optional flags
  for (int i = 3; i < argc; ++i) {
    if (std::strcmp(argv[i], "--smoothing") == 0) {
      if (!ParseFloat(argc, argv, i, params.smoothing)) {
        std::cerr << "Invalid value for --smoothing\n"; return 1;
      }
    } else if (std::strcmp(argv[i], "--whitening") == 0) {
      if (!ParseFloat(argc, argv, i, params.whitening)) {
        std::cerr << "Invalid value for --whitening\n"; return 1;
      }
    } else if (std::strcmp(argv[i], "--slim") == 0) {
      if (!ParseFloat(argc, argv, i, params.slim)) {
        std::cerr << "Invalid value for --slim\n"; return 1;
      }
    } else if (std::strcmp(argv[i], "--eye") == 0) {
      if (!ParseFloat(argc, argv, i, params.eye)) {
        std::cerr << "Invalid value for --eye\n"; return 1;
      }
    } else if (std::strcmp(argv[i], "--lipstick") == 0) {
      if (!ParseFloat(argc, argv, i, params.lipstick)) {
        std::cerr << "Invalid value for --lipstick\n"; return 1;
      }
    } else if (std::strcmp(argv[i], "--blusher") == 0) {
      if (!ParseFloat(argc, argv, i, params.blusher)) {
        std::cerr << "Invalid value for --blusher\n"; return 1;
      }
    } else {
      std::cerr << "Unknown option: " << argv[i] << "\n";
      PrintUsage(argv[0]);
      return 1;
    }
  }

  std::cout << "[facebeauty] Input : " << input_path  << "\n"
            << "[facebeauty] Output: " << output_path << "\n"
            << "[facebeauty] Params: "
            << "smoothing="  << params.smoothing
            << " whitening=" << params.whitening
            << " slim="      << params.slim
            << " eye="       << params.eye
            << " lipstick="  << params.lipstick
            << " blusher="   << params.blusher << "\n";

  // -------------------------------------------------------------------------
  // 1. Create a hidden GLFW window – GPUPixel needs an OpenGL context
  // -------------------------------------------------------------------------
  if (!glfwInit()) {
    std::cerr << "[facebeauty] Failed to initialise GLFW\n";
    return 1;
  }

#ifdef __APPLE__
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
  // Hide the window – we only need the GL context
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  GLFWwindow* window = glfwCreateWindow(1, 1, "offscreen", NULL, NULL);
  if (!window) {
    std::cerr << "[facebeauty] Failed to create GLFW window\n";
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "[facebeauty] Failed to initialise GLAD\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  // -------------------------------------------------------------------------
  // 2. Set GPUPixel resource path (same directory as the executable)
  // -------------------------------------------------------------------------
  std::string exe_dir = GetExecutableDir();
  GPUPixel::SetResourcePath(exe_dir);
  std::cout << "[facebeauty] Resource path: " << exe_dir << "\n";

  // -------------------------------------------------------------------------
  // 3. Build filter pipeline (mirrors the demo)
  // -------------------------------------------------------------------------
  auto lipstick_filter = LipstickFilter::Create();
  auto blusher_filter  = BlusherFilter::Create();
  auto reshape_filter  = FaceReshapeFilter::Create();
  auto beauty_filter   = BeautyFaceFilter::Create();

  auto source_image  = SourceImage::Create(input_path);
  auto sink_raw_data = SinkRawData::Create();

  source_image->AddSink(lipstick_filter)
      ->AddSink(blusher_filter)
      ->AddSink(reshape_filter)
      ->AddSink(beauty_filter)
      ->AddSink(sink_raw_data);

  // -------------------------------------------------------------------------
  // 4. Apply beauty parameters
  // -------------------------------------------------------------------------
  beauty_filter->SetBlurAlpha(params.smoothing);
  beauty_filter->SetWhite(params.whitening);
  reshape_filter->SetFaceSlimLevel(params.slim);
  reshape_filter->SetEyeZoomLevel(params.eye);
  lipstick_filter->SetBlendLevel(params.lipstick);
  blusher_filter->SetBlendLevel(params.blusher);

  // -------------------------------------------------------------------------
  // 5. Optionally run face detection if the library was built with it
  // -------------------------------------------------------------------------
#ifdef GPUPIXEL_ENABLE_FACE_DETECTOR
  auto face_detector = FaceDetector::Create();

  int src_w = source_image->GetWidth();
  int src_h = source_image->GetHeight();
  const unsigned char* src_buf = source_image->GetRgbaImageBuffer();

  std::vector<float> landmarks = face_detector->Detect(
      src_buf, src_w, src_h, src_w * 4,
      GPUPIXEL_MODE_FMT_PICTURE, GPUPIXEL_FRAME_TYPE_RGBA);

  if (!landmarks.empty()) {
    lipstick_filter->SetFaceLandmarks(landmarks);
    blusher_filter->SetFaceLandmarks(landmarks);
    reshape_filter->SetFaceLandmarks(landmarks);
    std::cout << "[facebeauty] Face detected – landmarks applied\n";
  } else {
    std::cout << "[facebeauty] No face detected, filters will still run\n";
  }
#endif

  // -------------------------------------------------------------------------
  // 6. Process
  // -------------------------------------------------------------------------
  source_image->Render();

  const uint8_t* rgba   = sink_raw_data->GetRgbaBuffer();
  int            out_w  = sink_raw_data->GetWidth();
  int            out_h  = sink_raw_data->GetHeight();

  if (!rgba || out_w <= 0 || out_h <= 0) {
    std::cerr << "[facebeauty] Processing produced no output\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  // -------------------------------------------------------------------------
  // 7. Write output image (PNG via stb_image_write)
  // -------------------------------------------------------------------------
  int stride = out_w * 4;  // RGBA
  if (!stbi_write_png(output_path.c_str(), out_w, out_h, 4, rgba, stride)) {
    std::cerr << "[facebeauty] Failed to write output image: "
              << output_path << "\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  std::cout << "[facebeauty] Done! Saved to: " << output_path << "\n";

  // -------------------------------------------------------------------------
  // 8. Cleanup
  // -------------------------------------------------------------------------
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
