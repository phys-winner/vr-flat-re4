// Scene presentation and touch-control overlay drawn into the owned swapchain image.
// Implemented in re4fix_overlay.c (separate translation unit that uses the real Vulkan headers).
#ifndef RE4FIX_OVERLAY_H
#define RE4FIX_OVERLAY_H

// One textured quad: rect in Vulkan NDC (x right, y down, -1..1), uv rect in the atlas (0..1), colour multiplier.
struct ovl_quad { float x0, y0, x1, y1, u0, v0, u1, v1, r, g, b, a; };

// Creates render pass, framebuffers on the swapchain images, pipeline and the icon atlas (uploaded lazily by the
// first ovl_record). gipa = real vkGetInstanceProcAddr. Returns 1 on success.
int ovl_init(void* (*gipa)(void*, const char*), void* instance, void* phys, void* dev, unsigned qfamily,
             unsigned w, unsigned h, unsigned fmt, unsigned n, void* const* images, void* const* proxy_images, int mutable_proxy);
int ovl_ready(void);
// Re-create the swapchain attachment views/framebuffers after the window swapchain was recreated (same count).
int ovl_rebind(void* const* images, unsigned n);
// Samples the owned sRGB scene first, then draws the touch overlay for image idx.
// Scene must be SHADER_READ_ONLY_OPTIMAL and destination COLOR_ATTACHMENT_OPTIMAL;
// the render pass leaves destination in PRESENT_SRC_KHR.
void ovl_record(void* cmd, unsigned idx, const struct ovl_quad* q, int nq);
// uv rect of atlas cell `cell`
void ovl_cell_uv(int cell, float* u0, float* v0, float* u1, float* v1);

#endif
