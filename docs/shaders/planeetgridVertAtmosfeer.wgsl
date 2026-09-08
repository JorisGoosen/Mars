//WGSL vertex-shader voor de atmosfeer-pass: een vaste schil rond de planeet,
//groter dan het wolkendek, als scherm voor de Rayleigh-gloed. De dikte volgt
//extra.atmosfeerDikte (GUI-slider) in genormaliseerde lagen boven sealevel.
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
    @builtin(position) glPos  : vec4f,
    @location(0) modelPos     : vec3f,
    @location(1) schilNormaal : vec3f,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;

    //Zelfde straal-maten als het wolkendek: hoogste terreinpunt -> rMax.
    let maxGrond = max(extra.maxGrondHoogte, 1.0);
    let rMax = (1.0 + maxGrond * extra.grondSchaal) / max(extra.grondMult, 0.0001);

    //Schil = rMax + een fractie (extra.atmosfeerDikte) van rMax erbovenop,
    //zo blijft hij altijd BOVEN de planeet, ook als de planeet kleiner is dan 1.
    let rAtmosfeer = rMax * (1.0 + extra.atmosfeerDikte);

    uit.schilNormaal = normalize(in.posV);
    uit.modelPos = in.posV * rAtmosfeer;
    uit.glPos = matrices.projectie * matrices.modelZicht * vec4f(uit.modelPos, 1.0);

    return uit;
}
