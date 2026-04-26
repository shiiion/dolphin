#include "Core/PrimeHack/Quaternion.h"

#include <cmath>

namespace prime {

quat::quat(float _x, float _y, float _z, float _w)
  : x(_x), y(_y), z(_z), w(_w) {}

void quat::build_aa(vec3 const& axis, float angle) {
  const float sin_t = sinf(angle / 2.f);
  const float cos_t = cosf(angle / 2.f);

  x = axis.x * sin_t;
  y = axis.y * sin_t;
  z = axis.z * sin_t;
  w = cos_t;
}

void quat::build_pyr(float pitch, float yaw, float roll) {
  const float sp = sinf(pitch / 2.f), cp = cosf(pitch / 2.f);
  const float sy = sinf(yaw / 2.f), cy = cosf(yaw / 2.f);
  const float sr = sinf(roll / 2.f), cr = cosf(roll / 2.f);

  x = sr * cp * cy - cr * sp * sy;
  y = cr * sp * cy + sr * cp * sy;
  z = cr * cp * sy - sr * sp * cy;
  w = cr * cp * cy + sr * sp * sy;
}

quat& quat::operator*=(quat const& rhs) {
  //float _w = w * rhs.w - axis().dot(rhs.axis());
  //vec3 _xyz = axis().cross(rhs.axis()) + (rhs.axis() * w) + (axis() * rhs.w);
  //return *this = quat(_xyz.x, _xyz.y, _xyz.z, _w);

  quat orig = *this;
  w = orig.w * rhs.w - vec3(orig.x, orig.y, orig.z).dot(vec3(rhs.x, rhs.y, rhs.z));
  x = orig.y * rhs.z - orig.z * rhs.y + orig.w * rhs.x + orig.x * rhs.w;
  y = orig.z * rhs.x - orig.x * rhs.z + orig.w * rhs.y + orig.y * rhs.w;
  z = orig.x * rhs.y - orig.y * rhs.x + orig.w * rhs.z + orig.z * rhs.w;
  return *this;
}

void quat::rotate_x(float t) {
  *this *= quat(vec3(1, 0, 0), t);
}

void quat::rotate_y(float t) {
  *this *= quat(vec3(0, 1, 0), t);
}

void quat::rotate_z(float t) {
  *this *= quat(vec3(0, 0, 1), t);
}

}
