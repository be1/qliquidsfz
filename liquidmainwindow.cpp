#include "liquidmainwindow.h"
#include "ui_liquidmainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QQueue>
#include <QDebug>
#include <liquidsfz.hh>
#include <jack/jack.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <alloca.h>
#include "knob.h"
#include "config.h"

LiquidMainWindow::LiquidMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LiquidMainWindow),
    filename(""),
    pendingFilename(""),
    channel(0),
    loader(nullptr),
    cc_queue(new QQueue<QPair<int, int>>),
    jack_client(nullptr),
    jack_midi_in(nullptr),
    jack_audio_l(nullptr),
    jack_audio_r(nullptr),
    last_on(false),
    gain(1.0),
    _gain(1.0)
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

    QObject::connect(ui->midiChannelSpinBox, &QSpinBox::textChanged, this, &LiquidMainWindow::onCommitChan);
    QObject::connect(ui->actionLoad, &QAction::triggered, this, &LiquidMainWindow::onLoadTriggered);
    QObject::connect(ui->actionClearLog, &QAction::triggered, this, &LiquidMainWindow::onClearLogTriggered);
    QObject::connect(ui->actionAbout, &QAction::triggered, this, &LiquidMainWindow::onHelpAbout);
    QObject::connect(this, &LiquidMainWindow::handleNote, this, &LiquidMainWindow::onHandleNote);
    QObject::connect(this, &LiquidMainWindow::handleCC, this, &LiquidMainWindow::onHandleCC);
    QObject::connect(this, &LiquidMainWindow::logEvent, this, &LiquidMainWindow::onLogEvent);
    QObject::connect(this, &LiquidMainWindow::progressEvent, this, &LiquidMainWindow::onProgressEvent);
    QObject::connect(ui->gainSlider, &QSlider::valueChanged, this, &LiquidMainWindow::onGainValueChanged);

    jack_status_t jack_status;
    this->jack_client = jack_client_open ("qliquidsfz", JackNullOption, &jack_status, NULL);

    if (!this->jack_client) {
        this->ui->statusBar->showMessage("jack_client_open() failed: " + QString::number(jack_status));
        if (jack_status & JackServerFailed) {
            this->ui->statusBar->showMessage(QObject::tr("Unable to connect to JACK server"));
        }

        return;
    }

    if (jack_status & JackServerStarted) {
        this->ui->statusBar->showMessage(QObject::tr("JACK server started"));
    }

    char* name = jack_get_client_name(this->jack_client);
    this->setWindowTitle(QString(name));

    this->synth.set_sample_rate(jack_get_sample_rate(this->jack_client));

    this->jack_midi_in = jack_port_register(this->jack_client, "input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    this->jack_audio_l = jack_port_register(this->jack_client, "output_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    this->jack_audio_r = jack_port_register(this->jack_client, "output_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    jack_set_process_callback(this->jack_client, [](jack_nframes_t nframes, void* arg) {
        auto self = static_cast<LiquidMainWindow *>(arg);

        if (self->gain != self->_gain) {
            self->synth.set_gain(self->_gain);
            self->gain = self->_gain;
        }

        return self->process(nframes);
    }, this);

    jack_activate(this->jack_client);
}

LiquidMainWindow::~LiquidMainWindow()
{
    jack_deactivate(this->jack_client);
    delete ui;
}

void LiquidMainWindow::loadFile(const QString &filename)
{
    this->pendingFilename = filename;
    QFileInfo info(filename);
    ui->actionLoad->setText(info.baseName());
    onCommitLoad();
}

int LiquidMainWindow::process(jack_nframes_t nframes)
{
    if (!this->mutex.tryLock()) {
        // The sfz loader thread is working. So, silence output with zeroes.
        float *l = (float*) jack_port_get_buffer (this->jack_audio_l, nframes);
        float *r = (float*) jack_port_get_buffer (this->jack_audio_r, nframes);
        memset(l, 0, nframes * sizeof (float));
        memset(r, 0, nframes * sizeof (float));
        return 0;
    }

    /* GUI CCs must be checked even when no MIDI event from jack */
    int i = 0;
    while (!this->cc_queue->isEmpty() && i < nframes) {
        QPair<int, int> cc = cc_queue->dequeue();
        synth.add_event_cc(i++, this->channel, cc.first, cc.second);
    }

    void* port_buf = jack_port_get_buffer (this->jack_midi_in, nframes);
    jack_nframes_t event_count = jack_midi_get_event_count (port_buf);

    for (jack_nframes_t event_index = 0; event_index < event_count; event_index++) {
        jack_midi_event_t in_event;
        jack_midi_event_get (&in_event, port_buf, event_index);
        if (in_event.size == 3) {
            int chan = in_event.buffer[0] & 0x0f;
            /* filter out channels that aren't mine (but keep omni) */
            if (this->channel && (chan != this->channel)) {
                synth.all_sound_off();
                continue;
            }

            /* Input MIDI */
            switch (in_event.buffer[0] & 0xf0) {
                case 0x90: synth.add_event_note_on (in_event.time, chan, in_event.buffer[1], in_event.buffer[2]);
                           emit handleNote(true, chan);
                           break;
                case 0x80: synth.add_event_note_off (in_event.time, chan, in_event.buffer[1]);
                           emit handleNote(false, chan);
                           break;
                case 0xb0: synth.add_event_cc (in_event.time, chan, in_event.buffer[1], in_event.buffer[2]);
                           emit handleCC(in_event.buffer[1], in_event.buffer[2]);
                           break;
                case 0xe0: synth.add_event_pitch_bend (in_event.time, chan, in_event.buffer[1] + 128 * in_event.buffer[2]);
                           break;
              }
          }
      }

      float *outputs[2] = {
          (float *) jack_port_get_buffer (this->jack_audio_l, nframes),
          (float *) jack_port_get_buffer (this->jack_audio_r, nframes)
      };
      synth.process (outputs, nframes);
      this->mutex.unlock();
      return 0;
}

void LiquidMainWindow::onClearLogTriggered()
{
    ui->logTextEdit->clear();
}

void LiquidMainWindow::onLoadTriggered()
{
    this->pendingFilename = QFileDialog::getOpenFileName(this, QObject::tr("Open SFZ"), "", QObject::tr("SFZ Files (*.sfz)"));

    if (pendingFilename.isEmpty())
        return;

    onCommitLoad();
}

void LiquidMainWindow::onCommitChan()
{
    // update channel number label
    if (this->channel != (ui->midiChannelSpinBox->value() - 1)) {
        this->channel = (qint8) ui->midiChannelSpinBox->value() -1;
        ui->midiChannelLabel->setText(QString::number(ui->midiChannelSpinBox->value()));
    }
}

void LiquidMainWindow::onCommitLoad()
{
    // clear log area
    ui->logTextEdit->clear();

    // load SFZ file in a separate thread.
    if (this->filename != this->pendingFilename) {
        ui->actionLoad->setEnabled(false);

        // clear knobs if any
        QHBoxLayout* knobs = ui->knobHorizontalLayout;
        qDebug() << "removing" << knobs->children().size() << "knob(s)";
        while (QLayoutItem* item = knobs->takeAt(0)) {
            if (QLayout* childLayout = item->layout()) {
                Knob* knob = static_cast<Knob*>(childLayout);
                delete knob;
            }
        }

        this->loader = new SFZLoader(&this->synth, &this->mutex, this);
        QObject::connect(this->loader, SIGNAL(finished()), this, SLOT(onLoaderFinished()));

        this->filename = this->pendingFilename;
        this->loader->setSFZ(this->filename);
        this->loader->start();

        ui->actionLoad->setText(QObject::tr("Load..."));
        QFileInfo info(this->filename);
        ui->sfzFileLabel->setText(info.baseName());
    }
}

void LiquidMainWindow::onHelpAbout()
{
    QMessageBox::about(this, QObject::tr("About QLiquidSFZ"), "QLiquidSFZ version " VERSION " (" REVISION ")\n\n"
                                                              "This program is a GUI to the liquidsfz library, an SFZ synth player.\n\n"
                                                              "This program is free software, Copyright (C) Benoit Rouits <brouits@free.fr> "
                                                              "and released under the GNU General Public Lisence version 3. It is delivered "
                                                              "AS IS in the hope it can be useful, but with no warranty of correctness.");
}

void LiquidMainWindow::onLoaderFinished()
{
    this->loader->deleteLater();
    this->ui->statusBar->showMessage(QObject::tr("SFZ loaded"));

    ui->actionLoad->setEnabled(true);

    QString message;
    auto ccs = synth.list_ccs();

    qDebug () << "adding" << ccs.size() << "knob(s)";
    if (ccs.size()) {
        for (const auto& cc_info : ccs) {
            message.append(" • CC #%1");
            message = message.arg(cc_info.cc());
            if (cc_info.has_label()) {
                message.append(" - ");
                message.append(cc_info.label().c_str());
                message.append(" (");
                message.append(QString::number(cc_info.default_value()));
                message.append(")\n");
            }

            QHBoxLayout* knobs = ui->knobHorizontalLayout;
            Knob* knob = new Knob(cc_queue, cc_info, this);
            knobs->addLayout(knob);
        }

        emit logEvent(message);
    }
}

void LiquidMainWindow::onHandleNote(bool on, int chan)
{
    if (this->channel && chan != this->channel)
        return;

    if (!on) {
        if (last_on) {
            this->ui->midiChannelLabel->setText("<font color=\"black\">•</font>");
            last_on = false;
        }
    } else {
        if (!last_on) {
            this->ui->midiChannelLabel->setText("<font color=\"green\">•</font>");
            last_on = true;
        }
    }
}

void LiquidMainWindow::onHandleCC(int cc, int val)
{
    QHBoxLayout* knobs = ui->knobHorizontalLayout;
    for (int i = 0; i < knobs->children().size(); i++) {
        Knob* knob = static_cast<Knob*>(knobs->children().at(i));
        if (knob->cc() == cc) {
            knob->setValue(val);
            break;
        }
    }
}

void LiquidMainWindow::onLogEvent(const QString &message)
{
    this->ui->logTextEdit->appendPlainText(message);
}

void LiquidMainWindow::onProgressEvent(int progress)
{
    this->ui->statusBar->showMessage(QString::number(progress) + "%");
}

void LiquidMainWindow::onGainValueChanged(int val)
{
    ui->gainSlider->setToolTip(QString::number(val));
    this->_gain = (float)val / 100.0;
}
