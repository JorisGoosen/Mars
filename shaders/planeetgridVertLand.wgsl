//WGSL vertex-shader voor de grond-pass van de planeet.
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

//Laagdikte (in terreinhogte-eenheden) waarboven de overgang zand->rots volledig is
const overgangDikte = 4.0;

struct naarFrag {
    @builtin(position) glPos          : vec4f,
    @location(0) normaal        : vec3f,
    @location(1) droesem        : f32, //zwevend sediment (wateroverlay); hergebruikt de slot van het oude, ongebruikte hoeks-veld (WebGPU limiet: 16 inter-stage variabelen)
    @location(2) texDraaien     : vec3f,
    @location(3) kleur          : vec4f,
    @location(4) waterHoogte    : f32,
    @location(5) grondHoogte    : f32,
    @location(6) snelheid       : vec2f,
    @location(7) leven          : f32,
    @location(8) plek           : vec2f,
    @location(9) pos            : vec4f,
    @location(10) temperatuur   : f32,
    @location(11) wind          : vec2f,
    @location(12) luchtdruk     : f32,
    @location(13) modelPos      : vec3f,
    @location(14) overlayVelden : vec4f, //(bodemVocht, ijs, wolken, luchtVocht)
    @location(15) zonZicht      : f32,
};

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    var grondKleur = vec4f(1.0);

    //Zachte overgang: waar de zand/deklaag dun wordt, gaat het oppervlak van zand
    //geleidelijk over in de daaronder liggende Mars-rots, i.p.v. een harde knip.
    let zandlaag  = vakken0[ID].zandHoogte;
    let overgang  = clamp(zandlaag / overgangDikte, 0.0, 1.0);
    let zandKleur = vec4f(0.7, 0.52, 0.3, 1.0);
    let rotsKleur = vec4f(0.95, 0.2, 0.05, 1.0); //Mars-rood
    grondKleur = mix(rotsKleur, zandKleur, overgang);

    //Naadloze textuur-coordinaten (zie Tarini 2012); de frag-shader kiest s0 of s1 mbv fwidth
    uit.texDraaien = vec3f(in.tex.y, fract(in.tex.x), fract(in.tex.x + 0.5) - 0.5);
    uit.grondHoogte = grondHoogte(vakken0[ID]);
    uit.kleur = grondKleur;
    uit.temperatuur = vakken0[ID].temperatuur;
    uit.waterHoogte = vakken0[ID].waterSchijn;
    uit.snelheid = vakken0[ID].snelheid;
    uit.leven = vakken0[ID].leven;
    uit.plek = vakken0[ID].plek - floor(vakken0[ID].plek);
    uit.wind      = vakken0[ID].wind;
    uit.luchtdruk = vakken0[ID].luchtdruk;
    uit.overlayVelden = vec4f(vakken0[ID].bodemVocht, vakken0[ID].ijs, vakken0[ID].wolken, vakken0[ID].luchtVocht);
    uit.zonZicht   = vakken0[ID].zonZicht;
    uit.droesem   = vakken0[ID].droesem;

    let hier = in.posV * (vakHoogte(ID, false) / extra.grondMult);

    uit.modelPos = hier;
    uit.normaal = normalize((matrices.modelZicht * vec4f(berekenNormaal(ID, false), 0.0)).xyz);
    uit.pos = matrices.modelZicht * vec4f(hier, 1.0);
    uit.glPos = matrices.projectie * uit.pos;

    return uit;
}