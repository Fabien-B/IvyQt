#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <IvyQt/ivyqt.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->label_button, &QPushButton::clicked, this, [=]() {
        //vbox->removeWidget(label);
        ui->label->deleteLater();
    });


    auto ivyqt = new IvyQt("plop", "plop Ready", this);
    ivyqt->start("192.168.1.255", 2010);

    ivyqt->bindMessage("Hello (.*)", ui->label, [=](Peer* sender, QStringList params) {
       qDebug() << "Hello with " << params[0] << " from " << sender->name();
       ui->label->setText(params[0]);
    });

    connect(ui->line_edit, &QLineEdit::returnPressed, this, [=]() {
        ivyqt->send(ui->line_edit->text());
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
