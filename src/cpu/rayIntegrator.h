#include "imageTileIntegrator.h"

class RayIntegrator : public ImageTileIntegrator {
  public:
    // RayIntegrator public methods
    RayIntegrator(Camera camera, Sampler sampler, Primitive aggregate, std::vector<Light> lights): ImageTileIntegrator(camera, sampler, aggregate, lights) {};
    virtual SampledSpectrum Li(RayDifferential ray, SampledWaveLengths &lambda, Sampler sampler, ScratchBuffer &scratchBuffer, VisibleSurface *visibleSurface) = 0;
}