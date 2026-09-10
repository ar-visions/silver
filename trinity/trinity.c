#include "video.h"
#include <string.h>

void h264_sps(StdVideoH264SequenceParameterSet* sps, StdVideoH264SequenceParameterSetVui* vui,
              int coded_w, int coded_h, int width, int height, int fps) {
    memset(sps, 0, sizeof(*sps));
    memset(vui, 0, sizeof(*vui));
    sps->profile_idc                       = STD_VIDEO_H264_PROFILE_IDC_HIGH;
    sps->level_idc                         = STD_VIDEO_H264_LEVEL_IDC_5_1;
    sps->chroma_format_idc                 = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420;
    sps->log2_max_frame_num_minus4         = 4;
    sps->pic_order_cnt_type                = STD_VIDEO_H264_POC_TYPE_0;
    sps->log2_max_pic_order_cnt_lsb_minus4 = 4;
    sps->max_num_ref_frames                = 1;
    sps->pic_width_in_mbs_minus1           = coded_w / 16 - 1;
    sps->pic_height_in_map_units_minus1    = coded_h / 16 - 1;
    sps->flags.frame_mbs_only_flag         = 1;
    sps->flags.direct_8x8_inference_flag   = 1;
    sps->flags.vui_parameters_present_flag = 1;
    // the coded size is a whole number of macroblocks; crop units are 2 px for 4:2:0
    if (coded_w != width || coded_h != height) {
        sps->flags.frame_cropping_flag = 1;
        sps->frame_crop_right_offset   = (coded_w - width)  / 2;
        sps->frame_crop_bottom_offset  = (coded_h - height) / 2;
    }
    vui->flags.timing_info_present_flag           = 1;
    vui->flags.fixed_frame_rate_flag              = 1;
    vui->num_units_in_tick                        = 1;
    vui->time_scale                               = fps * 2;
    vui->flags.video_signal_type_present_flag     = 1;
    vui->flags.color_description_present_flag     = 1;
    vui->video_format                             = 5;
    vui->colour_primaries                         = 1;
    vui->transfer_characteristics                 = 1;
    vui->matrix_coefficients                      = 1;
    sps->pSequenceParameterSetVui                 = vui;
}

void h264_pps(StdVideoH264PictureParameterSet* pps) {
    memset(pps, 0, sizeof(*pps));
    pps->flags.entropy_coding_mode_flag               = 1;
    pps->flags.deblocking_filter_control_present_flag = 1;
    pps->flags.transform_8x8_mode_flag                = 1;
}

void h264_picture(StdVideoEncodeH264PictureInfo* pic, StdVideoEncodeH264ReferenceListsInfo* refs,
                  int idr, unsigned frame_num, int poc, int idr_pic_id, int ref_slot) {
    memset(pic,  0, sizeof(*pic));
    memset(refs, 0, sizeof(*refs));
    memset(refs->RefPicList0, STD_VIDEO_H264_NO_REFERENCE_PICTURE, sizeof(refs->RefPicList0));
    memset(refs->RefPicList1, STD_VIDEO_H264_NO_REFERENCE_PICTURE, sizeof(refs->RefPicList1));
    pic->flags.IdrPicFlag   = idr ? 1 : 0;
    pic->flags.is_reference = 1;
    pic->primary_pic_type   = idr ? STD_VIDEO_H264_PICTURE_TYPE_IDR : STD_VIDEO_H264_PICTURE_TYPE_P;
    pic->frame_num          = frame_num;
    pic->PicOrderCnt        = poc;
    pic->idr_pic_id         = (uint16_t)idr_pic_id;
    if (!idr) refs->RefPicList0[0] = (uint8_t)ref_slot;
    pic->pRefLists = refs;
}

void h264_slice(StdVideoEncodeH264SliceHeader* slice, int idr) {
    memset(slice, 0, sizeof(*slice));
    slice->slice_type                    = idr ? STD_VIDEO_H264_SLICE_TYPE_I : STD_VIDEO_H264_SLICE_TYPE_P;
    slice->cabac_init_idc                = STD_VIDEO_H264_CABAC_INIT_IDC_0;
    slice->disable_deblocking_filter_idc = STD_VIDEO_H264_DISABLE_DEBLOCKING_FILTER_IDC_DISABLED;
}

void h264_reference(StdVideoEncodeH264ReferenceInfo* ref, int idr, unsigned frame_num, int poc) {
    memset(ref, 0, sizeof(*ref));
    ref->primary_pic_type = idr ? STD_VIDEO_H264_PICTURE_TYPE_IDR : STD_VIDEO_H264_PICTURE_TYPE_P;
    ref->FrameNum         = frame_num;
    ref->PicOrderCnt      = poc;
}
