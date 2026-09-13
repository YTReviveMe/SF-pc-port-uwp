#if defined(RENDERER_OGL)

namespace {

struct PsyXVolumetricResources {
  GLuint program = 0;
  GLuint framebuffer = 0;
  GLuint vertexArray = 0;
  GLuint densityAtlas = 0;
  GLint sceneDepth = -1;
  GLint densityAtlasSampler = -1;
  GLint densityAtlasReady = -1;
  GLint targetSize = -1;
  GLint viewport = -1;
  GLint view = -1;
  GLint presentationScale = -1;
  GLint depthProjection = -1;
  GLint centerKind = -1;
  GLint radiusEmission = -1;
  GLint colorDensity = -1;
  GLint phaseSeed = -1;
  GLint flags = -1;
  GLint axisX = -1;
  GLint axisY = -1;
  GLint axisZ = -1;
  bool available = false;
  bool frameReady = false;
};

PsyXVolumetricResources g_volumetrics;

constexpr const char *kVolumetricShader = R"PSYX(
#ifdef VERTEX
void main() {
  vec2 position = gl_VertexID == 0 ? vec2(0.0, 0.0)
                : gl_VertexID == 1 ? vec2(2.0, 0.0)
                                   : vec2(0.0, 2.0);
  gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
#else
uniform sampler2D u_scene_depth;
uniform sampler2D u_density_atlas;
uniform float u_density_atlas_ready;
uniform vec2 u_target_size;
uniform vec4 u_viewport;
uniform vec4 u_view;
uniform vec2 u_presentation_scale;
uniform vec2 u_depth_projection;
uniform vec4 u_center_kind;
uniform vec4 u_radius_emission;
uniform vec4 u_color_density;
uniform vec2 u_phase_seed;
uniform int u_flags;
uniform vec3 u_axis_x;
uniform vec3 u_axis_y;
uniform vec3 u_axis_z;

float volumeHash(vec2 point) {
  vec3 value = fract(vec3(point.xyx) * 0.1031);
  value += dot(value, value.yzx + 33.33);
  return fract((value.x + value.y) * value.z);
}

float volumeNoise(vec2 point) {
  vec2 cell = floor(point);
  vec2 local = fract(point);
  local = local * local * (3.0 - 2.0 * local);
  float n00 = volumeHash(cell);
  float n10 = volumeHash(cell + vec2(1.0, 0.0));
  float n01 = volumeHash(cell + vec2(0.0, 1.0));
  float n11 = volumeHash(cell + vec2(1.0, 1.0));
  return mix(mix(n00, n10, local.x), mix(n01, n11, local.x), local.y);
}

float volumeField(vec3 point, vec2 flow) {
  float coarse = volumeNoise(
      point.xy + vec2(0.61 * point.z, -0.37 * point.z) + flow);
  float detail = volumeNoise(
      point.yz * 2.03 + vec2(0.47 * point.x, 0.29 * point.x) -
      flow * 0.57 + 7.13);
  return coarse * 0.68 + detail * 0.32;
}

float volumeAtlasDensity(vec3 point, float kind, float seed) {
  if (u_density_atlas_ready < 0.5)
    return -1.0;

  vec2 card = point.xy;
  // Stable mirroring varies repeated effects without turning a rising flame
  // sideways. Lamp halos remain perfectly steady and symmetric.
  if (kind < 3.5 && mod(floor(seed), 2.0) > 0.5)
    card.x = -card.x;
  vec2 uv = vec2(card.x * 0.5 + 0.5, 0.5 - card.y * 0.5);
  if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
    return 0.0;

  float tile = floor(clamp(kind + 0.5, 0.0, 4.0));
  float column = mod(tile, 4.0);
  // Source row zero is the atlas top. Upload data is flipped once: the four
  // generic effect cells occupy OpenGL row two and the halo occupies row one.
  float row = tile < 3.5 ? 2.0 : 1.0;
  vec2 insetUv = mix(vec2(0.02), vec2(0.98), uv);
  vec2 atlasUv = (vec2(column, row) + insetUv) / vec2(4.0, 3.0);
  vec4 slices = texture2D(u_density_atlas, atlasUv);
  float slice = clamp((point.z + 0.75) * 2.0, 0.0, 3.0);
  if (slice < 1.0)
    return mix(slices.r, slices.g, slice);
  if (slice < 2.0)
    return mix(slices.g, slices.b, slice - 1.0);
  return mix(slices.b, slices.a, slice - 2.0);
}

float packedRetailFrameDensity(vec3 point, float phase, float frameCount,
                               float firstColumn, float row) {
  if (u_density_atlas_ready < 0.5)
    return -1.0;

  vec2 uv = vec2(point.x * 0.5 + 0.5, 0.5 - point.y * 0.5);
  if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
    return 0.0;

  float frame =
      clamp(floor(phase * frameCount + 0.001), 0.0, frameCount - 1.0);
  float group = floor(frame * 0.25);
  float channel = frame - group * 4.0;
  // Preserve the complete uncropped retail card and its authored pivot. Half
  // a cell texel is sufficient to avoid filtering into the adjacent group.
  vec2 insetUv = mix(vec2(0.00390625), vec2(0.99609375), uv);
  vec2 atlasUv = (vec2(firstColumn + group, row) + insetUv) /
                 vec2(4.0, 3.0);
  vec4 frames = texture2D(u_density_atlas, atlasUv);
  if (channel < 0.5)
    return frames.r;
  if (channel < 1.5)
    return frames.g;
  if (channel < 2.5)
    return frames.b;
  return frames.a;
}

float retailExplFrameDensity(vec3 point, float phase) {
  // Source row one (OpenGL row one), columns one through three: EXPL000..011.
  return packedRetailFrameDensity(point, phase, 12.0, 1.0, 1.0);
}

float retailFireFrameDensity(vec3 point, float phase) {
  // Source row two (OpenGL row zero), all four columns: FIRE000..015.
  return packedRetailFrameDensity(point, phase, 16.0, 0.0, 0.0);
}

float volumeParticleLobe(vec3 point, vec3 center, vec3 extent) {
  vec3 local = (point - center) / max(extent, vec3(0.05));
  float radiusSquared = dot(local, local);
  return exp2(-2.4 * radiusSquared) *
         (1.0 - smoothstep(0.72, 1.18, radiusSquared));
}

float volumeSoftLobe(vec3 point, float kind, float seed, vec2 flow) {
  float edge = pow(max(0.0, 1.0 - dot(point, point)),
                   kind < 2.5 ? 0.36 : 0.24);
  float noise = volumeField(
      point * (kind < 2.5 ? 1.78 : 1.12), flow);
  float procedural = edge * mix(
      0.44, 1.27, smoothstep(0.20, 0.80, noise + edge * 0.22));
  float authored = volumeAtlasDensity(point, kind, seed);
  if (authored < 0.0)
    return procedural;
  float authoredBody =
      edge * mix(0.20, 1.36, smoothstep(0.015, 0.78, authored));
  return mix(procedural, authoredBody, 0.78);
}

float volumeClusterBody(vec3 point, float kind, float seed, vec2 flow) {
  float variation = volumeHash(vec2(seed * 0.013 + 1.7,
                                    seed * 0.021 + 9.1));
  float slowMotion = kind < 2.5 ? 1.0 : 0.28;
  vec3 drift = vec3(sin(flow.x * 0.31 + seed * 0.017),
                    -sin(flow.y * 0.23 + seed * 0.011) * 0.45,
                    cos(flow.y * 0.27 + seed * 0.019)) *
               (0.075 * slowMotion);
  float center = volumeSoftLobe(
      (point - drift * 0.45) / vec3(0.78, 0.80, 0.78),
      kind, seed, flow) * 0.78;
  vec3 firstOffset =
      vec3(-0.27, -0.10 + variation * 0.12, 0.12) + drift;
  float first = volumeSoftLobe(
      (point - firstOffset) / vec3(0.78, 0.76, 0.80),
      kind, seed + 17.0, flow + vec2(2.3, -1.7)) * 0.72;
  vec3 secondOffset =
      vec3(0.29, 0.13 - variation * 0.10, -0.10) - drift * 0.72;
  float second = volumeSoftLobe(
      (point - secondOffset) / vec3(0.72, 0.69, 0.75),
      kind, seed + 31.0, flow + vec2(-1.4, 2.1)) * 0.66;
  return clamp(center + first + second, 0.0, 1.65);
}
)PSYX"
R"PSYX(

float sceneCameraDepth() {
  float stored = texture2D(u_scene_depth, gl_FragCoord.xy / u_target_size).r;
  float ndc = stored * 2.0 - 1.0;
  float denominator = ndc - u_depth_projection.x;
  if (abs(denominator) < 0.000001)
    return 1000.0;
  return clamp(u_depth_projection.y / denominator, 0.25, 1000.0);
}

void main() {
  vec2 viewportUv = (gl_FragCoord.xy - u_viewport.xy) / u_viewport.zw;
  vec2 ndc = viewportUv * 2.0 - 1.0;
  vec2 scale = max(u_presentation_scale, vec2(0.0001));
  vec3 ray = vec3(ndc.x * u_view.x / (2.0 * u_view.z * scale.x),
                 -ndc.y * u_view.y / (2.0 * u_view.z * scale.y), 1.0);
  vec3 center = u_center_kind.xyz;
  vec3 radius = max(u_radius_emission.xyz, vec3(0.0001));
  vec3 localRay = vec3(dot(ray, u_axis_x), dot(ray, u_axis_y),
                       dot(ray, u_axis_z));
  vec3 localCenter = vec3(dot(center, u_axis_x), dot(center, u_axis_y),
                          dot(center, u_axis_z));
  vec3 scaledRay = localRay / radius;
  vec3 scaledCenter = localCenter / radius;
  float a = dot(scaledRay, scaledRay);
  float b = dot(scaledRay, scaledCenter);
  float c = dot(scaledCenter, scaledCenter) - 1.0;
  float discriminant = b * b - a * c;
  if (discriminant <= 0.0)
    discard;

  float root = sqrt(discriminant);
  float first = (b - root) / a;
  float second = (b + root) / a;
  float startDepth = max(first, 0.25);
  // Lamp housings commonly intersect the authored corona. A small halo-only
  // bias keeps the emissive center visible without weakening other occlusion.
  float cameraDepthRadius = abs(u_axis_x.z) * radius.x +
                            abs(u_axis_y.z) * radius.y +
                            abs(u_axis_z.z) * radius.z;
  float sceneDepthBias =
      u_center_kind.w > 3.5 && u_center_kind.w < 4.5
          ? cameraDepthRadius * 0.35 : 0.0;
  float sceneDepth = sceneCameraDepth();
  float endDepth = min(second, sceneDepth + sceneDepthBias - 0.002);
  if (endDepth <= startDepth)
    discard;

  float segment = endDepth - startDepth;
  float rayLength = length(ray);
  float seed = u_phase_seed.y;
  float time = u_view.w + seed * 0.0007;
  float life = clamp(u_phase_seed.x, 0.0, 1.0);
  float normalizedSegment = segment * rayLength /
      max((radius.x + radius.y + radius.z) * 0.666667, 0.0001);
  float kind = u_center_kind.w;
  vec3 tint = max(u_color_density.rgb, vec3(0.0));
  float softDepthWidth = clamp(cameraDepthRadius * 0.35, 0.02, 0.50);
  // The volume owns the complete lamp particle: a compact stable hot point
  // surrounded by a restrained authored corona.
  if (kind > 3.5) {
    float middle = mix(startDepth, endDepth, 0.52);
    vec3 localDelta = ray * middle - center;
    vec3 local = vec3(dot(localDelta, u_axis_x),
                      dot(localDelta, u_axis_y),
                      dot(localDelta, u_axis_z)) / radius;
    float authored = volumeAtlasDensity(local, kind, seed);
    float radialSquared = dot(local.xy, local.xy);
    float volumeEdge = (1.0 - smoothstep(0.45, 1.0, dot(local, local))) *
                       smoothstep(0.0, 0.10, normalizedSegment);
    float outerGlow = exp2(-4.6 * radialSquared) * volumeEdge;
    float innerGlow = exp2(-18.0 * radialSquared) * volumeEdge;
    float hollowCore = smoothstep(0.42, 0.62, sqrt(radialSquared));
    float emission = max(u_radius_emission.w, 1.0);
    float authoredGlow = authored < 0.0
        ? 1.0
        : mix(0.18, 1.42, smoothstep(0.015, 0.72, authored));
    float hotPoint = exp2(-72.0 * radialSquared) * volumeEdge;
    float authoredHot = authored < 0.0
        ? 0.72
        : mix(0.25, 1.0, smoothstep(0.48, 0.96, authored));
    vec3 hotColor = mix(tint, vec3(1.0, 0.95, 0.76), 0.72);
    vec3 corona =
        tint * (outerGlow * 0.20 + innerGlow * 0.16) * authoredGlow;
    vec3 radiance = corona * mix(0.72, 1.0, hollowCore) +
                    hotColor * hotPoint * authoredHot * 0.64;
    fragColor = vec4(radiance * emission, 0.0);
    return;
  }

  // Ordinary fire is ray-marched as an irregular bed plus
  // four independently evolving three-dimensional tongues. There is no
  // single tapered shell: every tongue owns its centerline, reach and curl,
  // so the silhouette cannot collapse into a camera-facing cone.
  // Only an explicitly authored flame jet keeps the directional path below.
  if (kind < 0.5 && (u_flags & 2) == 0) {
    const int sampleCount = 12;
    float normalizedStep = normalizedSegment / float(sampleCount);
    float transmission = 1.0;
    vec3 radiance = vec3(0.0);
    float seed0 = volumeHash(vec2(seed * 0.013 + 2.7,
                                  seed * 0.021 + 5.1));
    float seed1 = volumeHash(vec2(seed * 0.019 + 7.3,
                                  seed * 0.031 + 1.9));
    float seed2 = volumeHash(vec2(seed * 0.027 + 4.1,
                                  seed * 0.017 + 8.7));
    float seed3 = volumeHash(vec2(seed * 0.037 + 9.9,
                                  seed * 0.023 + 3.3));
    // Preserve the rapid retail cadence while interpolating it at display FPS:
    // major lobes recycle in roughly 0.35-0.70 seconds and the finer silhouette
    // receives a new smoothly blended density pattern every 50-100 ms.
    float flowTime = time * 4.8;
    float shimmerTime = time * 13.33;

    // Noise-driven reach avoids synchronized saw-tooth lifetimes. Each tip
    // breathes at a different, incommensurate rate while the base stays put.
    float reach0 = mix(0.72, 0.99, volumeNoise(vec2(
        flowTime * 0.43 + seed0 * 11.0, seed * 0.0031 + 1.2)));
    float reach1 = mix(0.53, 0.88, volumeNoise(vec2(
        flowTime * 0.61 + seed1 * 13.0, seed * 0.0047 + 5.6)));
    float reach2 = mix(0.60, 0.94, volumeNoise(vec2(
        -flowTime * 0.51 + seed2 * 17.0, seed * 0.0037 + 9.1)));
    float reach3 = mix(0.44, 0.82, volumeNoise(vec2(
        flowTime * 0.77 + seed3 * 19.0, seed * 0.0053 + 3.8)));

    // Advected parcels keep energy moving through the columns instead of
    // merely bending a static silhouette. Their cycles and trajectories are
    // deliberately decorrelated; they stay comfortably inside the analytic
    // envelope so its ellipsoid never becomes a visible surface.
    float parcelLife0 = fract(flowTime * 0.31 + seed0 * 0.83);
    float parcelLife1 = fract(flowTime * 0.43 + seed1 * 0.71 + 0.37);
    float parcelLife2 = fract(flowTime * 0.57 + seed2 * 0.67 + 0.68);
    float parcelFade0 = smoothstep(0.0, 0.12, parcelLife0) *
                        (1.0 - smoothstep(0.72, 1.0, parcelLife0));
    float parcelFade1 = smoothstep(0.0, 0.15, parcelLife1) *
                        (1.0 - smoothstep(0.76, 1.0, parcelLife1));
    float parcelFade2 = smoothstep(0.0, 0.10, parcelLife2) *
                        (1.0 - smoothstep(0.68, 1.0, parcelLife2));
    vec3 parcelCenter0 = vec3(
        -0.18 + parcelLife0 * 0.24 + sin(flowTime * 0.91 + seed0 * 8.0) * 0.10,
        mix(0.63, -0.61, parcelLife0),
        0.08 - parcelLife0 * 0.13 + cos(flowTime * 1.13 + seed1 * 9.0) * 0.09);
    vec3 parcelCenter1 = vec3(
        0.22 - parcelLife1 * 0.30 + cos(flowTime * 1.27 + seed2 * 7.0) * 0.11,
        mix(0.58, -0.55, parcelLife1),
        -0.14 + parcelLife1 * 0.25 + sin(flowTime * 0.79 + seed3 * 8.0) * 0.10);
    vec3 parcelCenter2 = vec3(
        -0.06 + sin(flowTime * 1.41 + parcelLife2 * 5.0 + seed3 * 6.0) * 0.15,
        mix(0.54, -0.48, parcelLife2),
        0.20 - parcelLife2 * 0.31 + cos(flowTime * 1.03 + seed0 * 11.0) * 0.09);
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
)PSYX"
R"PSYX(
      float sampleFraction =
          (float(sampleIndex) + 0.5) / float(sampleCount);
      float sampleDepth = mix(startDepth, endDepth, sampleFraction);
      vec3 sampleDelta = ray * sampleDepth - center;
      vec3 sampleLocal = vec3(dot(sampleDelta, u_axis_x),
                              dot(sampleDelta, u_axis_y),
                              dot(sampleDelta, u_axis_z)) / radius;
      // local Y = +1 at the anchored base and -1 at the flame tip.
      float height = clamp((1.0 - sampleLocal.y) * 0.5, 0.0, 1.0);
      vec2 crossSection = sampleLocal.xz;

      float slowWarp = volumeNoise(vec2(
          height * 3.17 + seed0 * 7.1,
          flowTime * 0.52 + seed1 * 9.3));
      float fastWarp = volumeNoise(vec2(
          height * 7.43 - seed2 * 6.7,
          -flowTime * 0.89 + seed3 * 12.1));
      float detail = volumeField(
          sampleLocal * vec3(3.15, 2.05, 3.55),
          vec2(seed * 0.011 + flowTime * 0.41,
               seed * 0.017 - flowTime * 0.63));
      vec2 sharedWarp = vec2(slowWarp - 0.5, fastWarp - 0.5) *
                        mix(0.055, 0.31, height);
      sharedWarp += vec2(
          sin(flowTime * 0.83 + height * 8.1 + seed0 * 6.2831853),
          cos(flowTime * 1.07 + height * 6.7 + seed1 * 6.2831853)) *
          mix(0.018, 0.105, height);

      // The low bed is a turbulent, asymmetric cluster rather than the
      // bottom of a continuously tapered cone.
      vec2 bedCenter = sharedWarp * 0.18 +
                       vec2((seed1 - 0.5) * 0.10,
                            (seed2 - 0.5) * 0.12);
      vec2 bedPoint = vec2((crossSection.x - bedCenter.x) * 0.88,
                           (crossSection.y - bedCenter.y) * 1.13);
      float bedRadius = 0.63 + (detail - 0.5) * 0.23 +
                        (slowWarp - 0.5) * 0.10;
      float bed = 1.0 - smoothstep(bedRadius * 0.78,
                                   bedRadius * 1.08,
                                   length(bedPoint));
      bed *= smoothstep(0.0, 0.045, height) *
             (1.0 - smoothstep(0.24, 0.44, height));

      vec2 center0 = vec2(-0.12 + height * 0.17,
                          0.02 - height * 0.09) +
                     sharedWarp * 0.92 +
                     vec2(sin(flowTime * 1.17 + height * 10.3 + seed0 * 9.0),
                          cos(flowTime * 0.71 + height * 8.8 + seed1 * 7.0)) *
                         mix(0.012, 0.115, height);
      vec2 center1 = vec2(0.25 - height * 0.12,
                          -0.16 + height * 0.20) -
                     sharedWarp.yx * 0.73 +
                     vec2(cos(flowTime * 0.91 + height * 7.1 + seed2 * 8.0),
                          sin(flowTime * 1.31 + height * 11.2 + seed1 * 5.0)) *
                         mix(0.018, 0.135, height);
      vec2 center2 = vec2(-0.29 + height * 0.22,
                          0.18 - height * 0.07) +
                     vec2(sharedWarp.x, -sharedWarp.y) * 0.81 +
                     vec2(sin(flowTime * 0.67 + height * 12.6 + seed3 * 6.0),
                          cos(flowTime * 1.43 + height * 6.4 + seed2 * 9.0)) *
                         mix(0.015, 0.125, height);
      vec2 center3 = vec2(0.08 + height * 0.09,
                          0.27 - height * 0.24) +
                     sharedWarp.yx * 0.62 +
                     vec2(cos(flowTime * 1.53 + height * 9.7 + seed1 * 11.0),
                          sin(flowTime * 0.79 + height * 13.1 + seed3 * 7.0)) *
                         mix(0.010, 0.105, height);

      float widthNoise = mix(0.76, 1.24,
                             smoothstep(0.10, 0.90, detail));
      // Fast independent constrictions split the columns into turbulent lobes;
      // their union never settles into one readable tapered cone.
      float pulse0 = mix(0.64, 1.34, volumeNoise(vec2(
          height * 6.3 - shimmerTime * 0.82 + seed0 * 9.0,
          seed1 * 13.0 + 1.7)));
      float pulse1 = mix(0.67, 1.31, volumeNoise(vec2(
          height * 7.1 - shimmerTime * 0.97 + seed1 * 11.0,
          seed2 * 15.0 + 4.2)));
      float pulse2 = mix(0.62, 1.36, volumeNoise(vec2(
          height * 5.7 - shimmerTime * 0.73 + seed2 * 13.0,
          seed3 * 17.0 + 7.6)));
      float pulse3 = mix(0.70, 1.28, volumeNoise(vec2(
          height * 8.2 - shimmerTime * 1.06 + seed3 * 7.0,
          seed0 * 19.0 + 2.9)));
      float width0 = mix(0.31, 0.075,
                         smoothstep(0.04, reach0, height)) * widthNoise * pulse0;
      float width1 = mix(0.27, 0.065,
                         smoothstep(0.07, reach1, height)) *
                     mix(0.82, 1.18, fastWarp) * pulse1;
      float width2 = mix(0.25, 0.060,
                         smoothstep(0.10, reach2, height)) *
                     mix(0.80, 1.20, slowWarp) * pulse2;
      float width3 = mix(0.22, 0.055,
                         smoothstep(0.14, reach3, height)) *
                     mix(0.86, 1.14, 1.0 - fastWarp) * pulse3;
      // Depth thickness keeps thin tips visible between ray samples and makes
      // the lobes hold their volume when the emitter is viewed side-on.
      vec2 delta0 = crossSection - center0;
      vec2 delta1 = crossSection - center1;
      vec2 delta2 = crossSection - center2;
      vec2 delta3 = crossSection - center3;
      float distance0 = length(vec2(delta0.x, delta0.y * 0.76));
      float distance1 = length(vec2(delta1.x, delta1.y * 0.81));
      float distance2 = length(vec2(delta2.x, delta2.y * 0.73));
      float distance3 = length(vec2(delta3.x, delta3.y * 0.84));
      float gate0 = smoothstep(0.015, 0.10, height) *
                    (1.0 - smoothstep(reach0 - 0.12, reach0, height));
      float gate1 = smoothstep(0.055, 0.15, height) *
                    (1.0 - smoothstep(reach1 - 0.11, reach1, height));
      float gate2 = smoothstep(0.085, 0.18, height) *
                    (1.0 - smoothstep(reach2 - 0.13, reach2, height));
      float gate3 = smoothstep(0.13, 0.23, height) *
                    (1.0 - smoothstep(reach3 - 0.10, reach3, height));
      float tongue0 = (1.0 - smoothstep(width0 * 0.64,
                                        width0 * 1.10, distance0)) * gate0;
      float tongue1 = (1.0 - smoothstep(width1 * 0.62,
                                        width1 * 1.12, distance1)) * gate1;
      float tongue2 = (1.0 - smoothstep(width2 * 0.66,
                                        width2 * 1.14, distance2)) * gate2;
      float tongue3 = (1.0 - smoothstep(width3 * 0.60,
                                        width3 * 1.08, distance3)) * gate3;
      float tongues = tongue0 + tongue1 * 0.92 + tongue2 * 0.84 +
                      tongue3 * 0.70;
      float parcel0 = volumeParticleLobe(
          sampleLocal, parcelCenter0,
          vec3(mix(0.30, 0.15, parcelLife0), 0.25,
               mix(0.27, 0.13, parcelLife0))) * parcelFade0;
      float parcel1 = volumeParticleLobe(
          sampleLocal, parcelCenter1,
          vec3(mix(0.27, 0.13, parcelLife1), 0.23,
               mix(0.31, 0.15, parcelLife1))) * parcelFade1;
      float parcel2 = volumeParticleLobe(
          sampleLocal, parcelCenter2,
          vec3(mix(0.25, 0.12, parcelLife2), 0.21,
               mix(0.24, 0.12, parcelLife2))) * parcelFade2;
      float parcels = parcel0 + parcel1 * 0.88 + parcel2 * 0.76;


      // Exact CFIRE overlays select the same high-resolution frame as their
      // still-visible retail quad. Other fire keeps the generic four-slice
      // density, so FIRE/EXPLOSION/PARK2 presentation is unchanged.
      bool retailExplFrame = (u_flags & 8) != 0;
      bool retailFireFrame = (u_flags & 16) != 0;
      bool retailFrame = retailExplFrame || retailFireFrame;
      float authored = retailExplFrame
          ? retailExplFrameDensity(sampleLocal, life)
          : retailFireFrame
                ? retailFireFrameDensity(sampleLocal, life)
                : volumeAtlasDensity(sampleLocal, kind, seed);
      float authoredModulation = authored < 0.0
          ? 1.0
          : retailFrame
                ? mix(0.10, 1.54, smoothstep(0.008, 0.76, authored))
                : mix(0.65, 1.38, smoothstep(0.015, 0.82, authored));
      float breakup = mix(0.47, 1.34,
                          smoothstep(0.12, 0.88,
                                     detail + (slowWarp - 0.5) * 0.22));
      float boundaryFade = 1.0 - smoothstep(
          0.70, 0.94, dot(sampleLocal, sampleLocal));
      float shape = clamp(bed * 0.92 + tongues * 1.02 + parcels * 0.88,
                          0.0, 1.92);
      shape *= authoredModulation * breakup * boundaryFade;
      float sceneFade = smoothstep(
          0.0, softDepthWidth,
          max(sceneDepth - sampleDepth, 0.0) * rayLength);
      float sampleTau = max(u_color_density.a, 0.0) * normalizedStep *
                        shape * sceneFade * 1.55;
      float stepTransmission = exp(-sampleTau);
      float absorbed = transmission * (1.0 - stepTransmission);

      float bedCore = bed * (1.0 - smoothstep(0.12, 0.48,
                                               length(bedPoint)));
      float tongueCore = max(
          tongue0 * (1.0 - smoothstep(width0 * 0.16,
                                      width0 * 0.62, distance0)),
          max(tongue1 * (1.0 - smoothstep(width1 * 0.14,
                                          width1 * 0.58, distance1)),
              max(tongue2 * (1.0 - smoothstep(width2 * 0.16,
                                              width2 * 0.60, distance2)),
                  tongue3 * (1.0 - smoothstep(width3 * 0.14,
                                              width3 * 0.56, distance3)))));
      float authoredHeat = authored < 0.0
          ? detail
          : smoothstep(0.10, 0.88, authored);
      float heat = clamp(bedCore * 0.98 + bed * 0.30 +
                         tongueCore * 0.94 + tongues * 0.19 +
                         parcels * 0.28 + (1.0 - height) * 0.24 +
                         detail * 0.08 + authoredHeat * 0.14, 0.0, 1.0);
      // Retail EXPL is mostly red/orange; only its compact hottest peak turns
      // pale yellow, and no stage needs a white alpha veil.
      vec3 sampleColor = mix(vec3(0.195, 0.076, 0.045),
                             vec3(0.606, 0.264, 0.090),
                             smoothstep(0.02, 0.30, heat));
      sampleColor = mix(sampleColor, vec3(0.928, 0.575, 0.162),
                        smoothstep(0.25, 0.64, heat));
      sampleColor = mix(sampleColor, vec3(1.00, 0.81, 0.29),
                        smoothstep(0.58, 0.88, heat));
      sampleColor = mix(sampleColor, vec3(1.00, 0.91, 0.06),
                        smoothstep(0.94, 1.0, heat));
      sampleColor *= mix(vec3(1.0), tint, 0.28);
      radiance += sampleColor * absorbed * mix(1.08, 1.42, heat);
      transmission *= stepTransmission;
    }
    fragColor = vec4(radiance * u_radius_emission.w * 1.70, 0.0);
    return;
  }
)PSYX"
R"PSYX(
  if (kind < 0.5) {
    const int sampleCount = 6;
    float normalizedStep = normalizedSegment / float(sampleCount);
    float transmission = 1.0;
    vec3 radiance = vec3(0.0);
    float firstLife = fract(time * 0.18 + seed * 0.0017);
    float secondLife = fract(time * 0.16 + seed * 0.0023 + 0.47);
    float firstJitter = volumeHash(vec2(seed * 0.013, 3.7)) - 0.5;
    float secondJitter = volumeHash(vec2(seed * 0.019, 8.1)) - 0.5;
    vec3 firstCenter = vec3(firstJitter * 0.42,
                            mix(0.62, -0.68, firstLife),
                            secondJitter * 0.25);
    vec3 secondCenter = vec3(-secondJitter * 0.38,
                             mix(0.68, -0.62, secondLife),
                             -firstJitter * 0.28);
    float firstFade = smoothstep(0.0, 0.12, firstLife) *
                      (1.0 - smoothstep(0.78, 1.0, firstLife));
    float secondFade = smoothstep(0.0, 0.12, secondLife) *
                       (1.0 - smoothstep(0.78, 1.0, secondLife));
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
      float sampleFraction =
          (float(sampleIndex) + 0.5) / float(sampleCount);
      float sampleDepth = mix(startDepth, endDepth, sampleFraction);
      vec3 sampleDelta = ray * sampleDepth - center;
      vec3 sampleLocal = vec3(dot(sampleDelta, u_axis_x),
                              dot(sampleDelta, u_axis_y),
                              dot(sampleDelta, u_axis_z)) / radius;
      float authored = volumeAtlasDensity(sampleLocal, kind, seed);
      float up = clamp(0.5 - 0.5 * sampleLocal.y, 0.0, 1.0);
      float width = mix(0.96, 0.28, up);
      float noise = volumeField(
          sampleLocal * vec3(2.5, 1.8, 2.5),
          vec2(seed * 0.013, time * 0.72));
      float lateral = length(sampleLocal.xz) +
                      (noise - 0.5) * 0.22 * (0.4 + 0.6 * up);
      float shape =
          1.0 - smoothstep(width - 0.12, width + 0.10, lateral);
      float firstLobe = volumeParticleLobe(
          sampleLocal, firstCenter,
          vec3(mix(0.47, 0.23, firstLife), 0.48, 0.42)) * firstFade;
      float secondLobe = volumeParticleLobe(
          sampleLocal, secondCenter,
          vec3(mix(0.42, 0.21, secondLife), 0.44, 0.38)) * secondFade;
      float clusteredFlame =
          clamp(firstLobe + secondLobe * 0.82, 0.0, 1.0);
      shape *= mix(0.38, 1.34, clusteredFlame);
      shape *= (1.0 - smoothstep(0.76, 1.06, abs(sampleLocal.y))) *
               mix(1.0, 0.55, up);
      float temperature = clamp(
          1.12 - 0.72 * lateral / max(width, 0.10) - 0.25 * up +
              0.18 * (noise - 0.5),
          0.0, 1.0);
      float authoredShape = authored < 0.0
          ? mix(0.55, 1.20, noise)
          : mix(0.22, 1.38, smoothstep(0.02, 0.82, authored));
      if (authored >= 0.0)
        temperature =
            clamp(temperature + (authored - 0.42) * 0.28, 0.0, 1.0);
      float sceneFade = smoothstep(
          0.0, softDepthWidth,
          max(sceneDepth - sampleDepth, 0.0) * rayLength);
      float sampleTau = max(u_color_density.a, 0.0) * normalizedStep *
                        shape * authoredShape * sceneFade;
      float stepTransmission = exp(-sampleTau);
      float absorbed = transmission * (1.0 - stepTransmission);
      vec3 sampleColor = mix(
          vec3(0.55, 0.015, 0.001), vec3(1.0, 0.20, 0.01),
          smoothstep(0.0, 0.34, temperature));
      sampleColor = mix(sampleColor, vec3(1.0, 0.78, 0.18),
                        smoothstep(0.32, 0.72, temperature));
      sampleColor = mix(sampleColor, vec3(1.0, 0.98, 0.82),
                        smoothstep(0.72, 1.0, temperature));
      sampleColor *= mix(vec3(1.0), tint, 0.64);
      radiance += sampleColor * absorbed;
      transmission *= stepTransmission;
    }
    fragColor = vec4(radiance * u_radius_emission.w, 0.0);
    return;
  } else if (kind < 1.5) {
    const int sampleCount = 6;
    float normalizedStep = normalizedSegment / float(sampleCount);
    float transmission = 1.0;
    vec3 radiance = vec3(0.0);
    float shellRadius = mix(0.28, 0.82, smoothstep(0.0, 1.0, life));
    float burstAngle =
        volumeHash(vec2(seed * 0.017 + 2.0, seed * 0.029 + 5.0)) * 6.2831853;
    vec3 burstDirection = normalize(vec3(
        cos(burstAngle),
        (volumeHash(vec2(seed * 0.031, 7.0)) - 0.5) * 0.72,
        sin(burstAngle)));
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
      float sampleFraction =
          (float(sampleIndex) + 0.5) / float(sampleCount);
      float sampleDepth = mix(startDepth, endDepth, sampleFraction);
      vec3 sampleDelta = ray * sampleDepth - center;
      vec3 sampleLocal = vec3(dot(sampleDelta, u_axis_x),
                              dot(sampleDelta, u_axis_y),
                              dot(sampleDelta, u_axis_z)) / radius;
      float authored = volumeAtlasDensity(sampleLocal, kind, seed);
      float radiusFromCenter = length(sampleLocal);
      float noise = volumeField(sampleLocal * 2.6,
                                vec2(seed * 0.011 + time * 0.08,
                                     seed * 0.019 - time * 0.05));
      float shell = exp2(-50.0 *
                         (radiusFromCenter - shellRadius) *
                         (radiusFromCenter - shellRadius));
      float core = exp2(-3.5 * radiusFromCenter * radiusFromCenter) *
                   (1.0 - smoothstep(0.35, 0.85, life));
      float anisotropy = 1.0 + (noise - 0.5) * 0.20;
      float firstBurst = volumeParticleLobe(
          sampleLocal, burstDirection * shellRadius * 0.46,
          vec3(0.72, 0.68, 0.72) * anisotropy);
      float secondBurst = volumeParticleLobe(
          sampleLocal, -burstDirection * mix(0.08, 0.24, life),
          vec3(0.58, 0.54, 0.60));
      float authoredShape = authored < 0.0
          ? mix(0.70, 1.25, noise)
          : mix(0.26, 1.42, smoothstep(0.015, 0.80, authored));
      float cluster =
          clamp(firstBurst * 0.74 + secondBurst * 0.58, 0.0, 1.0);
      float shape = (0.75 * core + shell) * authoredShape *
                    mix(0.54, 1.28, cluster);
      float sceneFade = smoothstep(
          0.0, softDepthWidth,
          max(sceneDepth - sampleDepth, 0.0) * rayLength);
      float sampleTau =
          max(u_color_density.a, 0.0) * normalizedStep * shape * sceneFade;
      float stepTransmission = exp(-sampleTau);
      float absorbed = transmission * (1.0 - stepTransmission);
      float cooling =
          clamp(life * 0.72 + radiusFromCenter * 0.28, 0.0, 1.0);
      vec3 sampleColor = mix(
          vec3(1.0, 0.98, 0.78), vec3(1.0, 0.22, 0.012),
          smoothstep(0.18, 0.88, cooling));
      sampleColor *= mix(vec3(1.0), tint, 0.62);
      radiance += sampleColor * absorbed;
      transmission *= stepTransmission;
    }
    fragColor = vec4(radiance * u_radius_emission.w, 0.0);
    return;
  }

  float speed = kind < 2.5 ? 0.12 : 0.025;
  vec2 flow = vec2(seed * 0.013 + time * speed * 0.35,
                   seed * 0.021 + time * speed);
  const int sampleCount = 8;
  float normalizedStep = normalizedSegment / float(sampleCount);
  float attenuation = kind < 2.5 ? 0.72 : 0.42;
  float accumulatedOpacity = 0.0;
  vec3 accumulatedColor = vec3(0.0);
  float luminance = dot(tint, vec3(0.299, 0.587, 0.114));
  vec3 neutral = kind < 2.5 ? vec3(0.075 + luminance * 0.14)
                            : vec3(0.62, 0.67, 0.70);
  vec3 baseColor =
      mix(neutral, tint * (kind < 2.5 ? 0.34 : 0.78), 0.66);
  for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
    float sampleFraction =
        (float(sampleIndex) + 0.5) / float(sampleCount);
    float sampleDepth = mix(startDepth, endDepth, sampleFraction);
    vec3 sampleDelta = ray * sampleDepth - center;
      vec3 sampleLocal = vec3(dot(sampleDelta, u_axis_x),
                              dot(sampleDelta, u_axis_y),
                              dot(sampleDelta, u_axis_z)) / radius;
    float body = volumeClusterBody(sampleLocal, kind, seed, flow);
    float sceneFade = smoothstep(
        0.0, softDepthWidth,
        max(sceneDepth - sampleDepth, 0.0) * rayLength);
    float sampleTau = max(u_color_density.a, 0.0) * normalizedStep * body *
                      attenuation * sceneFade;
    float sampleOpacity = 1.0 - exp(-sampleTau);
    float visibleOpacity = (1.0 - accumulatedOpacity) * sampleOpacity;
    vec3 sampleColor = baseColor *
        mix(0.78, 1.08,
            clamp(0.5 - sampleLocal.y * 0.35, 0.0, 1.0));
    accumulatedColor += sampleColor * visibleOpacity;
    accumulatedOpacity += visibleOpacity;
  }
  float opacityLimit = kind < 2.5 ? 0.58 : 0.24;
  if (accumulatedOpacity > opacityLimit) {
    accumulatedColor *= opacityLimit / accumulatedOpacity;
    accumulatedOpacity = opacityLimit;
  }
  fragColor = vec4(accumulatedColor, accumulatedOpacity);
}
#endif
)PSYX";

bool PsyX_VolumeIsEmissive(const GrVolumetricEffect &effect) {
  return effect.kind == GR_VOLUME_FIRE ||
         effect.kind == GR_VOLUME_EXPLOSION ||
         effect.kind == GR_VOLUME_LIGHT_HALO;
}

struct PsyXVolumeBasis {
  float x_x{1.0F};
  float x_y{};
  float x_z{};
  float y_x{};
  float y_y{1.0F};
  float y_z{};
  float z_x{};
  float z_y{};
  float z_z{1.0F};
};

PsyXVolumeBasis PsyX_ResolveVolumeBasis(const GrVolumetricEffect &effect) {
  auto basis = PsyXVolumeBasis{
      effect.axis_x_x, effect.axis_x_y, effect.axis_x_z,
      effect.axis_y_x, effect.axis_y_y, effect.axis_y_z,
      effect.axis_z_x, effect.axis_z_y, effect.axis_z_z,
  };
  const auto finite = [](float value) { return std::isfinite(value); };
  const auto lengthSquared = [](float x, float y, float z) {
    return x * x + y * y + z * z;
  };
  if (!finite(basis.x_x) || !finite(basis.x_y) || !finite(basis.x_z) ||
      !finite(basis.y_x) || !finite(basis.y_y) || !finite(basis.y_z) ||
      !finite(basis.z_x) || !finite(basis.z_y) || !finite(basis.z_z)) {
    return {};
  }
  const auto xLength = std::sqrt(lengthSquared(basis.x_x, basis.x_y, basis.x_z));
  const auto yLength = std::sqrt(lengthSquared(basis.y_x, basis.y_y, basis.y_z));
  const auto zLength = std::sqrt(lengthSquared(basis.z_x, basis.z_y, basis.z_z));
  if (xLength <= 0.0001F || yLength <= 0.0001F || zLength <= 0.0001F) {
    return {};
  }
  basis.x_x /= xLength;
  basis.x_y /= xLength;
  basis.x_z /= xLength;
  basis.y_x /= yLength;
  basis.y_y /= yLength;
  basis.y_z /= yLength;
  basis.z_x /= zLength;
  basis.z_y /= zLength;
  basis.z_z /= zLength;
  const auto determinant =
      basis.x_x * (basis.y_y * basis.z_z - basis.y_z * basis.z_y) -
      basis.y_x * (basis.x_y * basis.z_z - basis.x_z * basis.z_y) +
      basis.z_x * (basis.x_y * basis.y_z - basis.x_z * basis.y_y);
  return std::abs(determinant) >= 0.75F ? basis : PsyXVolumeBasis{};
}

struct PsyXVolumeExtents {
  float x{};
  float y{};
  float z{};
};

PsyXVolumeExtents PsyX_VolumeCameraExtents(
    const GrVolumetricEffect &effect, const PsyXVolumeBasis &basis) {
  return {
      std::abs(basis.x_x) * effect.radius_x +
          std::abs(basis.y_x) * effect.radius_y +
          std::abs(basis.z_x) * effect.radius_z,
      std::abs(basis.x_y) * effect.radius_x +
          std::abs(basis.y_y) * effect.radius_y +
          std::abs(basis.z_y) * effect.radius_z,
      std::abs(basis.x_z) * effect.radius_x +
          std::abs(basis.y_z) * effect.radius_y +
          std::abs(basis.z_z) * effect.radius_z,
  };
}

bool PsyX_ValidVolume(const GrVolumetricEffect &effect) {
  const auto extents =
      PsyX_VolumeCameraExtents(effect, PsyX_ResolveVolumeBasis(effect));
  return effect.kind >= GR_VOLUME_FIRE &&
         effect.kind <= GR_VOLUME_LIGHT_HALO &&
         std::isfinite(effect.center_x) && std::isfinite(effect.center_y) &&
         std::isfinite(effect.center_z) && std::isfinite(effect.radius_x) &&
         std::isfinite(effect.radius_y) && std::isfinite(effect.radius_z) &&
         effect.center_z + extents.z > 0.25F &&
         effect.radius_x > 0.0F && effect.radius_y > 0.0F &&
         effect.radius_z > 0.0F && effect.density > 0.0F;
}

bool PsyX_VolumeScissor(const GrVolumetricEffect &effect, int projection,
                        int logicalWidth, int logicalHeight,
                        const PsyXPresentationViewport &viewport,
                        const PsyXPresentationScale &scale, GLint output[4]) {
  const auto extents =
      PsyX_VolumeCameraExtents(effect, PsyX_ResolveVolumeBasis(effect));
  const float front = std::max(effect.center_z - extents.z, 0.251F);
  const float centerX = static_cast<float>(logicalWidth) * 0.5F +
                        effect.center_x * static_cast<float>(projection) /
                            std::max(effect.center_z, 0.251F);
  const float centerY = static_cast<float>(logicalHeight) * 0.5F +
                        effect.center_y * static_cast<float>(projection) /
                            std::max(effect.center_z, 0.251F);
  const float centerDepth = std::max(effect.center_z, 0.251F);
  const float depthShiftX =
      std::abs(effect.center_x) * extents.z / centerDepth;
  const float depthShiftY =
      std::abs(effect.center_y) * extents.z / centerDepth;
  const float halfWidth = (extents.x + depthShiftX) *
                          static_cast<float>(projection) / front;
  const float halfHeight = (extents.y + depthShiftY) *
                           static_cast<float>(projection) / front;
  const auto mapX = [&](float logical) {
    const float ndc =
        ((logical / static_cast<float>(logicalWidth)) - 0.5F) * 2.0F * scale.x;
    return static_cast<float>(viewport.x) +
           (ndc * 0.5F + 0.5F) * static_cast<float>(viewport.w);
  };
  const auto mapY = [&](float logical) {
    const float ndc =
        -((logical / static_cast<float>(logicalHeight)) - 0.5F) * 2.0F *
        scale.y;
    return static_cast<float>(viewport.y) +
           (ndc * 0.5F + 0.5F) * static_cast<float>(viewport.h);
  };
  const float left = mapX(centerX - halfWidth);
  const float right = mapX(centerX + halfWidth);
  const float bottom = mapY(centerY + halfHeight);
  const float top = mapY(centerY - halfHeight);
  const int minimumX = std::max(viewport.x, static_cast<int>(std::floor(left)));
  const int maximumX = std::min(viewport.x + viewport.w,
                                static_cast<int>(std::ceil(right)));
  const int minimumY =
      std::max(viewport.y, static_cast<int>(std::floor(bottom)));
  const int maximumY = std::min(viewport.y + viewport.h,
                                static_cast<int>(std::ceil(top)));
  if (minimumX >= maximumX || minimumY >= maximumY)
    return false;
  output[0] = minimumX;
  output[1] = minimumY;
  output[2] = maximumX - minimumX;
  output[3] = maximumY - minimumY;
  return true;
}

void PsyX_InitialiseVolumetrics() {
  g_volumetrics.program = GR_Shader_Compile(kVolumetricShader, 0);
  GLint linked = GL_FALSE;
  glGetProgramiv(g_volumetrics.program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    glDeleteProgram(g_volumetrics.program);
    g_volumetrics.program = 0;
    eprintwarn("Volumetric effect shader is unavailable; using retail sprites\n");
    return;
  }
  g_volumetrics.sceneDepth =
      glGetUniformLocation(g_volumetrics.program, "u_scene_depth");
  g_volumetrics.densityAtlasSampler =
      glGetUniformLocation(g_volumetrics.program, "u_density_atlas");
  g_volumetrics.densityAtlasReady =
      glGetUniformLocation(g_volumetrics.program, "u_density_atlas_ready");
  g_volumetrics.targetSize =
      glGetUniformLocation(g_volumetrics.program, "u_target_size");
  g_volumetrics.viewport =
      glGetUniformLocation(g_volumetrics.program, "u_viewport");
  g_volumetrics.view = glGetUniformLocation(g_volumetrics.program, "u_view");
  g_volumetrics.presentationScale =
      glGetUniformLocation(g_volumetrics.program, "u_presentation_scale");
  g_volumetrics.depthProjection =
      glGetUniformLocation(g_volumetrics.program, "u_depth_projection");
  g_volumetrics.centerKind =
      glGetUniformLocation(g_volumetrics.program, "u_center_kind");
  g_volumetrics.radiusEmission =
      glGetUniformLocation(g_volumetrics.program, "u_radius_emission");
  g_volumetrics.colorDensity =
      glGetUniformLocation(g_volumetrics.program, "u_color_density");
  g_volumetrics.phaseSeed =
      glGetUniformLocation(g_volumetrics.program, "u_phase_seed");
  g_volumetrics.flags =
      glGetUniformLocation(g_volumetrics.program, "u_flags");
  g_volumetrics.axisX =
      glGetUniformLocation(g_volumetrics.program, "u_axis_x");
  g_volumetrics.axisY =
      glGetUniformLocation(g_volumetrics.program, "u_axis_y");
  g_volumetrics.axisZ =
      glGetUniformLocation(g_volumetrics.program, "u_axis_z");
  glUseProgram(g_volumetrics.program);
  glUniform1i(g_volumetrics.sceneDepth, 0);
  glUniform1i(g_volumetrics.densityAtlasSampler, 1);
  glUseProgram(0);
  glGenFramebuffers(1, &g_volumetrics.framebuffer);
  glGenVertexArrays(1, &g_volumetrics.vertexArray);
  g_volumetrics.available = true;
  eprintf("*Volumetric effects: depth-aware volumes enabled\n");
}

void PsyX_DestroyVolumetrics() {
  if (g_volumetrics.densityAtlas != 0)
    glDeleteTextures(1, &g_volumetrics.densityAtlas);
  if (g_volumetrics.vertexArray != 0)
    glDeleteVertexArrays(1, &g_volumetrics.vertexArray);
  if (g_volumetrics.framebuffer != 0)
    glDeleteFramebuffers(1, &g_volumetrics.framebuffer);
  if (g_volumetrics.program != 0)
    glDeleteProgram(g_volumetrics.program);
  g_volumetrics = {};
}

} // namespace

int GR_BeginVolumetricFrame(int enable) {
  g_volumetrics.frameReady = false;
  if (!enable || !g_volumetrics.available || g_nativeFramebufferWidth <= 0 ||
      g_nativeFramebufferHeight <= 0)
    return 0;

  glBindFramebuffer(GL_FRAMEBUFFER, g_volumetrics.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         g_glNativeColorTexture, 0);
  const bool complete =
      glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_FRAMEBUFFER, PsyX_GetNativeDrawFramebuffer());
  g_volumetrics.frameReady = complete;
  return complete ? 1 : 0;
}

int GR_VolumetricEffectsAvailable(void) {
  return g_cfg_volumetricEffects && g_volumetrics.frameReady ? 1 : 0;
}

int GR_UploadVolumetricDensityAtlas(int width, int height,
                                    const unsigned char *rgba) {
  if (!g_volumetrics.available || width <= 0 || height <= 0 || rgba == nullptr)
    return 0;

  GLint previousActiveTexture = GL_TEXTURE0;
  GLint previousTexture = 0;
  GLint previousUnpackAlignment = 4;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
  glActiveTexture(GL_TEXTURE1);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);

  // Isolate this upload from a stale caller error and commit only a complete
  // texture. The old atlas remains live if allocation or upload fails.
  while (glGetError() != GL_NO_ERROR) {
  }
  GLuint candidate = 0;
  glGenTextures(1, &candidate);
  GLint uploadedWidth = 0;
  GLint uploadedHeight = 0;
  if (candidate != 0) {
    glBindTexture(GL_TEXTURE_2D, candidate);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
                             &uploadedWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                             &uploadedHeight);
  }
  auto uploadError = GL_NO_ERROR;
  for (auto error = glGetError(); error != GL_NO_ERROR;
       error = glGetError()) {
    uploadError = error;
  }
  const auto complete = candidate != 0 && uploadError == GL_NO_ERROR &&
                        uploadedWidth == width && uploadedHeight == height;
  const auto oldAtlas = g_volumetrics.densityAtlas;
  auto restoredTexture = static_cast<GLuint>(previousTexture);
  if (complete && restoredTexture == oldAtlas) {
    restoredTexture = candidate;
  }
  glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
  glBindTexture(GL_TEXTURE_2D, restoredTexture);
  glActiveTexture(static_cast<GLenum>(previousActiveTexture));
  if (!complete) {
    if (candidate != 0) {
      glDeleteTextures(1, &candidate);
    }
    return 0;
  }
  g_volumetrics.densityAtlas = candidate;
  if (oldAtlas != 0) {
    glDeleteTextures(1, &oldAtlas);
  }
  eprintf("*Volumetric effects: authored four-slice density atlas enabled\n");
  return 1;
}

void GR_DrawVolumetricEffects(const GrVolumetricEffect *effects, int count,
                              int projection, int logicalWidth,
                              int logicalHeight, float timeSeconds) {
  if (!g_volumetrics.frameReady || effects == nullptr || count <= 0 ||
      projection <= 0 || logicalWidth <= 0 || logicalHeight <= 0) {
    g_volumetrics.frameReady = false;
    return;
  }

  std::vector<const GrVolumetricEffect *> alphaVolumes;
  std::vector<const GrVolumetricEffect *> emissiveVolumes;
  alphaVolumes.reserve(static_cast<std::size_t>(count));
  emissiveVolumes.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    if (!PsyX_ValidVolume(effects[index]))
      continue;
    auto &target = PsyX_VolumeIsEmissive(effects[index])
                       ? emissiveVolumes
                       : alphaVolumes;
    target.push_back(&effects[index]);
  }
  std::sort(alphaVolumes.begin(), alphaVolumes.end(),
            [](const GrVolumetricEffect *first,
               const GrVolumetricEffect *second) {
              return first->center_z > second->center_z;
            });

  const int savedBlendMode = g_PreviousBlendMode;
  const int savedDepthMode = g_PreviousDepthMode;
  const int savedDepthWrite = g_PreviousDepthWrite;
  const int savedStencilMode = g_PreviousStencilMode;
  const GLboolean savedStencilTest = glIsEnabled(GL_STENCIL_TEST);
  const GLboolean savedScissorTest = glIsEnabled(GL_SCISSOR_TEST);
  const GLboolean savedFramebufferSrgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
  GLint savedViewport[4] = {};
  GLint savedScissorBox[4] = {};
  glGetIntegerv(GL_VIEWPORT, savedViewport);
  glGetIntegerv(GL_SCISSOR_BOX, savedScissorBox);

  PsyX_ResolveNativeFramebuffer(1);
  g_nativeFramePostprocessed = 1;
  glBindFramebuffer(GL_FRAMEBUFFER, g_volumetrics.framebuffer);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_STENCIL_TEST);
  glEnable(GL_SCISSOR_TEST);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glUseProgram(g_volumetrics.program);
  glBindVertexArray(g_volumetrics.vertexArray);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_glNativeDepthTexture);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, g_volumetrics.densityAtlas);
  glActiveTexture(GL_TEXTURE0);
  glUniform1f(g_volumetrics.densityAtlasReady,
              g_volumetrics.densityAtlas != 0 ? 1.0F : 0.0F);

  const auto viewport = PsyX_GetRenderViewport();
  const auto scale = PsyX_GetPresentationScale();
  glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
  glUniform2f(g_volumetrics.targetSize,
              static_cast<float>(g_nativeFramebufferWidth),
              static_cast<float>(g_nativeFramebufferHeight));
  glUniform4f(g_volumetrics.viewport, static_cast<float>(viewport.x),
              static_cast<float>(viewport.y), static_cast<float>(viewport.w),
              static_cast<float>(viewport.h));
  glUniform4f(g_volumetrics.view, static_cast<float>(logicalWidth),
              static_cast<float>(logicalHeight),
              static_cast<float>(projection), timeSeconds);
  glUniform2f(g_volumetrics.presentationScale, scale.x, scale.y);
  glUniform2f(g_volumetrics.depthProjection, g_reversedDepthScale,
              g_reversedDepthBias);

  const auto draw = [&](const auto &volumes, bool additive) {
    if (additive)
      glBlendFunc(GL_ONE, GL_ONE);
    else
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto *effect : volumes) {
      GLint scissor[4]{};
      if (!PsyX_VolumeScissor(*effect, projection, logicalWidth,
                              logicalHeight, viewport, scale, scissor))
        continue;
      glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
      glUniform4f(g_volumetrics.centerKind, effect->center_x,
                  effect->center_y, effect->center_z,
                  static_cast<float>(effect->kind));
      glUniform4f(g_volumetrics.radiusEmission, effect->radius_x,
                  effect->radius_y, effect->radius_z, effect->emission);
      glUniform4f(g_volumetrics.colorDensity, effect->red, effect->green,
                  effect->blue, effect->density);
      glUniform2f(g_volumetrics.phaseSeed, effect->phase, effect->seed);
      const auto basis = PsyX_ResolveVolumeBasis(*effect);
      glUniform1i(g_volumetrics.flags, static_cast<GLint>(effect->flags));
      glUniform3f(g_volumetrics.axisX, basis.x_x, basis.x_y, basis.x_z);
      glUniform3f(g_volumetrics.axisY, basis.y_x, basis.y_y, basis.y_z);
      glUniform3f(g_volumetrics.axisZ, basis.z_x, basis.z_y, basis.z_z);
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }
  };
  draw(alphaVolumes, false);
  draw(emissiveVolumes, true);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
  glBindVertexArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER, g_glNativeFramebuffer);
  glViewport(savedViewport[0], savedViewport[1], savedViewport[2],
             savedViewport[3]);
  glScissor(savedScissorBox[0], savedScissorBox[1], savedScissorBox[2],
            savedScissorBox[3]);
  if (savedScissorTest)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);
  if (savedFramebufferSrgb)
    glEnable(GL_FRAMEBUFFER_SRGB);
  else
    glDisable(GL_FRAMEBUFFER_SRGB);
  if (savedStencilTest)
    glEnable(GL_STENCIL_TEST);
  else
    glDisable(GL_STENCIL_TEST);

  g_PreviousBlendMode = BM_NONE;
  glDisable(GL_BLEND);
  if (savedBlendMode != BM_NONE)
    GR_SetBlendMode(static_cast<BlendMode>(savedBlendMode));
  g_PreviousDepthMode = -1;
  g_PreviousDepthWrite = -1;
  g_PreviousDepthFunc = -1;
  GR_SetDepthState(savedDepthMode, savedDepthWrite);
  g_PreviousStencilMode = savedStencilMode == 0 ? 1 : 0;
  GR_SetStencilMode(savedStencilMode);
  g_PreviousScissorState = savedScissorTest ? 1 : 0;
  g_PreviousShader = -1;
  g_PreviousTextureBlendShader = -1;
  g_PreviousTextureBlendMode = -1;
  g_lastBoundTexture = -1;
  g_volumetrics.frameReady = false;
}

#else

namespace {
void PsyX_InitialiseVolumetrics() {}
void PsyX_DestroyVolumetrics() {}
} // namespace
int GR_BeginVolumetricFrame(int) { return 0; }
int GR_VolumetricEffectsAvailable(void) { return 0; }
int GR_UploadVolumetricDensityAtlas(int, int, const unsigned char *) { return 0; }
void GR_DrawVolumetricEffects(const GrVolumetricEffect *, int, int, int, int,
                              float) {}

#endif
