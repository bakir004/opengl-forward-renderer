# Materials — PBR Parameters, Texture Slots, and Best Practices

This document specifies the material system for the forward renderer, covering PBR (Physically Based Rendering) parameter definitions, texture slots, color-space policies, normal map conventions, and fallback handling.

---

## 1. PBR Material Parameters

The renderer uses **Metallic-Roughness** PBR workflow (also called "core" PBR). All materials support the following scalar and vector parameters:

### 1.1 Scalar Parameters

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `u_MetallicValue` | float | [0, 1] | 0.0 | Metallicness. 0 = dielectric (plastic, skin, wood); 1 = metal (copper, iron). |
| `u_RoughnessValue` | float | [0, 1] | 0.5 | Perceptual roughness. 0 = mirror-like (very smooth); 1 = diffuse (very rough). |
| `u_AoStrength` | float | [0, 1] | 1.0 | Multiplier for ambient occlusion strength. Reduces ambient light in occluded areas. |
| `u_NormalScale` | float | [0, ∞) | 1.0 | Multiplier for normal map strength. > 1 exaggerates; < 1 smooths. |
| `u_EmissiveStrength` | float | [0, ∞) | 1.0 | Multiplier for self-emitted light. 1.0 = use emissive color as-is; 2.0 = double brightness. |
| `u_IBLIntensity` | float | [0, ∞) | 1.0 | Global IBL (Image-Based Lighting) strength. Affects both diffuse and specular contributions. |
| `u_AmbientFloorStrength` | float | [0, 1] | 0.18 | Minimum ambient light in shadowed areas. Prevents black holes. |
| `u_MaxShadowOcclusion` | float | [0, 1] | 0.75 | Maximum shadow darkening. Prevents completely black shadows. |

### 1.2 Vector Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `u_AlbedoColor` | vec3 | (1, 1, 1) | Base color when no albedo map is present. Multiplied by albedo map if it exists. |
| `u_EmissiveColor` | vec3 | (0, 0, 0) | Self-emitted light color. Multiplied by `u_EmissiveStrength`. Ignored if emissive map exists. |
| `u_TintColor` | vec4 | (1, 1, 1, 1) | Optional global color tint applied to all shading. Alpha channel unused (reserved). |

### 1.3 Boolean Flags

| Parameter | Default | Description |
|-----------|---------|-------------|
| `u_UseNormalMap` | true | Whether to sample and apply the normal map. If false, geometry normals used as-is. |
| `u_HasAlbedoMap` | false | True if an albedo texture is bound (set automatically by MaterialInstance). |
| `u_HasNormalMap` | false | True if a normal texture is bound. |
| `u_HasMetallicMap` | false | True if a metallic texture is bound. |
| `u_HasRoughnessMap` | false | True if a roughness texture is bound. |
| `u_HasAoMap` | false | True if an AO texture is bound. |
| `u_HasEmissiveMap` | false | True if an emissive texture is bound. |
| `u_HasSpecularGlossinessMap` | false | True if using legacy specular/glossiness workflow. |
| `u_IsSpecularGlossiness` | false | Enables legacy specular/glossiness shading instead of metallic/roughness. |
| `u_IsPackedMetalRough` | false | True if metallic and roughness are packed into a single texture's channels. |

**Implementation detail**: Flags are automatically set by `MaterialInstance::Bind()` based on which textures are assigned. Shaders use these flags to decide branching (sample map vs. use scalar default).

---

## 2. Texture Slots

The material system defines **7 reserved texture slots** for the standard PBR workflow. Each slot is assigned a **fixed GL texture unit** to minimize unit thrashing.

### 2.1 Slot Definitions

#### Albedo (Metallic-Roughness Workflow)
- **Slot name**: `TextureSlot::Albedo` ("u_AlbedoMap")
- **GL texture unit**: 0
- **Color space**: **sRGB** (driver converts to linear during sampling)
- **Internal format**: `GL_SRGB8_ALPHA8`
- **Resolution**: 1K–4K typical; power-of-two for mipmaps
- **Content**: Base color of the material (diffuse color). Ignores metallic-ness — metallic is conveyed through `u_MetallicValue` or the metallic map.
- **Alpha channel**: Optional. If present, used for transparency (blend = `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`).

**Example workflow**:
```cpp
auto albedoTex = AssetImporter::LoadTexture("assets/models/stone_albedo.png", TextureColorSpace::sRGB);
material->SetTexture(TextureSlot::Albedo, albedoTex);
```

#### Normal (Tangent Space)
- **Slot name**: `TextureSlot::Normal` ("u_NormalMap")
- **GL texture unit**: 1
- **Color space**: **Linear** (no sRGB conversion)
- **Internal format**: `GL_RGBA8`
- **Content**: Per-pixel surface normal perturbations in **tangent space**. Stored as RGB (Blue-channel dominant for DXT5 compression).
  - R channel = tangent-space X (left-right bump)
  - G channel = tangent-space Y (forward-back bump)
  - B channel = tangent-space Z (surface normal base direction)
- **Convention**: OpenGL convention (right-handed tangent space).
  - G-channel inversion (flip green) may be needed for DirectX-sourced normal maps.

**Typical encoding**:
```glsl
// In mesh.frag
vec3 normal = texture(u_NormalMap, v_UV).rgb;
normal = normal * 2.0 - 1.0;  // Unpack from [0, 1] to [-1, 1]
normal = normalize(v_TBN * normal);  // Tangent-to-world transform
```

**Example**:
```cpp
auto normalTex = AssetImporter::LoadTexture("assets/models/stone_normal.png", TextureColorSpace::Linear);
material->SetTexture(TextureSlot::Normal, normalTex);
material->SetFloat("u_NormalScale", 1.5f);  // Exaggerate bumps
```

#### Metallic
- **Slot name**: `TextureSlot::Metallic` ("u_MetallicMap")
- **GL texture unit**: 2
- **Color space**: **Linear**
- **Internal format**: `GL_RGBA8` (only R channel used)
- **Content**: Per-pixel metallicness [0, 1]. Usually a single-channel map replicated to RGB for visualization.
  - 0 = dielectric (plastic, stone, skin, fabric)
  - 1 = metal (copper, steel, aluminum)
- **Typical values**:
  - Stone, fabric: 0.0–0.1
  - Copper oxidized: 0.4–0.6
  - Polished copper: 0.7–0.9
  - Bare steel: 0.8–1.0

**Example**:
```cpp
auto metallicTex = AssetImporter::LoadTexture("assets/models/metal_metallic.png", TextureColorSpace::Linear);
material->SetTexture(TextureSlot::Metallic, metallicTex);
```

#### Roughness
- **Slot name**: `TextureSlot::Roughness` ("u_RoughnessMap")
- **GL texture unit**: 3
- **Color space**: **Linear**
- **Internal format**: `GL_RGBA8` (only R channel used)
- **Content**: Per-pixel perceptual roughness [0, 1]. Single-channel or replicated.
  - 0 = mirror-like (very smooth, high specularity)
  - 0.5 = semi-rough (typical plastic, wood)
  - 1.0 = completely diffuse (no specularity)
- **Perceptual vs. linear**: Shader uses perceptual roughness directly; no remapping needed.

**Example**:
```cpp
auto roughnessTex = AssetImporter::LoadTexture("assets/models/plastic_roughness.png", TextureColorSpace::Linear);
material->SetTexture(TextureSlot::Roughness, roughnessTex);
```

#### Ambient Occlusion (AO)
- **Slot name**: `TextureSlot::AO` ("u_AOMap")
- **GL texture unit**: 4
- **Color space**: **Linear**
- **Internal format**: `GL_RGBA8` (only R channel used)
- **Content**: Per-pixel ambient occlusion [0, 1]. Darkens ambient light in crevices.
  - 1 = fully lit (no occlusion)
  - 0 = fully occluded (black)
- **Multiplied by**: `u_AoStrength` parameter (typically 1.0, can be reduced for subtle effect).

**Example**:
```cpp
auto aoTex = AssetImporter::LoadTexture("assets/models/stone_ao.png", TextureColorSpace::Linear);
material->SetTexture(TextureSlot::AO, aoTex);
material->SetFloat("u_AoStrength", 0.8f);  // Reduce intensity
```

#### Emissive
- **Slot name**: `TextureSlot::Emissive` ("u_EmissiveMap")
- **GL texture unit**: 5
- **Color space**: **sRGB** (this is light emission, so color space matters)
- **Internal format**: `GL_SRGB8_ALPHA8`
- **Content**: Per-pixel self-emitted light color. Adds glow and self-illumination.
  - Only non-black values contribute light.
  - Multiplied by `u_EmissiveStrength` and `u_EmissiveColor`.
- **Use cases**: Neon signs, glowing screens, bioluminescent organisms.

**Example**:
```cpp
auto emissiveTex = AssetImporter::LoadTexture("assets/models/screen_emissive.png", TextureColorSpace::sRGB);
material->SetTexture(TextureSlot::Emissive, emissiveTex);
material->SetFloat("u_EmissiveStrength", 2.0f);  // Double brightness
```

#### Specular-Glossiness (Legacy Workflow)
- **Slot name**: `TextureSlot::SpecularGlossiness` ("u_SpecularGlossinessMap")
- **GL texture unit**: 6
- **Color space**: **sRGB** (specular color is color-correlated)
- **Internal format**: `GL_SRGB8_ALPHA8`
- **Content**: Legacy PBR workflow (not metallic-roughness).
  - RGB = specular color (usually close to white or metal color)
  - A = glossiness (inverse of perceptual roughness: 1 = smooth, 0 = rough)
- **When used**: Set `u_IsSpecularGlossiness = true` and bind this map to override metallic-roughness.
- **Conversion rule** (if needed):
  ```cpp
  glossiness = 1.0 - roughness  // Approximate conversion
  specular ≈ mix(0.04, albedo, metallic)  // Approximate F0
  ```

**Example**:
```cpp
auto specGlossTex = AssetImporter::LoadTexture("assets/models/legacy_specgloss.png", TextureColorSpace::sRGB);
material->SetTexture(TextureSlot::SpecularGlossiness, specGlossTex);
material->SetBool("u_IsSpecularGlossiness", true);
```

---

## 3. sRGB vs. Linear Color Space Policy

The renderer enforces a **strict color space policy** to avoid banding, desaturation, and other visual artifacts:

### 3.1 sRGB Textures (Color Data)

**Use for**: Perceived color — data meant to be viewed by a human.

| Texture | Reason | Internal Format | Load Flag |
|---------|--------|-----------------|-----------|
| Albedo | Base visual color | `GL_SRGB8_ALPHA8` | `TextureColorSpace::sRGB` |
| Emissive | Light emission color | `GL_SRGB8_ALPHA8` | `TextureColorSpace::sRGB` |
| Specular-Glossiness | Specular color (legacy) | `GL_SRGB8_ALPHA8` | `TextureColorSpace::sRGB` |

**Driver behavior**: During sampling, GPU automatically converts sRGB → linear. Shader receives linear values. Prevents non-linear math on gamma-encoded data.

### 3.2 Linear Textures (Non-Color Data)

**Use for**: Physical measurements — data interpreted mathematically, not visually.

| Texture | Reason | Internal Format | Load Flag |
|---------|--------|-----------------|-----------|
| Normal | Surface perturbation vectors | `GL_RGBA8` | `TextureColorSpace::Linear` |
| Metallic | Physical metallicness [0, 1] | `GL_RGBA8` | `TextureColorSpace::Linear` |
| Roughness | Physical roughness [0, 1] | `GL_RGBA8` | `TextureColorSpace::Linear` |
| AO | Occlusion factor [0, 1] | `GL_RGBA8` | `TextureColorSpace::Linear` |
| Height/Parallax (future) | Displacement values | `GL_RGBA8` | `TextureColorSpace::Linear` |

**Shader consideration**: No automatic conversion. Shader reads the raw [0, 1] values and interprets them as-is. Correct math requires that the source image file uses linear intensity (not gamma-encoded).

### 3.3 Validation Checklist

Before assigning a texture to a slot:

1. **Albedo** → Should look slightly desaturated if viewed with gamma correction applied. Use sRGB.
2. **Normal** → Should have roughly equal blue-dominance (Z-facing). Use Linear.
3. **Metallic / Roughness / AO** → Usually monochrome or grayscale gradients. Use Linear.
4. **Emissive** → Should be bright/neon if self-lit. Use sRGB.
5. **Specular-Glossiness** → Should look like a color (e.g., metal or plastic) + transparency. Use sRGB.

**Tool tip**: If a map looks washed-out (desaturated) in the viewport, it may be loaded with the wrong color space. Swap it and recompile.

---

## 4. Normal Map Expectations and Conventions

### 4.1 Tangent-Space Normals

The renderer expects normal maps in **tangent space** (per-vertex TBN frame), not world space or object space. This is the standard across almost all real-time engines.

**Tangent space**: A local coordinate frame defined per-vertex:
- **T (Tangent)** = direction along increasing U (texture X)
- **B (Bitangent)** = direction along increasing V (texture Y)
- **N (Normal)** = geometric vertex normal (surface face-out)

**Normal map storage**:
- **X (Red channel)** → Tangent-space bump left/right
- **Y (Green channel)** → Tangent-space bump up/down (forward/back)
- **Z (Blue channel)** → Tangent-space Z component (usually highest magnitude, often dominant blue color)

### 4.2 Coordinate System Convention

The renderer uses **OpenGL convention**:
- **Right-handed tangent space**: T × B = N (cross product)
- **Y-axis up** in tangent space
- **Standard DirectX normal maps**: May have inverted Y (green channel). Solution: Flip green channel or set `u_NormalScale = vec2(1, -1)` in shader.

### 4.3 Generating or Creating Normal Maps

#### From Height Map
```bash
# Using an external tool (e.g., Substance Designer, xNormal, CrazyBump):
1. Load height map
2. Export as tangent-space normal map
3. Verify blue-channel dominance
4. Flip Y if using DirectX-convention source
```

#### Procedurally in Shader (for testing)
```glsl
// Simple flat-plane normal (should be mostly blue = (127, 127, 255) in 8-bit)
vec3 normalMap = normalize(vec3(0.0, 0.0, 1.0)) * 0.5 + 0.5;  // Pack to [0, 1]
```

### 4.4 Best Practices

- **High resolution**: 2K or 4K for hero characters/props; 1K for distant objects.
- **Baked from high-poly**: Normal maps should come from a high-resolution mesh baked to low-res.
- **Consistent source**: Use the same tool/settings for all maps in a material to avoid discontinuities.
- **Test with `u_NormalScale`**: Start at 1.0. Increase to exaggerate detail; decrease to smooth.
- **Preserve compression**: Use BC5 (DDS) or similar for 2-channel optimized storage if possible (otherwise RGBA8 is standard fallback).

---

## 5. Fallback Handling

The renderer gracefully handles missing or broken assets using fallback textures.

### 5.1 Fallback Texture Types

#### Magenta Checkerboard (Error Indicator)
```cpp
Texture2D fallback = Texture2D::CreateCheckerboard(8);
```
- **Color**: Alternating magenta (#FF00FF) and black
- **Purpose**: Highly visible indicator that an asset failed to load
- **Use case**: Missing albedo map, broken import

#### Solid Color
```cpp
Texture2D fallback = Texture2D::CreateFallback(128, 128, 128, 255);  // Mid-gray
```
- **Purpose**: Neutral default for optional maps
- **Use case**: Missing roughness (gray = 0.5 roughness default), missing AO (white = no occlusion)

### 5.2 Fallback Policy per Slot

| Slot | Missing Behavior | Reason |
|------|------------------|--------|
| Albedo | Magenta checkerboard | Very visible error; should never be missing. |
| Normal | Flat normal (blue) | Flat surface is safer than arbitrary perturbation. |
| Metallic | Mid-gray (0.5) | Dielectric default (0.0) or metal default (1.0) context-dependent; 0.5 is neutral. |
| Roughness | Mid-gray (0.5) | Neutral roughness for undecided surfaces. |
| AO | White (1.0) | No occlusion = fully lit. |
| Emissive | Black (0, 0, 0) | No self-emission. |

**Implementation**:
```cpp
auto albedoMap = AssetImporter::LoadTexture("path/to/albedo.png", TextureColorSpace::sRGB);
if (!albedoMap) {
    albedoMap = Texture2D::CreateCheckerboard(8);  // Magenta error
    spdlog::warn("Missing albedo map; using checkerboard fallback.");
}
material->SetTexture(TextureSlot::Albedo, albedoMap);
```

### 5.3 Fallback Shader (Error Material)

If a `RenderItem` has **no material and no shader** assigned:
- `RenderQueue` uses a fallback **error shader** (`error.vert` / `error.frag`).
- Renders the object in **magenta** with no lighting.
- Warning is logged so the issue is easy to spot.

**File**: `assets/shaders/error.vert` / `assets/shaders/error.frag`

---

## 6. Material Authoring Workflow

### 6.1 From Disk (AssetImporter)

```cpp
// Load a pre-authored material from JSON
auto material = AssetImporter::LoadMaterial("assets/materials/stone.mat");
```

**JSON format** (`stone.mat`):
```json
{
  "shader": "assets/shaders/mesh.vert|assets/shaders/mesh.frag",
  "textures": {
    "u_AlbedoMap": "assets/models/stone_albedo.png",
    "u_NormalMap": "assets/models/stone_normal.png",
    "u_RoughnessMap": "assets/models/stone_roughness.png"
  },
  "parameters": {
    "u_MetallicValue": 0.0,
    "u_RoughnessValue": 0.6,
    "u_AoStrength": 1.0,
    "u_NormalScale": 1.0
  }
}
```

### 6.2 Programmatic Creation

```cpp
auto meshShader = AssetImporter::LoadShader("mesh.vert", "mesh.frag");
auto material = std::make_shared<Material>(meshShader);

material->SetVec3("u_AlbedoColor", glm::vec3(0.9f, 0.8f, 0.7f));
material->SetFloat("u_MetallicValue", 0.1f);
material->SetFloat("u_RoughnessValue", 0.4f);
material->SetTexture(TextureSlot::Albedo, albedoTex);
material->SetTexture(TextureSlot::Normal, normalTex);

scene->AddObject(meshBuffer, material, transform);
```

### 6.3 Per-Instance Override

```cpp
auto instance = std::make_shared<MaterialInstance>(material);
instance->SetFloat("u_RoughnessValue", 0.2f);  // Shinier than default
instance->SetVec3("u_AlbedoColor", glm::vec3(1.0f, 0.5f, 0.2f));  // Orange tint

renderItem.material = instance.get();
renderer.SubmitDraw(renderItem);
```

---

## 7. Common Material Recipes

### 7.1 Brushed Metal

```cpp
auto metal = std::make_shared<Material>(meshShader);
metal->SetVec3("u_AlbedoColor", glm::vec3(0.5f, 0.5f, 0.5f));  // Gray
metal->SetFloat("u_MetallicValue", 1.0f);     // Fully metallic
metal->SetFloat("u_RoughnessValue", 0.3f);    // Polished but brushed
metal->SetTexture(TextureSlot::Normal, brushedMetalNormal);
metal->SetTexture(TextureSlot::Roughness, roughnessTex);  // Varies across surface
```

### 7.2 Rough Ceramic

```cpp
auto ceramic = std::make_shared<Material>(meshShader);
ceramic->SetTexture(TextureSlot::Albedo, ceramicAlbedo);
ceramic->SetFloat("u_MetallicValue", 0.0f);     // Non-metal
ceramic->SetFloat("u_RoughnessValue", 0.8f);    // Very rough
ceramic->SetTexture(TextureSlot::Normal, ceramicNormal);  // Subtle detail
ceramic->SetFloat("u_NormalScale", 0.5f);      // Reduce detail strength
```

### 7.3 Wet Surface (Shiny Plastic)

```cpp
auto wet = std::make_shared<Material>(meshShader);
wet->SetVec3("u_AlbedoColor", glm::vec3(0.8f, 0.8f, 0.8f));
wet->SetFloat("u_MetallicValue", 0.0f);   // Plastic (non-metal)
wet->SetFloat("u_RoughnessValue", 0.15f); // Very smooth, shiny
wet->SetFloat("u_AoStrength", 0.6f);      // Some ambient occlusion
```

### 7.4 Self-Emitting Neon

```cpp
auto neon = std::make_shared<Material>(meshShader);
neon->SetVec3("u_AlbedoColor", glm::vec3(0.1f, 0.1f, 0.1f));   // Dark base
neon->SetVec3("u_EmissiveColor", glm::vec3(0.0f, 1.0f, 1.0f)); // Cyan glow
neon->SetFloat("u_EmissiveStrength", 3.0f);  // Bright emission
neon->SetFloat("u_MetallicValue", 0.0f);
neon->SetFloat("u_RoughnessValue", 0.2f);    // Slightly shiny
```

---

## 8. Debugging Materials

### 8.1 ImGui Debug Panel (Runtime)

The renderer exposes `RendererDebugStats` with IBL and shadow visualization. Use ImGui to inspect:
- **Albedo only**: Disable direct lighting, show base color.
- **Normal map**: Render normals as colors (tangent space should appear mostly blue).
- **Metallic/Roughness**: Visualize as grayscale.
- **AO**: Show AO map separately.
- **Per-cascade shadow**: Inspect each cascade's depth and fit.

### 8.2 Shader Debugging

**Fragment shader output override**:
```glsl
// Temporarily replace final color with diagnostic data
// gl_FragColor = vec4(normal, 1.0);       // Show normals as color
// gl_FragColor = vec4(vec3(metallic), 1.0);  // Show metallic
// gl_FragColor = vec4(vec3(roughness), 1.0); // Show roughness
```

### 8.3 Missing Asset Workflow

1. **Magenta checkerboard appears** → Texture load failed.
   - Check file path in JSON or code.
   - Verify file exists and is readable.
   - Check color space setting (sRGB vs. Linear).

2. **Magenta object (error shader)** → No material or shader assigned.
   - Check `RenderItem::material` or `RenderItem::shader`.
   - Verify material is created and added to scene.

3. **Flat shading (no detail)** → Normal map not assigned or disabled.
   - Check `u_UseNormalMap` flag.
   - Verify normal map color space is Linear.
   - Try increasing `u_NormalScale` temporarily to exaggerate effect.

---

## Conclusion

The material system provides:
- **PBR metallic-roughness workflow** with 6 primary texture slots and 11 scalar/vector parameters.
- **Strict color space policy**: sRGB for color data, Linear for physical data.
- **Tangent-space normal maps** following OpenGL conventions.
- **Graceful fallbacks** for missing assets with clear error indicators.
- **Flexible assignment**: Materials can be authored in JSON, created programmatically, or instanced per-object.

Careful adherence to texture color spaces, normal map conventions, and parameter ranges ensures realistic, consistent lighting and avoids common visual artifacts (banding, desaturation, incorrect specular highlights).
