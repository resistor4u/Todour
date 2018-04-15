#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "todotablemodel.h"

#include "todotxt.h"
#include "settingsdialog.h"
#include "quickadddialog.h"
#include "aboutbox.h"
#include "globals.h"
#include "def.h"

#include <QSortFilterProxyModel>
#include <QFileSystemWatcher>
#include <QTime>
#include <QDebug>
#include <QSettings>
#include <QShortcut>
#include <QCloseEvent>
#include <QtAwesome.h>
#include <QDesktopWidget>

QNetworkAccessManager *networkaccessmanager;
TodoTableModel *model;
QString saved_selection; // Used for selection memory

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    QString title=this->windowTitle();

#ifdef QT_NO_DEBUG
    QCoreApplication::setOrganizationName("Nerdur");
    QCoreApplication::setOrganizationDomain("nerdur.com");
    QCoreApplication::setApplicationName("Todour");
    title.append("-");
#else
    QCoreApplication::setOrganizationName("Nerdur-debug");
    QCoreApplication::setOrganizationDomain("nerdur-debug.com");
    QCoreApplication::setApplicationName("Todour-Debug");
    title.append("-DEBUG-");
#endif

    title.append(VER);
    this->setWindowTitle(title);
    hotkey = new UGlobalHotkeys();
    setHotkey();



    // Check if we're supposed to have the settings from .ini file or not
    if(QCoreApplication::arguments().contains("-useini")){
        QSettings::setDefaultFormat(QSettings::IniFormat);
    }

    // Restore the position of the window
    auto rec = QApplication::desktop()->screenGeometry();
    auto maxx = rec.height();
    auto maxy = rec.width();
    auto minx = rec.topLeft().x();
    auto miny = rec.topLeft().y();

    QSettings settings;
    restoreGeometry(settings.value( "geometry", saveGeometry() ).toByteArray());
    restoreState(settings.value( "savestate", saveState() ).toByteArray());
    auto p = settings.value("pos", pos()).toPoint();

    if(p.x()>=maxx-100 || p.x()<minx){
       p.setX(minx); // Set to minx for safety
    }
    if(p.y()>=maxy-100 || p.y()<miny ){
        p.setY(miny); // Set to miny for safety
    }
    move(p);
    resize(settings.value( "size", size() ).toSize());
    if ( settings.value( "maximized", isMaximized() ).toBool() )
        showMaximized();


    // Fix some font-awesome stuff
    QtAwesome* awesome = new QtAwesome(qApp);
    awesome->initFontAwesome();     // This line is important as it loads the font and initializes the named icon map
    awesome->setDefaultOption("scale-factor",0.9);
    ui->btn_Alphabetical->setIcon(awesome->icon( fa::sortalphaasc ));
    ui->pushButton_3->setIcon(awesome->icon( fa::signout ));
    ui->pushButton_4->setIcon(awesome->icon( fa::refresh ));
    ui->pushButton->setIcon(awesome->icon( fa::plus ));
    ui->pushButton_2->setIcon(awesome->icon( fa::minus ));
    ui->context_lock->setIcon(awesome->icon(fa::lock));

    // Set some defaults if they dont exist
    if(!settings.contains("liveSearch")){
        settings.setValue("liveSearch",true);
    }

    // Started. Lets open the todo.txt file, parse it and show it.
    parse_todotxt();
    setFileWatch();
    networkaccessmanager = new QNetworkAccessManager(this);


    // Set up shortcuts . Mac translates the Ctrl -> Cmd
    // http://doc.qt.io/qt-5/qshortcut.html
    auto editshortcut = new QShortcut(QKeySequence(tr("Ctrl+n")),this);
    QObject::connect(editshortcut,SIGNAL(activated()),ui->lineEdit,SLOT(setFocus()));
    auto findshortcut = new QShortcut(QKeySequence(tr("Ctrl+f")),this);
    QObject::connect(findshortcut,SIGNAL(activated()),ui->lineEdit_2,SLOT(setFocus()));
    //auto contextshortcut = new QShortcut(QKeySequence(tr("Ctrl+l")),this);
    //QObject::connect(contextshortcut,SIGNAL(activated()),ui->context_lock,SLOT(setChecked(!(ui->context_lock->isChecked()))));
    QObject::connect(model,SIGNAL(dataChanged (const QModelIndex , const QModelIndex )),this,SLOT(dataInModelChanged(QModelIndex,QModelIndex)));

    // Do this late as it triggers action using data
    ui->btn_Alphabetical->setChecked(settings.value("sort_alpha").toBool());
}

// This method is for making sure we're re-selecting the item that has been edited
void MainWindow::dataInModelChanged(QModelIndex i1,QModelIndex i2){
    Q_UNUSED(i2);
    //qDebug()<<"Data in Model changed emitted:"<<i1.data(Qt::UserRole)<<"::"<<i2.data(Qt::UserRole)<<endl;
    //qDebug()<<"Changed:R="<<i1.row()<<":C="<<i1.column()<<endl;
    saved_selection = i1.data(Qt::UserRole).toString();
    resetTableSelection();
}


MainWindow::~MainWindow()
{
    delete ui;
    delete networkaccessmanager;
}

QSortFilterProxyModel *proxyModel;

QFileSystemWatcher *watcher;


// A simple delay function I pulled of the 'net.. Need to delay reading a file a little bit.. A second seems to be enough
// really don't like this though as I have no idea of knowing when a second is enough.
// Having more than one second will impact usability of the application as it changes the focus.
void delay()
{
    QTime dieTime= QTime::currentTime().addSecs(1);
    while( QTime::currentTime() < dieTime )
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void MainWindow::fileModified(const QString &str){
    Q_UNUSED(str);
    //qDebug()<<"MainWindow::fileModified  "<<watcher->files()<<" --- "<<str;
    //delay();
    saveTableSelection();
    model->refresh();
    resetTableSelection();
    setFileWatch();
}

void MainWindow::clearFileWatch(){
    if(watcher != NULL){
        delete watcher;
        watcher = NULL;
    }
}

void MainWindow::setFileWatch(){
    QSettings settings;
    if(settings.value("autorefresh").toBool()==false)
           return;

    clearFileWatch();

    watcher = new QFileSystemWatcher();
    //qDebug()<<"Mainwindow::setFileWatch :: "<<watcher->files();
    watcher->removePaths(watcher->files()); // Make sure this is empty. Should only be this file we're using in this program, and only one instance
    watcher->addPath(model->getTodoFile());
    QObject::connect(watcher, SIGNAL(fileChanged(QString)), this, SLOT(fileModified(QString)));
}



void MainWindow::parse_todotxt(){

    model = new TodoTableModel(this);
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    ui->tableView->setModel(proxyModel);
    //ui->tableView->resizeColumnsToContents();
    //ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
    ui->tableView->setWordWrap(true);
    ui->tableView->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    ui->tableView->resizeColumnToContents(0); // Checkboxes kept small
    //ui->tableView->resizeRowsToContents(); Om denna körs senare blir det riktigt bra, men inte här..
}

void MainWindow::on_lineEdit_2_textEdited(const QString &arg1)
{
    Q_UNUSED(arg1);
    QSettings settings;

    bool liveUpdate = settings.value("liveSearch").toBool();
    if(liveUpdate){
        updateSearchResults();
    }
}

void MainWindow::updateSearchResults(){
    // Take the text of the format of match1 match2 !match3 and turn it into
    //(?=.*match1)(?=.*match2)(?!.*match3) - all escaped of course
    QStringList words = ui->lineEdit_2->text().split(QRegularExpression("\\s"));
    QString regexpstring="(?=^.*$)"; // Seems a negative lookahead can't be first (!?), so this is a workaround
    for(QString word:words){
        QString start = "(?=^.*";
        if(word.length()>0 && word.at(0)=='!'){
            start = "(?!^.*";
            word = word.remove(0,1);
        }
        if(word.length()==0) break;
        regexpstring += start+QRegExp::escape(word)+".*$)";
    }
    QRegExp regexp(regexpstring,Qt::CaseInsensitive);
    proxyModel->setFilterRegExp(regexp);
    //qDebug()<<"Setting filter: "<<regexp.pattern();
    proxyModel->setFilterKeyColumn(1);
}

void MainWindow::on_lineEdit_2_returnPressed()
{
    QSettings settings;
    bool liveUpdate = settings.value("liveSearch").toBool();

    if(!liveUpdate){
        updateSearchResults();
    }

}

void MainWindow::on_hotkey(){
    auto dlg = new QuickAddDialog();
    dlg->setModal(true);
    dlg->show();
    dlg->exec();
    if(dlg->accepted){
        this->addTodo(dlg->text);
    }
}

void MainWindow::setHotkey(){
    QSettings settings;
    if(settings.value(SETTINGS_HOTKEY_ENABLE).toBool()){
        hotkey->registerHotkey(settings.value(SETTINGS_HOTKEY,DEFAULT_HOTKEY).toString());
        connect(hotkey,&UGlobalHotkeys::activated,[=](size_t id){
            on_hotkey();
        });
    }
}

void MainWindow::on_actionAbout_triggered(){
    AboutBox d;
    d.setModal(true);
    d.show();
    d.exec();
    //myanalytics->check_update();
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog d;
    d.setModal(true);
    d.show();
    d.exec();
    if(d.refresh){
        clearFileWatch();
        delete model;
        model = new TodoTableModel(this);
        proxyModel->setSourceModel(model);
        ui->tableView->setModel(proxyModel);
        ui->tableView->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
        ui->tableView->resizeColumnToContents(0);
        saveTableSelection();
        model->refresh();// Not 100% sure why this is needed.. Should be done by re-setting the model above
        resetTableSelection();
        setFileWatch();
    }
}

void MainWindow::on_pushButton_clicked()
{
    // Adding a new value into the model
    QString txt = ui->lineEdit->text();
   addTodo(txt);
   ui->lineEdit->clear();
}

void MainWindow::addTodo(QString &s){

    if(ui->context_lock->isChecked()){
        // The line should have the context of the search field except any negative search
        QStringList contexts = ui->lineEdit_2->text().split(QRegExp("\\s"));
        for(QString context:contexts){
         if(context.length()>0 && context.at(0)=='!') continue; // ignore this one
         if(!s.contains(context,Qt::CaseInsensitive)){
             s.append(" "+context);
         }
        }
    }
    model->add(s);
}

void MainWindow::on_lineEdit_returnPressed()
{
    on_pushButton_clicked();
}

void MainWindow::on_pushButton_2_clicked()
{
    // Remove the selected item
    QModelIndexList indexes = ui->tableView->selectionModel()->selection().indexes();
    // Only support for deleting one item at a time
    if(!indexes.empty()){
        QModelIndex i=indexes.at(0);
        QString t=model->data(proxyModel->mapToSource(i),Qt::UserRole).toString(); // User Role is Raw data
        //QString t=proxyModel->data(i).toString();
        model->remove(t);
    }

}

void MainWindow::on_pushButton_3_clicked()
{
    // Archive
    saveTableSelection();
    model->archive();
    resetTableSelection();
}

void MainWindow::on_pushButton_4_clicked()
{
    saveTableSelection();
    model->refresh();
    resetTableSelection();
}



void MainWindow::saveTableSelection(){
    //auto index = ui->tableView->selectionModel()->selectedIndexes();
    auto index = ui->tableView->selectionModel()->currentIndex();
//    if(index.count()>0){
    if(index.isValid()){
        // Vi har någonting valt.
        // qDebug()<<"Selected index: "<<index.at(0)<<endl;
        //saved_selection = model->data(index.at(0),Qt::UserRole).toString();
        saved_selection = model->data(index,Qt::UserRole).toString();
        //qDebug()<<"Selected text: "<<saved_selection<<endl;

    }
}

void MainWindow::resetTableSelection(){
    if(saved_selection.size()>0){
        // Set the selection again
        QModelIndexList foundIndexes = model->match(QModelIndex(),Qt::UserRole,saved_selection);
        if(foundIndexes.count()>0){
            //qDebug()<<"Found at index: "<<foundIndexes.at(0)<<endl;
            ui->tableView->setFocus(Qt::OtherFocusReason);
            ui->tableView->selectionModel()->select(foundIndexes.at(0),QItemSelectionModel::Select);
            ui->tableView->selectionModel()->setCurrentIndex(foundIndexes.at(0),QItemSelectionModel::ClearAndSelect);
            //ui->tableView->setCurrentIndex(foundIndexes.at(0));
       }
    }
    saved_selection="";

}

void MainWindow::closeEvent(QCloseEvent *ev){

    QSettings settings;

    settings.setValue( "geometry", saveGeometry() );
    settings.setValue( "savestate", saveState() );
    settings.setValue( "maximized", isMaximized() );
    if ( !isMaximized() ) {
        settings.setValue( "pos", pos() );
        settings.setValue( "size", size() );
    }

    ev->accept();
}

void MainWindow::on_btn_Alphabetical_toggled(bool checked)
{
    QSettings settings;
    settings.setValue("sort_alpha",checked);
    on_pushButton_4_clicked(); // Refresh
}
