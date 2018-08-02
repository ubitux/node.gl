/*
 * Copyright 2017 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "bstr.h"
#include "glincludes.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"

#define OFFSET(x) offsetof(struct computeprogram, x)
static const struct node_param computeprogram_params[] = {
    {"compute", PARAM_TYPE_STR, OFFSET(compute), .flags=PARAM_FLAG_CONSTRUCTOR,
                .desc=NGLI_DOCSTRING("compute shader")},
    {NULL}
};

#define DEFINE_GET_INFO_LOG_FUNCTION(func, name)                                      \
static void get_##func##_info_log(const struct glcontext *gl, GLuint id,              \
                                  char **info_logp, int *info_log_lengthp)            \
{                                                                                     \
    ngli_glGet##name##iv(gl, id, GL_INFO_LOG_LENGTH, info_log_lengthp);               \
    if (!*info_log_lengthp) {                                                         \
        *info_logp = NULL;                                                            \
        return;                                                                       \
    }                                                                                 \
                                                                                      \
    *info_logp = malloc(*info_log_lengthp);                                           \
    if (!*info_logp) {                                                                \
        *info_log_lengthp = 0;                                                        \
        return;                                                                       \
    }                                                                                 \
                                                                                      \
    ngli_glGet##name##InfoLog(gl, id, *info_log_lengthp, NULL, *info_logp);           \
    while (*info_log_lengthp && strchr(" \r\n", (*info_logp)[*info_log_lengthp - 1])) \
        (*info_logp)[--*info_log_lengthp] = 0;                                        \
}                                                                                     \

DEFINE_GET_INFO_LOG_FUNCTION(shader, Shader)
DEFINE_GET_INFO_LOG_FUNCTION(program, Program)

static GLuint load_shader(struct ngl_node *node, const char *compute_shader_data)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    char *info_log = NULL;
    int info_log_length = 0;

    GLint result = GL_FALSE;

    GLuint program = ngli_glCreateProgram(gl);
    GLuint compute_shader = ngli_glCreateShader(gl, GL_COMPUTE_SHADER);

    ngli_glShaderSource(gl, compute_shader, 1, &compute_shader_data, NULL);
    ngli_glCompileShader(gl, compute_shader);

    ngli_glGetShaderiv(gl, compute_shader, GL_COMPILE_STATUS, &result);
    if (!result) {
        get_shader_info_log(gl, compute_shader, &info_log, &info_log_length);
        goto fail;
    }

    ngli_glAttachShader(gl, program, compute_shader);
    ngli_glLinkProgram(gl, program);

    ngli_glGetProgramiv(gl, program, GL_LINK_STATUS, &result);
    if (!result) {
        get_program_info_log(gl, program, &info_log, &info_log_length);
        goto fail;
    }

    ngli_glDeleteShader(gl, compute_shader);

    return program;

fail:
    if (info_log) {
        LOG(ERROR, "could not compile or link shader: %s", info_log);
        free(info_log);
    }

    if (compute_shader) {
        ngli_glDeleteShader(gl, compute_shader);
    }

    if (program) {
        ngli_glDeleteProgram(gl, program);
    }

    return 0;
}

static void free_pinfo(void *user_arg, void *data)
{
    free(data);
}

static int computeprogram_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct computeprogram *s = node->priv_data;

    s->info.program_id = load_shader(node, s->compute);
    if (!s->info.program_id)
        return -1;

    s->info.active_uniforms = ngli_hmap_create();
    if (!s->info.active_uniforms)
        return -1;
    ngli_hmap_set_free(s->info.active_uniforms, free_pinfo, NULL);

    int nb_active_uniforms;
    ngli_glGetProgramiv(gl, s->info.program_id, GL_ACTIVE_UNIFORMS, &nb_active_uniforms);
    for (int i = 0; i < nb_active_uniforms; i++) {
        struct uniformprograminfo *info = malloc(sizeof(*info));
        if (!info)
            return -1;
        ngli_glGetActiveUniform(gl,
                                s->info.program_id,
                                i,
                                sizeof(info->name),
                                NULL,
                                &info->size,
                                &info->type,
                                info->name);

        /* Remove [0] suffix from names of uniform arrays */
        info->name[strcspn(info->name, "[")] = 0;

        info->id = ngli_glGetUniformLocation(gl,
                                             s->info.program_id,
                                             info->name);

        if (info->type == GL_IMAGE_2D) {
            ngli_glGetUniformiv(gl, s->info.program_id, info->id, &info->binding);
        } else {
            info->binding = -1;
        }

        LOG(DEBUG, "%s.uniform[%d/%d]: %s location:%d size=%d type=0x%x binding=%d", node->name,
            i + 1, nb_active_uniforms, info->name, info->id, info->size, info->type, info->binding);

        int ret = ngli_hmap_set(s->info.active_uniforms, info->name, info);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void computeprogram_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;

    struct computeprogram *s = node->priv_data;

    ngli_hmap_freep(&s->info.active_uniforms);
    ngli_glDeleteProgram(gl, s->info.program_id);
}

const struct node_class ngli_computeprogram_class = {
    .id        = NGL_NODE_COMPUTEPROGRAM,
    .name      = "ComputeProgram",
    .init      = computeprogram_init,
    .uninit    = computeprogram_uninit,
    .priv_size = sizeof(struct computeprogram),
    .params    = computeprogram_params,
    .file      = __FILE__,
};
