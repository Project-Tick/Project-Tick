# vi:set ts=8 sts=4 sw=4 et fdm=marker:
#
# danish.nsi: Danish language strings for gmnv NSIS installer.
#
# Locale ID    : 1030
# Locale Name  : da
# fileencoding : UTF-8
# Author       : scootergrisen

!insertmacro MUI_LANGUAGE "Danish"


# Overwrite the default translation.
# These strings should be always English.  Otherwise dosinst.c fails.
LangString ^SetupCaption     ${LANG_DANISH}         "$(^Name) Setup"
LangString ^UninstallCaption ${LANG_DANISH}         "$(^Name) Uninstall"

##############################################################################
# Translated license file for the license page                            {{{1
##############################################################################

LicenseLangString page_lic_file 0 "..\lang\LICENSE.nsis.txt"
#LicenseLangString page_lic_file ${LANG_DANISH} "..\lang\LICENSE.da.nsis.txt"

##############################################################################
# Translated README.txt file, which is opened after installation          {{{1
##############################################################################

LangString mnv_readme_file 0 "README.txt"
#LangString mnv_readme_file ${LANG_DANISH} "README.da.txt"

##############################################################################
# MUI Configuration Strings                                               {{{1
##############################################################################

#LangString str_dest_folder          ${LANG_DANISH}     "Destinationsmappe (skal slutte med $\"mnv$\")"

LangString str_show_readme          ${LANG_DANISH}     "Vis README efter installationen er gennemført"

# Install types:
LangString str_type_typical         ${LANG_DANISH}     "Typisk"

LangString str_type_minimal         ${LANG_DANISH}     "Minimal"

LangString str_type_full            ${LANG_DANISH}     "Fuld"


##############################################################################
# Section Titles & Description                                            {{{1
##############################################################################

LangString str_section_old_ver      ${LANG_DANISH}     "Afinstaller eksisterende version(er)"
LangString str_desc_old_ver         ${LANG_DANISH}     "Afinstaller eksisterende MNV-version(er) fra dit system."

LangString str_section_exe          ${LANG_DANISH}     "MNV GUI og afviklingsfiler"
LangString str_desc_exe             ${LANG_DANISH}     "MNV GUI-eksekverbare- og afviklingsfiler. Komponenten kræves."

LangString str_section_console      ${LANG_DANISH}     "MNV-konsolprogram"
LangString str_desc_console         ${LANG_DANISH}     "Konsolversion af MNV (mnv.exe)."

LangString str_section_batch        ${LANG_DANISH}     "Opret .bat-filer"
LangString str_desc_batch           ${LANG_DANISH}     "Opret .bat-filer til MNV-varianter i Windows-mappen til brug fra kommandolinjen."

LangString str_group_icons          ${LANG_DANISH}     "Opret ikoner til MNV"
LangString str_desc_icons           ${LANG_DANISH}     "Opret ikoner til MNV diverse steder for at hjælpe med at gøre adgangen let."

LangString str_section_desktop      ${LANG_DANISH}     "På skrivebordet"
LangString str_desc_desktop         ${LANG_DANISH}     "Opret ikoner til gMNV-eksekverbare på skrivebordet."

LangString str_section_start_menu   ${LANG_DANISH}     "I Programmer-mappen i menuen Start"
LangString str_desc_start_menu      ${LANG_DANISH}     "Tilføj MNV i Programmer-mappen i menuen Start."

#LangString str_section_quick_launch ${LANG_DANISH}     "I værktøjslinjen Hurtig start"
#LangString str_desc_quick_launch    ${LANG_DANISH}     "Tilføj MNV-genvej i værktøjslinjen Hurtig start."

LangString str_section_edit_with    ${LANG_DANISH}     "Tilføj MNV-genvejsmenu"
LangString str_desc_edit_with       ${LANG_DANISH}     "Tilføj MNV til listen i $\"Åbn med...$\"-genvejsmenuen."

#LangString str_section_edit_with32  ${LANG_DANISH}     "32-bit-version"
#LangString str_desc_edit_with32     ${LANG_DANISH}     "Tilføj MNV til listen i $\"Åbn med...$\"-genvejsmenuen for 32-bit-programmer."

#LangString str_section_edit_with64  ${LANG_DANISH}     "64-bit-version"
#LangString str_desc_edit_with64     ${LANG_DANISH}     "Tilføj MNV til listen i $\"Åbn med...$\"-genvejsmenuen for 64-bit-programmer."

LangString str_section_mnv_rc       ${LANG_DANISH}     "Opret standardkonfiguration"
LangString str_desc_mnv_rc          ${LANG_DANISH}     "Opret en standardkonfigurationsfil (_mnvrc) hvis der ikke allerede findes en."

LangString str_group_plugin         ${LANG_DANISH}     "Opret plugin-mapper"
LangString str_desc_plugin          ${LANG_DANISH}     "Opret plugin-mapper. Plugin-mapper giver mulighed for at udvide MNV ved at slippe en fil i en mappen."

LangString str_section_plugin_home  ${LANG_DANISH}     "Private"
LangString str_desc_plugin_home     ${LANG_DANISH}     "Opret plugin-mapper i HOME (hvis du har defineret et) eller MNV-installationsmappe."

LangString str_section_plugin_mnv   ${LANG_DANISH}     "Delte"
LangString str_desc_plugin_mnv      ${LANG_DANISH}     "Opret plugin-mapper i MNV-installationsmappe, det bruges af alle på systemet."

LangString str_section_nls          ${LANG_DANISH}     "Understøttelse af modersmål"
LangString str_desc_nls             ${LANG_DANISH}     "Installer filer til understøttelse af modersmål."

LangString str_unsection_register   ${LANG_DANISH}     "Afregistrer MNV"
LangString str_desc_unregister      ${LANG_DANISH}     "Afregistrer MNV fra systemet."

LangString str_unsection_exe        ${LANG_DANISH}     "Fjern MNV-eksekverbare-/afviklingsfiler"
LangString str_desc_rm_exe          ${LANG_DANISH}     "Fjern alle MNV-eksekverbare- og afviklingsfiler."

LangString str_ungroup_plugin       ${LANG_DANISH}     "Fjern plugin-mapper"
LangString str_desc_rm_plugin       ${LANG_DANISH}     "Fjern plugin-mapperne, hvis de er tomme."

LangString str_unsection_plugin_home ${LANG_DANISH}     "Private"
LangString str_desc_rm_plugin_home  ${LANG_DANISH}     "Fjern plugin-mapperne fra HOME-mappen."

LangString str_unsection_plugin_mnv ${LANG_DANISH}     "Delte"
LangString str_desc_rm_plugin_mnv   ${LANG_DANISH}     "Fjern plugin-mapperne fra MNV-installationsmappen."

LangString str_unsection_rootdir    ${LANG_DANISH}     "Fjern MNV-rodmappen"
LangString str_desc_rm_rootdir      ${LANG_DANISH}     "Fjern MNV-rodmappen. Den indeholder dine MNV-konfigurationsfiler!"


##############################################################################
# Messages                                                                {{{1
##############################################################################

#LangString str_msg_too_many_ver  ${LANG_DANISH}     "Fandt $mnv_old_ver_count MNV-versioner på dit system.$\r$\nInstallationsguiden kan højst håndtere ${MNV_MAX_OLD_VER}-versioner.$\r$\nFjern venligst nogle versioner og start igen."

#LangString str_msg_invalid_root  ${LANG_DANISH}     "Ugyldig installationssti: $mnv_install_root!$\r$\nDen skal slutte med $\"mnv$\"."

#LangString str_msg_bin_mismatch  ${LANG_DANISH}     "Uoverensstemmelse i binære sti!$\r$\n$\r$\nForventede at den binære sti var $\"$mnv_bin_path$\",$\r$\nmen systemet indikerer at den binære sti er $\"$INSTDIR$\"."

#LangString str_msg_mnv_running   ${LANG_DANISH}     "MNV kører stadig på dit system.$\r$\nLuk venligst alle instanser af MNV inden du fortsætter."

#LangString str_msg_register_ole  ${LANG_DANISH}     "Forsøger at registrere MNV med OLE. Der er ingen meddelelse til at indikere om det virker eller ej."

#LangString str_msg_unreg_ole     ${LANG_DANISH}     "Forsøger at afregistrere MNV med OLE. Der er ingen meddelelse til at indikere om det virker eller ej."

#LangString str_msg_rm_start      ${LANG_DANISH}     "Afinstallerer følgende version:"

#LangString str_msg_rm_fail       ${LANG_DANISH}     "Kunne ikke afinstallere følgende version:"

#LangString str_msg_no_rm_key     ${LANG_DANISH}     "Kan ikke finde registreringsdatabasenøgle for afinstallationsguiden."

#LangString str_msg_no_rm_reg     ${LANG_DANISH}     "Kan ikke finde afinstallationsguiden fra registreringsdatabasen."

#LangString str_msg_no_rm_exe     ${LANG_DANISH}     "Kan ikke tilgå afinstallationsguide."

#LangString str_msg_rm_copy_fail  ${LANG_DANISH}     "Kunne ikke kopiere afinstallationsguide til midlertidig mappe."

#LangString str_msg_rm_run_fail   ${LANG_DANISH}     "Kunne ikke køre afinstallationsguide."

#LangString str_msg_abort_install ${LANG_DANISH}     "Installationsguiden vil afbryde."

LangString str_msg_install_fail  ${LANG_DANISH}     "Installationen mislykkedes. Bedre held næste gang."

LangString str_msg_rm_exe_fail   ${LANG_DANISH}     "Nogle filer i $0 er ikke blevet slettet!$\r$\nDu skal gøre det manuelt."

#LangString str_msg_rm_root_fail  ${LANG_DANISH}     "ADVARSEL: Kan ikke fjerne $\"$mnv_install_root$\", den er ikke tom!"

LangString str_msg_uninstalling  ${LANG_DANISH}     "Afinstallerer den gamle version..."

LangString str_msg_registering   ${LANG_DANISH}     "Registrerer..."

LangString str_msg_unregistering ${LANG_DANISH}     "Afregistrerer..."


##############################################################################
# Dialog Box                                                              {{{1
##############################################################################

LangString str_mnvrc_page_title    ${LANG_DANISH}     "Vælg _mnvrc-indstillinger"
LangString str_mnvrc_page_subtitle ${LANG_DANISH}     "Vælg indstillingerne til forbedring, tastatur og mus."

LangString str_msg_compat_title    ${LANG_DANISH}     " Vi- / MNV-opførsel "
LangString str_msg_compat_desc     ${LANG_DANISH}     "&Kompatibilitet og forbedringer"
LangString str_msg_compat_vi       ${LANG_DANISH}     "Vi-kompatibel"
LangString str_msg_compat_mnv      ${LANG_DANISH}     "MNV original"
LangString str_msg_compat_defaults ${LANG_DANISH}     "MNV med nogle forbedringer (indlæs defaults.mnv)"
LangString str_msg_compat_all      ${LANG_DANISH}     "MNV med alle forbedringer (indlæs mnvrc_example.mnv) (standard)"

LangString str_msg_keymap_title   ${LANG_DANISH}     " Tilknytninger "
LangString str_msg_keymap_desc    ${LANG_DANISH}     "&Gentilknyt nogle få taster for Windows (Ctrl-V, Ctrl-C, Ctrl-A, Ctrl-S, Ctrl-F osv.)"
LangString str_msg_keymap_default ${LANG_DANISH}     "Gentilknyt ikke taster (standard)"
LangString str_msg_keymap_windows ${LANG_DANISH}     "Gentilknyt nogle få taster"

LangString str_msg_mouse_title   ${LANG_DANISH}     " Mus "
LangString str_msg_mouse_desc    ${LANG_DANISH}     "&Opførsel af højre og venstre knapper"
LangString str_msg_mouse_default ${LANG_DANISH}     "Højre: genvejsmenu, venstre: visuel tilstand (standard)"
LangString str_msg_mouse_windows ${LANG_DANISH}     "Højre: genvejsmenu, venstre: vælg-tilstand (Windows)"
LangString str_msg_mouse_unix    ${LANG_DANISH}     "Højre: udvider markering, venstre: visuel tilstand (Unix)"
