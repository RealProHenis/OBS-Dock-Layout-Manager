/*
OBS Auto Dock Resizer
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QByteArray>
#include <QApplication>
#include <QDockWidget>
#include <QSettings>
#include <QInputDialog>
#include <QTimer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-auto-dock-resizer", "en-US")

#define PLUGIN_NAME "OBS Auto Dock Resizer"
#define PLUGIN_VERSION "1.1.3"

class DockListDialog : public QDialog
{
    Q_OBJECT

public:
    DockListDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Dock Layout Manager");
        resize(400, 300); // Set initial size

        QVBoxLayout *layout = new QVBoxLayout(this);

        // Add the list widget at the top
        list_widget = new QListWidget(this);
        layout->addWidget(list_widget);

        // Add a horizontal layout for the buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout;

        // Add the Save Layout button
        QPushButton *saveButton = new QPushButton("Save Dock Layout", this);
        connect(saveButton, &QPushButton::clicked, this, &DockListDialog::saveDockLayout);
        buttonLayout->addWidget(saveButton);

        // Add the Restore Layout button
        QPushButton *restoreButton = new QPushButton("Restore Dock Layout", this);
        connect(restoreButton, &QPushButton::clicked, this, &DockListDialog::restoreDockLayout);
        buttonLayout->addWidget(restoreButton);

        // Add the Delete Layout button
        QPushButton *deleteButton = new QPushButton("Delete Dock Layout", this);
        connect(deleteButton, &QPushButton::clicked, this, &DockListDialog::deleteDockLayout);
        buttonLayout->addWidget(deleteButton);

        // Add the Set as Default button
        QPushButton *setDefaultButton = new QPushButton("Set as Default", this);
        connect(setDefaultButton, &QPushButton::clicked, this, &DockListDialog::setAsDefaultLayout);
        buttonLayout->addWidget(setDefaultButton);

        // Add the button layout to the main layout
        layout->addLayout(buttonLayout);

        // Set the main layout
        setLayout(layout);

        // Populate the list initially
        updateDockList();
    }

private slots:
    void updateDockList()
    {
        list_widget->clear();

        QSettings settings("OBS", "DockLayouts");

        QStringList layoutNames = settings.childGroups();

        // Get default layout from "Settings" group
        settings.beginGroup("Settings");
        QString defaultLayoutName = settings.value("DefaultLayout").toString();
        settings.endGroup();

        for (const QString &name : layoutNames) {
            if (name == "Settings") continue; // Skip the Settings group
            QString displayName = name;
            if (name == defaultLayoutName) {
                displayName += " (Default)";
            }
            list_widget->addItem(displayName);
        }

        if (layoutNames.isEmpty() || (layoutNames.size() == 1 && layoutNames.contains("Settings"))) {
            list_widget->addItem("No saved layouts");
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

        bool ok;
        QString layoutName = QInputDialog::getText(this, "Save Dock Layout", "Enter a name for the layout:", QLineEdit::Normal, "", &ok);

        if (!ok || layoutName.isEmpty()) {
            return; // User cancelled or empty name
        }

        QByteArray windowState = main_window->saveState();

        QSettings settings("OBS", "DockLayouts");
        settings.beginGroup(layoutName);
        settings.setValue("WindowState", windowState);
        settings.endGroup();
        settings.sync(); // Ensure changes are written to disk

        QMessageBox::information(this, "Success", "Dock layout saved successfully");

        updateDockList(); // Update the list to include the new layout
    }

    void restoreDockLayout()
    {
        void *main_window_handle = obs_frontend_get_main_window();
        QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

        if (!main_window) {
            QMessageBox::warning(this, "Error", "Failed to get main window");
            return;
        }

        QListWidgetItem *currentItem = list_widget->currentItem();

        if (!currentItem) {
            QMessageBox::warning(this, "Error", "Please select a layout to restore");
            return;
        }

        QString layoutName = currentItem->text();

        if (layoutName == "No saved layouts") {
            QMessageBox::warning(this, "Error", "No saved layouts to restore");
            return;
        }

        // Remove "(Default)" from the name if present
        if (layoutName.endsWith(" (Default)")) {
            layoutName.chop(9); // Remove last 9 characters
        }

        QSettings settings("OBS", "DockLayouts");
        settings.beginGroup(layoutName);
        QByteArray windowState = settings.value("WindowState").toByteArray();
        settings.endGroup();

        if (windowState.isEmpty()) {
            QMessageBox::warning(this, "Error", "Failed to retrieve the saved layout");
            return;
        }

        if (!main_window->restoreState(windowState)) {
            QMessageBox::warning(this, "Error", "Failed to restore dock layout");
            return;
        }

        QMessageBox::information(this, "Success", "Dock layout restored successfully");
    }

    void deleteDockLayout()
    {
        QListWidgetItem *currentItem = list_widget->currentItem();

        if (!currentItem) {
            QMessageBox::warning(this, "Error", "Please select a layout to delete");
            return;
        }

        QString layoutName = currentItem->text();

        if (layoutName == "No saved layouts") {
            QMessageBox::warning(this, "Error", "No saved layouts to delete");
            return;
        }

        // Remove "(Default)" from the name if present
        if (layoutName.endsWith(" (Default)")) {
            layoutName.chop(9); // Remove last 9 characters
        }

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Delete Layout", QString("Are you sure you want to delete the layout '%1'?").arg(layoutName),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }

        QSettings settings("OBS", "DockLayouts");

        // Check if the deleted layout is the default
        settings.beginGroup("Settings");
        QString defaultLayoutName = settings.value("DefaultLayout").toString();
        if (defaultLayoutName == layoutName) {
            settings.remove("DefaultLayout");
        }
        settings.endGroup();

        // Remove the entire group corresponding to the layout
        settings.remove(layoutName);

        settings.sync(); // Ensure changes are written to disk

        QMessageBox::information(this, "Success", "Dock layout deleted successfully");

        updateDockList();
    }

    void setAsDefaultLayout()
    {
        QListWidgetItem *currentItem = list_widget->currentItem();

        if (!currentItem) {
            QMessageBox::warning(this, "Error", "Please select a layout to set as default");
            return;
        }

        QString layoutName = currentItem->text();

        if (layoutName == "No saved layouts") {
            QMessageBox::warning(this, "Error", "No saved layouts to set as default");
            return;
        }

        // Remove "(Default)" from the name if present
        if (layoutName.endsWith(" (Default)")) {
            layoutName.chop(9); // Remove last 9 characters
        }

        QSettings settings("OBS", "DockLayouts");
        settings.beginGroup("Settings");
        settings.setValue("DefaultLayout", layoutName);
        settings.endGroup();
        settings.sync(); // Ensure changes are written to disk

        QMessageBox::information(this, "Success", QString("Layout '%1' set as default").arg(layoutName));

        updateDockList();
    }

private:
    QListWidget *list_widget;
};

#include "plugin-main.moc"

// Global pointer to the dialog
static DockListDialog *dockListDialog = nullptr;

// Function to restore the default layout after a delay
void restore_default_layout()
{
    void *main_window_handle = obs_frontend_get_main_window();
    QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

    if (!main_window) {
        blog(LOG_WARNING, "Failed to get main window");
        return;
    }

    QSettings settings("OBS", "DockLayouts");
    settings.beginGroup("Settings");
    QString defaultLayoutName = settings.value("DefaultLayout").toString();
    settings.endGroup();

    if (defaultLayoutName.isEmpty()) {
        blog(LOG_INFO, "No default layout set");
        return;
    }

    settings.beginGroup(defaultLayoutName);
    QByteArray windowState = settings.value("WindowState").toByteArray();
    settings.endGroup();

    if (windowState.isEmpty()) {
        blog(LOG_WARNING, "Default layout '%s' not found", defaultLayoutName.toUtf8().constData());
        return;
    }

    if (!main_window->restoreState(windowState)) {
        blog(LOG_WARNING, "Failed to restore default dock layout '%s'", defaultLayoutName.toUtf8().constData());
        return;
    }

    blog(LOG_INFO, "Default dock layout '%s' restored successfully", defaultLayoutName.toUtf8().constData());
}

// Frontend event callback
void on_frontend_event(enum obs_frontend_event event, void *private_data)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        // Delay the restoration to ensure all docks are loaded
        QTimer::singleShot(2000, restore_default_layout); // Delay of 2000 milliseconds (2 seconds)
    }
}

// Callback function for the Tools menu item
void show_dock_layout_manager(void *)
{
    if (!dockListDialog) {
        // Parent the dialog to the main OBS window
        void *main_window_handle = obs_frontend_get_main_window();
        QWidget *main_window = static_cast<QWidget *>(main_window_handle);

        dockListDialog = new DockListDialog(main_window);
        dockListDialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dockListDialog, &QDialog::destroyed, []() {
            dockListDialog = nullptr;
        });
    }
    dockListDialog->show();
    dockListDialog->raise();
    dockListDialog->activateWindow();
}

bool obs_module_load(void)
{
    // Add the plugin to the Tools menu
    obs_frontend_push_ui_translation(obs_module_get_string);
    obs_frontend_add_tools_menu_item("Dock Layout Manager", show_dock_layout_manager, nullptr);
    obs_frontend_pop_ui_translation();

    // Add frontend event callback
    obs_frontend_add_event_callback(on_frontend_event, nullptr);

    blog(LOG_INFO, "%s plugin loaded successfully (version %s)", PLUGIN_NAME, PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    if (dockListDialog) {
        dockListDialog->close();
        dockListDialog = nullptr;
    }

    blog(LOG_INFO, "%s plugin unloaded", PLUGIN_NAME);
}