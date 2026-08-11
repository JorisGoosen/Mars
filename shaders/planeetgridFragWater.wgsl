//WGSL fragment-shader voor de water-pass van de planeet.
#include "planeetStructen.wgsl"

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
};

fn berekenVervormdeNormaal(n : vec3f, hoeks : vec3f, vervorming : vec2f) -> vec3f {
    let haaks = cross(n, hoeks);
    return normalize(n + vervorming.x * hoeks + vervorming.y * haaks);
}

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    if(in.waterHoogte < 0.1) {
        discard;
    }

    //Naadloze textuur-coordinaten op basis van fwidth (Tarini 2012)
    let naadloosTex = vec2f(select(in.texDraaien.z, in.texDraaien.y, fwidth(in.texDraaien.y) <= fwidth(in.texDraaien.z) + 0.000001), in.texDraaien.x);

    var vervormdN = in.normaal;

    if(length(in.snelheid) > zeerKlein) {
        let coral = naadloosTex + in.plek * 0.001;
        let vervorming = textureSampleLevel(waterBumpTex, waterBumpSmp, vec2f(sin(coral.x * 3.142 * 2.0) * 10.0, coral.y * 10.0), 0.0).xy;
        vervormdN = berekenVervormdeNormaal(in.normaal, in.hoeks, vervorming);
    }

    let mijnPlek    = in.pos.xyz / max(in.pos.w, 0.0001);
    let lichtRicht  = normalize(extra.zonPos - mijnPlek);
    let lichtSpiegel = reflect(lichtRicht, vervormdN);
    let diffuus     = max(0.0, dot(lichtRicht, vervormdN));

    var lichtheid = 0.0;

    if(diffuus > 0.0 && dot(lichtRicht, normalize(mijnPlek)) < 0.0) {
        lichtheid = pow(max(0.0, dot(lichtSpiegel, normalize(extra.kijkPlek - mijnPlek))), 200.0) * 0.8;
    }

    var kleur = mix(vec4f(in.kleur.xyz * max(0.2, diffuus), in.kleur.a), vec4f(vec3f(1.0), in.kleur.a), lichtheid);
    kleur *= vec4f(1.0, 1.0, 1.0, 0.85);
    kleur.a = max(kleur.a, lichtheid);

    return kleur;
}