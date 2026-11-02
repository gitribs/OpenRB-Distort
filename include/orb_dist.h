#ifndef ORB_DIST_H
#define ORB_DIST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OrbDist OrbDist;

typedef struct OrbDistParams {
    uint8_t amount;  /* 0..127 */
    uint8_t shape;   /* 0..127, 0 = 1.5 hard clip */
    uint8_t bypass;
} OrbDistParams;

OrbDist *orb_dist_new(void);
void orb_dist_free(OrbDist *dist);
void orb_dist_reset(OrbDist *dist);
int32_t orb_dist_latency(void);
void orb_dist_process(OrbDist *dist,
                      float *left,
                      float *right,
                      int32_t nframes,
                      const OrbDistParams *params);

#ifdef __cplusplus
}
#endif

#endif
