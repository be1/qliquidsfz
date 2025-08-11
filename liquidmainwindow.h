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
    void loadFile(const QString& filename);

signals:
    void logEvent(const QString&);
    void progressEvent(int percent);
    void handleNote(bool doHandle, int chan);

protected:
    int process(jack_nframes_t nframes);

protected slots:
    void onLoadTriggered();
    void onClearLogTriggered();
    void onCommitChan();
    void onCommitLoad();
    void onHelpAbout();
    void onLoaderFinished();
    void onHandleNote(bool on, int chan);
    void onLogEvent(const QString& message);
    void onProgressEvent(int progress);
    void onGainValueChanged(int val);

private:
    Ui::LiquidMainWindow *ui;
    QString filename;
    QString pendingFilename;
    qint8 channel;
    QMutex mutex;
    LiquidSFZ::Synth synth;
    SFZLoader* loader;
    QQueue<QPair<int, int>>* cc_queue;

    jack_client_t* jack_client;
    jack_port_t* jack_midi_in;
    jack_port_t* jack_audio_l;
    jack_port_t* jack_audio_r;
    bool last_on; /* last note on or off */
    float gain, _gain;
};

#endif // LIQUIDMAINWINDOW_H
