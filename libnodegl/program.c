/*
 * Copyright 2018 GoPro Inc.
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

#include "gctx.h"
#include "program.h"

struct program *ngli_program_create(struct gctx *gctx)
{
    return gctx->class->program_create(gctx);
}

int ngli_program_init(struct program *s, const char *vertex, const char *fragment, const char *compute)
{
    return s->gctx->class->program_init(s, vertex, fragment, compute);
}

void ngli_program_freep(struct program **sp)
{
    if (!*sp)
        return;
    (*sp)->gctx->class->program_freep(sp);
}
