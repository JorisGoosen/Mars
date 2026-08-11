//WGSL fragment-shader voor de grond-pass van de planeet.
#include "planeetStructen.wgsl"

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
};

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    //Naadloze textuur-coordinaten op basis van fwidth (Tarini 2012)
    let naadloosTex = vec2f(select(in.texDraaien.z, in.texDraaien.y, fwidth(in.texDraaien.y) <= fwidth(in.texDraaien.z) + 0.000001), in.texDraaien.x);
    let marsHoogte = textureSampleLevel(marsHoogteTex, marsHoogteSmp, naadloosTex, 0.0).r;

    let mijnPlek  = in.pos.xyz / max(in.pos.w, 0.0001);
    let lichtRicht = normalize(extra.zonPos - mijnPlek);
    let diffuus = max(0.0, dot(lichtRicht, in.normaal));

    let kleur = mix(in.kleur * clamp(marsHoogte * 3.0, 0.35, 1.0), vec4f(0.0, 0.35, 0.0, 1.0), clamp(in.leven, 0.0, 1.0)) * max(0.2, diffuus);

    return kleur;
}