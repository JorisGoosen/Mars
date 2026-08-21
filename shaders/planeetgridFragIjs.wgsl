//WGSL fragment-shader voor de ijs-pass (gedeeld door onder- en bovenkant):
//wit ijsdeksel, belicht door de zon en met terreinschaduw, dekkend. De vertex-
//shaders geven de echte ijsdikte als interpolant door; hier wordt het ijs
//gesneden op de iso-lijn ijs = miniJs. Daardoor volgt de ijsrand de echte
//dikte i.p.v. celranden en sluiten top/bottom vanzelf (geen rok meer).
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
    @builtin(position) glPos   : vec4f,
    @location(0) normaal : vec3f,
    @location(1) ijsDikte : f32,
    @location(2) modelPos : vec3f,
};

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    if(in.ijsDikte <= miniJs) {
        discard;
    }

    //Richtingslicht van de zon (modelruimte -> view-ruimte)
    let zonModel = normalize(extra.zonPos.xyz);
    let zonView  = normalize((matrices.transInvMV * vec4f(zonModel, 0.0)).xyz);
    var diffuus  = max(0.0, dot(zonView, in.normaal));

    //Terreinschaduw op het ijs (zelfde lookup als land/water).
    if(extra.schaduwAan > 0.5 && diffuus > 0.0) {
        let straal = zonStraal(extra.grondMult, extra.grondSchaal, extra.maxGrondHoogte);
        diffuus *= zonSchaduwFactor(zonProjectie(in.modelPos, zonModel, straal), extra.schaduwGrootte, diffuus);
    }

    let kleur = vec3f(0.85, 0.9, 0.95) * max(0.2, diffuus);
    return vec4f(kleur, 1.0);
}
