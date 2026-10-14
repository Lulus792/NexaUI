#include "unicode_data.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(NX_UCD_VERSION_MAJOR == 18u);
    assert(NX_UCD_VERSION_MINOR == 0u);
    assert(NX_UCD_VERSION_PATCH == 0u);

    assert(nx_ucd_gcb('A') == NX_UCD_GCB_OTHER);
    assert(nx_ucd_gcb('\r') == NX_UCD_GCB_CR);
    assert(nx_ucd_gcb('\n') == NX_UCD_GCB_LF);
    assert(nx_ucd_gcb(0x0301u) == NX_UCD_GCB_EXTEND);
    assert(nx_ucd_gcb(0x200Du) == NX_UCD_GCB_ZWJ);
    assert(nx_ucd_gcb(0x1F1E6u) == NX_UCD_GCB_REGIONAL_INDICATOR);

    assert(nx_ucd_wb('A') == NX_UCD_WB_ALETTER);
    assert(nx_ucd_wb('7') == NX_UCD_WB_NUMERIC);
    assert(nx_ucd_wb(0x05D0u) == NX_UCD_WB_HEBREW_LETTER);
    assert(nx_ucd_wb(0x30A2u) == NX_UCD_WB_KATAKANA);

    assert(nx_ucd_lb('\n') == NX_UCD_LB_LF);
    assert(nx_ucd_lb(' ') == NX_UCD_LB_SP);
    assert(nx_ucd_lb(0x2FFFDu) == NX_UCD_LB_ID);
    assert(nx_ucd_lb(0x1F8FFu) == NX_UCD_LB_XX);
    assert(nx_ucd_lb(0x10FFFFu) == NX_UCD_LB_XX);
    assert(nx_ucd_gc(0x2018u) == NX_UCD_GC_PI);
    assert(nx_ucd_gc(0x2019u) == NX_UCD_GC_PF);
    assert(nx_ucd_gc(0x0378u) == NX_UCD_GC_CN);
    assert(nx_ucd_gc(0xD800u) == NX_UCD_GC_CS);
    assert(nx_ucd_eaw(0x3008u) == NX_UCD_EAW_W);
    assert(nx_ucd_eaw('A') == NX_UCD_EAW_NA);
    assert(nx_ucd_eaw(0x2FFFDu) == NX_UCD_EAW_W);

    assert(nx_ucd_ep('A') == NX_UCD_EP_NO);
    assert(nx_ucd_ep(0x1F600u) == NX_UCD_EP_YES);
    assert(nx_ucd_incb(0x094Du) == NX_UCD_INCB_LINKER);
    assert(nx_ucd_incb('A') == NX_UCD_INCB_NONE);

    assert(nx_ucd_bidi('A') == NX_UCD_BIDI_L);
    assert(nx_ucd_bidi(0x05D0u) == NX_UCD_BIDI_R);
    assert(nx_ucd_bidi(0x0627u) == NX_UCD_BIDI_AL);
    assert(nx_ucd_bidi(0x10FFFu) == NX_UCD_BIDI_R);
    assert(nx_ucd_bidi(0x10FFFFu) == NX_UCD_BIDI_BN);
    assert(nx_ucd_script_tag('A') == UINT32_C(0x4c61746e));
    assert(nx_ucd_script_tag(0x05D0u) == UINT32_C(0x48656272));
    assert(nx_ucd_script_tag(0x03A9u) == UINT32_C(0x4772656b));
    assert(nx_ucd_script_tag(0x10FFFFu) == UINT32_C(0x5a7a7a7a));
    assert(nx_ucd_di(0xFE0Fu) == NX_UCD_DI_YES);
    assert(nx_ucd_di('A') == NX_UCD_DI_NO);

    puts("unicode data passed");
    return 0;
}
