#include "sfzloader.h"

SFZLoader::SFZLoader(LiquidSFZ::Synth *synth, QObject *parent) :
    QThread (parent),
    synth(synth)
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

    exit(this->synth->load(this->filename.toStdString()));
}
