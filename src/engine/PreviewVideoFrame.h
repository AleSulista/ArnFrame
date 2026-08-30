#pragma once

#include <QMetaType>

#include <memory>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

// A decoded preview frame still owned as an AVFrame: hardware surfaces stay in the
// decoder pool, software frames stay in their original (or NV12) system-memory
// buffers. The GL importer samples this on the compositor's GL thread — nothing
// here is packed for CPU upload.
struct PreviewVideoFrame
{
    std::shared_ptr<AVFrame> frame;
    // Display-matrix rotation the GL convert shader applies (0/90/180/270). The
    // stored frame is coded-orientation; displayWidth/Height swap on 90/270.
    int rotation = 0;
    int colorspace = AVCOL_SPC_UNSPECIFIED;
    int colorRange = AVCOL_RANGE_UNSPECIFIED;

    bool isValid() const
    {
        return frame && frame->width > 0 && frame->height > 0 && frame->data[0] != nullptr;
    }

    int codedWidth() const { return frame ? frame->width : 0; }
    int codedHeight() const { return frame ? frame->height : 0; }

    int displayWidth() const
    {
        return (rotation == 90 || rotation == 270) ? codedHeight() : codedWidth();
    }
    int displayHeight() const
    {
        return (rotation == 90 || rotation == 270) ? codedWidth() : codedHeight();
    }

    bool isHardware() const
    {
        if (!frame)
            return false;
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
        return desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
    }
};

inline void avFrameDeleter(AVFrame *f)
{
    av_frame_free(&f);
}

// Clones `src` (refs hardware buffers, copies nothing) and snapshots colour/rotation.
inline PreviewVideoFrame makePreviewFrame(const AVFrame *src, int rotation)
{
    PreviewVideoFrame out;
    if (!src)
        return out;
    AVFrame *clone = av_frame_clone(src);
    if (!clone)
        return out;
    out.frame.reset(clone, avFrameDeleter);
    out.rotation = rotation;
    out.colorspace = src->colorspace;
    out.colorRange = src->color_range;
    return out;
}

// Takes ownership of `owned`.
inline PreviewVideoFrame takePreviewFrame(AVFrame *owned, int rotation)
{
    PreviewVideoFrame out;
    if (!owned)
        return out;
    out.frame.reset(owned, avFrameDeleter);
    out.rotation = rotation;
    out.colorspace = owned->colorspace;
    out.colorRange = owned->color_range;
    return out;
}

Q_DECLARE_METATYPE(PreviewVideoFrame)
