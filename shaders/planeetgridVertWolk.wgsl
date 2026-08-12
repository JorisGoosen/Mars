//WGSL vertex-shader voor de wolk-pass: een half-doorzichtige schil die net
//boven het terrein zweeft, opgeblazen waar een cel bewolkt is.
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
    @location(1) texDraaien     : vec3f,
    @location(2) wolken         : f32,
    @location(3) grondHoogte    : f32,
    @location(4) pos            : vec4f,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    uit.texDraaien = vec3f(in.tex.y, fract(in.tex.x), fract(in.tex.x + 0.5) - 0.5);
    uit.wolken     = vakken0[ID].wolken;
    uit.grondHoogte = vakken0[ID].grondHoogte;

    let hier = in.posV * (vakHoogte(ID, false) / extra.grondMult);

    //Wolken zweven iets boven het terrein; opgeblazen evenredig met de bewolking
    let ophef = 1.0 + 0.015 + clamp(uit.wolken, 0.0, 1.0) * 0.18;
    let hierWolk = hier * ophef;

    uit.normaal = normalize((matrices.modelZicht * vec4f(berekenNormaal(ID, false), 0.0)).xyz);
    uit.pos = matrices.modelZicht * vec4f(hierWolk, 1.0);
    uit.glPos = matrices.projectie * uit.pos;

    return uit;
}
