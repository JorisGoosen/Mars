//WGSL vertex-shader voor de schaduw-pass: rendert het terrein in de orthografische
//dieptekaart van de zon (depth-only pass, geen fragment-stage). De verplaatsing is
//identiek aan de land-pass (vakHoogte / grondMult) en de projectie is analytisch
//(zonProjectie), precies zoals de fragment-lookup en de reken-shaders hem gebruiken.
#include "planeetDefinitiesRender.wgsl"

struct vertexIn {
    @location(0) posV : vec3f,
    @location(1) tex   : vec2f,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> @builtin(position) vec4f {
    let ID = vertexIndex;

    //De kaart bevat het bovenste zichtbare oppervlak: terrein + waterspiegel +
    //ijs. Het ijs drijft op het water (grond + waterSchijn + ijs), precies zoals
    //de render-pass hem tekent — niet zoals de sim het als "grond onder water" rekent.
    let schaduwHoogte = grondHoogte(vakken0[ID]) + vakken0[ID].waterSchijn + vakken0[ID].ijs;

    let zon  = normalize(extra.zonPos.xyz);
    //Duw de caster een epsilon van de zon af zodat vlakken die van de zon af kijken
    //(die zelf in de kaart zitten) hun eigen diepte niet als schaduw lezen.
    let hier = in.posV * (max(0.001, 1.0 + schaduwHoogte * extra.grondSchaal) / extra.grondMult) - zon * schaduwEpsilon;
    let pr   = zonProjectie(hier, zon, zonStraal(extra.grondMult, extra.grondSchaal, extra.maxGrondHoogte));

    return vec4f(pr.x, pr.y, pr.z, 1.0);
}
