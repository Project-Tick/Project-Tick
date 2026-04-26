/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: MIT
 *
 * LinuxPerfPlugin — MMCO plugin for MangoHud and GameMode integration.
 *
 * Platforms: Linux only (UNIX && NOT APPLE).
 *
 * What it does
 * ============
 *   MangoHud  — Prepends the `mangohud` wrapper command so that the MangoHud
 *               overlay (FPS, frame timing, GPU/CPU metrics) is injected into
 *               Minecraft via LD_PRELOAD at launch.  Also sets MANGOHUD=1 and
 *               MANGOHUD_DLSYM=1 so both OpenGL and Vulkan backends are covered.
 *
 *   GameMode  — Prepends the `gamemoderun` wrapper command so that Feral
 *               Interactive's GameMode daemon raises the process priority and
 *               applies CPU governor / scheduler optimisations for the
 *               duration of the game session.
 *
 * When both are enabled the final command line becomes:
 *   gamemoderun mangohud java [jvm args] -jar game.jar
 *
 * Availability detection
 * ======================
 *   • MangoHud:  QStandardPaths::findExecutable("mangohud")
 *   • GameMode:  QStandardPaths::findExecutable("gamemoderun")
 *                + optional libgamemode.so probe via gamemode_client.h
 *
 * The checkboxes are disabled (not hidden) when the tools are absent so that
 * users understand the feature exists but needs the tool installed.
 *
 * UI injection
 * ============
 *   A "Linux Performance Tools" group box is injected into the Minecraft
 *   Settings page (verticalLayout_3) at plugin init time and whenever
 *   globalSettingsAboutToOpen fires, following the same pattern as the
 *   NVIDIAPrime plugin.
 *
 * Settings
 * ========
 *   plugin.linuxperf.mangohud.enabled  — bool (default false)
 *   plugin.linuxperf.gamemode.enabled  — bool (default false)
 */

#include "plugin/sdk/mmco_sdk.h"

#include "ui/pages/instance/InstanceSettingsPage.h"

/* Gamemode client (header-only, dynamic loading) */
#include "vendor/gamemode_client.h"

#include <QStandardPaths>

/* ──────────────────────────────────────────────────────────────────────────── */
/* Module metadata                                                              */
/* ──────────────────────────────────────────────────────────────────────────── */

MMCO_DEFINE_MODULE("Linux Performance Tools", "1.0.0", "Project Tick",
                   "MangoHud FPS overlay and GameMode performance integration "
                   "for Minecraft on Linux",
                   "MIT");

/* ──────────────────────────────────────────────────────────────────────────── */
/* Constants                                                                    */
/* ──────────────────────────────────────────────────────────────────────────── */

static constexpr const char SETTING_MANGOHUD[]  = "plugin.linuxperf.mangohud.enabled";
static constexpr const char SETTING_GAMEMODE[]  = "plugin.linuxperf.gamemode.enabled";
static constexpr const char SETTING_INSTANCE_OVERRIDE[] = "plugin.linuxperf.override";

/* ──────────────────────────────────────────────────────────────────────────── */
/* Global state                                                                 */
/* ──────────────────────────────────────────────────────────────────────────── */

static MMCOContext *g_ctx              = nullptr;
static QObject    *g_guard             = nullptr;

/* Widgets — raw ptrs, owned by Qt dialog tree */
static QCheckBox  *g_mangoCheckbox     = nullptr;
static QCheckBox  *g_gamemodeCheckbox  = nullptr;

/* ──────────────────────────────────────────────────────────────────────────── */
/* Helpers                                                                      */
/* ──────────────────────────────────────────────────────────────────────────── */

static bool is_flatpak()
{
    return QFile::exists(QStringLiteral("/.flatpak-info"));
}

static bool mangohud_available()
{
    /* Inside Flatpak the layer is pre-loaded by the runtime; no binary check. */
    if (is_flatpak())
        return true;
    return !QStandardPaths::findExecutable(QStringLiteral("mangohud")).isEmpty();
}

static bool gamemoderun_available()
{
    return !QStandardPaths::findExecutable(QStringLiteral("gamemoderun")).isEmpty();
}

/* ──────────────────────────────────────────────────────────────────────────── */
/* Setting helpers                                                              */
/* ──────────────────────────────────────────────────────────────────────────── */

static void ensureSettingsRegistered()
{
    auto s = APPLICATION->settings();
    auto ensureKey = [&](const char *key) {
        QString k = QString::fromLatin1(key);
        if (!s->contains(k))
            s->registerSetting(k, false);
    };
    ensureKey(SETTING_MANGOHUD);
    ensureKey(SETTING_GAMEMODE);
}

static bool isMangohudEnabled()
{
    auto s = APPLICATION->settings();
    QString k = QString::fromLatin1(SETTING_MANGOHUD);
    return s->contains(k) && s->get(k).toBool();
}

static bool isGamemodeEnabled()
{
    auto s = APPLICATION->settings();
    QString k = QString::fromLatin1(SETTING_GAMEMODE);
    return s->contains(k) && s->get(k).toBool();
}

static void ensureInstanceSettingsRegistered(BaseInstance *instance)
{
    if (!instance)
        return;

    ensureSettingsRegistered();

    auto settings = instance->settings();
    if (!settings)
        return;

    auto gateKey = QString::fromLatin1(SETTING_INSTANCE_OVERRIDE);
    auto gate = settings->getSetting(gateKey);
    if (!gate)
        gate = settings->registerSetting(gateKey, false);
    if (!gate)
        return;

    auto registerOverrideIfMissing = [&](const char *key) {
        QString settingKey = QString::fromLatin1(key);
        if (settings->contains(settingKey))
            return;
        auto original = APPLICATION->settings()->getSetting(settingKey);
        if (original)
            settings->registerOverride(original, gate);
    };

    registerOverrideIfMissing(SETTING_MANGOHUD);
    registerOverrideIfMissing(SETTING_GAMEMODE);
}

static bool isMangohudEnabledForInstance(BaseInstance *instance)
{
    if (!instance)
        return isMangohudEnabled();

    ensureInstanceSettingsRegistered(instance);

    auto settings = instance->settings();
    QString key = QString::fromLatin1(SETTING_MANGOHUD);
    return settings && settings->contains(key) && settings->get(key).toBool();
}

static bool isGamemodeEnabledForInstance(BaseInstance *instance)
{
    if (!instance)
        return isGamemodeEnabled();

    ensureInstanceSettingsRegistered(instance);

    auto settings = instance->settings();
    QString key = QString::fromLatin1(SETTING_GAMEMODE);
    return settings && settings->contains(key) && settings->get(key).toBool();
}

/* ──────────────────────────────────────────────────────────────────────────── */
/* UI injection                                                                 */
/* ──────────────────────────────────────────────────────────────────────────── */

static void injectCheckboxesIntoMinecraftPage()
{
    /* Locate the MinecraftPage widget by objectName (set in the .ui file). */
    QWidget *mcPage = nullptr;
    for (auto *w : qApp->allWidgets()) {
        if (w->objectName() == QStringLiteral("MinecraftPage")) {
            mcPage = w;
            break;
        }
    }
    if (!mcPage)
        return;

    /* Find the vertical layout that hosts the Minecraft tab content. */
    auto *layout = mcPage->findChild<QVBoxLayout *>(
        QStringLiteral("verticalLayout_3"));
    if (!layout)
        return;

    /* Skip injection if our group box is already present (re-open guard). */
    if (mcPage->findChild<QGroupBox *>(QStringLiteral("linuxPerfGroupBox")))
        return;

    /* ── Build group box ─────────────────────────────────────────────────── */
    auto *groupBox = new QGroupBox(QObject::tr("Linux Performance Tools"));
    groupBox->setObjectName(QStringLiteral("linuxPerfGroupBox"));
    auto *groupLayout = new QVBoxLayout(groupBox);

    /* ── MangoHud checkbox ───────────────────────────────────────────────── */
    g_mangoCheckbox = new QCheckBox(
        QObject::tr("Enable MangoHud overlay (FPS / GPU / CPU metrics)"),
        groupBox);
    g_mangoCheckbox->setObjectName(QStringLiteral("linuxPerfMangoHudCheck"));
    const bool mangoAvail = mangohud_available();
    if (mangoAvail) {
        QString mangoBin = is_flatpak()
            ? QObject::tr("(Flatpak runtime layer)")
            : QStandardPaths::findExecutable(QStringLiteral("mangohud"));
        g_mangoCheckbox->setToolTip(
            QObject::tr(
                "Injects the MangoHud overlay into Minecraft via the mangohud wrapper.\n"
                "Displays real-time FPS, frame timing, GPU and CPU metrics.\n"
                "Works with both OpenGL and Vulkan (LWJGL2 / LWJGL3).\n\n"
                "Found: %1").arg(mangoBin));
    } else {
        g_mangoCheckbox->setToolTip(
            QObject::tr("MangoHud is not installed or not found on PATH.\n"
                        "Install it (package: mangohud) and reopen this dialog."));
    }
    g_mangoCheckbox->setEnabled(mangoAvail);
    g_mangoCheckbox->setChecked(isMangohudEnabled() && mangoAvail);
    groupLayout->addWidget(g_mangoCheckbox);

    /* ── GameMode checkbox ───────────────────────────────────────────────── */
    g_gamemodeCheckbox = new QCheckBox(
        QObject::tr("Enable GameMode (CPU / scheduler performance optimisations)"),
        groupBox);
    g_gamemodeCheckbox->setObjectName(QStringLiteral("linuxPerfGameModeCheck"));
    g_gamemodeCheckbox->setToolTip(
        QObject::tr(
            "Launches Minecraft via gamemoderun so that Feral Interactive's\n"
            "GameMode daemon can apply CPU governor and scheduler optimisations\n"
            "for the duration of the game session.\n"
            "Requires GameMode to be installed (package: gamemode)."));
    const bool gmAvail = gamemoderun_available();
    g_gamemodeCheckbox->setEnabled(gmAvail);
    if (!gmAvail)
        g_gamemodeCheckbox->setToolTip(
            QObject::tr("gamemoderun is not installed or not found on PATH.\n"
                        "Install GameMode and restart MeshMC to enable this option."));
    g_gamemodeCheckbox->setChecked(isGamemodeEnabled() && gmAvail);
    groupLayout->addWidget(g_gamemodeCheckbox);

    /* ── GameMode daemon status label ───────────────────────────────────── */
    if (gmAvail) {
        /* gamemode_query_status() returns:
         *   0  = daemon running, no game registered
         *   1  = daemon running, some game registered
         *   2  = daemon running, this process registered
         *  -1  = daemon not reachable (not started or libgamemode unavailable)
         *
         * We show DAEMON reachability — not whether a game is currently
         * using GameMode (which would always be 0 / "inactive" pre-launch
         * and mislead the user into thinking the feature is broken). */
        int status = gamemode_query_status();
        QString statusText;
        QString gmBin = QStandardPaths::findExecutable(QStringLiteral("gamemoderun"));
        if (status >= 0) {
            statusText = QObject::tr("GameMode daemon: running — will activate at game launch  (%1)").arg(gmBin);
        } else {
            statusText = QObject::tr("GameMode daemon: not responding — run \"systemctl --user enable --now gamemoded\" or check gamemode install");
        }
        auto *statusLabel = new QLabel(statusText, groupBox);
        statusLabel->setObjectName(QStringLiteral("linuxPerfGameModeStatus"));
        statusLabel->setWordWrap(true);
        QFont f = statusLabel->font();
        f.setPointSizeF(f.pointSizeF() * 0.85);
        statusLabel->setFont(f);
        groupLayout->addWidget(statusLabel);
    }

    /* ── Insert before the spacer at the end of the layout ──────────────── */
    int spacerIdx = layout->count() - 1;
    layout->insertWidget(spacerIdx, groupBox);

    /* ── Persist settings on toggle ──────────────────────────────────────── */
    QObject::connect(
        g_mangoCheckbox, &QCheckBox::toggled, g_guard, [](bool checked) {
            APPLICATION->settings()->set(
                QString::fromLatin1(SETTING_MANGOHUD), checked);
        });
    QObject::connect(
        g_gamemodeCheckbox, &QCheckBox::toggled, g_guard, [](bool checked) {
            APPLICATION->settings()->set(
                QString::fromLatin1(SETTING_GAMEMODE), checked);
        });
}

static void injectCheckboxesIntoInstanceSettingsPage(InstanceSettingsPage *page,
                                                     BaseInstance *instance)
{
    if (!page || !instance)
        return;

    ensureInstanceSettingsRegistered(instance);

    auto *layout = page->findChild<QVBoxLayout *>(QStringLiteral("verticalLayout_8"));
    if (!layout)
        return;

    if (page->findChild<QGroupBox *>(QStringLiteral("linuxPerfInstanceGroupBox")))
        return;

    auto *groupBox = new QGroupBox(
        QObject::tr("Override global Linux performance tools settings"), page);
    groupBox->setObjectName(QStringLiteral("linuxPerfInstanceGroupBox"));
    groupBox->setCheckable(true);

    auto *groupLayout = new QVBoxLayout(groupBox);

    auto *mangoCheckbox = new QCheckBox(
        QObject::tr("Enable MangoHud overlay (FPS / GPU / CPU metrics)"),
        groupBox);
    mangoCheckbox->setObjectName(QStringLiteral("linuxPerfInstanceMangoHudCheck"));
    const bool mangoAvail = mangohud_available();
    if (mangoAvail) {
        QString mangoBin = is_flatpak()
            ? QObject::tr("(Flatpak runtime layer)")
            : QStandardPaths::findExecutable(QStringLiteral("mangohud"));
        mangoCheckbox->setToolTip(
            QObject::tr(
                "Injects the MangoHud overlay into Minecraft via the mangohud wrapper.\n"
                "Displays real-time FPS, frame timing, GPU and CPU metrics.\n"
                "Works with both OpenGL and Vulkan (LWJGL2 / LWJGL3).\n\n"
                "Found: %1").arg(mangoBin));
    } else {
        mangoCheckbox->setToolTip(
            QObject::tr("MangoHud is not installed or not found on PATH.\n"
                        "Install it (package: mangohud) and reopen this dialog."));
    }
    mangoCheckbox->setEnabled(mangoAvail);
    groupLayout->addWidget(mangoCheckbox);

    auto *gamemodeCheckbox = new QCheckBox(
        QObject::tr("Enable GameMode (CPU / scheduler performance optimisations)"),
        groupBox);
    gamemodeCheckbox->setObjectName(QStringLiteral("linuxPerfInstanceGameModeCheck"));
    gamemodeCheckbox->setToolTip(
        QObject::tr(
            "Launches Minecraft via gamemoderun so that Feral Interactive's\n"
            "GameMode daemon can apply CPU governor and scheduler optimisations\n"
            "for the duration of the game session.\n"
            "Requires GameMode to be installed (package: gamemode)."));
    const bool gmAvail = gamemoderun_available();
    gamemodeCheckbox->setEnabled(gmAvail);
    if (!gmAvail) {
        gamemodeCheckbox->setToolTip(
            QObject::tr("gamemoderun is not installed or not found on PATH.\n"
                        "Install GameMode and restart MeshMC to enable this option."));
    }
    groupLayout->addWidget(gamemodeCheckbox);

    if (gmAvail) {
        int status = gamemode_query_status();
        QString statusText;
        QString gmBin = QStandardPaths::findExecutable(QStringLiteral("gamemoderun"));
        if (status >= 0) {
            statusText = QObject::tr("GameMode daemon: running — will activate at game launch  (%1)").arg(gmBin);
        } else {
            statusText = QObject::tr("GameMode daemon: not responding — run \"systemctl --user enable --now gamemoded\" or check gamemode install");
        }
        auto *statusLabel = new QLabel(statusText, groupBox);
        statusLabel->setObjectName(QStringLiteral("linuxPerfInstanceGameModeStatus"));
        statusLabel->setWordWrap(true);
        QFont f = statusLabel->font();
        f.setPointSizeF(f.pointSizeF() * 0.85);
        statusLabel->setFont(f);
        groupLayout->addWidget(statusLabel);
    }

    int spacerIdx = layout->count() - 1;
    layout->insertWidget(spacerIdx, groupBox);

    auto syncFromSettings = [instance, groupBox, mangoCheckbox, gamemodeCheckbox]() {
        auto settings = instance->settings();
        if (!settings)
            return;

        groupBox->setChecked(
            settings->get(QString::fromLatin1(SETTING_INSTANCE_OVERRIDE)).toBool());
        mangoCheckbox->setChecked(
            isMangohudEnabledForInstance(instance) && mangoCheckbox->isEnabled());
        gamemodeCheckbox->setChecked(
            isGamemodeEnabledForInstance(instance) && gamemodeCheckbox->isEnabled());
    };

    QObject::connect(page, &InstanceSettingsPage::settingsLoaded, page,
                     [syncFromSettings]() { syncFromSettings(); });
    QObject::connect(page, &InstanceSettingsPage::settingsAboutToApply, page,
                     [instance, groupBox, mangoCheckbox, gamemodeCheckbox]() {
                         ensureInstanceSettingsRegistered(instance);
                         auto settings = instance->settings();
                         if (!settings)
                             return;

                         bool overrideEnabled = groupBox->isChecked();
                         settings->set(QString::fromLatin1(SETTING_INSTANCE_OVERRIDE),
                                       overrideEnabled);
                         if (overrideEnabled) {
                             settings->set(QString::fromLatin1(SETTING_MANGOHUD),
                                           mangoCheckbox->isChecked());
                             settings->set(QString::fromLatin1(SETTING_GAMEMODE),
                                           gamemodeCheckbox->isChecked());
                         } else {
                             settings->reset(QString::fromLatin1(SETTING_MANGOHUD));
                             settings->reset(QString::fromLatin1(SETTING_GAMEMODE));
                         }
                     });

    syncFromSettings();
}

/* ──────────────────────────────────────────────────────────────────────────── */
/* Hook callbacks                                                               */
/* ──────────────────────────────────────────────────────────────────────────── */

static int on_app_initialized(void * /*mh*/, uint32_t /*hook_id*/,
                               void * /*payload*/, void * /*user_data*/)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "LinuxPerf: MangoHud available=%s  GameMode available=%s  "
             "MangoHud enabled=%s  GameMode enabled=%s",
             mangohud_available() ? "yes" : "no",
             gamemoderun_available() ? "yes" : "no",
             isMangohudEnabled() ? "yes" : "no",
             isGamemodeEnabled() ? "yes" : "no");
    MMCO_LOG(g_ctx, buf);

    /* Hook into the global settings dialog lifecycle so we can inject the
     * checkboxes each time the dialog opens.  g_guard severs the connections
     * during mmco_unload() via Qt's automatic signal disconnection on delete. */
    QObject::connect(
        APPLICATION, &Application::globalSettingsAboutToOpen, g_guard, []() {
            g_mangoCheckbox    = nullptr;
            g_gamemodeCheckbox = nullptr;
            QTimer::singleShot(0, qApp, injectCheckboxesIntoMinecraftPage);
        });
    QObject::connect(
        APPLICATION, &Application::instanceSettingsPageCreated, g_guard,
        [](InstanceSettingsPage *page, BaseInstance *instance) {
            injectCheckboxesIntoInstanceSettingsPage(page, instance);
        });

    return 0;
}

static int on_instance_pre_launch(void *mh, uint32_t /*hook_id*/,
                                  void *payload, void * /*user_data*/)
{
    auto *info = static_cast<MMCOInstanceInfo *>(payload);
    const char *iname = info->instance_name ? info->instance_name : "?";

    /*
     * Wrapper command build order (each call *prepends* to the chain):
     *
     *   Step 1 — mangohud:   pending = "mangohud"
     *   Step 2 — gamemoderun: pending = "gamemoderun mangohud"
     *
     * Final command: gamemoderun mangohud java [args]
     *
     * gamemoderun wraps the entire chain and requests GameMode for the child;
     * mangohud hooks into the JVM's graphics APIs via LD_PRELOAD.
     */

    BaseInstance *instance = nullptr;
    if (APPLICATION->instances() && info->instance_id) {
        auto resolved = APPLICATION->instances()->getInstanceById(
            QString::fromUtf8(info->instance_id));
        if (resolved)
            instance = resolved.get();
    }

    bool mangoEnabled = isMangohudEnabledForInstance(instance) &&
                        mangohud_available();
    bool gamemodeEnabled = isGamemodeEnabledForInstance(instance) &&
                           gamemoderun_available();

    if (mangoEnabled) {
        if (is_flatpak()) {
            /* Inside Flatpak the VulkanLayer is already in the runtime;
             * we only need the env vars — no wrapper needed. */
            g_ctx->launch_set_env(mh, "MANGOHUD",       "1");
            g_ctx->launch_set_env(mh, "MANGOHUD_DLSYM", "1");

            char buf[512];
            snprintf(buf, sizeof(buf),
                     "LinuxPerf: instance '%s': MangoHud env vars injected (Flatpak)",
                     iname);
            MMCO_LOG(g_ctx, buf);
        } else {
            /* Non-Flatpak: use the mangohud wrapper (handles LD_PRELOAD for
             * OpenGL via LWJGL2/LWJGL3 and Vulkan layer for newer MC). */
            g_ctx->launch_prepend_wrapper(mh, "mangohud");

            /* MANGOHUD=1  — enables the Vulkan implicit layer (all MC ≥ 1.17) */
            g_ctx->launch_set_env(mh, "MANGOHUD", "1");

            /* MANGOHUD_DLSYM=1 — hook dlsym so MangoHud intercepts OpenGL
             * functions loaded dynamically by LWJGL (critical for Java/OpenGL) */
            g_ctx->launch_set_env(mh, "MANGOHUD_DLSYM", "1");

            /* MANGOHUD_CONFIGFILE — MangoHud auto-detects config by process name.
             * Since Minecraft runs as "java", it can't find "java.conf" and
             * may silently use no HUD.  Point it at the standard MangoHud.conf
             * so the overlay always appears with sensible defaults. */
            QByteArray mangoCfg =
                (QDir::homePath() + QStringLiteral("/.config/MangoHud/MangoHud.conf"))
                    .toUtf8();
            g_ctx->launch_set_env(mh, "MANGOHUD_CONFIGFILE", mangoCfg.constData());

            char buf[512];
            snprintf(buf, sizeof(buf),
                     "LinuxPerf: instance '%s': mangohud wrapper + env vars applied",
                     iname);
            MMCO_LOG(g_ctx, buf);
        }
    }

    if (gamemodeEnabled) {
        /* Prepend gamemoderun so it wraps the entire command (including mangohud
         * if both are enabled), ensuring GameMode activates for the child PID. */
        g_ctx->launch_prepend_wrapper(mh, "gamemoderun");

        char buf[512];
        snprintf(buf, sizeof(buf),
                 "LinuxPerf: instance '%s': gamemoderun wrapper applied",
                 iname);
        MMCO_LOG(g_ctx, buf);
    }

    return 0; /* Never cancel the launch */
}

/* ──────────────────────────────────────────────────────────────────────────── */
/* MMCO entry points                                                            */
/* ──────────────────────────────────────────────────────────────────────────── */

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext *ctx)
{
    g_ctx = ctx;
    MMCO_LOG(ctx, "LinuxPerf plugin initializing...");

    ensureSettingsRegistered();
    g_guard = new QObject();

    ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized, nullptr);
    ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_instance_pre_launch, nullptr);

    MMCO_LOG(ctx, "LinuxPerf plugin initialized.");
    return 0;
}

MMCO_EXPORT void mmco_unload()
{
    if (g_ctx)
        MMCO_LOG(g_ctx, "LinuxPerf plugin unloading.");
    g_ctx              = nullptr;
    g_mangoCheckbox    = nullptr;
    g_gamemodeCheckbox = nullptr;
    /* g_guard intentionally not deleted — see NVIDIAPrime note */
    g_guard = nullptr;
}

} /* extern "C" */
