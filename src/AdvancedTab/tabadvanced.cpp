#include "protocaldataitem.h"
#include "protocalheaderitem.h"
#include "tabadvanced.h"
#include "textinput.h"
#include "texttranslator.h"
#include "vartype.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QStandardItemModel>

#include <sstream>
#include <iostream>

TabAdvanced::TabAdvanced(QWidget *parent) : QWidget(parent),
    buttonEnable(new QPushButton("Enable")),
    buttonLoad(new QPushButton("Load Settings")),
    buttonSave(new QPushButton("Save Settings")),
    labelType(new QLabel("Var Type")),
    comboType(new QComboBox()),
    treeProtocals(new QTreeView()),
    groupEndianess(new QGroupBox("Endianess")),
    radioLittle(new QRadioButton("Little")),
    radioBig(new QRadioButton("Big")),
    listProtocals(new QList<Protocal *>()),
    mutexAdd(new QMutex()),
    buttonAddHeader(new QPushButton("Add Header")),
    buttonAddData(new QPushButton("Add Data")),
    buttonDelete(new QPushButton("Delete")),
    buttonUp(new QPushButton("Up")),
    buttonDown(new QPushButton("Down")),
    boxLog(new QPlainTextEdit()),
    buttonClearLog(new QPushButton("Clear")),
    graph(new DataVisualizationGraph()),
    enabled(false),
    endianess(BIG),
    nameAllocator(new NameAllocator()),
    validatorHeader(new QRegExpValidator(QRegExp("[0-9a-fA-F]{1,4}")))
{
    QVBoxLayout *layoutMain = new QVBoxLayout();
    setLayout(layoutMain);

    QHBoxLayout *layoutListLog = new QHBoxLayout();
    layoutMain->addLayout(layoutListLog);

    QHBoxLayout *layoutList = new QHBoxLayout();
    QVBoxLayout *layoutLog = new QVBoxLayout();

    layoutListLog->addLayout(layoutList);
    layoutListLog->addLayout(layoutLog);
    layoutMain->addWidget(graph);
    connect(graph, &DataVisualizationGraph::onGraphUpdate, this, &TabAdvanced::onGraphUpdate);

    comboType->addItem("U8", VAR_TYPE::U8);
    comboType->addItem("I8", VAR_TYPE::I8);
    comboType->addItem("U16", VAR_TYPE::U16);
    comboType->addItem("I16", VAR_TYPE::I16);
    comboType->addItem("U32", VAR_TYPE::U32);
    comboType->addItem("I32", VAR_TYPE::I32);
    comboType->addItem("float", VAR_TYPE::Float);
    comboType->addItem("double", VAR_TYPE::Double);

    layoutList->addWidget(treeProtocals);
    layoutList->setStretchFactor(treeProtocals, 4);
    QStandardItemModel *model = new QStandardItemModel();
    treeProtocals->setModel(model);
    connect(model, &QStandardItemModel::itemChanged, this, &TabAdvanced::ontreeProtocalItemChanged);

    model->setHorizontalHeaderLabels(QStringList() << QStringLiteral("Data") << QStringLiteral("Name"));
    QVBoxLayout *layoutListControls = new QVBoxLayout();
    layoutList->addLayout(layoutListControls);
    layoutListControls->addWidget(buttonEnable);
    layoutListControls->addWidget(buttonLoad);
    layoutListControls->addWidget(buttonSave);

    connect(buttonLoad, &QPushButton::clicked, this, &TabAdvanced::onButtonLoadSettingsClicked);
    connect(buttonSave, &QPushButton::clicked, this, &TabAdvanced::onButtonSaveSettingsClicked);

    layoutListControls->addWidget(groupEndianess);
    QVBoxLayout *layoutEndianess = new QVBoxLayout();
    groupEndianess->setLayout(layoutEndianess);

    layoutListControls->addWidget(labelType);
    layoutListControls->addWidget(comboType);
    layoutEndianess->addWidget(radioBig);
    layoutEndianess->addWidget(radioLittle);
    radioBig->setChecked(true);
    connect(radioLittle, &QRadioButton::clicked, this, &TabAdvanced::onRadioLittleBigClicked);
    connect(radioBig, &QRadioButton::clicked, this, &TabAdvanced::onRadioLittleBigClicked);
    layoutListControls->addWidget(buttonAddHeader);
    layoutListControls->addWidget(buttonAddData);
    layoutListControls->addWidget(buttonDelete);
    layoutListControls->addWidget(buttonUp);
    layoutListControls->addWidget(buttonDown);
    layoutListControls->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    connect(buttonAddHeader, &QPushButton::clicked, this, &TabAdvanced::onButtonAddHeaderClicked);
    connect(buttonAddData, &QPushButton::clicked, this, &TabAdvanced::onButtonAddDataClicked);
    connect(buttonDelete, &QPushButton::clicked, this, &TabAdvanced::onButtonDeleteClicked);
    connect(buttonUp, &QPushButton::clicked, this, &TabAdvanced::onButtonUpClicked);
    connect(buttonDown, &QPushButton::clicked, this, &TabAdvanced::onButtonDownClicked);
    connect(buttonEnable, &QPushButton::clicked, this, &TabAdvanced::onButtonEnableClicked);

    layoutList->setStretchFactor(layoutListControls, 1);

    layoutLog->addWidget(boxLog);
    QHBoxLayout *layoutLogControls = new QHBoxLayout();
    layoutLog->addLayout(layoutLogControls);
    layoutLogControls->addWidget(buttonClearLog);
    connect(buttonClearLog, &QPushButton::clicked, this, &TabAdvanced::onButtonClearLogClicked);
}

TabAdvanced::~TabAdvanced()
{
    delete nameAllocator;
    delete validatorHeader;
    qDeleteAll(*listProtocals);
}

void TabAdvanced::frameDataReady(int id, QByteArray array)
{
    QString bufferShow = QString().sprintf("[%d] \t", id);
    if (enabled) {
        Protocal *protocal = listProtocals->at(id);
        QList<VarType *> *listData = protocal->getListData();
        int count = 0;
        for (int i = 0; i < listData->count(); i++) {
            VarType *type = listData->at(i);
            type->setBufferValue(array.mid(count, type->getSize()));
            count += type->getSize();
            double value = type->getDouble(endianess);
            bufferShow.append(QString().sprintf("%.2lf ", value));
            //Graph data send.
            QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
            ProtocalDataItem *item = static_cast<ProtocalDataItem *>(model->item(id)->child(i));
            if (item->checkState() == Qt::CheckState::Checked) {
                item->setCurrentValue(value);
            }
        }
        if (bufferShow.count() != 0)
            boxLog->appendPlainText(bufferShow);
    }
}

bool TabAdvanced::checkIfHeaderExists(QByteArray header)
{
    QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
    for (int i = 0; i < listProtocals->count(); i++) {
        ProtocalHeaderItem *item = static_cast<ProtocalHeaderItem *>(model->item(i));
        if (item->getHeader().compare(header) == 0)
            return true;
    }
    return false;
}

bool TabAdvanced::checkIfNameExists(QString name)
{
    QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
    for (int i = 0; i < listProtocals->count(); i++) {
        ProtocalHeaderItem *headerItem = static_cast<ProtocalHeaderItem *>(model->item(i));
        for (int j = 0; j < headerItem->rowCount(); j++) {
            ProtocalDataItem *dataItem = static_cast<ProtocalDataItem *>(headerItem->child(j));
            if (dataItem->getName().compare(name) == 0)
                return true;
        }
    }
    return false;
}

void TabAdvanced::setAllowRunning(bool value)
{
    allowRunning = value;
    graph->setAllowRunning(allowRunning);
}

void TabAdvanced::onButtonAddHeaderClicked()
{
    TextInput *input = new TextInput(this, "AAAA", "[0-9a-fA-F]+");
    input->show();
    if (input->exec() == QDialog::Accepted) {
        TextTranslator translator = TextTranslator(input->getText());
        QByteArray header = translator.toHex();
        if (checkIfHeaderExists(header)) {
            QMessageBox::warning(this, "Error", "Header already exists.");
        } else {
            QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
            QList<QStandardItem *> list;
            list.append(new ProtocalHeaderItem(QByteArray(header)));
            list.append(new QStandardItem());

            model->appendRow(list);
            QItemSelectionModel *selectionModel = treeProtocals->selectionModel();
            int count = model->rowCount();
            QModelIndex indexLast = model->index(count - 1, 0, QModelIndex());
            selectionModel->select(indexLast,
                   QItemSelectionModel::SelectionFlag::ClearAndSelect |
                   QItemSelectionModel::SelectionFlag::Rows);
            selectionModel->setCurrentIndex(indexLast,
                    QItemSelectionModel::SelectionFlag::Current);
        }
    }
}

void TabAdvanced::onButtonAddDataClicked()
{
    QItemSelectionModel *selectionModel = treeProtocals->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    if (index.isValid()) {
        QString allocatedName = nameAllocator->allocateName();
        TextInput *input = new TextInput(this, allocatedName);
        input->show();
        if (input->exec() == QDialog::Accepted) {
            QString name = input->getText();
            if (name.compare(allocatedName) != 0) {
                nameAllocator->freeName(allocatedName);
            }
            if (checkIfNameExists(name)) {
                QMessageBox::warning(this, "Error", "Name already exists.");
                return;
            }
            QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
            QList<QStandardItem *> list;
            VAR_TYPE type = static_cast<VAR_TYPE>(comboType->currentData().toInt());
            list.append(new ProtocalDataItem(type, comboType->currentText()));
            list.append(new QStandardItem(name));
            QModelIndex indexLast;
            if (index.parent().isValid()) {
                model->itemFromIndex(index)->parent()->appendRow(list);
                //Calculate the index of the last element.
                int count = model->itemFromIndex(index)->parent()->rowCount();
                indexLast = index.siblingAtRow(count - 1);
            }
            else {
                model->itemFromIndex(index.siblingAtColumn(0))->appendRow(list);
                int count = model->itemFromIndex(index)->rowCount();
                indexLast = model->index(count - 1, 0, index);
            }
            selectionModel->select(indexLast,
                   QItemSelectionModel::SelectionFlag::ClearAndSelect |
                   QItemSelectionModel::SelectionFlag::Rows);
            selectionModel->setCurrentIndex(indexLast,
                    QItemSelectionModel::SelectionFlag::Current);
            updateDecodeParameters();
        } else
            nameAllocator->freeName(allocatedName);
    }
}

void TabAdvanced::onButtonDeleteClicked()
{
    QModelIndex index = treeProtocals->selectionModel()->currentIndex();
    if (index.isValid()) {
        QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
        if (index.parent().isValid()) {
            QStandardItem *item = (model->itemFromIndex(index.siblingAtColumn(1)));
            nameAllocator->freeName(item->text());
            model->removeRow(index.row(), index.parent());
        }
        else
            model->removeRow(index.row());
        updateDecodeParameters();
    }
}

void TabAdvanced::onButtonClearLogClicked()
{
    boxLog->clear();
}

void TabAdvanced::ontreeProtocalItemChanged(QStandardItem *item)
{
    //Assume only checkbox will change now.
    ProtocalDataItem *dataItem = static_cast<ProtocalDataItem *>(item);
    if (item->checkState() == Qt::CheckState::Checked) {
        QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
        QString name = model->itemFromIndex(dataItem->index().siblingAtColumn(1))->text();
        dataItem->setSeries(graph->createSeries(name));
    } else {
        graph->removeSeries(dataItem->getSeries());
        dataItem->clearSeries();
    }
}

void TabAdvanced::onGraphUpdate(int currentX)
{
    QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
    for (int i = 0; i < model->rowCount(); i++) {
        ProtocalHeaderItem *headerItem = static_cast<ProtocalHeaderItem *>(model->item(i));
        for (int j = 0; j < headerItem->rowCount(); j++) {
            ProtocalDataItem *dataItem = static_cast<ProtocalDataItem *>(headerItem->child(j));
            dataItem->onGraphUpdate(currentX);
        }
    }
}

void TabAdvanced::onButtonUpClicked()
{
    QItemSelectionModel *selectionModel = treeProtocals->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    if (index.isValid()) {
        if (index.siblingAtRow(0) != index) {
            QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
            if (!index.parent().isValid()) {
                model->insertRow(index.row() - 1, model->takeRow(index.row()));
            } else {
                QStandardItem *item = model->itemFromIndex(index);
                item->parent()->insertRow(index.row() - 1, item->parent()->takeRow(index.row()));
            }
            selectionModel->select(index.siblingAtRow(index.row() - 1),
                                   QItemSelectionModel::SelectionFlag::ClearAndSelect |
                                   QItemSelectionModel::SelectionFlag::Rows);
                    selectionModel->setCurrentIndex(index.siblingAtRow(index.row() - 1),
                                        QItemSelectionModel::SelectionFlag::Current);
        }
    }
    updateDecodeParameters();
}

void TabAdvanced::onButtonDownClicked()
{
    QItemSelectionModel *selectionModel = treeProtocals->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    int oldRow = index.row();
    if (index.isValid()) {
        QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
        if (!index.parent().isValid()) {	//If it's the last one.
            if (index.row() == model->rowCount() - 1)
                return;
            model->insertRow(index.row() + 1, model->takeRow(index.row()));
        } else {
            if (index.row() == model->itemFromIndex(index)->parent()->rowCount() - 1)
                return;

            QStandardItem *item = model->itemFromIndex(index);
            item->parent()->insertRow(index.row() + 1, item->parent()->takeRow(index.row()));
        }
        selectionModel->select(index.siblingAtRow(oldRow + 1),
                               QItemSelectionModel::SelectionFlag::ClearAndSelect |
                               QItemSelectionModel::SelectionFlag::Rows);
        selectionModel->setCurrentIndex(index.siblingAtRow(oldRow + 1),
                                        QItemSelectionModel::SelectionFlag::Current);
    }
    updateDecodeParameters();
}

void TabAdvanced::onButtonEnableClicked()
{
    if (enabled == false) {
        buttonEnable->setText("Disable");
        enabled = true;
        updateDecodeParameters();
    } else {
        buttonEnable->setText("Enable");
        enabled = false;
    }
}

void TabAdvanced::onButtonLoadSettingsClicked()
{
}

void TabAdvanced::onButtonSaveSettingsClicked()
{
    QString folderString = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/QSerial Socket Amigo";
    QFileInfo folder(folderString);
    if (!folder.exists())
        QDir().mkdir(folderString);

    QString fileName = QFileDialog::getSaveFileName(this,
                tr("Save Protocal Settings"),
                folderString,
                tr("JSON (*.json)"));
    if (fileName.isEmpty()) {
        return;
    } else {
        if (fileName.right(5).toLower().compare(".json") != 0)
            fileName.append(".json");

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "Error", "File open failed.");
            return;
        }

        QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
        QJsonObject object = QJsonObject();
        for (int i = 0; i < model->rowCount(); i++) {
            ProtocalHeaderItem *header = static_cast<ProtocalHeaderItem *>(model->item(i));
            QJsonObject frameObject = QJsonObject();
            frameObject["header"] = QString(header->getHeader().toHex()).toUpper();
            QJsonArray types = QJsonArray();
            for (int j = 0; j < header->rowCount(); j++) {
                ProtocalDataItem *data = static_cast<ProtocalDataItem *>(header->child(j));
                QJsonObject type = QJsonObject();
                type["name"] = header->child(j, 1)->text();
                type["type"] = data->getType();
                types.append(type);
            }
            frameObject["types"] = types;
            object[QString(char(i))] = frameObject;
        }

        QJsonDocument saveDoc(object);

        file.write(saveDoc.toJson());

        file.close();
    }
}

void TabAdvanced::onRadioLittleBigClicked()
{
    if (radioLittle->isChecked())
        endianess = LITTLE;
    else
        endianess = BIG;
}

void TabAdvanced::updateDecodeParameters()
{
    QStandardItemModel *model = static_cast<QStandardItemModel *>(treeProtocals->model());
    listProtocals->clear();
    for (int i = 0; i < model->rowCount(); i++) {
        ProtocalHeaderItem *header = static_cast<ProtocalHeaderItem *>(model->item(i));
        Protocal *protocal = new Protocal(header->getHeader());
        for (int j = 0; j < header->rowCount(); j++) {
            ProtocalDataItem *data = static_cast<ProtocalDataItem *>(header->child(j));
            protocal->append(new VarType(data->getType()));
        }
        listProtocals->append(protocal);
    }
}

QList<Protocal *> &TabAdvanced::getListProtocals() const
{
    return *listProtocals;
}
