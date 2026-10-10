/* SPDX-License-Identifier: MIT
 * Local linked-pipeline extension; base structures remain the Mesa public ABI. */
#ifndef SHADPS4_SPIRV_TO_DXIL_LINKED_H
#define SHADPS4_SPIRV_TO_DXIL_LINKED_H
#include "spirv_to_dxil.h"
#ifdef __cplusplus
extern "C" {
#endif
struct dxil_spirv_linked_stage {
   const uint32_t *words;
   size_t word_count;
   dxil_spirv_shader_stage stage;
   const char *entry_point_name;
   struct dxil_spirv_specialization *specializations;
   unsigned num_specializations;
};
/* Stages may arrive in any order; outputs preserve input order. HS and DS must
 * be supplied together. No partially usable binaries are returned on failure.
 * Free each successful output with spirv_to_dxil_free. */
bool spirv_to_dxil_linked(const struct dxil_spirv_linked_stage *stages,
                          unsigned stage_count,
                          enum dxil_validator_version validator_version_max,
                          const struct dxil_spirv_debug_options *debug_options,
                          const struct dxil_spirv_runtime_conf *conf,
                          const struct dxil_spirv_logger *logger,
                          struct dxil_spirv_object *out_dxils);
#ifdef __cplusplus
}
#endif
#endif
