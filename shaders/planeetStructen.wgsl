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
    pijpen      : array<f32, maxBuren>,
    snelheid    : vec2f,
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
    windAs      : vec4f, //rotatie-As van de circulatiecellen (xyz) + windsterkte (w)
    zonRicht    : vec4f, //zonrichting voor dag/nacht-verdamping (w ongebruikt)
    condenseer  : vec4f, //(basisVerzadiging, hoogteKoel, neerslagFactor, orografieFactor)
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
};