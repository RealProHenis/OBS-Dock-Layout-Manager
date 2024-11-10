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
#include <QMouseEvent>
#include <QByteArray>
#include <QApplication>
#include <QDockWidget>
#include <QSettings>
#include <QInputDialog>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QFont>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-auto-dock-resizer", "en-US")

#define PLUGIN_NAME "OBS Auto Dock Resizer"
#define PLUGIN_VERSION "1.1.3" // Update in 'buildspec.json' too

static QString settingsFilePath; // Global variable for settings file path

class DockListDialog : public QDialog
{
    Q_OBJECT

public:
    DockListDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Dock Layout Manager");
        resize(400, 300);

        QVBoxLayout *layout = new QVBoxLayout(this);

        // Initialize the list widget
        list_widget = new QListWidget(this);
        list_widget->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(list_widget);

        // Install event filter on the list widget's viewport
        list_widget->viewport()->installEventFilter(this);

        // Add a horizontal layout for the buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout;

        // Initialize buttons and add to layout with tooltips
        QPushButton *newButton = new QPushButton("New", this);
        newButton->setToolTip("Create a new dock layout");
        connect(newButton, &QPushButton::clicked, this, &DockListDialog::newDockLayout);
        buttonLayout->addWidget(newButton);

        saveButton = new QPushButton("Save", this);
        saveButton->setToolTip("Save the current dock layout");
        connect(saveButton, &QPushButton::clicked, this, &DockListDialog::saveDockLayout);
        buttonLayout->addWidget(saveButton);

        restoreButton = new QPushButton("Restore", this);
        restoreButton->setToolTip("Restore the selected dock layout");
        connect(restoreButton, &QPushButton::clicked, this, &DockListDialog::restoreDockLayout);
        buttonLayout->addWidget(restoreButton);

        renameButton = new QPushButton("Rename", this);
        renameButton->setToolTip("Rename the selected dock layout");
        connect(renameButton, &QPushButton::clicked, this, &DockListDialog::renameDockLayout);
        buttonLayout->addWidget(renameButton);

        deleteButton = new QPushButton("Delete", this);
        deleteButton->setToolTip("Delete the selected dock layout");
        connect(deleteButton, &QPushButton::clicked, this, &DockListDialog::deleteDockLayout);
        buttonLayout->addWidget(deleteButton);

        setDefaultButton = new QPushButton("Set as Default", this);
        setDefaultButton->setToolTip("Automatically apply the selected dock layout at startup");
        connect(setDefaultButton, &QPushButton::clicked, this, &DockListDialog::setAsDefaultLayout);
        buttonLayout->addWidget(setDefaultButton);

        // Add the button layout to the main layout
        layout->addLayout(buttonLayout);

        // Set the main layout
        setLayout(layout);

        // Connect the selection change signal to update the button states
        connect(list_widget, &QListWidget::itemSelectionChanged, this, &DockListDialog::updateButtonStates);

        // Populate the list initially
        updateDockList();

        // Initialize the buttons' states
        updateButtonStates();
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (obj == list_widget->viewport() && event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QModelIndex clickedIndex = list_widget->indexAt(mouseEvent->pos());
            if (!clickedIndex.isValid()) {
                list_widget->clearSelection();
                updateButtonStates();
                return true; // Event has been handled
            }
        }
        return QDialog::eventFilter(obj, event);
    }

private slots:
    void updateDockList()
    {
        list_widget->clear();

        QSettings settings(settingsFilePath, QSettings::IniFormat);

        QStringList layoutNames = settings.childGroups();

        // Get default layout from "Settings" group
        settings.beginGroup("Settings");
        QString defaultLayoutName = settings.value("DefaultLayout").toString();
        settings.endGroup();

        for (const QString &name : layoutNames) {
            if (name == "Settings") continue; // Skip the Settings group
            QListWidgetItem *item = new QListWidgetItem(name, list_widget);

            if (name == defaultLayoutName) {
                // Set the font to bold to indicate default
                QFont boldFont = item->font();
                boldFont.setBold(true);
                item->setFont(boldFont);

                // Optionally, set a tooltip to indicate default
                item->setToolTip("Default layout");
            }

            list_widget->addItem(item);
        }
    }

    void updateButtonStates()
    {
        bool validSelection = !list_widget->selectedItems().isEmpty();

        saveButton->setEnabled(validSelection);
        restoreButton->setEnabled(validSelection);
        deleteButton->setEnabled(validSelection);
        setDefaultButton->setEnabled(validSelection);
        renameButton->setEnabled(validSelection); // Enable/disable Rename button
    }

    void saveDockLayout()
    {
        void *main_window_handle = obs_frontend_get_main_window();
        QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

        if (!main_window) {
            QMessageBox::warning(this, "Error", "Failed to get main window");
            return;
        }

        QList<QListWidgetItem *> selectedItems = list_widget->selectedItems();
        if (!selectedItems.isEmpty()) {
            QString layoutName = selectedItems.first()->text().trimmed();

            // Confirm overwrite
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "Overwrite Layout",
                                        QString("Are you sure you want to overwrite the '%1' layout with the current dock sizes & positions?").arg(layoutName),
                                        QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes) {
                return;
            }

            QSettings settings(settingsFilePath, QSettings::IniFormat);

            // Remove existing layout
            settings.remove(layoutName);

            // Now proceed to save the new layout
            QByteArray windowState = main_window->saveState();

            if (windowState.isEmpty()) {
                QMessageBox::warning(this, "Error", "Failed to save window state. Ensure that the main window state is valid.");
                return;
            }

            settings.beginGroup(layoutName);
            settings.setValue("WindowState", windowState);
            settings.endGroup(); // layoutName

            settings.sync(); // Ensure changes are written to disk

            updateDockList(); // Update the list to reflect changes
        } else {
            // No layout selected, prompt the user to select a layout
            QMessageBox::information(this, "No Layout Selected", "Please select an existing layout to overwrite.");
            return;
        }
    }

    void restoreDockLayout()
    {
        void *main_window_handle = obs_frontend_get_main_window();
        QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

        if (!main_window) {
            QMessageBox::warning(this, "Error", "Failed to get main window");
            return;
        }

        QList<QListWidgetItem *> selectedItems = list_widget->selectedItems();

        if (!selectedItems.isEmpty()) {
            QString layoutName = selectedItems.first()->text();

            QSettings settings(settingsFilePath, QSettings::IniFormat);
            settings.beginGroup(layoutName);
            QByteArray windowState = settings.value("WindowState").toByteArray();
            settings.endGroup();

            if (!windowState.isEmpty()) {
                if (!main_window->restoreState(windowState)) {
                    QMessageBox::warning(this, "Error", "Failed to restore dock layout");
                    return;
                }
            } else {
                QMessageBox::warning(this, "Error", "Selected layout does not contain valid window state");
                return;
            }
        } else {
            QMessageBox::warning(this, "Error", "Please select a layout to restore");
            return;
        }
    }

    void deleteDockLayout()
    {
        QList<QListWidgetItem *> selectedItems = list_widget->selectedItems();

        if (!selectedItems.isEmpty()) {
            QString layoutName = selectedItems.first()->text();

            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "Delete Layout",
                                        QString("Are you sure you want to delete the '%1' layout?").arg(layoutName),
                                        QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes) {
                return;
            }

            QSettings settings(settingsFilePath, QSettings::IniFormat);

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

            updateDockList();
        } else {
            QMessageBox::warning(this, "Error", "Please select a layout to delete");
            return;
        }
    }

    void setAsDefaultLayout()
    {
        QList<QListWidgetItem *> selectedItems = list_widget->selectedItems();

        if (!selectedItems.isEmpty()) {
            QString layoutName = selectedItems.first()->text();

            // Save the current window state to the selected layout
            void *main_window_handle = obs_frontend_get_main_window();
            QMainWindow *main_window = static_cast<QMainWindow *>(main_window_handle);

            if (!main_window) {
                QMessageBox::warning(this, "Error", "Failed to get main window");
                return;
            }

            QByteArray windowState = main_window->saveState();

            if (windowState.isEmpty()) {
                QMessageBox::warning(this, "Error", "Failed to save window state. Ensure that the main window state is valid.");
                return;
            }

            QSettings settings(settingsFilePath, QSettings::IniFormat);

            settings.beginGroup(layoutName);
            settings.setValue("WindowState", windowState);
            settings.endGroup(); // layoutName

            settings.sync(); // Ensure changes are written to disk

            // Now set this layout as the default
            settings.beginGroup("Settings");
            settings.setValue("DefaultLayout", layoutName);
            settings.endGroup();
            settings.sync(); // Ensure changes are written to disk

            updateDockList();
        } else {
            QMessageBox::warning(this, "Error", "Please select a layout to set as default");
            return;
        }
    }

    void newDockLayout()
    {
        bool ok;
        QString newLayoutName = QInputDialog::getText(this, "New Dock Layout", "Enter a name for the new layout:", QLineEdit::Normal, "", &ok);

        if (!ok || newLayoutName.isEmpty()) {
            return; // User cancelled or entered an empty name
        }

        newLayoutName = newLayoutName.trimmed(); // Remove any extra whitespace

        QSettings settings(settingsFilePath, QSettings::IniFormat);

        // Check if a layout with this name already exists
        QStringList existingLayouts = settings.childGroups();
        existingLayouts.removeAll("Settings"); // Exclude the Settings group
        if (existingLayouts.contains(newLayoutName)) {
            QMessageBox::warning(this, "Error", QString("A layout with the name '%1' already exists. Please choose a different name.").arg(newLayoutName));
            return;
        }

        // FIX: Create a new layout group and initialize it with an empty WindowState
        settings.beginGroup(newLayoutName);
        settings.setValue("WindowState", QByteArray()); // Initialize with empty WindowState
        settings.endGroup();

        settings.sync(); // Ensure changes are written to disk

        updateDockList();
    }

    // *** New slot for renaming a layout ***
    void renameDockLayout()
    {
        QList<QListWidgetItem *> selectedItems = list_widget->selectedItems();

        if (selectedItems.isEmpty()) {
            QMessageBox::warning(this, "Error", "Please select a layout to rename.");
            return;
        }

        QString oldName = selectedItems.first()->text();

        bool ok;
        QString newName = QInputDialog::getText(this, "Rename Layout",
                                                QString("Enter a new name for the '%1' layout:").arg(oldName),
                                                QLineEdit::Normal, oldName, &ok);

        if (!ok || newName.isEmpty()) {
            return; // User cancelled or entered an empty name
        }

        newName = newName.trimmed();

        if (newName == oldName) {
            // No change in name
            return;
        }

        QSettings settings(settingsFilePath, QSettings::IniFormat);

        // Check if the new name already exists
        QStringList existingLayouts = settings.childGroups();
        existingLayouts.removeAll("Settings"); // Exclude the Settings group
        if (existingLayouts.contains(newName)) {
            QMessageBox::warning(this, "Error", QString("A layout with the name '%1' already exists. Please choose a different name.").arg(newName));
            return;
        }

        // Retrieve the window state from the old layout
        settings.beginGroup(oldName);
        QByteArray windowState = settings.value("WindowState").toByteArray();
        settings.endGroup();

        if (windowState.isEmpty()) {
            QMessageBox::warning(this, "Error", QString("The '%1' layout does not contain a valid window state.").arg(oldName));
            return;
        }

        // Create the new layout group with the same window state
        settings.beginGroup(newName);
        settings.setValue("WindowState", windowState);
        settings.endGroup();

        // Remove the old layout group
        settings.remove(oldName);

        // If the old layout was the default, update the default to the new name
        settings.beginGroup("Settings");
        QString defaultLayoutName = settings.value("DefaultLayout").toString();
        if (defaultLayoutName == oldName) {
            settings.setValue("DefaultLayout", newName);
        }
        settings.endGroup();

        settings.sync(); // Ensure changes are written to disk

        updateDockList();
    }
    // *** End of renameDockLayout slot ***

private:
    QListWidget *list_widget;
    QPushButton *saveButton;
    QPushButton *restoreButton;
    QPushButton *deleteButton;
    QPushButton *setDefaultButton;
    QPushButton *renameButton; // New Rename button
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

    QSettings settings(settingsFilePath, QSettings::IniFormat);
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

    if (!windowState.isEmpty()) {
        if (!main_window->restoreState(windowState)) {
            blog(LOG_WARNING, "Failed to restore default dock layout '%s'", defaultLayoutName.toUtf8().constData());
            return;
        }
    } else {
        blog(LOG_WARNING, "Default layout '%s' does not contain valid window state", defaultLayoutName.toUtf8().constData());
        return;
    }

    blog(LOG_INFO, "Default dock layout '%s' restored successfully", defaultLayoutName.toUtf8().constData());
}

// Frontend event callback
void on_frontend_event(enum obs_frontend_event event, void *private_data)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        // Delay the restoration to ensure all docks are loaded
        QTimer::singleShot(500, restore_default_layout); // Delay of 500 milliseconds
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
    // FIX: Ensure the settings file path includes the .ini extension
    const char *moduleFilePathCStr = obs_get_module_file_name(obs_current_module());
    QString moduleFilePath = QString::fromUtf8(moduleFilePathCStr);
    QFileInfo moduleFileInfo(moduleFilePath);
    QString moduleDir = moduleFileInfo.absolutePath();
    settingsFilePath = QDir::cleanPath(moduleDir + QDir::separator() + "obs-auto-dock-profiles.ini"); // Added .ini extension

    // Log the settings file path
    blog(LOG_INFO, "Settings file path: %s", settingsFilePath.toUtf8().constData());

    // Ensure the settings file exists
    QDir dir(moduleDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

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