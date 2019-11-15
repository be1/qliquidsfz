#include "liquidmainwindow.h"
#include "ui_liquidmainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSocketNotifier>
#include <liquidsfz.hh>
#include <jack/jack.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "config.h"

LiquidMainWindow::LiquidMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LiquidMainWindow),
    filename(""),
    pendingFilename(""),
    channel(0),
    loader(nullptr),
    jack_client(nullptr),
    jack_midi_in(nullptr),
    jack_audio_l(nullptr),
    jack_audio_r(nullptr)
{
    ui->setupUi(this);

    int pipeEnds[2];
    ::pipe(pipeEnds);
    ::dup2(pipeEnds[1], STDOUT_FILENO);
    ::close(pipeEnds[1]);

    QSocketNotifier* socketNotifier = new QSocketNotifier(pipeEnds[0], QSocketNotifier::Read, this);
    QObject::connect(socketNotifier, SIGNAL(activated(int)), this, SLOT(onPipeMessage(int)));

    QObject::connect(ui->sfzLoadButton, SIGNAL(clicked()), this, SLOT(onLoadClicked()));
    QObject::connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
    QObject::connect(ui->commitButton, SIGNAL(clicked()), this, SLOT(onCommitClicked()));
    QObject::connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(onHelpAbout()));

    jack_status_t jack_status;
    this->jack_client = jack_client_open ("qliquidsfz", JackNullOption, &jack_status, NULL);
    if (!this->jack_client) {
        this->ui->statusBar->showMessage("jack_client_open() failed: " + QString::number(jack_status));
        if (jack_status & JackServerFailed) {
            this->ui->statusBar->showMessage("unable to connect to JACK server");
        }

        return;
    }
    if (jack_status & JackServerStarted) {
        this->ui->statusBar->showMessage("JACK server started");
    }

    char* name = jack_get_client_name(this->jack_client);
    this->setWindowTitle(QString(name));

    this->synth.set_sample_rate(jack_get_sample_rate(this->jack_client));

    this->jack_midi_in = jack_port_register(this->jack_client, "input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    this->jack_audio_l = jack_port_register(this->jack_client, "output_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    this->jack_audio_r = jack_port_register(this->jack_client, "output_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    jack_set_process_callback(this->jack_client, [](jack_nframes_t nframes, void* arg) {
        auto self = static_cast<LiquidMainWindow *>(arg);
        return self->process(nframes);
    }, this);

    jack_activate(this->jack_client);
}

LiquidMainWindow::~LiquidMainWindow()
{
    jack_deactivate(this->jack_client);
    delete ui;
}

int LiquidMainWindow::process(jack_nframes_t nframes)
{
    void* port_buf = jack_port_get_buffer (this->jack_midi_in, nframes);
    jack_nframes_t event_count = jack_midi_get_event_count (port_buf);

    for (jack_nframes_t event_index = 0; event_index < event_count; event_index++) {
        jack_midi_event_t in_event;
        jack_midi_event_get (&in_event, port_buf, event_index);
        if (in_event.size == 3) {
            int channel = in_event.buffer[0] & 0x0f;
            if (channel && this->channel && (channel != this->channel))
                continue;

            switch (in_event.buffer[0] & 0xf0) {
                case 0x90: synth.add_event_note_on (in_event.time, channel, in_event.buffer[1], in_event.buffer[2]);
                           break;
                case 0x80: synth.add_event_note_off (in_event.time, channel, in_event.buffer[1]);
                           break;
                case 0xb0: synth.add_event_cc (in_event.time, channel, in_event.buffer[1], in_event.buffer[2]);
                           break;
              }
          }
      }

      float *outputs[2] = {
          (float *) jack_port_get_buffer (this->jack_audio_l, nframes),
          (float *) jack_port_get_buffer (this->jack_audio_r, nframes)
      };
      synth.process (outputs, nframes);
      return 0;
}

void LiquidMainWindow::onLoadClicked()
{
    this->pendingFilename = QFileDialog::getOpenFileName(this, QObject::tr("Open SFZ"), "", QObject::tr("SFZ Files (*.sfz)"));
    if (pendingFilename.isEmpty())
        return;

    QFileInfo info(this->pendingFilename);
    ui->sfzLoadButton->setText(info.baseName());
}

void LiquidMainWindow::onCancelClicked()
{
    ui->sfzLoadButton->setText(QObject::tr("Load..."));
    QFileInfo info(this->filename);
    ui->sfzFilelabel->setText(info.baseName());

    ui->midiChannelSpinBox->setValue(this->channel +1);
}

void LiquidMainWindow::onCommitClicked()
{
    // load SFZ file in a separate thread.
    if (this->filename != this->pendingFilename) {
        this->loader = new SFZLoader(&this->synth, this);
        QObject::connect(this->loader, SIGNAL(finished()), this, SLOT(onLoaderFinished()));

        this->filename = this->pendingFilename;
        this->loader->setSFZ(this->filename);
        this->loader->start();

        ui->sfzLoadButton->setText(QObject::tr("Load..."));
        QFileInfo info(this->filename);
        ui->sfzFilelabel->setText(info.baseName());
    }

    if (this->channel != (ui->midiChannelSpinBox->value() - 1)) {
        this->channel = (qint8) ui->midiChannelSpinBox->value() -1;
        ui->midiChannelLabel->setText(QString::number(ui->midiChannelSpinBox->value()));
    }
}

void LiquidMainWindow::onHelpAbout()
{
    QMessageBox::about(this, QObject::tr("About QLiquidSFZ"), "QLiquidSFZ version " VERSION "\n\n"
                                                              "This program is a GUI to the liquidsfz library, an SFZ synth player.\n\n"
                                                              "This program is free software, Copyright (C) Benoit Rouits <brouits@free.fr> "
                                                              "and released under the GNU General Public Lisence version 3. It is delivered "
                                                              "AS IS in the hope it can be useful, but with no warranty of correctness.");
}

void LiquidMainWindow::onPipeMessage(int fd)
{
    size_t bytes = 0;
    if (::ioctl(fd, FIONREAD, &bytes) == -1)
        return;

    if (!bytes)
        return;

    char buf[bytes +1];
    ssize_t r = ::read(fd, buf, bytes);
    if (r < 0)
        return;

    buf[r] = '\0';
    this->ui->statusBar->showMessage(buf);

    if (this->loader->isFinished())
        this->ui->statusBar->showMessage(QObject::tr("SFZ loaded"));
}

void LiquidMainWindow::onLoaderFinished()
{
    this->loader->deleteLater();
    this->ui->statusBar->showMessage(QObject::tr("SFZ loaded"));
}
