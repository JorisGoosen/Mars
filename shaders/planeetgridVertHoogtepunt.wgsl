//Vertex-shader voor de highlight/cursor-pass: spiegelt de bovenste zichtbare
//oppervlakte (land, waterspiegel of ijskap) zodat de tint op dat oppervlak zit;
//de bol-gewichten worden in de fragment-shader uitgerekend.
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
    let hier = in.posV * (max(0.001, 1.0 + oppervlakTopHoogte(ID) * extra.grondSchaal) / extra.grondMult);
    uit.glPos = matrices.projectie * matrices.modelZicht * vec4f(hier, 1.0);
    return uit;
}
