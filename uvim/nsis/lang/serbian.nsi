# vi:set ts=8 sts=4 sw=4 et fdm=marker:
#
# serbian.nsi: Serbian language strings for gmnv NSIS installer.
#
# Locale ID    : 3098
# Locale Name  : sr
# fileencoding : UTF-8
# Author       : Ivan Pešić

!insertmacro MUI_LANGUAGE "Serbian"


# Overwrite the default translation.
# These strings should be always English.  Otherwise dosinst.c fails.
LangString ^SetupCaption     ${LANG_SERBIAN} \
        "$(^Name) Setup"
LangString ^UninstallCaption ${LANG_SERBIAN} \
        "$(^Name) Uninstall"

##############################################################################
# Translated license file for the license page                            {{{1
##############################################################################

LicenseLangString page_lic_file 0 "..\lang\LICENSE.nsis.txt"
#LicenseLangString page_lic_file ${LANG_SERBIAN} "..\lang\LICENSE.sr.nsis.txt"

##############################################################################
# Translated README.txt file, which is opened after installation          {{{1
##############################################################################

LangString mnv_readme_file 0 "README.txt"
#LangString mnv_readme_file ${LANG_SERBIAN} "README.sr.txt"

##############################################################################
# MUI Configuration Strings                                               {{{1
##############################################################################

#LangString str_dest_folder          ${LANG_SERBIAN} \
#    "Destination Folder (Must end with $\"mnv$\")"

LangString str_show_readme          ${LANG_SERBIAN} \
    "Прикажи ПРОЧИТАЈМЕ када се заврши инсталација"

# Install types:
LangString str_type_typical         ${LANG_SERBIAN} \
    "Типична"

LangString str_type_minimal         ${LANG_SERBIAN} \
    "Минимална"

LangString str_type_full            ${LANG_SERBIAN} \
    "Пуна"


##############################################################################
# Section Titles & Description                                            {{{1
##############################################################################

LangString str_section_old_ver      ${LANG_SERBIAN} \
    "Уклањање постојећ(е/их) верзиј(е/а)"
LangString str_desc_old_ver         ${LANG_SERBIAN} \
    "Уклања постојећ(у/е) MNV верзииј(у/е) из вашег система."

LangString str_section_exe          ${LANG_SERBIAN} \
    "MNV ГКИ и фајлови потребни за извршавање"
LangString str_desc_exe             ${LANG_SERBIAN} \
    "MNV ГКИ извршни фајлови и фајлови потребни током извршавања. Ова компонента је неопходна."

LangString str_section_console      ${LANG_SERBIAN} \
    "MNV конзолни програм"
LangString str_desc_console         ${LANG_SERBIAN} \
    "Конзолна верзија програма MNV (mnv.exe)."

LangString str_section_batch        ${LANG_SERBIAN} \
    "Креирај .bat фајлове"
LangString str_desc_batch           ${LANG_SERBIAN} \
    "Креира у Windows директоријуму .bat фајлове за MNV варијанте \
     у циљу коришћења из командне линије."

LangString str_group_icons          ${LANG_SERBIAN} \
    "Креирај иконе за MNV"
LangString str_desc_icons           ${LANG_SERBIAN} \
    "Креира иконе за MNV на различитим местима, како би се олакшао приступ."

LangString str_section_desktop      ${LANG_SERBIAN} \
    "На радној површини"
LangString str_desc_desktop         ${LANG_SERBIAN} \
    "Креира иконе за gMNV извршне фајлове на радној површини."

LangString str_section_start_menu   ${LANG_SERBIAN} \
    "У фасцикли Програми унутар Старт менија"
LangString str_desc_start_menu      ${LANG_SERBIAN} \
    "Додаје MNV у фолдер Програми Старт менија."

#LangString str_section_quick_launch ${LANG_SERBIAN} \
#    "In the Quick Launch Bar"
#LangString str_desc_quick_launch    ${LANG_SERBIAN} \
#    "Add MNV shortcut in the quick launch bar."

LangString str_section_edit_with    ${LANG_SERBIAN} \
    "Додај MNV контекстни мени"
LangString str_desc_edit_with       ${LANG_SERBIAN} \
    "Додаје MNV у $\"Отвори помоћу...$\" листу контекстног менија."

#LangString str_section_edit_with32  ${LANG_SERBIAN} \
#    "32-bit Version"
#LangString str_desc_edit_with32     ${LANG_SERBIAN} \
#    "Add MNV to the $\"Open With...$\" context menu list \
#     for 32-bit applications."

#LangString str_section_edit_with64  ${LANG_SERBIAN} \
#    "64-bit Version"
#LangString str_desc_edit_with64     ${LANG_SERBIAN} \
#    "Add MNV to the $\"Open With...$\" context menu list \
#     for 64-bit applications."

LangString str_section_mnv_rc       ${LANG_SERBIAN} \
    "Креирај Подразумевану конфигурацију"
LangString str_desc_mnv_rc          ${LANG_SERBIAN} \
    "Креира подразумевани конфиг фајл (_mnvrc) ако неки већ не постоји."

LangString str_group_plugin         ${LANG_SERBIAN} \
    "Креира директоријуме додатака"
LangString str_desc_plugin          ${LANG_SERBIAN} \
    "Креира директоријуме додатака. Ови директоријуми омогућавају проширење програма MNV \
     убацивањем фајла у директоријум."

LangString str_section_plugin_home  ${LANG_SERBIAN} \
    "Приватне"
LangString str_desc_plugin_home     ${LANG_SERBIAN} \
    "Креира директоријуме додатака у HOME директоријуму."

LangString str_section_plugin_mnv   ${LANG_SERBIAN} \
    "Дељене"
LangString str_desc_plugin_mnv      ${LANG_SERBIAN} \
    "Креира директоријуме додатака у MNV инсталационом директоријуму, користе их сви \
     на систему."

LangString str_section_nls          ${LANG_SERBIAN} \
    "Подршка за домаћи језик"
LangString str_desc_nls             ${LANG_SERBIAN} \
    "Инсталира фајлове за домаћу језичку подршку."

LangString str_unsection_register   ${LANG_SERBIAN} \
    "Поништи регистрацију MNV"
LangString str_desc_unregister      ${LANG_SERBIAN} \
    "Поништава регистрацију програма MNV на систему."

LangString str_unsection_exe        ${LANG_SERBIAN} \
    "Уклони MNV извршне фајлове/фајлове потребне у време извршавања"
LangString str_desc_rm_exe          ${LANG_SERBIAN} \
    "Уклања све MNV извршне фајлове и оне потребне у време извршавања."

LangString str_ungroup_plugin       ${LANG_SERBIAN} \
    "Укони директоријуме додатака"
LangString str_desc_rm_plugin       ${LANG_SERBIAN} \
    "Уклања директоријуме додатака ако су празни."

LangString str_unsection_plugin_home ${LANG_SERBIAN} \
    "Приватне"
LangString str_desc_rm_plugin_home  ${LANG_SERBIAN} \
    "Уклања директоријуме додатака из HOME директоријума."

LangString str_unsection_plugin_mnv ${LANG_SERBIAN} \
    "Дељене"
LangString str_desc_rm_plugin_mnv   ${LANG_SERBIAN} \
    "Уклања директоријуме додатака из MNV инсталациониг директоријума."

LangString str_unsection_rootdir    ${LANG_SERBIAN} \
    "Уклони MNV корени директоријум"
LangString str_desc_rm_rootdir      ${LANG_SERBIAN} \
    "Уклања MNV корени директоријум. Он садржи ваше MNV конфигурационе фајлове!"


##############################################################################
# Messages                                                                {{{1
##############################################################################

#LangString str_msg_too_many_ver  ${LANG_SERBIAN} \
#    "Found $mnv_old_ver_count MNV versions on your system.$\r$\n\
#     This installer can only handle ${MNV_MAX_OLD_VER} versions \
#     at most.$\r$\n\
#     Please remove some versions and start again."

#LangString str_msg_invalid_root  ${LANG_SERBIAN} \
#    "Invalid install path: $mnv_install_root!$\r$\n\
#     It should end with $\"mnv$\"."

#LangString str_msg_bin_mismatch  ${LANG_SERBIAN} \
#    "Binary path mismatch!$\r$\n$\r$\n\
#     Expect the binary path to be $\"$mnv_bin_path$\",$\r$\n\
#     but system indicates the binary path is $\"$INSTDIR$\"."

#LangString str_msg_mnv_running   ${LANG_SERBIAN} \
#    "MNV is still running on your system.$\r$\n\
#     Please close all instances of MNV before you continue."

#LangString str_msg_register_ole  ${LANG_SERBIAN} \
#    "Attempting to register MNV with OLE. \
#     There is no message indicates whether this works or not."

#LangString str_msg_unreg_ole     ${LANG_SERBIAN} \
#    "Attempting to unregister MNV with OLE. \
#     There is no message indicates whether this works or not."

#LangString str_msg_rm_start      ${LANG_SERBIAN} \
#    "Uninstalling the following version:"

#LangString str_msg_rm_fail       ${LANG_SERBIAN} \
#    "Fail to uninstall the following version:"

#LangString str_msg_no_rm_key     ${LANG_SERBIAN} \
#    "Cannot find uninstaller registry key."

#LangString str_msg_no_rm_reg     ${LANG_SERBIAN} \
#    "Cannot find uninstaller from registry."

#LangString str_msg_no_rm_exe     ${LANG_SERBIAN} \
#    "Cannot access uninstaller."

#LangString str_msg_rm_copy_fail  ${LANG_SERBIAN} \
#    "Fail to copy uninstaller to temporary directory."

#LangString str_msg_rm_run_fail   ${LANG_SERBIAN} \
#    "Fail to run uninstaller."

#LangString str_msg_abort_install ${LANG_SERBIAN} \
#    "Installer will abort."

LangString str_msg_install_fail  ${LANG_SERBIAN} \
    "Инсталација није успела. Више среће идући пут."

LangString str_msg_rm_exe_fail   ${LANG_SERBIAN} \
    "Неки фајлови у $0 нису обрисани!$\r$\n\
     Морате то ручно да обавите."

#LangString str_msg_rm_root_fail  ${LANG_SERBIAN} \
#    "WARNING: Cannot remove $\"$mnv_install_root$\", it is not empty!"

LangString str_msg_uninstalling  ${LANG_SERBIAN} \
    "Уклањање старе верзије..."

LangString str_msg_registering   ${LANG_SERBIAN} \
    "Регистровање..."

LangString str_msg_unregistering ${LANG_SERBIAN} \
    "Поништавање регистрације..."


##############################################################################
# Dialog Box                                                              {{{1
##############################################################################

LangString str_mnvrc_page_title    ${LANG_SERBIAN} \
    "Изаберите _mnvrc подешавања"
LangString str_mnvrc_page_subtitle ${LANG_SERBIAN} \
    "Изаберите подешавања за побољшања, тастатуру и миша."

LangString str_msg_compat_title    ${LANG_SERBIAN} \
    " Vi / MNV понашање "
LangString str_msg_compat_desc     ${LANG_SERBIAN} \
    "&Компатибилност и побољшања"
LangString str_msg_compat_vi       ${LANG_SERBIAN} \
    "Vi компатибилно"
LangString str_msg_compat_mnv      ${LANG_SERBIAN} \
    "MNV оригинално"
LangString str_msg_compat_defaults ${LANG_SERBIAN} \
    "MNV са неким побољшањима (учитава defaults.mnv)"
LangString str_msg_compat_all      ${LANG_SERBIAN} \
    "MNV са свим побољшањима (учитава mnvrc_example.mnv) (Подразумевано)"

LangString str_msg_keymap_title   ${LANG_SERBIAN} \
    " Мапирања "
LangString str_msg_keymap_desc    ${LANG_SERBIAN} \
    "&Ремапира неколико тастера за Windows (Ctrl-V, Ctrl-C, Ctrl-A, Ctrl-S, Ctrl-F, итд.)"
LangString str_msg_keymap_default ${LANG_SERBIAN} \
    "Немој да ремапираш тастере (Подразумевано)"
LangString str_msg_keymap_windows ${LANG_SERBIAN} \
    "Ремапира неколико тастера"

LangString str_msg_mouse_title   ${LANG_SERBIAN} \
    " Миш "
LangString str_msg_mouse_desc    ${LANG_SERBIAN} \
    "&Понашање левог и десног тастера"
LangString str_msg_mouse_default ${LANG_SERBIAN} \
    "Десни: искачући мени, Леви: визуелни режим (Подразумевано)"
LangString str_msg_mouse_windows ${LANG_SERBIAN} \
    "Десни: искачући мени, Леви: режим избора (Windows)"
LangString str_msg_mouse_unix    ${LANG_SERBIAN} \
    "Десни: проширује избор, Леви: визуелни режим (Unix)"
