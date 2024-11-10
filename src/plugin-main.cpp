/*
OBS Auto Dock Resizer
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QListWidget>
#include <QTimer>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QByteArray>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-auto-dock-resizer", "en-US")

#define PLUGIN_NAME "OBS Auto Dock Resizer"
#define PLUGIN_VERSION "1.0.0"

class DockListWidget : public QWidget
{
    Q_OBJECT

public:
    DockListWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        QVBoxLayout *layout = new QVBoxLayout(this);

        // Add the Save Layout button
        QPushButton *saveButton = new QPushButton("Save Dock Layout", this);
        connect(saveButton, &QPushButton::clicked, this, &DockListWidget::saveDockLayout);
        layout->addWidget(saveButton);

        // Add the Restore Layout button
        QPushButton *restoreButton = new QPushButton("Restore Dock Layout", this);
        connect(restoreButton, &QPushButton::clicked, this, &DockListWidget::restoreDockLayout);
        layout->addWidget(restoreButton);

        list_widget = new QListWidget(this);
        layout->addWidget(list_widget);
        setLayout(layout);

        // Populate the list initially
        updateDockList();

        // Set up a timer to refresh the list periodically (optional)
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &DockListWidget::updateDockList);
        timer->start(2000); // Update every 2 seconds
    }

private slots:
    void updateDockList()
    {
        list_widget->clear();

        void *main_window_handle = obs_frontend_get_main_window();
        QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

        if (!main_window) {
            list_widget->addItem("Failed to get main window");
            return;
        }

        QList<QDockWidget *> docks = main_window->findChildren<QDockWidget *>();

        for (QDockWidget *dock : docks) {
            QString dock_name = dock->windowTitle();
            if (dock_name.isEmpty()) {
                dock_name = dock->objectName();
            }

            QSize size = dock->size();
            QString item_text = QString("%1 - Size: %2 x %3")
                                    .arg(dock_name)
                                    .arg(size.width())
                                    .arg(size.height());

            list_widget->addItem(item_text);
        }

        if (docks.isEmpty()) {
            list_widget->addItem("No docks found");
        }
    }

    void saveDockLayout()
    {
        void *main_window_handle = obs_frontend_get_main_window();
        QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

        if (!main_window) {
            QMessageBox::warning(this, "Error", "Failed to get main window");
            return;
        }

        QByteArray windowState = main_window->saveState();

        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(configDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        QString filePath = dir.filePath("dock_layout.dat");

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "Error", "Failed to save dock layout");
            return;
        }

        file.write(windowState);
        file.close();

        QMessageBox::information(this, "Success", "Dock layout saved successfully");
    }

    void restoreDockLayout()
    {
        void *main_window_handle = obs_frontend_get_main_window();
        QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

        if (!main_window) {
            QMessageBox::warning(this, "Error", "Failed to get main window");
            return;
        }

        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString filePath = QDir(configDir).filePath("dock_layout.dat");

        QFile file(filePath);
        if (!file.exists()) {
            QMessageBox::warning(this, "Error", "No saved dock layout found");
            return;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "Error", "Failed to open dock layout file for reading");
            return;
        }

        QByteArray windowState = file.readAll();
        file.close();

        if (!main_window->restoreState(windowState)) {
            QMessageBox::warning(this, "Error", "Failed to restore dock layout");
            return;
        }

        QMessageBox::information(this, "Success", "Dock layout restored successfully");
    }

private:
    QListWidget *list_widget;
};

bool obs_module_load(void)
{
    // Create our custom widget
    DockListWidget *dock_widget = new DockListWidget;

    // Register the dock using obs_frontend_add_dock_by_id
    obs_frontend_push_ui_translation(obs_module_get_string);
    bool success = obs_frontend_add_dock_by_id("obs_auto_dock_resizer_dock", "Dock List", dock_widget);
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

#include "plugin-main.moc" // Include the MOC file for Qt signals and slots