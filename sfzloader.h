#ifndef SFZLOADER_H
#define SFZLOADER_H

#include <QObject>
#include <QThread>
#include <liquidsfz.hh>

class SFZLoader : public QThread
{
    Q_OBJECT
public:
    explicit SFZLoader(LiquidSFZ::Synth* synth, QObject *parent = nullptr);
    void setSFZ(const QString& filename);

protected:
    void run();

private:
    LiquidSFZ::Synth* synth;
    QString filename;
};

#endif // SFZLOADER_H
