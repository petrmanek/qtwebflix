#ifndef NETFLIXMPRISINTERFACE_H
#define NETFLIXMPRISINTERFACE_H

#include <mutex>
#include <functional>

#include <QTimer>

#include "mprisinterface.h"

class MainWindow;

class NetflixMprisInterface : public MprisInterface {
  Q_OBJECT

public:
  explicit NetflixMprisInterface(QWidget *parent = nullptr);

  virtual void setup(MainWindow *window) override;

  void updatePlayerFullScreen();

private slots:
  // slots for handlers of hotkeys
  void playVideo();
  void pauseVideo();
  void togglePlayPause();
  void setVideoVolume(double volume);
  void setFullScreen(bool fullscreen);

  void playerStateTimerFired();
  void playerPositionTimerFired();
  void metadataTimerFired();
  void volumeTimerFired();

  void networkManagerFinished(QNetworkReply *reply);

private:
  QTimer playerStateTimer;
  QTimer playerPositionTimer;
  QTimer metadataTimer;
  QTimer volumeTimer;
  QNetworkAccessManager networkManager;
  QString prevTitleId;
  QString prevArtUrl;
  std::mutex mtx_titleInfo;
  bool titleInfoFetching;

  QString getArtUrl(const QString& nid);

  void getVideoState(std::function<void(Mpris::PlaybackStatus)> callback);
  void getVideoPosition(std::function<void(qlonglong)> callback);
  void getMetadata(std::function<void(qlonglong, const QString&, const QString&)> callback);
  void getVolume(std::function<void(double)> callback);

};

#endif // NETFLIXMPRISINTERFACE_H
