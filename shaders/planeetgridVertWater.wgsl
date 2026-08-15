//WGSL vertex-shader voor de water-pass van de planeet.
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
};

const waterSchaler = 2.0;

@vertex
fn main(in : vertexIn, @builtin(vertex_index) vertexIndex : u32) -> naarFrag {
    var uit : naarFrag;
    let ID = vertexIndex;

    uit.texDraaien = vec3f(in.tex.y, fract(in.tex.x), fract(in.tex.x + 0.5) - 0.5);
    uit.grondHoogte = vakken0[ID].grondHoogte;

    let lokaalWater = select(vakken0[ID].waterHoogte / waterSchaler, 1.0, vakken0[ID].waterHoogte > waterSchaler);
    let droesemVerhouding = select(vakken0[ID].droesem / max(droesemheid * vakken0[ID].waterHoogte, zeerKlein), 0.0, vakken0[ID].waterHoogte <= zeerKlein);
    uit.kleur = vec4f(droesemVerhouding, length(vakken0[ID].snelheid) * vertrager, 0.4, 0.3 + (lokaalWater * 0.7));

    uit.waterHoogte = vakken0[ID].waterSchijn;
    uit.snelheid = vakken0[ID].snelheid;
    uit.leven = vakken0[ID].leven;
    uit.plek = vakken0[ID].plek - floor(vakken0[ID].plek);
    uit.temperatuur = vakken0[ID].temperatuur;
    uit.ijs = vakken0[ID].ijs;
    uit.wind      = vakken0[ID].wind;
    uit.luchtdruk = vakken0[ID].luchtdruk;

    //het water ligt boven op de grond: schijnbare waterhoogte telt mee
    let hier = in.posV * (vakHoogte(ID, true) / extra.grondMult);

    //Voor de schaduw-lookup: water wordt op terreinhoogte bemonsterd (zelfde als het
    //land eronder, zodat de schaduwkaart — die geen water bevat — geen verschoven
    //rug-silhouetten als slingers over het wateroppervlak projecteert). IJs wordt op
    //de bovenkant van het ijsdek bemonsterd, omdat het ijsdek wél in de kaart zit.
    let ijsDek = select(0.0, vakken0[ID].ijs, vakken0[ID].ijs > 0.01);
    let kijkHoogte = vakken0[ID].grondHoogte + ijsDek;
    //Belichtingsnormaal: een dunne waterfilm of een ijsdek volgt voor de belichting
    //het terrein — de waterSchijn-gradiënten van zo'n vel geven anders lelijke
    //facet-vlakken die op geprojecteerde schaduwen lijken. Alleen echt diep water
    //krijgt zijn eigen (vlakke) oppervlakte-normaal.
    let diepWater = vakken0[ID].waterSchijn > 1.0 && vakken0[ID].ijs <= 0.01;

    uit.modelPos = in.posV * (max(0.001, 1.0 + kijkHoogte * extra.grondSchaal) / extra.grondMult);
    uit.normaal = normalize((matrices.modelZicht * vec4f(berekenNormaal(ID, diepWater), 0.0)).xyz);
    uit.hoeks = cross(normalize((matrices.modelZicht * vec4f(vakHoogteNormaal(buurID(ID, 0u), diepWater), 0.0)).xyz), uit.normaal);
    uit.pos = matrices.modelZicht * vec4f(hier, 1.0);
    uit.glPos = matrices.projectie * uit.pos;

    return uit;
}