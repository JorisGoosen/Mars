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
const oplosheid       = 0.05;  //hoe snel water materiaal oplost/erodeert (10x zo traag)
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

//Zand-rusthelling (angle of repose, zie grondGelijkmaker.comp): zand zakt naar een
//stabiele helling. zandRepose = maximale hoogte-drempel (in dezelfde eenheden als
//grondHoogte) voordat zand naar een lagere buur mag 'vallen'; zandZakhoek is de
//fractie van het overschot die per ronde daadwerkelijk verplaatst wordt.
const zandRepose  = 2.0;
const zandZakhoek = 1.0 / 6.0;

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

//IJsvorming (zie waterDruk.comp): onder 273 K bevriest water tot ijs, daarboven
//dooit het terug. Hoe kouder, hoe sneller. IJs telt als grond voor de stroming.
const vriespuntK    = 273.0;  //0 °C in Kelvin
const ijsTempo      = 0.005;  //fractie water dat per ronde per Kelvin onder het vriespunt bevriest
const miniJs        = 0.01;   //onder deze ijsdikte heet een cel ijsloos (render-drempel)

//Leven & temperatuur (zie waterDruk.comp): leven groeit alleen boven 0 °C en sterft
//bij vorst. De dood begint traag rond -20 °C en wordt steil (kwadratisch) snel bij
//-60 °C en kouder. De dood is uniform: hij remt elke levenscel, nat of droog.
const levenBevriesK   = 273.0;   //0 °C: boven dit punt mag leven pas groeien
const levenGroeiBand  = 6.0;     //K boven vriespunt waarover de groei naar vol oploopt
const levenKoudBegin  = 253.15;  //-20 °C: de dood begint hier langzaam
const levenKoudSnel   = 213.15;  //-60 °C: hier doodt het heel snel
const levenKoudTempo  = 0.5;     //fractie leven die per ronde sterft bij -60 °C

const maxBuren = 6u;

//Atmosferische dynamica (zie luchtStroming.comp): barotrope-achtige circulatie.
//De eigenlijke sterkte van de effecten komt uit reken.atmosfeer (zonkracht,
//rotatie-omega, wrijving, diffusie); onderstaande zijn de fysische constanten.
const luchtBaseTemp   = 250.0;  //start/referentietemperatuur (K) van de lucht
const lapseKoeling    = 15.0;   //gematigde koeling per genormaliseerde hoogtelaag
const opnameTempo     = 0.06;   //hoe snel zonne-energie de lucht opwarmt (zwakker: minder zonne-inkomende warmte)
const stralingKracht  = 0.015;  //hoe snel de planeet afkoelt naar het omringende (lager = betere warmtebehoud)
const tempDiffusie    = 0.15;   //hoe snel de temperatuur zich over de buren verdeelt
const ruimteK         = 180.0;  //effectieve hemeltemperatuur (K) zonder broeikas
const broeikasK       = 96.0;   //CO2-groeikaseffect: verhoogt de effectieve hemel-T
const drukKracht     = 0.8;    //drukgradiëntkracht-coëfficiënt (wind versnelling)
const drukRelax      = 0.05;   //hoe snel de druk naar het thermische evenwicht zakt
const drukDiffusie   = 0.04;   //extra gladstrijken van de druk (klein = scherpere banden)
const minLuchtdruk   = 0.2;    //klemmen op de druk zodat P>0 blijft
const maxLuchtdruk   = 5.0;

//Albedo's van het oppervlak/weer (moduleren hoe veel zonnestraling wordt geabsorbeerd).
const albedoIJs     = 0.60;
const albedoWolken  = 0.55;
const albedoWater   = 0.08;
const albedoGrond   = 0.30;
const albedoBegroei = 0.18;   //donkerder door leven (groen)
const wolkIsolatie  = 0.55;   //hoe sterk het wolkendek de uitstraling tegenhoudt

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
    fasen       : vec4f, //(verwarmtijdconstante, maxGrondHoogte, grondMult, ongebruikt)
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
    toonTemperatuur : f32, //1 = temperatuuroverlay aan (toets T)
};