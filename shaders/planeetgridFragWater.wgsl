//WGSL fragment-shader voor de water-pass van de planeet.
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
@group(1) @binding(0) var waterBumpTex : texture_2d<f32>;
@group(1) @binding(1) var waterBumpSmp : sampler;

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
    @location(11) ijs           : f32,
    @location(12) wind          : vec2f,
    @location(13) luchtdruk     : f32,
    @location(14) modelPos      : vec3f,
    @location(15) diepWater     : f32,
};

fn berekenVervormdeNormaal(n : vec3f, hoeks : vec3f, vervorming : vec2f) -> vec3f {
    let haaks = cross(n, hoeks);
    return normalize(n + vervorming.x * hoeks + vervorming.y * haaks);
}

//Temperatuuroverlay-kleurkaart (zelfde als het land): -35 °C blauw, 0 °C groen,
//+35 °C rood, kouder dan -35 °C klemt op blauw.
fn temperatuurKleur(TC : f32) -> vec3f {
    let t = clamp((TC - 238.0) / 70.0, 0.0, 1.0);
    var kleur = mix(vec3f(0.2, 0.35, 1.0), vec3f(0.2, 0.85, 0.2), smoothstep(0.0, 0.5, t));
    kleur = mix(kleur, vec3f(1.0, 0.25, 0.1), smoothstep(0.5, 1.0, t));
    return kleur;
}

//Windoverlay (toets V, zelfde als land): rood = oost-west, groen = noord-zuid,
//blauw = genormaliseerde luchtdruk.
fn windKleur(W : vec2f, P : f32) -> vec3f {
    let wScale = 2.0; //maxWindsnel (zie luchtStroming.comp)
    let r = clamp(W.x / wScale * 0.5 + 0.5, 0.0, 1.0);
    let g = clamp(W.y / wScale * 0.5 + 0.5, 0.0, 1.0);
    let b = clamp((P - 0.2) / 4.8, 0.0, 1.0);
    return vec3f(r, g, b);
}

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    //Alleen tonen waar water is; het ijs zit in zijn eigen pass (getekend bovenop).
    if(in.waterHoogte < 0.1) {
        discard;
    }

    //Naadloze textuur-coordinaten op basis van fwidth (Tarini 2012)
    let naadloosTex = vec2f(select(in.texDraaien.z, in.texDraaien.y, fwidth(in.texDraaien.y) <= fwidth(in.texDraaien.z) + 0.000001), in.texDraaien.x);

    var vervormdN = in.normaal;

    //Bump-golfjes alleen op echt vloeibaar water: op ijs en dunne films zou de
    //bump de normaal scheeftrekken en daarmee de belichting (en schaduw) kapotmaken.
    if(in.diepWater > 0.5 && length(in.snelheid) > zeerKlein) {
        let coral = naadloosTex + in.plek * 0.001;
        let vervorming = textureSampleLevel(waterBumpTex, waterBumpSmp, vec2f(sin(coral.x * 3.142 * 2.0) * 10.0, coral.y * 10.0), 0.0).xy;
        vervormdN = berekenVervormdeNormaal(in.normaal, in.hoeks, vervorming);
    }

    //Richtingslicht van de zon (modelruimte -> view-ruimte) + oog richting de camera
    let zonModel = normalize(extra.zonPos.xyz);
    let zonView  = normalize((matrices.transInvMV * vec4f(zonModel, 0.0)).xyz);

    let mijnPlek = in.pos.xyz / max(in.pos.w, 0.0001);
    let oogRicht = normalize(-mijnPlek);           //camera zit in view-ruimte in de oorsprong
    let lichtSpiegel = reflect(zonView, vervormdN);
    var diffuus     = max(0.0, dot(zonView, vervormdN));
    var schaduw     = 1.0;

    //Terrein-schaduw: bergen gooien hun schaduw ook op het water/ijs
    if(extra.schaduwAan > 0.5 && diffuus > 0.0) {
        let straal = zonStraal(extra.grondMult, extra.grondSchaal, extra.maxGrondHoogte);
        schaduw = zonSchaduwFactor(zonProjectie(in.modelPos, zonModel, straal), extra.schaduwGrootte);
        diffuus *= schaduw;
    }

    var lichtheid = 0.0;

    if(diffuus > 0.0) {
        //De spiegel wordt óók gedimd met het schaduwbedrag: in halfschaduw niet
        //vol stonderschaduw, in volle schaduw volledig uit.
        lichtheid = pow(max(0.0, dot(lichtSpiegel, oogRicht)), 150.0) * schaduw * extra.waterReflectie;
        lichtheid = min(lichtheid, 1.0);
    }

    //Basiswaterkleur: blauw, maar mengt naar modderbruin naarmate er droesem in zit.
    let modder = clamp(in.kleur.r, 0.0, 1.0);
    let waterKleur = mix(vec3f(0.05, 0.15, 0.6), vec3f(0.45, 0.32, 0.2), modder);
    //Swirflow-wit (uit kleur.g) wordt eerst in de kleur gemengd en daarna pas door de
    //diff belicht/verduisterd: het wit volgt dus óók de nachtzijde (lichtval).
    var waterRgb = mix(waterKleur, vec3f(1.0), in.kleur.g) * max(0.15, diffuus);
    waterRgb = mix(waterRgb, vec3f(1.0), lichtheid);

    //Fresnel-rand: schuin gekeken water (randen/silhouet) reflecteert extra een
    //lichtblauw-witte 'hemel'-gloed, meeschalend met het daglicht en de slider.
    //kleur.a blijft erbuiten, anders wordt de planeetrand opaak.
    let fresnel = pow(1.0 - clamp(dot(oogRicht, vervormdN), 0.0, 1.0), 1.5);
    waterRgb = mix(waterRgb, vec3f(0.85, 0.93, 1.0), fresnel * max(0.15, diffuus) * 0.30 * extra.waterReflectie);

    var kleur = vec4f(waterRgb, in.kleur.a);

    kleur.a = max(kleur.a, lichtheid);

    return kleur;
}
