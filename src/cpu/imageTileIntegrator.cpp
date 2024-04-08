#include "imageTileIntegrator.h";

void ImageTileIntegrator::render() {
  // Declare common variables for rendering image in tiles
  ThreadLocal<ScratchBuffer> scratchBuffers([]() { return ScratchBuffer(); });
  ThreadLocal<Sampler> samplers([this]() { return samplerPrototype.clone(); });
  Bounds2i pixelBounds = camera.getFilm().getPixelBounds();
  int spp = samplerPrototype.samplesPerPixel();
  ProgressReporter progress(int64_t(spp) * pixelBounds.getArea(), "Rendering", Options->quiet);
  int waveStart = 0, waveEnd = 1, nextWaveSize = 1;

  // Render image in waves
  while (waveStart < spp) {
    // Render current wave's image tiles in parallel
    ParallelFor2D(pixelBounds, [&](Bounds2i tileBounds) {
      // Render image tile given by tileBounds
      ScratchBuffer &scratchBuffer = scratchBuffers.get();
      Sampler &sampler = samplers.get();
      for (Point2i pPixel : tileBounds) {
        // Render samples in pixel pPixel
        for (int sampleIndex = waveStart; sampleIndex < waveEnd; ++sampleIndex) {
          sampler.startPixelSample(pPixel, sampleIndex);
          evaluatePixelSample(pPixel, sampleIndex, sampler, scratchBuffer);
          scratchBuffer.reset();
        }
      }
      progress.update((waveEnd - waveStart) * tileBounds.getArea())
    });

    // Update start and end wave
    waveStart = waveEnd;
    waveEnd = std::min(spp, waveEnd + nextWaveSize);
    nextWaveSize = std::min(2 * nextWaveSize, 64);

    // Optionally write current image to disk
    if (waveStart == spp || Options.writePartialImages) {
      LOG_VERBOSE("Writing image with spp = %d", waveStart);
      ImageMetadata metadata;
      metadata.renderTimeInSeconds = progress.elapsedSeconds();
      metadata.samplesPerPixel = waveStart;
      camera.initMetadata(&metadata);
      camera.getFilm().writeImage(metadata, 1.0f / waveStart);
    }

  }
}