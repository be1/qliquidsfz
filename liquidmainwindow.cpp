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

    this->synth.set_log_function([this](LiquidSFZ::Log level, const char *msg) {
            QString message;
            switch (level) {
            case LiquidSFZ::Log::INFO: message.append("INFO: "); break;
            case LiquidSFZ::Log::DEBUG: message.append("DEBUG: "); break;
            case LiquidSFZ::Log::ERROR: message.append("ERROR: "); break;
            case LiquidSFZ::Log::WARNING: message.append("WARNING: "); break;
            default: break;
            }
            QString str(msg);
            if (str.startsWith('/')) {
                int usable = str.lastIndexOf('/');
                str.remove(0, usable+1);
            }
            message.append(str);
            emit logEvent(message);
        });

    this->synth.set_log_level(LiquidSFZ::Log::INFO);

    this->synth.set_progress_function([this](double percent) {
        emit progressEvent((int)percent);
    });

    QObject::connect(ui->sfzLoadButton, SIGNAL(clicked()), this, SLOT(onLoadClicked()));
    QObject::connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
    QObject::connect(ui->commitButton, SIGNAL(clicked()), this, SLOT(onCommitClicked()));
    QObject::connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(onHelpAbout()));
    QObject::connect(this, SIGNAL(handleNote(bool)), this, SLOT(onHandleNote(bool)));
    QObject::connect(this, SIGNAL(logEvent(const QString&)), this, SLOT(onLogEvent(const QString&)));
    QObject::connect(this, SIGNAL(progressEvent(int)), this, SLOT(onProgressEvent(int)));

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
	    /* filter out channels that aren't mine (but keep omni) */
            if (this->channel && (channel != this->channel)) {
                emit handleNote(false);
                continue;
            }

            switch (in_event.buffer[0] & 0xf0) {
                case 0x90: synth.add_event_note_on (in_event.time, channel, in_event.buffer[1], in_event.buffer[2]);
                           emit handleNote(true);
                           break;
                case 0x80: synth.add_event_note_off (in_event.time, channel, in_event.buffer[1]);
                           emit handleNote(false);
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
    ui->sfzFileLabel->setText(info.baseName());

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
        ui->sfzFileLabel->setText(info.baseName());
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

void LiquidMainWindow::onLoaderFinished()
{
    this->loader->deleteLater();
    this->ui->statusBar->showMessage(QObject::tr("SFZ loaded"));
}

void LiquidMainWindow::onHandleNote(bool doHandle)
{
    if (!doHandle)
        this->ui->midiChannelLabel->setText("<font color=\"black\">" + QString::number(this->channel +1) + "</font>");
    else
        this->ui->midiChannelLabel->setText("<font color=\"red\">" + QString::number(this->channel + 1) + "</font>");
}

void LiquidMainWindow::onLogEvent(const QString &message)
{
    this->ui->logTextEdit->appendPlainText(message);
}

void LiquidMainWindow::onProgressEvent(int progress)
{
    this->ui->statusBar->showMessage(QString::number(progress) + "%");
}
