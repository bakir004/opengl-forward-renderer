# Asset Conventions

This document defines the folder layout, naming rules, and file format schemas
for all assets in the Forward Renderer project.  
Follow these conventions so that `AssetImporter` can resolve paths correctly and
the rest of the team can find files without guessing.

---

## Folder Layout

```
assets/
├── materials/       # .mat JSON descriptor files
├── models/          # 3D model files (.obj, .gltf, .glb, .fbx)
├── shaders/         # GLSL shader source files (.vert, .frag)
├── textures/        # Image files (.png, .jpg, .tga, .hdr)
│   ├── albedo/      # Color/diffuse maps      (sRGB)
│   ├── normal/      # Normal maps              (Linear)
│   ├── roughness/   # Roughness maps           (Linear)
│   ├── metallic/    # Metallic maps            (Linear)
│   └── ao/          # Ambient occlusion maps   (Linear)
└── scenes/          # Scene preset JSON files
```

---

## Naming Rules

| Asset type     | Convention                          | Example                        |
|----------------|-------------------------------------|--------------------------------|
| Shader pair    | Same stem, `.vert` + `.frag`        | `basic.vert` / `basic.frag`    |
| Albedo map     | `<name>_albedo.png`                 | `rock_albedo.png`              |
| Normal map     | `<name>_normal.png`                 | `rock_normal.png`              |
| Roughness map  | `<name>_roughness.png`              | `rock_roughness.png`           |
| Metallic map   | `<name>_metallic.png`               | `rock_metallic.png`            |
| AO map         | `<name>_ao.png`                     | `rock_ao.png`                  |
| Material file  | `<name>.mat`                        | `rock.mat`                     |
| Model file     | lowercase, no spaces                | `barrel.obj`, `helmet.gltf`    |

- Use **lowercase** and **underscores** — no spaces, no uppercase in filenames.
- Shader pairs **must share the same stem** so `AssetImporter` can find the partner automatically.

---

## Color Space Policy

Getting color space wrong causes incorrect lighting. Follow this table strictly:

| Texture type          | ColorSpace value | GL internal format  |
|-----------------------|------------------|---------------------|
| Albedo / Base color   | `"sRGB"`         | `GL_SRGB8_ALPHA8`   |
| Normal map            | `"Linear"`       | `GL_RGBA8`          |
| Roughness map         | `"Linear"`       | `GL_RGBA8`          |
| Metallic map          | `"Linear"`       | `GL_RGBA8`          |
| AO map                | `"Linear"`       | `GL_RGBA8`          |
| Emissive map          | `"sRGB"`         | `GL_SRGB8_ALPHA8`   |
| HDR environment map   | `"Linear"`       | `GL_RGBA16F`        |

---

## .mat File Schema

Material files are JSON documents with the `.mat` extension.  
They are loaded by `AssetImporter::LoadMaterial()` and parsed into a `Material` object.

### Full Schema

```json
{
  "shader": {
    "vert": "assets/shaders/pbr.vert",
    "frag": "assets/shaders/pbr.frag"
  },
  "textures": {
    "u_AlbedoMap":    { "path": "assets/textures/albedo/rock_albedo.png",       "colorSpace": "sRGB"   },
    "u_NormalMap":    { "path": "assets/textures/normal/rock_normal.png",        "colorSpace": "Linear" },
    "u_MetallicMap":  { "path": "assets/textures/metallic/rock_metallic.png",    "colorSpace": "Linear" },
    "u_RoughnessMap": { "path": "assets/textures/roughness/rock_roughness.png",  "colorSpace": "Linear" },
    "u_AOMap":        { "path": "assets/textures/ao/rock_ao.png",                "colorSpace": "Linear" },
    "u_EmissiveMap":  { "path": "assets/textures/albedo/rock_emissive.png",      "colorSpace": "sRGB"   }
  },
  "floats": {
    "u_Metallic":   0.0,
    "u_Roughness":  0.5
  },
  "vec3s": {
    "u_AlbedoColor": [1.0, 1.0, 1.0]
  },
  "vec4s": {
    "u_TintColor": [1.0, 1.0, 1.0, 1.0]
  }
}
```

### Field Reference

| Field       | Required | Description |
|-------------|----------|-------------|
| `shader`    | Yes      | Paths to the vertex and fragment shader. Both must exist. |
| `textures`  | No       | Map of GLSL uniform name → texture descriptor. Key must match the sampler name in the shader exactly. |
| `floats`    | No       | Map of GLSL uniform name → float scalar value. |
| `vec3s`     | No       | Map of GLSL uniform name → array of 3 floats `[r, g, b]`. |
| `vec4s`     | No       | Map of GLSL uniform name → array of 4 floats `[r, g, b, a]`. |

### Texture Descriptor Fields

| Field        | Required | Default  | Description |
|--------------|----------|----------|-------------|
| `path`       | Yes      | —        | Path to the image file, relative to the working directory. |
| `colorSpace` | Yes      | —        | `"sRGB"` for color data, `"Linear"` for non-color data. See color space table above. |

### Known Texture Slot Names

These uniform names are defined in `TextureSlot` namespace in `Material.h` and must be used exactly as written:

| Uniform name      | Texture type    |
|-------------------|-----------------|
| `u_AlbedoMap`     | Base color      |
| `u_NormalMap`     | Normal map      |
| `u_MetallicMap`   | Metallic map    |
| `u_RoughnessMap`  | Roughness map   |
| `u_AOMap`         | Ambient occlusion |
| `u_EmissiveMap`   | Emissive map    |

---

## Fallback Behaviour

`AssetImporter` never crashes on a missing asset. Instead it logs a warning and substitutes:

| Asset type | Fallback |
|------------|----------|
| Texture    | 8×8 magenta/black checkerboard texture |
| Mesh       | Unit cube generated by `GenerateCube()` |
| Shader     | `nullptr` returned, error logged — no silent fallback |
| Material   | `nullptr` returned, error logged — no silent fallback |

The checkerboard pattern is intentionally loud so missing textures are immediately
obvious during development. A solid magenta or grey fallback is too easy to miss.

---

## Minimal Working Example

The simplest valid `.mat` file — no textures, just a shader and a tint color:

```json
{
  "shader": {
    "vert": "assets/shaders/basic.vert",
    "frag": "assets/shaders/basic.frag"
  },
  "vec3s": {
    "u_AlbedoColor": [1.0, 0.5, 0.2]
  }
}
```

This is equivalent to `assets/materials/default.mat` which ships with the project
and serves as the reference for new material files.

---

## Shader Vertex Layout

Shaders must match the vertex layout defined in `Primitives.h` (`VertexPC`):

| Location | Attribute    | Type   | Description         |
|----------|--------------|--------|---------------------|
| 0        | `a_Position` | `vec3` | Vertex position     |
| 1        | `a_Color`    | `vec3` | Vertex color        |

When PBR textures are added in Sprint 7, normals, UVs, and tangents will be
added at locations 2, 3, and 4 respectively. This document will be updated.
