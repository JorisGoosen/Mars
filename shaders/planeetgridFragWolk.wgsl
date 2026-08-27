//WGSL fragment-shader voor de wolk-pass: volumetrische wolken met echte dikte.
//Het dek wordt langs de kijkstraal geraymarched tussen de per-cel basis (rBasis)
//en top (rTop), met een verticaal dichtheidsprofiel geschaald op de wolken-waarde
//van de cel. Belichting (zon + terreinschaduw) blijft zoals voorheen: een
//richtingslicht op oneindig met schaduw uit de schaduwkaart.
#include "planeetStructen.wgsl"

//De schaduwkaart van de zon (bind-groep 3; het framework bindt anders een 1x1 wit
//hulpje, dat als diepte 1.0 leest = alles verlicht).
@group(3) @binding(0) var zonSchaduwKaart : texture_2d<f32>;
@group(3) @binding(1) var zonSchaduwSmp   : sampler;

#include "zonSchaduwPCF.wgsl"

struct matricesDaar {
    projectie  : mat4x4f,
    modelZicht : mat4x4f,
    transInvMV : mat4x4f,
};
@group(0) @binding(1) var<uniform> matrices : matricesDaar;
@group(0) @binding(2) var<uniform> extra : extraParameters;

struct naarFrag {
    @builtin(position) glPos          : vec4f,
    @location(0) normaal        : vec3f,
    @location(1) texDraaien     : vec3f,
    @location(2) wolken         : f32,
    @location(3) grondHoogte    : f32,
    @location(4) pos            : vec4f,
    @location(5) modelPos       : vec3f,
    @location(6) schil          : vec2f,   //(rBasis, rTop)
    @builtin(front_facing) frontKant : bool,
};

//Optische dichtheid van het wolkendek (tunable; hoger = dekkender)
const wolkDichtheid = 60.0;
const wolkStappen   = 16;
const wolkMinZicht  = 0.1;   //onder deze wolkenwaarde geen zichtbare wolk

//Verticaal dichtheidsprofiel: 0 aan basis én top, piek in het midden.
fn wolkProfiel(t : f32) -> f32 {
    return smoothstep(0.0, 0.25, t) * smoothstep(1.0, 0.65, t);
}

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    //Alleen dikkere wolken tellen als wolken; een dunne nevel wordt niet getoond.
    if(in.wolken < wolkMinZicht) {
        discard;
    }

    let rBasis = in.schil.x;
    let rTop   = in.schil.y;
    let dikte  = max(rTop - rBasis, 0.0001);

    //De zon is een richtingslicht op oneindig (zelfde benadering als land/water).
    let zonModel = normalize(extra.zonPos.xyz);
    let zonView  = normalize((matrices.transInvMV * vec4f(zonModel, 0.0)).xyz);
    var diffuus  = max(0.0, dot(zonView, in.normaal));

    //Terreinschaduw op het dek (eenmalig): wolken achter een berg of in de schaduw
    //van de terminator krijgen geen direct zonlicht.
    if(extra.schaduwAan > 0.5 && diffuus > 0.0) {
        let straal = zonStraal(extra.grondMult, extra.grondSchaal, extra.maxGrondHoogte);
        diffuus *= zonSchaduwFactor(zonProjectie(in.modelPos, zonModel, straal), extra.schaduwGrootte, diffuus);
    }

    //Kijkstraal: vanaf het schil-oppervlak naar binnen marcheren. Voorkanten lopen
    //verder de planeet in (langs de straal), achterkanten marcheren terug naar het
    //centrum (de verre schil aan de andere kant van de bol).
    let rayDir   = normalize(in.modelPos - extra.kijkPlek);
    let marchDir = select(-rayDir, rayDir, in.frontKant);

    //Afstand tot de basis rBasis langs de marchrichting (eerste bol-snijding). Bij
    //een scherende straal die rBasis niet raakt (rand van de schijf) valt hij terug
    //op een vaste dubbele dikte, zodat de limb-band niet leeg wordt.
    let c     = dot(in.modelPos, marchDir);
    let disc  = c * c - (rTop * rTop - rBasis * rBasis);
    let afstand = select(2.0 * dikte, -c - sqrt(max(disc, 0.0)), disc > 0.0);

    //Marcheer en sommeer de dichtheid (opticaal geintegreerd over de kolom).
    var geaccumuleerd = 0.0;
    let stap = afstand / f32(wolkStappen);
    for(var i = 0; i < wolkStappen; i++) {
        let t = (f32(i) + 0.5) * stap;
        let p = in.modelPos + marchDir * t;
        let r = length(p);
        let h = clamp((r - rBasis) / dikte, 0.0, 1.0);
        geaccumuleerd += wolkProfiel(h) * stap;
    }

    //Optische dikte -> alpha; de wolkenwaarde van de cel schaalt de kolomdichtheid.
    let bewolkt = smoothstep(wolkMinZicht, 0.5, clamp(in.wolken, 0.0, 1.0));
    var a = 1.0 - exp(-geaccumuleerd * wolkDichtheid * bewolkt);
    a = clamp(a, 0.0, 0.9);
    a *= clamp(extra.wolkAlpha, 0.0, 1.0); //doorzichtigheids-slider

    //Nachtzijde zakt naar een zwak schemerlicht i.p.v. een vlakke 0.25-vloer.
    let helderheid = clamp(0.10 + 0.90 * diffuus, 0.0, 1.0);
    let kleur = mix(vec3f(0.6, 0.63, 0.70), vec3f(1.0, 0.99, 0.96), clamp(diffuus * 1.5, 0.0, 1.0));

    return vec4f(kleur * helderheid, a);
}
