/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * MNV - MNV is not Vim	by Bram Moolenaar
 *
 * QNX port by Julian Kinraid
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 */

/*
 * os_qnx.c
 */

#include "mnv.h"


#if defined(FEAT_GUI_PHOTON)
int is_photon_available;
#endif

void qnx_init(void)
{
#if defined(FEAT_GUI_PHOTON)
    PhChannelParms_t parms;

    CLEAR_FIELD(parms);
    parms.flags = Ph_DYNAMIC_BUFFER;

    is_photon_available = (PhAttach(NULL, &parms) != NULL) ? TRUE : FALSE;
#endif
}

#if defined(FEAT_GUI_PHOTON) && defined(FEAT_CLIPBOARD)

# define CLIP_TYPE_MNV "MNVTYPE"
# define CLIP_TYPE_TEXT "TEXT"

// Turn on the clipboard for a console mnv when photon is running
void qnx_clip_init(void)
{
    if (is_photon_available == TRUE && !gui.in_use)
	clip_init(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// Clipboard

// No support for owning the clipboard
int
clip_mch_own_selection(Clipboard_T *cbd)
{
    return FALSE;
}

void
clip_mch_lose_selection(Clipboard_T *cbd)
{
}

void
clip_mch_request_selection(Clipboard_T *cbd)
{
    int		    type = MLINE, clip_length = 0, is_type_set = FALSE;
    void	    *cbdata;
    PhClipHeader    *clip_header;
    char_u	    *clip_text = NULL;

    cbdata = PhClipboardPasteStart(PhInputGroup(NULL));
    if (cbdata == NULL)
	return;

    // Look for the mnv specific clip first
    clip_header = PhClipboardPasteType(cbdata, CLIP_TYPE_MNV);
    if (clip_header != NULL && clip_header->data != NULL)
    {
	switch(*(char *) clip_header->data)
	{
	    default: // fallthrough to line type
	    case 'L': type = MLINE; break;
	    case 'C': type = MCHAR; break;
	    case 'B': type = MBLOCK; break;
	}
	is_type_set = TRUE;
    }

    // Try for just normal text
    clip_header = PhClipboardPasteType(cbdata, CLIP_TYPE_TEXT);
    if (clip_header != NULL)
    {
	clip_text = clip_header->data;
	clip_length  = clip_header->length - 1;

	if (clip_text != NULL && is_type_set == FALSE)
	    type = MAUTO;
    }

    if ((clip_text != NULL) && (clip_length > 0))
	clip_yank_selection(type, clip_text, clip_length, cbd);

    PhClipboardPasteFinish(cbdata);
}

void
clip_mch_set_selection(Clipboard_T *cbd)
{
    int type;
    long_u  len;
    char_u *text_clip, mnv_clip[2], *str = NULL;
    PhClipHeader clip_header[2];

    // Prevent recursion from clip_get_selection()
    if (cbd->owned == TRUE)
	return;

    cbd->owned = TRUE;
    clip_get_selection(cbd);
    cbd->owned = FALSE;

    type = clip_convert_selection(&str, &len, cbd);
    if (type >= 0)
    {
	text_clip = alloc(len + 1); // Normal text

	if (text_clip && mnv_clip)
	{
	    CLEAR_FIELD(clip_header);

	    STRNCPY(clip_header[0].type, CLIP_TYPE_MNV, 8);
	    clip_header[0].length = sizeof(mnv_clip);
	    clip_header[0].data   = mnv_clip;

	    STRNCPY(clip_header[1].type, CLIP_TYPE_TEXT, 8);
	    clip_header[1].length = len + 1;
	    clip_header[1].data   = text_clip;

	    switch(type)
	    {
		default: // fallthrough to MLINE
		case MLINE:	*mnv_clip = 'L'; break;
		case MCHAR:	*mnv_clip = 'C'; break;
		case MBLOCK:	*mnv_clip = 'B'; break;
	    }

	    mnv_strncpy(text_clip, str, len);

	    mnv_clip[ 1 ] = NUL;

	    PhClipboardCopy(PhInputGroup(NULL), 2, clip_header);
	}
	mnv_free(text_clip);
    }
    mnv_free(str);
}
#endif
