#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QListWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileDialog>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QPainter>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlTableModel>
#include <QSortFilterProxyModel>
#include <QtSql/QSqlError>
#include <QProgressDialog>
#include <QThread>
#include <QtConcurrent/QtConcurrent>
#include <QUndoStack>
#include <QUndoView>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include <QClipboard>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QDateTime>
#include <QCryptographicHash>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QUrl>
#include <QDesktopServices>

// Класс для представления человека в генеалогическом дереве
class Person {
public:
    int id;                     // Уникальный идентификатор
    QString firstName;          // Имя
    QString lastName;           // Фамилия
    QString patronymic;         // Отчество
    QString birthDate;          // Дата рождения
    QString deathDate;          // Дата смерти
    QString gender;             // Пол
    QString birthPlace;         // Место рождения
    QString deathPlace;         // Место смерти
    QString occupation;         // Профессия
    QString biography;          // Биография
    QString photoPath;          // Путь к фото
    int fatherId;               // ID отца
    int motherId;               // ID матери
    int spouseId;               // ID супруга/и
    QList<int> childrenIds;     // Список ID детей

    Person() : id(-1), fatherId(-1), motherId(-1), spouseId(-1) {}

    // Полное имя
    QString fullName() const {
        return lastName + " " + firstName + (patronymic.isEmpty() ? "" : " " + patronymic);
    }
};

// Основной класс главного окна
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слоты для работы с деревом
    void addPerson();
    void editPerson();
    void deletePerson();
    void searchPerson();
    void clearSearch();
    void sortTree();
    //void exportToPdf(QString &fileName);
    //void importFromJson(QString &fileName);
    //void exportToJson(QString &fileName);
    void printTrees();
    void printTree();
    void showPersonInfo();
    void addChild();
    void addParent();
    void addSpouse();
    void removeRelationship();
    void findPath();
    void showAncestors();
    void showDescendants();
    void showSiblings();
    void generateReport();
    void statistics();
    void backupDatabase();
    void restoreDatabase();
    void settingsDialog();
    void about();
    void undo();
    void redo();
    void copyPerson();
    void pastePerson();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void toggleFullscreen();
    void treeViewMode();
    void listViewMode();
    void timelineView();
    void mapView();
    void photoGallery();
    void familyTree();
    void searchByDate();
    void searchByPlace();
    void advancedSearch();
    void filterByGender();
    void filterByAge();
    void exportToCsv();
    void exportToExcel();
    void shareToSocialMedia();
    void createFamilyGroup();
    void addPhoto();
    void removePhoto();
    void viewPhoto();
    void autoLayout();
    void manualLayout();
    void saveLayout();
    void loadLayout();
    void validateData();
    void mergeDuplicates();
    void generateFamilyCode();
    void createTimeline();
    void birthdayReminder();
    void anniversaryReminder();
    void sendNotification();
    void toggleDarkMode();
    void changeLanguage();
    void customizeTheme();
    void showHelp();

private:
    // Основные виджеты
    QSplitter *mainSplitter;                 // Главный разделитель
    QTabWidget *centralTabWidget;            // Центральный таб виджет
    QTreeWidget *treeWidget;                 // Древовидное представление
    QTableWidget *tableWidget;               // Табличное представление
    QGraphicsView *graphicsView;             // Графическое представление
    QGraphicsScene *graphicsScene;           // Сцена для графики
    QTextEdit *infoTextEdit;                 // Текстовое поле для информации
    QLineEdit *searchLineEdit;               // Поле поиска
    QComboBox *searchComboBox;               // Комбобокс для фильтрации
    QListWidget *photoListWidget;            // Список фото
    QDateEdit *birthDateEdit;                // Редактор даты рождения
    QDateEdit *deathDateEdit;                // Редактор даты смерти
    QTextEdit *biographyTextEdit;            // Редактор биографии

    // Кнопки и элементы управления
    QPushButton *addButton;                  // Кнопка добавления
    QPushButton *editButton;                 // Кнопка редактирования
    QPushButton *deleteButton;               // Кнопка удаления
    QPushButton *searchButton;               // Кнопка поиска
    QPushButton *clearButton;                // Кнопка очистки
    QPushButton *printButton;                // Кнопка печати
    QPushButton *exportButton;               // Кнопка экспорта
    QPushButton *importButton;               // Кнопка импорта
    QPushButton *undoButton;                 // Кнопка отмены
    QPushButton *redoButton;                 // Кнопка повтора
    QPushButton *zoomInButton;               // Кнопка увеличения
    QPushButton *zoomOutButton;              // Кнопка уменьшения
    QPushButton *resetZoomButton;            // Кнопка сброса масштаба
    QPushButton *fullscreenButton;           // Кнопка полноэкранного режима

    // Действия и меню
    QAction *addAction;                      // Действие добавления
    QAction *editAction;                     // Действие редактирования
    QAction *deleteAction;                   // Действие удаления
    QAction *searchAction;                   // Действие поиска
    QAction *printAction;                    // Действие печати
    QAction *exportAction;                   // Действие экспорта
    QAction *importAction;                   // Действие импорта
    QAction *undoAction;                     // Действие отмены
    QAction *redoAction;                     // Действие повтора
    QAction *copyAction;                     // Действие копирования
    QAction *pasteAction;                    // Действие вставки
    QAction *settingsAction;                 // Действие настроек
    QAction *aboutAction;                    // Действие "О программе"
    QAction *helpAction;                     // Действие помощи
    QAction *exitAction;                     // Действие выхода
    QAction *darkModeAction;                 // Действие темного режима
    QAction *treeViewAction;                 // Действие древовидного вида
    QAction *listViewAction;                 // Действие спискового вида
    QAction *timelineAction;                 // Действие временной линии
    QAction *mapAction;                      // Действие карты
    QAction *galleryAction;                  // Действие галереи
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *resetZoomAction;
    QAction *fullscreenAction;
    QAction *exportCsvAction;
    QAction *openAction;

    // Данные
    QMap<int, Person> persons;               // Карта всех людей
    int nextId;                              // Следующий доступный ID
    QSqlDatabase db;                         // База данных SQLite
    QSqlTableModel *model;                   // Модель таблицы
    QSortFilterProxyModel *proxyModel;       // Прокси-модель для фильтрации
    QUndoStack *undoStack;                   // Стек отмены действий
    QSettings *settings;                     // Настройки приложения
    QSystemTrayIcon *trayIcon;               // Иконка в системном трее
    QNetworkAccessManager *networkManager;   // Менеджер сетевых запросов
    QTimer *autoSaveTimer;                   // Таймер автосохранения

    // Переменные состояния
    double zoomFactor;                       // Коэффициент масштабирования
    bool isDarkMode;                         // Темный режим
    bool isFullscreen;                       // Полноэкранный режим
    int currentViewMode;                     // Текущий режим отображения
    Person *selectedPerson;                  // Выбранный человек

    // Приватные методы
    void setupUI();                          // Настройка интерфейса
    void createMenuBar();                    // Создание меню
    void createToolBar();                    // Создание панели инструментов
    void createStatusBar();                  // Создание строки состояния
    void createSystemTray();                 // Создание системного трея
    void setupDatabase();                    // Настройка базы данных
    void loadData();                         // Загрузка данных
    void saveData();                         // Сохранение данных
    void updateUI();                         // Обновление интерфейса
    void refreshTree();                      // Обновление дерева
    void refreshTable();                     // Обновление таблицы
    void refreshGraph();                     // Обновление графика
    void refreshInfo();                      // Обновление информации
    void buildTree(Person *person, QTreeWidgetItem *parent); // Построение дерева
    void drawPerson(Person *person, qreal x, qreal y); // Отрисовка человека
    void drawRelationship(Person *parent, Person *child); // Отрисовка связи
    void calculateLayout(Person *person, qreal &x, qreal &y); // Расчет макета
    void autoSave();                         // Автосохранение
    void showNotification(QString title, QString message); // Показ уведомления
    void logAction(QString action);          // Логирование действий
    bool validatePerson(Person &person);     // Валидация данных
    void findPersonInTree(int id, QTreeWidgetItem *parent, QTreeWidgetItem **result); // Поиск в дереве
    void exportToPdf();      // Экспорт в PDF
    void importFromJson();   // Импорт из JSON
    void exportToJson();     // Экспорт в JSON
    void generateFamilyCode(Person *person); // Генерация семейного кода
    void createTimeline(Person *person);     // Создание временной линии
    void checkBirthdays();                   // Проверка дней рождения
    void checkAnniversaries();               // Проверка годовщин
    void sendPushNotification(QString title, QString message); // Отправка push-уведомления
    void applyTheme(QString theme);          // Применение темы
    void setupShortcuts();                   // Настройка горячих клавиш
    void setupDragAndDrop();                 // Настройка перетаскивания
    void showContextMenu(const QPoint &pos); // Контекстное меню
    void personSelected(QTreeWidgetItem *item, int column); // Выбор человека
    void tableSelectionChanged();            // Изменение выделения в таблице
    void searchTextChanged(const QString &text); // Изменение текста поиска
    void filterChanged(int index);           // Изменение фильтра
    void dateFilterChanged();                // Изменение фильтра по дате
    void networkReplyFinished(QNetworkReply *reply); // Завершение сетевого запроса

    // Переопределенные методы
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // MAINWINDOW_H
