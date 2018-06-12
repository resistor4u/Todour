#include "todotxt.h"

// Todo.txt file format: https://github.com/ginatrapani/todo.txt-cli/wiki/The-Todo.txt-Format

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDate>
#include <set>
#include <QSettings>
#include <QRegularExpression>
#include <QDebug>
#include "def.h"

todotxt::todotxt()
{
}

void todotxt::setdirectory(QString &dir){
    filedirectory=dir;
}

void todotxt::parse(){
    //qDebug()<<"todotxt::parse";
    // parse the files todo.txt and done.txt (for now only todo.txt)
    todo.clear();
    QString todofile=getTodoFile();
    QFile file(todofile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        QString line = in.readLine();
        todo.push_back(line);
     }

}

QString todotxt::getTodoFile(){
    QSettings settings;
    QString dir = settings.value("directory").toString();
    QString todofile = dir.append(TODOFILE);
    return todofile;
}

void todotxt::getActive(QString& filter,vector<QString> &output){
        // Obsolete... remove?
    Q_UNUSED(filter);
        for(vector<QString>::iterator iter=todo.begin();iter!=todo.end();iter++){
                if( (*iter).length()==0 || (*iter).at(0) == 'x')
                        continue;
                output.push_back( (*iter));
        }
}


bool todotxt::isInactive(QString &text){
    QSettings settings;
    QString t=settings.value("inactive").toString();
    if(t.isEmpty())
        return false;
    QStringList inactives = t.split(";");
    for(int i=0;i<inactives.count();i++){
        if(text.contains(inactives[i])){
            return true;
        }
    }
    return false;
}

/* Comparator function.. We need to remove all the junk in the beginning of the line */
bool todotxt::lessThan(QString &s1,QString &s2){
    return prettyPrint(s1).toLower() < prettyPrint(s2).toLower();
}

QRegularExpression threshold("t:(\\d\\d\\d\\d-\\d\\d-\\d\\d)");
void todotxt::getAll(QString& filter,vector<QString> &output){
        // Vectors are probably not the best here...
    Q_UNUSED(filter);
        set<QString> prio;
        vector<QString> open;
        vector<QString> done;
        vector<QString> inactive;
        QSettings settings;
        QString t=settings.value("inactive").toString();
        QStringList inactives = t.split(";");
        if(!t.contains(";")){
            // There is really nothing here but inactives will still have one item. Lets just remove it
            inactives.clear();
        }

        bool separateinactives = settings.value("separateinactive").toBool();

        for(vector<QString>::iterator iter=todo.begin();iter!=todo.end();iter++){
            QString line = (*iter); // For debugging
            if(line.isEmpty())
                continue;

            // Begin by checking for inactive, as there are two different ways of sorting those
            bool inact=false;
            for(int i=0;i<inactives.count();i++){
                if((*iter).contains(inactives[i])){
                    inact=true;
                    break;

                }
            }

            // If we are respecting thresholds, we should check for that
            if(settings.value(SETTINGS_THRESHOLD).toBool()){
                QRegularExpressionMatch m=threshold.match((QString)(*iter));
                if(m.hasMatch()){
                    QString today = getToday();
                    QString t = m.captured(1);
                    if(t.compare(today)>0){
                        continue; // Don't show this one since it's in the future
                    }

                }
            }

            if(!(inact&&separateinactives) && (*iter).at(0) == '(' && (*iter).at(2) == ')'){
                prio.insert((*iter));
            } else if ( (*iter).at(0) == 'x'){
                done.push_back((*iter));
            } else if(inact){
                inactive.push_back((*iter));
            }
            else {
                open.push_back((*iter));
            }
        }

        // Sort the open and done sections alphabetically if needed

        if(settings.value("sort_alpha").toBool()){
            qSort(open.begin(),open.end(),lessThan);
            qSort(inactive.begin(),inactive.end(),lessThan);
            qSort(done.begin(),done.end(),lessThan);
            /*qSort(open);
            qSort(inactive);
            qSort(done);*/
        }

        for(set<QString>::iterator iter=prio.begin();iter!=prio.end();iter++)
            output.push_back((*iter));
        for(vector<QString>::iterator iter=open.begin();iter!=open.end();iter++)
            output.push_back((*iter));
        for(vector<QString>::iterator iter=inactive.begin();iter!=inactive.end();iter++)
            output.push_back((*iter));
        for(vector<QString>::iterator iter=done.begin();iter!=done.end();iter++)
            output.push_back((*iter));
}

Qt::CheckState todotxt::getState(QString& row){
    if(row.length()>1 && row.at(0)=='x' && row.at(1)==' '){
        return Qt::Checked;
    } else {
        return Qt::Unchecked;
    }
}

QString todotxt::getToday(){
    QDate d = QDate::currentDate();
    return d.toString("yyyy-MM-dd");
}

QString todotxt::prettyPrint(QString& row){
    QString ret;
    QSettings settings;

    // Remove dates
    todoline tl;
    String2Todo(row,tl);

    ret = tl.priority;
    if(settings.value(SETTINGS_SHOW_DATES).toBool()){
        ret.append(tl.closedDate+tl.createdDate);
    }

    ret.append(tl.text);

    // Remove all leading and trailing spaces
    return ret.trimmed();
}

void todotxt::slurp(QString& filename,vector<QString>& content){
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        QString line = in.readLine();
        content.push_back(line);
     }
}

void todotxt::write(QString& filename,vector<QString>&  content){
    //qDebug()<<"todotxt::write("<<filename<<")";
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
         return;

        QTextStream out(&file);
        out.setCodec("UTF-8");
        for(unsigned int i = 0;	i<content.size(); i++)
            out << content.at(i) << "\n";
}

void todotxt::remove(QString line){
    // Remove the line, but perhaps saving it for later as well..
    QSettings settings;
    if(settings.value(SETTINGS_DELETED_FILE).toBool()){
        QString dir = settings.value("directory").toString();
        QString deletedfile = dir.append(DELETEDFILE);
        vector<QString> deleteddata;
        slurp(deletedfile,deleteddata);
        deleteddata.push_back(line);
        write(deletedfile,deleteddata);
    }
    QString tmp;
    update(line,false,tmp);
}


void todotxt::archive(){
    // Slurp the files
    QSettings settings;
    QString dir = settings.value("directory").toString();
    QString todofile = dir.append(TODOFILE);
    dir = settings.value("directory").toString();
    QString donefile = dir.append(DONEFILE);
    vector<QString> tododata;
    vector<QString> donedata;
    slurp(todofile,tododata);
    slurp(donefile,donedata);
    for(vector<QString>::iterator iter=tododata.begin();iter!=tododata.end();){
        if((*iter).length()>0 && (*iter).at(0)=='x'){
            donedata.push_back((*iter));
            iter=tododata.erase(iter);
        } else {
            // No change
            iter++;
        }

    }
    write(todofile,tododata);
    write(donefile,donedata);
    parse();
}

void todotxt::refresh(){
    parse();
}

void todotxt::update(QString &row, bool checked, QString &newrow){
    // First slurp the file.
    QSettings settings;
    QString dir = settings.value("directory").toString();
    QString todofile = dir.append(TODOFILE);
    vector<QString> data;
    slurp(todofile,data);

    if(row.isEmpty()){
        todoline tl;
        String2Todo(newrow,tl);
        // Add a date to the line if where doing dates
        if(settings.value("dates").toBool()){
            QString today = getToday()+" ";
            tl.createdDate = today;
        }

        // Just add the line
        data.push_back(Todo2String(tl));

    } else {
        for(vector<QString>::iterator iter=data.begin();iter!=data.end();iter++){
            QString *r = &(*iter);
            if(!r->compare(row)){
                // Here it is.. Lets modify if we shouldn't remove it alltogether
                if(newrow.isEmpty()){
                    // Remove it
                    iter=data.erase(iter);
                    break;
                }

                if(checked && !r->startsWith("x ")){
                    todoline tl;
                    String2Todo(*r,tl);
                    tl.checked=true;

                    QString date;
                    if(settings.value("dates").toBool()){
                            date.append(getToday()+" "); // Add a date if needed
                    }
                    tl.closedDate=date;
                    *r=Todo2String(tl);

                }
                else if(!checked && r->startsWith("x ")){
                    todoline tl;
                    String2Todo(*r,tl);
                    tl.checked=false;
                    tl.closedDate="";
                    *r=Todo2String(tl);
                } else {
                    todoline tl;
                    String2Todo(row,tl);
                    todoline newtl;
                    String2Todo(newrow,newtl);
                    tl.priority=newtl.priority;
                    tl.text=newtl.text;
                    *r = Todo2String(tl);
                }
                break;
            }
        }
    }

    write(todofile,data);
    parse();
}

// A todo.txt line looks like this
QRegularExpression todo_line("(x\\s+)?(\\([A-Z]\\)\\s+)?(\\d\\d\\d\\d-\\d\\d-\\d\\d\\s+)?(\\d\\d\\d\\d-\\d\\d-\\d\\d\\s+)?(.*)");

void todotxt::String2Todo(QString &line,todoline &t){
    QRegularExpressionMatch match = todo_line.match(line);
    if(match.hasMatch() && match.lastCapturedIndex()==5){

        if(match.captured(1).isEmpty()){
            t.checked=false;
        } else {
            t.checked=true;
        }

        t.priority=match.captured(2);
        if(t.checked){
            t.closedDate=match.captured(3);
            t.createdDate=match.captured(4);
        } else {
            t.createdDate=match.captured(3); // No closed date on a line that isn't closed.
        }
        t.text = match.captured(5);


    } else {
        t.checked=false;
        t.priority="";
        t.closedDate="";
        t.createdDate="";
        t.text="";
    }

}

QString todotxt::Todo2String(todoline &t){
    QString ret;

    // Yep, an ugly side effect, but it make sure we're having the right format all the time
    if(t.checked && t.createdDate.isEmpty()){
        t.createdDate = t.closedDate;
    }

    if(t.checked){
        ret.append("x ");
    }
    ret.append(t.priority);
    ret.append(t.closedDate);
    ret.append(t.createdDate);
    ret.append(t.text);
    return ret;
}
