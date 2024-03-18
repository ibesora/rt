#include <vector>

class Integrator {
  public:
    // Integrator public methods
    virtual void render() = 0;
    pstd::optional<ShapeIntersection> intersect(const Ray &ray,
                                                float tMax = Infinity) const;

    // Integrator public members
    Primitive aggregate;
    std::vector<Light> lights;
    std::vector<Light> infiniteLights;
  protected:
    // Integrator protected methods
    Integrator(Primitive aggregate, std::vector<Light> lights): aggregate(aggregate), lights(lights) {
      // Integrator constructor implementation
      Bounds3f bounds = aggregate ? aggregate.bounds() : Bounds3f();
      for (auto &light : lights) {
        light.preprocess(bounds);
        if (light.Type() == LightType::Infinite)
          infiniteLights.push_back(&light);
      }
    }
}