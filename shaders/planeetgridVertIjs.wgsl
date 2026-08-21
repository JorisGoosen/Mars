//WGSL vertex-shader voor de BOVENKANT van de ijs-pass: tekent het ijs als een
//drijvende plaat op de waterspiegel + zijn echte dikte. De sim rekent ijs als
//"grond onder water" (vereenvoudiging), maar de weergave toont het erbovenop.
//
//De onderkant zit in planeetgridVertIjsOnder.wgsl (waterspiegel, cull Front);
//beide delen een fragment-shader (planeetgridFragIjs.wgsl). Waar het ijs naar 0
//afneemt sluiten beide vlakken vanzelf (geen zijwanden nodig); de randcel ("rok")
//wordt meegenomen zolang een buur ijs heeft.
#include "planeetDefinitiesRender.wgsl"

struct matricesDaar {
    projectie  : mat4x4f,
    modelZicht : mat4x4f,
    transInvMV : mat4x4f,
};
@group(0) @binding(1) var<uniform> matrices : matricesDaar;

struct vertexIn {
    @location(0) posV : vec3f,
    @location(1) tex   : vec2f,
};

struct naarFrag {
    @builtin(position) glPos   : vec4f,
    @location(0) normaal : vec3f,
    @location(1) randCel : f32,
    @location(2) modelPos : vec3f,
};

//Dun ijs z-fight met de grond eronder; echt ijs (dikker dan miniJs) krijgt deze
//lift op de bovenkant. De buitenste rok-cellen niet, anders sluit de rand niet.
const ijsLift = 0.01;

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    //Randcel: eigen cel of een buur heeft ijs. Zo houdt een oprukkende ijswand
    //langs de buitenrand een dichte zijkant (de bovenkant loopt af naar de
    //waterspiegel en raakt daar de onderkant).
    var randCel = vakken0[ID].ijs > miniJs;
    if(!randCel) {
        for(var b = 0u; b < vakMetas[ID].burenAantal && b < maxBuren; b = b + 1u) {
            if(vakken0[buurID(ID, b)].ijs > miniJs) { randCel = true; break; }
        }
    }
    uit.randCel = select(0.0, 1.0, randCel);

    //Lift alleen op echt ijs zodat de rok strak blijft aansluiten.
    let isEchtIjs = vakken0[ID].ijs > miniJs;
    let lift  = select(0.0, ijsLift, isEchtIjs);
    let hoogte = grondHoogte(vakken0[ID]) + vakken0[ID].waterSchijn + vakken0[ID].ijs + lift;

    let hier = in.posV * (max(0.001, 1.0 + hoogte * extra.grondSchaal) / extra.grondMult);

    //Belichting volgt de waterspiegel (glad ijsdek).
    uit.normaal = normalize((matrices.modelZicht * vec4f(berekenNormaal(ID, true), 0.0)).xyz);
    uit.modelPos = hier;
    uit.glPos = matrices.projectie * matrices.modelZicht * vec4f(hier, 1.0);

    return uit;
}
