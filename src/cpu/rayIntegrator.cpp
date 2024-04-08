#include "rayIntegrator.h"

void RayIntegrator::evaluatePixelSample(Point2i pPixel, int sampleIndex, Sampler sampler, ScratchBuffer &scratchBuffer) {
  // Sample wavelengths for the ray
  float lu = sampler.get1D();
  SampledWavelengths lambda = camera.getFilm().sampleWavelengths(lu);

  // Initialize CameraSample for current sample
  Filter filter = camera.getFilm().getFilter();
  CameraSample cameraSample = getCameraSample(sampler, pPixel, filter);

  // Generate camera ray for current sample
  pstd::optional<CameraRayDifferential> cameraRay = camera.generateRayDifferential(cameraSample, lambda);

  // Trace cameraRay if valid
  SampledSpectrum L(0.f);
  VisibleSurface visibleSurface;
  if (cameraRay) {
    // Scale camera ray differentials based on image sampling rate
    float rayDiffScale = std::max<float>(.125f, 1 / std::sqrt((float)sampler.getSamplesPerPixel()));
    cameraRay->ray.scaleDifferentials(rayDiffScale);

    // Evaluate radiance along camera ray
    bool initializeVisibleSurface = camera.getFilm().usesVisibleSurface();
    L = cameraRay->weight * Li(cameraRay->ray, lambda, sampler, scratchBuffer, initializeVisibleSurface ? &visibleSurface : nullptr);

    // Issue warning if unexpected radiance value is returned
    if (L.hasNaNs()) {
      LOG_ERROR("Not-a-number radiance value returned for pixel (%d, %d), sample %d. Setting to black.", pPixel.x, pPixel.y, sampleIndex);
      L = SampledSpectrum(0.f);
    } else if (IsInf(L.y(lambda))) {
      LOG_ERROR("Infinite radiance value returned for pixel (%d, %d), sample %d. Setting to black.", pPixel.x, pPixel.y, sampleIndex);
      L = SampledSpectrum(0.f);
    }

  }

  // Add camera ray's contribution to image
  camera.getFilm().addSample(pPixel, L, lambda, &visibleSurface, cameraSample.filterWeight);
}