const int NUM_CASCADES = 4;

in vec3  v_Normal;
in vec3  v_WorldPos;
in vec2  v_UV;
in mat3  v_TBN;
in float v_ViewDepth;
in vec4  v_TerrainData; // x=materialZone, y=mountainMask, z=grassSuitable, w=treeSuitable
in float v_RockSuitability;

layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

#include "light_block.glsl"
#include "pbr_helpers.glsl"

const int DEBUG_OFF           = 0;
const int DEBUG_HEIGHTMAP     = 1;
const int DEBUG_SLOPE         = 2;
const int DEBUG_NORMALS       = 3;
const int DEBUG_MATERIAL_ZONE = 4;
const int DEBUG_MOUNTAIN_MASK = 5;
const int DEBUG_GRASS_MASK    = 6;
const int DEBUG_TREE_MASK     = 7;
const int DEBUG_ROCK_MASK     = 8;

uniform int u_DebugView = DEBUG_OFF;

uniform sampler2DArray u_CascadeShadowMaps;
uniform mat4  u_CascadeViewProj[NUM_CASCADES];
uniform float u_CascadeSplits[NUM_CASCADES];
uniform int   u_PCFRadius          = 1;
uniform int   u_ReceiveShadow      = 1;
uniform float u_MaxShadowOcclusion = 0.75;

uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilteredMap;
uniform sampler2D   u_BRDFLUT;
uniform bool  u_HasIBL            = false;
uniform bool  u_HasIrradianceMap  = false;
uniform bool  u_HasPrefilteredMap = false;
uniform bool  u_HasBRDFLUT        = false;
uniform float u_IBLIntensity      = 1.0;
uniform float u_AmbientFloorStrength = 0.18;

// Zone PBR scalars — roughness/metallic only; albedo comes from TerrainPalette.
uniform float u_ZoneRoughness[7];
uniform float u_ZoneMetallic[7];

uniform float u_HeightScale = 80.0;

// Atmosphere
uniform float u_FogDensity = 0.0015;
uniform vec3  u_FogColor   = vec3(0.58, 0.65, 0.78);

out vec4 FragColor;

// ─── Cascade shadow ───────────────────────────────────────────────────────────

int SelectCascade(float viewDepth)
{
    for (int i = 0; i < NUM_CASCADES; ++i)
        if (viewDepth < u_CascadeSplits[i]) return i;
    return NUM_CASCADES - 1;
}

float SampleCascade(int cascade, vec3 worldPos, vec3 normal, float bias)
{
    float nos   = u_Directional.normalBias * (1.0 + float(cascade) * 0.5);
    vec4 lsPos  = u_CascadeViewProj[cascade] * vec4(worldPos + normal * nos, 1.0);
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
    float base = u_Directional.depthBias;
    return max(base * u_Directional.slopeBias * slope, base * 0.3) * (1.0 + float(cascade) * 0.75);
}

const float CASCADE_EDGE_BLEND_START = 0.65;
const float CASCADE_BLEND_FRACTION   = 0.85;

float CascadeEdgeBlend(int cascade, vec3 worldPos, vec3 normal)
{
    float nos  = u_Directional.normalBias * (1.0 + float(cascade) * 0.5);
    vec4 lsPos = u_CascadeViewProj[cascade] * vec4(worldPos + normal * nos, 1.0);
    vec3 ndc   = lsPos.xyz / lsPos.w;
    float maxC = max(max(abs(ndc.x), abs(ndc.y)), abs(ndc.z));
    return clamp((maxC - CASCADE_EDGE_BLEND_START) / max(1.0 - CASCADE_EDGE_BLEND_START, 0.0001), 0.0, 1.0);
}

float CalculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, float viewDepth)
{
    int cascade  = SelectCascade(viewDepth);
    float slope  = 1.0 - max(dot(normal, lightDir), 0.0);
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
        float ef         = CascadeEdgeBlend(cascade, worldPos, normal);
        float splitNear  = (cascade == 0) ? 0.0 : u_CascadeSplits[cascade - 1];
        float splitFar   = u_CascadeSplits[cascade];
        float blendStart = mix(splitFar, splitNear, CASCADE_BLEND_FRACTION);
        float df = clamp((viewDepth - blendStart) / max(splitFar - blendStart, 0.0001), 0.0, 1.0);
        float t  = max(ef, df); t = t * t * (3.0 - 2.0 * t);
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

// ─── PBR ─────────────────────────────────────────────────────────────────────

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculatePBRLight(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, vec3 F0, float roughness, float metallic)
{
    vec3  H     = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotV <= 0.0 || NdotL <= 0.0) return vec3(0.0);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    vec3 kD       = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ─── Zone PBR params ─────────────────────────────────────────────────────────

void ZoneWeights(float zoneF, out int zA, out int zB, out float t)
{
    zA = clamp(int(floor(zoneF)), 0, 6);
    zB = clamp(int(ceil(zoneF)),  0, 6);
    t  = fract(zoneF);
    t  = t * t * (3.0 - 2.0 * t);
}

float TerrainRoughness(float zoneF)
{
    int a, b; float t; ZoneWeights(zoneF, a, b, t);
    return mix(u_ZoneRoughness[a], u_ZoneRoughness[b], t);
}

float TerrainMetallic(float zoneF)
{
    int a, b; float t; ZoneWeights(zoneF, a, b, t);
    return mix(u_ZoneMetallic[a], u_ZoneMetallic[b], t);
}

// ─── Noise ───────────────────────────────────────────────────────────────────

float hash21(vec2 p)
{
    p  = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f      = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i),             hash21(i + vec2(1.0, 0.0)), f.x),
               mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}

// ─── Terrain palette ─────────────────────────────────────────────────────────
// All zone transitions are perturbed by ±0.03 low-frequency world-space noise
// so boundaries never appear as clean horizontal contour lines.

vec3 TerrainPalette(float h01, float slope, vec3 worldPos, vec3 N, vec3 V)
{
    vec2  xz        = worldPos.xz;
    // nBoundary offsets every smoothstep threshold — breaks zone contour lines.
    float nBoundary = (valueNoise(xz * 0.012) - 0.5) * 0.06;
    float nForest   =  valueNoise(xz * 0.022);   // 0..1, breaks canopy uniformity
    float nGrass    =  valueNoise(xz * 0.018);   // 0..1, grass micro-variation

    float hN = h01 + nBoundary;  // noise-perturbed height for all threshold lookups

    // ── DeepWater: depth cue, near-surface slightly lighter ───────────────────
    vec3 colDeep = mix(vec3(0.010, 0.030, 0.155),
                       vec3(0.022, 0.080, 0.255),
                       smoothstep(0.000, 0.025, h01));

    // ── ShallowWater: bright teal, sand bleed in shallowest areas, Fresnel ────
    float fresnel    = pow(clamp(1.0 - dot(N, V), 0.0, 1.0), 4.0);
    vec3 shallowBase = mix(vec3(0.52, 0.50, 0.34),
                           vec3(0.038, 0.210, 0.420),
                           smoothstep(0.025, 0.085, h01));
    vec3 colShallow  = mix(shallowBase, vec3(0.72, 0.88, 0.95), fresnel * 0.26);

    // ── Sand: pale ash / dry riverbed, slope drives warm–cool shift ───────────
    vec3 colSand = mix(vec3(0.74, 0.70, 0.60),   // flat: cooler ash
                       vec3(0.72, 0.60, 0.41),    // angled: warmer earth
                       smoothstep(0.04, 0.26, slope));

    // ── Grass: lush sheltered → dry yellowed on high/exposed ground ───────────
    float dryness = clamp(
          smoothstep(0.14, 0.52, h01)
        + smoothstep(0.09, 0.26, slope) * 0.45
        + (nGrass - 0.5) * 0.18,
        0.0, 1.0);
    vec3 colGrass = mix(vec3(0.10, 0.36, 0.06),   // lush deep green
                        vec3(0.42, 0.46, 0.14),    // dry yellowed
                        dryness);

    // ── Forest: dark blue-green; noise breaks canopy into patches ─────────────
    vec3 colForest = mix(vec3(0.038, 0.150, 0.058),
                         vec3(0.062, 0.230, 0.090),
                         nForest * 0.55);

    // ── Rock: dark flat base → warm lighter tone on steep angled faces ─────────
    vec3 rockBase = mix(vec3(0.265, 0.235, 0.205),   // lower: iron-stained
                        vec3(0.310, 0.270, 0.235),    // higher: paler granite
                        smoothstep(0.55, 0.84, h01));
    vec3 colRock  = mix(rockBase,
                        vec3(0.500, 0.440, 0.355),    // steep: warm buff
                        smoothstep(0.18, 0.68, slope));

    // ── Snow: cool blue-tint on flat, warm cream on steep; fades to rock cream ─
    vec3 snowBase = mix(vec3(0.78, 0.84, 0.96),       // flat: cool blue tint
                        vec3(0.82, 0.80, 0.76),        // steep: warm cream
                        smoothstep(0.10, 0.42, slope));
    // Lower snow/rock boundary: blend toward cream where snow meets rock
    float snowRockEdge = 1.0 - smoothstep(0.80, 0.92, h01);
    vec3 colSnow = mix(snowBase, vec3(0.86, 0.82, 0.74), snowRockEdge * 0.38);

    // ── Smooth zone weights — hN perturbs every transition ────────────────────
    float wDeep    = 1.0 - smoothstep(0.010, 0.040, hN);

    float wShallow = smoothstep(0.010, 0.050, hN)
                   * (1.0 - smoothstep(0.050, 0.095, hN));

    float wSand    = smoothstep(0.050, 0.110, hN)
                   * (1.0 - smoothstep(0.110, 0.180, hN))
                   * (1.0 - smoothstep(0.05, 0.25, slope));

    float wGrass   = smoothstep(0.075, 0.130, hN)
                   * (1.0 - smoothstep(0.480, 0.620, hN))
                   * (1.0 - smoothstep(0.15, 0.38, slope));

    float wForest  = smoothstep(0.180, 0.300, hN)
                   * (1.0 - smoothstep(0.540, 0.720, hN))
                   * (1.0 - smoothstep(0.21, 0.52, slope));

    float wRock    = max(smoothstep(0.42, 0.66, slope),
                         smoothstep(0.54, 0.86, hN));

    float wSnow    = smoothstep(0.850, 0.960, hN)
                   * (1.0 - smoothstep(0.23, 0.44, slope));

    float wTotal = max(wDeep + wShallow + wSand + wGrass + wForest + wRock + wSnow, 0.001);

    return (colDeep    * wDeep
          + colShallow * wShallow
          + colSand    * wSand
          + colGrass   * wGrass
          + colForest  * wForest
          + colRock    * wRock
          + colSnow    * wSnow) / wTotal;
}

// ─── Main ────────────────────────────────────────────────────────────────────

void main()
{
    float zoneF    = v_TerrainData.x;
    float mountMask = v_TerrainData.y;
    float grassM   = v_TerrainData.z;
    float treeM    = v_TerrainData.w;
    float rockM    = v_RockSuitability;

    vec3  nrm   = normalize(v_Normal);
    float slope = clamp(1.0 - nrm.y, 0.0, 1.0);
    float h01   = clamp(v_WorldPos.y / max(u_HeightScale, 0.0001), 0.0, 1.0);

    // ── Debug views ──────────────────────────────────────────────────────────
    if (u_DebugView == DEBUG_HEIGHTMAP)  { FragColor = vec4(vec3(h01), 1.0); return; }
    if (u_DebugView == DEBUG_NORMALS)    { FragColor = vec4(nrm * 0.5 + 0.5, 1.0); return; }
    if (u_DebugView == DEBUG_SLOPE)      { FragColor = vec4(vec3(slope), 1.0); return; }
    if (u_DebugView == DEBUG_MOUNTAIN_MASK) { FragColor = vec4(vec3(mountMask), 1.0); return; }
    if (u_DebugView == DEBUG_GRASS_MASK)    { FragColor = vec4(vec3(grassM), 1.0); return; }
    if (u_DebugView == DEBUG_TREE_MASK)     { FragColor = vec4(vec3(treeM), 1.0); return; }

    if (u_DebugView == DEBUG_MATERIAL_ZONE)
    {
        const vec3 zoneColors[7] = vec3[7](
            vec3(0.00, 0.05, 0.35), vec3(0.00, 0.40, 0.55), vec3(0.87, 0.77, 0.54),
            vec3(0.35, 0.75, 0.15), vec3(0.08, 0.38, 0.08), vec3(0.48, 0.44, 0.40),
            vec3(0.93, 0.95, 0.97));
        int a, b; float t; ZoneWeights(zoneF, a, b, t);
        FragColor = vec4(mix(zoneColors[a], zoneColors[b], t), 1.0);
        return;
    }
    if (u_DebugView == DEBUG_ROCK_MASK)
    {
        FragColor = vec4(vec3(clamp(rockM, 0.0, 1.0)), 1.0);
        return;
    }

    vec3 N = nrm;
    vec3 V = normalize(cameraPos - v_WorldPos);

    // ── Material ──────────────────────────────────────────────────────────────
    vec3  albedo    = TerrainPalette(h01, slope, v_WorldPos, N, V);
    float roughness = TerrainRoughness(zoneF);
    float metallic  = TerrainMetallic(zoneF);
    float ao        = 1.0;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── IBL ──────────────────────────────────────────────────────────────────
    float NdotV  = max(dot(N, V), 0.0);
    vec3 kS_ibl  = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD_ibl  = (vec3(1.0) - kS_ibl) * (1.0 - metallic);

    vec3 iblDiffuse  = vec3(0.0);
    vec3 iblSpecular = vec3(0.0);
    if (u_HasIBL && u_HasIrradianceMap)
    {
        vec3 irradiance = texture(u_IrradianceMap, N).rgb;
        iblDiffuse = kD_ibl * irradiance * albedo * ao * max(u_IBLIntensity, 0.0);
    }
    if (u_HasIBL && u_HasPrefilteredMap && u_HasBRDFLUT)
    {
        vec3  R        = reflect(-V, N);
        float mipCount = float(textureQueryLevels(u_PrefilteredMap));
        vec3  prefilt  = textureLod(u_PrefilteredMap, R, roughness * (mipCount - 1.0)).rgb;
        vec2  brdf     = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
        iblSpecular    = prefilt * (kS_ibl * brdf.x + brdf.y) * ao * max(u_IBLIntensity, 0.0);
    }

    // ── Ambient ───────────────────────────────────────────────────────────────
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

    if (u_HasDirectional != 0 && u_Directional.enabled != 0u)
    {
        vec3 L        = normalize(-u_Directional.direction);
        vec3 radiance = u_Directional.color * u_Directional.intensity;
        Lo += CalculatePBRLight(N, V, L, radiance, albedo, F0, roughness, metallic);
        if (u_ReceiveShadow != 0)
        {
            float shadow = CalculateShadow(v_WorldPos, N, L, v_ViewDepth);
            Lo *= (1.0 - min(shadow, clamp(u_MaxShadowOcclusion, 0.0, 1.0)));
        }
    }

    int ptCount = clamp(u_NumPointLights, 0, 16);
    for (int i = 0; i < ptCount; ++i)
    {
        GpuPointLight light = u_PointLights[i];
        if (light.enabled == 0u) continue;
        vec3  toL   = light.position - v_WorldPos;
        float d     = length(toL);
        if (d <= 0.0001) continue;
        float atten = DistanceAttenuation(light.radius, light.attnConstant, light.attnLinear, light.attnQuadratic, d);
        if (atten <= 0.0) continue;
        Lo += CalculatePBRLight(N, V, toL / d, light.color * light.intensity * atten, albedo, F0, roughness, metallic);
    }

    int spotCount = clamp(u_NumSpotLights, 0, 8);
    for (int i = 0; i < spotCount; ++i)
    {
        GpuSpotLight light = u_SpotLights[i];
        if (light.enabled == 0u) continue;
        vec3  toL   = light.position - v_WorldPos;
        float d     = length(toL);
        if (d <= 0.0001) continue;
        float atten = DistanceAttenuation(light.radius, light.attnConstant, light.attnLinear, light.attnQuadratic, d);
        if (atten <= 0.0) continue;
        vec3  spotDir  = normalize(light.direction);
        float cosTheta = dot(spotDir, normalize(v_WorldPos - light.position));
        if (cosTheta <= light.outerCos) continue;
        float cone    = smoothstep(light.outerCos, light.innerCos, cosTheta);
        Lo += CalculatePBRLight(N, V, toL / d, light.color * light.intensity * atten * cone, albedo, F0, roughness, metallic);
    }

    // ── Atmosphere: exponential distance fog ──────────────────────────────────
    vec3  lit       = ambient + Lo;
    float fogDist   = length(v_WorldPos - cameraPos);
    float fogFactor = clamp(1.0 - exp(-fogDist * u_FogDensity), 0.0, 1.0);

    FragColor = vec4(mix(lit, u_FogColor, fogFactor), 1.0);
}
