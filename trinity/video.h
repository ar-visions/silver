#ifndef TRINITY_VIDEO_H
#define TRINITY_VIDEO_H
#include <vulkan/vulkan.h>

// the h.264 standard structs are bitfields, which silver cannot write: filled here
void h264_sps       (StdVideoH264SequenceParameterSet* sps, StdVideoH264SequenceParameterSetVui* vui,
                     int coded_w, int coded_h, int width, int height, int fps);
void h264_pps       (StdVideoH264PictureParameterSet* pps);
void h264_picture   (StdVideoEncodeH264PictureInfo* pic, StdVideoEncodeH264ReferenceListsInfo* refs,
                     int idr, unsigned frame_num, int poc, int idr_pic_id, int ref_slot);
void h264_slice     (StdVideoEncodeH264SliceHeader* slice, int idr);
void h264_reference (StdVideoEncodeH264ReferenceInfo* ref, int idr, unsigned frame_num, int poc);

#endif
