#ifndef LIQUIDMAINWINDOW_H
#define LIQUIDMAINWINDOW_H

#include "sfzloader.h"

#include <QFile>
#include <QMainWindow>
#include <liquidsfz.hh>

#include <jack/jack.h>
#include <jack/midiport.h>

namespace Ui {
class LiquidMainWindow;
}

class LiquidMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LiquidMainWindow(QWidget *parent = nullptr);
    ~LiquidMainWindow();

protected:
    int process(jack_nframes_t nframes);

protected slots:
    void onLoadClicked();
    void onCancelClicked();
    void onCommitClicked();
    void onHelpAbout();
    void onPipeMessage(int);
    void onLoaderFinished();

private:
    Ui::LiquidMainWindow *ui;
    QString filename;
    QString pendingFilename;
    qint8 channel;
    LiquidSFZ::Synth synth;
    SFZLoader* loader;

    jack_client_t* jack_client;
    jack_port_t* jack_midi_in;
    jack_port_t* jack_audio_l;
    jack_port_t* jack_audio_r;
};

#endif // LIQUIDMAINWINDOW_H
