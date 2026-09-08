//Fragment-shader voor de pick-pass: schrijft de 32-bits cel-ID als 4×RGBA8.
//De lege achtergrond (clear-kleur wit) decodeert als 0xFFFFFFFF = "geen cel".
struct naarFrag {
    @builtin(position) glPos : vec4f,
    @location(0) @interpolate(flat) celId : u32,
};

@fragment
fn main(in : naarFrag) -> @location(0) vec4f {
    let id = in.celId;
    return vec4f(
        f32(id & 255u) / 255.0,
        f32((id >> 8u) & 255u) / 255.0,
        f32((id >> 16u) & 255u) / 255.0,
        f32((id >> 24u) & 255u) / 255.0,
    );
}
