#pragma once

// Minimal fallback header for d3d10effect.h when Windows SDK omits it.
// Provides forward declarations required by d3d10.h without adding dependencies.

#include <unknwn.h>

struct ID3D10Effect;
struct ID3D10EffectTechnique;
struct ID3D10EffectPass;
struct ID3D10EffectVariable;
struct ID3D10EffectScalarVariable;
struct ID3D10EffectVectorVariable;
struct ID3D10EffectMatrixVariable;
struct ID3D10EffectStringVariable;
struct ID3D10EffectShaderResourceVariable;
struct ID3D10EffectRenderTargetViewVariable;
struct ID3D10EffectDepthStencilViewVariable;
struct ID3D10EffectConstantBuffer;
struct ID3D10EffectShaderVariable;
struct ID3D10EffectBlendVariable;
struct ID3D10EffectDepthStencilVariable;
struct ID3D10EffectRasterizerVariable;
struct ID3D10EffectSamplerVariable;
