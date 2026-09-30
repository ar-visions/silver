#ifndef TRINITY_VIDEO_H
#define TRINITY_VIDEO_H
#include <vulkan/vulkan.h>
#include <vk_video/vulkan_video_codec_h264std_encode.h>

// the h.264 standard structs are bitfields, which silver cannot write: filled here
void h264_sps       (StdVideoH264SequenceParameterSet* sps, StdVideoH264SequenceParameterSetVui* vui,
                     int coded_w, int coded_h, int width, int height, int fps,
                     int chroma444, int lossless);
void h264_pps       (StdVideoH264PictureParameterSet* pps, int chroma444);
void h264_picture   (StdVideoEncodeH264PictureInfo* pic, StdVideoEncodeH264ReferenceListsInfo* refs,
                     int idr, unsigned frame_num, int poc, int idr_pic_id, int ref_slot);
void h264_slice     (StdVideoEncodeH264SliceHeader* slice, int idr);
void h264_reference (StdVideoEncodeH264ReferenceInfo* ref, int idr, unsigned frame_num, int poc);

// h.265 encode: the same, for full-colour (4:4:4) takes
#include <vk_video/vulkan_video_codec_h265std_encode.h>

typedef struct H265Params {
    StdVideoH265ProfileTierLevel        ptl;
    StdVideoH265DecPicBufMgr            dpbm;
    StdVideoH265VideoParameterSet       vps;
    StdVideoH265SequenceParameterSetVui vui;
    StdVideoH265SequenceParameterSet    sps;
    StdVideoH265PictureParameterSet     pps;
} H265Params;

typedef struct H265Picture {
    StdVideoEncodeH265PictureInfo         pic;
    StdVideoEncodeH265ReferenceListsInfo  refs;
    StdVideoH265ShortTermRefPicSet        rps;
    StdVideoEncodeH265SliceSegmentHeader  slice;
    StdVideoEncodeH265ReferenceInfo       ref_cur;
    StdVideoEncodeH265ReferenceInfo       ref_prev;
} H265Picture;

// C owns these (silver's view of a nested C struct's layout is not
// trusted for pointers into it); freed with h265_free
// ctb and transform sizes as log2; chroma444 picks format range
// extensions, lossless sets transquant bypass
H265Params*  h265_params  (int coded_w, int coded_h, int width, int height,
                           int fps, int chroma444, int lossless, int level,
                           int log2_ctb, int log2_min_tb, int log2_max_tb);
void h265_add_info (H265Params* p, VkVideoEncodeH265SessionParametersAddInfoKHR* add);
H265Picture* h265_picture_new (void);
// idr: poc 0, no references; else one reference in ref_slot
void h265_picture (H265Picture* h, int idr, int poc, int prev_poc, int ref_slot);
void h265_picture_vk (H265Picture* h, VkVideoEncodeH265DpbSlotInfoKHR* cur,
                      VkVideoEncodeH265DpbSlotInfoKHR* prev,
                      VkVideoEncodeH265NaluSliceSegmentInfoKHR* nalu,
                      VkVideoEncodeH265PictureInfoKHR* pic);
void h265_free (void* p);

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
// h.265 decode: the same shape as h264d_*
#include <vk_video/vulkan_video_codec_h265std_decode.h>
typedef struct H265Dec H265Dec;
H265Dec* h265d_new      (void);
void     h265d_free     (H265Dec* d);
// hvcC record: nal length size, vps, sps and pps
int      h265d_config   (H265Dec* d, const uint8_t* hvcc, int n);
int      h265d_sample   (H265Dec* d, const uint8_t* data, int n, int64_t pts);
int      h265d_params_dirty (H265Dec* d);
void     h265d_params   (H265Dec* d, VkVideoDecodeH265SessionParametersAddInfoKHR* add);
int      h265d_profile  (H265Dec* d);
// 1 for 4:2:0, 3 for 4:4:4
int      h265d_chroma   (H265Dec* d);
int      h265d_width    (H265Dec* d);
int      h265d_height   (H265Dec* d);
int      h265d_coded_w  (H265Dec* d);
int      h265d_coded_h  (H265Dec* d);
int      h265d_bits_size (H265Dec* d);
void     h265d_bits_write(H265Dec* d, uint8_t* dst);
void     h265d_vk       (H265Dec* d, VkImageView dpb, VkBuffer bits, uint64_t range,
                         VkVideoSessionKHR session, VkVideoSessionParametersKHR params,
                         VkVideoBeginCodingInfoKHR* begin, VkVideoDecodeInfoKHR* info);
void     h265d_decoded  (H265Dec* d);
int      h265d_output   (H265Dec* d, int64_t* pts);
void     h265d_flush    (H265Dec* d);
void     yuv444_rgba    (const uint8_t* y, const uint8_t* u, const uint8_t* v, int w, int h,
                         uint8_t* dst, int bt709);
// 4:4:4 two-plane at coded size to full-size y, u, v planes
void     nv24_split     (const uint8_t* src, int coded_w, int coded_h, int w, int h,
                         uint8_t* y, uint8_t* u, uint8_t* v);

// nv12 at coded size to y, u, v planes at display size
void     nv12_split     (const uint8_t* src, int coded_w, int coded_h, int w, int h,
                         uint8_t* y, uint8_t* u, uint8_t* v);
// limited-range y, u, v planes to rgba8, bt.709 or bt.601
void     yuv_rgba       (const uint8_t* y, const uint8_t* u, const uint8_t* v, int w, int h,
                         uint8_t* dst, int bt709);

#endif
