//WGSL fragment-shader voor de grond-pass van de planeet.
#include "planeetStructen.wgsl"

struct matricesDaar {
    projectie  : mat4x4f,
    modelZicht : mat4x4f,
    transInvMV : mat4x4f,
};
@group(0) @binding(1) var<uniform> matrices : matricesDaar;
@group(0) @binding(2) var<uniform> extra : extraParameters;
@group(1) @binding(0) var marsHoogteTex : texture_2d<f32>;
@group(1) @binding(1) var marsHoogteSmp : sampler;

struct naarFrag {
    @builtin(position) glPos          : vec4f,
    @location(0) normaal        : vec3f,
    @location(1) hoeks          : vec3f,
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
};

//Temperatuuroverlay-kleurkaart: -25 °C (= 248 K) blauw, 0 °C (= 273 K) groen,
//+25 °C (= 298 K) rood. Alles kouder dan -25 °C klemt op blauw.
fn temperatuurKleur(TC : f32) -> vec3f {
    let t = clamp((TC - 248.0) / 50.0, 0.0, 1.0);   //0 = blauw, 1 = rood
    var kleur = mix(vec3f(0.2, 0.35, 1.0), vec3f(0.2, 0.85, 0.2), smoothstep(0.0, 0.5, t));
    kleur = mix(kleur, vec3f(1.0, 0.25, 0.1), smoothstep(0.5, 1.0, t));
    return kleur;
}

//Windoverlay (toets V): rood = wind-component langs de lengtebreedte-as
//(oost-west), groen = langs de hoogtebreedte-as (noord-zuid), blauw = luchtdruk.
//De wind-componenten zijn gecentreerd rond 0.5 (tegenwind = donker, meewind = helder),
//de druk wordt genormaliseerd over [minLuchtdruk, maxLuchtdruk].
fn windKleur(W : vec2f, P : f32) -> vec3f {
    let wScale = 2.0; //maxWindsnel (zie luchtStroming.comp)
    let r = clamp(W.x / wScale * 0.5 + 0.5, 0.0, 1.0); //oost-west
    let g = clamp(W.y / wScale * 0.5 + 0.5, 0.0, 1.0); //noord-zuid
    let b = clamp((P - 0.2) / 4.8, 0.0, 1.0);          //luchtdruk
    return vec3f(r, g, b);
}

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    //Naadloze textuur-coordinaten op basis van fwidth (Tarini 2012)
    let naadloosTex = vec2f(select(in.texDraaien.z, in.texDraaien.y, fwidth(in.texDraaien.y) <= fwidth(in.texDraaien.z) + 0.000001), in.texDraaien.x);
    let marsHoogte = textureSampleLevel(marsHoogteTex, marsHoogteSmp, naadloosTex, 0.0).r;

    //De zon is een richtingslicht op oneindig: de richting in modelruimte wordt
    //naar de view-ruimte gebracht, zodat de belichting consistent is met de
    //normaal (= na modelZicht). Geen 'puntbron' op de planeet meer.
    let zonModel = normalize(extra.zonPos.xyz);
    let zonView  = normalize((matrices.transInvMV * vec4f(zonModel, 0.0)).xyz);
    let diffuus = max(0.0, dot(zonView, in.normaal));

    var kleur = mix(in.kleur * clamp(marsHoogte * 3.0, 0.35, 1.0), vec4f(0.0, 0.35, 0.0, 1.0), clamp(in.leven, 0.0, 1.0));

    //Temperatuuroverlay (toets T): vervang de oppervlaktekleur door de temp-kleurkaart
    if(extra.toonTemperatuur > 0.5) {
        kleur = vec4f(temperatuurKleur(in.temperatuur), 1.0);
    }

    //Windoverlay (toets V): rood/groen = windrichting, blauw = luchtdruk
    if(extra.toonWind > 0.5) {
        kleur = vec4f(windKleur(in.wind, in.luchtdruk), 1.0);
    }

    return kleur * max(0.2, diffuus);
}
