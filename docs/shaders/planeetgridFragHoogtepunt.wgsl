//Fragment-shader voor de highlight/cursor-pass: kleurt de cellen binnen het
//penseel-bereik (zelfde gewichtsfunctie als penseel.comp) en tekent de cursorkleur
//op de center-cel. Additief blenden + diepte-test LessEqual (geen diepte-schrijven).
#include "planeetDefinitiesRender.wgsl"
#include "penseelStructen.wgsl"

@group(2) @binding(4) var<storage, read> penseel : penseelBuffer;

struct naarFrag {
    @builtin(position) glPos : vec4f,
    @location(0) @interpolate(flat) celId : u32,
};

fn bolGewicht(id : u32) -> f32 {
    let centrum = oppervlakWereld(penseel.centerId, penseel.oppervlak);
    let hier    = oppervlakWereld(id, penseel.oppervlak);
    let d = distance(hier, centrum);
    return gaussiaansGewicht(d / max(penseel.straal, 0.0001), penseel.hardness);
}

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    let id = in.celId;
    if(penseel.actief == 0u) { discard; }

    if(id == penseel.centerId) {
        return vec4f(1.0, 1.0, 0.0, 0.9);   //cursor (geel)
    }

    let w = select(bolGewicht(id), penseel.gewichten[id], penseel.selectie == selectieGrid);
    if(w <= 0.0) { discard; }

    return vec4f(0.25, 0.8, 1.0, 0.45 * w);  //bereik-tint (lichtblauw)
}
