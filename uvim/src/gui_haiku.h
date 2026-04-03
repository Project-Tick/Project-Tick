/* vi:set ts=8 sts=4 sw=4:
 *
 * MNV - MNV is not Vim		by Bram Moolenaar
 *				GUI support by Olaf "Rhialto" Seibert
 *
 * Do ":help uganda"  in MNV to read copying and usage conditions.
 * Do ":help credits" in MNV to see a list of people who contributed.
 *
 * Haiku GUI.
 *
 * Based on "GUI support for the Buzzword Enhanced Operating System for PPC."
 *
 */

/*
 * This file must be acceptable both as C and C++.
 * The BeOS API is defined in terms of C++, but some classes
 * should be somewhat known in the common C code.
 */

// System classes

struct BMenu;
struct BMenuItem;
struct BPictureButton;

// Our own MNV-related classes

struct MNVApp;
struct MNVFormView;
struct MNVTextAreaView;
struct MNVWindow;
struct MNVScrollBar;

// Locking functions

extern int mnv_lock_screen();
extern void mnv_unlock_screen();

#ifndef __cplusplus

typedef struct BMenu BMenu;
typedef struct BMenuItem BMenuItem;
typedef struct BPictureButton BPictureButton;
typedef struct MNVWindow MNVWindow;
typedef struct MNVFormView MNVFormView;
typedef struct MNVTextAreaView MNVTextAreaView;
typedef struct MNVApp MNVApp;
typedef struct MNVScrollBar MNVScrollBar;

#endif
