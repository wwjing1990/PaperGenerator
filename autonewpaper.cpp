/**
*******************************************************************************
** Copyright (c) 2014
** All rights reserved.
**
** @file autonewpaper.cpp
** @brief
**
** @version 1.0
** @author 胡震宇 <andyhuzhill@gmail.com>
**
** @date 2014/4/22
**
** @revision： 最初版本
*******************************************************************************
*/

#include "autonewpaper.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QMessageBox>
#include <QList>
#include <QDateTime>

#include <QSqlQuery>
#include <QVariant>

#include <QDebug>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>

#include "newtestform.h"

static QStandardItemModel *tableModel = NULL;
static QList<QStandardItemModel *> pointModelList;
static QStandardItemModel *pointsModel = NULL;

AutoNewPaper::AutoNewPaper(QWidget *parent)
    : QWizard(parent)
{
    addPage(new SubjectSetup);
    addPage(new QuestionTypes);
    addPage(new PointSetup);

    setPixmap(QWizard::BackgroundPixmap, QPixmap("/images/background.png"));
    setWindowTitle(tr("自动生成新试卷向导"));

    connect(this, SIGNAL(finished(int)), this, SLOT(onFinished(int)));

}

void AutoNewPaper::generatePaper()
{
    QString subjectName = field("subjectName").toString();

    QList<Question> result;

    qDebug() << tableModel->rowCount();

    for (int row = 0; row < tableModel->rowCount(); ++row) {
        pointsModel = pointModelList.at(row);

        if (pointsModel->rowCount() == 0) {
            continue ;
        }

        QList<Question> eachResult = getQuestionList(subjectName, tableModel->item(row, 0)->text(), tableModel->item(row, 1)->text().toInt());

        if (eachResult.isEmpty()) {
            break;
        }

        result.append(eachResult);
    }

    newTestForm *parentForm = static_cast<newTestForm*> (this->parent());

    parentForm->setQuestionList(result);

    if (!result.isEmpty()) {
        QMessageBox::information(this, tr("通知"), tr("完成!"), QMessageBox::Ok);
    }
}

QList<Question> AutoNewPaper::getQuestionList(QString subjectName, QString questionTypeName, int num)
{
    QList<Question> result;
    int cnt= 0;
    QSqlQuery query;
    int failedCnt = 0;

    query.exec(QString("SELECT numOfQuestions FROM '%1' WHERE questionTypes == '%2'").arg(subjectName).arg(questionTypeName));
    query.next();

    for (int i = 0; i < query.ValuesAsRows; ++i) {
        qDebug() << query.value(i).toString();
    }

    int maxNumOfQuestions = query.value(0).toInt();

    if (num > maxNumOfQuestions) {
        QMessageBox::warning(this, tr("警告"), tr("科目《%1》题型“%2”没有 %3 道题目！").arg(subjectName).arg(questionTypeName).arg(num), QMessageBox::Ok);

        return  result;
    }

    int pointsCnt = getPointsCnt(subjectName, questionTypeName);
    if (num > pointsCnt) {
        QMessageBox::warning(this, tr("警告"), tr("%1 题型只有 %2 种 知识点，题目数量超过了知识点数量，无法保证知识点不重复，组卷失败！").arg(questionTypeName).arg(pointsCnt), QMessageBox::Ok);

        return result;
    }

    do {
        int id = static_cast<int>((static_cast<float> (qrand())/RAND_MAX) *(maxNumOfQuestions-1) + 1);

        QString point = (pointsModel->item(cnt, 1)->text() == tr("任意知识点")) ? "%" : pointsModel->item(cnt, 1)->text();

        QString difficulty = (pointsModel->item(cnt, 2)->text() == tr("任意难度")) ? "%" : pointsModel->item(cnt, 2)->text();

        if (!query.exec(QString("SELECT * FROM '%1_%2' WHERE id = %3 AND Point LIKE '%4' AND Difficulty LIKE '%5'").arg(subjectName).arg(questionTypeName).arg(id).arg(point).arg(difficulty))) {
            qDebug() << tr("搜索数据库失败!");
            failedCnt ++;
            continue ;
        }

        query.next();

        QString questionDocPath = query.value(3).toString();

        if (questionDocPath.isEmpty()) {
            qDebug() << subjectName;
            qDebug() << questionTypeName;
            qDebug() << id;
            qDebug() << point;
            qDebug() << difficulty;
            qDebug() << tr("该试题为空! 已被删除！");
            failedCnt ++;
            continue ;
        }

        QString answerDocPath = query.value(4).toString();
        QString queryPoint = query.value(5).toString();
        QString queryDifficulty = query.value(6).toString();
        QString grade = pointsModel->item(cnt, 3)->text() == tr("根据总分值计算") ? commonDegree[questionTypeName].toString() : pointsModel->item(cnt, 3)->text();


        if (pointsMap.contains(queryPoint)) {
            qDebug() << tr("已经有该知识点的题目了");
            failedCnt ++;
            continue ;
        }

        pointsMap[queryPoint] = true;

        Question question(subjectName, id, questionTypeName, questionDocPath, answerDocPath, queryPoint, queryDifficulty, grade.toInt());
        result.append(question);
        cnt ++;
    } while (cnt < num && failedCnt < 80);

    if (failedCnt >= 80) {
        QMessageBox::warning(this, tr("警告"), tr("自动组卷失败！请重新修改您设定的组卷条件！"), QMessageBox::Ok);
        result.clear();
    }

    return result;
}

void AutoNewPaper::getCommonDegree()
{
    qDebug() << "pointsModleList Length = " << pointModelList.length();
    qDebug() << "tableModel rowCount = " << tableModel->rowCount();

    for (int tabRow = 0; tabRow < tableModel->rowCount(); ++tabRow) {
        int totalDegree = tableModel->item(tabRow, 2)->text().trimmed().toInt();
        int totalQuestCount = tableModel->item(tabRow, 1)->text().trimmed().toInt();

        qDebug() << "totalQuestionCount:" << totalQuestCount;

        if (totalQuestCount == 0) {
            continue;
        }

        pointsModel = pointModelList.at(tabRow);

        int setCount = 0;
        int setDegreeSum = 0;
        for (int pointRow = 0; pointRow < totalQuestCount; ++pointRow) {
            qDebug() << pointRow;
            if (pointsModel->item(pointRow, 3)->text() != tr("根据总分值计算")) {
                setCount ++;
                setDegreeSum += pointsModel->item(pointRow, 3)->text().toInt();
            }
        }

        commonDegree[tableModel->item(tabRow,0)->text()] = (totalDegree - setDegreeSum) / (totalQuestCount - setCount);
    }
}

void AutoNewPaper::onFinished(int iFinishFlag)
{
    if (iFinishFlag) {

        getCommonDegree();

        generatePaper();

    } else {
        QMessageBox::information(this, tr("通知"), tr("自动生成试卷已取消！"), QMessageBox::Ok);
    }
}

int AutoNewPaper::getPointsCnt(QString subjectName, QString questionTypeName)
{
    QStringList Points;
    QSqlQuery query;
    query.exec(QString("SELECT Point FROM '%1_%2'").arg(subjectName).arg(questionTypeName));
    Points.clear();
    while (query.next()) {
        QString point  = query.value(0).toString();
        if (Points.contains(point) || point == "") {
            continue;
        }
        Points.append(point);
    }
    return Points.length();
}

SubjectSetup::SubjectSetup(QWidget *parent)
    :QWizardPage(parent)
{
    this->setTitle(tr("<h1>本向导将引导您设置自动抽卷的约束条件。</h1>"));
    setSubTitle(tr("<h2>请选择您要组卷的科目:</h2>"));

    QLabel *label = new QLabel(tr("题库中现有科目:"));
    QComboBox *subjectBox = new QComboBox(this);

    QSqlQuery query;

    query.exec("SELECT subjectName FROM subjects");

    while (query.next()) {
        subjectBox->addItem(query.value(0).toString());
    }

    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(label);
    layout->addWidget(subjectBox);

    setLayout(layout);

    if (field("subjectName").isNull()) {
        registerField("subjectName", subjectBox, "currentText");
        connect(subjectBox, SIGNAL(currentIndexChanged(QString)), this, SLOT(onSubjectChanged(QString)));
    }
}

void SubjectSetup::onSubjectChanged(QString subjectName)
{
    setField("subjectName", subjectName);
}

QuestionTypes::QuestionTypes(QWidget *parent)
    : QWizardPage(parent)
{
    typeLabel = NULL;
    numOfQuestionSpinBox = NULL;
    numsLabel = NULL;
    questionTypeComboBox = NULL;
    degreeLabel = NULL;
    degreeOfQuestionType = NULL;
    addButton = NULL;
}

void QuestionTypes::initializePage()
{
    if (this->layout()) {
        delete this->layout();
    }

    subjectName = field("subjectName").toString();

    setTitle(tr("<h1>科目:%1</h1>").arg(subjectName));

    setSubTitle(tr("<h2>请设置各个题型的题目数量与总分值：</h2>"));

    QSqlQuery query;

    query.exec(QString("SELECT questionTypes FROM '%1'").arg(subjectName));

    questionTypeList.clear();

    while (query.next()) {
        questionTypeList.append(query.value(0).toString());
    }

    if (questionTypeList.length() == 0) {
        QMessageBox::warning(this, tr("警告"), tr("您选择的科目尚没有建立题型，请先在“试题库管理”中新建题型"), QMessageBox::Ok);
    }

    QVBoxLayout *layout = new QVBoxLayout();

    tableModel = new QStandardItemModel(questionTypeList.length(), 3);

    tableModel->setHorizontalHeaderLabels(QStringList() << tr("题目类型") << tr("题目数量") << tr("题型总分"));

    int row = 0;

    pointModelList.clear();

    foreach (QString type, questionTypeList) {
        tableModel->setItem(row, 0, new QStandardItem(type));
        tableModel->item(row, 0)->setEditable(false);
        tableModel->setItem(row, 1, new QStandardItem("0"));
        tableModel->setItem(row, 2, new QStandardItem("0"));
        row++;
    }

    QTableView *view = new QTableView;

    view->setModel(tableModel);

    connect(view, SIGNAL(clicked(QModelIndex)), this, SLOT(onCellClicked(QModelIndex)));

    if (typeLabel == NULL) {
        typeLabel = new QLabel(tr("题目类型:"));
    }

    if (questionTypeComboBox == NULL) {
        questionTypeComboBox = new QComboBox;
    } else {
        questionTypeComboBox->clear();
    }

    foreach(QString type, questionTypeList){
        questionTypeComboBox->addItem(type);
    }

    connect(questionTypeComboBox, SIGNAL(currentIndexChanged(QString)), this, SLOT(onQuestionTypeChanged(QString)));

    query.exec(tr("SELECT numOfQuestions FROM '%1' WHERE questionTypes == '%2'").arg(subjectName).arg(questionTypeComboBox->currentText()));

    query.next();

    if (numsLabel == NULL) {
        numsLabel = new QLabel(tr("题目数量:"));
    }

    if (numOfQuestionSpinBox == NULL) {
        numOfQuestionSpinBox = new QSpinBox;
    }

    numOfQuestionSpinBox->setMinimum(0);
    numOfQuestionSpinBox->setMaximum(query.value(0).toInt());

    if (degreeLabel == NULL) {
        degreeLabel = new QLabel(tr("题型总分:"));
    }

    if (degreeOfQuestionType == NULL) {
        degreeOfQuestionType = new QSpinBox;
    }

    degreeOfQuestionType->setMinimum(0);

    if (addButton == NULL) {
        addButton = new QPushButton(tr("修改数量与分数"));
    }
    connect(addButton, SIGNAL(clicked()), this, SLOT(onAddClicked()));

    QHBoxLayout *hLayout = new QHBoxLayout();

    hLayout->addWidget(typeLabel);
    hLayout->addWidget(questionTypeComboBox);
    hLayout->addWidget(numsLabel);
    hLayout->addWidget(numOfQuestionSpinBox);
    hLayout->addWidget(degreeLabel);
    hLayout->addWidget(degreeOfQuestionType);
    hLayout->addWidget(addButton);

    layout->addWidget(view);

    layout->addLayout(hLayout);

    setLayout(layout);
}

void QuestionTypes::onCellClicked(QModelIndex index)
{
    int row =  index.row();
    questionTypeComboBox->setCurrentIndex(row);
    numOfQuestionSpinBox->setValue(tableModel->item(row, 1)->text().toInt());
    degreeOfQuestionType->setValue(tableModel->item(row, 2)->text().toInt());
}

void QuestionTypes::onQuestionTypeChanged(QString questionType)
{
    QSqlQuery query;

    query.exec(tr("SELECT numOfQuestions FROM '%1' WHERE questionTypes == '%2'").arg(subjectName).arg(questionType));
    query.next();
    numOfQuestionSpinBox->setMaximum(query.value(0).toInt());

    int row = questionTypeComboBox->currentIndex();

    if (row != -1) {
        numOfQuestionSpinBox->setValue(tableModel->item(row, 1)->text().toInt());
        degreeOfQuestionType->setValue(tableModel->item(row, 2)->text().toInt());
    }
}

void QuestionTypes::onAddClicked()
{
    QString typeName = questionTypeComboBox->currentText();
    int numOfType = numOfQuestionSpinBox->value();
    int degree = degreeOfQuestionType->value();

    for (int row = 0; row < tableModel->rowCount(); ++row) {
        if (tableModel->item(row, 0)->text() == typeName) {
            tableModel->setItem(row, 1, new QStandardItem(QString("%1").arg(numOfType)));
            tableModel->setItem(row, 2, new QStandardItem(QString("%1").arg(degree)));
        }
    }
}

PointSetup::PointSetup(QWidget *parent)
    : QWizardPage(parent)
{
    typeCount = 0;
    questionNumCount = 0;
    questionTypeList.clear();
    questionTypeComboBox  = NULL;
    numComboBox = NULL;
    pointComboBox = NULL;
    diffComboBox = NULL;
    setButton = NULL;
    pointsModel = NULL;
    pointsView = NULL;
    degreeSpinBox = NULL;

    numLabel = NULL;
    degreeLabel = NULL;
    pointLabel = NULL;
    diffLabel = NULL;
}

void PointSetup::initializePage()
{
    if (this->layout()) {
        delete this->layout();
    }

    subjectName = field("subjectName").toString();

    setTitle(tr("<h1>科目:%1 </h1>").arg(subjectName));

    setSubTitle(tr("<h2>请设置各个题目的知识点和分数</h2>"));

    if (questionTypeComboBox == NULL) {
        questionTypeComboBox = new QComboBox;
    } else {
        delete questionTypeComboBox;
        questionTypeComboBox = new QComboBox;
    }

    QSqlQuery query;

    query.exec(tr("SELECT questionTypes FROM '%1'").arg(subjectName));

    while(query.next()) {
        questionTypeComboBox->addItem(query.value(0).toString());
    }

    if (questionTypeComboBox->count() == 0) {
        return ;
    }

    questionTypeComboBox->setCurrentIndex(0);

    connect(questionTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onQuestionTypeChanged(int)));

    questionType = questionTypeComboBox->currentText();

    for (int row = 0; row < tableModel->rowCount(); row++) {
       appPointModelList(row);
    }

    if (pointsView == NULL) {
        pointsView = new QTableView;
    }

    pointsModel = pointModelList.at(0);

    pointsView->setModel(pointsModel);

    connect(pointsView, SIGNAL(clicked(QModelIndex)), this, SLOT(onPointsViewClicked(QModelIndex)));

    if (numLabel == NULL) {
        numLabel = new QLabel(tr("题号:"));
    }

    if (numComboBox == NULL) {
        numComboBox = new QComboBox;
    } else {
        delete numComboBox;
        numComboBox = new QComboBox;
    }

    for (int row = 0; row < tableModel->item(0,1)->text().toInt(); ++row) {
        numComboBox->addItem(QString("%1").arg(row+1));
    }

    if (pointLabel == NULL) {
        pointLabel = new QLabel(tr("知识点"));
    }

    if (pointComboBox == NULL) {
        pointComboBox = new QComboBox;
    } else {
        pointComboBox->clear();
    }

    if (diffLabel == NULL) {
        diffLabel = new QLabel(tr("难度"));
    }

    if (diffComboBox == NULL) {
        diffComboBox = new QComboBox;
        QStringList diffList;
        diffList  << tr("任意难度") << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9" << "10";
        diffComboBox->addItems(diffList);
    }

    QStringList points = getPoints(subjectName, questionType);

    pointComboBox->addItem(tr("任意知识点"));
    foreach (QString point, points) {
        pointComboBox->addItem(point);
    }

    if (degreeLabel == NULL)
        degreeLabel = new QLabel(tr("分数"));

    if (degreeSpinBox == NULL) {
        degreeSpinBox = new QSpinBox;
    } else {
        degreeSpinBox->setValue(0);
    }

    degreeSpinBox->setMinimum(0);

    if (setButton == NULL) {
        setButton = new QPushButton(tr("设置该题"));
    }

    connect(setButton, SIGNAL(clicked()), this, SLOT(onSetButtonClicked()));

    QHBoxLayout *hLayout = new QHBoxLayout;

    hLayout->addWidget(numLabel);
    hLayout->addWidget(numComboBox);
    hLayout->addWidget(pointLabel);
    hLayout->addWidget(pointComboBox);
    hLayout->addWidget(diffLabel);
    hLayout->addWidget(diffComboBox);
    hLayout->addWidget(degreeLabel);
    hLayout->addWidget(degreeSpinBox);
    hLayout->addWidget(setButton);

    QVBoxLayout *layout = new QVBoxLayout;

    QLabel *questionTypeLabel = new QLabel(tr("题目类型:"));
    QHBoxLayout *typeLayout = new QHBoxLayout;
    typeLayout->addWidget(questionTypeLabel);
    typeLayout->addWidget(questionTypeComboBox);

    layout->addLayout(typeLayout);

    layout->addWidget(pointsView);

    layout->addLayout(hLayout);

    setLayout(layout);
}

QStringList PointSetup::getPoints(QString subjectName, QString questionTypeName)
{
    QStringList Points;
    QSqlQuery query;
    query.exec(QString("SELECT Point FROM '%1_%2'").arg(subjectName).arg(questionTypeName));
    Points.clear();
    while (query.next()) {
        QString point = query.value(0).toString();
        if (Points.contains(point) || point == "") {
            continue;
        }
        Points.append(point);
    }
    return Points;
}

void PointSetup::onPointsViewClicked(QModelIndex index)
{
    int row = index.row();

    numComboBox->setCurrentIndex(row);

    for (int i = 0; i < pointComboBox->count(); ++i) {
        if (pointComboBox->itemText(i) == pointsModel->item(row, 1)->text()) {
            pointComboBox->setCurrentIndex(i);
            break;
        }

        if (i == pointComboBox->count() -1) {
            pointComboBox->setCurrentIndex(0);
        }
    }

    for (int i = 0; i < diffComboBox->count(); ++i) {
        if (diffComboBox->itemText(i) == pointsModel->item(row, 2)->text()) {
            diffComboBox->setCurrentIndex(i);
            break;
        }

        if (i == diffComboBox->count() - 1 ) {
            diffComboBox->setCurrentIndex(0);
        }
    }

    if (pointsModel->item(row, 3)->text().toInt()) {
        degreeSpinBox->setValue(pointsModel->item(row, 3)->text().toInt());
    }
}

void PointSetup::onQuestionTypeChanged(int index)
{
    pointsModel = pointModelList.at(index);

    pointsModel->setHorizontalHeaderLabels(QStringList() << tr("题号") << tr("知识点") << tr("分数"));

    if (pointsModel->rowCount() != tableModel->item(index, 1)->text().toInt()) {
        pointsModel->clear();
        for (int row = 0; row < tableModel->item(index ,1)->text().toInt(); ++row) {
            pointsModel->setItem(row, 0, new QStandardItem(QString("%1").arg(row+1)));
            pointsModel->setItem(row, 1, new QStandardItem(tr("任意知识点")));
            pointsModel->setItem(row, 2, new QStandardItem(tr("任意难度")));
            pointsModel->setItem(row, 3, new QStandardItem(tr("根据总分值计算")));
        }
    }

    pointsView->setModel(pointsModel);

    connect(pointsView, SIGNAL(clicked(QModelIndex)), this, SLOT(onPointsViewClicked(QModelIndex)));

    numComboBox->clear();

    for (int i = 0; i < tableModel->item(index, 1)->text().toInt(); ++i) {
        numComboBox->addItem(QString("%1").arg(i+1));
    }
}

void PointSetup::onSetButtonClicked()
{
    int row = numComboBox->currentText().toInt() - 1;

    QString point = pointComboBox->currentText();

    pointsModel->setItem(row, 1, new QStandardItem(point));

    QString diff = diffComboBox->currentText();

    pointsModel->setItem(row, 2, new QStandardItem(diff));

    int degree = degreeSpinBox->value();

    if (degree != 0) {
        pointsModel->setItem(row, 3, new QStandardItem(QString("%1").arg(degree)));
    }
}

void PointSetup::appPointModelList(int index)
{
    QStandardItemModel *pointModel = new QStandardItemModel;
    pointModel->setHorizontalHeaderLabels(QStringList() << tr("题号") << tr("知识点") << tr("分数"));


    pointModel->clear();
    for (int row = 0; row < tableModel->item(index ,1)->text().toInt(); ++row) {
        pointModel->setItem(row, 0, new QStandardItem(QString("%1").arg(row+1)));
        pointModel->setItem(row, 1, new QStandardItem(tr("任意知识点")));
        pointModel->setItem(row, 2, new QStandardItem(tr("任意难度")));
        pointModel->setItem(row, 3, new QStandardItem(tr("根据总分值计算")));
    }

    pointModelList.append(pointModel);

}
