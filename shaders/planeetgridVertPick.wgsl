//Vertex-shader voor de pick-pass: tekent de planeet op exact dezelfde (land-)
//verplaatsing als de grond-pass, zodat de cel-ID onder de cursor de cel op het
//échte oppervlak is (berg/occlusie kloppen). De fragment-shader schrijft de ID weg.
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
    @builtin(position) glPos : vec4f,
    @location(0) @interpolate(flat) celId : u32,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;
    uit.celId = ID;
    let hier = in.posV * (vakHoogte(ID, false) / extra.grondMult);
    uit.glPos = matrices.projectie * matrices.modelZicht * vec4f(hier, 1.0);
    return uit;
}
