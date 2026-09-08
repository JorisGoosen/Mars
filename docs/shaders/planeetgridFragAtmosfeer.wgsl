//WGSL fragment-shader voor de atmosfeer-pass: Rayleigh-verstrooiing als
//gloedrand rond de planeet. De kleur hangt af van de kijkhoek t.o.v. de zon
//(fasefunctie) en van dag/nacht op de schil; de alpha valt af met de
//kolomdiepte van de kijkstraal (gekoppeld aan hoogteUitstraling).
#include "planeetStructen.wgsl"

@group(0) @binding(2) var<uniform> extra : extraParameters;

struct naarFrag {
    @builtin(position) glPos  : vec4f,
    @location(0) modelPos     : vec3f,
    @location(1) schilNormaal : vec3f,
};

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    let zonModel = normalize(extra.zonPos.xyz);

    //Kijkstraal en diens dichtste nadering tot het planeetcentrum
    //(b = 0 in het midden, b = schilrand aan de buitenrand van de schijf).
    let kijkRichting = normalize(in.modelPos - extra.kijkPlek);
    let b = length(cross(extra.kijkPlek, kijkRichting));

    //Schilgrootte: rMax + een fractie (extra.atmosfeerDikte) van rMax erbovenop,
    //zo blijft hij altijd boven de planeet (ook als rMax < 1).
    let rMax = (1.0 + max(extra.maxGrondHoogte, 1.0) * extra.grondSchaal) / max(extra.grondMult, 0.0001);
    let rSchil = rMax * (1.0 + extra.atmosfeerDikte);

    //Radiale falloff op de kijkstraal-nadering b: 1 in het midden, EXACT 0 aan de
    //schilrand (b == rSchil). hoogteUitstraling als exponent: hoger = snellere (dunnere) falloff.
    let t = clamp(b / max(rSchil, 0.0001), 0.0, 1.0);
    let alpha = pow(1.0 - t, hoogteUitstraling);

    //Rayleigh-fasefunctie: strooiingshoek tussen kijkrichting en zon.
    let cosTheta = dot(kijkRichting, zonModel);
    let fase = 0.75 * (1.0 + cosTheta * cosTheta);

    //Dag/nacht op de schil: zachte terminator + een schemervloertje, zodat de gloed
    //ook op de nachtzijde zichtbaar blijft (zoals een echte atmosfeerrand).
    let dagKant = smoothstep(-0.2, 0.3, dot(in.schilNormaal, zonModel));
    let helderheid = clamp(0.30 + 0.70 * dagKant, 0.0, 1.0);

    let kleur = vec3f(0.45, 0.65, 1.1) * fase * helderheid * extra.atmosfeerSterkte;

    return vec4f(kleur, clamp(alpha, 0.0, 1.0));
}
