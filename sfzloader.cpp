#include <QMutex>
#include <QMutexLocker>
#include "sfzloader.h"

SFZLoader::SFZLoader(LiquidSFZ::Synth *synth, QMutex* mutex, QObject *parent) :
    QThread (parent),
    synth(synth),
    mutex(mutex)
{
}

void SFZLoader::setSFZ(const QString &filename)
{
    this->filename = filename;
}

void SFZLoader::run()
{
    if (!this->synth || this->filename.isEmpty())
        exit(-1);

    QMutexLocker locker(this->mutex);
    exit(this->synth->load(this->filename.toStdString()));
}
