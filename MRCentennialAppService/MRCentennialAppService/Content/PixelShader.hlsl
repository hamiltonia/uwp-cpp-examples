//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// Per-pixel color data passed through to the pixel shader.
struct PixelShaderInput
{
    min16float4 pos         : SV_POSITION;
    min16float3 color       : COLOR0;
    min16float2 texCoord    : TEXCOORD1;
};

Texture2D       tex         : t0;
SamplerState    samp        : s0;

// The pixel shader renders a color value based on an input texture and texture coord
min16float4 main(PixelShaderInput input) : SV_TARGET
{
  // Read both distance function values.
  min16float4 textureValue = tex.Sample(samp, input.texCoord);

  // Apply the result.
  return textureValue;
}