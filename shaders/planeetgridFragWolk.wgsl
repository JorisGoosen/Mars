//WGSL fragment-shader voor de wolk-pass: zachte, door de zon belichte wolken
//met doorzichtigheid uit de bewolkingswaarde van de cel.
#include "planeetStructen.wgsl"

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
};

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    if(in.wolken < 0.003) {
        discard;
    }

    //Zon vanuit modelruimte naar de view-ruimte brengen (zelfde modelZicht als
    //de vertex gebruikte), zodat de belichting consistent is met mijnPlek.
    let zonModel = normalize(extra.zonPos.xyz);
    let zonView  = normalize((matrices.transInvMV * vec4f(zonModel, 0.0)).xyz);

    let mijnPlek   = in.pos.xyz / max(in.pos.w, 0.0001);
    let lichtRicht = normalize(zonView - normalize(mijnPlek));
    let diffuus    = max(0.0, dot(lichtRicht, in.normaal));

    //Lichte golfjes voor wat volume/wisp in de wolken
    let wolkenWaas = in.wolken * (0.8 + 0.2 * sin(in.pos.x * 4.0 + in.pos.y * 3.0 + in.pos.z * 2.0));

    let bewolkt = smoothstep(0.003, 0.06, in.wolken);
    var a = bewolkt * clamp(wolkenWaas * 14.0, 0.0, 1.0);
    a = clamp(a, 0.0, 0.88);

    let helderheid = clamp(0.25 + 0.75 * diffuus, 0.0, 1.0);
    let kleur = mix(vec3f(0.6, 0.63, 0.70), vec3f(1.0, 0.99, 0.96), clamp(diffuus * 1.5, 0.0, 1.0));

    return vec4f(kleur * helderheid, a);
}
