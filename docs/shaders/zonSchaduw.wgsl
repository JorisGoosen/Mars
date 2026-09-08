//Gedeelde hulpjes voor de schaduwkaart: de zon is een richtingslicht op oneindig
//en de schaduwkaart is een orthografische dieptekaart loodrecht op de zonrichting.
//De projectie wordt hier ANALYTISCH uitgerekend (uit de zonrichting alleen), zodat
//de schaduw-pass (vertex), de fragment-lookup (PCF) en de reken-shaders (zonlicht-
//benadering) exact dezelfde afbeelding gebruiken — zonder matrices of uniforms.
//
//De straal R van het orthografische kader volgt uit de maximale render-straal van
//de planeet: (1 + maxGrond · grondSchaal) / grondMult, met een kleine marge zodat
//de rand van de planeet net buiten de kaart valt (alles buiten de kaart = verlicht).

const schaduwBias = 0.0002;   //diepte-marge tegen zelf-schaduw (acne), in genormaliseerde diepte
const schaduwZacht = 0.01;    //penumbra-verzachting: over dit diepteverschil (genormaliseerd, ≈ 4 hoogte-eenheden)
                              //loopt de schaduwrand gradueel van vol licht naar volle schaduw
const schaduwEpsilon = 0.005; //caster-oppervlak wordt langs de zon teruggeduwd (render-eenheden),
                              //zodat een oppervlak nooit zijn eigen diepte bemonstert; echte
                              //occluders dieper dan epsilon werpen nog steeds schaduw.
const schaduwMarge = 1.30;    //marge rond de planeet voor het orthografische kader: ruime
                              //hoofdruimte, want het bovenste oppervlak (water/ijs) kan boven
                              //hoogsteGrond() uitsteken en moet binnen de kaart blijven

//Deterministische orthonormale basis (u, v, zon) loodrecht op de zonrichting.
fn zonBasis(zon : vec3f) -> mat3x3f {
    var up = vec3f(0.0, 1.0, 0.0);
    if(abs(dot(zon, up)) > 0.99) {
        up = vec3f(0.0, 0.0, 1.0);
    }
    let u = normalize(cross(up, zon));
    let v = cross(zon, u);
    return mat3x3f(u, v, zon);
}

//Straal van het orthografische kader: dekt de hele planeet (max. render-straal + marge).
fn zonStraal(grondMult : f32, grondSchaal : f32, maxGrond : f32) -> f32 {
    return schaduwMarge * (1.0 + maxGrond * grondSchaal) / max(grondMult, 0.001);
}

//Projecteert een modelpositie in de orthografische schaduwruimte van de zon:
//xy in [-1, 1] (direct als NDC/clip te gebruiken), z in [0, 1] met 0 = dichtst bij
//de zon (de dagzijde) en 1 = het verste punt van de planeet.
fn zonProjectie(p : vec3f, zon : vec3f, straal : f32) -> vec3f {
    let b = zonBasis(zon);
    return vec3f(
        dot(p, b[0]) / straal,
        dot(p, b[1]) / straal,
        0.5 - dot(p, zon) / (2.0 * straal)
    );
}

//Van schaduwruimte naar textuur-coordinaten [0, 1]. LET OP: NDC y=+1 landt in
//texel-rij 0 (boven), dus de v-as moet omklappen t.o.v. pr.y — anders bemonstert
//elke lookup de noord-zuid-spiegelbeeldkant van de kaart (schaduw op de verkeerde
//helft van de planeet).
fn zonSchaduwUV(pr : vec3f) -> vec2f {
    return vec2f(pr.x * 0.5 + 0.5, 0.5 - pr.y * 0.5);
}

//1 als het punt buiten het orthografische kader valt (daar is geen schaduw).
fn zonBinnenKaart(pr : vec3f) -> bool {
    return abs(pr.x) < 1.0 && abs(pr.y) < 1.0;
}
