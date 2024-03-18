#include "integrator.h"

pstd::optional<ShapeIntersection> Integrator::intersect(const Ray &ray, float tMax) const {
  if (aggregate) return aggregate.intersect(ray, tMax);
  else return {}
}

bool Integrator::intersects(const Ray &ray, float tMax) const {
  if (aggregate) return aggregate.intersects(ray, tMax);
  else return false;
}