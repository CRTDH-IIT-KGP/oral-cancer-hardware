#ifndef TEXTTHREAD
#define TEXTTHREAD

#include <ctime>
#include <stdint.h>

#include <QThread>
#include <QtCore>
#include <QPixmap>
#include <QImage>

// [MJPEG] Include MJPEG HTTP server header
#include "MjpegHttpServer.h"

#define PACKET_SIZE 164
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)
#define PACKETS_PER_FRAME 120
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16*PACKETS_PER_FRAME)

class LeptonThread : public QThread
{
  Q_OBJECT;

public:
  LeptonThread();
  ~LeptonThread();

  void setLogLevel(uint16_t);
  void useColormap(int);
  void useLepton(int);
  void useSpiSpeedMhz(unsigned int);
  void setAutomaticScalingRange();
  void useRangeMinValue(uint16_t);
  void useRangeMaxValue(uint16_t);
  void run();
  int mapRGBToTemperature(int r, int g, int b);
  QString getUniqueFileName(const QString& baseName);
  QString getUniqueDataFolderName();

  // [MJPEG] Setter to inject MJPEG server
  void setMjpegServer(MjpegHttpServer *server) { mjpegServer = server; }

public slots:
  void performFFC();
  void captureBatchImagesAndTemperatureData(int count);
  void ImageCapture(const QString& imageDirPath);
  void saveTemperatureDataToCSV(const QString& tempDirPath);

signals:
  void updateText(QString);
  void updateImage(QImage);

private:
  void log_message(uint16_t, std::string);
  void initializeRGBToTemperatureMap();
  void extractTemperatureData(QVector<int>& temperatureData);

  uint16_t loglevel;
  int typeColormap;
  const int *selectedColormap;
  int selectedColormapSize;
  int typeLepton;
  unsigned int spiSpeed;
  bool autoRangeMin;
  bool autoRangeMax;
  uint16_t rangeMin;
  uint16_t rangeMax;
  int myImageWidth;
  int myImageHeight;
  QImage myImage;
  QMap<QRgb, int> rgbToTemperatureMap;

  uint8_t result[PACKET_SIZE*PACKETS_PER_FRAME];
  uint8_t shelf[4][PACKET_SIZE*PACKETS_PER_FRAME];
  uint16_t *frameBuffer;

  // [MJPEG] MJPEG server pointer
  MjpegHttpServer *mjpegServer = nullptr;
};

#endif
