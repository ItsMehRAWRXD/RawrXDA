// Stub for missing d3d10effect.h
// This file is not needed for D3D11 applications
#pragma once

// Minimal declarations to satisfy d3d10.h include
#ifndef __d3d10effect_h__
#define __d3d10effect_h__

// Forward declarations only - no actual implementation needed for D3D11
typedef struct ID3D10EffectType ID3D10EffectType;
typedef struct ID3D10EffectVariable ID3D10EffectVariable;
typedef struct ID3D10EffectConstantBuffer ID3D10EffectConstantBuffer;
typedef struct ID3D10EffectShaderVariable ID3D10EffectShaderVariable;
typedef struct ID3D10EffectBlendVariable ID3D10EffectBlendVariable;
typedef struct ID3D10EffectDepthStencilVariable ID3D10EffectDepthStencilVariable;
typedef struct ID3D10EffectRasterizerVariable ID3D10EffectRasterizerVariable;
typedef struct ID3D10EffectSamplerVariable ID3D10EffectSamplerVariable;
typedef struct ID3D10EffectPass ID3D10EffectPass;
typedef struct ID3D10EffectTechnique ID3D10EffectTechnique;
typedef struct ID3D10Effect ID3D10Effect;
typedef struct ID3D10EffectPool ID3D10EffectPool;

#endif // __d3d10effect_h__
