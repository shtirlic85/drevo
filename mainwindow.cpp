#include "mainwindow.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QProgressBar>
#include <QToolButton>
#include <QHeaderView>
#include <QInputDialog>
#include <QColorDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopWidget>
#include <QScreen>
#include <QApplication>

// Конструктор главного окна
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , nextId(1)
    , zoomFactor(1.0)
    , isDarkMode(false)
    , isFullscreen(false)
    , currentViewMode(0)
    , selectedPerson(nullptr)
{
    // Устанавливаем заголовок окна
    setWindowTitle("Генеалогическое древо - Профессиональная версия");

    // Устанавливаем иконку окна
    setWindowIcon(QIcon(":/icons/app_icon.png"));

    // Инициализируем стек отмены действий
    undoStack = new QUndoStack(this);

    // Инициализируем настройки приложения
    settings = new QSettings("GenealogyApp", "FamilyTree", this);

    // Инициализируем менеджер сетевых запросов
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::networkReplyFinished);

    // Настраиваем таймер автосохранения
    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setInterval(300000); // 5 минут
    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);
    autoSaveTimer->start();

    // Настраиваем интерфейс
    setupUI();

    // Создаем меню
    createMenuBar();

    // Создаем панель инструментов
    createToolBar();

    // Создаем строку состояния
    createStatusBar();

    // Создаем системный трей
    createSystemTray();

    // Настраиваем базу данных
    setupDatabase();

    // Загружаем данные
    loadData();

    // Настраиваем горячие клавиши
    setupShortcuts();

    // Настраиваем перетаскивание
    setupDragAndDrop();

    // Обновляем интерфейс
    updateUI();

    // Показываем приветственное сообщение
    showNotification("Добро пожаловать!", "Генеалогическое древо загружено успешно");

    // Проверяем дни рождения
    checkBirthdays();

    // Проверяем годовщины
    checkAnniversaries();
}

// Деструктор
MainWindow::~MainWindow()
{
    // Сохраняем данные перед закрытием
    saveData();

    // Закрываем соединение с базой данных
    if (db.isOpen()) {
        db.close();
    }
}

// Настройка интерфейса
void MainWindow::setupUI()
{
    // Создаем главный разделитель
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(mainSplitter);

    // Создаем левую панель с деревом
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);

    // Создаем панель поиска
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLineEdit = new QLineEdit(this);
    searchLineEdit->setPlaceholderText("Поиск по имени, фамилии, отчеству...");
    connect(searchLineEdit, &QLineEdit::textChanged,
            this, &MainWindow::searchTextChanged);

    searchComboBox = new QComboBox(this);
    searchComboBox->addItems({"Все", "Имя", "Фамилия", "Отчество", "Место рождения", "Профессия"});
    connect(searchComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::filterChanged);

    searchButton = new QPushButton("Поиск", this);
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::searchPerson);

    clearButton = new QPushButton("Очистить", this);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearSearch);

    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(searchComboBox);
    searchLayout->addWidget(searchButton);
    searchLayout->addWidget(clearButton);
    leftLayout->addLayout(searchLayout);

    // Создаем вкладки для разных режимов просмотра
    centralTabWidget = new QTabWidget(this);

    // Вкладка "Дерево"
    treeWidget = new QTreeWidget(this);
    treeWidget->setHeaderLabels({"Имя", "Фамилия", "Дата рождения", "Дата смерти", "Пол"});
    treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);
    connect(treeWidget, &QTreeWidget::itemClicked,
            this, &MainWindow::personSelected);
    centralTabWidget->addTab(treeWidget, "Дерево");

    // Вкладка "Таблица"
    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(6);
    tableWidget->setHorizontalHeaderLabels({"ID", "Имя", "Фамилия", "Дата рождения", "Дата смерти", "Пол"});
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(tableWidget, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::tableSelectionChanged);
    centralTabWidget->addTab(tableWidget, "Таблица");

    // Вкладка "Граф"
    graphicsView = new QGraphicsView(this);
    graphicsScene = new QGraphicsScene(this);
    graphicsView->setScene(graphicsScene);
    graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    graphicsView->setRenderHint(QPainter::Antialiasing);
    centralTabWidget->addTab(graphicsView, "Граф");

    // Вкладка "Информация"
    QWidget *infoPanel = new QWidget(this);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);

    // Группа основной информации
    QGroupBox *infoGroup = new QGroupBox("Информация о человеке", this);
    QGridLayout *infoGrid = new QGridLayout(infoGroup);

    QLabel *nameLabel = new QLabel("Имя:", this);
    QLineEdit *nameEdit = new QLineEdit(this);
    infoGrid->addWidget(nameLabel, 0, 0);
    infoGrid->addWidget(nameEdit, 0, 1);

    QLabel *surnameLabel = new QLabel("Фамилия:", this);
    QLineEdit *surnameEdit = new QLineEdit(this);
    infoGrid->addWidget(surnameLabel, 1, 0);
    infoGrid->addWidget(surnameEdit, 1, 1);

    QLabel *patronymicLabel = new QLabel("Отчество:", this);
    QLineEdit *patronymicEdit = new QLineEdit(this);
    infoGrid->addWidget(patronymicLabel, 2, 0);
    infoGrid->addWidget(patronymicEdit, 2, 1);

    QLabel *genderLabel = new QLabel("Пол:", this);
    QComboBox *genderCombo = new QComboBox(this);
    genderCombo->addItems({"Мужской", "Женский"});
    infoGrid->addWidget(genderLabel, 3, 0);
    infoGrid->addWidget(genderCombo, 3, 1);

    QLabel *birthLabel = new QLabel("Дата рождения:", this);
    birthDateEdit = new QDateEdit(this);
    birthDateEdit->setCalendarPopup(true);
    birthDateEdit->setDate(QDate::currentDate());
    infoGrid->addWidget(birthLabel, 4, 0);
    infoGrid->addWidget(birthDateEdit, 4, 1);

    QLabel *deathLabel = new QLabel("Дата смерти:", this);
    deathDateEdit = new QDateEdit(this);
    deathDateEdit->setCalendarPopup(true);
    deathDateEdit->setDate(QDate::currentDate());
    deathDateEdit->setSpecialValueText("Жив");
    infoGrid->addWidget(deathLabel, 5, 0);
    infoGrid->addWidget(deathDateEdit, 5, 1);

    QLabel *birthPlaceLabel = new QLabel("Место рождения:", this);
    QLineEdit *birthPlaceEdit = new QLineEdit(this);
    infoGrid->addWidget(birthPlaceLabel, 6, 0);
    infoGrid->addWidget(birthPlaceEdit, 6, 1);

    QLabel *occupationLabel = new QLabel("Профессия:", this);
    QLineEdit *occupationEdit = new QLineEdit(this);
    infoGrid->addWidget(occupationLabel, 7, 0);
    infoGrid->addWidget(occupationEdit, 7, 1);

    infoLayout->addWidget(infoGroup);

    // Группа биографии
    QGroupBox *bioGroup = new QGroupBox("Биография", this);
    QVBoxLayout *bioLayout = new QVBoxLayout(bioGroup);
    biographyTextEdit = new QTextEdit(this);
    bioLayout->addWidget(biographyTextEdit);
    infoLayout->addWidget(bioGroup);

    // Кнопки действий
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    addButton = new QPushButton("Добавить", this);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addPerson);

    editButton = new QPushButton("Редактировать", this);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::editPerson);

    deleteButton = new QPushButton("Удалить", this);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deletePerson);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    infoLayout->addLayout(buttonLayout);

    centralTabWidget->addTab(infoPanel, "Информация");

    // Вкладка "Фото"
    QWidget *photoPanel = new QWidget(this);
    QVBoxLayout *photoLayout = new QVBoxLayout(photoPanel);
    photoListWidget = new QListWidget(this);
    photoLayout->addWidget(photoListWidget);

    QHBoxLayout *photoButtonLayout = new QHBoxLayout();
    QPushButton *addPhotoButton = new QPushButton("Добавить фото", this);
    connect(addPhotoButton, &QPushButton::clicked, this, &MainWindow::addPhoto);

    QPushButton *removePhotoButton = new QPushButton("Удалить фото", this);
    connect(removePhotoButton, &QPushButton::clicked, this, &MainWindow::removePhoto);

    QPushButton *viewPhotoButton = new QPushButton("Просмотр", this);
    connect(viewPhotoButton, &QPushButton::clicked, this, &MainWindow::viewPhoto);

    photoButtonLayout->addWidget(addPhotoButton);
    photoButtonLayout->addWidget(removePhotoButton);
    photoButtonLayout->addWidget(viewPhotoButton);
    photoLayout->addLayout(photoButtonLayout);

    centralTabWidget->addTab(photoPanel, "Фото");

    // Добавляем вкладки в левую панель
    leftLayout->addWidget(centralTabWidget);

    // Добавляем панели в разделитель
    mainSplitter->addWidget(leftPanel);

    // Создаем правую панель с дополнительной информацией
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

    // Текстовое поле для отображения полной информации
    infoTextEdit = new QTextEdit(this);
    infoTextEdit->setReadOnly(true);
    rightLayout->addWidget(infoTextEdit);

    // Прогресс-бар для длительных операций
    QProgressBar *progressBar = new QProgressBar(this);
    progressBar->setVisible(false);
    rightLayout->addWidget(progressBar);

    mainSplitter->addWidget(rightPanel);

    // Устанавливаем пропорции разделителя
    mainSplitter->setSizes({width() * 3 / 4, width() / 4});
}

// Создание меню
void MainWindow::createMenuBar()
{
    // Меню "Файл"
    QMenu *fileMenu = menuBar()->addMenu("Файл");

    // Действие "Новое дерево"
    QAction *newAction = new QAction("Новое дерево", this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, [this]() {
        // Очищаем все данные
        persons.clear();
        nextId = 1;
        treeWidget->clear();
        tableWidget->setRowCount(0);
        graphicsScene->clear();
        infoTextEdit->clear();
        updateUI();
        showNotification("Новое дерево", "Создано новое генеалогическое древо");
    });
    fileMenu->addAction(newAction);

    // Действие "Открыть"
    openAction = new QAction("Открыть", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, [this]() {
        QString filename = QFileDialog::getOpenFileName(this,
                                                        "Открыть генеалогическое древо",
                                                        "",
                                                        "JSON файлы (*.json);;Все файлы (*.*)");
        if (!filename.isEmpty()) {
            importFromJson();
        }
    });
    fileMenu->addAction(openAction);

    // Действие "Сохранить"
    QAction *saveAction = new QAction("Сохранить", this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, [this]() {
        saveData();
        showNotification("Сохранение", "Данные сохранены успешно");
    });
    fileMenu->addAction(saveAction);

    // Действие "Сохранить как"
    QAction *saveAsAction = new QAction("Сохранить как...", this);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, [this]() {
        QString filename = QFileDialog::getSaveFileName(this,
                                                        "Сохранить генеалогическое древо",
                                                        "",
                                                        "JSON файлы (*.json);;Все файлы (*.*)");
        if (!filename.isEmpty()) {
            exportToJson();
            showNotification("Сохранение", "Данные сохранены в файл: " + filename);
        }
    });
    fileMenu->addAction(saveAsAction);

    fileMenu->addSeparator();

    // Действие "Экспорт в PDF"
    exportAction = new QAction("Экспорт в PDF", this);
    exportAction->setShortcut(QKeySequence("Ctrl+P"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportToPdf);
    fileMenu->addAction(exportAction);

    // Действие "Экспорт в CSV"
    exportCsvAction = new QAction("Экспорт в CSV", this);
    connect(exportCsvAction, &QAction::triggered, this, &MainWindow::exportToCsv);
    fileMenu->addAction(exportCsvAction);

    // Действие "Импорт из JSON"
    importAction = new QAction("Импорт из JSON", this);
    connect(importAction, &QAction::triggered, this, &MainWindow::importFromJson);
    fileMenu->addAction(importAction);

    fileMenu->addSeparator();

    // Действие "Печать"
    printAction = new QAction("Печать", this);
    printAction->setShortcut(QKeySequence::Print);
    connect(printAction, &QAction::triggered, this, &MainWindow::printTree);
    fileMenu->addAction(printAction);

    // Действие "Предпросмотр печати"
    QAction *printPreviewAction = new QAction("Предпросмотр печати", this);
    connect(printPreviewAction, &QAction::triggered, [this]() {
        // Создаем диалог предпросмотра печати
        QPrinter printer;
        QPrintPreviewDialog preview(&printer, this);
        connect(&preview, &QPrintPreviewDialog::paintRequested, this, &MainWindow::printTree);
        preview.exec();
    });
    fileMenu->addAction(printPreviewAction);

    fileMenu->addSeparator();

    // Действие "Настройки"
    settingsAction = new QAction("Настройки", this);
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::settingsDialog);
    fileMenu->addAction(settingsAction);

    fileMenu->addSeparator();

    // Действие "Выход"
    exitAction = new QAction("Выход", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);

    // Меню "Правка"
    QMenu *editMenu = menuBar()->addMenu("Правка");

    // Действие "Отменить"
    undoAction = undoStack->createUndoAction(this, "Отменить");
    undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAction);

    // Действие "Повторить"
    redoAction = undoStack->createRedoAction(this, "Повторить");
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(redoAction);

    editMenu->addSeparator();

    // Действие "Вырезать"
    QAction *cutAction = new QAction("Вырезать", this);
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, [this]() {
        if (selectedPerson) {
            QClipboard *clipboard = QApplication::clipboard();
            QJsonObject json;
            json["id"] = selectedPerson->id;
            json["firstName"] = selectedPerson->firstName;
            json["lastName"] = selectedPerson->lastName;
            json["patronymic"] = selectedPerson->patronymic;
            json["birthDate"] = selectedPerson->birthDate;
            json["deathDate"] = selectedPerson->deathDate;
            json["gender"] = selectedPerson->gender;
            json["birthPlace"] = selectedPerson->birthPlace;
            json["occupation"] = selectedPerson->occupation;
            json["biography"] = selectedPerson->biography;
            QJsonDocument doc(json);
            clipboard->setText(doc.toJson());

            // Удаляем вырезанного человека
            deletePerson();
        }
    });
    editMenu->addAction(cutAction);

    // Действие "Копировать"
    copyAction = new QAction("Копировать", this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &MainWindow::copyPerson);
    editMenu->addAction(copyAction);

    // Действие "Вставить"
    pasteAction = new QAction("Вставить", this);
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, &MainWindow::pastePerson);
    editMenu->addAction(pasteAction);

    editMenu->addSeparator();

    // Действие "Удалить"
    deleteAction = new QAction("Удалить", this);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deletePerson);
    editMenu->addAction(deleteAction);

    editMenu->addSeparator();

    // Действие "Поиск"
    searchAction = new QAction("Поиск", this);
    searchAction->setShortcut(QKeySequence::Find);
    connect(searchAction, &QAction::triggered, [this]() {
        searchLineEdit->setFocus();
        searchLineEdit->selectAll();
    });
    editMenu->addAction(searchAction);

    // Меню "Вид"
    QMenu *viewMenu = menuBar()->addMenu("Вид");

    // Действие "Древо"
    treeViewAction = new QAction("Древо", this);
    treeViewAction->setCheckable(true);
    treeViewAction->setChecked(true);
    connect(treeViewAction, &QAction::triggered, this, &MainWindow::treeViewMode);
    viewMenu->addAction(treeViewAction);

    // Действие "Таблица"
    listViewAction = new QAction("Таблица", this);
    listViewAction->setCheckable(true);
    connect(listViewAction, &QAction::triggered, this, &MainWindow::listViewMode);
    viewMenu->addAction(listViewAction);

    // Действие "Граф"
    timelineAction = new QAction("Граф", this);
    timelineAction->setCheckable(true);
    connect(timelineAction, &QAction::triggered, this, &MainWindow::timelineView);
    viewMenu->addAction(timelineAction);

    viewMenu->addSeparator();

    // Действие "Увеличить"
    zoomInAction = new QAction("Увеличить", this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);
    viewMenu->addAction(zoomInAction);

    // Действие "Уменьшить"
    zoomOutAction = new QAction("Уменьшить", this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);
    viewMenu->addAction(zoomOutAction);

    // Действие "Сбросить масштаб"
    resetZoomAction = new QAction("Сбросить масштаб", this);
    resetZoomAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetZoomAction, &QAction::triggered, this, &MainWindow::resetZoom);
    viewMenu->addAction(resetZoomAction);

    viewMenu->addSeparator();

    // Действие "Полноэкранный режим"
    fullscreenAction = new QAction("Полноэкранный режим", this);
    fullscreenAction->setShortcut(QKeySequence::FullScreen);
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    viewMenu->addAction(fullscreenAction);

    // Действие "Темный режим"
    darkModeAction = new QAction("Темный режим", this);
    darkModeAction->setCheckable(true);
    connect(darkModeAction, &QAction::triggered, this, &MainWindow::toggleDarkMode);
    viewMenu->addAction(darkModeAction);

    // Меню "Человек"
    QMenu *personMenu = menuBar()->addMenu("Человек");

    // Действие "Добавить"
    addAction = new QAction("Добавить человека", this);
    addAction->setShortcut(QKeySequence("Ctrl+N"));
    connect(addAction, &QAction::triggered, this, &MainWindow::addPerson);
    personMenu->addAction(addAction);

    // Действие "Редактировать"
    editAction = new QAction("Редактировать человека", this);
    editAction->setShortcut(QKeySequence("Ctrl+E"));
    connect(editAction, &QAction::triggered, this, &MainWindow::editPerson);
    personMenu->addAction(editAction);

    personMenu->addSeparator();

    // Действие "Добавить ребенка"
    QAction *addChildAction = new QAction("Добавить ребенка", this);
    addChildAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(addChildAction, &QAction::triggered, this, &MainWindow::addChild);
    personMenu->addAction(addChildAction);

    // Действие "Добавить родителя"
    QAction *addParentAction = new QAction("Добавить родителя", this);
    connect(addParentAction, &QAction::triggered, this, &MainWindow::addParent);
    personMenu->addAction(addParentAction);

    // Действие "Добавить супруга"
    QAction *addSpouseAction = new QAction("Добавить супруга", this);
    connect(addSpouseAction, &QAction::triggered, this, &MainWindow::addSpouse);
    personMenu->addAction(addSpouseAction);

    personMenu->addSeparator();

    // Действие "Показать предков"
    QAction *showAncestorsAction = new QAction("Показать предков", this);
    connect(showAncestorsAction, &QAction::triggered, this, &MainWindow::showAncestors);
    personMenu->addAction(showAncestorsAction);

    // Действие "Показать потомков"
    QAction *showDescendantsAction = new QAction("Показать потомков", this);
    connect(showDescendantsAction, &QAction::triggered, this, &MainWindow::showDescendants);
    personMenu->addAction(showDescendantsAction);

    // Действие "Показать братьев и сестер"
    QAction *showSiblingsAction = new QAction("Показать братьев и сестер", this);
    connect(showSiblingsAction, &QAction::triggered, this, &MainWindow::showSiblings);
    personMenu->addAction(showSiblingsAction);

    personMenu->addSeparator();

    // Действие "Найти путь"
    QAction *findPathAction = new QAction("Найти путь между людьми", this);
    connect(findPathAction, &QAction::triggered, this, &MainWindow::findPath);
    personMenu->addAction(findPathAction);

    // Меню "Отчеты"
    QMenu *reportMenu = menuBar()->addMenu("Отчеты");

    // Действие "Генерация отчета"
    QAction *generateReportAction = new QAction("Генерация отчета", this);
    connect(generateReportAction, &QAction::triggered, this, &MainWindow::generateReport);
    reportMenu->addAction(generateReportAction);

    // Действие "Статистика"
    QAction *statisticsAction = new QAction("Статистика", this);
    connect(statisticsAction, &QAction::triggered, this, &MainWindow::statistics);
    reportMenu->addAction(statisticsAction);

    // Меню "Сервис"
    QMenu *serviceMenu = menuBar()->addMenu("Сервис");

    // Действие "Проверка данных"
    QAction *validateAction = new QAction("Проверка данных", this);
    connect(validateAction, &QAction::triggered, this, &MainWindow::validateData);
    serviceMenu->addAction(validateAction);

    // Действие "Объединение дубликатов"
    QAction *mergeAction = new QAction("Объединение дубликатов", this);
    connect(mergeAction, &QAction::triggered, this, &MainWindow::mergeDuplicates);
    serviceMenu->addAction(mergeAction);

    // Действие "Резервное копирование"
    QAction *backupAction = new QAction("Резервное копирование", this);
    connect(backupAction, &QAction::triggered, this, &MainWindow::backupDatabase);
    serviceMenu->addAction(backupAction);

    // Действие "Восстановление из резервной копии"
    QAction *restoreAction = new QAction("Восстановление из резервной копии", this);
    connect(restoreAction, &QAction::triggered, this, &MainWindow::restoreDatabase);
    serviceMenu->addAction(restoreAction);

    // Меню "Справка"
    QMenu *helpMenu = menuBar()->addMenu("Справка");

    // Действие "Справка"
    helpAction = new QAction("Справка", this);
    helpAction->setShortcut(QKeySequence::HelpContents);
    connect(helpAction, &QAction::triggered, this, &MainWindow::showHelp);
    helpMenu->addAction(helpAction);

    // Действие "О программе"
    aboutAction = new QAction("О программе", this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);
    helpMenu->addAction(aboutAction);

    // Добавляем раздел "Недавние файлы" в меню "Файл"
    fileMenu->addSeparator();
    QMenu *recentMenu = new QMenu("Недавние файлы", this);
    fileMenu->addMenu(recentMenu);

    // Загружаем список недавних файлов из настроек
    QStringList recentFiles = settings->value("recentFiles").toStringList();
    for (const QString &file : recentFiles) {
        QAction *recentAction = new QAction(file, this);
        connect(recentAction, &QAction::triggered, [this, file]() {
            importFromJson();
        });
        recentMenu->addAction(recentAction);
    }
}

// Создание панели инструментов
void MainWindow::createToolBar()
{
    // Создаем главную панель инструментов
    QToolBar *mainToolBar = addToolBar("Главная");
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(32, 32));

    // Добавляем кнопки на панель
    mainToolBar->addAction(addAction);
    mainToolBar->addAction(editAction);
    mainToolBar->addAction(deleteAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(searchAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(undoAction);
    mainToolBar->addAction(redoAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(exportAction);
    mainToolBar->addAction(importAction);
    mainToolBar->addAction(printAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(zoomInAction);
    mainToolBar->addAction(zoomOutAction);
    mainToolBar->addAction(resetZoomAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(fullscreenAction);
    mainToolBar->addAction(darkModeAction);

    // Создаем панель инструментов "Форматирование"
    QToolBar *formatToolBar = addToolBar("Форматирование");
    formatToolBar->setMovable(false);

    // Кнопка "Жирный"
    QAction *boldAction = new QAction("Ж", this);
    boldAction->setToolTip("Жирный шрифт");
    boldAction->setCheckable(true);
    connect(boldAction, &QAction::triggered, [this](bool checked) {
        QFont font = infoTextEdit->font();
        font.setBold(checked);
        infoTextEdit->setFont(font);
    });
    formatToolBar->addAction(boldAction);

    // Кнопка "Курсив"
    QAction *italicAction = new QAction("К", this);
    italicAction->setToolTip("Курсив");
    italicAction->setCheckable(true);
    connect(italicAction, &QAction::triggered, [this](bool checked) {
        QFont font = infoTextEdit->font();
        font.setItalic(checked);
        infoTextEdit->setFont(font);
    });
    formatToolBar->addAction(italicAction);

    // Кнопка "Подчеркнутый"
    QAction *underlineAction = new QAction("Ч", this);
    underlineAction->setToolTip("Подчеркнутый");
    underlineAction->setCheckable(true);
    connect(underlineAction, &QAction::triggered, [this](bool checked) {
        QFont font = infoTextEdit->font();
        font.setUnderline(checked);
        infoTextEdit->setFont(font);
    });
    formatToolBar->addAction(underlineAction);

    formatToolBar->addSeparator();

    // Выбор размера шрифта
    QComboBox *fontSizeCombo = new QComboBox(this);
    for (int i = 8; i <= 72; i += 2) {
        fontSizeCombo->addItem(QString::number(i));
    }
    fontSizeCombo->setCurrentText("12");
    connect(fontSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, fontSizeCombo]() {
                QFont font = infoTextEdit->font();
                font.setPointSize(fontSizeCombo->currentText().toInt());
                infoTextEdit->setFont(font);
            });
    formatToolBar->addWidget(fontSizeCombo);

    // Выбор цвета
    QPushButton *colorButton = new QPushButton("Цвет", this);
    connect(colorButton, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(infoTextEdit->textColor(), this, "Выберите цвет");
        if (color.isValid()) {
            infoTextEdit->setTextColor(color);
        }
    });
    formatToolBar->addWidget(colorButton);

    // Создаем панель инструментов "Навигация"
    QToolBar *navToolBar = addToolBar("Навигация");
    navToolBar->setMovable(false);

    // Кнопка "Назад"
    QAction *backAction = new QAction("←", this);
    backAction->setToolTip("Назад");
    connect(backAction, &QAction::triggered, [this]() {
        // Возврат к предыдущему человеку
        if (!persons.isEmpty()) {
            QList<int> keys = persons.keys();
            int currentIndex = keys.indexOf(selectedPerson ? selectedPerson->id : -1);
            if (currentIndex > 0) {
                int id = keys[currentIndex - 1];
                selectedPerson = &persons[id];
                refreshInfo();
            }
        }
    });
    navToolBar->addAction(backAction);

    // Кнопка "Вперед"
    QAction *forwardAction = new QAction("→", this);
    forwardAction->setToolTip("Вперед");
    connect(forwardAction, &QAction::triggered, [this]() {
        // Переход к следующему человеку
        if (!persons.isEmpty()) {
            QList<int> keys = persons.keys();
            int currentIndex = keys.indexOf(selectedPerson ? selectedPerson->id : -1);
            if (currentIndex < keys.size() - 1 && currentIndex >= 0) {
                int id = keys[currentIndex + 1];
                selectedPerson = &persons[id];
                refreshInfo();
            }
        }
    });
    navToolBar->addAction(forwardAction);
}

// Создание строки состояния
void MainWindow::createStatusBar()
{
    // Создаем строку состояния
    statusBar()->showMessage("Готов к работе");

    // Добавляем индикатор количества людей
    QLabel *countLabel = new QLabel(this);
    countLabel->setText("Людей: 0");
    statusBar()->addWidget(countLabel);

    // Добавляем индикатор базы данных
    QLabel *dbLabel = new QLabel(this);
    dbLabel->setText("База данных: OK");
    statusBar()->addPermanentWidget(dbLabel);

    // Добавляем индикатор времени
    QLabel *timeLabel = new QLabel(this);
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [timeLabel]() {
        timeLabel->setText(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));
    });
    timer->start(1000);
    statusBar()->addPermanentWidget(timeLabel);

    // Добавляем прогресс-бар в строку состояния
    QProgressBar *statusProgress = new QProgressBar(this);
    statusProgress->setMaximumWidth(150);
    statusProgress->setVisible(false);
    statusBar()->addPermanentWidget(statusProgress);
}

// Создание системного трея
void MainWindow::createSystemTray()
{
    // Проверяем, поддерживается ли системный трей
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    // Создаем иконку в системном трее
    trayIcon = new QSystemTrayIcon(QIcon(":/icons/app_icon.png"), this);
    trayIcon->setToolTip("Генеалогическое древо");

    // Создаем контекстное меню для трея
    QMenu *trayMenu = new QMenu(this);
    QAction *showAction = new QAction("Показать", this);
    connect(showAction, &QAction::triggered, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    trayMenu->addAction(showAction);

    QAction *hideAction = new QAction("Скрыть", this);
    connect(hideAction, &QAction::triggered, [this]() {
        hide();
    });
    trayMenu->addAction(hideAction);

    trayMenu->addSeparator();

    QAction *exitTrayAction = new QAction("Выход", this);
    connect(exitTrayAction, &QAction::triggered, [this]() {
        close();
    });
    trayMenu->addAction(exitTrayAction);

    trayIcon->setContextMenu(trayMenu);

    // Показываем иконку в трее
    trayIcon->show();

    // Обработка двойного клика по иконке
    connect(trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            if (isHidden()) {
                showNormal();
                raise();
                activateWindow();
            } else {
                hide();
            }
        }
    });
}

// Настройка базы данных
void MainWindow::setupDatabase()
{
    // Создаем базу данных SQLite
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("genealogy.db");

    if (!db.open()) {
        QMessageBox::critical(this, "Ошибка базы данных",
                              "Не удалось открыть базу данных: " + db.lastError().text());
        return;
    }

    // Создаем таблицу, если она не существует
    QSqlQuery query;
    bool success = query.exec(
        "CREATE TABLE IF NOT EXISTS persons ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "firstName TEXT NOT NULL, "
        "lastName TEXT NOT NULL, "
        "patronymic TEXT, "
        "birthDate TEXT, "
        "deathDate TEXT, "
        "gender TEXT, "
        "birthPlace TEXT, "
        "deathPlace TEXT, "
        "occupation TEXT, "
        "biography TEXT, "
        "photoPath TEXT, "
        "fatherId INTEGER, "
        "motherId INTEGER, "
        "spouseId INTEGER, "
        "childrenIds TEXT"
        ")"
        );

    if (!success) {
        QMessageBox::critical(this, "Ошибка базы данных",
                              "Не удалось создать таблицу: " + query.lastError().text());
        return;
    }

    // Настраиваем модель таблицы
    model = new QSqlTableModel(this, db);
    model->setTable("persons");
    model->select();

    // Настраиваем прокси-модель для фильтрации
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setFilterKeyColumn(1); // Фильтрация по имени
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

// Загрузка данных
void MainWindow::loadData()
{
    // Очищаем существующие данные
    persons.clear();
    treeWidget->clear();
    tableWidget->setRowCount(0);
    graphicsScene->clear();

    // Загружаем данные из базы данных
    QSqlQuery query("SELECT * FROM persons");
    while (query.next()) {
        Person person;
        person.id = query.value("id").toInt();
        person.firstName = query.value("firstName").toString();
        person.lastName = query.value("lastName").toString();
        person.patronymic = query.value("patronymic").toString();
        person.birthDate = query.value("birthDate").toString();
        person.deathDate = query.value("deathDate").toString();
        person.gender = query.value("gender").toString();
        person.birthPlace = query.value("birthPlace").toString();
        person.deathPlace = query.value("deathPlace").toString();
        person.occupation = query.value("occupation").toString();
        person.biography = query.value("biography").toString();
        person.photoPath = query.value("photoPath").toString();
        person.fatherId = query.value("fatherId").toInt();
        person.motherId = query.value("motherId").toInt();
        person.spouseId = query.value("spouseId").toInt();

        // Загружаем ID детей
        QString childrenStr = query.value("childrenIds").toString();
        if (!childrenStr.isEmpty()) {
            for (const QString &idStr : childrenStr.split(",")) {
                person.childrenIds.append(idStr.toInt());
            }
        }

        persons[person.id] = person;
        if (person.id >= nextId) {
            nextId = person.id + 1;
        }
    }

    // Обновляем интерфейс
    refreshTree();
    refreshTable();
    refreshGraph();
    refreshInfo();

    // Обновляем статус
    statusBar()->showMessage("Загружено " + QString::number(persons.size()) + " человек");

    // Обновляем индикатор количества людей
    QList<QLabel*> labels = statusBar()->findChildren<QLabel*>();
    if (!labels.isEmpty()) {
        labels.first()->setText("Людей: " + QString::number(persons.size()));
    }
}

// Сохранение данных
void MainWindow::saveData()
{
    // Начинаем транзакцию
    db.transaction();

    // Очищаем таблицу
    QSqlQuery query("DELETE FROM persons");
    if (!query.exec()) {
        db.rollback();
        QMessageBox::critical(this, "Ошибка сохранения",
                              "Не удалось очистить таблицу: " + query.lastError().text());
        return;
    }

    // Сохраняем всех людей
    for (const Person &person : persons) {
        QSqlQuery insertQuery;
        insertQuery.prepare(
            "INSERT INTO persons ("
            "id, firstName, lastName, patronymic, birthDate, deathDate, "
            "gender, birthPlace, deathPlace, occupation, biography, photoPath, "
            "fatherId, motherId, spouseId, childrenIds"
            ") VALUES ("
            ":id, :firstName, :lastName, :patronymic, :birthDate, :deathDate, "
            ":gender, :birthPlace, :deathPlace, :occupation, :biography, :photoPath, "
            ":fatherId, :motherId, :spouseId, :childrenIds"
            ")"
            );

        insertQuery.bindValue(":id", person.id);
        insertQuery.bindValue(":firstName", person.firstName);
        insertQuery.bindValue(":lastName", person.lastName);
        insertQuery.bindValue(":patronymic", person.patronymic);
        insertQuery.bindValue(":birthDate", person.birthDate);
        insertQuery.bindValue(":deathDate", person.deathDate);
        insertQuery.bindValue(":gender", person.gender);
        insertQuery.bindValue(":birthPlace", person.birthPlace);
        insertQuery.bindValue(":deathPlace", person.deathPlace);
        insertQuery.bindValue(":occupation", person.occupation);
        insertQuery.bindValue(":biography", person.biography);
        insertQuery.bindValue(":photoPath", person.photoPath);
        insertQuery.bindValue(":fatherId", person.fatherId);
        insertQuery.bindValue(":motherId", person.motherId);
        insertQuery.bindValue(":spouseId", person.spouseId);

        // Сохраняем ID детей как строку
        QStringList childrenStr;
        for (int childId : person.childrenIds) {
            childrenStr.append(QString::number(childId));
        }
        insertQuery.bindValue(":childrenIds", childrenStr.join(","));

        if (!insertQuery.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Ошибка сохранения",
                                  "Не удалось сохранить данные: " + insertQuery.lastError().text());
            return;
        }
    }

    // Подтверждаем транзакцию
    if (!db.commit()) {
        QMessageBox::critical(this, "Ошибка сохранения",
                              "Не удалось подтвердить транзакцию: " + db.lastError().text());
        return;
    }

    // Сохраняем настройки
    settings->setValue("lastSave", QDateTime::currentDateTime().toString());
    settings->sync();

    // Показываем уведомление
    showNotification("Сохранение", "Данные успешно сохранены в базе данных");
}

// Обновление интерфейса
void MainWindow::updateUI()
{
    // Обновляем состояние кнопок
    bool hasSelection = selectedPerson != nullptr;
    editButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
    editAction->setEnabled(hasSelection);
    deleteAction->setEnabled(hasSelection);
    copyAction->setEnabled(hasSelection);

    // Обновляем строку состояния
    if (selectedPerson) {
        statusBar()->showMessage("Выбран: " + selectedPerson->fullName());
    }
}

// Обновление дерева
void MainWindow::refreshTree()
{
    treeWidget->clear();

    // Находим корневые элементы (люди без родителей)
    QList<int> rootIds;
    for (const Person &person : persons) {
        if (person.fatherId == -1 && person.motherId == -1) {
            rootIds.append(person.id);
        }
    }

    // Если корневых элементов нет, добавляем всех
    if (rootIds.isEmpty()) {
        for (const Person &person : persons) {
            rootIds.append(person.id);
        }
    }

    // Строим дерево для каждого корневого элемента
    for (int id : rootIds) {
        if (persons.contains(id)) {
            QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget);
            Person *person = &persons[id];
            item->setText(0, person->firstName);
            item->setText(1, person->lastName);
            item->setText(2, person->birthDate);
            item->setText(3, person->deathDate);
            item->setText(4, person->gender);
            item->setData(0, Qt::UserRole, person->id);

            // Рекурсивно добавляем детей
            buildTree(person, item);
        }
    }

    // Раскрываем все элементы
    treeWidget->expandAll();

    // Автоматически подгоняем ширину столбцов
    for (int i = 0; i < treeWidget->columnCount(); ++i) {
        treeWidget->resizeColumnToContents(i);
    }
}

// Построение дерева (рекурсивная функция)
void MainWindow::buildTree(Person *person, QTreeWidgetItem *parent)
{
    // Находим детей текущего человека
    for (int childId : person->childrenIds) {
        if (persons.contains(childId)) {
            Person *child = &persons[childId];
            QTreeWidgetItem *childItem = new QTreeWidgetItem(parent);
            childItem->setText(0, child->firstName);
            childItem->setText(1, child->lastName);
            childItem->setText(2, child->birthDate);
            childItem->setText(3, child->deathDate);
            childItem->setText(4, child->gender);
            childItem->setData(0, Qt::UserRole, child->id);

            // Рекурсивно добавляем детей ребенка
            buildTree(child, childItem);
        }
    }
}

// Обновление таблицы
void MainWindow::refreshTable()
{
    // Устанавливаем количество строк
    tableWidget->setRowCount(persons.size());

    // Заполняем таблицу
    int row = 0;
    for (const Person &person : persons) {
        tableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(person.id)));
        tableWidget->setItem(row, 1, new QTableWidgetItem(person.firstName));
        tableWidget->setItem(row, 2, new QTableWidgetItem(person.lastName));
        tableWidget->setItem(row, 3, new QTableWidgetItem(person.birthDate));
        tableWidget->setItem(row, 4, new QTableWidgetItem(person.deathDate));
        tableWidget->setItem(row, 5, new QTableWidgetItem(person.gender));
        row++;
    }
}

// Обновление графа
void MainWindow::refreshGraph()
{
    graphicsScene->clear();

    if (persons.isEmpty()) {
        return;
    }

    // Находим корневой элемент (первый человек без родителей)
    Person *root = nullptr;
    for (auto &person : persons) {
        if (person.fatherId == -1 && person.motherId == -1) {
            root = &person;
            break;
        }
    }

    if (!root) {
        // Если нет корневого, берем первого
        root = &persons.first();
    }

    // Рисуем дерево
    qreal x = 0, y = 0;
    calculateLayout(root, x, y);
}

// Расчет макета для графа
void MainWindow::calculateLayout(Person *person, qreal &x, qreal &y)
{
    if (!person) return;

    // Рисуем текущего человека
    drawPerson(person, x, y);

    // Рисуем детей
    qreal childX = x - (person->childrenIds.size() - 1) * 50;
    qreal childY = y + 100;

    for (int childId : person->childrenIds) {
        if (persons.contains(childId)) {
            Person *child = &persons[childId];
            drawRelationship(person, child);
            calculateLayout(child, childX, childY);
            childX += 100;
        }
    }
}

// Отрисовка человека на графе
void MainWindow::drawPerson(Person *person, qreal x, qreal y)
{
    if (!person) return;

    // Создаем прямоугольник для человека
    qreal width = 100;
    qreal height = 60;
    QRectF rect(x - width/2, y - height/2, width, height);

    // Создаем прямоугольник с закругленными углами
    QGraphicsRectItem *rectItem = graphicsScene->addRect(rect, QPen(Qt::black, 2),
                                                         QBrush(person->gender == "Мужской" ? Qt::lightGray : QColorConstants::Svg::lightpink));

    // Добавляем текст (имя и фамилия)
    QString text = person->firstName + "\n" + person->lastName;
    QGraphicsTextItem *textItem = graphicsScene->addText(text);
    textItem->setPos(x - textItem->boundingRect().width()/2, y - textItem->boundingRect().height()/2);
    textItem->setDefaultTextColor(Qt::black);

    // Сохраняем указатель на человека в элементе
    rectItem->setData(0, person->id);
    textItem->setData(0, person->id);

    // Добавляем даты рождения и смерти
    QString dates = person->birthDate + " - " + person->deathDate;
    QGraphicsTextItem *datesItem = graphicsScene->addText(dates);
    datesItem->setPos(x - datesItem->boundingRect().width()/2, y + height/2 + 5);
    datesItem->setDefaultTextColor(Qt::darkBlue);
    datesItem->setFont(QFont("Arial", 8));
    datesItem->setData(0, person->id);

    // Если человек жив, добавляем индикатор
    if (person->deathDate.isEmpty() || person->deathDate == "Жив") {
        QGraphicsEllipseItem *aliveIndicator = graphicsScene->addEllipse(
            x + width/2 - 10, y - height/2, 10, 10, QPen(Qt::green), QBrush(Qt::green));
        aliveIndicator->setData(0, person->id);
    }
}

// Отрисовка связи между людьми
void MainWindow::drawRelationship(Person *parent, Person *child)
{
    if (!parent || !child) return;

    // Находим координаты родителей и детей на сцене
    QList<QGraphicsItem*> items = graphicsScene->items();
    QPointF parentPos, childPos;

    for (QGraphicsItem *item : items) {
        if (item->data(0).toInt() == parent->id) {
            parentPos = item->pos();
        }
        if (item->data(0).toInt() == child->id) {
            childPos = item->pos();
        }
    }

    // Рисуем линию связи
    QGraphicsLineItem *line = graphicsScene->addLine(
        parentPos.x(), parentPos.y() + 30,
        childPos.x(), childPos.y() - 30,
        QPen(Qt::darkGray, 2, Qt::SolidLine, Qt::RoundCap)
        );
    line->setData(0, -1);
}

// Обновление информации
void MainWindow::refreshInfo()
{
    if (!selectedPerson) {
        infoTextEdit->clear();
        return;
    }

    QString info;
    info += "<h2>" + selectedPerson->fullName() + "</h2>";
    info += "<hr>";
    info += "<b>ID:</b> " + QString::number(selectedPerson->id) + "<br>";
    info += "<b>Пол:</b> " + selectedPerson->gender + "<br>";
    info += "<b>Дата рождения:</b> " + selectedPerson->birthDate + "<br>";
    info += "<b>Дата смерти:</b> " + (selectedPerson->deathDate.isEmpty() ? "Жив" : selectedPerson->deathDate) + "<br>";
    info += "<b>Место рождения:</b> " + selectedPerson->birthPlace + "<br>";
    info += "<b>Место смерти:</b> " + selectedPerson->deathPlace + "<br>";
    info += "<b>Профессия:</b> " + selectedPerson->occupation + "<br>";

    // Информация о родителях
    if (selectedPerson->fatherId != -1 && persons.contains(selectedPerson->fatherId)) {
        info += "<b>Отец:</b> " + persons[selectedPerson->fatherId].fullName() + "<br>";
    }
    if (selectedPerson->motherId != -1 && persons.contains(selectedPerson->motherId)) {
        info += "<b>Мать:</b> " + persons[selectedPerson->motherId].fullName() + "<br>";
    }

    // Информация о супруге
    if (selectedPerson->spouseId != -1 && persons.contains(selectedPerson->spouseId)) {
        info += "<b>Супруг(а):</b> " + persons[selectedPerson->spouseId].fullName() + "<br>";
    }

    // Информация о детях
    if (!selectedPerson->childrenIds.isEmpty()) {
        info += "<b>Дети:</b><br>";
        for (int childId : selectedPerson->childrenIds) {
            if (persons.contains(childId)) {
                info += "&nbsp;&nbsp;&nbsp;- " + persons[childId].fullName() + "<br>";
            }
        }
    }

    info += "<hr>";
    info += "<b>Биография:</b><br>";
    info += selectedPerson->biography;

    // Добавляем фото, если есть
    if (!selectedPerson->photoPath.isEmpty()) {
        info += "<hr>";
        info += "<b>Фото:</b><br>";
        info += "<img src='" + selectedPerson->photoPath + "' width='200'><br>";
    }

    infoTextEdit->setHtml(info);
}

// Добавление человека
void MainWindow::addPerson()
{
    // Создаем диалог для ввода данных
    QDialog dialog(this);
    dialog.setWindowTitle("Добавление человека");
    dialog.setModal(true);
    dialog.resize(500, 600);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Создаем поля ввода
    QGridLayout *gridLayout = new QGridLayout();

    QLabel *nameLabel = new QLabel("Имя:", &dialog);
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    gridLayout->addWidget(nameLabel, 0, 0);
    gridLayout->addWidget(nameEdit, 0, 1);

    QLabel *surnameLabel = new QLabel("Фамилия:", &dialog);
    QLineEdit *surnameEdit = new QLineEdit(&dialog);
    gridLayout->addWidget(surnameLabel, 1, 0);
    gridLayout->addWidget(surnameEdit, 1, 1);

    QLabel *patronymicLabel = new QLabel("Отчество:", &dialog);
    QLineEdit *patronymicEdit = new QLineEdit(&dialog);
    gridLayout->addWidget(patronymicLabel, 2, 0);
    gridLayout->addWidget(patronymicEdit, 2, 1);

    QLabel *genderLabel = new QLabel("Пол:", &dialog);
    QComboBox *genderCombo = new QComboBox(&dialog);
    genderCombo->addItems({"Мужской", "Женский"});
    gridLayout->addWidget(genderLabel, 3, 0);
    gridLayout->addWidget(genderCombo, 3, 1);

    QLabel *birthLabel = new QLabel("Дата рождения:", &dialog);
    QDateEdit *birthEdit = new QDateEdit(&dialog);
    birthEdit->setCalendarPopup(true);
    birthEdit->setDate(QDate::currentDate());
    gridLayout->addWidget(birthLabel, 4, 0);
    gridLayout->addWidget(birthEdit, 4, 1);

    QLabel *deathLabel = new QLabel("Дата смерти:", &dialog);
    QDateEdit *deathEdit = new QDateEdit(&dialog);
    deathEdit->setCalendarPopup(true);
    deathEdit->setSpecialValueText("Жив");
    deathEdit->setDate(QDate::currentDate());
    gridLayout->addWidget(deathLabel, 5, 0);
    gridLayout->addWidget(deathEdit, 5, 1);

    QLabel *birthPlaceLabel = new QLabel("Место рождения:", &dialog);
    QLineEdit *birthPlaceEdit = new QLineEdit(&dialog);
    gridLayout->addWidget(birthPlaceLabel, 6, 0);
    gridLayout->addWidget(birthPlaceEdit, 6, 1);

    QLabel *occupationLabel = new QLabel("Профессия:", &dialog);
    QLineEdit *occupationEdit = new QLineEdit(&dialog);
    gridLayout->addWidget(occupationLabel, 7, 0);
    gridLayout->addWidget(occupationEdit, 7, 1);

    layout->addLayout(gridLayout);

    QLabel *bioLabel = new QLabel("Биография:", &dialog);
    layout->addWidget(bioLabel);

    QTextEdit *bioEdit = new QTextEdit(&dialog);
    bioEdit->setMaximumHeight(150);
    layout->addWidget(bioEdit);

    // Кнопки
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("Добавить", &dialog);
    QPushButton *cancelButton = new QPushButton("Отмена", &dialog);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        // Создаем нового человека
        Person newPerson;
        newPerson.id = nextId++;
        newPerson.firstName = nameEdit->text();
        newPerson.lastName = surnameEdit->text();
        newPerson.patronymic = patronymicEdit->text();
        newPerson.gender = genderCombo->currentText();
        newPerson.birthDate = birthEdit->date().toString("dd.MM.yyyy");
        newPerson.deathDate = deathEdit->date().toString("dd.MM.yyyy");
        if (newPerson.deathDate == "Жив") {
            newPerson.deathDate = "";
        }
        newPerson.birthPlace = birthPlaceEdit->text();
        newPerson.occupation = occupationEdit->text();
        newPerson.biography = bioEdit->toPlainText();
        newPerson.fatherId = -1;
        newPerson.motherId = -1;
        newPerson.spouseId = -1;

        // Добавляем в карту
        persons[newPerson.id] = newPerson;

        // Обновляем интерфейс
        refreshTree();
        refreshTable();
        refreshGraph();
        refreshInfo();
        updateUI();

        // Логируем действие
        logAction("Добавлен человек: " + newPerson.fullName());

        // Показываем уведомление
        showNotification("Добавление", "Человек успешно добавлен");
    }
}

// Редактирование человека
void MainWindow::editPerson()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Не выбран человек для редактирования");
        return;
    }

    // Создаем диалог для редактирования
    QDialog dialog(this);
    dialog.setWindowTitle("Редактирование человека");
    dialog.setModal(true);
    dialog.resize(500, 600);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Создаем поля ввода
    QGridLayout *gridLayout = new QGridLayout();

    QLabel *nameLabel = new QLabel("Имя:", &dialog);
    QLineEdit *nameEdit = new QLineEdit(selectedPerson->firstName, &dialog);
    gridLayout->addWidget(nameLabel, 0, 0);
    gridLayout->addWidget(nameEdit, 0, 1);

    QLabel *surnameLabel = new QLabel("Фамилия:", &dialog);
    QLineEdit *surnameEdit = new QLineEdit(selectedPerson->lastName, &dialog);
    gridLayout->addWidget(surnameLabel, 1, 0);
    gridLayout->addWidget(surnameEdit, 1, 1);

    QLabel *patronymicLabel = new QLabel("Отчество:", &dialog);
    QLineEdit *patronymicEdit = new QLineEdit(selectedPerson->patronymic, &dialog);
    gridLayout->addWidget(patronymicLabel, 2, 0);
    gridLayout->addWidget(patronymicEdit, 2, 1);

    QLabel *genderLabel = new QLabel("Пол:", &dialog);
    QComboBox *genderCombo = new QComboBox(&dialog);
    genderCombo->addItems({"Мужской", "Женский"});
    genderCombo->setCurrentText(selectedPerson->gender);
    gridLayout->addWidget(genderLabel, 3, 0);
    gridLayout->addWidget(genderCombo, 3, 1);

    QLabel *birthLabel = new QLabel("Дата рождения:", &dialog);
    QDateEdit *birthEdit = new QDateEdit(&dialog);
    birthEdit->setCalendarPopup(true);
    birthEdit->setDate(QDate::fromString(selectedPerson->birthDate, "dd.MM.yyyy"));
    gridLayout->addWidget(birthLabel, 4, 0);
    gridLayout->addWidget(birthEdit, 4, 1);

    QLabel *deathLabel = new QLabel("Дата смерти:", &dialog);
    QDateEdit *deathEdit = new QDateEdit(&dialog);
    deathEdit->setCalendarPopup(true);
    deathEdit->setSpecialValueText("Жив");
    if (!selectedPerson->deathDate.isEmpty()) {
        deathEdit->setDate(QDate::fromString(selectedPerson->deathDate, "dd.MM.yyyy"));
    } else {
        deathEdit->setDate(QDate::currentDate());
    }
    gridLayout->addWidget(deathLabel, 5, 0);
    gridLayout->addWidget(deathEdit, 5, 1);

    QLabel *birthPlaceLabel = new QLabel("Место рождения:", &dialog);
    QLineEdit *birthPlaceEdit = new QLineEdit(selectedPerson->birthPlace, &dialog);
    gridLayout->addWidget(birthPlaceLabel, 6, 0);
    gridLayout->addWidget(birthPlaceEdit, 6, 1);

    QLabel *occupationLabel = new QLabel("Профессия:", &dialog);
    QLineEdit *occupationEdit = new QLineEdit(selectedPerson->occupation, &dialog);
    gridLayout->addWidget(occupationLabel, 7, 0);
    gridLayout->addWidget(occupationEdit, 7, 1);

    layout->addLayout(gridLayout);

    QLabel *bioLabel = new QLabel("Биография:", &dialog);
    layout->addWidget(bioLabel);

    QTextEdit *bioEdit = new QTextEdit(selectedPerson->biography, &dialog);
    bioEdit->setMaximumHeight(150);
    layout->addWidget(bioEdit);

    // Кнопки
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("Сохранить", &dialog);
    QPushButton *cancelButton = new QPushButton("Отмена", &dialog);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        // Сохраняем изменения
        selectedPerson->firstName = nameEdit->text();
        selectedPerson->lastName = surnameEdit->text();
        selectedPerson->patronymic = patronymicEdit->text();
        selectedPerson->gender = genderCombo->currentText();
        selectedPerson->birthDate = birthEdit->date().toString("dd.MM.yyyy");
        selectedPerson->deathDate = deathEdit->date().toString("dd.MM.yyyy");
        if (selectedPerson->deathDate == "Жив") {
            selectedPerson->deathDate = "";
        }
        selectedPerson->birthPlace = birthPlaceEdit->text();
        selectedPerson->occupation = occupationEdit->text();
        selectedPerson->biography = bioEdit->toPlainText();

        // Обновляем интерфейс
        refreshTree();
        refreshTable();
        refreshGraph();
        refreshInfo();

        // Логируем действие
        logAction("Отредактирован человек: " + selectedPerson->fullName());

        // Показываем уведомление
        showNotification("Редактирование", "Данные успешно обновлены");
    }
}

// Удаление человека
void MainWindow::deletePerson()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Не выбран человек для удаления");
        return;
    }

    // Подтверждаем удаление
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Подтверждение удаления",
                                  "Вы уверены, что хотите удалить " + selectedPerson->fullName() + "?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) {
        return;
    }

    // Удаляем человека
    int id = selectedPerson->id;

    // Удаляем из карты
    persons.remove(id);

    // Удаляем из дерева
    QTreeWidgetItem *item = nullptr;
    findPersonInTree(id, treeWidget->invisibleRootItem(), &item);
    if (item) {
        delete item;
    }

    // Удаляем из таблицы
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        if (tableWidget->item(row, 0)->text().toInt() == id) {
            tableWidget->removeRow(row);
            break;
        }
    }

    // Очищаем выбранного человека
    selectedPerson = nullptr;
    refreshInfo();
    updateUI();

    // Логируем действие
    logAction("Удален человек с ID: " + QString::number(id));

    // Показываем уведомление
    showNotification("Удаление", "Человек успешно удален");
}

// Поиск человека
void MainWindow::searchPerson()
{
    QString searchText = searchLineEdit->text().trimmed();
    if (searchText.isEmpty()) {
        QMessageBox::information(this, "Поиск", "Введите текст для поиска");
        return;
    }

    // Выполняем поиск
    QList<int> results;
    int searchField = searchComboBox->currentIndex();

    for (const Person &person : persons) {
        bool found = false;
        switch (searchField) {
        case 0: // Все
            if (person.firstName.contains(searchText, Qt::CaseInsensitive) ||
                person.lastName.contains(searchText, Qt::CaseInsensitive) ||
                person.patronymic.contains(searchText, Qt::CaseInsensitive) ||
                person.birthPlace.contains(searchText, Qt::CaseInsensitive) ||
                person.occupation.contains(searchText, Qt::CaseInsensitive)) {
                found = true;
            }
            break;
        case 1: // Имя
            if (person.firstName.contains(searchText, Qt::CaseInsensitive)) {
                found = true;
            }
            break;
        case 2: // Фамилия
            if (person.lastName.contains(searchText, Qt::CaseInsensitive)) {
                found = true;
            }
            break;
        case 3: // Отчество
            if (person.patronymic.contains(searchText, Qt::CaseInsensitive)) {
                found = true;
            }
            break;
        case 4: // Место рождения
            if (person.birthPlace.contains(searchText, Qt::CaseInsensitive)) {
                found = true;
            }
            break;
        case 5: // Профессия
            if (person.occupation.contains(searchText, Qt::CaseInsensitive)) {
                found = true;
            }
            break;
        }
        if (found) {
            results.append(person.id);
        }
    }

    // Показываем результаты
    if (results.isEmpty()) {
        QMessageBox::information(this, "Поиск", "Ничего не найдено");
        return;
    }

    QString message = "Найдено " + QString::number(results.size()) + " человек(а):\n";
    for (int id : results) {
        if (persons.contains(id)) {
            message += "- " + persons[id].fullName() + "\n";
        }
    }
    QMessageBox::information(this, "Результаты поиска", message);

    // Выделяем первого найденного
    if (!results.isEmpty()) {
        selectedPerson = &persons[results.first()];
        refreshInfo();
        updateUI();

        // Показываем уведомление
        showNotification("Поиск", "Найден: " + selectedPerson->fullName());
    }
}

// Очистка поиска
void MainWindow::clearSearch()
{
    searchLineEdit->clear();
    searchComboBox->setCurrentIndex(0);
    refreshTree();
    refreshTable();
    refreshGraph();
}

// Экспорт в PDF
void MainWindow::exportToPdf()
{
    // Создаем диалог сохранения файла
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Экспорт в PDF",
                                                    "family_tree.pdf",
                                                    "PDF файлы (*.pdf)");

    if (filename.isEmpty()) {
        return;
    }

    // Создаем принтер
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filename);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(10, 10, 10, 10));

    // Создаем диалог печати
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() != QDialog::Accepted) {
        return;
    }
    // Печатаем дерево
    printTree();
}

// Экспорт в JSON
void MainWindow::exportToJson()
{
    QString filename;
    if (filename.isEmpty()) {
        filename = QFileDialog::getSaveFileName(this,
                                                "Экспорт в JSON",
                                                "family_tree.json",
                                                "JSON файлы (*.json)");
        if (filename.isEmpty()) {
            return;
        }
    }

    // Создаем JSON объект
    QJsonObject rootObject;
    QJsonArray personsArray;

    for (const Person &person : persons) {
        QJsonObject personObject;
        personObject["id"] = person.id;
        personObject["firstName"] = person.firstName;
        personObject["lastName"] = person.lastName;
        personObject["patronymic"] = person.patronymic;
        personObject["birthDate"] = person.birthDate;
        personObject["deathDate"] = person.deathDate;
        personObject["gender"] = person.gender;
        personObject["birthPlace"] = person.birthPlace;
        personObject["deathPlace"] = person.deathPlace;
        personObject["occupation"] = person.occupation;
        personObject["biography"] = person.biography;
        personObject["photoPath"] = person.photoPath;
        personObject["fatherId"] = person.fatherId;
        personObject["motherId"] = person.motherId;
        personObject["spouseId"] = person.spouseId;

        QJsonArray childrenArray;
        for (int childId : person.childrenIds) {
            childrenArray.append(childId);
        }
        personObject["childrenIds"] = childrenArray;

        personsArray.append(personObject);
    }

    rootObject["persons"] = personsArray;
    rootObject["nextId"] = nextId;

    // Сохраняем в файл
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл");
        return;
    }

    QJsonDocument doc(rootObject);
    file.write(doc.toJson());
    file.close();

    // Добавляем файл в список недавних
    QStringList recentFiles = settings->value("recentFiles").toStringList();
    recentFiles.removeAll(filename);
    recentFiles.prepend(filename);
    if (recentFiles.size() > 10) {
        recentFiles.removeLast();
    }
    settings->setValue("recentFiles", recentFiles);

    // Показываем уведомление
    showNotification("Экспорт", "Данные успешно экспортированы в " + filename);
}

// Импорт из JSON
void MainWindow::importFromJson()
{
    QString filename;
    if (filename.isEmpty()) {
        filename = QFileDialog::getOpenFileName(this,
                                                "Импорт из JSON",
                                                "",
                                                "JSON файлы (*.json)");
        if (filename.isEmpty()) {
            return;
        }
    }

    // Загружаем файл
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // Парсим JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        QMessageBox::critical(this, "Ошибка", "Неверный формат JSON");
        return;
    }

    QJsonObject rootObject = doc.object();
    QJsonArray personsArray = rootObject["persons"].toArray();

    // Очищаем текущие данные
    persons.clear();
    nextId = rootObject["nextId"].toInt();

    // Загружаем данные
    for (const QJsonValue &value : personsArray) {
        QJsonObject personObject = value.toObject();
        Person person;
        person.id = personObject["id"].toInt();
        person.firstName = personObject["firstName"].toString();
        person.lastName = personObject["lastName"].toString();
        person.patronymic = personObject["patronymic"].toString();
        person.birthDate = personObject["birthDate"].toString();
        person.deathDate = personObject["deathDate"].toString();
        person.gender = personObject["gender"].toString();
        person.birthPlace = personObject["birthPlace"].toString();
        person.deathPlace = personObject["deathPlace"].toString();
        person.occupation = personObject["occupation"].toString();
        person.biography = personObject["biography"].toString();
        person.photoPath = personObject["photoPath"].toString();
        person.fatherId = personObject["fatherId"].toInt();
        person.motherId = personObject["motherId"].toInt();
        person.spouseId = personObject["spouseId"].toInt();

        QJsonArray childrenArray = personObject["childrenIds"].toArray();
        for (const QJsonValue &childValue : childrenArray) {
            person.childrenIds.append(childValue.toInt());
        }

        persons[person.id] = person;
    }

    // Обновляем интерфейс
    refreshTree();
    refreshTable();
    refreshGraph();
    refreshInfo();
    updateUI();

    // Добавляем файл в список недавних
    QStringList recentFiles = settings->value("recentFiles").toStringList();
    recentFiles.removeAll(filename);
    recentFiles.prepend(filename);
    if (recentFiles.size() > 10) {
        recentFiles.removeLast();
    }
    settings->setValue("recentFiles", recentFiles);

    // Показываем уведомление
    showNotification("Импорт", "Данные успешно импортированы из " + filename);
}

// Печать дерева
void MainWindow::printTrees()
{
    // Создаем принтер
    QPrinter printer;
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() != QDialog::Accepted) {
        printTrees();
        return;
    }
}

// Печать дерева с принтером
void MainWindow::printTree()
{
    QPrinter printer;
    // Создаем документ для печати
    QTextDocument doc;
    QString html;

    html += "<html><head><style>";
    html += "body { font-family: Arial, sans-serif; }";
    html += "h1 { color: #2c3e50; text-align: center; }";
    html += ".person { margin: 10px; padding: 10px; border: 1px solid #bdc3c7; border-radius: 5px; }";
    html += ".name { font-size: 14pt; font-weight: bold; }";
    html += ".info { margin-left: 20px; }";
    html += "</style></head><body>";

    html += "<h1>Генеалогическое древо</h1>";
    html += "<p><b>Дата создания:</b> " + QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss") + "</p>";
    html += "<hr>";

    // Добавляем информацию о всех людях
    for (const Person &person : persons) {
        html += "<div class='person'>";
        html += "<div class='name'>" + person.fullName() + "</div>";
        html += "<div class='info'>";
        html += "<b>Пол:</b> " + person.gender + "<br>";
        html += "<b>Дата рождения:</b> " + person.birthDate + "<br>";
        if (!person.deathDate.isEmpty()) {
            html += "<b>Дата смерти:</b> " + person.deathDate + "<br>";
        }
        if (!person.birthPlace.isEmpty()) {
            html += "<b>Место рождения:</b> " + person.birthPlace + "<br>";
        }
        if (!person.occupation.isEmpty()) {
            html += "<b>Профессия:</b> " + person.occupation + "<br>";
        }
        html += "</div>";
        html += "</div>";
    }

    html += "</body></html>";

    doc.setHtml(html);
    doc.print(&printer);
}

// Добавление ребенка
void MainWindow::addChild()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для добавления ребенка");
        return;
    }

    // Создаем нового человека
    addPerson();

    // Если человек был добавлен, связываем его с текущим
    if (!persons.isEmpty()) {
        int lastId = nextId - 1;
        if (persons.contains(lastId)) {
            // Добавляем ребенка к текущему человеку
            selectedPerson->childrenIds.append(lastId);

            // Устанавливаем родителя для ребенка
            if (selectedPerson->gender == "Мужской") {
                persons[lastId].fatherId = selectedPerson->id;
            } else {
                persons[lastId].motherId = selectedPerson->id;
            }

            // Обновляем интерфейс
            refreshTree();
            refreshGraph();
            refreshInfo();

            // Логируем действие
            logAction("Добавлен ребенок для " + selectedPerson->fullName());
        }
    }
}

// Добавление родителя
void MainWindow::addParent()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для добавления родителя");
        return;
    }

    // Создаем нового человека
    addPerson();

    // Если человек был добавлен, связываем его с текущим
    if (!persons.isEmpty()) {
        int lastId = nextId - 1;
        if (persons.contains(lastId)) {
            // Добавляем родителя к текущему человеку
            if (persons[lastId].gender == "Мужской") {
                selectedPerson->fatherId = lastId;
            } else {
                selectedPerson->motherId = lastId;
            }

            // Добавляем текущего человека как ребенка родителя
            persons[lastId].childrenIds.append(selectedPerson->id);

            // Обновляем интерфейс
            refreshTree();
            refreshGraph();
            refreshInfo();

            // Логируем действие
            logAction("Добавлен родитель для " + selectedPerson->fullName());
        }
    }
}

// Добавление супруга
void MainWindow::addSpouse()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для добавления супруга");
        return;
    }

    // Создаем нового человека
    addPerson();

    // Если человек был добавлен, связываем его с текущим
    if (!persons.isEmpty()) {
        int lastId = nextId - 1;
        if (persons.contains(lastId)) {
            // Устанавливаем супругов друг для друга
            selectedPerson->spouseId = lastId;
            persons[lastId].spouseId = selectedPerson->id;

            // Обновляем интерфейс
            refreshTree();
            refreshGraph();
            refreshInfo();

            // Логируем действие
            logAction("Добавлен супруг для " + selectedPerson->fullName());
        }
    }
}

// Поиск пути между людьми
void MainWindow::findPath()
{
    if (persons.size() < 2) {
        QMessageBox::information(this, "Поиск пути", "Недостаточно людей для поиска пути");
        return;
    }

    // Создаем список для выбора двух людей
    QStringList names;
    QList<int> ids;
    for (const Person &person : persons) {
        names.append(person.fullName());
        ids.append(person.id);
    }

    bool ok;
    QString name1 = QInputDialog::getItem(this, "Выбор первого человека",
                                          "Выберите первого человека:", names, 0, false, &ok);
    if (!ok) return;

    QString name2 = QInputDialog::getItem(this, "Выбор второго человека",
                                          "Выберите второго человека:", names, 0, false, &ok);
    if (!ok) return;

    // Находим ID выбранных людей
    int id1 = -1, id2 = -1;
    for (int i = 0; i < names.size(); ++i) {
        if (names[i] == name1) id1 = ids[i];
        if (names[i] == name2) id2 = ids[i];
    }

    if (id1 == -1 || id2 == -1 || id1 == id2) {
        QMessageBox::warning(this, "Ошибка", "Некорректный выбор");
        return;
    }

    // Поиск пути (простой алгоритм BFS)
    QMap<int, int> visited; // ID -> родительский ID
    QQueue<int> queue;
    queue.enqueue(id1);
    visited[id1] = -1;

    bool found = false;
    while (!queue.isEmpty() && !found) {
        int currentId = queue.dequeue();
        Person &current = persons[currentId];

        // Проверяем всех связанных людей
        QList<int> related;
        related.append(current.fatherId);
        related.append(current.motherId);
        related.append(current.spouseId);
        related.append(current.childrenIds);

        for (int relId : related) {
            if (relId != -1 && persons.contains(relId) && !visited.contains(relId)) {
                visited[relId] = currentId;
                queue.enqueue(relId);
                if (relId == id2) {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {
        QMessageBox::information(this, "Поиск пути", "Путь не найден");
        return;
    }

    // Восстанавливаем путь
    QList<int> path;
    int currentId = id2;
    while (currentId != -1) {
        path.prepend(currentId);
        currentId = visited[currentId];
    }

    // Показываем путь
    QString pathStr = "Путь: ";
    for (int i = 0; i < path.size(); ++i) {
        pathStr += persons[path[i]].fullName();
        if (i < path.size() - 1) {
            pathStr += " → ";
        }
    }

    QMessageBox::information(this, "Найденный путь", pathStr);
}

// Показ предков
void MainWindow::showAncestors()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для показа предков");
        return;
    }

    QString ancestors = "Предки " + selectedPerson->fullName() + ":\n\n";

    // Рекурсивно собираем предков
    QList<int> ancestorsIds;
    QQueue<int> queue;
    queue.enqueue(selectedPerson->fatherId);
    queue.enqueue(selectedPerson->motherId);

    while (!queue.isEmpty()) {
        int id = queue.dequeue();
        if (id != -1 && persons.contains(id) && !ancestorsIds.contains(id)) {
            ancestorsIds.append(id);
            Person &person = persons[id];
            ancestors += "- " + person.fullName() + " (" + person.birthDate;
            if (!person.deathDate.isEmpty()) {
                ancestors += " - " + person.deathDate;
            }
            ancestors += ")\n";
            queue.enqueue(person.fatherId);
            queue.enqueue(person.motherId);
        }
    }

    if (ancestorsIds.isEmpty()) {
        ancestors += "Нет информации о предках\n";
    }

    QMessageBox::information(this, "Предки", ancestors);
}

// Показ потомков
void MainWindow::showDescendants()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для показа потомков");
        return;
    }

    QString descendants = "Потомки " + selectedPerson->fullName() + ":\n\n";

    // Рекурсивно собираем потомков
    QList<int> descendantsIds;
    QQueue<int> queue;
    for (int childId : selectedPerson->childrenIds) {
        queue.enqueue(childId);
    }

    while (!queue.isEmpty()) {
        int id = queue.dequeue();
        if (id != -1 && persons.contains(id) && !descendantsIds.contains(id)) {
            descendantsIds.append(id);
            Person &person = persons[id];
            descendants += "- " + person.fullName() + " (" + person.birthDate;
            if (!person.deathDate.isEmpty()) {
                descendants += " - " + person.deathDate;
            }
            descendants += ")\n";
            for (int childId : person.childrenIds) {
                queue.enqueue(childId);
            }
        }
    }

    if (descendantsIds.isEmpty()) {
        descendants += "Нет информации о потомках\n";
    }

    QMessageBox::information(this, "Потомки", descendants);
}

// Показ братьев и сестер
void MainWindow::showSiblings()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для показа братьев и сестер");
        return;
    }

    QString siblings = "Братья и сестры " + selectedPerson->fullName() + ":\n\n";

    // Находим общих родителей
    QList<int> parentIds;
    if (selectedPerson->fatherId != -1) parentIds.append(selectedPerson->fatherId);
    if (selectedPerson->motherId != -1) parentIds.append(selectedPerson->motherId);

    if (parentIds.isEmpty()) {
        siblings += "Нет информации о родителях\n";
        QMessageBox::information(this, "Братья и сестры", siblings);
        return;
    }

    // Ищем всех детей общих родителей
    QList<int> siblingIds;
    for (int parentId : parentIds) {
        if (persons.contains(parentId)) {
            for (int childId : persons[parentId].childrenIds) {
                if (childId != selectedPerson->id && !siblingIds.contains(childId)) {
                    siblingIds.append(childId);
                }
            }
        }
    }

    if (siblingIds.isEmpty()) {
        siblings += "Нет братьев и сестер\n";
    } else {
        for (int id : siblingIds) {
            Person &person = persons[id];
            siblings += "- " + person.fullName() + " (" + person.birthDate;
            if (!person.deathDate.isEmpty()) {
                siblings += " - " + person.deathDate;
            }
            siblings += ")\n";
        }
    }

    QMessageBox::information(this, "Братья и сестры", siblings);
}

// Генерация отчета
void MainWindow::generateReport()
{
    // Создаем диалог сохранения файла
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Сохранить отчет",
                                                    "report.html",
                                                    "HTML файлы (*.html)");

    if (filename.isEmpty()) {
        return;
    }

    // Генерируем HTML отчет
    QString html;
    html += "<!DOCTYPE html>\n";
    html += "<html>\n<head>\n";
    html += "<meta charset='UTF-8'>\n";
    html += "<title>Генеалогический отчет</title>\n";
    html += "<style>\n";
    html += "body { font-family: Arial, sans-serif; margin: 20px; }\n";
    html += "h1 { color: #2c3e50; text-align: center; }\n";
    html += ".person { margin: 15px 0; padding: 15px; border: 1px solid #bdc3c7; border-radius: 5px; background: #f8f9fa; }\n";
    html += ".name { font-size: 16pt; font-weight: bold; color: #2c3e50; }\n";
    html += ".detail { margin-left: 20px; color: #34495e; }\n";
    html += ".label { font-weight: bold; }\n";
    html += "</style>\n";
    html += "</head>\n<body>\n";

    html += "<h1>Генеалогическое древо - Полный отчет</h1>\n";
    html += "<p><b>Дата создания:</b> " + QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss") + "</p>\n";
    html += "<p><b>Всего людей:</b> " + QString::number(persons.size()) + "</p>\n";
    html += "<hr>\n";

    // Добавляем информацию о всех людях
    for (const Person &person : persons) {
        html += "<div class='person'>\n";
        html += "<div class='name'>" + person.fullName() + "</div>\n";
        html += "<div class='detail'>\n";
        html += "<span class='label'>ID:</span> " + QString::number(person.id) + "<br>\n";
        html += "<span class='label'>Пол:</span> " + person.gender + "<br>\n";
        html += "<span class='label'>Дата рождения:</span> " + person.birthDate + "<br>\n";
        if (!person.deathDate.isEmpty()) {
            html += "<span class='label'>Дата смерти:</span> " + person.deathDate + "<br>\n";
        }
        if (!person.birthPlace.isEmpty()) {
            html += "<span class='label'>Место рождения:</span> " + person.birthPlace + "<br>\n";
        }
        if (!person.occupation.isEmpty()) {
            html += "<span class='label'>Профессия:</span> " + person.occupation + "<br>\n";
        }

        // Родители
        if (person.fatherId != -1 && persons.contains(person.fatherId)) {
            html += "<span class='label'>Отец:</span> " + persons[person.fatherId].fullName() + "<br>\n";
        }
        if (person.motherId != -1 && persons.contains(person.motherId)) {
            html += "<span class='label'>Мать:</span> " + persons[person.motherId].fullName() + "<br>\n";
        }

        // Супруг
        if (person.spouseId != -1 && persons.contains(person.spouseId)) {
            html += "<span class='label'>Супруг(а):</span> " + persons[person.spouseId].fullName() + "<br>\n";
        }

        // Дети
        if (!person.childrenIds.isEmpty()) {
            html += "<span class='label'>Дети:</span><br>\n";
            for (int childId : person.childrenIds) {
                if (persons.contains(childId)) {
                    html += "&nbsp;&nbsp;&nbsp;- " + persons[childId].fullName() + "<br>\n";
                }
            }
        }

        // Биография
        if (!person.biography.isEmpty()) {
            html += "<span class='label'>Биография:</span><br>\n";
            html += "<div style='margin-left: 20px;'>" + person.biography + "</div>\n";
        }

        html += "</div>\n";
        html += "</div>\n";
    }

    html += "</body>\n</html>";

    // Сохраняем файл
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл");
        return;
    }

    file.write(html.toUtf8());
    file.close();

    QMessageBox::information(this, "Отчет", "Отчет успешно создан: " + filename);
}

// Статистика
void MainWindow::statistics()
{
    if (persons.isEmpty()) {
        QMessageBox::information(this, "Статистика", "Нет данных для статистики");
        return;
    }

    QString stats;
    stats += "Статистика генеалогического древа:\n\n";
    stats += "Всего людей: " + QString::number(persons.size()) + "\n";

    // Подсчет по полу
    int male = 0, female = 0;
    for (const Person &person : persons) {
        if (person.gender == "Мужской") male++;
        else if (person.gender == "Женский") female++;
    }
    stats += "Мужчин: " + QString::number(male) + "\n";
    stats += "Женщин: " + QString::number(female) + "\n\n";

    // Подсчет по поколениям
    QMap<int, int> generations;
    for (const Person &person : persons) {
        // Подсчет поколения (простая эвристика)
        int generation = 0;
        int currentId = person.id;
        while (currentId != -1 && persons.contains(currentId)) {
            if (persons[currentId].fatherId != -1) {
                currentId = persons[currentId].fatherId;
                generation++;
            } else if (persons[currentId].motherId != -1) {
                currentId = persons[currentId].motherId;
                generation++;
            } else {
                break;
            }
        }
        generations[generation]++;
    }

    stats += "Поколения:\n";
    for (auto it = generations.begin(); it != generations.end(); ++it) {
        stats += "  Поколение " + QString::number(it.key()) + ": " + QString::number(it.value()) + " человек\n";
    }
    stats += "\n";

    // Средняя продолжительность жизни
    int totalAge = 0;
    int countWithDeath = 0;
    for (const Person &person : persons) {
        if (!person.deathDate.isEmpty() && !person.birthDate.isEmpty()) {
            QDate birth = QDate::fromString(person.birthDate, "dd.MM.yyyy");
            QDate death = QDate::fromString(person.deathDate, "dd.MM.yyyy");
            if (birth.isValid() && death.isValid()) {
                totalAge += birth.daysTo(death) / 365;
                countWithDeath++;
            }
        }
    }
    if (countWithDeath > 0) {
        stats += "Средняя продолжительность жизни: " +
                 QString::number(totalAge / countWithDeath) + " лет\n";
    }

    // Самый старший и младший
    QDate oldestDate, youngestDate;
    QString oldestName, youngestName;
    for (const Person &person : persons) {
        if (!person.birthDate.isEmpty()) {
            QDate birth = QDate::fromString(person.birthDate, "dd.MM.yyyy");
            if (birth.isValid()) {
                if (!oldestDate.isValid() || birth < oldestDate) {
                    oldestDate = birth;
                    oldestName = person.fullName();
                }
                if (!youngestDate.isValid() || birth > youngestDate) {
                    youngestDate = birth;
                    youngestName = person.fullName();
                }
            }
        }
    }
    if (!oldestName.isEmpty()) {
        stats += "Самый старший: " + oldestName + " (" + oldestDate.toString("dd.MM.yyyy") + ")\n";
    }
    if (!youngestName.isEmpty()) {
        stats += "Самый младший: " + youngestName + " (" + youngestDate.toString("dd.MM.yyyy") + ")\n";
    }

    QMessageBox::information(this, "Статистика", stats);
}

// Резервное копирование
void MainWindow::backupDatabase()
{
    // Создаем диалог сохранения файла
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Резервное копирование",
                                                    "backup_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".json",
                                                    "JSON файлы (*.json)");

    if (filename.isEmpty()) {
        return;
    }

    // Экспортируем данные
    exportToJson();

    // Показываем уведомление
    showNotification("Резервное копирование", "Резервная копия создана: " + filename);
}

// Восстановление из резервной копии
void MainWindow::restoreDatabase()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Восстановление",
                                  "Восстановление из резервной копии заменит все текущие данные. Продолжить?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) {
        return;
    }

    // Выбираем файл для восстановления
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Восстановление из резервной копии",
                                                    "",
                                                    "JSON файлы (*.json)");

    if (filename.isEmpty()) {
        return;
    }

    // Импортируем данные
    importFromJson();

    // Показываем уведомление
    showNotification("Восстановление", "Данные восстановлены из: " + filename);
}

// Диалог настроек
void MainWindow::settingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Настройки");
    dialog.setModal(true);
    dialog.resize(600, 400);

    QTabWidget *tabWidget = new QTabWidget(&dialog);

    // Вкладка "Общие"
    QWidget *generalTab = new QWidget();
    QVBoxLayout *generalLayout = new QVBoxLayout(generalTab);

    QCheckBox *autoSaveCheck = new QCheckBox("Автосохранение", generalTab);
    autoSaveCheck->setChecked(settings->value("autoSave", true).toBool());
    connect(autoSaveCheck, &QCheckBox::toggled, [this](bool checked) {
        settings->setValue("autoSave", checked);
        if (checked) {
            autoSaveTimer->start();
        } else {
            autoSaveTimer->stop();
        }
    });
    generalLayout->addWidget(autoSaveCheck);

    QCheckBox *startupCheck = new QCheckBox("Запускать при старте системы", generalTab);
    startupCheck->setChecked(settings->value("startup", false).toBool());
    connect(startupCheck, &QCheckBox::toggled, [this](bool checked) {
        settings->setValue("startup", checked);
        // Здесь можно добавить код для добавления/удаления из автозагрузки
    });
    generalLayout->addWidget(startupCheck);

    QCheckBox *trayCheck = new QCheckBox("Сворачивать в трей", generalTab);
    trayCheck->setChecked(settings->value("tray", true).toBool());
    connect(trayCheck, &QCheckBox::toggled, [this](bool checked) {
        settings->setValue("tray", checked);
    });
    generalLayout->addWidget(trayCheck);

    generalLayout->addStretch();
    tabWidget->addTab(generalTab, "Общие");

    // Вкладка "Внешний вид"
    QWidget *appearanceTab = new QWidget();
    QVBoxLayout *appearanceLayout = new QVBoxLayout(appearanceTab);

    QLabel *themeLabel = new QLabel("Тема:", appearanceTab);
    QComboBox *themeCombo = new QComboBox(appearanceTab);
    themeCombo->addItems({"Светлая", "Темная", "Системная"});
    themeCombo->setCurrentText(settings->value("theme", "Светлая").toString());
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, themeCombo]() {
                settings->setValue("theme", themeCombo->currentText());
                applyTheme(themeCombo->currentText());
            });
    appearanceLayout->addWidget(themeLabel);
    appearanceLayout->addWidget(themeCombo);

    QLabel *fontLabel = new QLabel("Шрифт:", appearanceTab);
    QFont currentFont = font();
    QPushButton *fontButton = new QPushButton("Выбрать шрифт", appearanceTab);
    fontButton->setText(currentFont.family() + " " + QString::number(currentFont.pointSize()));
    connect(fontButton, &QPushButton::clicked, [this, fontButton]() {
        bool ok;
        QFont font = QFontDialog::getFont(&ok, this->font(), this, "Выберите шрифт");
        if (ok) {
            this->setFont(font);
            fontButton->setText(font.family() + " " + QString::number(font.pointSize()));
            settings->setValue("font", font.toString());
        }
    });
    appearanceLayout->addWidget(fontLabel);
    appearanceLayout->addWidget(fontButton);

    appearanceLayout->addStretch();
    tabWidget->addTab(appearanceTab, "Внешний вид");

    // Вкладка "База данных"
    QWidget *dbTab = new QWidget();
    QVBoxLayout *dbLayout = new QVBoxLayout(dbTab);

    QLabel *dbPathLabel = new QLabel("Путь к базе данных:", dbTab);
    QLineEdit *dbPathEdit = new QLineEdit(dbTab);
    dbPathEdit->setText(db.databaseName());
    dbPathEdit->setReadOnly(true);
    dbLayout->addWidget(dbPathLabel);
    dbLayout->addWidget(dbPathEdit);

    QPushButton *dbPathButton = new QPushButton("Изменить путь", dbTab);
    connect(dbPathButton, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getSaveFileName(this,
                                                    "Выберите путь для базы данных",
                                                    "genealogy.db",
                                                    "Базы данных (*.db)");
        if (!path.isEmpty()) {
            db.setDatabaseName(path);
            if (db.open()) {
                QMessageBox::information(this, "База данных", "База данных перемещена");
                db.close();
            }
        }
    });
    dbLayout->addWidget(dbPathButton);

    QPushButton *exportDbButton = new QPushButton("Экспортировать базу данных", dbTab);
    connect(exportDbButton, &QPushButton::clicked, this, &MainWindow::exportToJson);
    dbLayout->addWidget(exportDbButton);

    QPushButton *importDbButton = new QPushButton("Импортировать базу данных", dbTab);
    connect(importDbButton, &QPushButton::clicked, this, &MainWindow::importFromJson);
    dbLayout->addWidget(importDbButton);

    dbLayout->addStretch();
    tabWidget->addTab(dbTab, "База данных");

    // Кнопки диалога
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("OK", &dialog);
    QPushButton *cancelButton = new QPushButton("Отмена", &dialog);
    QPushButton *applyButton = new QPushButton("Применить", &dialog);

    buttonLayout->addWidget(applyButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, [this]() {
        settings->sync();
        QMessageBox::information(this, "Настройки", "Настройки применены");
    });

    dialog.exec();
}

// Информация о программе
void MainWindow::about()
{
    QMessageBox::about(this, "О программе",
                       "<h2>Генеалогическое древо</h2>"
                       "<p><b>Версия:</b> 1.0.0</p>"
                       "<p><b>Разработчик:</b> Профессиональная команда</p>"
                       "<p><b>Описание:</b> Мощная программа для ведения генеалогического древа</p>"
                       "<p><b>Особенности:</b></p>"
                       "<ul>"
                       "<li>Визуальное представление древа</li>"
                       "<li>Табличный и графический режимы</li>"
                       "<li>Поиск и фильтрация</li>"
                       "<li>Экспорт в PDF, JSON, CSV</li>"
                       "<li>Печать древа</li>"
                       "<li>Статистика и отчеты</li>"
                       "<li>Резервное копирование</li>"
                       "<li>Поддержка фото</li>"
                       "<li>Темный режим</li>"
                       "<li>Многоязычный интерфейс</li>"
                       "</ul>"
                       "<p><b>Используемые технологии:</b></p>"
                       "<ul>"
                       "<li>Qt 6</li>"
                       "<li>C++17</li>"
                       "<li>SQLite</li>"
                       "</ul>"
                       "<p><b>Лицензия:</b> MIT</p>"
                       "<p><b>Сайт:</b> <a href='https://example.com'>example.com</a></p>"
                       "<p><b>Email:</b> <a href='mailto:support@example.com'>support@example.com</a></p>"
                       );
}

// Справка
void MainWindow::showHelp()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Справка");
    dialog.resize(800, 600);

    QTextEdit *helpText = new QTextEdit(&dialog);
    helpText->setReadOnly(true);

    QString help;
    help += "<h1>Руководство пользователя</h1>";
    help += "<h2>Основные функции</h2>";
    help += "<h3>1. Добавление человека</h3>";
    help += "<p>Для добавления человека нажмите кнопку 'Добавить' или выберите 'Человек → Добавить человека'.</p>";

    help += "<h3>2. Редактирование человека</h3>";
    help += "<p>Выберите человека в дереве или таблице и нажмите кнопку 'Редактировать'.</p>";

    help += "<h3>3. Удаление человека</h3>";
    help += "<p>Выберите человека и нажмите кнопку 'Удалить'.</p>";

    help += "<h3>4. Поиск</h3>";
    help += "<p>Введите текст в поле поиска и выберите категорию для поиска.</p>";

    help += "<h3>5. Экспорт и импорт</h3>";
    help += "<p>Используйте меню 'Файл' для экспорта в PDF, JSON, CSV или импорта из JSON.</p>";

    help += "<h3>6. Печать</h3>";
    help += "<p>Выберите 'Файл → Печать' для печати генеалогического древа.</p>";

    help += "<h3>7. Отчеты</h3>";
    help += "<p>Меню 'Отчеты' позволяет генерировать отчеты и статистику.</p>";

    help += "<h3>8. Настройки</h3>";
    help += "<p>Меню 'Файл → Настройки' позволяет настроить внешний вид и поведение программы.</p>";

    help += "<h2>Горячие клавиши</h2>";
    help += "<table border='1' cellpadding='5'>";
    help += "<tr><th>Действие</th><th>Клавиши</th></tr>";
    help += "<tr><td>Новое дерево</td><td>Ctrl+N</td></tr>";
    help += "<tr><td>Открыть</td><td>Ctrl+O</td></tr>";
    help += "<tr><td>Сохранить</td><td>Ctrl+S</td></tr>";
    help += "<tr><td>Сохранить как</td><td>Ctrl+Shift+S</td></tr>";
    help += "<tr><td>Печать</td><td>Ctrl+P</td></tr>";
    help += "<tr><td>Поиск</td><td>Ctrl+F</td></tr>";
    help += "<tr><td>Добавить</td><td>Ctrl+N</td></tr>";
    help += "<tr><td>Редактировать</td><td>Ctrl+E</td></tr>";
    help += "<tr><td>Удалить</td><td>Delete</td></tr>";
    help += "<tr><td>Отменить</td><td>Ctrl+Z</td></tr>";
    help += "<tr><td>Повторить</td><td>Ctrl+Y</td></tr>";
    help += "<tr><td>Копировать</td><td>Ctrl+C</td></tr>";
    help += "<tr><td>Вставить</td><td>Ctrl+V</td></tr>";
    help += "<tr><td>Увеличить</td><td>Ctrl++</td></tr>";
    help += "<tr><td>Уменьшить</td><td>Ctrl+-</td></tr>";
    help += "<tr><td>Сбросить масштаб</td><td>Ctrl+0</td></tr>";
    help += "<tr><td>Полноэкранный режим</td><td>F11</td></tr>";
    help += "<tr><td>Справка</td><td>F1</td></tr>";
    help += "</table>";

    helpText->setHtml(help);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(helpText);

    dialog.exec();
}

// Выбор человека из дерева
void MainWindow::personSelected(QTreeWidgetItem *item, int column)
{
    if (!item) return;

    int id = item->data(0, Qt::UserRole).toInt();
    if (persons.contains(id)) {
        selectedPerson = &persons[id];
        refreshInfo();
        updateUI();

        // Логируем действие
        logAction("Выбран человек: " + selectedPerson->fullName());
    }
}

// Изменение выделения в таблице
void MainWindow::tableSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = tableWidget->selectedItems();
    if (selectedItems.isEmpty()) return;

    int row = selectedItems.first()->row();
    int id = tableWidget->item(row, 0)->text().toInt();

    if (persons.contains(id)) {
        selectedPerson = &persons[id];
        refreshInfo();
        updateUI();
    }
}

// Поиск в дереве (рекурсивная функция)
void MainWindow::findPersonInTree(int id, QTreeWidgetItem *parent, QTreeWidgetItem **result)
{
    if (!parent) return;

    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->data(0, Qt::UserRole).toInt() == id) {
            *result = child;
            return;
        }
        findPersonInTree(id, child, result);
        if (*result) return;
    }
}

// Автосохранение
void MainWindow::autoSave()
{
    if (settings->value("autoSave", true).toBool() && !persons.isEmpty()) {
        saveData();
        showNotification("Автосохранение", "Данные автоматически сохранены");
    }
}

// Показ уведомления
void MainWindow::showNotification(QString title, QString message)
{
    if (trayIcon && trayIcon->isVisible()) {
        trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    }

    // Также показываем в строке состояния
    statusBar()->showMessage(title + ": " + message, 5000);
}

// Логирование действий
void MainWindow::logAction(QString action)
{
    // Добавляем запись в лог
    QString logEntry = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") +
                       " - " + action;

    // Сохраняем в файл лога
    QFile logFile("genealogy.log");
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << logEntry << "\n";
        logFile.close();
    }

    // Добавляем в строку состояния
    statusBar()->showMessage("Лог: " + action, 3000);
}

// Валидация данных
bool MainWindow::validatePerson(Person &person)
{
    // Проверяем обязательные поля
    if (person.firstName.isEmpty() || person.lastName.isEmpty()) {
        return false;
    }

    // Проверяем дату рождения
    if (!person.birthDate.isEmpty()) {
        QDate birth = QDate::fromString(person.birthDate, "dd.MM.yyyy");
        if (!birth.isValid()) {
            return false;
        }
    }

    // Проверяем дату смерти
    if (!person.deathDate.isEmpty()) {
        QDate death = QDate::fromString(person.deathDate, "dd.MM.yyyy");
        if (!death.isValid()) {
            return false;
        }
    }

    return true;
}

// Проверка данных
void MainWindow::validateData()
{
    if (persons.isEmpty()) {
        QMessageBox::information(this, "Проверка данных", "Нет данных для проверки");
        return;
    }

    QString errors;
    int errorCount = 0;

    for (const Person &person : persons) {
        if (!validatePerson(const_cast<Person&>(person))) {
            errors += "Ошибка в данных человека: " + person.fullName() + "\n";
            errorCount++;
        }
    }

    // Проверяем связи
    for (const Person &person : persons) {
        // Проверяем родителей
        if (person.fatherId != -1 && !persons.contains(person.fatherId)) {
            errors += "Отец не найден для: " + person.fullName() + "\n";
            errorCount++;
        }
        if (person.motherId != -1 && !persons.contains(person.motherId)) {
            errors += "Мать не найдена для: " + person.fullName() + "\n";
            errorCount++;
        }
        if (person.spouseId != -1 && !persons.contains(person.spouseId)) {
            errors += "Супруг не найден для: " + person.fullName() + "\n";
            errorCount++;
        }

        // Проверяем детей
        for (int childId : person.childrenIds) {
            if (!persons.contains(childId)) {
                errors += "Ребенок не найден для: " + person.fullName() + "\n";
                errorCount++;
            }
        }
    }

    if (errorCount == 0) {
        QMessageBox::information(this, "Проверка данных", "Все данные корректны");
    } else {
        QMessageBox::warning(this, "Ошибки в данных",
                             "Найдено " + QString::number(errorCount) + " ошибок:\n\n" + errors);
    }
}

// Объединение дубликатов
void MainWindow::mergeDuplicates()
{
    if (persons.isEmpty()) {
        QMessageBox::information(this, "Объединение", "Нет данных для объединения");
        return;
    }

    // Поиск дубликатов по имени и дате рождения
    QMap<QString, QList<int>> duplicates;
    for (const Person &person : persons) {
        QString key = person.fullName() + "|" + person.birthDate;
        duplicates[key].append(person.id);
    }

    int merged = 0;
    for (auto it = duplicates.begin(); it != duplicates.end(); ++it) {
        if (it.value().size() > 1) {
            // Объединяем дубликаты
            int mainId = it.value().first();
            Person &main = persons[mainId];

            for (int i = 1; i < it.value().size(); ++i) {
                int dupId = it.value()[i];
                Person &dup = persons[dupId];

                // Объединяем данные
                if (dup.biography.length() > main.biography.length()) {
                    main.biography = dup.biography;
                }
                if (!dup.photoPath.isEmpty() && main.photoPath.isEmpty()) {
                    main.photoPath = dup.photoPath;
                }
                if (!dup.birthPlace.isEmpty() && main.birthPlace.isEmpty()) {
                    main.birthPlace = dup.birthPlace;
                }
                if (!dup.occupation.isEmpty() && main.occupation.isEmpty()) {
                    main.occupation = dup.occupation;
                }

                // Перенаправляем связи
                for (Person &person : persons) {
                    if (person.fatherId == dupId) person.fatherId = mainId;
                    if (person.motherId == dupId) person.motherId = mainId;
                    if (person.spouseId == dupId) person.spouseId = mainId;

                    // Заменяем в списке детей
                    for (int &childId : person.childrenIds) {
                        if (childId == dupId) childId = mainId;
                    }
                }

                // Удаляем дубликат
                persons.remove(dupId);
                merged++;
            }
        }
    }

    // Обновляем интерфейс
    refreshTree();
    refreshTable();
    refreshGraph();
    refreshInfo();

    QMessageBox::information(this, "Объединение",
                             "Объединено " + QString::number(merged) + " дубликатов");
}

// Генерация семейного кода
void MainWindow::generateFamilyCode(Person *person)
{
    if (!person) return;

    // Создаем уникальный код на основе данных
    QString code = person->lastName + "_" + person->birthDate;
    code = code.toUpper();

    // Добавляем хеш для уникальности
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(code.toUtf8());
    QString hashStr = hash.result().toHex().left(8);

    QString familyCode = code + "_" + hashStr;

    // Показываем код
    QMessageBox::information(this, "Семейный код",
                             "Семейный код для " + person->fullName() + ":\n\n" + familyCode);
}

// Создание временной линии
void MainWindow::createTimeline(Person *person)
{
    if (!person) return;

    QString timeline = "Временная линия для " + person->fullName() + ":\n\n";
    timeline += "Рождение: " + person->birthDate + "\n";

    // Добавляем события из биографии
    if (!person->biography.isEmpty()) {
        timeline += "\nСобытия:\n";
        QStringList events = person->biography.split("\n");
        for (const QString &event : events) {
            if (!event.trimmed().isEmpty()) {
                timeline += "  - " + event + "\n";
            }
        }
    }

    if (!person->deathDate.isEmpty()) {
        timeline += "\nСмерть: " + person->deathDate + "\n";
    }

    QMessageBox::information(this, "Временная линия", timeline);
}

// Проверка дней рождения
void MainWindow::checkBirthdays()
{
    QDate today = QDate::currentDate();
    QString birthdays;

    for (const Person &person : persons) {
        if (!person.birthDate.isEmpty()) {
            QDate birth = QDate::fromString(person.birthDate, "dd.MM.yyyy");
            if (birth.isValid()) {
                QDate thisYearBirth(birth.year(), birth.month(), birth.day());
                if (thisYearBirth.dayOfYear() == today.dayOfYear()) {
                    birthdays += "- " + person.fullName() + " (" +
                                 QString::number(today.year() - birth.year()) + " лет)\n";
                }
            }
        }
    }

    if (!birthdays.isEmpty()) {
        showNotification("Дни рождения сегодня",
                         "Сегодня день рождения у:\n" + birthdays);
    }
}

// Проверка годовщин
void MainWindow::checkAnniversaries()
{
    QDate today = QDate::currentDate();
    QString anniversaries;

    for (const Person &person : persons) {
        if (!person.deathDate.isEmpty()) {
            QDate death = QDate::fromString(person.deathDate, "dd.MM.yyyy");
            if (death.isValid()) {
                QDate thisYearDeath(death.year(), death.month(), death.day());
                int years = today.year() - death.year();
                if (thisYearDeath.dayOfYear() == today.dayOfYear() && years > 0) {
                    anniversaries += "- " + person.fullName() + " (" +
                                     QString::number(years) + " лет со дня смерти)\n";
                }
            }
        }
    }

    if (!anniversaries.isEmpty()) {
        showNotification("Годовщины сегодня",
                         "Сегодня годовщины:\n" + anniversaries);
    }
}

// Отправка push-уведомления
void MainWindow::sendPushNotification(QString title, QString message)
{
    // Здесь можно добавить интеграцию с push-сервисами
    // Например, через Firebase Cloud Messaging или свои серверы

    // В демонстрационных целях просто показываем уведомление
    showNotification("Push: " + title, message);
}

// Применение темы
void MainWindow::applyTheme(QString theme)
{
    if (theme == "Темная") {
        isDarkMode = true;
        darkModeAction->setChecked(true);

        // Применяем темную тему
        QString styleSheet =
            "QMainWindow { background-color: #2b2b2b; }"
            "QWidget { background-color: #2b2b2b; color: #ffffff; }"
            "QTreeWidget, QTableWidget, QTextEdit, QLineEdit, QComboBox { "
            "  background-color: #3c3c3c; color: #ffffff; "
            "  border: 1px solid #5a5a5a; "
            "}"
            "QPushButton { "
            "  background-color: #3c3c3c; color: #ffffff; "
            "  border: 1px solid #5a5a5a; border-radius: 3px; "
            "  padding: 5px 10px; "
            "}"
            "QPushButton:hover { background-color: #4a4a4a; }"
            "QPushButton:pressed { background-color: #2a2a2a; }"
            "QMenuBar { background-color: #2b2b2b; color: #ffffff; }"
            "QMenuBar::item:selected { background-color: #3c3c3c; }"
            "QMenu { background-color: #2b2b2b; color: #ffffff; }"
            "QMenu::item:selected { background-color: #3c3c3c; }"
            "QHeaderView::section { "
            "  background-color: #3c3c3c; color: #ffffff; "
            "  border: 1px solid #5a5a5a; "
            "}"
            "QTabWidget::pane { border: 1px solid #5a5a5a; }"
            "QTabBar::tab { background-color: #3c3c3c; color: #ffffff; }"
            "QTabBar::tab:selected { background-color: #4a4a4a; }"
            "QStatusBar { background-color: #2b2b2b; color: #ffffff; }"
            "QToolBar { background-color: #2b2b2b; border: none; }"
            "QScrollBar:vertical { background-color: #2b2b2b; }"
            "QScrollBar::handle:vertical { background-color: #5a5a5a; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
            "  background-color: #3c3c3c; "
            "}";
        setStyleSheet(styleSheet);
    } else {
        isDarkMode = false;
        darkModeAction->setChecked(false);
        setStyleSheet("");
    }
}

// Переключение темного режима
void MainWindow::toggleDarkMode()
{
    applyTheme(darkModeAction->isChecked() ? "Темная" : "Светлая");
    settings->setValue("theme", darkModeAction->isChecked() ? "Темная" : "Светлая");
}

// Переключение полноэкранного режима
void MainWindow::toggleFullscreen()
{
    isFullscreen = !isFullscreen;
    if (isFullscreen) {
        showFullScreen();
    } else {
        showMaximized();
    }
}

// Настройка горячих клавиш
void MainWindow::setupShortcuts()
{
    // Добавляем дополнительные горячие клавиши
    QShortcut *addShortcut = new QShortcut(QKeySequence("Ctrl+Shift+N"), this);
    connect(addShortcut, &QShortcut::activated, this, &MainWindow::addPerson);

    QShortcut *editShortcut = new QShortcut(QKeySequence("Ctrl+Shift+E"), this);
    connect(editShortcut, &QShortcut::activated, this, &MainWindow::editPerson);

    QShortcut *deleteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+D"), this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::deletePerson);

    QShortcut *fullscreenShortcut = new QShortcut(QKeySequence("F11"), this);
    connect(fullscreenShortcut, &QShortcut::activated, this, &MainWindow::toggleFullscreen);

    QShortcut *helpShortcut = new QShortcut(QKeySequence("F1"), this);
    connect(helpShortcut, &QShortcut::activated, this, &MainWindow::showHelp);

    QShortcut *refreshShortcut = new QShortcut(QKeySequence("F5"), this);
    connect(refreshShortcut, &QShortcut::activated, [this]() {
        refreshTree();
        refreshTable();
        refreshGraph();
        refreshInfo();
        showNotification("Обновление", "Интерфейс обновлен");
    });
}

// Настройка перетаскивания
void MainWindow::setupDragAndDrop()
{
    setAcceptDrops(true);
    treeWidget->setDragEnabled(true);
    treeWidget->setAcceptDrops(true);
    treeWidget->setDropIndicatorShown(true);
    treeWidget->setDragDropMode(QAbstractItemView::InternalMove);
}

// Обработка перетаскивания
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasText() || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

// Обработка падения
void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
                    importFromJson();
                } else if (filePath.endsWith(".jpg", Qt::CaseInsensitive) ||
                           filePath.endsWith(".png", Qt::CaseInsensitive)) {
                    // Добавляем фото
                    if (selectedPerson) {
                        selectedPerson->photoPath = filePath;
                        refreshInfo();
                        showNotification("Фото", "Фото добавлено");
                    }
                }
            }
        }
        event->acceptProposedAction();
    } else if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

// Контекстное меню
void MainWindow::showContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);

    QAction *addAction = new QAction("Добавить человека", this);
    connect(addAction, &QAction::triggered, this, &MainWindow::addPerson);
    contextMenu.addAction(addAction);

    QAction *editAction = new QAction("Редактировать", this);
    connect(editAction, &QAction::triggered, this, &MainWindow::editPerson);
    contextMenu.addAction(editAction);

    QAction *deleteAction = new QAction("Удалить", this);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deletePerson);
    contextMenu.addAction(deleteAction);

    contextMenu.addSeparator();

    QAction *addChildAction = new QAction("Добавить ребенка", this);
    connect(addChildAction, &QAction::triggered, this, &MainWindow::addChild);
    contextMenu.addAction(addChildAction);

    QAction *addParentAction = new QAction("Добавить родителя", this);
    connect(addParentAction, &QAction::triggered, this, &MainWindow::addParent);
    contextMenu.addAction(addParentAction);

    QAction *addSpouseAction = new QAction("Добавить супруга", this);
    connect(addSpouseAction, &QAction::triggered, this, &MainWindow::addSpouse);
    contextMenu.addAction(addSpouseAction);

    contextMenu.exec(treeWidget->mapToGlobal(pos));
}

// Копирование человека
void MainWindow::copyPerson()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Не выбран человек для копирования");
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
    QJsonObject json;
    json["id"] = selectedPerson->id;
    json["firstName"] = selectedPerson->firstName;
    json["lastName"] = selectedPerson->lastName;
    json["patronymic"] = selectedPerson->patronymic;
    json["birthDate"] = selectedPerson->birthDate;
    json["deathDate"] = selectedPerson->deathDate;
    json["gender"] = selectedPerson->gender;
    json["birthPlace"] = selectedPerson->birthPlace;
    json["occupation"] = selectedPerson->occupation;
    json["biography"] = selectedPerson->biography;
    json["photoPath"] = selectedPerson->photoPath;

    QJsonDocument doc(json);
    clipboard->setText(doc.toJson());

    showNotification("Копирование", "Данные скопированы в буфер обмена");
}

// Вставка человека
void MainWindow::pastePerson()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();

    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (doc.isNull()) {
        QMessageBox::warning(this, "Ошибка", "Неверные данные в буфере обмена");
        return;
    }

    QJsonObject json = doc.object();
    Person newPerson;
    newPerson.id = nextId++;
    newPerson.firstName = json["firstName"].toString();
    newPerson.lastName = json["lastName"].toString();
    newPerson.patronymic = json["patronymic"].toString();
    newPerson.birthDate = json["birthDate"].toString();
    newPerson.deathDate = json["deathDate"].toString();
    newPerson.gender = json["gender"].toString();
    newPerson.birthPlace = json["birthPlace"].toString();
    newPerson.occupation = json["occupation"].toString();
    newPerson.biography = json["biography"].toString();
    newPerson.photoPath = json["photoPath"].toString();
    newPerson.fatherId = -1;
    newPerson.motherId = -1;
    newPerson.spouseId = -1;

    persons[newPerson.id] = newPerson;

    refreshTree();
    refreshTable();
    refreshGraph();
    refreshInfo();
    updateUI();

    showNotification("Вставка", "Человек вставлен из буфера обмена");
}

// Увеличение масштаба
void MainWindow::zoomIn()
{
    zoomFactor *= 1.1;
    graphicsView->scale(1.1, 1.1);
    showNotification("Масштаб", "Увеличение: " + QString::number(zoomFactor * 100, 'f', 0) + "%");
}

// Уменьшение масштаба
void MainWindow::zoomOut()
{
    zoomFactor *= 0.9;
    graphicsView->scale(0.9, 0.9);
    showNotification("Масштаб", "Уменьшение: " + QString::number(zoomFactor * 100, 'f', 0) + "%");
}

// Сброс масштаба
void MainWindow::resetZoom()
{
    zoomFactor = 1.0;
    graphicsView->resetTransform();
    showNotification("Масштаб", "Масштаб сброшен");
}

// Режим дерева
void MainWindow::treeViewMode()
{
    currentViewMode = 0;
    centralTabWidget->setCurrentIndex(0);

    treeViewAction->setChecked(true);
    listViewAction->setChecked(false);
    timelineAction->setChecked(false);
}

// Режим таблицы
void MainWindow::listViewMode()
{
    currentViewMode = 1;
    centralTabWidget->setCurrentIndex(1);

    treeViewAction->setChecked(false);
    listViewAction->setChecked(true);
    timelineAction->setChecked(false);
}

// Режим графа
void MainWindow::timelineView()
{
    currentViewMode = 2;
    centralTabWidget->setCurrentIndex(2);

    treeViewAction->setChecked(false);
    listViewAction->setChecked(false);
    timelineAction->setChecked(true);
}

// Изменение текста поиска
void MainWindow::searchTextChanged(const QString &text)
{
    if (text.isEmpty()) {
        refreshTree();
        refreshTable();
        refreshGraph();
    } else {
        // Фильтруем дерево
        QTreeWidgetItem *root = treeWidget->invisibleRootItem();
        for (int i = 0; i < root->childCount(); ++i) {
            QTreeWidgetItem *item = root->child(i);
            bool visible = false;
            for (int col = 0; col < item->columnCount(); ++col) {
                if (item->text(col).contains(text, Qt::CaseInsensitive)) {
                    visible = true;
                    break;
                }
            }
            item->setHidden(!visible);
        }
    }
}

// Изменение фильтра
void MainWindow::filterChanged(int index)
{
    // Применяем фильтр
    if (proxyModel) {
        proxyModel->setFilterKeyColumn(index);
    }
}

// Завершение сетевого запроса
void MainWindow::networkReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        showNotification("Сетевая ошибка", reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        // Обработка полученных данных
        QJsonObject obj = doc.object();
        if (obj.contains("version")) {
            QString version = obj["version"].toString();
            showNotification("Обновление", "Доступна новая версия: " + version);
        }
    }

    reply->deleteLater();
}

// Закрытие окна
void MainWindow::closeEvent(QCloseEvent *event)
{
    // Сохраняем данные перед закрытием
    saveData();

    // Спрашиваем подтверждение
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Закрытие",
                                  "Вы уверены, что хотите закрыть программу?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Скрываем иконку из трея
        if (trayIcon) {
            trayIcon->hide();
        }
        event->accept();
    } else {
        event->ignore();
    }
}

// Обработка нажатий клавиш
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (isFullscreen) {
            toggleFullscreen();
        }
    }

    QMainWindow::keyPressEvent(event);
}

// Добавление фото
void MainWindow::addPhoto()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для добавления фото");
        return;
    }

    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Выберите фото",
                                                    "",
                                                    "Изображения (*.jpg *.jpeg *.png *.bmp *.gif)");

    if (filename.isEmpty()) {
        return;
    }

    selectedPerson->photoPath = filename;
    refreshInfo();

    // Добавляем в список фото
    photoListWidget->addItem(QFileInfo(filename).fileName());

    showNotification("Фото", "Фото добавлено для " + selectedPerson->fullName());
}

// Удаление фото
void MainWindow::removePhoto()
{
    if (!selectedPerson) {
        QMessageBox::warning(this, "Предупреждение", "Выберите человека для удаления фото");
        return;
    }

    if (selectedPerson->photoPath.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "У этого человека нет фото");
        return;
    }

    selectedPerson->photoPath.clear();
    refreshInfo();
    photoListWidget->clear();

    showNotification("Фото", "Фото удалено");
}

// Просмотр фото
void MainWindow::viewPhoto()
{
    if (!selectedPerson || selectedPerson->photoPath.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Нет фото для просмотра");
        return;
    }

    // Создаем диалог просмотра фото
    QDialog dialog(this);
    dialog.setWindowTitle("Фото - " + selectedPerson->fullName());
    dialog.resize(800, 600);

    QLabel *label = new QLabel(&dialog);
    QPixmap pixmap(selectedPerson->photoPath);
    if (pixmap.isNull()) {
        QMessageBox::warning(&dialog, "Ошибка", "Не удалось загрузить фото");
        return;
    }

    // Масштабируем фото с сохранением пропорций
    QPixmap scaled = pixmap.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(scaled);
    label->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(label);

    dialog.exec();
}

// Экспорт в CSV
void MainWindow::exportToCsv()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Экспорт в CSV",
                                                    "family_tree.csv",
                                                    "CSV файлы (*.csv)");

    if (filename.isEmpty()) {
        return;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл");
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    // Заголовки
    stream << "ID;Имя;Фамилия;Отчество;Пол;Дата рождения;Дата смерти;"
           << "Место рождения;Место смерти;Профессия;Отец;Мать;Супруг;Дети;Биография\n";

    // Данные
    for (const Person &person : persons) {
        stream << person.id << ";"
               << person.firstName << ";"
               << person.lastName << ";"
               << person.patronymic << ";"
               << person.gender << ";"
               << person.birthDate << ";"
               << person.deathDate << ";"
               << person.birthPlace << ";"
               << person.deathPlace << ";"
               << person.occupation << ";";

        // Родители
        if (person.fatherId != -1 && persons.contains(person.fatherId)) {
            stream << persons[person.fatherId].fullName();
        }
        stream << ";";
        if (person.motherId != -1 && persons.contains(person.motherId)) {
            stream << persons[person.motherId].fullName();
        }
        stream << ";";

        // Супруг
        if (person.spouseId != -1 && persons.contains(person.spouseId)) {
            stream << persons[person.spouseId].fullName();
        }
        stream << ";";

        // Дети
        QStringList childrenNames;
        for (int childId : person.childrenIds) {
            if (persons.contains(childId)) {
                childrenNames.append(persons[childId].fullName());
            }
        }
        stream << childrenNames.join(", ") << ";";

        // Биография
        stream << person.biography.replace("\n", " ") << "\n";
    }
    file.close();

    showNotification("Экспорт", "Данные экспортированы в CSV: " + filename);
}

// Полноэкранный режим (альтернативный метод)
//void MainWindow::zoomIn() { /* уже реализовано выше */ }
//void MainWindow::zoomOut() { /* уже реализовано выше */ }
//void MainWindow::resetZoom() { /* уже реализовано выше */ }

// Переключение вида
//void MainWindow::treeViewMode() { /* уже реализовано выше */ }
//void MainWindow::listViewMode() { /* уже реализовано выше */ }
//void MainWindow::timelineView() { /* уже реализовано выше */ }

// Изменение языка
void MainWindow::changeLanguage()
{
    // Здесь можно добавить поддержку переключения языка
    // Для этого необходимо использовать QTranslator
    showNotification("Язык", "Функция смены языка в разработке");
}

// Кастомизация темы
void MainWindow::customizeTheme()
{
    // Здесь можно добавить расширенные настройки темы
    // Например, выбор цветовой схемы
    showNotification("Тема", "Функция кастомизации темы в разработке");
}
