#if defined(RENDERER_OGL)

namespace {

struct PsyXSkyboxResources {
  GLuint program = 0;
  GLuint vertexArray = 0;
  GLint skyTexture = -1;
  GLint viewport = -1;
  GLint rayScale = -1;
  GLint right = -1;
  GLint down = -1;
  GLint forward = -1;
  GLint tintExposure = -1;
  GLint horizon = -1;
  GLint horizonShape = -1;
  bool available = false;
};

PsyXSkyboxResources g_skybox;

constexpr const char *kSkyboxShader = R"PSYX(
#ifdef VERTEX
void main() {
  vec2 position = gl_VertexID == 0 ? vec2(0.0, 0.0)
                : gl_VertexID == 1 ? vec2(2.0, 0.0)
                                   : vec2(0.0, 2.0);
  gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
#else
uniform sampler2D u_sky;
uniform vec4 u_viewport;
uniform vec2 u_ray_scale;
uniform vec3 u_right;
uniform vec3 u_down;
uniform vec3 u_forward;
uniform vec4 u_tint_exposure;
uniform vec4 u_horizon;
uniform vec2 u_horizon_shape;

const float kPi = 3.14159265358979323846;

void main() {
  vec2 viewportPosition =
      (gl_FragCoord.xy - u_viewport.xy) / max(u_viewport.zw, vec2(1.0));
  vec2 ndc = viewportPosition * 2.0 - 1.0;
  // Reconstruct a world-space direction only: translation never enters this
  // path, so the panorama behaves as an infinite dome around the level rather
  // than as a screen overlay or a textured plane.
  vec3 cameraRay = normalize(
      vec3(ndc.x * u_ray_scale.x, -ndc.y * u_ray_scale.y, 1.0));
  vec3 worldDirection = normalize(u_right * cameraRay.x +
                                  u_down * cameraRay.y +
                                  u_forward * cameraRay.z);

  float longitude = atan(worldDirection.x, worldDirection.z) /
                    (2.0 * kPi) + 0.5 + u_horizon_shape.y;
  float latitude = acos(clamp(-worldDirection.y, -1.0, 1.0)) / kPi;
  vec2 skyUv = vec2(fract(longitude), latitude);
#if __VERSION__ >= 130
  // Equirectangular U wraps from one to zero. Fold that periodic derivative
  // to the shortest path so implicit LOD cannot select a coarse seam mip.
  vec2 skyDx = dFdx(skyUv);
  vec2 skyDy = dFdy(skyUv);
  skyDx.x -= floor(skyDx.x + 0.5);
  skyDy.x -= floor(skyDy.x + 0.5);
  vec3 sky = textureGrad(u_sky, skyUv, skyDx, skyDy).rgb;
#else
  vec3 sky = texture2D(u_sky, skyUv).rgb;
#endif
  sky *= u_tint_exposure.rgb * u_tint_exposure.a;

  float horizonBand = max(u_horizon_shape.x, 0.001);
  float horizonMix = 1.0 - smoothstep(
      0.0, horizonBand, abs(worldDirection.y));
  // Holes below the authored scene converge to the same far colour as world
  // fog instead of exposing a second inverted cloud layer. The full blend is
  // intentional: it leaves no seam between the dome and the retail dark fog.
  float lowerHemisphere = smoothstep(0.0, 0.42, worldDirection.y);
  float fogMix = max(horizonMix * u_horizon.a, lowerHemisphere);
  sky = mix(sky, u_horizon.rgb, clamp(fogMix, 0.0, 1.0));
  fragColor = vec4(sky, 1.0);
}
#endif
)PSYX";

void PsyX_DestroySkybox() {
  if (g_skybox.vertexArray != 0) {
    glDeleteVertexArrays(1, &g_skybox.vertexArray);
  }
  if (g_skybox.program != 0) {
    glDeleteProgram(g_skybox.program);
  }
  g_skybox = {};
}

void PsyX_InitialiseSkybox() {
  PsyX_DestroySkybox();
  g_skybox.program = GR_Shader_Compile(kSkyboxShader, 0);
  GLint linked = GL_FALSE;
  if (g_skybox.program != 0) {
    glGetProgramiv(g_skybox.program, GL_LINK_STATUS, &linked);
  }
  if (linked != GL_TRUE) {
    eprintwarn("Mission skybox shader is unavailable; using retail clear\n");
    PsyX_DestroySkybox();
    return;
  }

  glGenVertexArrays(1, &g_skybox.vertexArray);
  g_skybox.skyTexture = glGetUniformLocation(g_skybox.program, "u_sky");
  g_skybox.viewport = glGetUniformLocation(g_skybox.program, "u_viewport");
  g_skybox.rayScale = glGetUniformLocation(g_skybox.program, "u_ray_scale");
  g_skybox.right = glGetUniformLocation(g_skybox.program, "u_right");
  g_skybox.down = glGetUniformLocation(g_skybox.program, "u_down");
  g_skybox.forward = glGetUniformLocation(g_skybox.program, "u_forward");
  g_skybox.tintExposure =
      glGetUniformLocation(g_skybox.program, "u_tint_exposure");
  g_skybox.horizon = glGetUniformLocation(g_skybox.program, "u_horizon");
  g_skybox.horizonShape =
      glGetUniformLocation(g_skybox.program, "u_horizon_shape");

  GLint savedProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
  glUseProgram(g_skybox.program);
  glUniform1i(g_skybox.skyTexture, 0);
  glUseProgram(static_cast<GLuint>(savedProgram));
  g_skybox.available = g_skybox.vertexArray != 0;
  if (g_skybox.available) {
    eprintf("*Mission skyboxes: depth-free equirectangular pass enabled\n");
  }
}

} // namespace

TextureID GR_CreateSkyboxTexture(int width, int height,
                                 const unsigned char *rgba) {
  if (!g_skybox.available || rgba == nullptr || width <= 0 || height <= 0 ||
      width != height * 2) {
    return 0;
  }

  GLint savedActiveTexture = GL_TEXTURE0;
  GLint savedTexture = 0;
  GLint savedUnpackAlignment = 4;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTexture);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &savedUnpackAlignment);

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  if (g_cfg_anisotropicFiltering && g_maxTextureAnisotropy > 1.0F) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    std::min(g_maxTextureAnisotropy, 8.0F));
  }
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgba);
  glGenerateMipmap(GL_TEXTURE_2D);

  glPixelStorei(GL_UNPACK_ALIGNMENT, savedUnpackAlignment);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(savedTexture));
  glActiveTexture(static_cast<GLenum>(savedActiveTexture));
  return texture;
}

void GR_DrawSkybox(TextureID texture, const GrSkyboxView *view) {
  if (!g_skybox.available || texture == 0 || view == nullptr ||
      view->projection <= 0 || view->logical_width <= 0 ||
      view->logical_height <= 0) {
    return;
  }

  GLint savedProgram = 0;
  GLint savedVertexArray = 0;
  GLint savedActiveTexture = GL_TEXTURE0;
  GLint savedTexture = 0;
  GLint savedDrawFramebuffer = 0;
  GLint savedViewport[4] = {};
  GLint savedScissor[4] = {};
  GLboolean savedColorMask[4] = {};
  GLboolean savedDepthMask = GL_TRUE;
  const GLboolean savedBlend = glIsEnabled(GL_BLEND);
  const GLboolean savedDepth = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean savedStencil = glIsEnabled(GL_STENCIL_TEST);
  const GLboolean savedScissorTest = glIsEnabled(GL_SCISSOR_TEST);
  const GLboolean savedCull = glIsEnabled(GL_CULL_FACE);
  const GLboolean savedFramebufferSrgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);

  glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedVertexArray);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTexture);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
  glGetIntegerv(GL_VIEWPORT, savedViewport);
  glGetIntegerv(GL_SCISSOR_BOX, savedScissor);
  glGetBooleanv(GL_COLOR_WRITEMASK, savedColorMask);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &savedDepthMask);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, PsyX_GetNativeDrawFramebuffer());
  const auto viewport = PsyX_GetRenderViewport();
  const auto scale = PsyX_GetPresentationScale();
  glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glUseProgram(g_skybox.program);
  glBindVertexArray(g_skybox.vertexArray);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform4f(g_skybox.viewport, static_cast<float>(viewport.x),
              static_cast<float>(viewport.y), static_cast<float>(viewport.w),
              static_cast<float>(viewport.h));
  glUniform2f(g_skybox.rayScale,
              static_cast<float>(view->logical_width) /
                  (2.0F * static_cast<float>(view->projection) * scale.x),
              static_cast<float>(view->logical_height) /
                  (2.0F * static_cast<float>(view->projection) * scale.y));
  glUniform3f(g_skybox.right, view->right_x, view->right_y, view->right_z);
  glUniform3f(g_skybox.down, view->down_x, view->down_y, view->down_z);
  glUniform3f(g_skybox.forward, view->forward_x, view->forward_y,
              view->forward_z);
  glUniform4f(g_skybox.tintExposure, view->tint_red, view->tint_green,
              view->tint_blue, view->exposure);
  glUniform4f(g_skybox.horizon, view->horizon_red, view->horizon_green,
              view->horizon_blue, view->horizon_strength);
  glUniform2f(g_skybox.horizonShape, view->horizon_band,
              view->yaw_offset_turns);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(savedTexture));
  glActiveTexture(static_cast<GLenum>(savedActiveTexture));
  glBindVertexArray(static_cast<GLuint>(savedVertexArray));
  glUseProgram(static_cast<GLuint>(savedProgram));
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                    static_cast<GLuint>(savedDrawFramebuffer));
  glViewport(savedViewport[0], savedViewport[1], savedViewport[2],
             savedViewport[3]);
  glScissor(savedScissor[0], savedScissor[1], savedScissor[2], savedScissor[3]);
  glColorMask(savedColorMask[0], savedColorMask[1], savedColorMask[2],
              savedColorMask[3]);
  glDepthMask(savedDepthMask);

  if (savedBlend)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  if (savedDepth)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  if (savedStencil)
    glEnable(GL_STENCIL_TEST);
  else
    glDisable(GL_STENCIL_TEST);
  if (savedScissorTest)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);
  if (savedCull)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);
  if (savedFramebufferSrgb)
    glEnable(GL_FRAMEBUFFER_SRGB);
  else
    glDisable(GL_FRAMEBUFFER_SRGB);
}

#else

namespace {
void PsyX_InitialiseSkybox() {}
void PsyX_DestroySkybox() {}
} // namespace

TextureID GR_CreateSkyboxTexture(int, int, const unsigned char *) { return 0; }
void GR_DrawSkybox(TextureID, const GrSkyboxView *) {}

#endif
