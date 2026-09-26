#ifndef TRINITY_VIDEO_H
#define TRINITY_VIDEO_H
#include <vulkan/vulkan.h>
#include <vk_video/vulkan_video_codec_h264std_encode.h>

// the h.264 standard structs are bitfields, which silver cannot write: filled here
void h264_sps       (StdVideoH264SequenceParameterSet* sps, StdVideoH264SequenceParameterSetVui* vui,
                     int coded_w, int coded_h, int width, int height, int fps);
void h264_pps       (StdVideoH264PictureParameterSet* pps);
void h264_picture   (StdVideoEncodeH264PictureInfo* pic, StdVideoEncodeH264ReferenceListsInfo* refs,
                     int idr, unsigned frame_num, int poc, int idr_pic_id, int ref_slot);
void h264_slice     (StdVideoEncodeH264SliceHeader* slice, int idr);
void h264_reference (StdVideoEncodeH264ReferenceInfo* ref, int idr, unsigned frame_num, int poc);

// h.264 decode, the host half: parse, reference slots, order
#include <vk_video/vulkan_video_codec_h264std_decode.h>
#include <stdint.h>

#define H264D_SLOTS 17

typedef struct H264Dec H264Dec;

H264Dec* h264d_new      (void);
void     h264d_free     (H264Dec* d);
// avcC record: nal length size, sps and pps
int      h264d_config   (H264Dec* d, const uint8_t* avcc, int n);
// one length-prefixed sample: 1 when a picture is ready
int      h264d_sample   (H264Dec* d, const uint8_t* data, int n, int64_t pts);
// the sps/pps set changed since the last session params
int      h264d_params_dirty (H264Dec* d);
void     h264d_params   (H264Dec* d, VkVideoDecodeH264SessionParametersAddInfoKHR* add);
int      h264d_profile  (H264Dec* d);
int      h264d_width    (H264Dec* d);
int      h264d_height   (H264Dec* d);
int      h264d_coded_w  (H264Dec* d);
int      h264d_coded_h  (H264Dec* d);
// annex b bytes of the ready picture, written at dst
int      h264d_bits_size (H264Dec* d);
void     h264d_bits_write(H264Dec* d, uint8_t* dst);
// begin and decode infos for the ready picture
void     h264d_vk       (H264Dec* d, VkImageView dpb, VkBuffer bits, uint64_t range,
                         VkVideoSessionKHR session, VkVideoSessionParametersKHR params,
                         VkVideoBeginCodingInfoKHR* begin, VkVideoDecodeInfoKHR* info);
// after the gpu decode: marking and display order
void     h264d_decoded  (H264Dec* d);
// the next picture in display order: its slot, or -1
int      h264d_output   (H264Dec* d, int64_t* pts);
// end of stream: every waiting picture goes out
void     h264d_flush    (H264Dec* d);
// nv12 at coded size to y, u, v planes at display size
void     nv12_split     (const uint8_t* src, int coded_w, int coded_h, int w, int h,
                         uint8_t* y, uint8_t* u, uint8_t* v);
// limited-range y, u, v planes to rgba8, bt.709 or bt.601
void     yuv_rgba       (const uint8_t* y, const uint8_t* u, const uint8_t* v, int w, int h,
                         uint8_t* dst, int bt709);

#endif
