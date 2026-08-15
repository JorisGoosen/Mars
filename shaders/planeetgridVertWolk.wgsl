//WGSL vertex-shader voor de wolk-pass: een doorzichtig wolkendek op een ABSOLUTE
//hoogte (straal vanaf Marscentrum) die per vak uit temperatuur, druk en damp wordt
//berekend (T/P-gestuurd dek) in DEZELFDE hoogte->straal-afbeelding als het terrein.
//
//Het dek zweeft dus in de atmosfeerlaag i.p.v. als een vast percentage boven de
//grond: bergtoppen die hoger reiken dan het lokale dek steken er bovenuit en
//hebben daar geen wolk. Sealevel = straal 1.0; het plafond = 90% van het hoogste
//terreinpunt (maxGrondHoogte, bij het laden bepaald).
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
    @builtin(position) glPos          : vec4f,
    @location(0) normaal        : vec3f,
    @location(1) texDraaien     : vec3f,
    @location(2) wolken         : f32,
    @location(3) grondHoogte    : f32,
    @location(4) pos            : vec4f,
    @location(5) modelPos       : vec3f,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    uit.texDraaien = vec3f(in.tex.y, fract(in.tex.x), fract(in.tex.x + 0.5) - 0.5);
    uit.grondHoogte = vakken0[ID].grondHoogte;

    let T = vakken0[ID].temperatuur;

    //Hoogste terreinpunt (bij het laden bepaald) in dezelfde hoogte-eenheden als
    //het terrein; de bijbehorende straal rMax. Sealevel = straal 1.0 (radius 1).
    let maxGrond = max(extra.maxGrondHoogte, 1.0);
    let rMax = (1.0 + maxGrond * extra.grondSchaal) / max(extra.grondMult, 0.0001);

    //Plafond = 90% van het hoogste punt (gemeten vanaf sealevel). Het dek zweeft
    //dus in de atmosfeerlaag, boven sealevel maar onder de hoogste piek.
    let rPlafond = 1.0 + 0.9 * (rMax - 1.0);

    //Dekhoogte uit de temperatuur: warme lucht houdt de damp hoger vast (hoger
    //dek), koude lucht laat het dek zakken. Soepel en stabiel (niet afhankelijk
    //van de huidige damp, anders versterkt het wegdrainen zichzelf). Het dek
    //zweeft hoog zodat alleen hoge bergtoppen erbovenuit steken.
    let dekFract = clamp(0.75 + 0.15 * clamp((T - 255.0) / 30.0, -1.0, 1.0), 0.6, 0.9);

    //Absolute straal: sealevel (1.0) + fractie van het parcours tot het plafond.
    let rWolk = 1.0 + dekFract * (rPlafond - 1.0);

    //Schil op absolute straal rWolk (onafhankelijk van het lokale terrein).
    let hierWolk = in.posV * max(rWolk, 0.15);

    //Bergtoppen boven het lokale dek: geen wolk (piek steekt erbovenuit).
    let terreinR = vakHoogte(ID, false) / extra.grondMult;
    if(terreinR >= rWolk) {
        uit.wolken = 0.0;
    } else {
        uit.wolken = vakken0[ID].wolken;
    }

    //Radiale normaal: gladde belichting over het dek (i.p.v. het terrein te volgen)
    uit.normaal = normalize((matrices.modelZicht * vec4f(in.posV, 0.0)).xyz);
    uit.modelPos = hierWolk;
    uit.pos = matrices.modelZicht * vec4f(hierWolk, 1.0);
    uit.glPos = matrices.projectie * uit.pos;

    return uit;
}
