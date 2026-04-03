# vi:set ts=8 sts=4 sw=4 et fdm=marker:
#
# russian.nsi: Russian language strings for gmnv NSIS installer.
#
# Locale ID    : 1049
# Locale name  : ru
# fileencoding : UTF-8
# Author       : Restorer

!insertmacro MUI_LANGUAGE "Russian"


# Overwrite the default translation.
# These strings should be always English.  Otherwise dosinst.c fails.
LangString ^SetupCaption     ${LANG_RUSSIAN} \
        "$(^Name) Setup"
LangString ^UninstallCaption ${LANG_RUSSIAN} \
        "$(^Name) Uninstall"

##############################################################################
# Translated license file for the license page                            {{{1
##############################################################################

LicenseLangString page_lic_file ${LANG_RUSSIAN} "..\lang\LICENSE.ru.nsis.txt"

##############################################################################
# Translated README.txt file, which is opened after installation          {{{1
##############################################################################

LangString mnv_readme_file ${LANG_RUSSIAN} "README.ru.txt"

##############################################################################
# MUI Configuration Strings                                               {{{1
##############################################################################

#LangString str_dest_folder          ${LANG_RUSSIAN} \
#    "Маршрут установки программы (должен завершаться каталогом $\"mnv$\")"

LangString str_show_readme          ${LANG_RUSSIAN} \
    " Ознакомиться с кратким описанием программы"

# Install types:
LangString str_type_typical         ${LANG_RUSSIAN} \
    "Стандартный"

LangString str_type_minimal         ${LANG_RUSSIAN} \
    "Минимальный"

LangString str_type_full            ${LANG_RUSSIAN} \
    "Полный"


##############################################################################
# Section Titles & Description                                            {{{1
##############################################################################

LangString str_section_old_ver      ${LANG_RUSSIAN} \
    "Удаление предыдущих версий"
LangString str_desc_old_ver         ${LANG_RUSSIAN} \
    "Будут удалены предыдущие установленные версии программы"

LangString str_section_exe          ${LANG_RUSSIAN} \
    "Графический интерфейс и вспомогательные файлы"
LangString str_desc_exe             ${LANG_RUSSIAN} \
    "Исполняемые файлы и все необходимые для работы программы файлы. \
    Это обязательный компонент"

LangString str_section_console      ${LANG_RUSSIAN} \
    "Консольная программа MNV"
LangString str_desc_console         ${LANG_RUSSIAN} \
    "Вариант редактора MNV (mnv.exe), используемый для работы в командной \
    оболочке"

LangString str_section_batch        ${LANG_RUSSIAN} \
    "Создать командные файлы"
LangString str_desc_batch           ${LANG_RUSSIAN} \
    "Создание командных bat-файлов в каталоге Windows для работы с редактором \
    MNV из командной строки"

LangString str_group_icons          ${LANG_RUSSIAN} \
    "Создать ярлыки для редактора MNV"
LangString str_desc_icons           ${LANG_RUSSIAN} \
    "Создание ярлыков программы для удобного и быстрого запуска редактора MNV"

LangString str_section_desktop      ${LANG_RUSSIAN} \
    "На Рабочем столе"
LangString str_desc_desktop         ${LANG_RUSSIAN} \
    "Создание ярлыков редактора MNV на Рабочем столе"

LangString str_section_start_menu   ${LANG_RUSSIAN} \
    "В меню кнопки Пуск"
LangString str_desc_start_menu      ${LANG_RUSSIAN} \
    "Создание ярлыков редактора MNV в меню кнопки Пуск"

#LangString str_section_quick_launch ${LANG_RUSSIAN} \
#    "На панели быстрого запуска"
#LangString str_desc_quick_launch    ${LANG_RUSSIAN} \
#    "Создание ярлыков редактора MNV на панели быстрого запуска"

LangString str_section_edit_with    ${LANG_RUSSIAN} \
    "Запуск редактора MNV из контекстного меню"
LangString str_desc_edit_with       ${LANG_RUSSIAN} \
    "Добавление необходимой строки в пункт контекстного меню \
    «Открыть с помощью...»"

#LangString str_section_edit_with32  ${LANG_RUSSIAN} \
#    "Для 32-разрядной версии программы"
#LangString str_desc_edit_with32     ${LANG_RUSSIAN} \
#    "Добавление в пункт контекстного меню \
#    «Открыть с помощью...» 32-разрядных приложений"

#LangString str_section_edit_with64  ${LANG_RUSSIAN} \
#    "Для 64-разрядной версии программы"
#LangString str_desc_edit_with64     ${LANG_RUSSIAN} \
#    "Добавление в пункт контекстного меню \
#    «Открыть с помощью...» 64-разрядных приложений"

LangString str_section_mnv_rc       ${LANG_RUSSIAN} \
    "Начальная настройка программы"
LangString str_desc_mnv_rc          ${LANG_RUSSIAN} \
    "Создание файла _mnvrc с предустановленными настройками, если нет других \
    файлов настроек"

LangString str_group_plugin         ${LANG_RUSSIAN} \
    "Создать каталог для подключаемых модулей"
LangString str_desc_plugin          ${LANG_RUSSIAN} \
    "Создание каталога для подключаемых модулей, которые расширяют возможности \
    редактора MNV"

LangString str_section_plugin_home  ${LANG_RUSSIAN} \
    "Личный каталог"
LangString str_desc_plugin_home     ${LANG_RUSSIAN} \
    "В домашнем каталоге пользователя. Модули в этом каталоге доступны только \
    этому пользователю"

LangString str_section_plugin_mnv   ${LANG_RUSSIAN} \
    "Общий каталог"
LangString str_desc_plugin_mnv      ${LANG_RUSSIAN} \
    "В каталоге установки редактора MNV. Модули в этом каталоге доступны для \
    всех пользователей"

#LangString str_section_vis_mnv      ${LANG_RUSSIAN} \
#    "Подключаемый модуль VisMNV"
#LangString str_desc_vis_mnv         ${LANG_RUSSIAN} \
#    "Подключаемый модуль VisMNV используется для интеграции с \
#    Microsoft Visual Studio"

LangString str_section_nls          ${LANG_RUSSIAN} \
    "Поддержка региональных языков"
LangString str_desc_nls             ${LANG_RUSSIAN} \
    "Установка файлов для работы программе на различных региональных языках"

LangString str_unsection_register   ${LANG_RUSSIAN} \
    "Отменить регистрацию компонентов программы MNV"
LangString str_desc_unregister      ${LANG_RUSSIAN} \
    "Отмена регистрации компонентов программы MNV в операционной системе"

LangString str_unsection_exe        ${LANG_RUSSIAN} \
    "Удалить файлы редактора MNV"
LangString str_desc_rm_exe          ${LANG_RUSSIAN} \
    "Удаление всех исполняемых и вспомогательных файлов редактора MNV"

LangString str_ungroup_plugin       ${LANG_RUSSIAN} \
    "Удалить каталог подключаемых модулей"
LangString str_desc_rm_plugin       ${LANG_RUSSIAN} \
    "Удаление каталога подключаемых модулей, если в нём нет файлов"

LangString str_unsection_plugin_home ${LANG_RUSSIAN} \
    "Личный каталог"
LangString str_desc_rm_plugin_home  ${LANG_RUSSIAN} \
    "Удаление каталога подключаемых модулей из домашнего каталога пользователя"

LangString str_unsection_plugin_mnv ${LANG_RUSSIAN} \
    "Общий каталог"
LangString str_desc_rm_plugin_mnv   ${LANG_RUSSIAN} \
    "Удаление каталога подключаемых модулей из каталога установки редактора MNV"

LangString str_unsection_rootdir    ${LANG_RUSSIAN} \
    "Удалить основной каталог программы MNV"
LangString str_desc_rm_rootdir      ${LANG_RUSSIAN} \
    "Удаление основного каталога программы MNV. В этом каталоге находятся \
    файлы настроек!"


##############################################################################
# Messages                                                                {{{1
##############################################################################

#LangString str_msg_too_many_ver  ${LANG_RUSSIAN} \
#    "Обнаружено предыдущих версий программы MNV: $mnv_old_ver_count.$\r$\n\
#     Данная программа установки может удалить не более ${MNV_MAX_OLD_VER}.$\r$\n\
#     Удалить лишние версии программы MNV и повторите установку"

#LangString str_msg_invalid_root  ${LANG_RUSSIAN} \
#    "Недопустимый каталог установки программы MNV $mnv_install_root!$\r$\n\
#     Маршрут установки должен оканчиваться каталогом $\"mnv$\""

#LangString str_msg_bin_mismatch  ${LANG_RUSSIAN} \
#    "Недопустимый маршрут к каталогу с исполняемыми файлами!$\r$\n$\r$\n\
#     Маршрут к каталогу с исполняемыми файлами должен быть $\"$mnv_bin_path$\",$\r$\n\
#     но от операционной системы получен как $\"$INSTDIR$\"."

#LangString str_msg_mnv_running   ${LANG_RUSSIAN} \
#    "Программа MNV сейчас работает.$\r$\n\
#     Прежде чем продолжить, закройте все работающие редакторы MNV"

#LangString str_msg_register_ole  ${LANG_RUSSIAN} \
#    "Попытка зарегистрировать компоненты программы MNV в пространстве OLE. \
#     Но не получено уведомление об успешности данной операции"

#LangString str_msg_unreg_ole     ${LANG_RUSSIAN} \
#    "Попытка отменить регистрацию компонентов программы MNV в пространстве OLE. \
#     Но не получено уведомление об успешности данной операции"

#LangString str_msg_rm_start      ${LANG_RUSSIAN} \
#    "Выполняется удаление следующих версий программы:"

#LangString str_msg_rm_fail       ${LANG_RUSSIAN} \
#    "Произошёл сбой при выполнении удаления следующих версий программы:"

#LangString str_msg_no_rm_key     ${LANG_RUSSIAN} \
#    "Не удалось найти раздел реестра, содержащий информацию об удалении \
#    программы"

#LangString str_msg_no_rm_reg     ${LANG_RUSSIAN} \
#    "Не удалось найти указанную в реестре программу, которая выполняет удаление"

#LangString str_msg_no_rm_exe     ${LANG_RUSSIAN} \
#    "Отсутствуют права на доступ к программе, выполняющей удаление"

#LangString str_msg_rm_copy_fail  ${LANG_RUSSIAN} \
#    "Произошла ошибка при копировании программы удаления во временный каталог"

#LangString str_msg_rm_run_fail   ${LANG_RUSSIAN} \
#    "Произошёл сбой при запуске программы, выполняющей удаление"

#LangString str_msg_abort_install ${LANG_RUSSIAN} \
#    "Установка программы была отменена"

LangString str_msg_install_fail  ${LANG_RUSSIAN} \
    "Произошла ошибка при установке программы. Попробуйте повторить установку \
    немного попозже"
# когда Луна будет в другой фазе и ветер должен дуть с юго‐запада

LangString str_msg_rm_exe_fail   ${LANG_RUSSIAN} \
    "Некоторые файлы не были удалены из каталога $0 $\r$\n\
     Необходимо выполнить их удаление самостоятельно"

#LangString str_msg_rm_root_fail  ${LANG_RUSSIAN} \
#    "Внимание! В каталоге $\"$mnv_install_root$\" содержатся файлы. Удаление \
#    каталога не выполнено"

LangString str_msg_uninstalling  ${LANG_RUSSIAN} \
    "Удаление предыдущих версий программ..."

LangString str_msg_registering   ${LANG_RUSSIAN} \
    "Регистрация компонентов программы в системе..."

LangString str_msg_unregistering ${LANG_RUSSIAN} \
    "Отмена регистрации компонентов программы в системе..."


##############################################################################
# Dialog Box                                                              {{{1
##############################################################################

LangString str_mnvrc_page_title    ${LANG_RUSSIAN} \
    "Установка параметров программы"
LangString str_mnvrc_page_subtitle ${LANG_RUSSIAN} \
    "Параметры, используемые для клавиатуры, манипулятора «мышь» и \
    функциональности программы"

LangString str_msg_compat_title    ${LANG_RUSSIAN} \
    " Варианты использования программы "
LangString str_msg_compat_desc     ${LANG_RUSSIAN} \
    "Совместимость и функциональность программы"
LangString str_msg_compat_vi       ${LANG_RUSSIAN} \
    "Работа в варианте совместимости с редактором Vi"
LangString str_msg_compat_mnv      ${LANG_RUSSIAN} \
    "Работа в варианте функциональности редактора MNV"
LangString str_msg_compat_defaults ${LANG_RUSSIAN} \
    "Включить некоторые улучшения (из файла defaults.mnv)"
LangString str_msg_compat_all      ${LANG_RUSSIAN} \
    "Включить все улучшения (из файла mnvrc_example.mnv). Стандартно"

LangString str_msg_keymap_title   ${LANG_RUSSIAN} \
    " Клавиатурные команды "
LangString str_msg_keymap_desc    ${LANG_RUSSIAN} \
    "Изменение клавиатурных команд CTRL+V, CTRL+C, CTRL+S, CTRL+F и т. п."
LangString str_msg_keymap_default ${LANG_RUSSIAN} \
    "Без изменения, использовать как принято в редакторе MNV"
LangString str_msg_keymap_windows ${LANG_RUSSIAN} \
    "Изменить и использовать как принято в ОС Windows"

LangString str_msg_mouse_title   ${LANG_RUSSIAN} \
    " Манипулятор «мышь» "
LangString str_msg_mouse_desc    ${LANG_RUSSIAN} \
    "Действия правой и левой кнопки манипулятора «мышь»"
LangString str_msg_mouse_default ${LANG_RUSSIAN} \
    "Правая — всплывающее меню, левая — режим визуальный (MNV)"
LangString str_msg_mouse_windows ${LANG_RUSSIAN} \
    "Правая — всплывающее меню, левая — режим выборки (Windows)"
LangString str_msg_mouse_unix    ${LANG_RUSSIAN} \
    "Правая — расширение выборки, левая — режим визуальный (UNIX)"
