//PCF-lookup in de schaduwkaart. Verwacht dat de omringende shader de globals
//zonSchaduwKaart (texture_2d<f32>) en zonSchaduwSmp (sampler) zelf declareert/bindt
//(in de fragment-shaders is dat bind-groep 3, in de reken-shaders bind-groep 1).
#include "zonSchaduw.wgsl"

//Vergelijkt de diepte van het punt met de kaart (3x3 PCF): 1 = verlicht, 0 = schaduw.
fn zonSchaduwFactor(pr : vec3f, kaartGrootte : f32) -> f32 {
    if(!zonBinnenKaart(pr)) {
        return 1.0;
    }
    let uv = zonSchaduwUV(pr);
    let texel = 1.0 / kaartGrootte;
    var som = 0.0;
    for(var dy = -1; dy <= 1; dy++) {
        for(var dx = -1; dx <= 1; dx++) {
            let diepte = textureSampleLevel(zonSchaduwKaart, zonSchaduwSmp, uv + vec2f(f32(dx), f32(dy)) * texel, 0.0).r;
            som += select(1.0, 0.0, pr.z - schaduwBias > diepte);
        }
    }
    return som / 9.0;
}
