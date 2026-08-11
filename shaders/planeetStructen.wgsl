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
const bezinkheid      = 0.05;
const droesemheid     = 0.1;
const vertrager       = 1.0 / 3.0;
const zeerKlein       = 0.0001;
const minGrondHoogte  = 10.0;
const maxGrondHoogte  = 200.0;
const toonSediment    = 0.003; //boven deze waarde wordt grondSoort zand
const minWaterSed     = 0.1;

const maxBuren = 6u;

struct vak {
    grondSoort  : i32,
    grondHoogte : f32,
    waterHoogte : f32,
    waterSchijn : f32,
    vocht       : f32,
    ijs         : f32,
    leven       : f32,
    droesem     : f32,
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
    verdamping  : f32,
    _padA       : f32,
    _padB       : f32,
    regenPlek   : vec4f,
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