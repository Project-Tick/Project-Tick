# vi:set ts=8 sts=4 sw=4 et fdm=marker:
#
# italian.nsi : Italian language strings for gmnv NSIS installer.
#
# Locale ID    : 1040
# Locale Name  : it
# fileencoding : UTF-8
# Author       : Antonio Colombo, bovirus - revision: 12.05.2023

!insertmacro MUI_LANGUAGE "Italian"


# Overwrite the default translation.
# These strings should be always English.  Otherwise dosinst.c fails.
LangString ^SetupCaption     ${LANG_ITALIAN} \
        "$(^Name) Setup"
LangString ^UninstallCaption ${LANG_ITALIAN} \
        "$(^Name) Uninstall"

##############################################################################
# Translated license file for the license page                            {{{1
##############################################################################

LicenseLangString page_lic_file ${LANG_ITALIAN} "..\lang\LICENSE.it.nsis.txt"

##############################################################################
# Translated README.txt file, which is opened after installation          {{{1
##############################################################################

LangString mnv_readme_file ${LANG_ITALIAN} "README.it.txt"

##############################################################################
# MUI Configuration Strings                                               {{{1
##############################################################################

#LangString str_dest_folder          ${LANG_ITALIAN} \
#    "Cartella installazione (il percorso deve finire con $\"mnv$\")"

LangString str_show_readme          ${LANG_ITALIAN} \
    "Visualizza file README a fine installazione"

# Install types:
LangString str_type_typical         ${LANG_ITALIAN} \
    "Tipica"

LangString str_type_minimal         ${LANG_ITALIAN} \
    "Minima"

LangString str_type_full            ${LANG_ITALIAN} \
    "Completa"


##############################################################################
# Section Titles & Description                                            {{{1
##############################################################################

LangString str_section_old_ver      ${LANG_ITALIAN} \
    "Disinstalla versioni esistenti"
LangString str_desc_old_ver         ${LANG_ITALIAN} \
    "Disinstalla versioni esistenti di MNV."

LangString str_section_exe          ${LANG_ITALIAN} \
    "GUI e file supporto MNV"
LangString str_desc_exe             ${LANG_ITALIAN} \
    "GUI programmi e file di supporto MNV.  Questa componente è indispensabile."

LangString str_section_console      ${LANG_ITALIAN} \
    "Console MNV (mnv.exe per MS-DOS)"
LangString str_desc_console         ${LANG_ITALIAN} \
    "Versione console di MNV (mnv.exe)."

LangString str_section_batch        ${LANG_ITALIAN} \
    "Crea file .bat"
LangString str_desc_batch           ${LANG_ITALIAN} \
    "Crea file .bat per varianti di MNV nella cartella \
     di Windows, per utilizzo da riga di comando."

LangString str_group_icons          ${LANG_ITALIAN} \
    "Crea icone MNV"
LangString str_desc_icons           ${LANG_ITALIAN} \
    "Crea icone MNV per rendere facile l'accesso."

LangString str_section_desktop      ${LANG_ITALIAN} \
    "Icone sul Desktop"
LangString str_desc_desktop         ${LANG_ITALIAN} \
    "Crea icone programma gMNV sul desktop."

LangString str_section_start_menu   ${LANG_ITALIAN} \
    "Gruppo programmi menù START"
LangString str_desc_start_menu      ${LANG_ITALIAN} \
    "Aggiunge gruppo programmi al menù START."

#LangString str_section_quick_launch ${LANG_ITALIAN} \
#    "Barra avvio veloce"
#LangString str_desc_quick_launch    ${LANG_ITALIAN} \
#    "Aggiunge un collegamento a MNV nella barra di avvio veloce."

LangString str_section_edit_with    ${LANG_ITALIAN} \
    "Aggiungi MNV al menù contestuale"
LangString str_desc_edit_with       ${LANG_ITALIAN} \
    "Aggiunge MNV al menu contestuale $\"Apri con...$\"."

#LangString str_section_edit_with32  ${LANG_ITALIAN} \
#    "Versione a 32 bit"
#LangString str_desc_edit_with32     ${LANG_ITALIAN} \
#    "Aggiungi MNV al menu contestuale $\"Apri con...$\" \
#     per applicazioni a 32 bit."

#LangString str_section_edit_with64  ${LANG_ITALIAN} \
#    "Versione a 64 bit"
#LangString str_desc_edit_with64     ${LANG_ITALIAN} \
#    "Aggiunge MNV al menu contestuale $\"Apri con...$\" \
#     per applicazioni a 64 bit."

LangString str_section_mnv_rc       ${LANG_ITALIAN} \
    "Crea configurazione predefinita"
LangString str_desc_mnv_rc          ${LANG_ITALIAN} \
    "Crea, se non ne esiste già uno, un file configurazione predefinito (_mnvrc) ."

LangString str_group_plugin         ${LANG_ITALIAN} \
    "Crea cartella plugin"
LangString str_desc_plugin          ${LANG_ITALIAN} \
    "Crea cartella plugin.  I plugin consentono di aggiungere funzionalità \
     a MNV copiando i relativi file in una di queste cartelle."

LangString str_section_plugin_home  ${LANG_ITALIAN} \
    "Privata"
LangString str_desc_plugin_home     ${LANG_ITALIAN} \
    "Crea cartella plugin nella cartella HOME."

LangString str_section_plugin_mnv   ${LANG_ITALIAN} \
    "Condivisa"
LangString str_desc_plugin_mnv      ${LANG_ITALIAN} \
    "Crea cartella plugin nella cartella di installazione di MNV \
     per uso da parte di tutti gli utenti di questo sistema."

LangString str_section_nls          ${LANG_ITALIAN} \
    "Supporto nativo lingua (NLS)"
LangString str_desc_nls             ${LANG_ITALIAN} \
    "Installa i file per il supporto nativo multilingua."

LangString str_unsection_register   ${LANG_ITALIAN} \
    "Rimuovi MNV dal registro"
LangString str_desc_unregister      ${LANG_ITALIAN} \
    "Rimuove MNV dal registro di configurazione sistema."

LangString str_unsection_exe        ${LANG_ITALIAN} \
    "Elimina programmi/file di supporto MNV"
LangString str_desc_rm_exe          ${LANG_ITALIAN} \
    "Elimina tutti i programmi/file di supporto di MNV."

LangString str_ungroup_plugin       ${LANG_ITALIAN} \
    "Elimina cartelle plugin"
LangString str_desc_rm_plugin       ${LANG_ITALIAN} \
    "Elimina le cartelle plugin se sono vuote."

LangString str_unsection_plugin_home ${LANG_ITALIAN} \
    "Private"
LangString str_desc_rm_plugin_home  ${LANG_ITALIAN} \
    "Elimina cartelle plugin nella cartella HOME."

LangString str_unsection_plugin_mnv ${LANG_ITALIAN} \
    "Condivise"
LangString str_desc_rm_plugin_mnv   ${LANG_ITALIAN} \
    "Elimina cartelle plugin nella cartella di installazione di MNV."

LangString str_unsection_rootdir    ${LANG_ITALIAN} \
    "Elimina la cartella di installazione di MNV"
LangString str_desc_rm_rootdir      ${LANG_ITALIAN} \
    "Elimina la cartella di installazione di MNV. Contiene i file di configurazione!"


##############################################################################
# Messages                                                                {{{1
##############################################################################

#LangString str_msg_too_many_ver  ${LANG_ITALIAN} \
#    "Rilevate nel sistema $mnv_old_ver_count versioni di MNV.$\r$\n\
#     Questo programma di installazione può gestire solo \
#     ${MNV_MAX_OLD_VER} versioni.$\r$\n\
#     Disinstalla qualche versione precedente e ricomincia."

#LangString str_msg_invalid_root  ${LANG_ITALIAN} \
#    "Nome cartella di installazione non valida: $mnv_install_root!$\r$\n\
#     Dovrebbe terminare con $\"mnv$\"."

#LangString str_msg_bin_mismatch  ${LANG_ITALIAN} \
#    "Conflitto nella cartella di installazione!$\r$\n$\r$\n\
#     La cartella di installazione dev'essere $\"$mnv_bin_path$\",$\r$\n\
#     ma il sistema indica che il percorso è $\"$INSTDIR$\"."

#LangString str_msg_mnv_running   ${LANG_ITALIAN} \
#    "MNV è ancora in esecuzione nel sistema.$\r$\n\
#     Per continuare chiudi tutte le sessioni attive di MNV."

#LangString str_msg_register_ole  ${LANG_ITALIAN} \
#    "Tentativo di registrazione di MNV con OLE. \
#     Non ci sono messaggi che indicano se l'operazione è riuscita."

#LangString str_msg_unreg_ole     ${LANG_ITALIAN} \
#    "Tentativo di rimozione di MNV dal registro via OLE. \
#     Non ci sono messaggi che indicano se l'operazione è riuscita."

#LangString str_msg_rm_start      ${LANG_ITALIAN} \
#    "Disinstallazione della versione:"

#LangString str_msg_rm_fail       ${LANG_ITALIAN} \
#    "Disinstallazione non riuscita per la versione:"

#LangString str_msg_no_rm_key     ${LANG_ITALIAN} \
#    "Impossibile trovare chiave disinstallazione nel registro."

#LangString str_msg_no_rm_reg     ${LANG_ITALIAN} \
#    "Impossibile trovare programma disinstallazione nel registro."

#LangString str_msg_no_rm_exe     ${LANG_ITALIAN} \
#    "Impossibile trovare programma disinstallazione."

#LangString str_msg_rm_copy_fail  ${LANG_ITALIAN} \
#    "Impossibile copiare il programma disinstallazione in una cartella temporanea."

#LangString str_msg_rm_run_fail   ${LANG_ITALIAN} \
#    "Impossibile eseguire programma disinstallazione."

#LangString str_msg_abort_install ${LANG_ITALIAN} \
#    "Il programma di disinstallazione verrà chiuso senza aver eseguito nessuna modifica."

LangString str_msg_install_fail  ${LANG_ITALIAN} \
    "Installazione non riuscita."

LangString str_msg_rm_exe_fail   ${LANG_ITALIAN} \
    "Alcuni file in $0 non sono stati eliminati!$\r$\n\
     I file vanno rimossi manualmente."

#LangString str_msg_rm_root_fail  ${LANG_ITALIAN} \
#    "AVVISO: impossibile eliminare $\"$mnv_install_root$\", non è vuota!"

LangString str_msg_uninstalling  ${LANG_ITALIAN} \
    "Disinstallazione vecchia versione MNV..."

LangString str_msg_registering   ${LANG_ITALIAN} \
    "Aggiunta di MNV al registro..."

LangString str_msg_unregistering ${LANG_ITALIAN} \
    "Rimozione di MNV dal registro..."


##############################################################################
# Dialog Box                                                              {{{1
##############################################################################

LangString str_mnvrc_page_title    ${LANG_ITALIAN} \
    "Scelta impostazioni _mnvrc"
LangString str_mnvrc_page_subtitle ${LANG_ITALIAN} \
    "Scelta impostazioni funzionalità aggiuntive, tastiera e mouse."

LangString str_msg_compat_title    ${LANG_ITALIAN} \
    " Comportamento come Vi / MNV "
LangString str_msg_compat_desc     ${LANG_ITALIAN} \
    "&Compatibilità e funzionalità"
LangString str_msg_compat_vi       ${LANG_ITALIAN} \
    "Compatibile Vi"
LangString str_msg_compat_mnv      ${LANG_ITALIAN} \
    "MNV originale"
LangString str_msg_compat_defaults ${LANG_ITALIAN} \
    "MNV con alcune funzionalità aggiuntive (defaults.mnv)"
LangString str_msg_compat_all      ${LANG_ITALIAN} \
    "MNV con tutte le funzionalità aggiuntive (mnvrc_example.mnv) (predefinito)"

LangString str_msg_keymap_title   ${LANG_ITALIAN} \
    " Mappature tastiera "
LangString str_msg_keymap_desc    ${LANG_ITALIAN} \
    "&Rimappa alcuni tasti Windows (Ctrl-V, Ctrl-C, Ctrl-A, Ctrl-S, Ctrl-F, etc.)"
LangString str_msg_keymap_default ${LANG_ITALIAN} \
    "Non rimappare i tasti (predefinito)"
LangString str_msg_keymap_windows ${LANG_ITALIAN} \
    "Rimappa solo alcuni tasti"

LangString str_msg_mouse_title   ${LANG_ITALIAN} \
    " Mouse "
LangString str_msg_mouse_desc    ${LANG_ITALIAN} \
    "&Comportamento pulsanti destro/sinistro"
LangString str_msg_mouse_default ${LANG_ITALIAN} \
    "Destro: menu popup, Sinistro: modalità visuale (predefinito)"
LangString str_msg_mouse_windows ${LANG_ITALIAN} \
    "Destro: menu popup, Sinistro: selezione modalità (Windows)"
LangString str_msg_mouse_unix    ${LANG_ITALIAN} \
    "Destro: estensione selezione, Sinistro: modalità visuale (Unix)"
