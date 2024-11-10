/*
OBS Auto Dock Resizer
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-auto-dock-resizer", "en-US")

#define PLUGIN_NAME "OBS Auto Dock Resizer"
#define PLUGIN_VERSION "1.0.0"

bool obs_module_load(void)
{
    // Create a QWidget that will be the content of the dock
    QWidget *dock_widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(dock_widget);
    QLabel *label = new QLabel("Hello, this is a dockable window!", dock_widget);
    layout->addWidget(label);
    dock_widget->setLayout(layout);

    // Register the dock using obs_frontend_add_dock_by_id
    obs_frontend_push_ui_translation(obs_module_get_string);
    bool success = obs_frontend_add_dock_by_id("obs_auto_dock_resizer_dock", "Auto Dock Resizer", dock_widget);
    obs_frontend_pop_ui_translation();

    if (!success) {
        blog(LOG_ERROR, "%s: Failed to add dock", PLUGIN_NAME);
        delete dock_widget;
        return false;
    }

    blog(LOG_INFO, "%s plugin loaded successfully (version %s)", PLUGIN_NAME, PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    // Remove the dock when the plugin is unloaded
    obs_frontend_remove_dock("obs_auto_dock_resizer_dock");
    blog(LOG_INFO, "%s plugin unloaded", PLUGIN_NAME);
}