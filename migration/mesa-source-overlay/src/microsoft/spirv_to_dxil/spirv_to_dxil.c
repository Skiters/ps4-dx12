/*
 * Copyright © Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "dxil_spirv_nir.h"
#include "spirv_to_dxil.h"
#include "spirv_to_dxil_linked.h"
#include "dxil_nir.h"
#include "nir_to_dxil.h"
#include "shader_enums.h"
#include "spirv/nir_spirv.h"
#include "util/blob.h"

#include "git_sha1.h"
#include "vulkan/vulkan.h"

static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_NONE == MESA_SHADER_NONE, "must match");
static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_VERTEX == MESA_SHADER_VERTEX, "must match");
static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_TESS_CTRL == MESA_SHADER_TESS_CTRL, "must match");
static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_TESS_EVAL == MESA_SHADER_TESS_EVAL, "must match");
static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_GEOMETRY == MESA_SHADER_GEOMETRY, "must match");
static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_FRAGMENT == MESA_SHADER_FRAGMENT, "must match");
static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_COMPUTE == MESA_SHADER_COMPUTE, "must match");
static_assert((mesa_shader_stage)DXIL_SPIRV_SHADER_KERNEL == MESA_SHADER_KERNEL, "must match");

static bool
spirv_to_dxil_impl(const uint32_t *words, size_t word_count,
              struct dxil_spirv_specialization *specializations,
              unsigned int num_specializations, dxil_spirv_shader_stage stage,
              const char *entry_point_name,
              enum dxil_validator_version validator_version_max,
              const struct dxil_spirv_debug_options *dgb_opts,
              const struct dxil_spirv_runtime_conf *conf,
              const struct dxil_spirv_logger *logger,
              struct dxil_spirv_object *out_dxil, unsigned wave_size)
{
   if (stage == DXIL_SPIRV_SHADER_NONE || stage == DXIL_SPIRV_SHADER_KERNEL)
      return false;

   glsl_type_singleton_init_or_ref();

   struct nir_to_dxil_options opts = {
      .environment = DXIL_ENVIRONMENT_VULKAN,
      .shader_model_max = conf->shader_model_max,
      .validator_version_max = validator_version_max,
   };

   const struct spirv_to_nir_options *spirv_opts = dxil_spirv_nir_get_spirv_options();
   nir_shader_compiler_options nir_options;
   const unsigned supported_bit_sizes = 16 | 32 | 64;
   dxil_get_nir_compiler_options(&nir_options, conf->shader_model_max, supported_bit_sizes, supported_bit_sizes);
   // We will manually handle base_vertex when vertex_id and instance_id have
   // have been already converted to zero-base.
   nir_options.lower_base_vertex = conf->first_vertex_and_base_instance_mode != DXIL_SPIRV_SYSVAL_TYPE_ZERO;

   struct nir_spirv_specialization *spec = NULL;
   if (specializations && num_specializations > 0) {
      spec = vtn_alloc_specialization(num_specializations);

      for (unsigned i = 0; i < num_specializations; i++) {
         vtn_add_specialization_entry(spec, i, specializations[i].id,
                                      sizeof(specializations[i].value),
                                      &specializations[i].value, false);
      }
   }
   nir_shader *nir = spirv_to_nir(words, word_count, spec,
                                  (mesa_shader_stage)stage, entry_point_name,
                                  spirv_opts, &nir_options);
   vtn_free_specialization(spec);
   if (!nir) {
      glsl_type_singleton_decref();
      return false;
   }

   nir_validate_shader(nir,
                       "Validate before feeding NIR to the DXIL compiler");

   if (wave_size) {
      if (stage != DXIL_SPIRV_SHADER_COMPUTE || conf->shader_model_max < SHADER_MODEL_6_6) {
         ralloc_free(nir);
         glsl_type_singleton_decref();
         return false;
      }
      nir->info.min_subgroup_size = wave_size;
      nir->info.max_subgroup_size = wave_size;
      nir->info.api_subgroup_size = wave_size;
   }
   dxil_spirv_nir_prep(nir);

   dxil_spirv_nir_passes(nir, conf, &out_dxil->metadata);

   if (dgb_opts->dump_nir)
      nir_print_shader(nir, stderr);

   struct dxil_logger logger_inner = {.priv = logger->priv,
                                      .log = logger->log};

   struct blob dxil_blob;
   if (!nir_to_dxil(nir, &opts, &logger_inner, &dxil_blob)) {
      if (dxil_blob.allocated)
         blob_finish(&dxil_blob);
      ralloc_free(nir);
      glsl_type_singleton_decref();
      return false;
   }

   ralloc_free(nir);
   blob_finish_get_buffer(&dxil_blob, &out_dxil->binary.buffer,
                          &out_dxil->binary.size);

   glsl_type_singleton_decref();
   return true;
}

bool
spirv_to_dxil(const uint32_t *words, size_t word_count,
              struct dxil_spirv_specialization *specializations,
              unsigned int num_specializations, dxil_spirv_shader_stage stage,
              const char *entry_point_name,
              enum dxil_validator_version validator_version_max,
              const struct dxil_spirv_debug_options *dgb_opts,
              const struct dxil_spirv_runtime_conf *conf,
              const struct dxil_spirv_logger *logger,
              struct dxil_spirv_object *out_dxil)
{
   return spirv_to_dxil_impl(words, word_count, specializations, num_specializations,
                            stage, entry_point_name, validator_version_max,
                            dgb_opts, conf, logger, out_dxil, 0);
}

bool
spirv_to_dxil_compute_waves32(const uint32_t *words, size_t word_count,
              struct dxil_spirv_specialization *specializations,
              unsigned int num_specializations, dxil_spirv_shader_stage stage,
              const char *entry_point_name,
              enum dxil_validator_version validator_version_max,
              const struct dxil_spirv_debug_options *dgb_opts,
              const struct dxil_spirv_runtime_conf *conf,
              const struct dxil_spirv_logger *logger,
              struct dxil_spirv_object *out_dxil)
{
   return spirv_to_dxil_impl(words, word_count, specializations, num_specializations,
                            stage, entry_point_name, validator_version_max,
                            dgb_opts, conf, logger, out_dxil, 32);
}

void
spirv_to_dxil_free(struct dxil_spirv_object *dxil)
{
   free(dxil->binary.buffer);
}

uint64_t
spirv_to_dxil_get_version()
{
   const char sha1[] = MESA_GIT_SHA1;
   const char* dash = strchr(sha1, '-');
   if (dash) {
      return strtoull(dash + 1, NULL, 16);
   }
   return 0;
}

bool
spirv_to_dxil_linked(const struct dxil_spirv_linked_stage *stages,
                     unsigned stage_count,
                     enum dxil_validator_version validator_version_max,
                     const struct dxil_spirv_debug_options *debug_options,
                     const struct dxil_spirv_runtime_conf *conf,
                     const struct dxil_spirv_logger *logger,
                     struct dxil_spirv_object *out_dxils)
{
   if (!stages || !out_dxils || !conf || !logger || !logger->log ||
       stage_count == 0 || stage_count > MESA_SHADER_COMPUTE + 1)
      return false;
   memset(out_dxils, 0, sizeof(*out_dxils) * stage_count);
   int indices[MESA_SHADER_COMPUTE + 1];
   for (unsigned i = 0; i <= MESA_SHADER_COMPUTE; ++i)
      indices[i] = -1;
   for (unsigned i = 0; i < stage_count; ++i) {
      int stage = (int)stages[i].stage;
      if (stage < MESA_SHADER_VERTEX || stage > MESA_SHADER_COMPUTE || indices[stage] >= 0 ||
          !stages[i].words || stages[i].word_count < 5 || stages[i].words[0] != 0x07230203u ||
          !stages[i].entry_point_name || !stages[i].entry_point_name[0] ||
          (stages[i].num_specializations && !stages[i].specializations)) {
         logger->log(logger->priv, "Invalid or duplicate linked shader stage");
         return false;
      }
      indices[stage] = i;
   }
   if ((indices[MESA_SHADER_COMPUTE] >= 0 && stage_count != 1) ||
       ((indices[MESA_SHADER_TESS_CTRL] >= 0) != (indices[MESA_SHADER_TESS_EVAL] >= 0))) {
      logger->log(logger->priv, "Linked graphics compilation requires an HS/DS pair and excludes compute");
      return false;
   }
   glsl_type_singleton_init_or_ref();
   bool success = false;
   nir_shader *shaders[MESA_SHADER_COMPUTE + 1] = {0};
   nir_shader_compiler_options nir_options;
   const unsigned supported_bit_sizes = 16 | 32 | 64;
   dxil_get_nir_compiler_options(&nir_options, conf->shader_model_max,
                                supported_bit_sizes, supported_bit_sizes);
   nir_options.lower_base_vertex =
      conf->first_vertex_and_base_instance_mode != DXIL_SPIRV_SYSVAL_TYPE_ZERO;
   const struct spirv_to_nir_options *spirv_options = dxil_spirv_nir_get_spirv_options();
   for (unsigned i = 0; i < stage_count; ++i) {
      const struct dxil_spirv_linked_stage *input = &stages[i];
      struct nir_spirv_specialization *spec = NULL;
      if (input->num_specializations) {
         spec = vtn_alloc_specialization(input->num_specializations);
         for (unsigned j = 0; j < input->num_specializations; ++j)
            vtn_add_specialization_entry(spec, j, input->specializations[j].id,
                                         sizeof(input->specializations[j].value),
                                         &input->specializations[j].value, false);
      }
      shaders[i] = spirv_to_nir(input->words, input->word_count, spec,
                                (mesa_shader_stage)input->stage,
                                input->entry_point_name, spirv_options, &nir_options);
      vtn_free_specialization(spec);
      if (!shaders[i]) {
         logger->log(logger->priv, "Linked SPIR-V to NIR conversion failed");
         goto cleanup;
      }
      nir_validate_shader(shaders[i], "Validate before linked DXIL compilation");
      dxil_spirv_nir_prep(shaders[i]);
      /* Apply viewport conversion exactly once, after the last position
       * producing stage. Applying it to VS as well as DS/GS inverts twice;
       * passing it to PS/HS/CS violates the NIR lowering contract. */
      struct dxil_spirv_runtime_conf stage_conf = *conf;
      int last_position = indices[MESA_SHADER_GEOMETRY] >= 0 ? MESA_SHADER_GEOMETRY :
                          indices[MESA_SHADER_TESS_EVAL] >= 0 ? MESA_SHADER_TESS_EVAL :
                          MESA_SHADER_VERTEX;
      if ((int)input->stage != last_position)
         stage_conf.yz_flip.mode = DXIL_SPIRV_YZ_FLIP_NONE;
      dxil_spirv_nir_passes(shaders[i], &stage_conf, &out_dxils[i].metadata);
   }
   /* Reverse stage order is intentional: downstream linking changes upstream
    * tessellation IO and patch constants before that upstream stage is linked. */
   for (int current = MESA_SHADER_FRAGMENT; current >= MESA_SHADER_VERTEX; --current) {
      if (indices[current] < 0)
         continue;
      for (int previous = current - 1; previous >= MESA_SHADER_VERTEX; --previous) {
         if (indices[previous] < 0)
            continue;
         int index = indices[current];
         dxil_spirv_nir_link(shaders[index], shaders[indices[previous]], conf,
                             &out_dxils[index].metadata);
         break;
      }
   }
   struct nir_to_dxil_options opts = {
      .environment = DXIL_ENVIRONMENT_VULKAN,
      .shader_model_max = conf->shader_model_max,
      .validator_version_max = validator_version_max,
   };
   struct dxil_logger inner_logger = {.priv = logger->priv, .log = logger->log};
   for (unsigned i = 0; i < stage_count; ++i) {
      if (debug_options && debug_options->dump_nir)
         nir_print_shader(shaders[i], stderr);
      struct blob binary;
      if (!nir_to_dxil(shaders[i], &opts, &inner_logger, &binary)) {
         if (binary.allocated)
            blob_finish(&binary);
         logger->log(logger->priv, "Linked NIR to DXIL conversion failed");
         goto cleanup;
      }
      blob_finish_get_buffer(&binary, &out_dxils[i].binary.buffer, &out_dxils[i].binary.size);
   }
   success = true;
cleanup:
   for (unsigned i = 0; i < stage_count; ++i) {
      if (shaders[i])
         ralloc_free(shaders[i]);
      if (!success) {
         free(out_dxils[i].binary.buffer);
         memset(&out_dxils[i], 0, sizeof(out_dxils[i]));
      }
   }
   glsl_type_singleton_decref();
   return success;
}
