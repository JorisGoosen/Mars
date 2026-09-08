//Hulpdefinities voor de weergave-vertex-shaders.
//De opslag-buffers hangen hier aan bind-groep 2 (zoals Gereedschap die bindt).
#include "planeetStructen.wgsl"
#include "zonSchaduw.wgsl"

@group(0) @binding(2) var<uniform> extra : extraParameters;

@group(2) @binding(0) var<storage, read> vakken0  : array<vak>;
@group(2) @binding(1) var<storage, read> vakken1  : array<vak>;
@group(2) @binding(2) var<storage, read> vakMetas : array<vakMeta>;

fn buurID(id : u32, buur : u32) -> u32 {
    return vakMetas[id].buren[buur];
}

//Hoogtemodus: 0 = land, 1 = water, 2 = ijs
const hLand  = 0u;
const hWater = 1u;
const hIjs   = 2u;

fn hoogteBuur(id : u32, modus : u32) -> f32 {
    if(modus == hIjs) {
        return vakken0[id].ijsSchijn;
    }
    let v = vakken0[id];
    return grondHoogte(v) + select(0.0, v.waterSchijn, modus == hWater);
}

fn vakHoogte(id : u32, modus : u32) -> f32 {
    return max(0.001, 1.0 + (hoogteBuur(id, modus) * extra.grondSchaal));
}

fn vakHoogteNormaal(id : u32, modus : u32) -> vec3f {
    return vakMetas[id].normaal.xyz * vakHoogte(id, modus);
}

//Hoogte van het bovenste zichtbare oppervlak: grond + waterspiegel + ijs.
//Gebruikt door de ijs-pass (bovenkant) én door highlight/pick, zodat die op
//land, water én ijs liggen i.p.v. altijd op het kale terrein.
fn oppervlakTopHoogte(id : u32) -> f32 {
    let v = vakken0[id];
    return select(grondHoogte(v) + v.waterSchijn, v.ijsSchijn, v.ijs > miniJs);
}

//Spiegelpositie van de schaduwkaart-CASTERS: het bovenste zichtbare oppervlak op
//de vertex-richting posV, met dezelfde epsilon-push langs de zon als de
//schaduw-pass zelf (planeetgridVertSchaduw.wgsl). Elke schaduw-LOOKUP moet deze
//positie bemonsteren — niet het eigen render-oppervlak — anders correleren kaart
//en lezer niet: de oude zeebodem-lookup las op elke nat/ijectige cel het
//"waterspiegel-membraan" erboven en gaf bij lage zon schaduwbanden die dwars
//door de planeet leken te lopen.
fn schaduwSpiegelPos(posV : vec3f, id : u32) -> vec3f {
    let top = oppervlakTopHoogte(id);
    let zon = normalize(extra.zonPos.xyz);
    return posV * (max(0.001, 1.0 + top * extra.grondSchaal) / extra.grondMult) - zon * schaduwEpsilon;
}

fn berekenNormaal(id : u32, modus : u32) -> vec3f {
    let burenAantal = vakMetas[id].burenAantal;
    let hier = vakHoogteNormaal(id, modus);
    var kruis = vec3f(0.0);
    var buurPos = array<vec3f, maxBuren>(vec3f(0.0), vec3f(0.0), vec3f(0.0), vec3f(0.0), vec3f(0.0), vec3f(0.0));

    for(var p = 0u; p < burenAantal && p < maxBuren; p++) {
        buurPos[p] = vakHoogteNormaal(vakMetas[id].buren[p], modus);
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