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

// ---- h.264 decode ----
#include <stdlib.h>

#define H264D_PS 32
#define H264D_SLICES 512

typedef struct {
    const uint8_t* p;
    int n;
    int bit;
} Bits;

static uint32_t bits_u(Bits* b, int k) {
    uint32_t v = 0;
    while (k-- > 0) {
        int byte = b->bit >> 3;
        int one  = byte < b->n ? (b->p[byte] >> (7 - (b->bit & 7))) & 1 : 0;
        v = (v << 1) | (uint32_t)one;
        b->bit++;
    }
    return v;
}

static uint32_t bits_ue(Bits* b) {
    int zeros = 0;
    while (zeros < 31 && bits_u(b, 1) == 0)
        zeros++;
    return ((1u << zeros) - 1) + bits_u(b, zeros);
}

static int32_t bits_se(Bits* b) {
    uint32_t k = bits_ue(b);
    return (k & 1) ? (int32_t)((k + 1) / 2) : -(int32_t)(k / 2);
}

// rbsp trailing bits start at the last set bit
static int bits_more(Bits* b) {
    int last = b->n - 1;
    while (last >= 0 && b->p[last] == 0)
        last--;
    if (last < 0)
        return 0;
    int stop = last * 8 + 7;
    while (!((b->p[last] >> (7 - (stop & 7))) & 1))
        stop--;
    return b->bit < stop;
}

// emulation prevention bytes out
static int rbsp(const uint8_t* s, int n, uint8_t* d, int cap) {
    int o = 0, zeros = 0;
    for (int i = 0; i < n && o < cap; i++) {
        if (zeros >= 2 && s[i] == 3) {
            zeros = 0;
            continue;
        }
        zeros = s[i] ? 0 : zeros + 1;
        d[o++] = s[i];
    }
    return o;
}

typedef struct {
    int valid;
    StdVideoH264SequenceParameterSet std;
    StdVideoH264SequenceParameterSetVui vui;
    StdVideoH264ScalingLists lists;
    int32_t ref_offsets[256];
    int profile_idc;
    int reorder;
} Sps;

typedef struct {
    int valid;
    StdVideoH264PictureParameterSet std;
    StdVideoH264ScalingLists lists;
} Pps;

typedef struct {
    int in_use;
    int short_ref;
    int long_ref;
    int long_idx;
    int frame_num;
    int top;
    int bottom;
    int output;
    int queued;
    int64_t pts;
} Slot;

typedef struct {
    int first_mb;
    int type;
    int pps_id;
    int frame_num;
    int idr_pic_id;
    int poc_lsb;
    int delta_bottom;
    int delta[2];
    int long_term;
    int adaptive;
    int mmco_n;
    int mmco[66][2];
} Slice;

struct H264Dec {
    int nal_len;
    Sps sps[H264D_PS];
    Pps pps[H264D_PS];
    int dirty;
    int active_sps;
    StdVideoH264SequenceParameterSet add_sps[H264D_PS];
    StdVideoH264PictureParameterSet add_pps[H264D_PS];
    Slot slot[H264D_SLOTS];
    int max_long_idx;
    // poc state
    int prev_msb, prev_lsb, prev_frame_num, prev_offset, prev_ref_frame_num;
    // the ready picture
    int ready, cur, idr, ref_idc, intra, mmco5;
    Slice head;
    int top, bottom, frame_offset;
    int64_t pts;
    uint8_t* bits;
    int bits_n, bits_cap;
    uint32_t offsets[H264D_SLICES];
    int slices;
    // output fifo of slots
    int queue[64];
    int64_t queue_pts[64];
    int q_head, q_n;
    // vulkan structs the begin/decode infos point at
    StdVideoDecodeH264PictureInfo pic;
    VkVideoDecodeH264PictureInfoKHR pic_vk;
    StdVideoDecodeH264ReferenceInfo refs[H264D_SLOTS];
    VkVideoDecodeH264DpbSlotInfoKHR dpb[H264D_SLOTS];
    VkVideoPictureResourceInfoKHR res[H264D_SLOTS];
    VkVideoReferenceSlotInfoKHR slots_vk[H264D_SLOTS];
    VkVideoReferenceSlotInfoKHR begin_slots[H264D_SLOTS];
    StdVideoDecodeH264ReferenceInfo setup_ref;
    VkVideoDecodeH264DpbSlotInfoKHR setup_dpb;
    VkVideoPictureResourceInfoKHR setup_res;
    VkVideoReferenceSlotInfoKHR setup_slot;
    uint8_t scratch[4096];
};

H264Dec* h264d_new(void) {
    H264Dec* d = calloc(1, sizeof(H264Dec));
    d->nal_len = 4;
    d->active_sps = -1;
    d->max_long_idx = -1;
    return d;
}

void h264d_free(H264Dec* d) {
    if (!d)
        return;
    free(d->bits);
    free(d);
}

static void scaling_list(Bits* b, uint8_t* list, int size, uint16_t* defaults, int index) {
    int last = 8, next = 8;
    for (int j = 0; j < size; j++) {
        if (next) {
            next = (last + bits_se(b) + 256) % 256;
            if (j == 0 && next == 0) {
                *defaults |= (uint16_t)(1 << index);
                return;
            }
        }
        list[j] = (uint8_t)(next ? next : last);
        last = list[j];
    }
}

static void scaling_lists(Bits* b, StdVideoH264ScalingLists* l, int count) {
    for (int i = 0; i < count; i++) {
        if (!bits_u(b, 1))
            continue;
        l->scaling_list_present_mask |= (uint16_t)(1 << i);
        if (i < 6)
            scaling_list(b, l->ScalingList4x4[i], 16, &l->use_default_scaling_matrix_mask, i);
        else
            scaling_list(b, l->ScalingList8x8[i - 6], 64, &l->use_default_scaling_matrix_mask, i);
    }
}

static void skip_hrd(Bits* b) {
    int n = (int)bits_ue(b) + 1;
    bits_u(b, 8);
    for (int i = 0; i < n; i++) {
        bits_ue(b);
        bits_ue(b);
        bits_u(b, 1);
    }
    bits_u(b, 20);
}

static StdVideoH264LevelIdc level_of(int idc) {
    switch (idc) {
    case 9: case 10: return STD_VIDEO_H264_LEVEL_IDC_1_0;
    case 11: return STD_VIDEO_H264_LEVEL_IDC_1_1;
    case 12: return STD_VIDEO_H264_LEVEL_IDC_1_2;
    case 13: return STD_VIDEO_H264_LEVEL_IDC_1_3;
    case 20: return STD_VIDEO_H264_LEVEL_IDC_2_0;
    case 21: return STD_VIDEO_H264_LEVEL_IDC_2_1;
    case 22: return STD_VIDEO_H264_LEVEL_IDC_2_2;
    case 30: return STD_VIDEO_H264_LEVEL_IDC_3_0;
    case 31: return STD_VIDEO_H264_LEVEL_IDC_3_1;
    case 32: return STD_VIDEO_H264_LEVEL_IDC_3_2;
    case 40: return STD_VIDEO_H264_LEVEL_IDC_4_0;
    case 41: return STD_VIDEO_H264_LEVEL_IDC_4_1;
    case 42: return STD_VIDEO_H264_LEVEL_IDC_4_2;
    case 50: return STD_VIDEO_H264_LEVEL_IDC_5_0;
    case 51: return STD_VIDEO_H264_LEVEL_IDC_5_1;
    case 52: return STD_VIDEO_H264_LEVEL_IDC_5_2;
    case 60: return STD_VIDEO_H264_LEVEL_IDC_6_0;
    case 61: return STD_VIDEO_H264_LEVEL_IDC_6_1;
    default: return STD_VIDEO_H264_LEVEL_IDC_6_2;
    }
}

static void parse_sps(H264Dec* d, const uint8_t* nal, int n) {
    int len = rbsp(nal + 1, n - 1, d->scratch, sizeof(d->scratch));
    Bits b = { d->scratch, len, 0 };
    Sps s;
    memset(&s, 0, sizeof(s));
    StdVideoH264SequenceParameterSet* p = &s.std;
    s.profile_idc = (int)bits_u(&b, 8);
    p->profile_idc = (StdVideoH264ProfileIdc)s.profile_idc;
    uint32_t cons = bits_u(&b, 8);
    p->flags.constraint_set0_flag = (cons >> 7) & 1;
    p->flags.constraint_set1_flag = (cons >> 6) & 1;
    p->flags.constraint_set2_flag = (cons >> 5) & 1;
    p->flags.constraint_set3_flag = (cons >> 4) & 1;
    p->flags.constraint_set4_flag = (cons >> 3) & 1;
    p->flags.constraint_set5_flag = (cons >> 2) & 1;
    p->level_idc = level_of((int)bits_u(&b, 8));
    int id = (int)bits_ue(&b);
    if (id >= H264D_PS)
        return;
    p->seq_parameter_set_id = (uint8_t)id;
    p->chroma_format_idc = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420;
    int pi = s.profile_idc;
    if (pi == 100 || pi == 110 || pi == 122 || pi == 244 || pi == 44 || pi == 83 ||
        pi == 86 || pi == 118 || pi == 128 || pi == 138 || pi == 139 || pi == 134 || pi == 135) {
        int cf = (int)bits_ue(&b);
        p->chroma_format_idc = (StdVideoH264ChromaFormatIdc)cf;
        if (cf == 3)
            p->flags.separate_colour_plane_flag = bits_u(&b, 1);
        p->bit_depth_luma_minus8 = (uint8_t)bits_ue(&b);
        p->bit_depth_chroma_minus8 = (uint8_t)bits_ue(&b);
        p->flags.qpprime_y_zero_transform_bypass_flag = bits_u(&b, 1);
        p->flags.seq_scaling_matrix_present_flag = bits_u(&b, 1);
        if (p->flags.seq_scaling_matrix_present_flag)
            scaling_lists(&b, &s.lists, cf != 3 ? 8 : 12);
    }
    p->log2_max_frame_num_minus4 = (uint8_t)bits_ue(&b);
    p->pic_order_cnt_type = (StdVideoH264PocType)bits_ue(&b);
    if (p->pic_order_cnt_type == 0)
        p->log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)bits_ue(&b);
    else if (p->pic_order_cnt_type == 1) {
        p->flags.delta_pic_order_always_zero_flag = bits_u(&b, 1);
        p->offset_for_non_ref_pic = bits_se(&b);
        p->offset_for_top_to_bottom_field = bits_se(&b);
        int cycle = (int)bits_ue(&b);
        if (cycle > 255)
            return;
        p->num_ref_frames_in_pic_order_cnt_cycle = (uint8_t)cycle;
        for (int i = 0; i < cycle; i++)
            s.ref_offsets[i] = bits_se(&b);
    }
    p->max_num_ref_frames = (uint8_t)bits_ue(&b);
    p->flags.gaps_in_frame_num_value_allowed_flag = bits_u(&b, 1);
    p->pic_width_in_mbs_minus1 = bits_ue(&b);
    p->pic_height_in_map_units_minus1 = bits_ue(&b);
    p->flags.frame_mbs_only_flag = bits_u(&b, 1);
    if (!p->flags.frame_mbs_only_flag)
        p->flags.mb_adaptive_frame_field_flag = bits_u(&b, 1);
    p->flags.direct_8x8_inference_flag = bits_u(&b, 1);
    p->flags.frame_cropping_flag = bits_u(&b, 1);
    if (p->flags.frame_cropping_flag) {
        p->frame_crop_left_offset = bits_ue(&b);
        p->frame_crop_right_offset = bits_ue(&b);
        p->frame_crop_top_offset = bits_ue(&b);
        p->frame_crop_bottom_offset = bits_ue(&b);
    }
    s.reorder = p->max_num_ref_frames;
    p->flags.vui_parameters_present_flag = bits_u(&b, 1);
    if (p->flags.vui_parameters_present_flag) {
        StdVideoH264SequenceParameterSetVui* v = &s.vui;
        v->flags.aspect_ratio_info_present_flag = bits_u(&b, 1);
        if (v->flags.aspect_ratio_info_present_flag) {
            v->aspect_ratio_idc = (StdVideoH264AspectRatioIdc)bits_u(&b, 8);
            if (v->aspect_ratio_idc == 255) {
                v->sar_width = (uint16_t)bits_u(&b, 16);
                v->sar_height = (uint16_t)bits_u(&b, 16);
            }
        }
        v->flags.overscan_info_present_flag = bits_u(&b, 1);
        if (v->flags.overscan_info_present_flag)
            v->flags.overscan_appropriate_flag = bits_u(&b, 1);
        v->flags.video_signal_type_present_flag = bits_u(&b, 1);
        if (v->flags.video_signal_type_present_flag) {
            v->video_format = (uint8_t)bits_u(&b, 3);
            v->flags.video_full_range_flag = bits_u(&b, 1);
            v->flags.color_description_present_flag = bits_u(&b, 1);
            if (v->flags.color_description_present_flag) {
                v->colour_primaries = (uint8_t)bits_u(&b, 8);
                v->transfer_characteristics = (uint8_t)bits_u(&b, 8);
                v->matrix_coefficients = (uint8_t)bits_u(&b, 8);
            }
        }
        v->flags.chroma_loc_info_present_flag = bits_u(&b, 1);
        if (v->flags.chroma_loc_info_present_flag) {
            v->chroma_sample_loc_type_top_field = (uint8_t)bits_ue(&b);
            v->chroma_sample_loc_type_bottom_field = (uint8_t)bits_ue(&b);
        }
        v->flags.timing_info_present_flag = bits_u(&b, 1);
        if (v->flags.timing_info_present_flag) {
            v->num_units_in_tick = bits_u(&b, 32);
            v->time_scale = bits_u(&b, 32);
            v->flags.fixed_frame_rate_flag = bits_u(&b, 1);
        }
        int nal_hrd = (int)bits_u(&b, 1);
        if (nal_hrd)
            skip_hrd(&b);
        int vcl_hrd = (int)bits_u(&b, 1);
        if (vcl_hrd)
            skip_hrd(&b);
        if (nal_hrd || vcl_hrd)
            bits_u(&b, 1);
        bits_u(&b, 1);
        v->flags.bitstream_restriction_flag = bits_u(&b, 1);
        if (v->flags.bitstream_restriction_flag) {
            bits_u(&b, 1);
            bits_ue(&b);
            bits_ue(&b);
            bits_ue(&b);
            bits_ue(&b);
            v->max_num_reorder_frames = (uint8_t)bits_ue(&b);
            v->max_dec_frame_buffering = (uint8_t)bits_ue(&b);
            s.reorder = v->max_num_reorder_frames;
        }
    }
    // baseline has no b frames: nothing waits to reorder
    if (pi == 66 && !p->flags.vui_parameters_present_flag)
        s.reorder = 0;
    s.valid = 1;
    d->sps[id] = s;
    d->dirty = 1;
}

static void parse_pps(H264Dec* d, const uint8_t* nal, int n) {
    int len = rbsp(nal + 1, n - 1, d->scratch, sizeof(d->scratch));
    Bits b = { d->scratch, len, 0 };
    Pps s;
    memset(&s, 0, sizeof(s));
    StdVideoH264PictureParameterSet* p = &s.std;
    int id = (int)bits_ue(&b);
    int sid = (int)bits_ue(&b);
    if (id >= H264D_PS || sid >= H264D_PS)
        return;
    p->pic_parameter_set_id = (uint8_t)id;
    p->seq_parameter_set_id = (uint8_t)sid;
    p->flags.entropy_coding_mode_flag = bits_u(&b, 1);
    p->flags.bottom_field_pic_order_in_frame_present_flag = bits_u(&b, 1);
    if (bits_ue(&b) != 0)
        return;
    p->num_ref_idx_l0_default_active_minus1 = (uint8_t)bits_ue(&b);
    p->num_ref_idx_l1_default_active_minus1 = (uint8_t)bits_ue(&b);
    p->flags.weighted_pred_flag = bits_u(&b, 1);
    p->weighted_bipred_idc = (StdVideoH264WeightedBipredIdc)bits_u(&b, 2);
    p->pic_init_qp_minus26 = (int8_t)bits_se(&b);
    p->pic_init_qs_minus26 = (int8_t)bits_se(&b);
    p->chroma_qp_index_offset = (int8_t)bits_se(&b);
    p->flags.deblocking_filter_control_present_flag = bits_u(&b, 1);
    p->flags.constrained_intra_pred_flag = bits_u(&b, 1);
    p->flags.redundant_pic_cnt_present_flag = bits_u(&b, 1);
    p->second_chroma_qp_index_offset = p->chroma_qp_index_offset;
    if (bits_more(&b)) {
        p->flags.transform_8x8_mode_flag = bits_u(&b, 1);
        p->flags.pic_scaling_matrix_present_flag = bits_u(&b, 1);
        if (p->flags.pic_scaling_matrix_present_flag) {
            int cf = d->sps[sid].valid ? d->sps[sid].std.chroma_format_idc : 1;
            scaling_lists(&b, &s.lists, 6 + (p->flags.transform_8x8_mode_flag ? (cf != 3 ? 2 : 6) : 0));
        }
        p->second_chroma_qp_index_offset = (int8_t)bits_se(&b);
    }
    s.valid = 1;
    d->pps[id] = s;
    d->dirty = 1;
}

// up to dec_ref_pic_marking; the gpu reads the rest
static int parse_slice(H264Dec* d, const uint8_t* nal, int n, Slice* sl) {
    int len = rbsp(nal + 1, n - 1, d->scratch, sizeof(d->scratch));
    Bits b = { d->scratch, len, 0 };
    int ref_idc = (nal[0] >> 5) & 3;
    int idr = (nal[0] & 31) == 5;
    memset(sl, 0, sizeof(*sl));
    sl->first_mb = (int)bits_ue(&b);
    sl->type = (int)(bits_ue(&b) % 5);
    sl->pps_id = (int)bits_ue(&b);
    if (sl->pps_id >= H264D_PS || !d->pps[sl->pps_id].valid)
        return 0;
    Pps* pps = &d->pps[sl->pps_id];
    Sps* sps = &d->sps[pps->std.seq_parameter_set_id];
    if (!sps->valid)
        return 0;
    StdVideoH264SequenceParameterSet* s = &sps->std;
    if (s->flags.separate_colour_plane_flag)
        bits_u(&b, 2);
    sl->frame_num = (int)bits_u(&b, s->log2_max_frame_num_minus4 + 4);
    // field pictures: not supported, frames only
    if (!s->flags.frame_mbs_only_flag && bits_u(&b, 1))
        return 0;
    if (idr)
        sl->idr_pic_id = (int)bits_ue(&b);
    if (s->pic_order_cnt_type == 0) {
        sl->poc_lsb = (int)bits_u(&b, s->log2_max_pic_order_cnt_lsb_minus4 + 4);
        if (pps->std.flags.bottom_field_pic_order_in_frame_present_flag)
            sl->delta_bottom = bits_se(&b);
    }
    if (s->pic_order_cnt_type == 1 && !s->flags.delta_pic_order_always_zero_flag) {
        sl->delta[0] = bits_se(&b);
        if (pps->std.flags.bottom_field_pic_order_in_frame_present_flag)
            sl->delta[1] = bits_se(&b);
    }
    if (pps->std.flags.redundant_pic_cnt_present_flag)
        bits_ue(&b);
    int is_b = sl->type == 1, is_p = sl->type == 0 || sl->type == 3;
    if (is_b)
        bits_u(&b, 1);
    int l0 = pps->std.num_ref_idx_l0_default_active_minus1 + 1;
    int l1 = pps->std.num_ref_idx_l1_default_active_minus1 + 1;
    if (is_p || is_b) {
        if (bits_u(&b, 1)) {
            l0 = (int)bits_ue(&b) + 1;
            if (is_b)
                l1 = (int)bits_ue(&b) + 1;
        }
    }
    for (int list = 0; list < (is_b ? 2 : (is_p ? 1 : 0)); list++) {
        if (!bits_u(&b, 1))
            continue;
        for (int guard = 0; guard < 64; guard++) {
            uint32_t op = bits_ue(&b);
            if (op == 3)
                break;
            bits_ue(&b);
        }
    }
    int chroma = s->flags.separate_colour_plane_flag ? 0 : s->chroma_format_idc;
    if ((pps->std.flags.weighted_pred_flag && is_p) || (pps->std.weighted_bipred_idc == 1 && is_b)) {
        bits_ue(&b);
        if (chroma)
            bits_ue(&b);
        for (int list = 0; list < (is_b ? 2 : 1); list++) {
            int count = list ? l1 : l0;
            for (int i = 0; i < count; i++) {
                if (bits_u(&b, 1)) {
                    bits_se(&b);
                    bits_se(&b);
                }
                if (chroma && bits_u(&b, 1)) {
                    for (int j = 0; j < 4; j++)
                        bits_se(&b);
                }
            }
        }
    }
    if (ref_idc) {
        if (idr) {
            bits_u(&b, 1);
            sl->long_term = (int)bits_u(&b, 1);
        } else if ((sl->adaptive = (int)bits_u(&b, 1))) {
            for (sl->mmco_n = 0; sl->mmco_n < 66; sl->mmco_n++) {
                int op = (int)bits_ue(&b);
                if (op == 0)
                    break;
                int v = 0;
                if (op == 1 || op == 3 || op == 2 || op == 6 || op == 4)
                    v = (int)bits_ue(&b);
                sl->mmco[sl->mmco_n][0] = op;
                sl->mmco[sl->mmco_n][1] = v;
                // op 3 carries a second value: the long-term index
                if (op == 3)
                    sl->mmco[sl->mmco_n][1] = v | ((int)bits_ue(&b) << 16);
            }
        }
    }
    return 1;
}

static int max_frame_num(H264Dec* d) {
    return 1 << (d->sps[d->active_sps].std.log2_max_frame_num_minus4 + 4);
}

static void compute_poc(H264Dec* d) {
    StdVideoH264SequenceParameterSet* s = &d->sps[d->active_sps].std;
    Slice* sl = &d->head;
    int max_fn = max_frame_num(d);
    if (s->pic_order_cnt_type == 0) {
        if (d->idr) {
            d->prev_msb = 0;
            d->prev_lsb = 0;
        }
        int max_lsb = 1 << (s->log2_max_pic_order_cnt_lsb_minus4 + 4);
        int msb = d->prev_msb;
        if (sl->poc_lsb < d->prev_lsb && d->prev_lsb - sl->poc_lsb >= max_lsb / 2)
            msb = d->prev_msb + max_lsb;
        else if (sl->poc_lsb > d->prev_lsb && sl->poc_lsb - d->prev_lsb > max_lsb / 2)
            msb = d->prev_msb - max_lsb;
        d->top = msb + sl->poc_lsb;
        d->bottom = d->top + sl->delta_bottom;
        if (d->ref_idc) {
            d->prev_msb = msb;
            d->prev_lsb = sl->poc_lsb;
        }
        return;
    }
    int offset = 0;
    if (!d->idr)
        offset = d->prev_frame_num > sl->frame_num ? d->prev_offset + max_fn : d->prev_offset;
    d->frame_offset = offset;
    if (s->pic_order_cnt_type == 1) {
        int n = s->num_ref_frames_in_pic_order_cnt_cycle;
        int abs = n ? offset + sl->frame_num : 0;
        if (!d->ref_idc && abs > 0)
            abs--;
        int expected = 0;
        if (abs > 0) {
            int32_t* off = d->sps[d->active_sps].ref_offsets;
            int per_cycle = 0;
            for (int i = 0; i < n; i++)
                per_cycle += off[i];
            int cycles = (abs - 1) / n, in_cycle = (abs - 1) % n;
            expected = cycles * per_cycle;
            for (int i = 0; i <= in_cycle; i++)
                expected += off[i];
        }
        if (!d->ref_idc)
            expected += s->offset_for_non_ref_pic;
        d->top = expected + sl->delta[0];
        d->bottom = d->top + s->offset_for_top_to_bottom_field + sl->delta[1];
        return;
    }
    int poc = d->idr ? 0 : 2 * (offset + sl->frame_num) - (d->ref_idc ? 0 : 1);
    d->top = d->bottom = poc;
}

static void queue_push(H264Dec* d, int slot) {
    if (d->q_n >= 64)
        return;
    int at = (d->q_head + d->q_n) % 64;
    d->queue[at] = slot;
    d->queue_pts[at] = d->slot[slot].pts;
    d->slot[slot].queued = 1;
    d->q_n++;
}

// the waiting picture with the lowest order count goes out
static int bump(H264Dec* d) {
    int best = -1;
    for (int i = 0; i < H264D_SLOTS; i++) {
        Slot* s = &d->slot[i];
        if (!s->output)
            continue;
        int poc = s->top < s->bottom ? s->top : s->bottom;
        int bpoc = best < 0 ? 0 : (d->slot[best].top < d->slot[best].bottom ? d->slot[best].top : d->slot[best].bottom);
        if (best < 0 || poc < bpoc)
            best = i;
    }
    if (best < 0)
        return 0;
    d->slot[best].output = 0;
    queue_push(d, best);
    return 1;
}

static void refresh_use(H264Dec* d) {
    for (int i = 0; i < H264D_SLOTS; i++) {
        Slot* s = &d->slot[i];
        s->in_use = s->short_ref || s->long_ref || s->output || s->queued;
    }
}

int h264d_config(H264Dec* d, const uint8_t* avcc, int n) {
    if (n < 7 || avcc[0] != 1)
        return 0;
    d->nal_len = (avcc[4] & 3) + 1;
    int at = 5;
    int count = avcc[at++] & 31;
    for (int i = 0; i < count && at + 2 <= n; i++) {
        int len = (avcc[at] << 8) | avcc[at + 1];
        at += 2;
        if (at + len > n)
            return 0;
        parse_sps(d, avcc + at, len);
        at += len;
    }
    if (at >= n)
        return 0;
    count = avcc[at++];
    for (int i = 0; i < count && at + 2 <= n; i++) {
        int len = (avcc[at] << 8) | avcc[at + 1];
        at += 2;
        if (at + len > n)
            return 0;
        parse_pps(d, avcc + at, len);
        at += len;
    }
    for (int i = 0; i < H264D_PS; i++) {
        if (d->sps[i].valid) {
            d->active_sps = i;
            break;
        }
    }
    return d->active_sps >= 0;
}

static void add_bits(H264Dec* d, const uint8_t* nal, int n) {
    if (d->bits_n + n + 3 > d->bits_cap) {
        d->bits_cap = (d->bits_n + n + 3) * 2;
        d->bits = realloc(d->bits, (size_t)d->bits_cap);
    }
    if (d->slices < H264D_SLICES)
        d->offsets[d->slices++] = (uint32_t)d->bits_n;
    d->bits[d->bits_n++] = 0;
    d->bits[d->bits_n++] = 0;
    d->bits[d->bits_n++] = 1;
    memcpy(d->bits + d->bits_n, nal, (size_t)n);
    d->bits_n += n;
}

int h264d_sample(H264Dec* d, const uint8_t* data, int n, int64_t pts) {
    d->ready = 0;
    d->bits_n = 0;
    d->slices = 0;
    d->intra = 1;
    int have = 0;
    for (int at = 0; at + d->nal_len <= n;) {
        int len = 0;
        for (int i = 0; i < d->nal_len; i++)
            len = (len << 8) | data[at + i];
        at += d->nal_len;
        if (len <= 0 || at + len > n)
            break;
        const uint8_t* nal = data + at;
        at += len;
        int type = nal[0] & 31;
        if (type == 7)
            parse_sps(d, nal, len);
        else if (type == 8)
            parse_pps(d, nal, len);
        else if (type == 1 || type == 5) {
            Slice sl;
            if (!parse_slice(d, nal, len, &sl))
                continue;
            if (!have) {
                d->head = sl;
                d->idr = type == 5;
                d->ref_idc = (nal[0] >> 5) & 3;
                have = 1;
            }
            if (sl.type != 2 && sl.type != 4)
                d->intra = 0;
            add_bits(d, nal, len);
        }
    }
    if (!have)
        return 0;
    d->active_sps = d->pps[d->head.pps_id].std.seq_parameter_set_id;
    d->pts = pts;
    d->mmco5 = 0;
    for (int i = 0; i < d->head.mmco_n; i++)
        if (d->head.mmco[i][0] == 5)
            d->mmco5 = 1;
    // an idr or a full reset: what waits goes out first
    if (d->idr || d->mmco5) {
        while (bump(d))
            ;
        if (d->idr) {
            for (int i = 0; i < H264D_SLOTS; i++)
                d->slot[i].short_ref = d->slot[i].long_ref = 0;
            d->max_long_idx = d->head.long_term ? 0 : -1;
        }
    }
    compute_poc(d);
    // a slot frees once the caller takes its queued picture
    for (int tries = 0; tries <= H264D_SLOTS; tries++) {
        int free_slot = 0;
        for (int i = 0; i < H264D_SLOTS; i++) {
            Slot* t = &d->slot[i];
            if (!t->short_ref && !t->long_ref && !t->output)
                free_slot = 1;
        }
        if (free_slot || !bump(d))
            break;
    }
    d->cur = -1;
    d->ready = 1;
    return 1;
}

int h264d_params_dirty(H264Dec* d) {
    return d->dirty;
}

void h264d_params(H264Dec* d, VkVideoDecodeH264SessionParametersAddInfoKHR* add) {
    int ns = 0, np = 0;
    for (int i = 0; i < H264D_PS; i++) {
        Sps* s = &d->sps[i];
        if (!s->valid)
            continue;
        d->add_sps[ns] = s->std;
        d->add_sps[ns].pOffsetForRefFrame = s->ref_offsets;
        d->add_sps[ns].pScalingLists = s->std.flags.seq_scaling_matrix_present_flag ? &s->lists : NULL;
        d->add_sps[ns].pSequenceParameterSetVui = s->std.flags.vui_parameters_present_flag ? &s->vui : NULL;
        ns++;
    }
    for (int i = 0; i < H264D_PS; i++) {
        Pps* p = &d->pps[i];
        if (!p->valid)
            continue;
        d->add_pps[np] = p->std;
        d->add_pps[np].pScalingLists = p->std.flags.pic_scaling_matrix_present_flag ? &p->lists : NULL;
        np++;
    }
    memset(add, 0, sizeof(*add));
    add->sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR;
    add->stdSPSCount = (uint32_t)ns;
    add->pStdSPSs = d->add_sps;
    add->stdPPSCount = (uint32_t)np;
    add->pStdPPSs = d->add_pps;
    d->dirty = 0;
}

static StdVideoH264SequenceParameterSet* active(H264Dec* d) {
    return d->active_sps >= 0 ? &d->sps[d->active_sps].std : NULL;
}

int h264d_profile(H264Dec* d) {
    return d->active_sps >= 0 ? d->sps[d->active_sps].profile_idc : 0;
}

int h264d_coded_w(H264Dec* d) {
    StdVideoH264SequenceParameterSet* s = active(d);
    return s ? (int)(s->pic_width_in_mbs_minus1 + 1) * 16 : 0;
}

int h264d_coded_h(H264Dec* d) {
    StdVideoH264SequenceParameterSet* s = active(d);
    return s ? (int)(s->pic_height_in_map_units_minus1 + 1) * 16 * (s->flags.frame_mbs_only_flag ? 1 : 2) : 0;
}

int h264d_width(H264Dec* d) {
    StdVideoH264SequenceParameterSet* s = active(d);
    if (!s)
        return 0;
    int sub = s->chroma_format_idc == 1 || s->chroma_format_idc == 2 ? 2 : 1;
    return h264d_coded_w(d) - sub * (int)(s->frame_crop_left_offset + s->frame_crop_right_offset);
}

int h264d_height(H264Dec* d) {
    StdVideoH264SequenceParameterSet* s = active(d);
    if (!s)
        return 0;
    int sub = s->chroma_format_idc == 1 ? 2 : 1;
    sub *= s->flags.frame_mbs_only_flag ? 1 : 2;
    return h264d_coded_h(d) - sub * (int)(s->frame_crop_top_offset + s->frame_crop_bottom_offset);
}

int h264d_bits_size(H264Dec* d) {
    return d->ready ? d->bits_n : 0;
}

void h264d_bits_write(H264Dec* d, uint8_t* dst) {
    memcpy(dst, d->bits, (size_t)d->bits_n);
}

static void resource(VkVideoPictureResourceInfoKHR* r, VkImageView view, int layer, int w, int h) {
    memset(r, 0, sizeof(*r));
    r->sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
    r->codedExtent.width = (uint32_t)w;
    r->codedExtent.height = (uint32_t)h;
    r->baseArrayLayer = (uint32_t)layer;
    r->imageViewBinding = view;
}

static void reference_of(H264Dec* d, int i, StdVideoDecodeH264ReferenceInfo* r) {
    Slot* s = &d->slot[i];
    memset(r, 0, sizeof(*r));
    r->flags.used_for_long_term_reference = s->long_ref ? 1 : 0;
    r->FrameNum = (uint16_t)(s->long_ref ? s->long_idx : s->frame_num);
    r->PicOrderCnt[0] = s->top;
    r->PicOrderCnt[1] = s->bottom;
}

void h264d_vk(H264Dec* d, VkImageView dpb, VkBuffer bits, uint64_t range,
              VkVideoSessionKHR session, VkVideoSessionParametersKHR params,
              VkVideoBeginCodingInfoKHR* begin, VkVideoDecodeInfoKHR* info) {
    int w = h264d_coded_w(d), h = h264d_coded_h(d);
    refresh_use(d);
    d->cur = 0;
    for (int i = 0; i < H264D_SLOTS; i++) {
        if (!d->slot[i].in_use) {
            d->cur = i;
            break;
        }
    }
    memset(&d->pic, 0, sizeof(d->pic));
    d->pic.flags.IdrPicFlag = d->idr ? 1 : 0;
    d->pic.flags.is_intra = d->intra ? 1 : 0;
    d->pic.flags.is_reference = d->ref_idc ? 1 : 0;
    d->pic.seq_parameter_set_id = (uint8_t)d->active_sps;
    d->pic.pic_parameter_set_id = (uint8_t)d->head.pps_id;
    d->pic.frame_num = (uint16_t)d->head.frame_num;
    d->pic.idr_pic_id = (uint16_t)d->head.idr_pic_id;
    d->pic.PicOrderCnt[0] = d->top;
    d->pic.PicOrderCnt[1] = d->bottom;
    memset(&d->pic_vk, 0, sizeof(d->pic_vk));
    d->pic_vk.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR;
    d->pic_vk.pStdPictureInfo = &d->pic;
    d->pic_vk.sliceCount = (uint32_t)d->slices;
    d->pic_vk.pSliceOffsets = d->offsets;

    int nref = 0;
    for (int i = 0; i < H264D_SLOTS; i++) {
        if (i == d->cur || !(d->slot[i].short_ref || d->slot[i].long_ref))
            continue;
        reference_of(d, i, &d->refs[nref]);
        memset(&d->dpb[nref], 0, sizeof(d->dpb[nref]));
        d->dpb[nref].sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
        d->dpb[nref].pStdReferenceInfo = &d->refs[nref];
        resource(&d->res[nref], dpb, i, w, h);
        memset(&d->slots_vk[nref], 0, sizeof(d->slots_vk[nref]));
        d->slots_vk[nref].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
        d->slots_vk[nref].pNext = &d->dpb[nref];
        d->slots_vk[nref].slotIndex = i;
        d->slots_vk[nref].pPictureResource = &d->res[nref];
        d->begin_slots[nref] = d->slots_vk[nref];
        nref++;
    }
    memset(&d->setup_ref, 0, sizeof(d->setup_ref));
    d->setup_ref.FrameNum = (uint16_t)d->head.frame_num;
    d->setup_ref.PicOrderCnt[0] = d->top;
    d->setup_ref.PicOrderCnt[1] = d->bottom;
    memset(&d->setup_dpb, 0, sizeof(d->setup_dpb));
    d->setup_dpb.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
    d->setup_dpb.pStdReferenceInfo = &d->setup_ref;
    resource(&d->setup_res, dpb, d->cur, w, h);
    memset(&d->setup_slot, 0, sizeof(d->setup_slot));
    d->setup_slot.sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
    d->setup_slot.pNext = &d->setup_dpb;
    d->setup_slot.slotIndex = d->cur;
    d->setup_slot.pPictureResource = &d->setup_res;
    // the slot being set up is bound with no picture yet
    d->begin_slots[nref] = d->setup_slot;
    d->begin_slots[nref].slotIndex = -1;

    memset(begin, 0, sizeof(*begin));
    begin->sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR;
    begin->videoSession = session;
    begin->videoSessionParameters = params;
    begin->referenceSlotCount = (uint32_t)nref + 1;
    begin->pReferenceSlots = d->begin_slots;

    memset(info, 0, sizeof(*info));
    info->sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR;
    info->pNext = &d->pic_vk;
    info->srcBuffer = bits;
    info->srcBufferOffset = 0;
    info->srcBufferRange = range;
    info->dstPictureResource = d->setup_res;
    info->pSetupReferenceSlot = &d->setup_slot;
    info->referenceSlotCount = (uint32_t)nref;
    info->pReferenceSlots = d->slots_vk;
}

static int frame_num_wrap(H264Dec* d, Slot* s) {
    return s->frame_num > d->head.frame_num ? s->frame_num - max_frame_num(d) : s->frame_num;
}

static void unmark_long_idx(H264Dec* d, int idx, int keep) {
    for (int i = 0; i < H264D_SLOTS; i++)
        if (i != keep && d->slot[i].long_ref && d->slot[i].long_idx == idx)
            d->slot[i].long_ref = 0;
}

static void mark(H264Dec* d) {
    Slot* cur = &d->slot[d->cur];
    StdVideoH264SequenceParameterSet* s = active(d);
    int as_long = 0;
    if (d->idr) {
        if (d->head.long_term) {
            as_long = 1;
            cur->long_idx = 0;
        }
    } else if (d->head.adaptive) {
        for (int k = 0; k < d->head.mmco_n; k++) {
            int op = d->head.mmco[k][0], v = d->head.mmco[k][1] & 0xffff;
            int pic_num = d->head.frame_num - (v + 1);
            if (op == 1 || op == 3) {
                for (int i = 0; i < H264D_SLOTS; i++) {
                    Slot* t = &d->slot[i];
                    if (i == d->cur || !t->short_ref || frame_num_wrap(d, t) != pic_num)
                        continue;
                    t->short_ref = 0;
                    if (op == 3) {
                        int idx = d->head.mmco[k][1] >> 16;
                        unmark_long_idx(d, idx, i);
                        t->long_ref = 1;
                        t->long_idx = idx;
                    }
                }
            } else if (op == 2) {
                for (int i = 0; i < H264D_SLOTS; i++)
                    if (i != d->cur && d->slot[i].long_ref && d->slot[i].long_idx == v)
                        d->slot[i].long_ref = 0;
            } else if (op == 4) {
                d->max_long_idx = v - 1;
                for (int i = 0; i < H264D_SLOTS; i++)
                    if (d->slot[i].long_ref && d->slot[i].long_idx > d->max_long_idx)
                        d->slot[i].long_ref = 0;
            } else if (op == 5) {
                for (int i = 0; i < H264D_SLOTS; i++)
                    if (i != d->cur)
                        d->slot[i].short_ref = d->slot[i].long_ref = 0;
                d->max_long_idx = -1;
            } else if (op == 6) {
                unmark_long_idx(d, v, d->cur);
                as_long = 1;
                cur->long_idx = v;
            }
        }
    } else {
        int shorts = 0, longs = 0, oldest = -1;
        for (int i = 0; i < H264D_SLOTS; i++) {
            if (i == d->cur)
                continue;
            if (d->slot[i].short_ref) {
                shorts++;
                if (oldest < 0 || frame_num_wrap(d, &d->slot[i]) < frame_num_wrap(d, &d->slot[oldest]))
                    oldest = i;
            }
            if (d->slot[i].long_ref)
                longs++;
        }
        int limit = s->max_num_ref_frames > 0 ? s->max_num_ref_frames : 1;
        if (shorts + longs >= limit && oldest >= 0)
            d->slot[oldest].short_ref = 0;
    }
    if (as_long)
        cur->long_ref = 1;
    else
        cur->short_ref = 1;
}

void h264d_decoded(H264Dec* d) {
    if (!d->ready)
        return;
    Slot* cur = &d->slot[d->cur];
    cur->short_ref = cur->long_ref = 0;
    cur->frame_num = d->head.frame_num;
    cur->top = d->top;
    cur->bottom = d->bottom;
    cur->pts = d->pts;
    if (d->ref_idc)
        mark(d);
    if (d->mmco5) {
        int low = d->top < d->bottom ? d->top : d->bottom;
        cur->top -= low;
        cur->bottom -= low;
        cur->frame_num = 0;
        d->prev_msb = 0;
        d->prev_lsb = cur->top;
    }
    cur->output = 1;
    refresh_use(d);
    int waiting = 0;
    for (int i = 0; i < H264D_SLOTS; i++)
        waiting += d->slot[i].output;
    int reorder = d->sps[d->active_sps].reorder;
    while (waiting > reorder && bump(d))
        waiting--;
    d->prev_frame_num = d->mmco5 ? 0 : d->head.frame_num;
    d->prev_offset = d->mmco5 ? 0 : d->frame_offset;
    d->ready = 0;
}

int h264d_output(H264Dec* d, int64_t* pts) {
    if (d->q_n == 0)
        return -1;
    int slot = d->queue[d->q_head];
    if (pts)
        *pts = d->queue_pts[d->q_head];
    d->q_head = (d->q_head + 1) % 64;
    d->q_n--;
    d->slot[slot].queued = 0;
    refresh_use(d);
    return slot;
}

void h264d_flush(H264Dec* d) {
    while (bump(d))
        ;
}

static uint8_t clamp8(int v) {
    return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

void nv12_split(const uint8_t* src, int coded_w, int coded_h, int w, int h,
                uint8_t* y, uint8_t* u, uint8_t* v) {
    const uint8_t* uv = src + (size_t)coded_w * coded_h;
    for (int r = 0; r < h; r++)
        memcpy(y + (size_t)r * w, src + (size_t)r * coded_w, (size_t)w);
    int cw = w / 2, ch = h / 2;
    for (int r = 0; r < ch; r++) {
        const uint8_t* row = uv + (size_t)r * coded_w;
        for (int x = 0; x < cw; x++) {
            u[(size_t)r * cw + x] = row[x * 2];
            v[(size_t)r * cw + x] = row[x * 2 + 1];
        }
    }
}

void yuv_rgba(const uint8_t* yp, const uint8_t* up, const uint8_t* vp, int w, int h,
              uint8_t* dst, int bt709) {
    // 16.16 fixed point of the limited-range matrix
    int ky = 76309;
    int rv = bt709 ? 117489 : 104597, gu = bt709 ? 13975 : 25675;
    int gv = bt709 ? 34925 : 53279, bu = bt709 ? 138438 : 132201;
    int cw = w / 2;
    for (int y = 0; y < h; y++) {
        uint8_t* out = dst + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            int l = (yp[(size_t)y * w + x] - 16) * ky;
            size_t c = (size_t)(y / 2) * cw + x / 2;
            int u = up[c] - 128, v = vp[c] - 128;
            out[x * 4 + 0] = clamp8((l + rv * v + 32768) >> 16);
            out[x * 4 + 1] = clamp8((l - gu * u - gv * v + 32768) >> 16);
            out[x * 4 + 2] = clamp8((l + bu * u + 32768) >> 16);
            out[x * 4 + 3] = 255;
        }
    }
}
