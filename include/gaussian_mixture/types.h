#ifndef GMM_TYPES_H_
#define GMM_TYPES_H_

#include <limits>
#include <iostream>

#include <gaussian_mixture/defs.h>

namespace gmm
{
  const bool DEBUG = true;

  typedef float g_float;
  const g_float GFLOAT_MIN = std::numeric_limits<g_float>::min();//-1e7;
  const g_float GFLOAT_MAX = std::numeric_limits<g_float>::max();//1e7;

  // gaussian definition
  template<int DIM>
    class Gaussian;
  // converter definition
  template<int DIM, int P_DIM>
    class GaussianConverter;
  // gaussian mixture model definition
  template<int DIM>
    class GMM;
  // gaussian mixture regression definition
  template<int DIM, int P_DIM>
    class GMR;
  // EM algorithm on gaussian mixture models
  template<int DIM>
    class EM;
}

#endif /* GMM_TYPES_H_ */
