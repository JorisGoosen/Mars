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
const oplosheid       = 0.70;  //hoe snel water materiaal oplost/erodeert
const bezinkheid      = 0.01;  //hoe snel materiaal weer bezinkt (sedimentatie; was 0.10)
const droesemheid     = 0.5;
const vertrager       = 0.05;  //vertraging van het eroderen: schaalt de draagcapaciteit omlaag (rustige erosie)
const zeerKlein       = 0.0001;
const minGrondHoogte  = 10.0;
const maxGrondHoogte  = 200.0;
const toonSediment    = 0.003; //boven deze waarde wordt grondSoort zand
const minWaterSed     = 0.01;  //onder deze waterdiepte erodeert een cel niet meer (lager = ook ondiepe rivier/overland-stroming schuurt het terrein uit)

//Materiaal-afhankelijke erosiesnelheden (zie waterDruk.comp).
//Zand/sediment dient als snelle, makkelijk verplaatste deklaag en vangt de
//erosievraag eerst op; de ondergrond (rots) wordt pas geraakt zodra de vraag
//groter is dan de zandvoorraad, en erodeert dan veel langzamer.
const zandErosie   = 0.1;
const rotsErosie   = 0.01; //10x langzamer dan zand (was 20x)
const hellingKracht = 0.5;    //hoe sterk de helling de draagcapaciteit verhoogt
const maxDichtheid = 1.0;    //max. zwevend sediment t.o.v. de waterhoogte

//Zand-rusthelling (angle of repose, toegepast in waterDruk.comp): zand zakt naar een
//stabiele helling. zandRepose = maximale hoogte-drempel (in dezelfde eenheden als de
//terreinhoogte) voordat zand naar een lagere buur mag 'vallen'; zandZakhoek is de
//fractie van het overschot die per ronde daadwerkelijk verplaatst wordt.
const zandRepose  = 0.8;
const zandZakhoek = 1.0 / 20.0;

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
const veldCapaciteit    = 1.0; //max. bodemvocht dat een cel kan vasthouden
const bodemDiffusie     = 0.5; //lichter bodemvocht verspreidt zich wat door de grond
const infiltratie       = 0.3; //fractie staand water dat per ronde de grond in zakt
const evapotranspiratie = 0.001;   //hoe snel vochtige grond verdroogt naar droge lucht (rechtstreeks * verdamping; laag: grond houdt vocht vast, dáárvoor is er transpiratie via leven)
const levensDamp        = 0.02;  //hoeveel bodemvocht een cel MET leven per ronde opneemt en als damp afgeeft (20x t.o.v. 0.001: leven is de actieve waterpomp)
const maxWaterBergtop   = 0.1;  //max. waterlaag op een piek boven het wolkendek (waterplafond)

//IJsvorming (zie waterDruk.comp): onder 273 K bevriest water tot ijs, daarboven
//dooit het terug. Hoe kouder, hoe sneller. IJs telt als grond voor de stroming.
const vriespuntK    = 273.0;  //0 °C in Kelvin
const ijsTempo      = 0.0005; //fractie water/ijs dat per ronde per Kelvin onder/boven het vriespunt bevriest/dooit
const miniJs        = 0.01;   //onder deze ijsdikte heet een cel ijsloos (render-drempel)

//Leven & temperatuur (zie waterDruk.comp): leven groeit alleen boven 0 °C en sterft
//bij vorst. De dood begint traag rond -20 °C en wordt steil (kwadratisch) snel bij
//-60 °C en kouder. De dood is uniform: hij remt elke levenscel, nat of droog.
const levenBevriesK   = 273.0;   //0 °C: boven dit punt mag leven pas groeien
const levenGroeiBand  = 6.0;     //K boven vriespunt waarover de groei naar vol oploopt
const levenKoudBegin  = 253.15;  //-20 °C: de dood begint hier langzaam
const levenKoudSnel   = 213.15;  //-60 °C: hier doodt het heel snel
const levenKoudTempo  = 0.5;     //fractie leven die per ronde sterft bij -60 °C
const levenDroogTempo = 0.0005;  //fractie leven dat per ronde afsterft bij volkomen droogte (taai: geen plotseling verdwijnen)
const levenVerwelk    = 0.25;    //verwelkdrempel: pas onder deze vochtmaat doodt droogte (daarboven overleeft het, maar groeit het niet)
const levenMax        = 1.0;     //verzadigingsgrens: leven benadert dit asymptotisch (volop vegetatie)

const maxBuren = 6u;

//Atmosferische dynamica (zie luchtStroming.comp): barotrope-achtige circulatie.
//De eigenlijke sterkte van de effecten komt uit reken.atmosfeer (zonkracht,
//rotatie-omega, wrijving, diffusie); onderstaande zijn de fysische constanten.
const luchtBaseTemp   = 250.0;  //start/referentietemperatuur (K) van de lucht
const lapseKoeling    = 30.0;   //gematigde koeling per genormaliseerde hoogtelaag
const opnameTempo     = 0.30;   //hoe snel zonne-energie de lucht opwarmt
const stralingKracht  = 0.05;  //hoe snel de planeet afkoelt naar het omringende (uitstraling)
const tempDiffusie    = 0.025; //temperatuur gladstrijken (stabiel: monotone limiter vangt clusters op)
const ruimteK         = 180.0;  //effectieve hemeltemperatuur (K) zonder broeikas
const broeikasK       = 96.0;   //CO2-groeikaseffect: verhoogt de effectieve hemel-T
const drukKracht     = 0.06;    //drukgradiëntkracht-coëfficiënt (wind versnelling, met ware gradient)
const drukRelax      = 0.5;   //hoe snel de druk naar het thermische evenwicht zakt
const drukDiffusie   = 0.15; //zachte gladstrijking van de druk: neem de grid-schaal P-ruis weg, maar laat de grote dag/nacht- en poolgradaciënt staan (diffusie is schaalselectief)
const drukTempKoppel = 0.10;    //ideaalgas-koppeling: P reageert (anti-proportioneel) op de absolute T rond drukTempRef — de dag/nacht-golf en de evenaar→pool-gradaciënt worden zo échte drukgradiënten i.p.v. dat alleen lokale T-afwijking telt (0.14 gaf een meridionale hitte-export die de evenaar niet meer liet smelten: sneeuwbal-instabiliteit)
const drukTempRef    = 271.0;   //referentie-T = planeetklimaatgemiddelde (gemeten rond 265-270 K); overdrijven drijft P tegen de klemmen
const drukReferent   = 2.5;    //vast planeetniveau waar de druk zacht aan verankerd wordt (zonder reductie over de hele planeet): eerst de thermische koppeling anders de basisschaal kan laten wegdrijven naar de klemmen
const drukAnker      = 0.012;   //langzame terugtrek naar drukReferent (klimaattschaal, de dag/nacht- en poolstructuur rijdt er ongehinderd bovenop)
const rotatieWind    = 3.0;  //vaste zonale (oostwaartse) basiswind evenaar-sterk, polen 0 (vertegenwoordigt planeetrotatie)
const minLuchtdruk   = 0.2;    //klemmen op de druk zodat P>0 blijft
const maxLuchtdruk   = 5.0;

//Advectie-snelheidskoppeling: hoe ver een luchtpakket per ronde met de wind
//opschuift (in cel-eenheden). Gedeeld door luchtStroming (T/P) én waterLucht
//(vocht/wolken), zodat alle atmosferische grootheden als één pakket even hard
//meereizen. afstand = |wind| * tijdVerschil * advectieSnelheid, geclipt op 12.
const advectieSnelheid = 4.0;

//Albedo's van het oppervlak/weer (moduleren hoe veel zonnestraling wordt geabsorbeerd).
const albedoIJs     = 0.52;   //(0.60 was een sneeuwbal-val: met de coherente meridionale hitte-export bleef het ijs staan en koelde de planeet uit; iets lager houdt het ijs in de warme tak)
const albedoWolken  = 0.55;
const albedoWater   = 0.08;
const albedoGrond   = 0.30;
const albedoBegroei = 0.16;   //leven: tussen water (0.08) en grond (0.30) in, duidelijk anders dan rots
const wolkIsolatie  = 0.45;   //hoe sterk het wolkendek de uitstraling tegenhoudt (0.55 gaf een positieve terugkoppeling: dikke wolken isoleerden hete cellen, die werden heter, hielden meer damp vast en stapelden nog meer wolk — extreem-hete ophopingspunten)

//Thermische traagheid (warmtecapaciteit) per oppervlaktetype: boven water/ijs/natte
//bodem reageert de luchttemperatuur trager op zon en nachtelijke uitstraling dan
//boven kale grond (beide diabate termen delen door C, dus het evenwichteinde blijft
//gelijk — alleen de dag/nacht-amplitude wordt gedempt, zoals echte bufering).
const zeebuffer     = 2.5;   //max. extra traagheid t.o.v. kale grond boven (diep)water
const waterDrempel  = 0.5;   //waterdeksel waar de buffering begint (≈ albedo-drempel)
const waterBereik   = 2.0;   //waterdiepte waarover zeebuffer naar vol loopt
const ijsBuffer     = 1.0;   //extra traagheid van ijsdekken (latente-warmte-achtig, mild)
const ijsBereik     = 1.0;   //ijsdikte waarover ijsbuffer verzadigt
const bodemBuffer   = 0.6;   //natte bodem (bodemVocht richting veldCapaciteit) buffert wat

//Twee-fasen vocht (zie waterLucht.comp): damp <-> wolk <-> regen.
//luchtVocht is de damp (capaciteit volgt de temperatuur), wolken is het
//gecondenseerde water. Regen valt uitsluitend uit wolken.
const condensTempo   = 0.01;   //fractie oververzadigde damp die per ronde condenseert (zeer laag: damp blijft lang damp, wolkopbouw heel geleidelijk)
const wolkVerdamp    = 0.006;  //fractie wolkwater dat per ronde in droge lucht terugverdampt (lager = langlevendere wolken die ver worden meegeblazen)
const regenTempo     = 0.30;   //fractie wolkwater boven de draagkracht dat per ronde als regen uitvalt (hoog genoeg om de instroom bij te houden: wolken blijven beperkt, geen ophopende stapels)
const minWolk        = 0.01;   //onder deze waarde heet een cel wolkloos
const wolkDraagKracht = 0.40;  //max. wolkwater per eenheid; daarboven regent het uit (hoger = wolken dragen meer water vóór ze regenen)
const wolkDiffusie   = 0.8;    //nabije wolkpatchjes vloeien opzij samen (hoger = bredere, minder lijnvormige dekken i.p.v. dunne windstrepen; was 0.25)
const dekDump        = 0.08;   //fractie wolk die per ronde op een bergtop boven het wolkendek neerslaat (rate-limit: geen tsunami-dump in één ronde)
const maxRegenPerRonde = 0.6;  //het absolute neerslagplafond per cel per ronde (piekbegrenzer: een dikke wolk loopt geleidelijk leeg, nooit in één slag)

//Terrein wordt bijgehouden als twee onafhankelijke lagen: rotsHoogte (de vaste
//ondergrond) en zandHoogte (de losse deklaag; invariant >= 0). De terreinhoogte
//(het oppervlak waarover water stroomt en die wordt gerenderd) is daarvan de
//afgeleide som — zie grondHoogte() hieronder.
struct vak {
    grondSoort  : i32,
    zandHoogte  : f32,
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
    zonZicht    : f32, //fractie zonlicht die het terrein bereikt (uit de schaduwkaart; 1 = volle zon)
    pijpen      : array<f32, maxBuren>,
    vochtPijpenA : array<f32, maxBuren>, //flux van damp (luchtVocht) per buur, behoudend
    vochtPijpenB : array<f32, maxBuren>, //flux van wolken per buur, behoudend
    snelheid    : vec2f,
    wind        : vec2f,
    plek        : vec2f,
};

//De terreinhoogte is afgeleid: ondergrond + zandlaag. De helper werkt zowel op
//vakken0 als vakken1.
fn grondHoogte(v : vak) -> f32 {
    return v.rotsHoogte + v.zandHoogte;
}

struct vakMeta {
    normaal     : vec4f,
    oost        : vec4f,        //lokale raakvlak-basis (oost) in wereldcoördinaten
    noord       : vec4f,        //lokale raakvlak-basis (noord) in wereldcoördinaten
    gradWeights : vec3f,        //(a,b,c) van de 2×2 correctiematrix M = avgDist · C⁻¹
    buurRicht   : array<vec2f, maxBuren>,
    buren       : array<u32, maxBuren>,
    burenAantal : u32,
    gradSchaal  : f32,        //2 / gemiddelde buurafstand: schaalt LS-gradient/divergentie naar de ware waarde
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
    schaduw     : vec4f, //(schaduwAan, schaduwKaartGrootte, ongebruikt, ongebruikt)
};

//Parameters voor de weergave-shaders (bind-groep 0, binding 2)
struct extraParameters {
    grondMult   : f32,
    grondSchaal : f32,
    schaduwGrootte : f32, //resolutie van de schaduwkaart (pixels per zijde)
    _padB       : f32,
    kijkPlek    : vec3f,
    _padC       : f32,
    zonPos      : vec3f,
    _padD       : f32,
    maxGrondHoogte : f32, //hoogste terreinpunt (bepaald bij het laden); basis voor het wolkendek
    overlayKeuze    : f32, //weergave-overlay (cijfertoetsen): 0 = normaal, 1 = temperatuur, 2 = wind+druk, 3 = bodemvocht, 4 = luchtvocht/wolken/druk, 5 = ijs/water/bodemvocht, 6 = wolken, 7 = zonZicht, 8 = leven, 9 = terreinhoogte
    _padE           : f32,
    schaduwAan      : f32, //1 = schaduwkaart aan (toets N)
};