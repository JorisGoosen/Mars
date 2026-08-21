//WGSL fragment-shader voor de wolk-pass: zachte, door de zon belichte wolken
//met doorzichtigheid uit de bewolkingswaarde van de cel. Het dek krijgt echt
//richtingslicht (donkere nachtzijde) en terreinschaduwen uit de schaduwkaart.
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
};

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    //Alleen dikkere wolken tellen als wolken; een dunne nevel (onder 0.1)
    //wordt niet getoond, zodat de planeet niet overal wit is.
    if(in.wolken < 0.1) {
        discard;
    }

    //De zon is een richtingslicht op oneindig (zelfde benadering als land/water).
    let zonModel = normalize(extra.zonPos.xyz);
    let zonView  = normalize((matrices.transInvMV * vec4f(zonModel, 0.0)).xyz);
    var diffuus  = max(0.0, dot(zonView, in.normaal));

    //Terreinschaduw op het dek: wolken die achter een berg of in de schaduw van de
    //terminator hangen krijgen geen direct zonlicht. Het dek zweeft boven het
    //terrein, dus de lookup vergelijkt tegen terrein+ijs die dieper ligt.
    if(extra.schaduwAan > 0.5 && diffuus > 0.0) {
        let straal = zonStraal(extra.grondMult, extra.grondSchaal, extra.maxGrondHoogte);
        diffuus *= zonSchaduwFactor(zonProjectie(in.modelPos, zonModel, straal), extra.schaduwGrootte);
    }

    //Lichte golfjes voor wat volume/wisp in de wolken
    let wolkenWaas = in.wolken * (0.8 + 0.2 * sin(in.pos.x * 4.0 + in.pos.y * 3.0 + in.pos.z * 2.0));

    let bewolkt = smoothstep(0.1, 0.35, in.wolken);
    var a = bewolkt * clamp(wolkenWaas * 3.0, 0.0, 1.0);
    a = clamp(a, 0.0, 0.85);
    a *= clamp(extra.wolkAlpha, 0.0, 1.0); //doorzichtigheids-slider

    //Nachtzijde zakt naar een zwak schemerlicht i.p.v. een vlakke 0.25-vloer.
    let helderheid = clamp(0.10 + 0.90 * diffuus, 0.0, 1.0);
    let kleur = mix(vec3f(0.6, 0.63, 0.70), vec3f(1.0, 0.99, 0.96), clamp(diffuus * 1.5, 0.0, 1.0));

    return vec4f(kleur * helderheid, a);
}
