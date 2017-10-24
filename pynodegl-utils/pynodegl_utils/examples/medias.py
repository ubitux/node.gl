import array
import math

from pynodegl import (
        AnimKeyFrameScalar,
        AnimKeyFrameVec3,
        AnimationScalar,
        AnimationVec3,
        Circle,
        GLColorState,
        GLState,
        GLStencilState,
        Group,
        Media,
        Program,
        Quad,
        Render,
        Rotate,
        Scale,
        Texture,
        UniformScalar,
)

from pynodegl_utils.misc import scene

from OpenGL import GL

pgbar_shader = '''
#version 100
precision mediump float;

uniform float time;
uniform float ar;
uniform sampler2D tex0_sampler;
varying vec2 var_tex0_coord;

void main()
{
    float pad    = 2.0 / 100.;
    float padw   = pad;
    float padh   = pad * ar;
    float height = 3.0 / 100. * ar;
    float thick  = 0.3 / 100.;
    float thickw = thick;
    float thickh = thick * ar;

    float x = var_tex0_coord.x;
    float y = var_tex0_coord.y;
    vec4 video_pix = texture2D(tex0_sampler, var_tex0_coord);

    float bar_width = 1. - padw*2. - thickw*2.;

    float t = time*bar_width + padw+thickw;

    if (x < t && x > padw+thickw &&
        y < 1.-padh-thickh && y > 1.-padh-height) {
        vec4 color = vec4(1,0,0,1);
        gl_FragColor = mix(video_pix, color, 0.7);
    } else {
        gl_FragColor = video_pix;
    }

    if (y < 1.-padh-thickh && y > 1.-padh-height &&
        ((x > padw && x < padw+thickw) ||
         (x > 1.-padw-thickw && x < 1.-padw))) {
        gl_FragColor = vec4(1);
    }

    if (x < 1.-padw-thickw && x > padw+thickw &&
        ((y < 1.-padh-height && y > 1.-padh-height-thickh) ||
         (y < 1.-padh && y > 1. - padh-thickh))) {
        gl_FragColor = vec4(1);
    }
}
'''

@scene(uv_corner_x={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_corner_y={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_width={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_height={'type': 'range', 'range': [0,1], 'unit_base': 100},
       progress_bar={'type': 'bool'})
def centered_media(cfg, uv_corner_x=0, uv_corner_y=0, uv_width=1, uv_height=1, progress_bar=True):

    cfg.duration = cfg.medias[0].duration

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0),
             (uv_corner_x, uv_corner_y), (uv_width, 0), (0, uv_height))
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=t)

    if progress_bar:
        p.set_fragment(pgbar_shader)
        time_animkf = [AnimKeyFrameScalar(0, 0),
                       AnimKeyFrameScalar(cfg.duration, 1)]
        time = UniformScalar(anim=AnimationScalar(time_animkf))
        ar = UniformScalar(cfg.aspect_ratio)
        render.update_uniforms(time=time, ar=ar)
    return render

frag_data='''
#version 100
void main(void)
{
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
'''

@scene()
def centered_masked_media(cfg):
    cfg.duration = 2
    cfg.glstates = [
        GLState(GL.GL_STENCIL_TEST, GL.GL_TRUE),
    ]

    g = Group()

    q = Quad((-0.2, -0.2, 0), (0.4, 0.0, 0.0), (0.0, 0.4, 0.0))
    p = Program(fragment=frag_data)
    node = Render(q, p)
    node.add_glstates(GLStencilState(GL.GL_TRUE,
                                     0xFF,
                                     GL.GL_ALWAYS,
                                     1,
                                     0xFF,
                                     GL.GL_KEEP,
                                     GL.GL_KEEP,
                                     GL.GL_REPLACE),
                      GLColorState(GL.GL_TRUE, 0, 0, 0, 0))


    scale_animkf = [AnimKeyFrameVec3(0, (0.1,  0.1, 1.0)),
                    AnimKeyFrameVec3(10, (10., 10.0,  3), "exp_out")]
    node = Scale(node, anim=AnimationVec3(scale_animkf))

    rotate_animkf = [AnimKeyFrameScalar(0, 0),
                     AnimKeyFrameScalar(cfg.duration, 360, "exp_out")]
    node = Rotate(node, anim=AnimationScalar(rotate_animkf))

    g.add_children(node)

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    p = Program()
    node = Render(q, p)
    node.update_textures(tex0=t)
    node.add_glstates(GLStencilState(GL.GL_TRUE,
                                     0x00,
                                     GL.GL_EQUAL,
                                     1,
                                     0xFF,
                                     GL.GL_KEEP,
                                     GL.GL_KEEP,
                                     GL.GL_KEEP),
                      GLColorState(GL.GL_TRUE, 1, 1, 1, 1))

    g.add_children(node)
    return g


@scene(npoints={'type': 'range', 'range': [3, 300]},
       radius={'type': 'range', 'range': [0.01, 5], 'unit_base': 100})
def media_in_circle(cfg, npoints=64, radius=0.5):
    circle = Circle(npoints=npoints, radius=radius)
    m = Media(cfg.medias[0].filename)
    t = Texture(data_src=m)
    p = Program()
    render = Render(circle, p)
    render.update_textures(tex0=t)
    return render


@scene(speed={'type': 'range', 'range': [0.01,2], 'unit_base': 1000})
def playback_speed(cfg, speed=1.0):
    cfg.duration = cfg.medias[0].duration / speed

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    time_animkf = [AnimKeyFrameScalar(0, 0),
                   AnimKeyFrameScalar(cfg.duration, cfg.duration * speed)]
    m = Media(cfg.medias[0].filename, initial_seek=5, time_anim=AnimationScalar(time_animkf))
    t = Texture(data_src=m)
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=t)
    return render
