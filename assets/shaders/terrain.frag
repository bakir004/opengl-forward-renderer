const int NUM_CASCADES = 4;

in vec3  v_Normal;
in vec3  v_WorldPos;
in vec2  v_UV;
in mat3  v_TBN;
in float v_ViewDepth;
in vec4  v_TerrainData; // x=materialZone, y=mountainMask, z=grassSuitable, w=treeSuitable

layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

#include "light_block.glsl"
#include "pbr_helpers.glsl"

// Debug view modes (must match TerrainDebugView enum in TerrainScene.h)
const int DEBUG_OFF          = 0;
const int DEBUG_HEIGHTMAP    = 1;
const int DEBUG_SLOPE        = 2;
const int DEBUG_NORMALS      = 3;
const int DEBUG_MATERIAL_ZONE = 4;
const int DEBUG_MOUNTAIN_MASK = 5;
const int DEBUG_GRASS_MASK   = 6;
const int DEBUG_TREE_MASK    = 7;
const int DEBUG_ROCK_MASK    = 8;

uniform int u_DebugView = DEBUG_OFF;

// Shadow
uniform sampler2DArray u_CascadeShadowMaps;
uniform mat4  u_CascadeViewProj[NUM_CASCADES];
uniform float u_CascadeSplits[NUM_CASCADES];
uniform int   u_PCFRadius    = 1;
uniform int   u_ReceiveShadow = 1;
uniform float u_MaxShadowOcclusion = 0.75;

// IBL
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilteredMap;
uniform sampler2D   u_BRDFLUT;
uniform bool  u_HasIBL          = false;
uniform bool  u_HasIrradianceMap = false;
uniform bool  u_HasPrefilteredMap = false;
uniform bool  u_HasBRDFLUT      = false;
uniform float u_IBLIntensity    = 1.0;
uniform float u_AmbientFloorStrength = 0.18;

// Per-zone PBR scalars (set by TerrainScene)
// Zone order matches TerrainMaterialZone enum: 0=DeepWater .. 6=Snow
uniform vec3  u_ZoneColor[7];
uniform float u_ZoneRoughness[7];
uniform float u_ZoneMetallic[7];

// Sprint 10 — Terrain texture samplers for zone-specific visual materials.
// Texture units 12-18 reserved for terrain zone textures.
// These sample procedurally or from artist-provided textures.
uniform sampler2D u_GrassAlbedo;      // Unit 12 — Grass zone base color
uniform sampler2D u_RockAlbedo;       // Unit 13 — Rock zone base color
uniform sampler2D u_SnowAlbedo;       // Unit 14 — Snow zone base color
uniform sampler2D u_SandAlbedo;       // Unit 15 — Sand zone base color
uniform sampler2D u_ForestAlbedo;     // Unit 16 — Forest zone base color
uniform sampler2D u_GrassNormal;      // Unit 17 — Grass normal map (optional)
uniform sampler2D u_RockNormal;       // Unit 18 — Rock normal map (optional)

// Texture blending controls
uniform bool  u_UseTerrainTextures   = true;   ///< Enable texture sampling; fallback to u_ZoneColor if false
uniform float u_TextureScale         = 0.5f;   ///< World-space texture repeat scale
uniform float u_SlopeToRockTransition = 0.4f;  ///< Steepness threshold for rock appearance

// World-space terrain height range, used to normalize the heightmap debug view.
uniform float u_HeightScale = 80.0;

// Rock-scatter mask is not packed per-vertex (the terrain vertex only carries
// zone, mountain, grass and tree in its 4-float channel). For the debug view it
// is reconstructed here from slope + height, mirroring the generator's logic.
uniform float u_RockMaskHeightStart = 0.55;
uniform float u_RockMaskHeightEnd   = 0.80;
uniform float u_RockMaskSlopeStart  = 0.35;
uniform float u_RockMaskSlopeEnd    = 0.65;

out vec4 FragColor;

// ─── Cascade shadow helpers (copied from mesh.frag) ─────────────────────────

int SelectCascade(float viewDepth)
{
    for (int i = 0; i < NUM_CASCADES; ++i)
        if (viewDepth < u_CascadeSplits[i]) return i;
    return NUM_CASCADES - 1;
}

float SampleCascade(int cascade, vec3 worldPos, vec3 normal, float bias)
{
    float normalOffsetScale = u_Directional.normalBias * (1.0 + float(cascade) * 0.5);
    vec3  biasedPos = worldPos + normal * normalOffsetScale;

    vec4 lsPos = u_CascadeViewProj[cascade] * vec4(biasedPos, 1.0);
    vec3 proj   = lsPos.xyz / lsPos.w * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return -1.0;

    vec2 texelSize = 1.0 / vec2(textureSize(u_CascadeShadowMaps, 0).xy);
    int radius = clamp(u_PCFRadius, 0, 4);
    if (radius == 0)
    {
        float d = texture(u_CascadeShadowMaps, vec3(proj.xy, float(cascade))).r;
        return (proj.z - bias) > d ? 1.0 : 0.0;
    }
    float shadow = 0.0, samples = 0.0;
    for (int x = -radius; x <= radius; ++x)
        for (int y = -radius; y <= radius; ++y)
        {
            float pcfD = texture(u_CascadeShadowMaps, vec3(proj.xy + vec2(x,y)*texelSize, float(cascade))).r;
            shadow += (proj.z - bias) > pcfD ? 1.0 : 0.0;
            samples += 1.0;
        }
    return shadow / samples;
}

float CascadeBias(int cascade, float slope)
{
    float base  = u_Directional.depthBias;
    float biasV = max(base * u_Directional.slopeBias * slope, base * 0.3);
    return biasV * (1.0 + float(cascade) * 0.75);
}

const float CASCADE_EDGE_BLEND_START = 0.65;
const float CASCADE_BLEND_FRACTION   = 0.85;

float CascadeEdgeBlend(int cascade, vec3 worldPos, vec3 normal)
{
    float nos = u_Directional.normalBias * (1.0 + float(cascade) * 0.5);
    vec4 lsPos = u_CascadeViewProj[cascade] * vec4(worldPos + normal * nos, 1.0);
    vec3 ndc   = lsPos.xyz / lsPos.w;
    float maxC = max(max(abs(ndc.x), abs(ndc.y)), abs(ndc.z));
    float f = (maxC - CASCADE_EDGE_BLEND_START) / max(1.0 - CASCADE_EDGE_BLEND_START, 0.0001);
    return clamp(f, 0.0, 1.0);
}

float CalculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, float viewDepth)
{
    int cascade = SelectCascade(viewDepth);
    float slope = 1.0 - max(dot(normal, lightDir), 0.0);
    float shadow = SampleCascade(cascade, worldPos, normal, CascadeBias(cascade, slope));
    if (shadow < 0.0)
    {
        for (int i = cascade + 1; i < NUM_CASCADES; ++i)
        {
            shadow = SampleCascade(i, worldPos, normal, CascadeBias(i, slope));
            if (shadow >= 0.0) break;
        }
        if (shadow < 0.0) return 0.0;
    }
    if (cascade < NUM_CASCADES - 1)
    {
        float ef = CascadeEdgeBlend(cascade, worldPos, normal);
        float splitNear = (cascade == 0) ? 0.0 : u_CascadeSplits[cascade - 1];
        float splitFar  = u_CascadeSplits[cascade];
        float blendStart = mix(splitFar, splitNear, CASCADE_BLEND_FRACTION);
        float df = clamp((viewDepth - blendStart) / max(splitFar - blendStart, 0.0001), 0.0, 1.0);
        float t = max(ef, df); t = t * t * (3.0 - 2.0 * t);
        if (t > 0.0)
        {
            float ns = SampleCascade(cascade + 1, worldPos, normal, CascadeBias(cascade + 1, slope));
            if (ns >= 0.0) shadow = mix(shadow, ns, t);
        }
    }
    return shadow;
}

// ─── Attenuation ─────────────────────────────────────────────────────────────

float DistanceAttenuation(float radius, float c, float l, float q, float d)
{
    if (d >= radius) return 0.0;
    float phys = 1.0 / max(c + l * d + q * d * d, 0.0001);
    float norm = 1.0 - (d / radius);
    return phys * norm * norm;
}

// ─── PBR helpers ──────────────────────────────────────────────────────────────

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculatePBRLight(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, vec3 F0, float roughness, float metallic)
{
    vec3 H     = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotV <= 0.0 || NdotL <= 0.0) return vec3(0.0);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;
    return (diffuse + specular) * radiance * NdotL;
}

// ─── Zone blending ───────────────────────────────────────────────────────────

// Smooth interpolation weights between the two nearest integer zones.
// zoneF = materialZone float (e.g. 2.7 = 30% zone 3, 70% zone 2).
void ZoneWeights(float zoneF, out int zoneA, out int zoneB, out float t)
{
    zoneA = int(floor(zoneF));
    zoneB = int(ceil(zoneF));
    zoneA = clamp(zoneA, 0, 6);
    zoneB = clamp(zoneB, 0, 6);
    t = fract(zoneF);
    // Smooth the transition
    t = t * t * (3.0 - 2.0 * t);
}

// ─── Sprint 10: Terrain texture sampling with mask-driven blending ──────────

/// Sample zone-specific texture with world-space UV tiling.
/// Uses v_UV which is derived from the heightfield grid coordinates.
vec3 SampleZoneTexture(sampler2D tex, float scale)
{
    return texture(tex, v_UV * scale).rgb;
}

/// Blend between two zone textures based on zone transition factor.
vec3 BlendZoneTextures(sampler2D texA, sampler2D texB, float t, float scale)
{
    vec3 colorA = SampleZoneTexture(texA, scale);
    vec3 colorB = SampleZoneTexture(texB, scale);
    return mix(colorA, colorB, t);
}

/// Sample terrain albedo with texture blending by material zone and masks.
/// Integrates height, slope, and mask-driven transitions (grass/rock/snow).
/// Falls back to u_ZoneColor if u_UseTerrainTextures is false.
vec3 TerrainAlbedoTextured(float zoneF, float slope, float h01,
                           float grassM, float mountMask,
                           out float outRoughness, out float outMetallic)
{
    int zoneA, zoneB; float zoneMix;
    ZoneWeights(zoneF, zoneA, zoneB, zoneMix);

    vec3 albedo = vec3(0.0);

    if (!u_UseTerrainTextures)
    {
        // Fallback: solid zone colors
        albedo = mix(u_ZoneColor[zoneA], u_ZoneColor[zoneB], zoneMix);
        outRoughness = mix(u_ZoneRoughness[zoneA], u_ZoneRoughness[zoneB], zoneMix);
        outMetallic  = mix(u_ZoneMetallic[zoneA], u_ZoneMetallic[zoneB], zoneMix);
        return albedo;
    }

    // ─── Terrain zone material sampling ──────────────────────────────────────
    // Each zone contributes its textured appearance based on:
    //   - Base zone float value (zoneF)
    //   - Height and slope masks (for rock scatter on slopes)
    //   - Grass/tree/mountain masks (for vegetation override)
    
    float scale = u_TextureScale;

    // Grass (zone 3): appears on moderate slopes in grass-suitable areas
    float grassBlend = 0.0;
    if (zoneA == 3 || zoneB == 3)
    {
        grassBlend = (zoneA == 3) ? zoneMix : (1.0 - zoneMix);
        // Suppress grass on very steep slopes
        grassBlend *= smoothstep(0.7, 0.2, slope);
        // Enhance grass in grass-suitable areas
        grassBlend *= grassM;
    }

    // Rock (zone 5): appears on steep slopes and in mountain scatter areas
    float rockBlend = 0.0;
    if (zoneA == 5 || zoneB == 5)
    {
        rockBlend = (zoneA == 5) ? zoneMix : (1.0 - zoneMix);
        // Enhance rock on steep slopes
        rockBlend *= smoothstep(u_SlopeToRockTransition - 0.1, u_SlopeToRockTransition + 0.1, slope);
        // Mountain scatter overlay
        rockBlend = mix(rockBlend, 1.0, mountMask * 0.4);
    }

    // Snow (zone 6): appears at high elevations and on steep frozen slopes
    float snowBlend = 0.0;
    if (zoneA == 6 || zoneB == 6)
    {
        snowBlend = (zoneA == 6) ? zoneMix : (1.0 - zoneMix);
        // Suppress snow on very steep slopes (slides off)
        snowBlend *= smoothstep(0.8, 0.4, slope);
    }

    // Sand (zone 2): mostly horizontal, suppressed on slopes
    float sandBlend = 0.0;
    if (zoneA == 2 || zoneB == 2)
    {
        sandBlend = (zoneA == 2) ? zoneMix : (1.0 - zoneMix);
        sandBlend *= smoothstep(0.4, 0.0, slope);
    }

    // Forest (zone 4): canopy color, suppressed on steeper slopes
    float forestBlend = 0.0;
    if (zoneA == 4 || zoneB == 4)
    {
        forestBlend = (zoneA == 4) ? zoneMix : (1.0 - zoneMix);
        forestBlend *= smoothstep(0.5, 0.1, slope);
    }

    // ─── Build final albedo with mask-driven transitions ────────────────────

    // Normalize blends to 1.0 for smooth transitions
    float totalBlend = grassBlend + rockBlend + snowBlend + sandBlend + forestBlend;
    if (totalBlend < 0.0001)
    {
        // Fallback if no mask blends: use zone-based texture
        int mainZone = (zoneMix < 0.5) ? zoneA : zoneB;
        if (mainZone == 3)
            albedo = SampleZoneTexture(u_GrassAlbedo, scale);
        else if (mainZone == 5)
            albedo = SampleZoneTexture(u_RockAlbedo, scale);
        else if (mainZone == 6)
            albedo = SampleZoneTexture(u_SnowAlbedo, scale);
        else if (mainZone == 2)
            albedo = SampleZoneTexture(u_SandAlbedo, scale);
        else if (mainZone == 4)
            albedo = SampleZoneTexture(u_ForestAlbedo, scale);
        else
            albedo = mix(u_ZoneColor[zoneA], u_ZoneColor[zoneB], zoneMix);
    }
    else
    {
        // Blend multiple textures based on mask contributions
        grassBlend /= totalBlend;
        rockBlend /= totalBlend;
        snowBlend /= totalBlend;
        sandBlend /= totalBlend;
        forestBlend /= totalBlend;

        albedo = vec3(0.0);
        if (grassBlend > 0.001)
            albedo += SampleZoneTexture(u_GrassAlbedo, scale) * grassBlend;
        if (rockBlend > 0.001)
            albedo += SampleZoneTexture(u_RockAlbedo, scale) * rockBlend;
        if (snowBlend > 0.001)
            albedo += SampleZoneTexture(u_SnowAlbedo, scale) * snowBlend;
        if (sandBlend > 0.001)
            albedo += SampleZoneTexture(u_SandAlbedo, scale) * sandBlend;
        if (forestBlend > 0.001)
            albedo += SampleZoneTexture(u_ForestAlbedo, scale) * forestBlend;
    }

    // Ensure we always have a valid color
    if (length(albedo) < 0.001)
        albedo = mix(u_ZoneColor[zoneA], u_ZoneColor[zoneB], zoneMix);

    // ─── PBR parameters: blend roughness/metallic between zones ─────────────
    outRoughness = mix(u_ZoneRoughness[zoneA], u_ZoneRoughness[zoneB], zoneMix);
    outMetallic  = mix(u_ZoneMetallic[zoneA], u_ZoneMetallic[zoneB], zoneMix);

    return albedo;
}

vec3 TerrainAlbedo(float zoneF)
{
    int a, b; float t;
    ZoneWeights(zoneF, a, b, t);
    return mix(u_ZoneColor[a], u_ZoneColor[b], t);
}

float TerrainRoughness(float zoneF)
{
    int a, b; float t;
    ZoneWeights(zoneF, a, b, t);
    return mix(u_ZoneRoughness[a], u_ZoneRoughness[b], t);
}

float TerrainMetallic(float zoneF)
{
    int a, b; float t;
    ZoneWeights(zoneF, a, b, t);
    return mix(u_ZoneMetallic[a], u_ZoneMetallic[b], t);
}

// ─── Main ───────────────────────────────────────────────────────────────────

void main()
{
    float zoneF    = v_TerrainData.x;
    float mountMask = v_TerrainData.y;
    float grassM   = v_TerrainData.z;
    float treeM    = v_TerrainData.w;

    // Slope and normalized height are reconstructed once for both the debug
    // views and the in-shader rock mask. The generator defines:
    //   slope = clamp(1 - N.y, 0, 1)   (0 = flat, 1 = vertical)
    //   h01   = worldPosY / heightScale
    vec3  nrm    = normalize(v_Normal);
    float slope  = clamp(1.0 - nrm.y, 0.0, 1.0);
    float h01    = clamp(v_WorldPos.y / max(u_HeightScale, 0.0001), 0.0, 1.0);

    // ── Debug views ──────────────────────────────────────────────────────────
    if (u_DebugView == DEBUG_HEIGHTMAP)
    {
        FragColor = vec4(vec3(h01), 1.0);
        return;
    }
    if (u_DebugView == DEBUG_NORMALS)
    {
        FragColor = vec4(nrm * 0.5 + 0.5, 1.0);
        return;
    }
    if (u_DebugView == DEBUG_SLOPE)
    {
        FragColor = vec4(vec3(slope), 1.0);
        return;
    }
    if (u_DebugView == DEBUG_MATERIAL_ZONE)
    {
        // Colour per zone: deepwater=navy, shallow=teal, sand=tan, grass=lime,
        //                  forest=darkgreen, rock=grey, snow=white
        const vec3 zoneColors[7] = vec3[7](
            vec3(0.0, 0.05, 0.35),  // DeepWater
            vec3(0.0, 0.40, 0.55),  // ShallowWater
            vec3(0.87, 0.77, 0.54), // Sand
            vec3(0.35, 0.75, 0.15), // Grass
            vec3(0.08, 0.38, 0.08), // Forest
            vec3(0.48, 0.44, 0.40), // Rock
            vec3(0.93, 0.95, 0.97)  // Snow
        );
        int a, b; float t;
        ZoneWeights(zoneF, a, b, t);
        FragColor = vec4(mix(zoneColors[a], zoneColors[b], t), 1.0);
        return;
    }
    if (u_DebugView == DEBUG_MOUNTAIN_MASK)
    {
        FragColor = vec4(vec3(mountMask), 1.0);
        return;
    }
    if (u_DebugView == DEBUG_GRASS_MASK)
    {
        FragColor = vec4(vec3(grassM), 1.0);
        return;
    }
    if (u_DebugView == DEBUG_TREE_MASK)
    {
        FragColor = vec4(vec3(treeM), 1.0);
        return;
    }
    if (u_DebugView == DEBUG_ROCK_MASK)
    {
        // Reconstruct rock-scatter suitability the same way the generator does:
        // max(height-band, slope-band), suppressed where grass/tree dominate.
        float rockH = smoothstep(u_RockMaskHeightStart, u_RockMaskHeightEnd, h01);
        float rockS = smoothstep(u_RockMaskSlopeStart,  u_RockMaskSlopeEnd,  slope);
        float rock  = clamp(max(rockH, rockS) * (1.0 - grassM) * (1.0 - treeM), 0.0, 1.0);
        FragColor = vec4(vec3(rock), 1.0);
        return;
    }

    // ── Normal ───────────────────────────────────────────────────────────────
    vec3 N = nrm;
    vec3 V = normalize(cameraPos - v_WorldPos);

    // ── Zone-blended PBR material with texture blending ──────────────────────
    // Sprint 10: TerrainAlbedoTextured samples zone textures based on height,
    // slope, and mask data (grass, mountain, tree suitability masks).
    float roughness, metallic;
    vec3  albedo = TerrainAlbedoTextured(zoneF, slope, h01, grassM, mountMask,
                                         roughness, metallic);

    float ao = 1.0;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── IBL ──────────────────────────────────────────────────────────────────
    float NdotV = max(dot(N, V), 0.0);
    vec3 kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD_ibl = (vec3(1.0) - kS_ibl) * (1.0 - metallic);

    vec3 iblDiffuse  = vec3(0.0);
    vec3 iblSpecular = vec3(0.0);
    if (u_HasIBL && u_HasIrradianceMap)
    {
        vec3 irradiance = texture(u_IrradianceMap, N).rgb;
        iblDiffuse = kD_ibl * irradiance * albedo * ao * max(u_IBLIntensity, 0.0);
    }
    if (u_HasIBL && u_HasPrefilteredMap && u_HasBRDFLUT)
    {
        vec3  R       = reflect(-V, N);
        float mipCount = float(textureQueryLevels(u_PrefilteredMap));
        vec3  prefilt  = textureLod(u_PrefilteredMap, R, roughness * (mipCount - 1.0)).rgb;
        vec2  brdf     = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
        iblSpecular    = prefilt * (kS_ibl * brdf.x + brdf.y) * ao * max(u_IBLIntensity, 0.0);
    }

    // ── Flat ambient floor ────────────────────────────────────────────────────
    vec3 kS_dir = FresnelSchlick(NdotV, F0);
    vec3 kD_dir = (vec3(1.0) - kS_dir) * (1.0 - metallic);

    vec3 sceneAmbient = kD_dir * albedo * u_AmbientColor * u_AmbientIntensity * ao;
    vec3 ambient = (u_HasIBL && (u_HasIrradianceMap || (u_HasPrefilteredMap && u_HasBRDFLUT)))
                 ? (iblDiffuse + iblSpecular)
                 : sceneAmbient;

    vec3 ambientFloor = kD_dir * albedo * vec3(max(u_AmbientFloorStrength, 0.0));
    ambient = max(ambient, ambientFloor);

    // ── Direct lights ─────────────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);

    // Directional
    if (u_HasDirectional != 0 && u_Directional.enabled != 0u)
    {
        vec3 L = normalize(-u_Directional.direction);
        vec3 radiance = u_Directional.color * u_Directional.intensity;
        Lo += CalculatePBRLight(N, V, L, radiance, albedo, F0, roughness, metallic);

        if (u_ReceiveShadow != 0)
        {
            float shadow = CalculateShadow(v_WorldPos, N, L, v_ViewDepth);
            shadow = min(shadow, clamp(u_MaxShadowOcclusion, 0.0, 1.0));
            Lo *= (1.0 - shadow);
        }
    }

    // Point lights
    int ptCount = clamp(u_NumPointLights, 0, 16);
    for (int i = 0; i < ptCount; ++i)
    {
        GpuPointLight light = u_PointLights[i];
        if (light.enabled == 0u) continue;

        vec3 toL = light.position - v_WorldPos;
        float d  = length(toL);
        if (d <= 0.0001) continue;

        float atten = DistanceAttenuation(light.radius, light.attnConstant, light.attnLinear, light.attnQuadratic, d);
        if (atten <= 0.0) continue;

        vec3 radiance = light.color * light.intensity * atten;
        Lo += CalculatePBRLight(N, V, toL / d, radiance, albedo, F0, roughness, metallic);
    }

    // Spot lights
    int spotCount = clamp(u_NumSpotLights, 0, 8);
    for (int i = 0; i < spotCount; ++i)
    {
        GpuSpotLight light = u_SpotLights[i];
        if (light.enabled == 0u) continue;

        vec3 toL = light.position - v_WorldPos;
        float d  = length(toL);
        if (d <= 0.0001) continue;

        vec3  L = toL / d;
        float atten = DistanceAttenuation(light.radius, light.attnConstant, light.attnLinear, light.attnQuadratic, d);
        if (atten <= 0.0) continue;

        vec3  spotDir = normalize(light.direction);
        float cosTheta = dot(spotDir, normalize(v_WorldPos - light.position));
        if (cosTheta <= light.outerCos) continue;

        float coneBlend = smoothstep(light.outerCos, light.innerCos, cosTheta);
        vec3  radiance  = light.color * light.intensity * atten * coneBlend;
        Lo += CalculatePBRLight(N, V, L, radiance, albedo, F0, roughness, metallic);
    }

    FragColor = vec4(ambient + Lo, 1.0);
}
