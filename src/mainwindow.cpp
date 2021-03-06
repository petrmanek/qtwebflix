#include <QContextMenuEvent>
#include <QDebug>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>
#include <QWidget>

#include "mainwindow.h"
#include "commandlineparser.h"
#include "ui_mainwindow.h"
#include "urlrequestinterceptor.h"
#include "mprisinterface.h"
#include "dummymprisinterface.h"
#include "netflixmprisinterface.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), mprisType(typeid(DummyMprisInterface)), mpris(new DummyMprisInterface)
{
  QWebEngineSettings::globalSettings()->setAttribute(
      QWebEngineSettings::PluginsEnabled, true);
  appSettings = new QSettings("Qtwebflix", "Save State", this);

  // set playbackrate and read jquery file
  playRate = 1.0;
  playRateStr = QString::number(playRate);
  QFile file;
  file.setFileName(":/jquery.min.js");
  file.open(QIODevice::ReadOnly);
  jQuery = file.readAll();
  jQuery.append("\nvar qt = { 'jQuery': jQuery.noConflict(true) };");
  file.close();

  ui->setupUi(this);
  this->setWindowTitle("QtWebFlix");
  readSettings();
  webview = new QWebEngineView;
  ui->horizontalLayout->addWidget(webview);

  if (appSettings->value("site").toString() == "") {
    webview->setUrl(QUrl(QStringLiteral("http://netflix.com")));

  } else {
    webview->setUrl(QUrl(appSettings->value("site").toString()));
  }
  webview->settings()->setAttribute(
      QWebEngineSettings::FullScreenSupportEnabled, true);
// Check for QT if equal or greater than 5.10 hide scrollbars
#if HAS_SCROLLBAR
  webview->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
#endif

  // connect handler for fullscreen press on video
  connect(webview->page(), &QWebEnginePage::fullScreenRequested, this,
          &MainWindow::fullScreenRequested);

  // Check if user is using arm processor(Raspberry pi)
  QString UserAgent = this->webview->page()->profile()->httpUserAgent();
  // qDebug() << UserAgent;
  // qDebug() << "Changing user agent to Firefox 57";
  // Testing User Agent
  // UserAgent ="Mozilla/5.0 (X11; Linux x86_64; rv:57.0) Gecko/20100101
  // Firefox/57.0";
  // this->webview->page()->profile()->setHttpUserAgent(UserAgent);
  // qDebug() << UserAgent.contains("Linux arm");
  if (UserAgent.contains("Linux arm")) {
    qDebug() << "Changing user agent for raspberry pi users";
    QString UserAgent = "Mozilla/5.0 (X11; CrOS armv7l 6946.86.0) "
                        "AppleWebKit/537.36 (KHTML, like Gecko) "
                        "Chrome/51.0.2704.91 Safari/537.36";
    this->webview->page()->profile()->setHttpUserAgent(UserAgent);
  }

  // key short cuts

  // F11
  keyF11 = new QShortcut(this); // Initialize the object
  keyF11->setKey(Qt::Key_F11);  // Set the key code
  // connect handler to keypress
  connect(keyF11, SIGNAL(activated()), this, SLOT(slotShortcutF11()));

  // Ctrl + Q
  keyCtrlQ = new QShortcut(this);         // Initialize the object
  keyCtrlQ->setKey(Qt::CTRL + Qt::Key_Q); // Set the key code
  // connect handler to keypress
  connect(keyCtrlQ, SIGNAL(activated()), this, SLOT(slotShortcutCtrlQ()));

  // Ctrl + W
  keyCtrlW = new QShortcut(this);
  keyCtrlW->setKey(Qt::CTRL + Qt::Key_W); // Set the key code
  connect(keyCtrlW, SIGNAL(activated()), this, SLOT(slotShortcutCtrlW()));

  // Ctrl + S
  keyCtrlS = new QShortcut(this);
  keyCtrlS->setKey(Qt::CTRL + Qt::Key_S); // Set the key code
  connect(keyCtrlS, SIGNAL(activated()), this, SLOT(slotShortcutCtrlS()));

  // Ctrl + R
  keyCtrlR = new QShortcut(this);
  keyCtrlR->setKey(Qt::CTRL + Qt::Key_R); // Set the key code
  connect(keyCtrlR, SIGNAL(activated()), this, SLOT(slotShortcutCtrlR()));


  // Ctrl + F5
  keyCtrlF5 = new QShortcut(this);
  keyCtrlF5->setKey(Qt::CTRL + Qt::Key_F5); // Set the key code
  connect(keyCtrlF5, SIGNAL(activated()), this, SLOT(slotShortcutCtrlF5()));

  // Connect finished loading boolean
  connect(webview, &QWebEngineView::loadFinished, this,
          &MainWindow::finishLoading);

  // Window size settings
  QSettings settings;
  restoreState(settings.value("mainWindowState").toByteArray());

  webview->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(webview, SIGNAL(customContextMenuRequested(const QPoint &)), this,
          SLOT(ShowContextMenu(const QPoint &)));

  mpris->setup(this);
}

MainWindow::~MainWindow() { delete ui; }

// Slot handler of F11
void MainWindow::slotShortcutF11() {
  /* This handler will make switching applications in full screen mode
   * and back to normal window mode
   * */
  this->setFullScreen(!this->isFullScreen());
}

QWebEngineView * MainWindow::webView() const {
  return webview;
}

// Slot handler for Ctrl + Q
void MainWindow::slotShortcutCtrlQ() {
  writeSettings();
  QApplication::quit();
}

void MainWindow::finishLoading(bool) {
  webview->page()->runJavaScript(jQuery);
  exchangeMprisInterfaceIfNeeded();
}

void MainWindow::exchangeMprisInterfaceIfNeeded() {
  QString hostname = webview->url().host();

  if (hostname.endsWith("netflix.com")) {
    setMprisInterface<NetflixMprisInterface>();
  } else {
    setMprisInterface<DummyMprisInterface>();
  }
}

// Slot handler for Ctrl + W
void MainWindow::slotShortcutCtrlW() {
  QString getPlayer =
      ("var netflix = document.getElementsByClassName('ellipsize-text')[0];");
  webview->page()->runJavaScript(getPlayer);
  if (playRate >= 2) {
    return;
  }
  playRate += .1;
  playRateStr = QString::number(playRate);
  // QString code = QStringLiteral("qt.jQuery('video').get(0).playbackRate =")
  QString code =
      QStringLiteral("document.querySelector('video').playbackRate = ")
          .append(playRateStr);
  QString setSpeedText = QStringLiteral("var y = document.createTextNode('")
                             .append(playRateStr)
                             .append(" X');");

  QString replaceText = ("netflix.replaceChild(y, netflix.childNodes[3])");
  QString addTextToPlayer = ("netflix.appendChild(y);");
  QString addTextCode = (setSpeedText + addTextToPlayer + replaceText);
  qDebug() << "Player Speed set to: " << playRateStr;
  webview->page()->runJavaScript(code);
  webview->page()->runJavaScript(addTextCode);
}

// Slot handler for Ctrl + S
void MainWindow::slotShortcutCtrlS() {

  QString getPlayer =
      ("var netflix = document.getElementsByClassName('ellipsize-text')[0];");
  webview->page()->runJavaScript(getPlayer);
  if (playRate < 0.2) {
    return;
  }
  playRate -= .1;
  playRateStr = QString::number(playRate);
  QString code =
      QStringLiteral("document.querySelector('video').playbackRate = ")
          .append(playRateStr);
  QString setSpeedText = QStringLiteral("var y = document.createTextNode('")
                             .append(playRateStr)
                             .append(" X');");

  QString replaceText = ("netflix.replaceChild(y, netflix.childNodes[3])");
  QString addTextToPlayer = ("netflix.appendChild(y);");
  QString addTextCode = (setSpeedText + addTextToPlayer + replaceText);
  qDebug() << "Player Speed set to: " << playRateStr;
  webview->page()->runJavaScript(code);
  webview->page()->runJavaScript(addTextCode);
}

// Slot handler for Ctrl + R
void MainWindow::slotShortcutCtrlR() {
  webview->page()->runJavaScript(jQuery);
  if (playRate != 1.0) {
    playRate = 1.0;
    playRateStr = QString::number(playRate);
    QString code = QStringLiteral("qt.jQuery('video').get(0).playbackRate =")
                       .append(playRateStr);
    QString setSpeedText = QStringLiteral("var y = document.createTextNode('")
                               .append(playRateStr)
                               .append(" X');");

    QString replaceText = ("netflix.replaceChild(y, netflix.childNodes[3])");
    QString addTextToPlayer = ("netflix.appendChild(y);");
    QString addTextCode = (setSpeedText + addTextToPlayer + replaceText);
    qDebug() << "Player Speed set to: " << playRateStr;
    webview->page()->runJavaScript(code);
    webview->page()->runJavaScript(addTextCode);
  }
}

void MainWindow::slotShortcutCtrlF5() {
     webview->triggerPageAction(QWebEnginePage::ReloadAndBypassCache);
}

void MainWindow::setFullScreen(bool fullscreen) {
  if (!fullscreen) {
    this->showNormal();
  } else {
    this->showFullScreen();
  }
  mpris->updatePlayerFullScreen();
}

void MainWindow::closeEvent(QCloseEvent *) {
  // This will be called whenever this window is closed.
  writeSettings();
}

void MainWindow::writeSettings() {
  // Write the values to disk in categories.
  appSettings->setValue("state/mainWindowState", saveState());
  appSettings->setValue("geometry/mainWindowGeometry", saveGeometry());
  QString site = webview->url().toString();
  appSettings->setValue("site", site);
  qDebug() << " write settings:" << site;
}

void MainWindow::restore() {

  QByteArray stateData =
      appSettings->value("state/mainWindowState").toByteArray();

  QByteArray geometryData =
      appSettings->value("geometry/mainWindowGeometry").toByteArray();

  restoreState(stateData);
  restoreGeometry(geometryData);
}

void MainWindow::readSettings() { restore(); }

void MainWindow::fullScreenRequested(QWebEngineFullScreenRequest request) {

  // fullscreen on video players

  if (request.toggleOn() && !this->isFullScreen()) {
    this->showFullScreen();
  } else {
    this->showNormal();
  }
  mpris->updatePlayerFullScreen();
  request.accept();
}

void MainWindow::ShowContextMenu(const QPoint &pos) // this is a slot
{

  QPoint globalPos = webview->mapToGlobal(pos);
  provSettings = new QSettings("Qtwebflix", "Providers", this);
  provSettings->setIniCodec("UTF-8");
  provSettings->beginGroup("providers");
  QString conf(provSettings->fileName());

  // Check if config file exists,if not create a default key.
  if (!QFile::exists(conf))

  {
    qDebug() << "Config file does not exist, creating default";
    provSettings->setValue("netflix", "http://netflix.com");
    provSettings->sync();
  }

  QStringList keys = provSettings->allKeys();

  QMenu myMenu;
  for (const auto &i : keys) {
    // qDebug() << "keys" << i;

    if (!i.startsWith("#")) {
      myMenu.addAction(i);
      myMenu.addSeparator();
    }
  }

  QAction *selectedItem = myMenu.exec(globalPos);

  if (selectedItem == nullptr) {
    return;
  } else if (selectedItem) {
    QString url = provSettings->value(selectedItem->text()).toString();
    qDebug() << "URL is :" << url;
    webview->setUrl(QUrl(url));
    provSettings->endGroup();
  }

  else {
    // nothing was chosen
  }
}

void MainWindow::parseCommand() {
  // create parser object and get arguemts
  Commandlineparser parser;

  // check if argument is used and set provider
  if (parser.providerIsSet()) {
    if (parser.getProvider() == "") {
      qDebug() << "site is invalid reditecting to netflix.com";
      webview->setUrl(QUrl(QStringLiteral("http://netflix.com")));
    } else if (parser.getProvider() != "") {
      qDebug() << "site is set to" << parser.getProvider();
      webview->setUrl(QUrl::fromUserInput(parser.getProvider()));
    }
  }

  // check if argument is used and set useragent
  if (parser.userAgentisSet()) {
    qDebug() << "Changing useragent to :" << parser.getUserAgent();
    this->webview->page()->profile()->setHttpUserAgent(parser.getUserAgent());
  }
  if (!parser.nonHDisSet()) {
    this->m_interceptor = new UrlRequestInterceptor;
    this->webview->page()->profile()->setRequestInterceptor(
        this->m_interceptor);
  }
}
