const std = @import("std");
const apprt = @import("apprt.zig");
const Surface = apprt.embedded.Surface;
const OpenGL = @import("renderer/OpenGL.zig");
const ExportedFrame = OpenGL.ExportedFrame;

pub const khostty_frame_type_e = enum(c_int) {
    none = 0,
    dmabuf = 1,
    memory = 2,
};

pub const khostty_dmabuf_planes_s = extern struct {
    count: u8,
    fds: [4]c_int,
    offsets: [4]i32,
    strides: [4]i32,
};

pub const khostty_frame_dmabuf_s = extern struct {
    width: u32,
    height: u32,
    fourcc: u32,
    modifier: u64,
    premultiplied: bool,
    planes: khostty_dmabuf_planes_s,
};

pub const khostty_frame_memory_s = extern struct {
    width: u32,
    height: u32,
    pixels: ?[*]const u8,
    len: usize,
};

pub const khostty_exported_frame_s = extern struct {
    frame_type: khostty_frame_type_e,
    dmabuf: khostty_frame_dmabuf_s,
    memory: khostty_frame_memory_s,
    internal_ptr: ?*anyopaque,
};

export fn khostty_surface_take_frame(
    surface: *Surface,
    out_frame: *khostty_exported_frame_s,
) bool {
    const maybe_frame = surface.core_surface.renderer.takeFrame();
    if (maybe_frame) |frame| {
        const alloc = surface.app.core_app.alloc;
        const box = alloc.create(ExportedFrame) catch return false;
        box.* = frame;

        out_frame.* = .{
            .frame_type = .none,
            .dmabuf = std.mem.zeroes(khostty_frame_dmabuf_s),
            .memory = std.mem.zeroes(khostty_frame_memory_s),
            .internal_ptr = box,
        };

        switch (box.*) {
            .dmabuf => |d| {
                out_frame.frame_type = .dmabuf;
                var planes: khostty_dmabuf_planes_s = .{
                    .count = d.planes.count,
                    .fds = @splat(-1),
                    .offsets = @splat(0),
                    .strides = @splat(0),
                };
                for (0..d.planes.count) |i| {
                    planes.fds[i] = d.planes.fds[i];
                    planes.offsets[i] = d.planes.offsets[i];
                    planes.strides[i] = d.planes.strides[i];
                }
                out_frame.dmabuf = .{
                    .width = d.width,
                    .height = d.height,
                    .fourcc = d.fourcc,
                    .modifier = d.modifier,
                    .premultiplied = d.premultiplied,
                    .planes = planes,
                };
            },
            .memory => |m| {
                out_frame.frame_type = .memory;
                out_frame.memory = .{
                    .width = m.width,
                    .height = m.height,
                    .pixels = m.pixels.ptr,
                    .len = m.pixels.len,
                };
            },
        }
        return true;
    }
    return false;
}

export fn khostty_surface_release_frame(
    surface: *Surface,
    frame: *khostty_exported_frame_s,
) void {
    if (frame.internal_ptr) |ptr| {
        const box: *ExportedFrame = @ptrCast(@alignCast(ptr));
        box.deinit();
        surface.app.core_app.alloc.destroy(box);
        frame.internal_ptr = null;
        frame.frame_type = .none;
    }
}

export fn khostty_surface_report_presentation_health(
    surface: *Surface,
    healthy: bool,
) void {
    surface.core_surface.reportPresentationHealth(if (healthy) .healthy else .unhealthy);
}
