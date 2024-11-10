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

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-auto-dock-resizer", "en-US")

#define PLUGIN_NAME "OBS Auto Dock Resizer"
#define PLUGIN_VERSION "1.0.0"

class DockListDialog : public QDialog
{
    Q_OBJECT

public:
    DockListDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Dock Layout Manager");
        resize(400, 300); // Optional: Set initial size

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

        QStringList layoutNames = settings.childKeys();

        for (const QString &name : layoutNames) {
            list_widget->addItem(name);
        }

        if (layoutNames.isEmpty()) {
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

        settings.setValue(layoutName, windowState);

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

        QSettings settings("OBS", "DockLayouts");

        QByteArray windowState = settings.value(layoutName).toByteArray();

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

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Delete Layout", QString("Are you sure you want to delete the layout '%1'?").arg(layoutName),
                                      QMessageBox::Yes|QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }

        QSettings settings("OBS", "DockLayouts");

        settings.remove(layoutName);

        QMessageBox::information(this, "Success", "Dock layout deleted successfully");

        updateDockList();
    }

private:
    QListWidget *list_widget;
};

#include "plugin-main.moc"

// Global pointer to the dialog
static DockListDialog *dockListDialog = nullptr;

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