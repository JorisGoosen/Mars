//WGSL vertex-shader voor de grond-pass van de planeet.
#include "planeetDefinitiesRender.wgsl"

struct matricesDaar {
    projectie  : mat4x4f,
    modelZicht : mat4x4f,
    transInvMV : mat4x4f,
};
@group(0) @binding(1) var<uniform> matrices : matricesDaar;

struct vertexIn {
    @location(0) posV : vec3f,
    @location(1) tex   : vec2f,
};

struct naarFrag {
    @builtin(position) glPos          : vec4f,
    @location(0) normaal        : vec3f,
    @location(1) hoeks          : vec3f,
    @location(2) texDraaien     : vec3f,
    @location(3) kleur          : vec4f,
    @location(4) waterHoogte    : f32,
    @location(5) grondHoogte    : f32,
    @location(6) snelheid       : vec2f,
    @location(7) leven          : f32,
    @location(8) plek           : vec2f,
    @location(9) pos            : vec4f,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    var grondKleur = vec4f(1.0);
    let soort = vakken0[ID].grondSoort;

    if      (soort == i32(gsZand))  { grondKleur = vec4f(0.7,  0.52, 0.3,  1.0); }
    else if (soort == i32(gsGrond)) { grondKleur = vec4f(0.16, 0.13, 0.1,  1.0); }
    else if (soort == i32(gsRots))  { grondKleur = vec4f(0.3,  0.3,  0.3,  1.0); }
    else if (soort == i32(gsKlei))  { grondKleur = vec4f(0.416,0.38, 0.344,1.0); }
    else if (soort == i32(gsIjs))   { grondKleur = vec4f(0.8,  0.8,  0.8,  1.0); }
    else if (soort == i32(gsLoess)) { grondKleur = vec4f(0.58, 0.45, 0.4,  1.0); }

    //Naadloze textuur-coordinaten (zie Tarini 2012); de frag-shader kiest s0 of s1 mbv fwidth
    uit.texDraaien = vec3f(in.tex.y, fract(in.tex.x), fract(in.tex.x + 0.5) - 0.5);
    uit.grondHoogte = vakken0[ID].grondHoogte;
    uit.kleur = grondKleur;
    uit.waterHoogte = vakken0[ID].waterSchijn;
    uit.snelheid = vakken0[ID].snelheid;
    uit.leven = vakken0[ID].leven;
    uit.plek = vakken0[ID].plek - floor(vakken0[ID].plek);

    let hier = in.posV * (vakHoogte(ID, false) / extra.grondMult);

    uit.normaal = normalize((matrices.modelZicht * vec4f(berekenNormaal(ID, false), 0.0)).xyz);
    uit.hoeks = cross(normalize((matrices.modelZicht * vec4f(vakHoogteNormaal(buurID(ID, 0u), false), 0.0)).xyz), uit.normaal);
    uit.pos = matrices.modelZicht * vec4f(hier, 1.0);
    uit.glPos = matrices.projectie * uit.pos;

    return uit;
}