//WGSL fragment-shader voor de water-pass van de planeet.
#include "planeetStructen.wgsl"

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
};

fn berekenVervormdeNormaal(n : vec3f, hoeks : vec3f, vervorming : vec2f) -> vec3f {
    let haaks = cross(n, hoeks);
    return normalize(n + vervorming.x * hoeks + vervorming.y * haaks);
}

//Temperatuuroverlay-kleurkaart (zelfde als het land): -25 °C blauw, 0 °C groen,
//+25 °C rood, kouder dan -25 °C klemt op blauw.
fn temperatuurKleur(TC : f32) -> vec3f {
    let t = clamp((TC - 248.0) / 50.0, 0.0, 1.0);
    var kleur = mix(vec3f(0.2, 0.35, 1.0), vec3f(0.2, 0.85, 0.2), smoothstep(0.0, 0.5, t));
    kleur = mix(kleur, vec3f(1.0, 0.25, 0.1), smoothstep(0.5, 1.0, t));
    return kleur;
}

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    //Alleen tonen waar er water óf een ijsdeksel is
    if(in.waterHoogte < 0.1 && in.ijs <= 0.01) {
        discard;
    }

    let isIJs = in.ijs > 0.01;

    //Naadloze textuur-coordinaten op basis van fwidth (Tarini 2012)
    let naadloosTex = vec2f(select(in.texDraaien.z, in.texDraaien.y, fwidth(in.texDraaien.y) <= fwidth(in.texDraaien.z) + 0.000001), in.texDraaien.x);

    var vervormdN = in.normaal;

    if(length(in.snelheid) > zeerKlein) {
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
    let diffuus     = max(0.0, dot(zonView, vervormdN));

    var lichtheid = 0.0;

    if(diffuus > 0.0) {
        lichtheid = pow(max(0.0, dot(lichtSpiegel, oogRicht)), 200.0) * 0.8;
    }

    var kleur = mix(vec4f(in.kleur.xyz * max(0.2, diffuus), in.kleur.a), vec4f(vec3f(1.0), in.kleur.a), lichtheid);
    kleur *= vec4f(1.0, 1.0, 1.0, 0.85);

    //IJs: wit deksel; met temperatuurview aan toont ook ijs zijn temperatuurkleur,
    //met een dunne witte contour op de rand (fwidth) zodat je ijs toch herkent.
    let ijsRand = fwidth(select(0.0, 1.0, in.ijs > 0.01));

    if(extra.toonTemperatuur > 0.5) {
        //temperatuurkleur voor zowel water als ijs
        kleur = vec4f(temperatuurKleur(in.temperatuur), 0.85);
        if(isIJs) {
            kleur = mix(vec4f(temperatuurKleur(in.temperatuur), 0.9),
                        vec4f(1.0, 1.0, 1.0, 0.95),
                        smoothstep(0.03, 0.2, ijsRand));
        }
    }
    else if(isIJs) {
        kleur = vec4f(0.85, 0.9, 0.95, 0.95);
    }

    kleur.a = max(kleur.a, lichtheid);

    return kleur;
}
