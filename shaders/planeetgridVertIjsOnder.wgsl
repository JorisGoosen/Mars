//WGSL vertex-shader voor de ONDERKANT van de ijs-pass: het vlak op de waterspiegel
//(grond + waterSchijn) dat de ijswand aan de onderkant sluit ("andersom" getekend
//via cull Front). Gedeelde fragment-shader: planeetgridFragIjs.wgsl; die snijdt
//onder- en bovenkant op de iso-lijn ijs = miniJs (geen rok meer).
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

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    //Zelfde interpolant als de bovenkant: de fragment-shader snijdt top én bottom
    //op de iso-lijn ijs = miniJs, dus de wand sluit op de ijsrand.
    uit.ijsDikte = vakken0[ID].ijs;

    //Geen dikte en geen lift: de onderkant ligt op de waterspiegel.
    let hoogte = grondHoogte(vakken0[ID]) + vakken0[ID].waterSchijn;
    let hier = in.posV * (max(0.001, 1.0 + hoogte * extra.grondSchaal) / extra.grondMult);

    //Normaal gespiegeld: de onderzijde lijkt donker (in de schaduw).
    uit.normaal = -normalize((matrices.modelZicht * vec4f(berekenNormaal(ID, true), 0.0)).xyz);
    //Onderkant rendert op de waterspiegel, maar de schaduw-lookup spiegelt de
    //casters (bovenste oppervlak): consistent met land/water/ijs-bovenkant.
    uit.modelPos = schaduwSpiegelPos(in.posV, ID);
    uit.glPos = matrices.projectie * matrices.modelZicht * vec4f(hier, 1.0);

    return uit;
}
