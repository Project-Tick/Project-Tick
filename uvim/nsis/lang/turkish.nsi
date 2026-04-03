# vi:set ts=8 sts=4 sw=4 et fdm=marker:
#
# turkish.nsi: Turkish language strings for gmnv NSIS installer.
#
# Locale ID    : 1055
# Locale Name  : tr
# fileencoding : UTF-8
# Author       : Emir SARI

!insertmacro MUI_LANGUAGE "Turkish"

# Overwrite the default translation.
# These strings should be always English.  Otherwise dosinst.c fails.
LangString ^SetupCaption     ${LANG_TURKISH} \
        "$(^Name) Setup"
LangString ^UninstallCaption ${LANG_TURKISH} \
        "$(^Name) Uninstall"

##############################################################################
# Translated license file for the license page                            {{{1
##############################################################################

LicenseLangString page_lic_file 0 "..\lang\LICENSE.nsis.txt"
#LicenseLangString page_lic_file ${LANG_TURKISH} "..\lang\LICENSE.tr.nsis.txt"

##############################################################################
# Translated README.txt file, which is opened after installation          {{{1
##############################################################################

LangString mnv_readme_file 0 "README.txt"
#LangString mnv_readme_file ${LANG_TURKISH} "README.tr.txt"

##############################################################################

LangString str_show_readme          ${LANG_TURKISH} \
    "Kurulum bittikten sonra README dosyasını aç"

# Install types:
LangString str_type_typical         ${LANG_TURKISH} \
    "Normal"

LangString str_type_minimal         ${LANG_TURKISH} \
    "Küçük"

LangString str_type_full            ${LANG_TURKISH} \
    "Tam"

##############################################################################
# Section Titles & Description                                            {{{1
##############################################################################

LangString str_section_old_ver      ${LANG_TURKISH} \
    "Eski sürümü kaldır"
LangString str_desc_old_ver         ${LANG_TURKISH} \
    "MNV'in eski sürümünü sisteminizden kaldırır."

LangString str_section_exe          ${LANG_TURKISH} \
    "MNV grafik arabirimi ve çalışma dosyaları"
LangString str_desc_exe             ${LANG_TURKISH} \
    "MNV program başlatıcı ve çalışma dosyaları. Bu bileşen zorunludur."

LangString str_section_console      ${LANG_TURKISH} \
    "MNV konsol sürümü"
LangString str_desc_console         ${LANG_TURKISH} \
    "MNV'in konsol sürümü (mnv.exe)."

LangString str_section_batch        ${LANG_TURKISH} \
    ".bat dosyaları oluştur"
LangString str_desc_batch           ${LANG_TURKISH} \
    "MNV için komut satırında kullanmak üzere .bat dosyaları oluşturur"

LangString str_group_icons          ${LANG_TURKISH} \
    "MNV için kısayollar oluştur"
LangString str_desc_icons           ${LANG_TURKISH} \
    "Kolay erişim için MNV kısayolları oluşturur."

LangString str_section_desktop      ${LANG_TURKISH} \
    "Masaüstünde"
LangString str_desc_desktop         ${LANG_TURKISH} \
    "gMNV programı için Masaüstünde kısayol oluşturur."

LangString str_section_start_menu   ${LANG_TURKISH} \
    "Başlat Menüsü - Programlar klasöründe"
LangString str_desc_start_menu      ${LANG_TURKISH} \
    "MNV kısayolunu Başlat Menüsüne ekler."

LangString str_section_edit_with    ${LANG_TURKISH} \
    "MNV ile Aç"
LangString str_desc_edit_with       ${LANG_TURKISH} \
    "MNV'i $\"Birlikte aç$\" sağ tık menüsüne ekler."

 LangString str_section_mnv_rc       ${LANG_TURKISH} \
    "Bir yapılandırma dosyası oluştur"
LangString str_desc_mnv_rc          ${LANG_TURKISH} \
    "Eğer yoksa bir yapılandırma dosyası (_mnvrc) oluşturur."

LangString str_group_plugin         ${LANG_TURKISH} \
    "Eklenti dizinleri oluştur"
LangString str_desc_plugin          ${LANG_TURKISH} \
    "Bu dizinlere MNV eklentilerini yerleştirerek MNV'e yeni \
     özellikler kazandırabilirsiniz."

LangString str_section_plugin_home  ${LANG_TURKISH} \
    "Gizli"
LangString str_desc_plugin_home     ${LANG_TURKISH} \
    "Eklenti dizinlerini EV dizininde oluşturur."

LangString str_section_plugin_mnv   ${LANG_TURKISH} \
    "Paylaşılan"
LangString str_desc_plugin_mnv      ${LANG_TURKISH} \
    "Eklenti dizinlerini MNV yükleme dizininde oluşturur. Bu eklentilerden \
     bilgisayarın tüm kullanıcıları yararlanabilir."

LangString str_section_nls          ${LANG_TURKISH} \
    "Ek dil desteği"
LangString str_desc_nls             ${LANG_TURKISH} \
    "Mevcut olan MNV yerelleştirmelerini yükler."

LangString str_unsection_register   ${LANG_TURKISH} \
    "MNV kaydını kaldır"
LangString str_desc_unregister      ${LANG_TURKISH} \
    "MNV'in bu bilgisayardaki kaydını kaldırır."

LangString str_unsection_exe        ${LANG_TURKISH} \
    "MNV programını ve çalıştırma dosyalarını kaldır"
LangString str_desc_rm_exe          ${LANG_TURKISH} \
    "MNV çalıştırılabilir dosyalarını ve diğer dosyaları kaldırır."

LangString str_ungroup_plugin       ${LANG_TURKISH} \
    "Eklenti dizinlerini kaldır"
LangString str_desc_rm_plugin       ${LANG_TURKISH} \
    "Eklenti dizinlerini eğer boş ise kaldırır."

LangString str_unsection_plugin_home ${LANG_TURKISH} \
    "Gizli"
LangString str_desc_rm_plugin_home  ${LANG_TURKISH} \
    "Eklenti dizinlerini EV dizininden kaldırır."

LangString str_unsection_plugin_mnv ${LANG_TURKISH} \
    "Paylaşılan"
LangString str_desc_rm_plugin_mnv   ${LANG_TURKISH} \
    "Eklenti dizinlerini MNV yükleme dizininden kaldırır."

LangString str_unsection_rootdir    ${LANG_TURKISH} \
    "MNV kök dizinini kaldır"
LangString str_desc_rm_rootdir      ${LANG_TURKISH} \
    "MNV kök dizinini kaldırır. Bu dizin MNV yapılandırma dosyalarını içerir!"

 LangString str_msg_install_fail  ${LANG_TURKISH} \
    "Yükleme başarısız oldu. Yeniden deneyin."

LangString str_msg_rm_exe_fail   ${LANG_TURKISH} \
    "$0 içindeki bazı dosyalar silinemedi!$\r$\n\
     Bu dosyaları el ile kaldırmalısınız."

 LangString str_msg_uninstalling  ${LANG_TURKISH} \
    "Eski sürüm kaldırılıyor..."

LangString str_msg_registering   ${LANG_TURKISH} \
    "Kaydediliyor..."

LangString str_msg_unregistering ${LANG_TURKISH} \
    "Kayıt siliniyor..."


##############################################################################
# Dialog Box                                                              {{{1
##############################################################################

LangString str_mnvrc_page_title    ${LANG_TURKISH} \
    "_mnvrc ayarlarını seçin"
LangString str_mnvrc_page_subtitle ${LANG_TURKISH} \
    "Yüklenecek ek özellikler, klavye ve fare için ayarları seçin."

LangString str_msg_compat_title    ${LANG_TURKISH} \
    " Vi / MNV davranışı "
LangString str_msg_compat_desc     ${LANG_TURKISH} \
    "&Uyumluluk ve ek özellikler"
LangString str_msg_compat_vi       ${LANG_TURKISH} \
    "Vi uyumlu"
LangString str_msg_compat_mnv      ${LANG_TURKISH} \
    "MNV orijinal"
LangString str_msg_compat_defaults ${LANG_TURKISH} \
    "MNV ve ek olarak bazı ek özellikler (load defaults.mnv)"
LangString str_msg_compat_all      ${LANG_TURKISH} \
    "MNV ve ek olarak tüm ek özellikler (load mnvrc_example.mnv) (Default)"

LangString str_msg_keymap_title   ${LANG_TURKISH} \
    " Klavye İşlevleri "
LangString str_msg_keymap_desc    ${LANG_TURKISH} \
    "&Windows için bazı düğmeleri yeniden ayarla (Ctrl-V, Ctrl-C, Ctrl-A, Ctrl-S, Ctrl-F, etc)"
LangString str_msg_keymap_default ${LANG_TURKISH} \
    "Düğme işlevlerini değiştirme (varsayılan)"
LangString str_msg_keymap_windows ${LANG_TURKISH} \
    "Bazı düğmeleri yeniden ayarla"

LangString str_msg_mouse_title   ${LANG_TURKISH} \
    " Fare İşlevleri "
LangString str_msg_mouse_desc    ${LANG_TURKISH} \
    "&Sağ ve sol düğme davranışı"
LangString str_msg_mouse_default ${LANG_TURKISH} \
    "Sağ: açılır menü, Sol: Görsel Kip (varsayılan)"
LangString str_msg_mouse_windows ${LANG_TURKISH} \
    "Sağ: açılır menü, Sol: seçim kipi (Windows)"
LangString str_msg_mouse_unix    ${LANG_TURKISH} \
    "Sağ: seçimi genişlet, Sol: Görsel Kip (Unix)"
