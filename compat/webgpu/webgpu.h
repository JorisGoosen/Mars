// wgpu-native levert <webgpu.h> op de top-level include-map, maar imgui_impl_wgpu
// verwacht <webgpu/webgpu.h>. Deze shim re-exporteert wgpu-native's webgpu.h.
// (Alleen gebruikt op native; op web levert de emdawnwebgpu-port het echte bestand.)
#pragma once
#include <webgpu.h>
