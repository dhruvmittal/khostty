#pragma once

#include <ghostty.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  KHOSTTY_FRAME_NONE = 0,
  KHOSTTY_FRAME_DMABUF = 1,
  KHOSTTY_FRAME_MEMORY = 2,
} khostty_frame_type_e;

typedef struct {
  uint8_t count;
  int fds[4];
  int32_t offsets[4];
  int32_t strides[4];
} khostty_dmabuf_planes_s;

typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t fourcc;
  uint64_t modifier;
  bool premultiplied;
  khostty_dmabuf_planes_s planes;
} khostty_frame_dmabuf_s;

typedef struct {
  uint32_t width;
  uint32_t height;
  const uint8_t* pixels;
  size_t len;
} khostty_frame_memory_s;

typedef struct {
  khostty_frame_type_e frame_type;
  khostty_frame_dmabuf_s dmabuf;
  khostty_frame_memory_s memory;
  void* internal_ptr;
} khostty_exported_frame_s;

/**
 * Fetch the latest completed frame from Ghostty's background renderer queue.
 * Returns true if a new frame was available, false otherwise.
 *
 * NOTE: The caller MUST call khostty_surface_release_frame() on out_frame
 * as soon as it has finished importing the DMA-BUF FDs into OpenGL, or
 * copying/uploading memory pixels.
 */
GHOSTTY_API bool khostty_surface_take_frame(
    ghostty_surface_t surface,
    khostty_exported_frame_s* out_frame);

/**
 * Release kernel DMA-BUF file descriptors or memory backing the exported frame.
 */
GHOSTTY_API void khostty_surface_release_frame(
    ghostty_surface_t surface,
    khostty_exported_frame_s* frame);

/**
 * Report whether the host can present DMA-BUFs (healthy = true) or needs
 * Ghostty to switch to CPU RGBA8 readback (healthy = false).
 */
GHOSTTY_API void khostty_surface_report_presentation_health(
    ghostty_surface_t surface,
    bool healthy);

#ifdef __cplusplus
}
#endif
