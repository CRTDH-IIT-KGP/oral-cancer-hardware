#include <QApplication>
#include <QThread>
#include <QMutex>
#include <QMessageBox>
#include <QColor>
#include <QLabel>
#include <QtDebug>
#include <QString>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>
#include <gpiod.h>

#include "LeptonThread.h"
#include "MyLabel.h"

// [MJPEG] Include the MJPEG streaming server
#include "MjpegHttpServer.h"

void printUsage(char *cmd) {
    char *cmdname = basename(cmd);
    printf("Usage: %s [OPTION]...\n"
           " -h      display this help and exit\n"
           " -cm x   select colormap\n"
           "           1 : rainbow\n"
           "           2 : grayscale\n"
           "           3 : ironblack [default]\n"
           " -tl x   select type of Lepton\n"
           "           2 : Lepton 2.x [default]\n"
           "           3 : Lepton 3.x\n"
           " -ss x   SPI bus speed [MHz] (10 - 30)\n"
           " -min x  override minimum value for scaling (0 - 65535)\n"
           " -max x  override maximum value for scaling (0 - 65535)\n"
           " -d x    log level (0-255)\n", cmdname);
}

int main(int argc, char **argv) {
    int typeColormap = 3;
    int typeLepton = 3;
    int spiSpeed = 25;
    int rangeMin = -1;
    int rangeMax = -1;
    int loglevel = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            loglevel = std::atoi(argv[++i]) & 0xFF;
        } else if (strcmp(argv[i], "-cm") == 0 && i + 1 < argc) {
            int val = std::atoi(argv[++i]);
            if (val == 1 || val == 2) typeColormap = val;
        } else if (strcmp(argv[i], "-tl") == 0 && i + 1 < argc) {
            int val = std::atoi(argv[++i]);
            if (val == 3) typeLepton = val;
        } else if (strcmp(argv[i], "-ss") == 0 && i + 1 < argc) {
            int val = std::atoi(argv[++i]);
            if (val >= 10 && val <= 30) spiSpeed = val;
        } else if (strcmp(argv[i], "-min") == 0 && i + 1 < argc) {
            int val = std::atoi(argv[++i]);
            if (val >= 0 && val <= 65535) rangeMin = val;
        } else if (strcmp(argv[i], "-max") == 0 && i + 1 < argc) {
            int val = std::atoi(argv[++i]);
            if (val >= 0 && val <= 65535) rangeMax = val;
        }
    }

    QApplication a(argc, argv);

    QWidget *myWidget = new QWidget;
    myWidget->setGeometry(0, 0, 800, 480);

    // Create placeholder image
    QImage myImage(320, 240, QImage::Format_RGB888);
    QRgb red = qRgb(255, 0, 0);
    for (int i = 0; i < 80; i++)
        for (int j = 0; j < 60; j++)
            myImage.setPixel(i, j, red);

    MyLabel myLabel(myWidget);
    myLabel.setGeometry(80, 0, 800, 480);
    myLabel.setPixmap(QPixmap::fromImage(myImage));

    //QPushButton *button1 = new QPushButton("Perform FFC", myWidget);
    //button1->setGeometry(200, 500, 200, 30);

    //QPushButton *captureButton = new QPushButton("Capture Image", myWidget);
    //captureButton->setGeometry(200, 600, 200, 30);

    LeptonThread *thread = new LeptonThread();
    thread->setLogLevel(loglevel);
    thread->useColormap(typeColormap);
    thread->useLepton(typeLepton);
    thread->useSpiSpeedMhz(spiSpeed);
    thread->setAutomaticScalingRange();
    if (rangeMin >= 0) thread->useRangeMinValue(rangeMin);
    if (rangeMax >= 0) thread->useRangeMaxValue(rangeMax);

    QObject::connect(thread, SIGNAL(updateImage(QImage)), &myLabel, SLOT(setImage(QImage)));
    //QObject::connect(button1, SIGNAL(clicked()), thread, SLOT(performFFC()));
    //QObject::connect(captureButton, SIGNAL(clicked()), thread, SLOT(ImageCapture()));
    //QObject::connect(captureButton, SIGNAL(clicked()), thread, SLOT(saveTemperatureDataToCSV()));

    // ========== [MJPEG] Start HTTP MJPEG server ==========
    MjpegHttpServer *mjpegServer = new MjpegHttpServer();
    if (!mjpegServer->start(8080)) {
        qWarning("Failed to start MJPEG server on port 8080");
    } else {
        qDebug("MJPEG server started on http://localhost:8080");
    }

    thread->setMjpegServer(mjpegServer);
    thread->start();

    // ========== GPIO Button Setup ==========
    const char *chipname = "gpiochip0";
    int line_num = 17;  // BCM GPIO 17

    gpiod_chip *chip = gpiod_chip_open_by_name(chipname);
    if (!chip) {
        qCritical("Failed to open GPIO chip");
        return -1;
    }

    gpiod_line *line = gpiod_chip_get_line(chip, line_num);
    if (!line) {
        qCritical("Failed to get GPIO line");
        gpiod_chip_close(chip);
        return -1;
    }

    struct gpiod_line_request_config config;
    config.consumer = "capture-button";
    config.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
    config.flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;

    if (gpiod_line_request(line, &config, 0) < 0) {
        qWarning("Could not request line with pull-up (falling back to default)");
        if (gpiod_line_request_input(line, "capture-button") < 0) {
            qCritical("Failed to request input for GPIO line");
            gpiod_chip_close(chip);
            return -1;
        }
    }

    QTimer *gpioPollTimer = new QTimer(myWidget);
    QObject::connect(gpioPollTimer, &QTimer::timeout, [=]() {
        int value = gpiod_line_get_value(line);
        static qint64 lastTrigger = 0;
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        if (value == 0 && now - lastTrigger > 500) {
            lastTrigger = now;
            qDebug() << "[GPIO] External button pressed!";
            QMetaObject::invokeMethod(thread, "captureBatchImagesAndTemperatureData", Qt::QueuedConnection, Q_ARG(int, 100));
        }
    });
    gpioPollTimer->start(100);

    myWidget->show();
    int ret = a.exec();

    // Cleanup
    gpioPollTimer->stop();
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    if (mjpegServer) mjpegServer->stop();

    return ret;
}
