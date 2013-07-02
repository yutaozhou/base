#if defined( LOAD_MODELS_H )
/* the file has been included already */
#else
#define LOAD_MODELS_H

#include "msRgbEvol.hpp"
#include "gBergMag.hpp"
#include "wdCooling.hpp"
#include "gBaraffeMag.hpp"
#include "Settings.hpp"

void loadModels (Cluster *theCluster, const Model &, Settings &);

#endif
