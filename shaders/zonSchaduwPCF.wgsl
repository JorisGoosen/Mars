//PCF-lookup in de schaduwkaart. Verwacht dat de omringende shader de globals
//zonSchaduwKaart (texture_2d<f32>) en zonSchaduwSmp (sampler) zelf declareert/bindt
//(in de fragment-shaders is dat bind-groep 3, in de reken-shaders bind-groep 1).
#include "zonSchaduw.wgsl"

//Vergelijkt de diepte van het punt met de kaart (3x3 PCF): 1 = vol licht,
//0 = diepe schaduw. De rand loopt gradueel (penumbra): over schaduwZacht aan
//diepteverschil zakt de factor van 1 naar 0, i.p.v. een harde aan/uit-knip.
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
            let d = pr.z - schaduwBias - diepte;               //>0 = achter de caster
            som += 1.0 - smoothstep(0.0, schaduwZacht, d);
        }
    }
    return som / 9.0;
}
