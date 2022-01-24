// lib.c
#include <math.h>
double Hook(const double lhs, const double rhs) {
  double res = lhs + rhs;
  if (res > 100.0) {
    return fmod(res, 100.0);
  } else {
    return res;
  }
}
