#include "model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
extern const ps_experiment_api *ps_get_experiment(void);
nx_result physim_model_run(double length, double angle, uint64_t generation, physim_snapshot *out) {
    if(!out||!generation||!isfinite(length)||!isfinite(angle)) return NX_ERROR_ARGUMENT;
    const ps_experiment_api *api=ps_get_experiment();
    if(!api||api->abi_version!=PS_ABI_VERSION||api->struct_size<PS_EXPERIMENT_API_BASE_SIZE||
        !api->create||!api->step||!api->build_scene||!api->destroy) return NX_ERROR_UNSUPPORTED;
    ps_context *context=calloc(1,sizeof(*context));physim_snapshot *next=calloc(1,sizeof(*next));
    if(!context||!next) { free(context);free(next);return NX_ERROR_MEMORY; }
    context->struct_size=sizeof(*context);context->api_version=PS_API_VERSION;
    ps_result result=ps_parameter_override(context,"length",length);
    if(result==PS_OK) result=ps_parameter_override(context,"initialAngle",angle);
    int created=0;
    if(result==PS_OK) { created=1;result=api->create(context); }
    if(result==PS_OK) result=ps_parameter_finalize(context);
    nx_result answer=result==PS_INVALID?NX_ERROR_ARGUMENT:result==PS_MEMORY?NX_ERROR_MEMORY:NX_ERROR_STATE;
    if(result!=PS_OK) goto done;
    if(context->channel_count!=9||context->parameter_count!=3) goto done;
    for(uint32_t i=0;i<context->parameter_count;++i) {
        if(!strcmp(context->parameters[i].name,"length")) {
            next->length=context->parameters[i];
            if(ps_parameter_unit_read(context,i,&next->length_unit)!=PS_OK) goto done;
        } else if(!strcmp(context->parameters[i].name,"initialAngle")) {
            next->angle=context->parameters[i];
            if(ps_parameter_unit_read(context,i,&next->angle_unit)!=PS_OK) goto done;
        }
    }
    if(!next->length.defined||!next->angle.defined||!next->length_unit.declared||!next->angle_unit.declared) goto done;
    for(unsigned i=0;i<PHYSIM_SAMPLE_COUNT;++i) {
        if(i) {
            const double dt=1.0/120.0;context->dt_s=dt;
            if(api->step(context,dt)!=PS_OK) goto done;
            context->time_s+=dt; /* Original module contract: host advances time. */
        }
        next->samples[i].time=context->time_s;
        for(unsigned c=0;c<9;++c) {
            if(!isfinite(context->values[c])) goto done;
            next->samples[i].values[c]=context->values[c];
        }
    }
    api->build_scene(context,&next->scene);
    if(!ps_scene_valid(&next->scene)) goto done;
    next->generation=generation;*out=*next;answer=NX_OK;
done:
    if(created) api->destroy(context);
    free(next);free(context);return answer;
}
