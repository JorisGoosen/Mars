//Gedeelde structs en constanten voor de planeet-simulatie en -weergave (Mars)
//De structs moeten qua gmlayout exact matchen met de C++-structs in planeet.h

const gsZand   = 0u;
const gsGrond  = 1u; //ook wel humus of rijke grond
const gsRots   = 2u;
const gsKlei   = 3u;
const gsIjs    = 4u;
const gsLoess  = 5u;

const tijdVerschil    = 0.1;
const zwaartekracht   = 0.8;
const pijpDoorsnee    = 0.5;
const pijpLengte      = 1.0;
const oplosheid       = 0.5;
const bezinkheid      = 0.15;
const droesemheid     = 0.45;
const vertrager       = 1.0 / 3.0;
const zeerKlein       = 0.0001;
const minGrondHoogte  = 10.0;
const maxGrondHoogte  = 200.0;
const toonSediment    = 0.003; //boven deze waarde wordt grondSoort zand
const minWaterSed     = 0.1;

//Materiaal-afhankelijke erosiesnelheden (zie waterDruk.comp).
//Zand/sediment dient als snelle, makkelijk verplaatste deklaag; de diepste
//ondergrond (rots) erodeert rotsVertragingKeer langzamer dan zand.
const zandErosie   = 1.0;
const rotsErosie   = 0.01; //100x langzamer dan zand
const hellingKracht = 5.0;   //hoe sterk de helling de draagcapaciteit verhoogt
const maxDichtheid = 0.5;    //max. zwevend sediment t.o.v. de waterhoogte

//Extreem hoge kleppen: puur bescherming tegen Niet-eindige waarden en
//f32-overflow, ver boven elk reëel fysisch niveau. De pijpen kunnen door de
//K-factor (water/dt, dt=0.1) tot ~10x de waterhoogte oplopen.
const maxWaterHoogte = 1.0e12;
const maxPijp        = 1.0e13;
const maxSnelheid    = 1.0e13;
const maxDroesem     = 1.0e12;
const maxLuchtVocht  = 1.0e12;

//Waterkringloop (zie waterLucht.comp en waterDruk.comp): bodemvocht is de
//grondwatervoorraad (vult via infiltratie, voedt het leven), luchtvocht is de
//atmosferische vochtigheid (advectie door de wind, regent uit boven verzadiging
//en op bergflanken).
const veldCapaciteit    = 0.5; //max. bodemvocht dat een cel kan vasthouden
const infiltratie       = 0.03; //fractie staand water dat per ronde de grond in zakt
const evapotranspiratie = 0.05; //hoe snel vochtige grond verdroogt naar droge lucht
const maxRegenPerRonde  = 0.02; //hoogstens zoveel diepte regen per ronde (piekbegrenzer)

const maxBuren = 6u;

//Atmosferische dynamica (zie luchtStroming.comp): barotrope-achtige circulatie.
//De eigenlijke sterkte van de effecten komt uit reken.atmosfeer (zonkracht,
//rotatie-omega, wrijving, diffusie); onderstaande zijn de fysische constanten.
const luchtBaseTemp   = 250.0;  //referentietemperatuur (K)
const lapseKoeling    = 35.0;   //koeling per genormaliseerde hoogtelaag
const evenaarWarm     = 45.0;   //K die de evenaar warmer is dan de polen
const drukKracht     = 0.8;    //drukgradiëntkracht-coëfficiënt (wind versnelling)
const drukRelax      = 0.05;   //hoe snel de druk naar het thermische evenwicht zakt
const drukDiffusie   = 0.04;   //extra gladstrijken van de druk (klein = scherpere banden)
const minLuchtdruk   = 0.2;    //klemmen op de druk zodat P>0 blijft
const maxLuchtdruk   = 5.0;

//Twee-fasen vocht (zie waterLucht.comp): damp <-> wolk <-> regen.
//luchtVocht is de damp (capaciteit volgt de temperatuur), wolken is het
//gecondenseerde water. Regen valt uitsluitend uit wolken.
const condensTempo   = 0.5;    //fractie oververzadigde damp die per ronde condenseert
const wolkVerdamp    = 0.05;   //fractie wolkwater dat per ronde in droge lucht terugverdampf
const regenTempo     = 0.02;   //fractie wolkwater dat per ronde als regen uitvalt
const minWolk        = 0.0005; //onder deze waarde heet een cel wolkloos
const wolkDraagKracht = 0.02;  //max. wolkwater per eenheid; daarboven regent het uit

struct vak {
    grondSoort  : i32,
    grondHoogte : f32,
    rotsHoogte  : f32,
    waterHoogte : f32,
    waterSchijn : f32,
    bodemVocht  : f32,
    ijs         : f32,
    leven       : f32,
    droesem     : f32,
    luchtVocht  : f32,
    temperatuur : f32,
    luchtdruk   : f32,
    wolken      : f32,
    padLucht    : f32,
    pijpen      : array<f32, maxBuren>,
    snelheid    : vec2f,
    wind        : vec2f,
    plek        : vec2f,
};

struct vakMeta {
    normaal     : vec4f,
    buurRicht   : array<vec2f, maxBuren>,
    buren       : array<u32, maxBuren>,
    burenAantal : u32,
    opvulling   : u32,
};

//Parameters die de reken-shaders krijgen (bind-groep 0, binding 3)
struct rekenParameters {
    grondSchaal : f32,
    verdamping  : f32,      //basale verdamping: waterHoogte -> luchtVocht (zon-afhankelijk)
    erosie      : f32, //0 = terrein verandert niet (erosie uit), 1 = normaal
    levenAan    : f32, //0 = geen leven (plantengroei uit)
    atmosfeer   : vec4f, //(zonkracht, rotatieOmega, wrijving, diffusie)
    zonRicht    : vec4f, //zonrichting (dagzijde; vast in modelruimte, de planeet draait)
    condenseer  : vec4f, //(basisVerzadiging, hoogteKoel, neerslagFactor, orografieFactor)
    fasen       : vec4f, //(verwarmtijdconstante, wolkEvapSchaal, ongebruikt, ongebruikt)
};

//Parameters voor de weergave-shaders (bind-groep 0, binding 2)
struct extraParameters {
    grondMult   : f32,
    grondSchaal : f32,
    _padA       : f32,
    _padB       : f32,
    kijkPlek    : vec3f,
    _padC       : f32,
    zonPos      : vec3f,
    _padD       : f32,
    maxGrondHoogte : f32, //hoogste terreinpunt (bepaald bij het laden); basis voor het wolkendek
    _padE       : f32,    //(align-vulling om het uniform netjes op de glsl-struct te laten passen)
};