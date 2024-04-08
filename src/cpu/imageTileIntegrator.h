#include "integrator.h"

class ImageTileIntegrator : public Integrator {
  public:
    // ImageTileIntegrator public methods
    ImageTileIntegrator(Camera camera, Sampler sampler, Primitive aggregate, std::vector<Light> lights): Integrator(aggregate, lights), camera(camera), samplerPrototype(sampler) {};
    virtual void evaluatePixelSample(Point2i pPixel, int sampleIndex, Sampler sampler, ScratchBuffer & scratchBuffer) = 0;

    void render();
  protected:
    // ImageTileIntegrator protected members
    Camera camera;
    Sampler samplerPrototype;
}