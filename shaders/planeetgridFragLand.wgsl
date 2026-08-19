//WGSL fragment-shader voor de grond-pass van de planeet.
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
@group(1) @binding(0) var marsHoogteTex : texture_2d<f32>;
@group(1) @binding(1) var marsHoogteSmp : sampler;

struct naarFrag {
    @builtin(position) glPos          : vec4f,
    @location(0) normaal        : vec3f,
    @location(1) droesem        : f32, //zwevend sediment (wateroverlay)
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

//Temperatuuroverlay-kleurkaart: -35 °C (= 238 K) blauw, 0 °C (= 273 K) groen,
//+35 °C (= 308 K) rood. Alles kouder dan -35 °C klemt op blauw.
fn temperatuurKleur(TC : f32) -> vec3f {
    let t = clamp((TC - 238.0) / 70.0, 0.0, 1.0);   //0 = blauw, 1 = rood
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

//Wateroverlay: rood = waterstroming langs de oost-west-as, groen = langs de
//noord-zuid-as (beide gecentreerd rond 0.5), blauw = genormaliseerd zwevend
//sediment (droesem). Analoog aan windKleur, maar voor de waterstroom (snelheid).
fn waterKleur(S : vec2f, D : f32) -> vec3f {
    let sSchaal = 2.0;   //typische waterstroomsnelheid (zie waterDruk.comp)
    let r = clamp(S.x / sSchaal * 0.5 + 0.5, 0.0, 1.0); //oost-west
    let g = clamp(S.y / sSchaal * 0.5 + 0.5, 0.0, 1.0); //noord-zuid
    let b = clamp(D * 2.0, 0.0, 1.0);                   //droesem
    return vec3f(r, g, b);
}

//Enkel-veld-overlay: een stuk luchtdruk/temperatuur-gevoel geeft meer contrast.
fn enkelVeldKleur(v : f32) -> vec3f {
    let t = clamp(v, 0.0, 1.0);
    //donker lila (0) -> donkergroen (0.5) -> fel geel (1)
    var kleur = mix(vec3f(0.45, 0.2, 0.6), vec3f(0.1, 0.5, 0.15), smoothstep(0.0, 0.5, t));
    kleur = mix(kleur, vec3f(1.0, 0.9, 0.1), smoothstep(0.5, 1.0, t));
    return kleur;
}

fn grijs(fel : f32) -> vec3f {
    let f = clamp(fel, 0.0, 1.0);
    return vec3f(0.05 + 0.95 * f);
}

//Overlay voor het (o)verlaag-keuzeveld: index naar kleur.
fn overlayKleurKeuze(in : naarFrag) -> vec3f {
    let bodemVocht = in.overlayVelden.x;
    let ijs        = in.overlayVelden.y;
    let wolken     = in.overlayVelden.z;
    let luchtVocht = in.overlayVelden.w;
    switch(i32(extra.overlayKeuze)) {
        case 1: { return temperatuurKleur(in.temperatuur); }
        case 2: { return windKleur(in.wind, in.luchtdruk); }
        case 3: { return enkelVeldKleur(bodemVocht * 2.0); } //0..veldCapaciteit (0.5)
        case 4: { //blauw = luchtvocht, groen = wolken(condens), rood = luchtdruk
            let b = clamp(luchtVocht / 0.5, 0.0, 1.0);
            let g = clamp(wolken * 2.0, 0.0, 1.0);
            let r = clamp((in.luchtdruk - 0.2) / 4.8, 0.0, 1.0);
            return vec3f(r, g, b);
        }
        case 5: { //rood = ijs, groen = bodemvocht, blauw = water
            let r = clamp(ijs * 5.0, 0.0, 1.0);
            let g = clamp(bodemVocht * 2.0, 0.0, 1.0);
            let b = clamp(in.waterHoogte * 2.0, 0.0, 1.0);
            return vec3f(r, g, b);
        }
        case 6: { return mix(vec3f(0.4, 0.45, 0.55), vec3f(1.0, 0.99, 0.96), clamp(wolken * 3.0, 0.0, 1.0)); }
        case 7: { return grijs(in.zonZicht); }
        case 8: { return vec3f(0.0, in.leven, 0.0) + vec3f(0.02); } //groen naar dichtheid leven
        case 9: { return grijs(clamp(in.grondHoogte / extra.maxGrondHoogte, 0.0, 1.0)); } //terreinhoogte
        case 10: { return waterKleur(in.snelheid, in.droesem); } //waterstroming + zwevend sediment
        default: { return vec3f(1.0, 0.0, 1.0); } //magenta = onbekende keuze
    }
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
    let diffuus  = max(0.0, dot(zonView, in.normaal));

    //Terrein-schaduw uit de schaduwkaart ( bergen/flanken die dit punt overschaduwen).
    //Dezelfde analytische zon-projectie als de schaduw-pass en de reken-shaders.
    var schaduw = 1.0;
    if(extra.schaduwAan > 0.5 && diffuus > 0.0) {
        let straal = zonStraal(extra.grondMult, extra.grondSchaal, extra.maxGrondHoogte);
        schaduw = zonSchaduwFactor(zonProjectie(in.modelPos, zonModel, straal), extra.schaduwGrootte);
    }

    var kleur = mix(in.kleur * clamp(marsHoogte * 3.0, 0.35, 1.0), vec4f(0.0, 0.35, 0.0, 1.0), clamp(in.leven, 0.0, 1.0));

    //Overlay (cijfertoetsen 1-0): vervang de oppervlaktekleur door de kleurkaart
    if(extra.overlayKeuze > 0.5) {
        return vec4f(overlayKleurKeuze(in), 1.0);
    }

    return kleur * max(0.2, diffuus * schaduw);
}
