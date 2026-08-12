//Hulpdefinities voor de reken-shaders (bind-groep 0: vijf opslag-buffers)
#include "planeetStructen.wgsl"

@group(0) @binding(0) var<storage, read_write> vakken0 : array<vak>;
@group(0) @binding(1) var<storage, read_write> vakken1 : array<vak>;
@group(0) @binding(2) var<storage, read_write> vakMetas : array<vakMeta>;
@group(0) @binding(3) var<storage, read_write> reken    : rekenParameters;

fn buurID(id : u32, buur : u32) -> u32 {
    return vakMetas[id].buren[buur];
}

//Vervangt Niet-eindige (NaN/±inf) waarden door een zinnige vervanger en klemt
//daarna op [laag, hoog]. De extremen-hoge kleppen zijn een laatste vangnet;
//een uitbijter wordt dus teruggezet op zijn vorige waarde i.p.v. dat een
//NaN/inf de hele planeet besmet.
fn goed(x : f32, vervanger : f32, laag : f32, hoog : f32) -> f32 {
    if(!(x >= -1.0e30 && x <= 1.0e30)) {
        return vervanger;
    }
    return clamp(x, laag, hoog);
}

fn goedV2(v : vec2f, vervanger : f32, laag : f32, hoog : f32) -> vec2f {
    return vec2f(goed(v.x, vervanger, laag, hoog), goed(v.y, vervanger, laag, hoog));
}

fn hoogteverschil(id : u32, buurId : u32) -> f32 {
    //De kolom die stroomt telt het zwevende sediment mee: droesem beweegt zo met
    //het water mee en kan bij depositie nooit boven de (water+droesem)-kolom uitkomen.
    //Alleen het draagkracht-gedeelte (maxDichtheid x water) duwt echter de stroming;
    //sediment boven de concentratielimiet blijft zweven maar versnelt het water niet.
    return kolom(id) - kolom(buurId);
}

fn kolom(id : u32) -> f32 {
    let waterHoogte = max(0.0, vakken0[id].waterHoogte);
    return vakken0[id].grondHoogte + (waterHoogte + min(waterHoogte * maxDichtheid, vakken0[id].droesem));
}

fn hoogteBuur(id : u32, water : bool) -> f32 {
    return vakken0[id].grondHoogte + select(0.0, vakken0[id].waterSchijn, water);
}

//De buur waar de wind vandaan komt (stroomopwaarts), gezocht in het lokale
//raakvlak: wind is een vec2 (west, noord) en buurRicht[buur] is de richting
//naar de buur in dezelfde basis. De buur die het meeste TEGEN de wind in ligt
//is de donor-cel (eerste-orde upwind / semi-Lagrangiaans).
fn opwaartseBuur(id : u32, wind : vec2f) -> u32 {
    let n = vakMetas[id].burenAantal;
    var beste = id;
    var besteScore = 1.0e30;
    for(var buur = 0u; buur < n && buur < maxBuren; buur++) {
        let score = dot(vakMetas[id].buurRicht[buur], wind);
        if(score < besteScore) {
            besteScore = score;
            beste = vakMetas[id].buren[buur];
        }
    }
    return beste;
}

//Advecteer een scalar met de wind: meng de eigen waarde met die van de
//stroomopwaartse donor-cel, evenredig aan de windkracht (monotoon, niet-negatief).
fn advecteerWaarde(id : u32, wind : vec2f, sterkte : f32, eigen : f32, op : f32) -> f32 {
    let k = clamp(sterkte, 0.0, 1.0);
    return mix(eigen, op, k);
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