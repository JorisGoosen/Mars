//Hulpdefinities voor de weergave-vertex-shaders.
//De opslag-buffers hangen hier aan bind-groep 2 (zoals Gereedschap die bindt).
#include "planeetStructen.wgsl"

@group(0) @binding(2) var<uniform> extra : extraParameters;

@group(2) @binding(0) var<storage, read> vakken0  : array<vak>;
@group(2) @binding(1) var<storage, read> vakken1  : array<vak>;
@group(2) @binding(2) var<storage, read> vakMetas : array<vakMeta>;

fn buurID(id : u32, buur : u32) -> u32 {
    return vakMetas[id].buren[buur];
}

fn hoogteBuur(id : u32, water : bool) -> f32 {
    return grondHoogte(vakken0[id]) + select(0.0, vakken0[id].waterSchijn, water);
}

fn vakHoogte(id : u32, water : bool) -> f32 {
    return max(0.001, 1.0 + (hoogteBuur(id, water) * extra.grondSchaal));
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

//Oppervlak-hoogte voor de bol-selectie: rots is de basis, zand/water/ijs tellen
//optioneel mee (bitvlag penseel.oppervlak, zie shaders/penseelStructen.wgsl).
fn oppervlakHoogte(id : u32, oppervlak : u32) -> f32 {
    var h = vakken0[id].rotsHoogte;
    if((oppervlak & 1u) != 0u) { h = h + vakken0[id].zandHoogte; }
    if((oppervlak & 2u) != 0u) { h = h + vakken0[id].waterSchijn; }
    if((oppervlak & 4u) != 0u) { h = h + vakken0[id].ijs; }
    return h;
}

//Straal (wereldlengte) van het oppervlak op een cel, voor de bol-afstand.
fn oppervlakStraal(id : u32, oppervlak : u32) -> f32 {
    return max(0.001, 1.0 + oppervlakHoogte(id, oppervlak) * extra.grondSchaal) / extra.grondMult;
}

//Wereldpositie van het oppervlak (voor de bol-afstand).
fn oppervlakWereld(id : u32, oppervlak : u32) -> vec3f {
    return vakMetas[id].normaal.xyz * oppervlakStraal(id, oppervlak);
}