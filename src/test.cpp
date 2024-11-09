#include "test.hpp"

TestWidget::TestWidget(QWidget *parent) : QDockWidget("OBS Dock Resizer", parent) {
    this->parent = parent;

    QWidget *widget = new QWidget();
    this->button->setText("Press me!");
    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget(this->button);

    widget->setLayout(layout);

    setWidget(widget);

    setVisible(false);
    setFloating(true);
    resize(300, 300);

    QObject::connect(button, SIGNAL(clicked()), SLOT(ButtonClicked()));
}

TestWidget::~TestWidget() {

};

void TestWidget::ButtonClicked() {
    QMessageBox::information(this, "Infor", "You clicked on a box!");
}