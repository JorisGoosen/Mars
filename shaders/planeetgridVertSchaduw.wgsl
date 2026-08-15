//WGSL vertex-shader voor de schaduw-pass: rendert het terrein in de orthografische
//dieptekaart van de zon (depth-only pass, geen fragment-stage). De verplaatsing is
//identiek aan de land-pass (vakHoogte / grondMult) en de projectie is analytisch
//(zonProjectie), precies zoals de fragment-lookup en de reken-shaders hem gebruiken.
#include "planeetDefinitiesRender.wgsl"
#include "zonSchaduw.wgsl"

struct vertexIn {
    @location(0) posV : vec3f,
    @location(1) tex   : vec2f,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> @builtin(position) vec4f {
    let ID = vertexIndex;

    let zon  = normalize(extra.zonPos.xyz);
    let hier = in.posV * (vakHoogte(ID, false) / extra.grondMult);
    let pr   = zonProjectie(hier, zon, zonStraal(extra.grondMult, extra.grondSchaal, extra.maxGrondHoogte));

    return vec4f(pr.x, pr.y, pr.z, 1.0);
}
