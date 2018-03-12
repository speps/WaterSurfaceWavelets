#include <Magnum/Buffer.h>
#include <Magnum/DefaultFramebuffer.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Mesh.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/CompressIndices.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Primitives/Plane.h>
#include <Magnum/Primitives/UVSphere.h>
#include <Magnum/Renderer.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/Shader.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Shaders/VertexColor.h>
#include <Magnum/Trade/MeshData3D.h>

#include <MagnumImGui/MagnumImGui.h>
#include <MagnumImGui/imgui.h>

#include <algorithm>
#include <iostream>
#include <tuple>

#include "../StokesWave.h"
#include "WaterSurfaceMesh.h"
#include "WaterSurfaceShader.h"
#include "drawables/VisualizationPrimitives.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;

using Object3D = SceneGraph::Object<SceneGraph::MatrixTransformation3D>;
using Scene3D  = SceneGraph::Scene<SceneGraph::MatrixTransformation3D>;

constexpr float pi = 3.14159265359f;

struct CameraParameters {
  Vector3 target         = {0.f, 0.f, 0.f};
  float   longitude      = pi / 4;
  float   latitude       = pi / 4;
  float   targetDistance = 20.0f;

  Matrix4 getCameraTransformation() {
    Matrix4 trans = Matrix4::translation(target) *
                    Matrix4::rotationZ(Rad{longitude}) *
                    Matrix4::rotationX(-Rad{latitude}) *
                    Matrix4::translation(Vector3{0.f, -targetDistance, 0.f}) *
                    Matrix4::rotationX(Rad{pi / 2});
    return trans;
  }
};

class MyApplication : public Platform::Application {
public:
  explicit MyApplication(const Arguments &arguments);

private:
  void drawEvent() override;
  void drawGui();

  void viewportEvent(const Vector2i &size) override;

  void keyPressEvent(KeyEvent &event) override;
  void keyReleaseEvent(KeyEvent &event) override;
  void mousePressEvent(MouseEvent &event) override;
  void mouseReleaseEvent(MouseEvent &event) override;
  void mouseMoveEvent(MouseMoveEvent &event) override;
  void mouseScrollEvent(MouseScrollEvent &event) override;
  void textInputEvent(TextInputEvent &event) override;

  void mouseRotation(MouseMoveEvent const &event, Vector2 delta);
  void mouseZoom(MouseMoveEvent const &event, Vector2 delta);
  void mousePan(MouseMoveEvent const &event, Vector2 delta);

  /*
   * Sends a ray from a camera pixel
   *
   * \param pixel pixel to send a ray from
   * \return direction vector and camera position(in that order)
   */
  std::tuple<Vector3, Vector3> cameraRayCast(Vector2i pixel) const;

  Scene3D                     _scene;
  Object3D *                  _cameraObject;
  SceneGraph::Camera3D *      _camera;
  SceneGraph::DrawableGroup3D _drawables;
  CameraParameters            _cameraParams;

  Vector2i _previousMousePosition;

  MagnumImGui _gui;

  // Example objects to draw
  DrawablePlane * plane;
  DrawableSphere *sphere;
  // DrawableLine *  line;

  WaterSurfaceMesh *water_surface;

  // Stokes wave
  float   logdt      = -1.0;
  float   amplitude  = 0.5;
  float   time       = 0.0;
  Vector2 kvec       = {1.0, 0.0};
  float   plane_size = 20.0;
  int     wave_type  = 1; // { STOKES = 0, GERSTNER = 1 }
};

MyApplication::MyApplication(const Arguments &arguments)
    : Platform::Application{
          arguments,
          Configuration{}
              .setTitle("Magnum object picking example")
              .setWindowFlags(
                  Sdl2Application::Configuration::WindowFlag::Resizable)} {
  /* Configure OpenGL state */
  Renderer::enable(Renderer::Feature::DepthTest);
  Renderer::enable(Renderer::Feature::FaceCulling);

  Shaders::WaterSurfaceShader sh;

  /* Configure camera */
  _cameraObject = new Object3D{&_scene};
  _cameraObject->setTransformation(_cameraParams.getCameraTransformation());
  _camera = new SceneGraph::Camera3D{*_cameraObject};
  viewportEvent(defaultFramebuffer.viewport().size()); // set up camera

  /* Set up object to draw */
  // plane = new DrawablePlane(&_scene, &_drawables, 200, 200);
  // plane->translate(Vector3{0, 0, -5});
  water_surface = new WaterSurfaceMesh(&_scene, &_drawables, 200);
}

void MyApplication::drawEvent() {
  defaultFramebuffer.clear(FramebufferClear::Color | FramebufferClear::Depth);

  water_surface->setVertices([&](int i, WaterSurfaceMesh::VertexData &v) {
    v.position *= plane_size;
    for (int i = 0; i < DIR_NUM; i++) {
      v.amplitude[i] = 0;
    }
    v.amplitude[0] = amplitude;
  });

  // switch (wave_type) {
  // case 0:
  //   // Stokes wave
  //   plane->setVertices([&](int i, DrawableMesh::VertexData &v) {
  //     v.position *= plane_size;
  //     Vector2 pos_xy = v.position.xy();
  //     v.position[2] =
  //         stokes_wave(amplitude, kvec.length(), dot(kvec, pos_xy), time);
  //   });
  //   break;
  // case 1:
  //   // Gerstner wave
  //   plane->setVertices([&](int i, DrawableMesh::VertexData &v) {
  //     v.position *= plane_size;
  //     Vector2 kdir   = kvec.normalized();
  //     Vector2 pos_xy = v.position.xy();
  //     auto[X, Z] =
  //         gerstner_wave(amplitude, kvec.length(), dot(kvec, pos_xy), time);
  //     v.position +=
  //         X * Vector3{kdir.x(), kdir.y(), 0.f} + Z * Vector3{0.f, 0.f, 1.f};
  //   });
  //   break;
  // }

  time += pow(10.f, logdt);

  _camera->draw(_drawables);

  drawGui();

  swapBuffers();
}

void MyApplication::drawGui() {
  _gui.newFrame(windowSize(), defaultFramebuffer.viewport().size());

  // ImGui::ColorEdit3("Box color", &(_color[0]));
  ImGui::Text("asdfasdf");

  // if (ImGui::Button("Vertex color shader")) {
  //   plane->_shader = Shaders::VertexColor3D{};
  // }

  // if (ImGui::Button("Phong shader")) {
  //   plane->_shader = Shaders::Phong{};
  //   auto &shader   = std::get<Shaders::Phong>(plane->_shader);
  //   shader.setDiffuseColor(Color4{0.4f, 0.4f, 0.8f, 1.f})
  //       .setAmbientColor(Color3{0.25f, 0.2f, 0.23f});
  // }

  // if (ImGui::Button("MeshVisualizer shader")) {
  //   plane->_shader =
  //       Shaders::MeshVisualizer{Shaders::MeshVisualizer::Flag::Wireframe};
  //   auto &s = std::get<Shaders::MeshVisualizer>(plane->_shader);
  //   s.setColor(Color4{0.3f, 0.3f, 0.3f, 1.f});
  //   s.setLabel("mesh");
  //   s.setWireframeColor(Color4{1.f, 1.f, 1.f, 1.f});
  //   s.setWireframeWidth(1.f);
  // }

  ImGui::SliderFloat("plane size", &plane_size, 1, 100);
  ImGui::SliderFloat("amplitude", &amplitude, 0, 2);
  ImGui::SliderFloat("log10(dt)", &logdt, -3, 3);
  ImGui::SliderFloat2("wave vector", &kvec[0], -3, 3);
  ImGui::RadioButton("Stokes", &wave_type, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Gerstner", &wave_type, 1);

  _gui.drawFrame();

  redraw();
}

void MyApplication::viewportEvent(const Vector2i &size) {
  defaultFramebuffer.setViewport({{}, size});

  _camera->setProjectionMatrix(Matrix4::perspectiveProjection(
      60.0_degf, Vector2{size}.aspectRatio(), 0.001f, 10000.0f));
}

void MyApplication::keyPressEvent(KeyEvent &event) {
  if (_gui.keyPressEvent(event)) {
    redraw();
    return;
  }

  if (event.key() == KeyEvent::Key::Esc) {
    exit();
  }

  if (event.key() == KeyEvent::Key::F) {
    _cameraParams.target = Vector3{0.f, 0.f, 0.f};
    _cameraObject->setTransformation(_cameraParams.getCameraTransformation());
  }

  redraw();
}

void MyApplication::keyReleaseEvent(KeyEvent &event) {
  if (_gui.keyReleaseEvent(event)) {
    redraw();
    return;
  }
}

void MyApplication::mousePressEvent(MouseEvent &event) {
  if (_gui.mousePressEvent(event)) {
    redraw();
    return;
  }

  if (event.button() == MouseEvent::Button::Left) {
    _previousMousePosition = event.position();
    event.setAccepted();
  }
}

void MyApplication::mouseReleaseEvent(MouseEvent &event) {
  if (_gui.mouseReleaseEvent(event)) {
    redraw();
    return;
  }

  event.setAccepted();
  redraw();
}

void MyApplication::mouseMoveEvent(MouseMoveEvent &event) {
  if (_gui.mouseMoveEvent(event)) {
    redraw();
    return;
  }

  const Vector2 delta = Vector2{event.position() - _previousMousePosition} /
                        Vector2{defaultFramebuffer.viewport().size()};

  if ((event.modifiers() & MouseMoveEvent::Modifier::Alt) &&
      (event.buttons() & MouseMoveEvent::Button::Left))
    mouseRotation(event, delta);

  if (event.modifiers() & MouseMoveEvent::Modifier::Alt &&
      event.buttons() & MouseMoveEvent::Button::Right)
    mouseZoom(event, delta);

  if (event.modifiers() & MouseMoveEvent::Modifier::Alt &&
      event.buttons() & MouseMoveEvent::Button::Middle)
    mousePan(event, delta);

  _previousMousePosition = event.position();
  event.setAccepted();
  redraw();
}

void MyApplication::mouseScrollEvent(MouseScrollEvent &event) {
  if (_gui.mouseScrollEvent(event)) {
    redraw();
    return;
  }
}

void MyApplication::textInputEvent(TextInputEvent &event) {
  if (_gui.textInputEvent(event)) {
    redraw();
    return;
  }
}

void MyApplication::mouseRotation(MouseMoveEvent const &event, Vector2 delta) {

  _cameraParams.longitude -= 3.0f * delta.x();
  _cameraParams.latitude += 3.0f * delta.y();

  _cameraParams.latitude = std::clamp(_cameraParams.latitude, -pi / 2, pi / 2);
  _cameraObject->setTransformation(_cameraParams.getCameraTransformation());
}

void MyApplication::mouseZoom(MouseMoveEvent const &event, Vector2 delta) {
  _cameraParams.targetDistance -= 10.0f * delta.y();
  _cameraObject->setTransformation(_cameraParams.getCameraTransformation());
}

void MyApplication::mousePan(MouseMoveEvent const &event, Vector2 delta) {

  auto trans = _cameraParams.getCameraTransformation();
  _cameraParams.target +=
      trans.transformVector(Vector3{-4.f * delta.x(), 4.f * delta.y(), 0.f});
  _cameraObject->setTransformation(_cameraParams.getCameraTransformation());
}

std::tuple<Vector3, Vector3>
MyApplication::cameraRayCast(Vector2i pixel) const {

  Vector2 mouseScreenPos =
      2.0f * (Vector2{pixel} / Vector2{defaultFramebuffer.viewport().size()} -
              Vector2{.5f, 0.5f});
  mouseScreenPos[1] *= -1.f;

  Vector3 dir = {mouseScreenPos[0], mouseScreenPos[1], -1.f};
  auto    trans =
      _cameraObject->transformation() * _camera->projectionMatrix().inverted();
  dir = trans.transformVector(dir);

  Vector3 camPos =
      _cameraObject->transformation().transformPoint(Vector3{0.f, 0.f, 0.f});

  return {dir, camPos};
}

MAGNUM_APPLICATION_MAIN(MyApplication)