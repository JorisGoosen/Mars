//WGSL vertex-shader voor de BOVENKANT van de ijs-pass: tekent het ijs als een
//drijvende plaat op de waterspiegel + zijn echte dikte. De sim rekent ijs als
//"grond onder water" (vereenvoudiging), maar de weergave toont het erbovenop.
//
//De onderkant zit in planeetgridVertIjsOnder.wgsl (waterspiegel, cull Front);
//beide delen een fragment-shader (planeetgridFragIjs.wgsl) en geven de echte
//ijsdikte als interpolant door. De fragment-shader snijdt top én bottom op de
//interpolatie-iso-lijn (ijs <= miniJs weg): de wand sluit daar vanzelf en de
//ijsrand volgt de echte ijsdikte i.p.v. celranden (geen rok meer).
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
    @location(1) ijsDikte : f32,
    @location(2) modelPos : vec3f,
};

//Dun ijs z-fight met de grond eronder; echt ijs (dikker dan miniJs) krijgt deze
//lift op de bovenkant, zodat de top los blijft van de onderkant.
const ijsLift = 0.01;

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    //De echte ijsdikte als interpolant: de fragment-shader snijdt de ijsoppervlak-
    //ken op de iso-lijn ijs = miniJs, zodat de ijsrand de echte dikte volgt.
    uit.ijsDikte = vakken0[ID].ijs;

    //Lift alleen op echt ijs zodat de rand strak aansluit.
    let isEchtIjs = vakken0[ID].ijs > miniJs;
    let lift  = select(0.0, ijsLift, isEchtIjs);
    let hoogte = grondHoogte(vakken0[ID]) + vakken0[ID].waterSchijn + vakken0[ID].ijs + lift;

    let hier = in.posV * (max(0.001, 1.0 + hoogte * extra.grondSchaal) / extra.grondMult);

    //Belichting volgt de waterspiegel (glad ijsdek).
    uit.normaal = normalize((matrices.modelZicht * vec4f(berekenNormaal(ID, true), 0.0)).xyz);
    //Schaduw-lookup op de spiegelpositie (zonder de ijsLift-render-truc): de
    //schaduwkaart zelf bevat het echte ijs-top.
    uit.modelPos = schaduwSpiegelPos(in.posV, ID);
    uit.glPos = matrices.projectie * matrices.modelZicht * vec4f(hier, 1.0);

    return uit;
}
