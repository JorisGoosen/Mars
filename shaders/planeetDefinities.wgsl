//Hulpdefinities voor de reken-shaders (bind-groep 0: vijf opslag-buffers)
#include "planeetStructen.wgsl"

@group(0) @binding(0) var<storage, read_write> vakken0 : array<vak>;
@group(0) @binding(1) var<storage, read_write> vakken1 : array<vak>;
@group(0) @binding(2) var<storage, read_write> vakMetas : array<vakMeta>;
@group(0) @binding(3) var<storage, read_write> reken    : rekenParameters;

fn buurID(id : u32, buur : u32) -> u32 {
    return vakMetas[id].buren[buur];
}

fn hoogteverschil(id : u32, buurId : u32) -> f32 {
    return ((vakken0[id].grondHoogte) + vakken0[id].waterHoogte) - ((vakken0[buurId].grondHoogte) + vakken0[buurId].waterHoogte);
}

fn hoogteBuur(id : u32, water : bool) -> f32 {
    return vakken0[id].grondHoogte + select(0.0, vakken0[id].waterSchijn, water);
}

fn vakHoogte(id : u32, water : bool) -> f32 {
    return max(0.001, 1.0 + (hoogteBuur(id, water) * reken.grondSchaal));
}

fn vakHoogteNormaal(id : u32, water : bool) -> vec3f {
    return vakMetas[id].normaal.xyz * vakHoogte(id, water);
}

fn berekenNormaal(id : u32, water : bool) -> vec3f {
    let burenAantal = vakMetas[id].burenAantal;
    let hier = vakHoogteNormaal(id, water);
    var kruis = vec3f(0.0);
    var buurPos = array<vec3f, maxBuren>(vec3f(0.0), vec3f(0.0), vec3f(0.0), vec3f(0.0), vec3f(0.0), vec3f(0.0));

    for(var p = 0u; p < burenAantal && p < maxBuren; p++) {
        buurPos[p] = vakHoogteNormaal(vakMetas[id].buren[p], water);
    }
    for(var i = 0u; i < burenAantal && i < maxBuren; i++) {
        let tussen = cross(buurPos[i] - hier, buurPos[(i + 1u) % burenAantal] - hier);
        kruis += tussen * select(-1.0, 1.0, dot(tussen, hier) >= 0.0);
    }
    return normalize(kruis);
}