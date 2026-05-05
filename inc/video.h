#ifndef VIDEO_H
#define VIDEO_H

#include <QThread>
#include <QImage>

class Video : public QThread
{
    Q_OBJECT
public:
    explicit Video(QObject *parent = nullptr);
    bool open(const QString &url);
    void stop();

signals:
    void frameReady(const QImage &img);

protected:
    void run() override;

private:
    QString m_url;
    bool m_isPlaying;
};

#endif