#ifndef EXAMPLE_PHYSIM_MODEL_H
#define EXAMPLE_PHYSIM_MODEL_H
#include <nexaui/nexaui.h>
#include <physim/experiment.h>
#include "plot_view.h"
#define PHYSIM_SAMPLE_COUNT 241u
typedef struct physim_sample {
    double time, values[9];
} physim_sample;
/* Owned immutable result of the original documented pendulum, not another
   physics implementation. Coordinates/parameters/channels stay in SI units. */
typedef struct physim_snapshot {
    uint64_t generation;
    ps_parameter length, angle;
    ps_parameter_unit length_unit, angle_unit;
    ps_scene scene;
    physim_sample samples[PHYSIM_SAMPLE_COUNT];
} physim_snapshot;
/* Owner-thread, bounded synchronous reference workload: 240 original RK4
   steps at 1/120s. Caller supplies full output storage, preserved on error.
   Production scheduling/worker admission remains the application's job. */
nx_result physim_model_run(double length, double angle, uint64_t generation, physim_snapshot *out);
#endif
